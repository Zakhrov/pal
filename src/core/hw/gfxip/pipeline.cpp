/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/device.h"
#include "core/g_palSettings.h"
#include "core/platform.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/pipeline.h"
#include "palFile.h"
#include "palPipelineAbiProcessorImpl.h"
#include "palEventDefs.h"
#include "palSysUtil.h"

#include "core/devDriverUtil.h"

using namespace Util;

namespace Pal
{

// GPU memory alignment for shader programs.
constexpr size_t GpuMemByteAlign = 256;

constexpr Abi::ApiShaderType PalToAbiShaderType[] =
{
    Abi::ApiShaderType::Cs, // ShaderType::Cs
    Abi::ApiShaderType::Vs, // ShaderType::Vs
    Abi::ApiShaderType::Hs, // ShaderType::Hs
    Abi::ApiShaderType::Ds, // ShaderType::Ds
    Abi::ApiShaderType::Gs, // ShaderType::Gs
    Abi::ApiShaderType::Ps, // ShaderType::Ps
};
static_assert(ArrayLen(PalToAbiShaderType) == NumShaderTypes,
              "PalToAbiShaderType[] array is incorrectly sized!");

// =====================================================================================================================
Pipeline::Pipeline(
    Device* pDevice,
    bool    isInternal)  // True if this is a PAL-owned pipeline (i.e., an RPM pipeline).
    :
    m_pDevice(pDevice),
    m_gpuMem(),
    m_gpuMemSize(0),
    m_pPipelineBinary(nullptr),
    m_pipelineBinaryLen(0),
    m_apiHwMapping(),
    m_perfDataMem(),
    m_perfDataGpuMemSize(0)
{
    m_flags.value      = 0;
    m_flags.isInternal = isInternal;

    m_apiHwMapping.u64All = 0;

    memset(&m_info, 0, sizeof(m_info));
    memset(&m_shaderMetaData, 0, sizeof(m_shaderMetaData));
    memset(&m_perfDataInfo, 0, sizeof(m_perfDataInfo));
}

// =====================================================================================================================
Pipeline::~Pipeline()
{
    if (m_gpuMem.IsBound())
    {
        m_pDevice->MemMgr()->FreeGpuMem(m_gpuMem.Memory(), m_gpuMem.Offset());
        m_gpuMem.Update(nullptr, 0);
    }

    if (m_perfDataMem.IsBound())
    {
        m_pDevice->MemMgr()->FreeGpuMem(m_perfDataMem.Memory(), m_perfDataMem.Offset());
        m_perfDataMem.Update(nullptr, 0);
    }

    ResourceDestroyEventData data = {};
    data.pObj = this;
    m_pDevice->GetPlatform()->GetEventProvider()->LogGpuMemoryResourceDestroyEvent(data);

    PAL_SAFE_FREE(m_pPipelineBinary, m_pDevice->GetPlatform());
}

// =====================================================================================================================
// Destroys a pipeline object allocated via a subclass' CreateInternal()
void Pipeline::DestroyInternal()
{
    PAL_ASSERT(IsInternal());

    Platform*const pPlatform = m_pDevice->GetPlatform();
    Destroy();
    PAL_FREE(this, pPlatform);
}

// =====================================================================================================================
// Allocates GPU memory for this pipeline and uploads the code and data contain in the ELF binary to it.  Any ELF
// relocations are also applied to the memory during this operation.
Result Pipeline::PerformRelocationsAndUploadToGpuMemory(
    const AbiProcessor&       abiProcessor,
    const CodeObjectMetadata& metadata,
    const GpuHeap&            clientPreferredHeap,
    PipelineUploader*         pUploader)
{
    PAL_ASSERT(pUploader != nullptr);

    // Compute the total size of all shader stages' performance data buffers.
    gpusize performanceDataOffset = 0;
    for (uint32 s = 0; s < static_cast<uint32>(Abi::HardwareStage::Count); ++s)
    {
        const uint32 performanceDataBytes = metadata.pipeline.hardwareStage[s].perfDataBufferSize;
        if (performanceDataBytes != 0)
        {
            m_perfDataInfo[s].sizeInBytes = performanceDataBytes;
            m_perfDataInfo[s].cpuOffset   = static_cast<size_t>(performanceDataOffset);

            performanceDataOffset += performanceDataBytes;
        }
    } // for each hardware stage

    m_perfDataGpuMemSize = performanceDataOffset;
    Result result        = Result::Success;

    if (m_perfDataGpuMemSize > 0)
    {
        // Allocate gpu memory for the perf data.
        GpuMemoryCreateInfo createInfo = { };
        createInfo.heapCount           = 1;
        createInfo.heaps[0]            = GpuHeap::GpuHeapLocal;
        createInfo.alignment           = GpuMemByteAlign;
        createInfo.vaRange             = VaRange::DescriptorTable;
        createInfo.priority            = GpuMemPriority::High;
        createInfo.size                = m_perfDataGpuMemSize;

        GpuMemoryInternalCreateInfo internalInfo = { };
        internalInfo.flags.alwaysResident        = 1;

        GpuMemory* pGpuMem         = nullptr;
        gpusize    perfDataOffset  = 0;

        result = m_pDevice->MemMgr()->AllocateGpuMem(createInfo,
                                                     internalInfo,
                                                     false,
                                                     &pGpuMem,
                                                     &perfDataOffset);

        if (result == Result::Success)
        {
            m_perfDataMem.Update(pGpuMem, perfDataOffset);

            void* m_pPerfDataMapped = nullptr;
            result                  = pGpuMem->Map(&m_pPerfDataMapped);

            if (result == Result::Success)
            {
                memset(VoidPtrInc(m_pPerfDataMapped, static_cast<size_t>(perfDataOffset)),
                       0,
                       static_cast<size_t>(m_perfDataGpuMemSize));

                // Initialize the performance data buffer for each shader stage and finalize its GPU virtual address.
                for (uint32 s = 0; s < static_cast<uint32>(Abi::HardwareStage::Count); ++s)
                {
                    if (m_perfDataInfo[s].sizeInBytes != 0)
                    {
                        m_perfDataInfo[s].gpuVirtAddr =
                            LowPart(m_perfDataMem.GpuVirtAddr() + m_perfDataInfo[s].cpuOffset);
                    }
                } // for each hardware stage

                pGpuMem->Unmap();
            }
        }
    }

    if (result == Result::Success)
    {
        result = pUploader->Begin(abiProcessor, metadata, clientPreferredHeap);
    }

    if (result == Result::Success)
    {
        m_gpuMemSize = pUploader->GpuMemSize();
        m_gpuMem.Update(pUploader->GpuMem(), pUploader->GpuMemOffset());
    }

    return result;
}

// =====================================================================================================================
// Helper function for extracting the pipeline hash and per-shader hashes from pipeline metadata.
void Pipeline::ExtractPipelineInfo(
    const CodeObjectMetadata& metadata,
    ShaderType                firstShader,
    ShaderType                lastShader)
{
    m_info.internalPipelineHash =
        { metadata.pipeline.internalPipelineHash[0], metadata.pipeline.internalPipelineHash[1] };

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 476
    // Default the PAL runtime hash to the unique portion of the internal pipeline hash. PAL pipelines that include
    // additional state should override this with a new hash composed of that state and this hash.
    m_info.palRuntimeHash = m_info.internalPipelineHash.unique;
#endif

    // We don't expect the pipeline ABI to report a hash of zero.
    PAL_ALERT((metadata.pipeline.internalPipelineHash[0] | metadata.pipeline.internalPipelineHash[1]) == 0);

    for (uint32 s = static_cast<uint32>(firstShader); s <= static_cast<uint32>(lastShader); ++s)
    {
        Abi::ApiShaderType shaderType = PalToAbiShaderType[s];

        const auto& shaderMetadata = metadata.pipeline.shader[static_cast<uint32>(shaderType)];

        m_info.shader[s].hash = { shaderMetadata.apiShaderHash[0], shaderMetadata.apiShaderHash[1] };
        m_apiHwMapping.apiShaders[static_cast<uint32>(shaderType)] = static_cast<uint8>(shaderMetadata.hardwareMapping);
    }
}

// =====================================================================================================================
// Query this pipeline's Bound GPU Memory.
Result Pipeline::QueryAllocationInfo(
    size_t*                   pNumEntries,
    GpuMemSubAllocInfo* const pGpuMemList
    ) const
{
    Result result = Result::ErrorInvalidPointer;

    if (pNumEntries != nullptr)
    {
        (*pNumEntries) = 1;

        if (pGpuMemList != nullptr)
        {
            pGpuMemList[0].offset     = m_gpuMem.Offset();
            pGpuMemList[0].pGpuMemory = m_gpuMem.Memory();
            pGpuMemList[0].size       = m_gpuMemSize;
        }

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Extracts the pipeline's code object ELF binary.
Result Pipeline::GetPipelineElf(
    uint32*    pSize,
    void*      pBuffer
    ) const
{
    Result result = Result::ErrorInvalidPointer;

    if (pSize != nullptr)
    {
        if ((m_pPipelineBinary != nullptr) && (m_pipelineBinaryLen != 0))
        {
            if (pBuffer == nullptr)
            {
                (*pSize) = static_cast<uint32>(m_pipelineBinaryLen);
                result = Result::Success;
            }
            else if ((*pSize) >= static_cast<uint32>(m_pipelineBinaryLen))
            {
                memcpy(pBuffer, m_pPipelineBinary, m_pipelineBinaryLen);
                result = Result::Success;
            }
            else
            {
                result = Result::ErrorInvalidMemorySize;
            }
        }
        else
        {
            result = Result::ErrorUnavailable;
        }
    }

    return result;
}

// =====================================================================================================================
// Extracts the binary shader instructions for a specific API shader stage.
Result Pipeline::GetShaderCode(
    ShaderType shaderType,
    size_t*    pSize,
    void*      pBuffer
    ) const
{
    Result result = Result::ErrorUnavailable;

    const ShaderStageInfo*const pInfo = GetShaderStageInfo(shaderType);
    if (pSize == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (pInfo != nullptr)
    {
        PAL_ASSERT(pInfo->codeLength != 0); // How did we get here if there's no shader code?!

        if (pBuffer == nullptr)
        {
            (*pSize) = pInfo->codeLength;
            result   = Result::Success;
        }
        else if ((*pSize) >= pInfo->codeLength)
        {
            // To extract the shader code, we can re-parse the saved ELF binary and lookup the shader's program
            // instructions by examining the symbol table entry for that shader's entrypoint.
            AbiProcessor abiProcessor(m_pDevice->GetPlatform());
            result = abiProcessor.LoadFromBuffer(m_pPipelineBinary, m_pipelineBinaryLen);
            if (result == Result::Success)
            {
                const auto& symbol = abiProcessor.GetPipelineSymbolEntry(
                        Abi::GetSymbolForStage(Abi::PipelineSymbolType::ShaderMainEntry, pInfo->stageId));
                PAL_ASSERT(symbol.size == pInfo->codeLength);

                const void* pCodeSection   = nullptr;
                size_t      codeSectionLen = 0;
                abiProcessor.GetPipelineCode(&pCodeSection, &codeSectionLen);
                PAL_ASSERT((symbol.size + symbol.value) <= codeSectionLen);

                memcpy(pBuffer,
                       VoidPtrInc(pCodeSection, static_cast<size_t>(symbol.value)),
                       static_cast<size_t>(symbol.size));
            }
        }
        else
        {
            result = Result::ErrorInvalidMemorySize;
        }
    }

    return result;
}

// =====================================================================================================================
// Extracts the performance data from GPU memory and copies it to the specified buffer.
Result Pipeline::GetPerformanceData(
    Util::Abi::HardwareStage hardwareStage,
    size_t*                  pSize,
    void*                    pBuffer)
{
    Result       result       = Result::ErrorUnavailable;
    const uint32 index        = static_cast<uint32>(hardwareStage);
    const auto&  perfDataInfo = m_perfDataInfo[index];

    if (pSize == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (perfDataInfo.sizeInBytes > 0)
    {
        if (pBuffer == nullptr)
        {
            (*pSize) = perfDataInfo.sizeInBytes;
            result   = Result::Success;
        }
        else if ((*pSize) >= perfDataInfo.sizeInBytes)
        {
            auto pPerfDataMem = m_perfDataMem.Memory();
            void* pData       = nullptr;
            result            = pPerfDataMem->Map(&pData);

            if (result == Result::Success)
            {
                memcpy(pBuffer, VoidPtrInc(pData, perfDataInfo.cpuOffset), perfDataInfo.sizeInBytes);
                result = pPerfDataMem->Unmap();
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Helper method which extracts shader statistics from the pipeline ELF binary for a particular hardware stage.
Result Pipeline::GetShaderStatsForStage(
    const ShaderStageInfo& stageInfo,
    const ShaderStageInfo* pStageInfoCopy, // Optional: Non-null if we care about copy shader statistics.
    ShaderStats*           pStats
    ) const
{
    PAL_ASSERT(pStats != nullptr);
    memset(pStats, 0, sizeof(ShaderStats));

    // We can re-parse the saved pipeline ELF binary to extract shader statistics.
    AbiProcessor abiProcessor(m_pDevice->GetPlatform());
    Result result = abiProcessor.LoadFromBuffer(m_pPipelineBinary, m_pipelineBinaryLen);

    MsgPackReader      metadataReader;
    CodeObjectMetadata metadata;

    if (result == Result::Success)
    {
        result = abiProcessor.GetMetadata(&metadataReader, &metadata);
    }

    if (result == Result::Success)
    {
        const auto&  gpuInfo       = m_pDevice->ChipProperties();
        const auto&  stageMetadata = metadata.pipeline.hardwareStage[static_cast<uint32>(stageInfo.stageId)];

        pStats->common.numUsedSgprs = stageMetadata.sgprCount;
        pStats->common.numUsedVgprs = stageMetadata.vgprCount;

#if PAL_BUILD_GFX6
        if (gpuInfo.gfxLevel < GfxIpLevel::GfxIp9)
        {
            pStats->numAvailableSgprs = (stageMetadata.hasEntry.sgprLimit != 0) ? stageMetadata.sgprLimit
                                                                                : gpuInfo.gfx6.numShaderVisibleSgprs;
            pStats->numAvailableVgprs = (stageMetadata.hasEntry.vgprLimit != 0) ? stageMetadata.vgprLimit
                                                                                : gpuInfo.gfx6.numPhysicalVgprsPerSimd;
        }
#endif

        if (gpuInfo.gfxLevel >= GfxIpLevel::GfxIp9)
        {
            pStats->numAvailableSgprs = (stageMetadata.hasEntry.sgprLimit != 0) ? stageMetadata.sgprLimit
                                                                                : gpuInfo.gfx9.numShaderVisibleSgprs;
            pStats->numAvailableVgprs = (stageMetadata.hasEntry.vgprLimit != 0) ? stageMetadata.vgprLimit
                                                                                : gpuInfo.gfx9.numPhysicalVgprsPerSimd;
        }

        pStats->common.ldsUsageSizeInBytes    = stageMetadata.ldsSize;
        pStats->common.scratchMemUsageInBytes = stageMetadata.scratchMemorySize;

        pStats->isaSizeInBytes = stageInfo.disassemblyLength;

        if (pStageInfoCopy != nullptr)
        {
            const auto& copyStageMetadata =
                metadata.pipeline.hardwareStage[static_cast<uint32>(pStageInfoCopy->stageId)];

            pStats->flags.copyShaderPresent = 1;

            pStats->copyShader.numUsedSgprs = copyStageMetadata.sgprCount;
            pStats->copyShader.numUsedVgprs = copyStageMetadata.vgprCount;

            pStats->copyShader.ldsUsageSizeInBytes    = copyStageMetadata.ldsSize;
            pStats->copyShader.scratchMemUsageInBytes = copyStageMetadata.scratchMemorySize;
        }
    }

    return result;
}

// =====================================================================================================================
// Calculates the size, in bytes, of the performance data buffers needed total for the entire pipeline.
size_t Pipeline::PerformanceDataSize(
    const CodeObjectMetadata& metadata
    ) const
{
    size_t dataSize = 0;

    for (uint32 i = 0; i < static_cast<uint32>(Abi::HardwareStage::Count); i++)
    {
        dataSize += metadata.pipeline.hardwareStage[i].perfDataBufferSize;
    }

    return dataSize;
}

// =====================================================================================================================
void Pipeline::DumpPipelineElf(
    const AbiProcessor& abiProcessor,
    const char*         pPrefix,
    const char*         pName         // Optional: Non-null if we want to use a human-readable name for the filename.
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    const PalSettings& settings = m_pDevice->Settings();
    uint64 hashToDump = settings.pipelineLogConfig.logPipelineHash;
    bool hashMatches = ((hashToDump == 0) || (m_info.internalPipelineHash.stable == hashToDump));

    const bool dumpInternal  = settings.pipelineLogConfig.logInternal;
    const bool dumpExternal  = settings.pipelineLogConfig.logExternal;
    const bool dumpPipeline  =
        (hashMatches && ((dumpExternal && !IsInternal()) || (dumpInternal && IsInternal())));

    if (dumpPipeline)
    {
        const char*const pLogDir = &settings.pipelineLogConfig.pipelineLogDirectory[0];

        // Create the directory. We don't care if it fails (existing is fine, failure is caught when opening the file).
        MkDir(pLogDir);

        char fileName[512] = { };
        if ((pName == nullptr) || (pName[0] == '\0'))
        {
            Snprintf(&fileName[0],
                     sizeof(fileName),
                     "%s/%s_0x%016llX.elf",
                     pLogDir,
                     pPrefix,
                     m_info.internalPipelineHash.stable);
        }
        else
        {
            Snprintf(&fileName[0], sizeof(fileName), "%s/%s_%s.elf", pLogDir, pPrefix, pName);
        }

        File file;
        file.Open(fileName, FileAccessWrite | FileAccessBinary);
        file.Write(m_pPipelineBinary, m_pipelineBinaryLen);
    }
#endif
}

// =====================================================================================================================
PipelineUploader::PipelineUploader(
    Device* pDevice,
    uint32  ctxRegisterCount,
    uint32  shRegisterCount)
    :
    m_pDevice(pDevice),
    m_pGpuMemory(nullptr),
    m_baseOffset(0),
    m_gpuMemSize(0),
    m_pUploadGpuMem(nullptr),
    m_uploadOffset(0),
    m_codeGpuVirtAddr(0),
    m_dataGpuVirtAddr(0),
    m_ctxRegGpuVirtAddr(0),
    m_shRegGpuVirtAddr(0),
    m_shRegisterCount(shRegisterCount),
    m_ctxRegisterCount(ctxRegisterCount),
    m_pMappedPtr(nullptr),
    m_pCtxRegWritePtr(nullptr),
    m_pShRegWritePtr(nullptr)
#if PAL_ENABLE_PRINTS_ASSERTS
    , m_pCtxRegWritePtrStart(nullptr),
    m_pShRegWritePtrStart(nullptr)
#endif
    , m_pipelineHeapType(GpuHeap::GpuHeapCount)
{
}

// =====================================================================================================================
PipelineUploader::~PipelineUploader()
{
    PAL_ASSERT(m_pMappedPtr == nullptr); // If this fires, the caller forgot to call End()!
}

// =====================================================================================================================
// Allocates GPU memory for the current pipeline.  Also, maps the memory for CPU access and uploads the pipeline code
// and data.  The GPU virtual addresses for the code, data, and register segments are also computed.  The caller is
// responsible for calling End() which unmaps the GPU memory.
Result PipelineUploader::Begin(
    const AbiProcessor&       abiProcessor,
    const CodeObjectMetadata& metadata,
    const GpuHeap&            clientPreferredHeap)
{
    static_assert(static_cast<uint32>(PreferredPipelineUploadHeap::PipelineHeapLocal)         ==
        static_cast<uint32>(GpuHeap::GpuHeapLocal),         "Pipeline heap enumeration needs to be updated!");
    static_assert(static_cast<uint32>(PreferredPipelineUploadHeap::PipelineHeapInvisible)     ==
        static_cast<uint32>(GpuHeap::GpuHeapInvisible),     "Pipeline heap enumeration needs to be updated!");
    static_assert(static_cast<uint32>(PreferredPipelineUploadHeap::PipelineHeapGartUswc)      ==
        static_cast<uint32>(GpuHeap::GpuHeapGartUswc),      "Pipeline heap enumeration needs to be updated!");
    static_assert(static_cast<uint32>(PreferredPipelineUploadHeap::PipelineHeapGartCacheable) ==
        static_cast<uint32>(GpuHeap::GpuHeapGartCacheable), "Pipeline heap enumeration needs to be updated!");
    static_assert(static_cast<uint32>(PreferredPipelineUploadHeap::PipelineHeapDeferToClient) ==
        static_cast<uint32>(GpuHeap::GpuHeapCount),         "Pipeline heap enumeration needs to be updated!");

    const auto& settingPreferredHeap = m_pDevice->Settings().preferredPipelineUploadHeap;

    // Compute the final destination heap of the pipeline.
    if (settingPreferredHeap == PreferredPipelineUploadHeap::PipelineHeapDeferToClient)
    {
        // The panel setting is set to use the client specified heap.
        m_pipelineHeapType = clientPreferredHeap;
    }
    else
    {
        // Non-default panel setting.
        m_pipelineHeapType = static_cast<GpuHeap>(settingPreferredHeap);
    }

    if (m_pDevice->ValidatePipelineUploadHeap(m_pipelineHeapType) == false)
    {
        // Fall back to local visible heap.
        m_pipelineHeapType = GpuHeap::GpuHeapLocal;

        // We cannot upload to this heap for this device. We will fall back to using the optimal heap instead.
        PAL_ALERT(m_pDevice->ValidatePipelineUploadHeap(clientPreferredHeap));
    }

    GpuMemoryCreateInfo createInfo = { };
    createInfo.alignment           = GpuMemByteAlign;
    createInfo.vaRange             = VaRange::DescriptorTable;
    createInfo.heaps[0]            = m_pipelineHeapType;
    createInfo.heaps[1]            = GpuHeapGartUswc;
    createInfo.heapCount           = 2;
    createInfo.priority            = GpuMemPriority::High;

    GpuMemoryInternalCreateInfo internalInfo = { };
    internalInfo.flags.alwaysResident        = 1;

    const void* pCodeBuffer = nullptr;
    size_t      codeLength  = 0;
    abiProcessor.GetPipelineCode(&pCodeBuffer, &codeLength);

    createInfo.size = codeLength;

    const void* pDataBuffer   = nullptr;
    size_t      dataLength    = 0;
    gpusize     dataAlignment = 0;
    abiProcessor.GetData(&pDataBuffer, &dataLength, &dataAlignment);

    if (dataLength > 0)
    {
        createInfo.size = (Pow2Align(createInfo.size, dataAlignment) + dataLength);
    }

    const uint32 totalRegisters = (m_ctxRegisterCount + m_shRegisterCount);
    if (totalRegisters > 0)
    {
        constexpr uint32 RegisterEntryBytes = (sizeof(uint32) << 1);
        createInfo.size = (Pow2Align(createInfo.size, sizeof(uint32)) + (RegisterEntryBytes * totalRegisters));
    }

    // The driver must make sure there is a distance of at least gpuInfo.shaderPrefetchBytes
    // that follows the end of the shader to avoid a page fault when the SQ tries to
    // prefetch past the end of a shader

    // shaderPrefetchBytes is set from "SQC_CONFIG.INST_PRF_COUNT" (gfx8-9)
    // defaulting to the hardware supported maximum if necessary

    const gpusize minSafeSize = Pow2Align(codeLength, ShaderICacheLineSize) +
                                m_pDevice->ChipProperties().gfxip.shaderPrefetchBytes;

    createInfo.size = Max(createInfo.size, minSafeSize);

    m_gpuMemSize  = createInfo.size;
    Result result = m_pDevice->MemMgr()->AllocateGpuMem(createInfo, internalInfo, false, &m_pGpuMemory, &m_baseOffset);

    if (result == Result::Success)
    {
        if (m_pipelineHeapType != GpuHeap::GpuHeapInvisible)
        {
            result       = m_pGpuMemory->Map(&m_pMappedPtr);
            m_pMappedPtr = VoidPtrInc(m_pMappedPtr, static_cast<size_t>(m_baseOffset));
        }
        else
        {
            m_pMappedPtr = PAL_CALLOC_ALIGNED(static_cast<size_t>(m_gpuMemSize),
                                              GpuMemByteAlign,
                                              m_pDevice->GetPlatform(),
                                              AllocInternal);
            if (m_pMappedPtr == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
        }

        if (result == Result::Success)
        {
            gpusize gpuVirtAddr = (m_pGpuMemory->Desc().gpuVirtAddr + m_baseOffset);
            void* pMappedPtr    = m_pMappedPtr;
            m_codeGpuVirtAddr   = gpuVirtAddr;

            memcpy(pMappedPtr, pCodeBuffer, codeLength);

            pMappedPtr   = VoidPtrInc(pMappedPtr, codeLength);
            gpuVirtAddr += codeLength;

            m_prefetchGpuVirtAddr = m_codeGpuVirtAddr;
            m_prefetchSize        = codeLength;

            if (dataLength > 0)
            {
                pMappedPtr  = VoidPtrAlign(pMappedPtr, static_cast<size_t>(dataAlignment));
                gpuVirtAddr = Pow2Align(gpuVirtAddr, dataAlignment);

                m_dataGpuVirtAddr = gpuVirtAddr;
                memcpy(pMappedPtr, pDataBuffer, dataLength);

                // The for loop which follows is entirely non-standard behavior for an ELF loader, but is intended to
                // only be temporary code.
                for (uint32 s = 0; s < static_cast<uint32>(Abi::HardwareStage::Count); ++s)
                {
                    const Abi::PipelineSymbolType symbolType =
                        Abi::GetSymbolForStage(Abi::PipelineSymbolType::ShaderIntrlTblPtr,
                                               static_cast<Abi::HardwareStage>(s));

                    Abi::PipelineSymbolEntry symbol = { };
                    if (abiProcessor.HasPipelineSymbolEntry(symbolType, &symbol) &&
                        (symbol.sectionType == Abi::AbiSectionType::Data))
                    {
                        m_pDevice->GetGfxDevice()->PatchPipelineInternalSrdTable(
                            VoidPtrInc(pMappedPtr,  static_cast<size_t>(symbol.value)), // Dst
                            VoidPtrInc(pDataBuffer, static_cast<size_t>(symbol.value)), // Src
                            static_cast<size_t>(symbol.size),
                            m_dataGpuVirtAddr);
                    }
                } // for each hardware stage
                // End temporary code

                pMappedPtr   = VoidPtrInc(pMappedPtr, dataLength);
                gpuVirtAddr += dataLength;

                m_prefetchSize = gpuVirtAddr - m_prefetchGpuVirtAddr;
            } // if dataLength > 0

            if (totalRegisters > 0)
            {
                gpusize regGpuVirtAddr = Pow2Align(gpuVirtAddr, sizeof(uint32));
                uint32* pRegWritePtr   = static_cast<uint32*>(VoidPtrAlign(pMappedPtr, sizeof(uint32)));

                if (m_ctxRegisterCount > 0)
                {
                    m_ctxRegGpuVirtAddr = regGpuVirtAddr;
                    m_pCtxRegWritePtr   = pRegWritePtr;

                    regGpuVirtAddr += (m_ctxRegisterCount * (sizeof(uint32) * 2));
                    pRegWritePtr   += (m_ctxRegisterCount * 2);
                }

                if (m_shRegisterCount > 0)
                {
                    m_shRegGpuVirtAddr = regGpuVirtAddr;
                    m_pShRegWritePtr   = pRegWritePtr;
                }

#if PAL_ENABLE_PRINTS_ASSERTS
                m_pCtxRegWritePtrStart = m_pCtxRegWritePtr;
                m_pShRegWritePtrStart  = m_pShRegWritePtr;
#endif
            }
        } // if Map() succeeded
    } // if AllocateGpuMem() succeeded

    return result;
}

// =====================================================================================================================
// "Finishes" uploading a pipeline to GPU memory by requesting the device to submit a DMA copy of the pipeline from
// its initial heap to the local invisible heap. The temporary CPU visible heap is freed.
Result PipelineUploader::End()
{
    Result result = Result::Success;

    if ((m_pGpuMemory != nullptr) && (m_pMappedPtr != nullptr))
    {
        // Sanity check to make sure we allocated the correct amount of memory for any loaded SH or context registers.
#if PAL_ENABLE_PRINTS_ASSERTS
        PAL_ASSERT(m_pCtxRegWritePtr == (m_pCtxRegWritePtrStart + (m_ctxRegisterCount * 2)));
        PAL_ASSERT(m_pShRegWritePtr  == (m_pShRegWritePtrStart  + (m_shRegisterCount  * 2)));

        m_pCtxRegWritePtrStart = nullptr;
        m_pShRegWritePtrStart  = nullptr;
#endif
        m_pCtxRegWritePtr = nullptr;
        m_pShRegWritePtr  = nullptr;

        if (m_pipelineHeapType == GpuHeap::GpuHeapInvisible)
        {
            result = m_pDevice->CopyUsingEmbeddedData(m_pMappedPtr, m_gpuMemSize, m_baseOffset, m_pGpuMemory);
            PAL_SAFE_FREE(m_pMappedPtr, m_pDevice->GetPlatform());
        }
        else
        {
            m_pGpuMemory->Unmap();
        }

        m_pMappedPtr = nullptr;
    }

    return result;
}

// =====================================================================================================================
Result PipelineUploader::CreateUploadCmdBuffer()
{
    // Perform a DMA copy to the final destination.
    CmdBufferCreateInfo cmdBufCreateInfo = { };
    cmdBufCreateInfo.engineType          = EngineType::EngineTypeDma;
    cmdBufCreateInfo.queueType           = QueueType::QueueTypeDma;
    cmdBufCreateInfo.pCmdAllocator       = m_pDevice->InternalCmdAllocator(EngineType::EngineTypeDma);

    CmdBufferInternalCreateInfo cmdBufInternalCreateInfo = { };
    cmdBufInternalCreateInfo.flags.isInternal            = true;

    return m_pDevice->CreateInternalCmdBuffer(cmdBufCreateInfo,
                                              cmdBufInternalCreateInfo,
                                              &m_pUploadCmdBuffer);
}

} // Pal
