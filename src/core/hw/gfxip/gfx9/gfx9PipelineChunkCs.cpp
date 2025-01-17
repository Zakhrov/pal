/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/

#include "core/platform.h"
#include "core/hw/gfxip/computePipeline.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9ComputePipeline.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9GraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkCs.h"
#include "palPipelineAbiProcessorImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// Base count of SH registers which are loaded using LOAD_SH_REG_INDEX when binding to a universal command buffer.
constexpr uint32 BaseLoadedShRegCount =
    1 + // mmCOMPUTE_PGM_LO
    1 + // mmCOMPUTE_PGM_HI
    1 + // mmCOMPUTE_PGM_RSRC1
    0 + // mmCOMPUTE_PGM_RSRC2 is not included because it partially depends on bind-time state
    0 + // mmCOMPUTE_PGM_RSRC3 is not included because it is not present on all HW
    0 + // mmCOMPUTE_USER_ACCUM_0...3 is not included because it is not present on all HW
    0 + // mmCOMPUTE_RESOURCE_LIMITS is not included because it partially depends on bind-time state
    1 + // mmCOMPUTE_NUM_THREAD_X
    1 + // mmCOMPUTE_NUM_THREAD_Y
    1 + // mmCOMPUTE_NUM_THREAD_Z
    1 + // mmCOMPUTE_USER_DATA_0 + ConstBufTblStartReg
    0;  // mmCOMPUTE_SHADER_CHKSUM is not included because it is not present on all HW

// =====================================================================================================================
PipelineChunkCs::PipelineChunkCs(
    const Device&    device,
    ShaderStageInfo* pStageInfo,
    PerfDataInfo*    pPerfDataInfo)
    :
    m_device(device),
    m_pCsPerfDataInfo(pPerfDataInfo),
    m_pStageInfo(pStageInfo)
{
    memset(&m_commands, 0, sizeof(m_commands));
    m_pStageInfo->stageId = Abi::HardwareStage::Cs;
}

// =====================================================================================================================
// Early initialization for this pipeline chunk when used in a compute pipeline. Responsible for determining the number
// of SH registers to be loaded using LOAD_SH_REG_INDEX.
uint32 PipelineChunkCs::EarlyInit()
{
    const Gfx9PalSettings&   settings  = m_device.Settings();
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    uint32 count = 0;

    if (settings.enableLoadIndexForObjectBinds)
    {
        // Add one register if the GPU supports SPP.
        count += (BaseLoadedShRegCount + ((chipProps.gfx9.supportSpp == 1) ? 1 : 0));

        if (IsGfx10(chipProps.gfxLevel))
        {
            count += 1; //  mmCOMPUTE_PGM_RSRC3
            if (chipProps.gfx9.supportSpiPrefPriority)
            {
                count += 4; // mmCOMPUTE_USER_ACCUM_0...3
            }
        }
    }

    return count;
}

// =====================================================================================================================
// Early initialization for this pipeline chunk when used in a graphics pipeline. Responsible for determining the number
// of SH registers to be loaded using LOAD_SH_REG_INDEX.
void PipelineChunkCs::EarlyInit(
    GraphicsPipelineLoadInfo* pInfo)
{
    PAL_ASSERT(pInfo != nullptr);
    pInfo->loadedShRegCount += EarlyInit();
}

// =====================================================================================================================
// Late initialization for this pipeline chunk.  Responsible for fetching register values from the pipeline binary and
// determining the values of other registers.  Also uploads register state into GPU memory.
template <typename CsPipelineUploader>
void PipelineChunkCs::LateInit(
    const AbiProcessor&              abiProcessor,
    const RegisterVector&            registers,
    uint32                           wavefrontSize,
    ComputePipelineIndirectFuncInfo* pIndirectFuncList,
    uint32                           indirectFuncCount,
    uint32*                          pThreadsPerTgX,
    uint32*                          pThreadsPerTgY,
    uint32*                          pThreadsPerTgZ,
    CsPipelineUploader*              pUploader)
{
    const auto&              cmdUtil          = m_device.CmdUtil();
    const auto&              regInfo          = cmdUtil.GetRegInfo();
    const GpuChipProperties& chipProps        = m_device.Parent()->ChipProperties();
    const bool               useLoadIndexPath = pUploader->EnableLoadIndexPath();

    BuildPm4Headers(*pUploader);
    Abi::PipelineSymbolEntry csProgram  = { };
    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::CsMainEntry, &csProgram))
    {
        m_pStageInfo->codeLength  = static_cast<size_t>(csProgram.size);
        const gpusize csProgramVa = (csProgram.value + pUploader->CodeGpuVirtAddr());
        PAL_ASSERT(IsPow2Aligned(csProgramVa, 256u));

        m_commands.set.computePgmLo.bits.DATA = Get256BAddrLo(csProgramVa);
        m_commands.set.computePgmHi.bits.DATA = Get256BAddrHi(csProgramVa);
    }

    Abi::PipelineSymbolEntry csSrdTable = { };
    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::CsShdrIntrlTblPtr, &csSrdTable))
    {
        const gpusize csSrdTableVa = (csSrdTable.value + pUploader->DataGpuVirtAddr());
        m_commands.set.computeUserDataLo.bits.DATA = LowPart(csSrdTableVa);
    }

    m_commands.set.computePgmRsrc1.u32All     = registers.At(mmCOMPUTE_PGM_RSRC1);
    m_commands.dynamic.computePgmRsrc2.u32All = registers.At(mmCOMPUTE_PGM_RSRC2);
    m_commands.set.computeNumThreadX.u32All   = registers.At(mmCOMPUTE_NUM_THREAD_X);
    m_commands.set.computeNumThreadY.u32All   = registers.At(mmCOMPUTE_NUM_THREAD_Y);
    m_commands.set.computeNumThreadZ.u32All   = registers.At(mmCOMPUTE_NUM_THREAD_Z);

    if (IsGfx10(chipProps.gfxLevel))
    {
        m_commands.set.computePgmRsrc3.u32All = registers.At(Gfx10::mmCOMPUTE_PGM_RSRC3);

        if (chipProps.gfx9.supportSpiPrefPriority)
        {
            registers.HasEntry(Gfx10::mmCOMPUTE_USER_ACCUM_0, &m_commands.set.regComputeUserAccum0.u32All);
            registers.HasEntry(Gfx10::mmCOMPUTE_USER_ACCUM_1, &m_commands.set.regComputeUserAccum1.u32All);
            registers.HasEntry(Gfx10::mmCOMPUTE_USER_ACCUM_2, &m_commands.set.regComputeUserAccum2.u32All);
            registers.HasEntry(Gfx10::mmCOMPUTE_USER_ACCUM_3, &m_commands.set.regComputeUserAccum3.u32All);
        }
    }

    if (chipProps.gfx9.supportSpp == 1)
    {
        PAL_ASSERT(regInfo.mmComputeShaderChksum != 0);
        registers.HasEntry(regInfo.mmComputeShaderChksum, &m_commands.set.computeShaderChksum.u32All);
    }

    *pThreadsPerTgX = m_commands.set.computeNumThreadX.bits.NUM_THREAD_FULL;
    *pThreadsPerTgY = m_commands.set.computeNumThreadY.bits.NUM_THREAD_FULL;
    *pThreadsPerTgZ = m_commands.set.computeNumThreadZ.bits.NUM_THREAD_FULL;

    if (pUploader->EnableLoadIndexPath())
    {
        pUploader->AddShReg(mmCOMPUTE_PGM_LO, m_commands.set.computePgmLo);
        pUploader->AddShReg(mmCOMPUTE_PGM_HI, m_commands.set.computePgmHi);

        pUploader->AddShReg((mmCOMPUTE_USER_DATA_0 + ConstBufTblStartReg), m_commands.set.computeUserDataLo);

        pUploader->AddShReg(mmCOMPUTE_PGM_RSRC1,    m_commands.set.computePgmRsrc1);
        pUploader->AddShReg(mmCOMPUTE_NUM_THREAD_X, m_commands.set.computeNumThreadX);
        pUploader->AddShReg(mmCOMPUTE_NUM_THREAD_Y, m_commands.set.computeNumThreadY);
        pUploader->AddShReg(mmCOMPUTE_NUM_THREAD_Z, m_commands.set.computeNumThreadZ);

        if (IsGfx10(chipProps.gfxLevel))
        {
            pUploader->AddShReg(Gfx10::mmCOMPUTE_PGM_RSRC3, m_commands.set.computePgmRsrc3);

            if (chipProps.gfx9.supportSpiPrefPriority)
            {
                pUploader->AddShReg(Gfx10::mmCOMPUTE_USER_ACCUM_0, m_commands.set.regComputeUserAccum0);
                pUploader->AddShReg(Gfx10::mmCOMPUTE_USER_ACCUM_1, m_commands.set.regComputeUserAccum1);
                pUploader->AddShReg(Gfx10::mmCOMPUTE_USER_ACCUM_2, m_commands.set.regComputeUserAccum2);
                pUploader->AddShReg(Gfx10::mmCOMPUTE_USER_ACCUM_3, m_commands.set.regComputeUserAccum3);
            }
        }

        if (chipProps.gfx9.supportSpp == 1)
        {
            pUploader->AddShReg(regInfo.mmComputeShaderChksum, m_commands.set.computeShaderChksum);
        }
    }

    registers.HasEntry(mmCOMPUTE_RESOURCE_LIMITS, &m_commands.dynamic.computeResourceLimits.u32All);

    const uint32 threadsPerGroup = (*pThreadsPerTgX) * (*pThreadsPerTgY) * (*pThreadsPerTgZ);
    const uint32 wavesPerGroup   = RoundUpQuotient(threadsPerGroup, wavefrontSize);

    // SIMD_DEST_CNTL: Controls which SIMDs thread groups get scheduled on.  If the number of
    // waves-per-TG is a multiple of 4, this should be 1, otherwise 0.
    m_commands.dynamic.computeResourceLimits.bits.SIMD_DEST_CNTL = ((wavesPerGroup % 4) == 0) ? 1 : 0;

    // Force even distribution on all SIMDs in CU for workgroup size is 64
    // This has shown some good improvements if #CU per SE not a multiple of 4
    if (((chipProps.gfx9.numShaderArrays * chipProps.gfx9.numCuPerSh) & 0x3) && (wavesPerGroup == 1))
    {
        m_commands.dynamic.computeResourceLimits.bits.FORCE_SIMD_DIST = 1;
    }

    if (m_device.Parent()->LegacyHwsTrapHandlerPresent() && (chipProps.gfxLevel == GfxIpLevel::GfxIp9))
    {

        // If the legacy HWS's trap handler is present, compute shaders must always set the TRAP_PRESENT
        // flag.

        // TODO: Handle the case where the client enabled a trap handler and the hardware scheduler's trap handler
        // is already active!
        PAL_ASSERT(m_commands.dynamic.computePgmRsrc2.bits.TRAP_PRESENT == 0);
        m_commands.dynamic.computePgmRsrc2.bits.TRAP_PRESENT = 1;
    }

    const auto& settings = m_device.Settings();

    // LOCK_THRESHOLD: Sets per-SH low threshold for locking.  Set in units of 4, 0 disables locking.
    // LOCK_THRESHOLD's maximum value: (6 bits), in units of 4, so it is max of 252.
    constexpr uint32 Gfx9MaxLockThreshold = 252;
    PAL_ASSERT(settings.csLockThreshold <= Gfx9MaxLockThreshold);
    m_commands.dynamic.computeResourceLimits.bits.LOCK_THRESHOLD = Min((settings.csLockThreshold >> 2),
                                                                        (Gfx9MaxLockThreshold >> 2));

    // SIMD_DEST_CNTL: Controls whichs SIMDs thread groups get scheduled on.  If no override is set, just keep
    // the existing value in COMPUTE_RESOURCE_LIMITS.
    switch (settings.csSimdDestCntl)
    {
    case CsSimdDestCntlForce1:
        m_commands.dynamic.computeResourceLimits.bits.SIMD_DEST_CNTL = 1;
        break;
    case CsSimdDestCntlForce0:
        m_commands.dynamic.computeResourceLimits.bits.SIMD_DEST_CNTL = 0;
        break;
    default:
        PAL_ASSERT(settings.csSimdDestCntl == CsSimdDestCntlDefault);
        break;
    }

    cmdUtil.BuildPipelinePrefetchPm4(*pUploader, &m_commands.prefetch);

    ComputePipeline::GetFunctionGpuVirtAddrs(abiProcessor, *pUploader, pIndirectFuncList, indirectFuncCount);
}

// =====================================================================================================================
// Initializes the signature of a compute shader using a pipeline ELF.
// NOTE: Must be called before LateInit!
void PipelineChunkCs::SetupSignatureFromElf(
    ComputeShaderSignature*   pSignature,
    const CodeObjectMetadata& metadata,
    const RegisterVector&     registers)
{
    const auto& chipProps = m_device.Parent()->ChipProperties();

    pSignature->stage.firstUserSgprRegAddr = (mmCOMPUTE_USER_DATA_0 + FastUserDataStartReg);
    for (uint16 offset = mmCOMPUTE_USER_DATA_0; offset <= mmCOMPUTE_USER_DATA_15; ++offset)
    {
        uint32 value = 0;
        if (registers.HasEntry(offset, &value))
        {
            if (value < MaxUserDataEntries)
            {
                PAL_ASSERT(offset >= pSignature->stage.firstUserSgprRegAddr);
                const uint8 userSgprId = static_cast<uint8>(offset - pSignature->stage.firstUserSgprRegAddr);

                pSignature->stage.mappedEntry[userSgprId] = static_cast<uint8>(value);
                pSignature->stage.userSgprCount = Max<uint8>(userSgprId + 1, pSignature->stage.userSgprCount);
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::GlobalTable))
            {
                PAL_ASSERT(offset == (mmCOMPUTE_USER_DATA_0 + InternalTblStartReg));
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::PerShaderTable))
            {
                PAL_ASSERT(offset == (mmCOMPUTE_USER_DATA_0 + ConstBufTblStartReg));
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::SpillTable))
            {
                pSignature->stage.spillTableRegAddr = static_cast<uint16>(offset);
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::Workgroup))
            {
                pSignature->numWorkGroupsRegAddr = static_cast<uint16>(offset);
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::GdsRange))
            {
                PAL_ASSERT(offset == (mmCOMPUTE_USER_DATA_0 + GdsRangeRegCompute));
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::PerShaderPerfData))
            {
                m_pCsPerfDataInfo->regOffset = offset;
            }
            else if ((value == static_cast<uint32>(Abi::UserDataMapping::VertexBufferTable)) ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::StreamOutTable))    ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::BaseVertex))        ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::BaseInstance))      ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::DrawIndex))         ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::BaseIndex))         ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::Log2IndexSize))     ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::EsGsLdsSize)))
            {
                PAL_ALERT_ALWAYS(); // These are for graphics pipelines only!
            }
            else
            {
                // This appears to be an illegally-specified user-data register!
                PAL_NEVER_CALLED();
            }
        } // If HasEntry()
    } // For each user-SGPR

#if PAL_ENABLE_PRINTS_ASSERTS
    // Indirect user-data table(s) are not supported on compute pipelines, so just assert that the table addresses
    // are unmapped.
    if (metadata.pipeline.hasEntry.indirectUserDataTableAddresses != 0)
    {
        constexpr uint32 MetadataIndirectTableAddressCount =
            (sizeof(metadata.pipeline.indirectUserDataTableAddresses) /
             sizeof(metadata.pipeline.indirectUserDataTableAddresses[0]));
        constexpr uint32 DummyAddresses[MetadataIndirectTableAddressCount] = { 0 };

        PAL_ASSERT_MSG(0 == memcmp(&metadata.pipeline.indirectUserDataTableAddresses[0],
                                   &DummyAddresses[0], sizeof(DummyAddresses)),
                       "Indirect user-data tables are not supported for Compute Pipelines!");
    }
#endif

    // NOTE: We skip the stream-out table address here because it is not used by compute pipelines.

    if (metadata.pipeline.hasEntry.spillThreshold != 0)
    {
        pSignature->spillThreshold = static_cast<uint16>(metadata.pipeline.spillThreshold);
    }

    if (metadata.pipeline.hasEntry.userDataLimit != 0)
    {
        pSignature->userDataLimit = static_cast<uint16>(metadata.pipeline.userDataLimit);
    }

    // We don't bother checking the wavefront size for pre-Gfx10 GPU's since it is implicitly 64 before Gfx10. Any ELF
    // which doesn't specify a wavefront size is assumed to use 64, even on Gfx10 and newer.
    if (IsGfx10(chipProps.gfxLevel))
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 495
        // Older ABI versions encoded wave32 vs. wave64 using the CS_W32_EN field of COMPUTE_DISPATCH_INITIATOR. Fall
        // back to that encoding if the CS metadata does not specify a wavefront size.
        regCOMPUTE_DISPATCH_INITIATOR dispatchInitiator = { };
#endif

        const auto& csMetadata = metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Cs)];
        if (csMetadata.hasEntry.wavefrontSize != 0)
        {
            PAL_ASSERT((csMetadata.wavefrontSize == 64) || (csMetadata.wavefrontSize == 32));
            pSignature->flags.isWave32 = (csMetadata.wavefrontSize == 32);
        }
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 495
        else if (registers.HasEntry(mmCOMPUTE_DISPATCH_INITIATOR, &dispatchInitiator.u32All))
        {
            pSignature->flags.isWave32 = dispatchInitiator.gfx10.CS_W32_EN;
        }
#endif
    }
}

// Instantiate template versions for the linker.
template
void PipelineChunkCs::LateInit<ComputePipelineUploader>(
    const AbiProcessor&              abiProcessor,
    const RegisterVector&            registers,
    uint32                           wavefrontSize,
    ComputePipelineIndirectFuncInfo* pIndirectFuncList,
    uint32                           indirectFuncCount,
    uint32*                          pThreadsPerTgX,
    uint32*                          pThreadsPerTgY,
    uint32*                          pThreadsPerTgZ,
    ComputePipelineUploader*         pUploader);

// =====================================================================================================================
// Copies this pipeline chunk's sh commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkCs::WriteShCommands(
    CmdStream*                      pCmdStream,
    uint32*                         pCmdSpace,
    const DynamicComputeShaderInfo& csInfo,
    bool                            prefetch
    ) const
{
    // Disable the LOAD_INDEX path if the PM4 optimizer is enabled or for compute command buffers.  The optimizer cannot
    // optimize these load packets because the register values are in GPU memory.  Additionally, any client requesting
    // PM4 optimization is trading CPU cycles for GPU performance, so the savings of using LOAD_INDEX is not important.
    // This gets disabled for compute command buffers because the MEC does not support any LOAD packets.
    const bool useSetPath =
        ((m_commands.loadIndex.loadShRegIndex.header.u32All == 0) ||
         pCmdStream->Pm4OptimizerEnabled()                        ||
         (pCmdStream->GetEngineType() == EngineType::EngineTypeCompute));

    if (useSetPath)
    {
        pCmdSpace = pCmdStream->WritePm4Image(m_commands.set.spaceNeeded, &m_commands.set, pCmdSpace);
    }
    else
    {
        constexpr uint32 SpaceNeeded = sizeof(m_commands.loadIndex) / sizeof(uint32);
        pCmdSpace = pCmdStream->WritePm4Image(SpaceNeeded, &m_commands.loadIndex, pCmdSpace);
    }

    auto dynamicCmds = m_commands.dynamic;

    // TG_PER_CU: Sets the CS threadgroup limit per CU. Range is 1 to 15, 0 disables the limit.
    constexpr uint32 Gfx9MaxTgPerCu = 15;
    dynamicCmds.computeResourceLimits.bits.TG_PER_CU = Min(csInfo.maxThreadGroupsPerCu, Gfx9MaxTgPerCu);
    if (csInfo.maxWavesPerCu > 0)
    {
        dynamicCmds.computeResourceLimits.bits.WAVES_PER_SH =
            Gfx9::ComputePipeline::CalcMaxWavesPerSh(m_device.Parent()->ChipProperties(), csInfo.maxWavesPerCu);
    }

    if (csInfo.ldsBytesPerTg > 0)
    {
        // Round to nearest multiple of the LDS granularity, then convert to the register value.
        // NOTE: Granularity for the LDS_SIZE field is 128, range is 0->128 which allocates 0 to 16K DWORDs.
        dynamicCmds.computePgmRsrc2.bits.LDS_SIZE =
            Pow2Align((csInfo.ldsBytesPerTg / sizeof(uint32)), Gfx9LdsDwGranularity) >> Gfx9LdsDwGranularityShift;
    }

    constexpr uint32 SpaceNeededDynamic = sizeof(dynamicCmds) / sizeof(uint32);
    pCmdSpace = pCmdStream->WritePm4Image(SpaceNeededDynamic, &dynamicCmds, pCmdSpace);

    if (m_pCsPerfDataInfo->regOffset != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(m_pCsPerfDataInfo->regOffset,
                                                                m_pCsPerfDataInfo->gpuVirtAddr,
                                                                pCmdSpace);
    }

    if (prefetch)
    {
        memcpy(pCmdSpace, &m_commands.prefetch, m_commands.prefetch.spaceNeeded * sizeof(uint32));
        pCmdSpace += m_commands.prefetch.spaceNeeded;
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Assembles the PM4 headers for the commands in this Pipeline chunk.
template <typename CsPipelineUploader>
void PipelineChunkCs::BuildPm4Headers(
    const CsPipelineUploader& uploader)
{
    const auto&    chipProps = m_device.Parent()->ChipProperties();
    const CmdUtil& cmdUtil   = m_device.CmdUtil();
    const auto&    regInfo   = cmdUtil.GetRegInfo();

    // PM4 image for compute command buffers:
    m_commands.set.spaceNeeded = cmdUtil.BuildSetSeqShRegs(mmCOMPUTE_NUM_THREAD_X,
                                                           mmCOMPUTE_NUM_THREAD_Z,
                                                           ShaderCompute,
                                                           &m_commands.set.hdrComputeNumThread);

    m_commands.set.spaceNeeded += cmdUtil.BuildSetSeqShRegs(mmCOMPUTE_PGM_LO,
                                                            mmCOMPUTE_PGM_HI,
                                                            ShaderCompute,
                                                            &m_commands.set.hdrComputePgm);

    m_commands.set.spaceNeeded += cmdUtil.BuildSetOneShReg(mmCOMPUTE_PGM_RSRC1,
                                                           ShaderCompute,
                                                           &m_commands.set.hdrComputePgmRsrc1);

    m_commands.set.spaceNeeded += cmdUtil.BuildSetOneShReg(mmCOMPUTE_USER_DATA_0 + ConstBufTblStartReg,
                                                           ShaderCompute,
                                                           &m_commands.set.hdrComputeUserData);
    if (chipProps.gfx9.supportSpp == 1)
    {
        m_commands.set.spaceNeeded += cmdUtil.BuildSetOneShReg(regInfo.mmComputeShaderChksum,
                                                               ShaderCompute,
                                                               &m_commands.set.hdrComputeShaderChksum);
    }
    else
    {
        m_commands.set.spaceNeeded += cmdUtil.BuildNop(CmdUtil::ShRegSizeDwords + 1,
                                                       &m_commands.set.hdrComputeShaderChksum);
    }

    if (IsGfx10(chipProps.gfxLevel))
    {
        // Sets the following compute register: COMPUTE_PGM_RSRC3.  Note that all GFX10 devices support SPP.
        m_commands.set.spaceNeeded += cmdUtil.BuildSetOneShReg(Gfx10::mmCOMPUTE_PGM_RSRC3,
                                                               ShaderCompute,
                                                               &m_commands.set.hdrComputePgmRsrc3);
    }
    else
    {
        m_commands.set.spaceNeeded += cmdUtil.BuildNop(CmdUtil::ShRegSizeDwords + 1,
                                                       &m_commands.set.hdrComputePgmRsrc3);
    }

    if (chipProps.gfx9.supportSpiPrefPriority)
    {
        m_commands.set.spaceNeeded += cmdUtil.BuildSetSeqShRegs(Gfx10::mmCOMPUTE_USER_ACCUM_0,
                                                                Gfx10::mmCOMPUTE_USER_ACCUM_3,
                                                                ShaderCompute,
                                                                &m_commands.set.hdrComputeUserAccum);
    }
    else
    {
        m_commands.set.spaceNeeded += cmdUtil.BuildNop(CmdUtil::ShRegSizeDwords + 4,
                                                       &m_commands.set.hdrComputeUserAccum);
    }

    // PM4 image for universal command buffers:
    if (uploader.EnableLoadIndexPath())
    {
        cmdUtil.BuildLoadShRegsIndex(uploader.ShRegGpuVirtAddr(),
                                     uploader.ShRegisterCount(),
                                     ShaderCompute,
                                     &m_commands.loadIndex.loadShRegIndex);
    }

    // PM4 image for dynamic (bind-time) state:
    cmdUtil.BuildSetOneShReg(mmCOMPUTE_PGM_RSRC2,       ShaderCompute, &m_commands.dynamic.hdrComputePgmRsrc2);
    cmdUtil.BuildSetOneShReg(mmCOMPUTE_RESOURCE_LIMITS, ShaderCompute, &m_commands.dynamic.hdrComputeResourceLimits);
}

} // Gfx9
} // Pal
