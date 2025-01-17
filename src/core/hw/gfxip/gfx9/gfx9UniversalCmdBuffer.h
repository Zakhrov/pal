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

#pragma once

#include "core/hw/gfxip/universalCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9Gds.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9WorkaroundState.h"
#include "core/hw/gfxip/gfx9/g_gfx9PalSettings.h"
#include "palIntervalTree.h"

#include "palPipelineAbi.h"

namespace Pal
{
namespace Gfx9
{

class GraphicsPipeline;
class UniversalCmdBuffer;

// Structure to track the state of internal command buffer operations.
struct UniversalCmdBufferState
{
    union
    {
        struct
        {
            // Tracks whether or not *ANY* piece of ring memory being dumped-to by the CE (by PAL or the client) has
            // wrapped back to the beginning within this command buffer. If no ring has wrapped yet, there is no need
            // to ever stall the CE from getting too far ahead or to ask the DE to invalidate the Kcache for us.
            uint32 ceHasAnyRingWrapped   :  1;
            // CE memory dumps go through the L2 cache, but not the L1 cache! In order for the shader cores to read
            // correct data out of piece of ring memory, we need to occasionally invalidate the Kcache when waiting
            // for the CE to finish dumping its memory. If set, the next INCREMENT_CE_COUNTER inserted into the DE
            // stream should also invalidate the Kcache.
            uint32 ceInvalidateKcache    :  1;
            uint32 ceWaitOnDeCounterDiff :  1;
            uint32 deCounterDirty        :  1;
            uint32 paScAaConfigUpdated   :  1;
            uint32 containsDrawIndirect  :  1;
            uint32 optimizeLinearGfxCpy  :  1;
            uint32 firstDrawExecuted     :  1;
            uint32 fsrEnabled            :  1;
            uint32 cbTargetMaskChanged   :  1; // Flag setup at Pipeline bind-time informing the draw-time set
                                               // that the CB_TARGET_MASK has been changed.
            uint32 reserved              : 22;
        };
        uint32 u32All;
    } flags;
    // According to the UDX implementation, CP uCode and CE programming guide, the ideal DE counter diff amount we
    // should ask the CE to wait for is 1/4 the minimum size (in entries!) of all pieces of memory being ringed.
    // Thus we only need to track this minimum diff amount. If ceWaitOnDeCounterDiff flag is also set, the CE will
    // be asked to wait for a DE counter diff at the next Draw or Dispatch.
    uint32  minCounterDiff;

    // If non-null, points to the most recent DUMP_CONST_RAM or DUMP_CONST_RAM_OFFSET packet written into the CE cmd
    // stream.  If null, then no DUMP_CONST_RAM_* packets have been written since the previous Draw or Dispatch.
    uint32*               pLastDumpCeRam;
    // Stores the 2nd ordinal of the most-recent DUMP_CONST_RAM_* packet to avoid a read-modify-write when updating
    // that packet to set the increment_ce bit.
    DumpConstRamOrdinal2  lastDumpCeRamOrdinal2;

    // Copy of what will be written into CE RAM for NGG pipelines.
    Util::Abi::PrimShaderCbLayout  primShaderCbLayout;
};

// Represents an "image" of the PM4 headers necessary to write NULL depth-stencil state to hardware. The required
// register writes are grouped into sets based on sequential register addresses, so that we can minimize the amount
// of PM4 space needed by setting several reg's in each packet.
struct NullDepthStencilPm4Img
{
    PM4PFP_SET_CONTEXT_REG       hdrDbRenderOverride2;
    regDB_RENDER_OVERRIDE2       dbRenderOverride2;
    regDB_HTILE_DATA_BASE        dbHtileDataBase;

    PM4PFP_SET_CONTEXT_REG       hdrDbRenderControl;
    regDB_RENDER_CONTROL         dbRenderControl;

    // Note:  this must be last, because the size of this union is a constant, but the number of registers
    //        written to the GPU is of a variable length.  This struct is copied into the PM4 stream "as is",
    //        so any blank / unused spaces need to be at the end.
    PM4PFP_SET_CONTEXT_REG       hdrDbInfo;
    union
    {
        struct
        {
            regDB_Z_INFO                 dbZInfo;
            regDB_STENCIL_INFO           dbStencilInfo;
        } gfx9;

        struct
        {
            uint32                       dbDepthInfo;
            regDB_Z_INFO                 dbZInfo;
            regDB_STENCIL_INFO           dbStencilInfo;

            ///@note Writing HI base addresses in the preamble as they are known to be 0 always.
        } gfx10;
    };
};

// Structure used by UniversalCmdBuffer to track particular bits of hardware state that might need to be updated
// per-draw. Note that the 'valid' flags exist to indicate when we don't know the actual value of certain state. For
// example, we don't know what NUM_INSTANCES is set to at the beginning of a command buffer or after an indirect draw.
// WARNING: If you change anything in here please update ValidateDrawTimeHwState.
struct DrawTimeHwState
{
    union
    {
        struct
        {
            uint32 instanceOffset         :  1; // Set when instanceOffset matches the HW value.
            uint32 vertexOffset           :  1; // Set when vertexOffset matches the HW value.
            uint32 drawIndex              :  1; // Set when drawIndex matches the HW value.
            uint32 indexOffset            :  1; // Set when startIndex matches the HW value.
            uint32 log2IndexSize          :  1; // Set when log2IndexSize matches the HW value.
            uint32 numInstances           :  1; // Set when numInstances matches the HW value.
            uint32 paScModeCntl1          :  1; // Set when paScModeCntl1 matches the HW value.
            uint32 dbCountControl         :  1; // Set when dbCountControl matches the HW value.
            uint32 vgtMultiPrimIbResetEn  :  1; // Set when vgtMultiPrimIbResetEn matches the HW value.
            uint32 nggIndexBufferBaseAddr :  1; // Set when nggIndexBufferBaseAddr matches the HW value.
            uint32 reserved               : 22; // Reserved bits
        };
        uint32     u32All;                // The flags as a single integer.
    } valid;                              // Draw state valid flags.

    union
    {
        struct
        {
            uint32 indexType        :  1; // Set when the index type is dirty
            uint32 indexBufferBase  :  1; // Set when the index buffer base address is dirty
            uint32 indexBufferSize  :  1; // Set when the index buffer size is dirty
            uint32 indexedIndexType :  1; // Set when the index type is dirty and needs to be rewritten for the next
                                          // indexed draw.
            uint32 reserved         : 28; // Reserved bits
        };
        uint32 u32All;                   // The flags as a single integer.
    } dirty;                             // Draw state dirty flags. If any of these are set, the next call to
                                         // ValidateDrawTimeHwState needs to write them.

    uint32                        instanceOffset;            // Current value of the instance offset user data.
    uint32                        vertexOffset;              // Current value of the vertex offset user data.
    uint32                        startIndex;                // Current value of the start index user data.
    uint32                        log2IndexSize;             // Current value of the Log2(sizeof(indexType)) user data.
    uint32                        numInstances;              // Current value of the NUM_INSTANCES state.
    regPA_SC_MODE_CNTL_1          paScModeCntl1;             // Current value of the PA_SC_MODE_CNTL1 register.
    regDB_COUNT_CONTROL           dbCountControl;            // Current value of the DB_COUNT_CONTROL register.
    regVGT_MULTI_PRIM_IB_RESET_EN vgtMultiPrimIbResetEn;     // Current value of the VGT_MULTI_PRIM_IB_RESET_EN
                                                             // register.
    gpusize                       nggIndexBufferBaseAddr;    // Current value of the IndexBufferBaseAddr for NGG.
    gpusize                       nggIndexBufferPfStartAddr; // Start address of last IndexBuffer prefetch for NGG.
    gpusize                       nggIndexBufferPfEndAddr;   // End address of last IndexBuffer prefetch for NGG.
};

struct ColorInfoReg
{
    PM4PFP_SET_CONTEXT_REG header;
    regCB_COLOR0_INFO      cbColorInfo;
};

struct ScreenScissorReg
{
    PM4PFP_SET_CONTEXT_REG     hdrPaScScreenScissors;
    regPA_SC_SCREEN_SCISSOR_TL paScScreenScissorTl;
    regPA_SC_SCREEN_SCISSOR_BR paScScreenScissorBr;
};

constexpr size_t MaxNullColorTargetPm4ImgSize = sizeof(ColorInfoReg) * MaxColorTargets;

struct BlendConstReg
{
    PM4_PFP_SET_CONTEXT_REG header;
    regCB_BLEND_RED         red;
    regCB_BLEND_GREEN       green;
    regCB_BLEND_BLUE        blue;
    regCB_BLEND_ALPHA       alpha;
};

struct InputAssemblyStatePm4Img
{
    PM4_PFP_SET_UCONFIG_REG             hdrPrimType;
    regVGT_PRIMITIVE_TYPE               primType;

    PM4_PFP_SET_CONTEXT_REG             hdrVgtMultiPrimIbResetIndex;
    regVGT_MULTI_PRIM_IB_RESET_INDX     vgtMultiPrimIbResetIndex;
};

struct StencilRefMasksReg
{
    PM4_PFP_SET_CONTEXT_REG header;
    regDB_STENCILREFMASK    dbStencilRefMaskFront;
    regDB_STENCILREFMASK_BF dbStencilRefMaskBack;
};

struct StencilRefMaskRmwReg
{
    PM4_ME_REG_RMW dbStencilRefMaskFront;
    PM4_ME_REG_RMW dbStencilRefMaskBack;
};

constexpr size_t MaxStencilSetPm4ImgSize = sizeof(StencilRefMasksReg) > sizeof(StencilRefMaskRmwReg) ?
                                           sizeof(StencilRefMasksReg) : sizeof(StencilRefMaskRmwReg);
struct DepthBoundsStateReg
{
    PM4_PFP_SET_CONTEXT_REG header;
    regDB_DEPTH_BOUNDS_MIN  dbDepthBoundsMin;
    regDB_DEPTH_BOUNDS_MAX  dbDepthBoundsMax;
};

struct TriangleRasterStateReg
{
    PM4_PFP_SET_CONTEXT_REG header;
    regPA_SU_SC_MODE_CNTL   paSuScModeCntl;
};

struct DepthBiasStateReg
{
    PM4_PFP_SET_CONTEXT_REG           header;
    regPA_SU_POLY_OFFSET_CLAMP        paSuPolyOffsetClamp;       // Poly offset clamp value
    regPA_SU_POLY_OFFSET_FRONT_SCALE  paSuPolyOffsetFrontScale;  // Front-facing poly scale
    regPA_SU_POLY_OFFSET_FRONT_OFFSET paSuPolyOffsetFrontOffset; // Front-facing poly offset
    regPA_SU_POLY_OFFSET_BACK_SCALE   paSuPolyOffsetBackScale;   // Back-facing poly scale
    regPA_SU_POLY_OFFSET_BACK_OFFSET  paSuPolyOffsetBackOffset;  // Back-facing poly offset
};

struct PointLineRasterStateReg
{
    PM4_PFP_SET_CONTEXT_REG paSuHeader;
    regPA_SU_POINT_SIZE     paSuPointSize;
    regPA_SU_POINT_MINMAX   paSuPointMinMax;
    regPA_SU_LINE_CNTL      paSuLineCntl;
};

struct GlobalScissorReg
{
    PM4_PFP_SET_CONTEXT_REG    header;
    regPA_SC_WINDOW_SCISSOR_TL topLeft;
    regPA_SC_WINDOW_SCISSOR_BR bottomRight;
};

// Register state for a single viewport's X,Y,Z scales and offsets.
struct VportScaleOffsetPm4Img
{
    regPA_CL_VPORT_XSCALE  xScale;
    regPA_CL_VPORT_XOFFSET xOffset;
    regPA_CL_VPORT_YSCALE  yScale;
    regPA_CL_VPORT_YOFFSET yOffset;
    regPA_CL_VPORT_ZSCALE  zScale;
    regPA_CL_VPORT_ZOFFSET zOffset;
};

// Register state for a single viewport's Z min and max bounds.
struct VportZMinMaxPm4Img
{
    regPA_SC_VPORT_ZMIN_0 zMin;
    regPA_SC_VPORT_ZMAX_0 zMax;
};

// Register state for the clip guardband.
struct GuardbandPm4Img
{
    regPA_CL_GB_VERT_CLIP_ADJ paClGbVertClipAdj;
    regPA_CL_GB_VERT_DISC_ADJ paClGbVertDiscAdj;
    regPA_CL_GB_HORZ_CLIP_ADJ paClGbHorzClipAdj;
    regPA_CL_GB_HORZ_DISC_ADJ paClGbHorzDiscAdj;
};

// Register state for a single scissor rect.
struct ScissorRectPm4Img
{
    regPA_SC_VPORT_SCISSOR_0_TL tl;
    regPA_SC_VPORT_SCISSOR_0_BR br;
};

// Register state for a single plane's x y z and w coordinates.
struct UserClipPlaneStateReg
{
    regPA_CL_UCP_0_X            paClUcpX;
    regPA_CL_UCP_0_Y            paClUcpY;
    regPA_CL_UCP_0_Z            paClUcpZ;
    regPA_CL_UCP_0_W            paClUcpW;
};

// Command for setting up user clip planes.
struct UserClipPlaneStatePm4Img
{
    PM4_PFP_SET_CONTEXT_REG     header;
    UserClipPlaneStateReg       plane[6];
};

// PM4 image for loading context registers from memory
struct LoadDataIndexPm4Img
{
    // PM4 load context regs packet to load the register data from memory
    union
    {
        PM4PFP_LOAD_CONTEXT_REG       loadData;
        PM4PFP_LOAD_CONTEXT_REG_INDEX loadDataIndex;
    };

    // Command space needed, in DWORDs. This field must always be last in the structure to not
    // interfere w/ the actual commands contained within.
    size_t                            spaceNeeded;

};

// Represents an image of the PM4 commands necessary to write RB-plus related info to hardware.
struct RbPlusPm4Img
{
    PM4_PFP_SET_CONTEXT_REG  header;
    regSX_PS_DOWNCONVERT     sxPsDownconvert;
    regSX_BLEND_OPT_EPSILON  sxBlendOptEpsilon;
    regSX_BLEND_OPT_CONTROL  sxBlendOptControl;

    size_t  spaceNeeded;
};

// All NGG related state tracking.
struct NggState
{
    union
    {
        struct
        {
            union
            {
                struct
                {
                    uint8 hasPrimShaderWorkload : 1;
                    uint8 reserved              : 7;
                };
                uint8 u8All;
            } state;

            union
            {
                struct
                {
                    uint8 viewports           : 1;
                    uint8 triangleRasterState : 1;
                    uint8 inputAssemblyState  : 1;
                    uint8 msaaState           : 1;
                    uint8 reserved            : 4;
                };
                uint8 u8All;
            } dirty;
        };
        uint16 u16All;
    } flags;

    uint32 numSamples;          // Number of active MSAA samples.
    uint16 startIndexReg;       // Register where the index start offset is written
    uint16 log2IndexSizeReg;    // Register where the Log2(sizeof(indexType)) is written
};

// Register state for a clip rectangle's left top and right bottom parameters.
struct ClipRectsStateReg
{
    regPA_SC_CLIPRECT_0_TL    paScClipRectTl;
    regPA_SC_CLIPRECT_0_BR    paScClipRectBr;
};

// Command for setting up clip rects
struct ClipRectsPm4Img
{
    PM4_PFP_SET_CONTEXT_REG header;
    regPA_SC_CLIPRECT_RULE  paScClipRectRule;
    ClipRectsStateReg       rects[MaxClipRects];
};

// =====================================================================================================================
// GFX9 universal command buffer class: implements GFX9 specific functionality for the UniversalCmdBuffer class.
class UniversalCmdBuffer : public Pal::UniversalCmdBuffer
{
    // Shorthand for function pointers which validate graphics user-data at Draw-time.
    typedef uint32* (UniversalCmdBuffer::*ValidateUserDataGfxFunc)(const GraphicsPipelineSignature*, uint32*);

public:
    static size_t GetSize(const Device& device);

    UniversalCmdBuffer(const Device& device, const CmdBufferCreateInfo& createInfo);

    virtual Result Init(const CmdBufferInternalCreateInfo& internalInfo) override;

    virtual void CmdBindPipeline(
        const PipelineBindParams& params) override;

    virtual void CmdBindIndexData(gpusize gpuAddr, uint32 indexCount, IndexType indexType) override;
    virtual void CmdBindMsaaState(const IMsaaState* pMsaaState) override;
    virtual void CmdBindColorBlendState(const IColorBlendState* pColorBlendState) override;
    virtual void CmdBindDepthStencilState(const IDepthStencilState* pDepthStencilState) override;

    virtual void CmdSetBlendConst(const BlendConstParams& params) override;
    virtual void CmdSetInputAssemblyState(const InputAssemblyStateParams& params) override;
    virtual void CmdSetStencilRefMasks(const StencilRefMaskParams& params) override;
    virtual void CmdSetDepthBounds(const DepthBoundsParams& params) override;
    virtual void CmdSetTriangleRasterState(const TriangleRasterStateParams& params) override;
    virtual void CmdSetDepthBiasState(const DepthBiasParams& params) override;
    virtual void CmdSetPointLineRasterState(const PointLineRasterStateParams& params) override;
    virtual void CmdSetMsaaQuadSamplePattern(uint32                       numSamplesPerPixel,
                                             const MsaaQuadSamplePattern& quadSamplePattern) override;
    virtual void CmdSetViewports(const ViewportParams& params) override;
    virtual void CmdSetScissorRects(const ScissorRectParams& params) override;
    virtual void CmdSetGlobalScissor(const GlobalScissorParams& params) override;
    virtual void CmdSetUserClipPlanes(uint32               firstPlane,
                                      uint32               planeCount,
                                      const UserClipPlane* pPlanes) override;
    virtual void CmdSetClipRects(uint16      clipRule,
                                 uint32      rectCount,
                                 const Rect* pRectList) override;
    virtual void CmdFlglSync() override;
    virtual void CmdFlglEnable() override;
    virtual void CmdFlglDisable() override;

    static uint32* BuildSetBlendConst(const BlendConstParams& params, const CmdUtil& cmdUtil, uint32* pCmdSpace);
    static uint32* BuildSetInputAssemblyState(
        const InputAssemblyStateParams& params,
        const Device&                   device,
        uint32*                         pCmdSpace);
    static uint32* BuildSetStencilRefMasks(
        const StencilRefMaskParams& params,
        const CmdUtil&              cmdUtil,
        uint32*                     pCmdSpace);
    static uint32* BuildSetDepthBounds(const DepthBoundsParams& params, const CmdUtil& cmdUtil, uint32* pCmdSpace);
    static uint32* BuildSetDepthBiasState(const DepthBiasParams& params, const CmdUtil& cmdUtil, uint32* pCmdSpace);
    static uint32* BuildSetPointLineRasterState(
        const PointLineRasterStateParams& params,
        const CmdUtil&                    cmdUtil,
        uint32*                           pCmdSpace);
    static uint32* BuildSetGlobalScissor(
        const GlobalScissorParams& params,
        const CmdUtil&             cmdUtil,
        uint32*                    pCmdSpace);
    static uint32* BuildSetUserClipPlane(uint32               firstPlane,
                                         uint32               count,
                                         const UserClipPlane* pPlanes,
                                         const CmdUtil&       cmdUtil,
                                         uint32*              pCmdSpace);

    virtual void CmdBarrier(const BarrierInfo& barrierInfo) override;

    virtual void CmdRelease(
        const AcquireReleaseInfo& releaseInfo,
        const IGpuEvent*          pGpuEvent) override;

    virtual void CmdAcquire(
        const AcquireReleaseInfo& acquireInfo,
        uint32                    gpuEventCount,
        const IGpuEvent*const*    ppGpuEvents) override;

    virtual void CmdReleaseThenAcquire(const AcquireReleaseInfo& barrierInfo) override;

    virtual void CmdSetVertexBuffers(
        uint32                firstBuffer,
        uint32                bufferCount,
        const BufferViewInfo* pBuffers) override;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 473
    virtual void CmdSetIndirectUserData(
        uint16      tableId,
        uint32      dwordOffset,
        uint32      dwordSize,
        const void* pSrcData) override;
    virtual void CmdSetIndirectUserDataWatermark(
        uint16 tableId,
        uint32 dwordLimit) override;
#endif

    virtual void CmdBindTargets(const BindTargetParams& params) override;
    virtual void CmdBindStreamOutTargets(const BindStreamOutTargetParams& params) override;

    virtual void CmdCloneImageData(const IImage& srcImage, const IImage& dstImage) override;

    virtual void CmdCopyRegisterToMemory(
        uint32            srcRegisterOffset,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset) override;

    virtual void CmdCopyMemory(
        const IGpuMemory&       srcGpuMemory,
        const IGpuMemory&       dstGpuMemory,
        uint32                  regionCount,
        const MemoryCopyRegion* pRegions) override;

    virtual void CmdUpdateMemory(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        gpusize           dataSize,
        const uint32*     pData) override;

    virtual void CmdUpdateBusAddressableMemoryMarker(
        const IGpuMemory& dstGpuMemory,
        gpusize           offset,
        uint32            value) override;

    virtual void CmdMemoryAtomic(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        uint64            srcData,
        AtomicOp          atomicOp) override;

    virtual void CmdWriteTimestamp(HwPipePoint pipePoint, const IGpuMemory& dstGpuMemory, gpusize dstOffset) override;

    virtual void CmdWriteImmediate(
        HwPipePoint        pipePoint,
        uint64             data,
        ImmediateDataWidth dataSize,
        gpusize            address) override;

    virtual void CmdBindBorderColorPalette(
        PipelineBindPoint          pipelineBindPoint,
        const IBorderColorPalette* pPalette) override;

    virtual void CmdInsertTraceMarker(PerfTraceMarkerType markerType, uint32 markerData) override;
    virtual void CmdInsertRgpTraceMarker(uint32 numDwords, const void* pData) override;

    virtual void AddQuery(QueryPoolType queryPoolType, QueryControlFlags flags) override;
    virtual void RemoveQuery(QueryPoolType queryPoolType) override;

    virtual void CmdLoadGds(
        HwPipePoint       pipePoint,
        uint32            dstGdsOffset,
        const IGpuMemory& srcGpuMemory,
        gpusize           srcMemOffset,
        uint32            size) override;

    virtual void CmdStoreGds(
        HwPipePoint       pipePoint,
        uint32            srcGdsOffset,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstMemOffset,
        uint32            size,
        bool              waitForWC) override;

    virtual void CmdUpdateGds(
        HwPipePoint       pipePoint,
        uint32            gdsOffset,
        uint32            dataSize,
        const uint32*     pData) override;

    virtual void CmdFillGds(
        HwPipePoint       pipePoint,
        uint32            gdsOffset,
        uint32            fillSize,
        uint32            data) override;

    virtual void CmdLoadBufferFilledSizes(
        const gpusize (&gpuVirtAddr)[MaxStreamOutTargets]) override;

    virtual void CmdSaveBufferFilledSizes(
        const gpusize (&gpuVirtAddr)[MaxStreamOutTargets]) override;

    virtual void CmdSetBufferFilledSize(
        uint32  bufferId,
        uint32  offset) override;

    virtual void CmdBeginQuery(
        const IQueryPool& queryPool,
        QueryType         queryType,
        uint32            slot,
        QueryControlFlags flags) override;

    virtual void CmdEndQuery(const IQueryPool& queryPool, QueryType queryType, uint32 slot) override;

    virtual void CmdResolveQuery(
        const IQueryPool& queryPool,
        QueryResultFlags  flags,
        QueryType         queryType,
        uint32            startQuery,
        uint32            queryCount,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        gpusize           dstStride) override;

    virtual void CmdResetQueryPool(
        const IQueryPool& queryPool,
        uint32            startQuery,
        uint32            queryCount) override;

    virtual CmdStream* GetCmdStreamByEngine(uint32 engineType) override;

    virtual void CmdUpdateSqttTokenMask(const ThreadTraceTokenConfig& sqttTokenConfig) override;

    virtual void CmdLoadCeRam(
        const IGpuMemory& srcGpuMemory,
        gpusize           memOffset,
        uint32            ramOffset,
        uint32            dwordSize) override;

    virtual void CmdDumpCeRam(
        const IGpuMemory& dstGpuMemory,
        gpusize           memOffset,
        uint32            ramOffset,
        uint32            dwordSize,
        uint32            currRingPos,
        uint32            ringSize) override;

    virtual void CmdWriteCeRam(
        const void* pSrcData,
        uint32      ramOffset,
        uint32      dwordSize) override;

    virtual void CmdIf(
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint64            data,
        uint64            mask,
        CompareFunc       compareFunc) override;

    virtual void CmdElse() override;

    virtual void CmdEndIf() override;

    virtual void CmdWhile(
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint64            data,
        uint64            mask,
        CompareFunc       compareFunc) override;

    virtual void CmdEndWhile() override;

    virtual void CmdWaitRegisterValue(
        uint32      registerOffset,
        uint32      data,
        uint32      mask,
        CompareFunc compareFunc) override;

    virtual void CmdWaitMemoryValue(
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            data,
        uint32            mask,
        CompareFunc       compareFunc) override;

    virtual void CmdWaitBusAddressableMemoryMarker(
        const IGpuMemory& gpuMemory,
        uint32            data,
        uint32            mask,
        CompareFunc       compareFunc) override;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 509
    virtual void CmdSetHiSCompareState0(
        CompareFunc compFunc,
        uint32      compMask,
        uint32      compValue,
        bool        enable) override;

    virtual void CmdSetHiSCompareState1(
        CompareFunc compFunc,
        uint32      compMask,
        uint32      compValue,
        bool        enable) override;
#endif

    virtual void CmdUpdateHiSPretests(
        const IImage*      pImage,
        const HiSPretests& pretests,
        uint32             firstMip,
        uint32             numMips) override;

    virtual void CmdSetPredication(
        IQueryPool*         pQueryPool,
        uint32              slot,
        const IGpuMemory*   pGpuMemory,
        gpusize             offset,
        PredicateType       predType,
        bool                predPolarity,
        bool                waitResults,
        bool                accumulateData) override;

    virtual void CmdExecuteNestedCmdBuffers(
        uint32            cmdBufferCount,
        ICmdBuffer*const* ppCmdBuffers) override;

    virtual void CmdCommentString(const char* pComment) override;
    virtual void CmdNop(
        const void* pPayload,
        uint32      payloadSize) override;

    virtual uint32 CmdInsertExecutionMarker() override;

    virtual void CmdExecuteIndirectCmds(
        const IIndirectCmdGenerator& generator,
        const IGpuMemory&            gpuMemory,
        gpusize                      offset,
        uint32                       maximumCount,
        gpusize                      countGpuAddr) override;

    virtual CmdStreamChunk* GetChunkForCmdGeneration(
        const Pal::IndirectCmdGenerator& generator,
        const Pal::Pipeline&             pipeline,
        uint32                           maxCommands,
        uint32*                          pCommandsInChunk,
        gpusize*                         pEmbeddedDataAddr,
        uint32*                          pEmbeddedDataSize) override;

    Util::IntervalTree<gpusize, bool, Platform>* ActiveOcclusionQueryWriteRanges()
        { return &m_activeOcclusionQueryWriteRanges; }

    void CmdSetTriangleRasterStateInternal(
        const TriangleRasterStateParams& params,
        bool                             optimizeLinearDestGfxCopy);

    virtual void AddPerPresentCommands(
        gpusize frameCountGpuAddr,
        uint32  frameCntReg) override;

    virtual void CmdOverwriteRbPlusFormatForBlits(
        SwizzledFormat format,
        uint32         targetIndex) override;

    void SetPrimShaderWorkload() { m_nggState.flags.state.hasPrimShaderWorkload = 1; }
    bool HasPrimShaderWorkload() const { return m_nggState.flags.state.hasPrimShaderWorkload; }

    uint32 BuildScissorRectImage(
        bool               multipleViewports,
        ScissorRectPm4Img* pScissorRectImg) const;

    template <bool pm4OptImmediate>
    uint32* ValidateScissorRects(uint32* pDeCmdSpace);
    uint32* ValidateScissorRects(uint32* pDeCmdSpace);

    bool NeedsToValidateScissorRects(const bool pm4OptImmediate) const;
    bool NeedsToValidateScissorRects() const;

    virtual void CpCopyMemory(gpusize dstAddr, gpusize srcAddr, gpusize numBytes) override;

    bool IsRasterizationKilled() const { return (m_pipelineFlags.noRaster != 0); }

protected:
    virtual ~UniversalCmdBuffer() {}

    virtual Result AddPreamble() override;
    virtual Result AddPostamble() override;

    virtual void BeginExecutionMarker(uint64 clientHandle) override;
    virtual void EndExecutionMarker() override;

    virtual void ResetState() override;

    virtual void WriteEventCmd(const BoundGpuMemory& boundMemObj, HwPipePoint pipePoint, uint32 data) override;

    virtual void CmdXdmaWaitFlipPending() override;

    virtual void SetGraphicsState(const GraphicsState& newGraphicsState) override;

    virtual void InheritStateFromCmdBuf(const GfxCmdBuffer* pCmdBuffer) override;

    template <bool pm4OptImmediate>
    uint32* ValidateBinSizes(
        const GraphicsPipeline&  pipeline,
        const ColorBlendState*   pColorBlendState,
        bool                     disableDfsm,
        uint32*                  pDeCmdSpace);

    bool ShouldEnablePbb(
        const GraphicsPipeline&  pipeline,
        const ColorBlendState*   pColorBlendState,
        const DepthStencilState* pDepthStencilState,
        const MsaaState*         pMsaaState) const;

    template <bool Indexed, bool Indirect>
    void ValidateDraw(const ValidateDrawInfo& drawInfo);

    template <bool Indexed, bool Indirect, bool Pm4OptImmediate>
    void ValidateDraw(const ValidateDrawInfo& drawInfopDeCmdSpace);

    template <bool Indexed, bool Indirect, bool Pm4OptImmediate, bool PipelineDirty>
    uint32* ValidateDraw(
        const ValidateDrawInfo& drawInfo,
        uint32*                 pDeCmdSpace);

    template <bool Indexed, bool Indirect, bool Pm4OptImmediate, bool PipelineDirty, bool StateDirty>
    uint32* ValidateDraw(
        const ValidateDrawInfo& drawInfo,
        uint32*                 pDeCmdSpace);

    template <bool Indexed, bool Indirect, bool Pm4OptImmediate, bool PipelineDirty, bool StateDirty, bool IsNgg>
    uint32* ValidateDraw(
        const ValidateDrawInfo& drawInfo,
        uint32*                 pDeCmdSpace);

    template <bool Indexed,
              bool Indirect,
              bool Pm4OptImmediate,
              bool PipelineDirty,
              bool StateDirty,
              bool IsNgg,
              bool IsNggFastLaunch>
    uint32* ValidateDraw(
        const ValidateDrawInfo& drawInfo,
        uint32*                 pDeCmdSpace);

    template <bool Indexed, bool Indirect, bool IsNggFastLaunch, bool Pm4OptImmediate>
    uint32* ValidateDrawTimeHwState(
        regPA_SC_MODE_CNTL_1          paScModeCntl1,
        regDB_COUNT_CONTROL           dbCountControl,
        regVGT_MULTI_PRIM_IB_RESET_EN vgtMultiPrimIbResetEn,
        const ValidateDrawInfo&       drawInfo,
        uint32*                       pDeCmdSpace);

    template <bool indexed, bool indirect, bool pm4OptImmediate>
    uint32* ValidateDrawTimeNggFastLaunchState(
        const ValidateDrawInfo& drawInfo,
        uint32*                 pDeCmdSpace);

    // Gets vertex offset register address
    uint16 GetVertexOffsetRegAddr() const { return m_vertexOffsetReg; }

    // Gets instance offset register address. It always immediately follows the vertex offset register.
    uint16 GetInstanceOffsetRegAddr() const { return m_vertexOffsetReg + 1; }

    // Gets the start index offset register address
    uint16 GetStartIndexRegAddr() const { return m_nggState.startIndexReg; }

    virtual void P2pBltWaCopyBegin(
        const GpuMemory* pDstMemory,
        uint32           regionCount,
        const gpusize*   pChunkAddrs) override;
    virtual void P2pBltWaCopyNextRegion(gpusize chunkAddr) override;
    virtual void P2pBltWaCopyEnd() override;

private:
    template <bool IssueSqttMarkerEvent,
              bool HasUavExport,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDraw(
        ICmdBuffer* pCmdBuffer,
        uint32      firstVertex,
        uint32      vertexCount,
        uint32      firstInstance,
        uint32      instanceCount);

    template <bool IssueSqttMarkerEvent,
              bool HasUavExport,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDrawOpaque(
        ICmdBuffer* pCmdBuffer,
        gpusize streamOutFilledSizeVa,
        uint32  streamOutOffset,
        uint32  stride,
        uint32  firstInstance,
        uint32  instanceCount);

    template <bool IssueSqttMarkerEvent,
              bool IsNggFastLaunch,
              bool HasUavExport,
              bool ViewInstancingEnable,
              bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDrawIndexed(
        ICmdBuffer* pCmdBuffer,
        uint32      firstIndex,
        uint32      indexCount,
        int32       vertexOffset,
        uint32      firstInstance,
        uint32      instanceCount);

    template <bool IssueSqttMarkerEvent, bool ViewInstancingEnable, bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDrawIndirectMulti(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);

    template <bool IssueSqttMarkerEvent, bool IsNggFastLaunch, bool ViewInstancingEnable, bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDrawIndexedIndirectMulti(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);

    template <bool IssueSqttMarkerEvent, bool UseCpuPathForUserDataTables, bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatch(
        ICmdBuffer* pCmdBuffer,
        uint32      x,
        uint32      y,
        uint32      z);
    template <bool IssueSqttMarkerEvent, bool UseCpuPathForUserDataTables, bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatchIndirect(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset);
    template <bool IssueSqttMarkerEvent, bool UseCpuPathForUserDataTables, bool DescribeDrawDispatch>
    static void PAL_STDCALL CmdDispatchOffset(
        ICmdBuffer* pCmdBuffer,
        uint32      xOffset,
        uint32      yOffset,
        uint32      zOffset,
        uint32      xDim,
        uint32      yDim,
        uint32      zDim);
    template <bool isNgg>
    uint32 CalcGeCntl(
        bool                  usesLineStipple,
        regIA_MULTI_VGT_PARAM iaMultiVgtParam) const;

    uint32* Gfx10ValidateTriangleRasterState(
        const GraphicsPipeline*  pPipeline,
        uint32*                  pDeCmdSpace);

    virtual void DeactivateQueryType(QueryPoolType queryPoolType) override;
    virtual void ActivateQueryType(QueryPoolType queryPoolType) override;

    template <bool pm4OptImmediate>
    uint32* UpdateDbCountControl(uint32               log2SampleRate,
                                 regDB_COUNT_CONTROL* pDbCountControl,
                                 uint32*              pDeCmdSpace);

    bool ForceWdSwitchOnEop(const GraphicsPipeline& pipeline, const ValidateDrawInfo& drawInfo) const;

    template <bool pm4OptImmediate>
    uint32* ValidateViewports(uint32* pDeCmdSpace);
    uint32* ValidateViewports(uint32* pDeCmdSpace);

    uint32* WriteNullColorTargets(
        uint32* pCmdSpace,
        uint32  newColorTargetMask,
        uint32  oldColorTargetMask);
    uint32* WriteNullDepthTarget(uint32* pCmdSpace);

    uint32* FlushStreamOut(uint32* pDeCmdSpace);

    bool HasStreamOutBeenSet() const;

    uint32* WaitOnCeCounter(uint32* pDeCmdSpace);
    uint32* IncrementDeCounter(uint32* pDeCmdSpace);

    Pm4Predicate PacketPredicate() const { return static_cast<Pm4Predicate>(m_gfxCmdBufState.flags.packetPredicate); }

    template <bool IssueSqttMarkerEvent, bool DescribeDrawDispatch>
    void SetDispatchFunctions();

    template <bool TessEnabled, bool GsEnabled, bool VsEnabled>
    void SetUserDataValidationFunctions();
    void SetUserDataValidationFunctions(bool tessEnabled, bool gsEnabled, bool isNgg);

    template <bool UseCpuPathForUserDataTables>
    uint32* ValidateDispatch(
        gpusize indirectGpuVirtAddr,
        uint32  xDim,
        uint32  yDim,
        uint32  zDim,
        uint32* pDeCmdSpace);

    uint32* SwitchGraphicsPipeline(
        const GraphicsPipelineSignature* pPrevSignature,
        const GraphicsPipeline*          pCurrPipeline,
        uint32*                          pDeCmdSpace);

    template <uint32 AlignmentInDwords>
    void RelocateUserDataTable(
        UserDataTableState* pTable,
        uint32              offsetInDwords,
        uint32              dwordsNeeded);
    uint32* UploadToUserDataTable(
        UserDataTableState* pTable,
        uint32              offsetInDwords,
        uint32              dwordsNeeded,
        const uint32*       pSrcData,
        uint32              highWatermark,
        uint32*             pCeCmdSpace);
    uint32* DumpUserDataTable(
        UserDataTableState* pTable,
        uint32              offsetInDwords,
        uint32              dwordsNeeded,
        uint32*             pCeCmdSpace);

    template <bool HasPipelineChanged, bool TessEnabled, bool GsEnabled, bool VsEnabled>
    uint32* ValidateGraphicsUserDataCeRam(
        const GraphicsPipelineSignature* pPrevSignature,
        uint32*                          pDeCmdSpace);
    template <bool HasPipelineChanged, bool TessEnabled, bool GsEnabled, bool VsEnabled>
    uint32* ValidateGraphicsUserDataCpu(
        const GraphicsPipelineSignature* pPrevSignature,
        uint32*                          pDeCmdSpace);

    template <bool HasPipelineChanged>
    uint32* ValidateComputeUserDataCeRam(
        const ComputePipelineSignature* pPrevSignature,
        uint32*                         pDeCmdSpace);
    template <bool HasPipelineChanged>
    uint32* ValidateComputeUserDataCpu(
        const ComputePipelineSignature* pPrevSignature,
        uint32*                         pDeCmdSpace);

    template <bool TessEnabled, bool GsEnabled, bool VsEnabled>
    uint32* WriteDirtyUserDataEntriesToSgprsGfx(
        const GraphicsPipelineSignature* pPrevSignature,
        uint8                            alreadyWrittenStageMask,
        uint32*                          pDeCmdSpace);

    uint32* WriteDirtyUserDataEntriesToUserSgprsCs(
        uint32* pDeCmdSpace);

    template <typename PipelineSignature>
    uint32* WriteDirtyUserDataEntriesToCeRam(
        const PipelineSignature* pPrevSignature,
        const PipelineSignature* pCurrSignature,
        uint32*                  pCeCmdSpace);

    template <bool TessEnabled, bool GsEnabled, bool VsEnabled>
    uint8 FixupUserSgprsOnPipelineSwitch(
        const GraphicsPipelineSignature* pPrevSignature,
        uint32**                         ppDeCmdSpace);

    template <typename PipelineSignature>
    void FixupSpillTableOnPipelineSwitch(
        const PipelineSignature* pPrevSignature,
        const PipelineSignature* pCurrSignature);

    void LeakNestedCmdBufferState(
        const UniversalCmdBuffer& cmdBuffer);

    uint8 CheckStreamOutBufferStridesOnPipelineSwitch();
    uint32* UploadStreamOutBufferStridesToCeRam(
        uint8   dirtyStrideMask,
        uint32* pCeCmdSpace);

    void Gfx9GetColorBinSize(Extent2d* pBinSize) const;
    void Gfx9GetDepthBinSize(Extent2d* pBinSize) const;
    void Gfx10GetColorBinSize(Extent2d* pBinSize) const;
    void Gfx10GetDepthBinSize(Extent2d* pBinSize) const;
    void SetPaScBinnerCntl0(const GraphicsPipeline&  pipeline,
                            const ColorBlendState*   pColorBlendState,
                            Extent2d*                pBinSize,
                            bool                     disableDfsm);

    void SendFlglSyncCommands(FlglRegSeqType type);

    void DescribeDraw(Developer::DrawDispatchType cmdType);

    void P2pBltWaSync();

    uint32* UpdateNggCullingDataBufferWithCpu(
        uint32* pDeCmdSpace);

    uint32* UpdateNggCullingDataBufferWithGpu(
        uint32* pDeCmdSpace);

    uint32* BuildWriteViewId(
        uint32  viewId,
        uint32* pCmdSpace);

    void UpdateUavExportTable();

    void SwitchDrawFunctions(
        bool hasUavExport,
        bool viewInstancingEnable,
        bool nggFastLaunch);

    template <bool IssueSqtt,
              bool DescribeDrawDispatch>
    void SwitchDrawFunctionsInternal(
        bool hasUavExport,
        bool viewInstancingEnable,
        bool nggFastLaunch);

    template <bool NggFastLaunch,
              bool IssueSqtt,
              bool DescribeDrawDispatch>
    void SwitchDrawFunctionsInternal(
        bool hasUavExport,
        bool viewInstancingEnable);

    template <bool ViewInstancing,
              bool NggFastLaunch,
              bool IssueSqtt,
              bool DescribeDrawDispatch>
    void SwitchDrawFunctionsInternal(
        bool hasUavExport );

    template <bool ViewInstancing,
              bool NggFastLaunch,
              bool HasUavExport,
              bool IssueSqtt,
              bool DescribeDrawDispatch>
    void SwitchDrawFunctionsInternal();

    BinningMode GetDisableBinningSetting(Extent2d* pBinSize) const;

    const Device&   m_device;
    const CmdUtil&  m_cmdUtil;
    CmdStream       m_deCmdStream;
    CmdStream       m_ceCmdStream;

    // Tracks the user-data signature of the currently active compute & graphics pipelines.
    const ComputePipelineSignature*   m_pSignatureCs;
    const GraphicsPipelineSignature*  m_pSignatureGfx;

    uint64      m_pipelineCtxPm4Hash;   // Hash of current pipeline's PM4 image for context registers.
    ShaderHash  m_pipelinePsHash;       // Hash of current pipeline's pixel shader program.
    union
    {
        struct
        {
            uint32  usesTess :  1;
            uint32  usesGs   :  1;
            uint32  isNgg    :  1;
            uint32  noRaster :  1;
            uint32  reserved : 28;
        };
        uint32 u32All;
    }  m_pipelineFlags;  // Flags describing the currently active pipeline stages.

    // Function pointers which validate all graphics user-data at Draw-time for the cases where the pipeline is
    // changing and cases where it is not.
    ValidateUserDataGfxFunc  m_pfnValidateUserDataGfx;
    ValidateUserDataGfxFunc  m_pfnValidateUserDataGfxPipelineSwitch;

    struct
    {
        // Per-pipeline watermark of the size of the vertex buffer table needed per draw (in DWORDs).
        uint32      watermark : 31;
        // Tracks whether or not the vertex buffer table was modified somewhere in the command buffer.
        uint32      modified  :  1;
        BufferSrd*  pSrds;  // Tracks the contents of the vertex buffer table.

        UserDataTableState  state;  // Tracks the state for the indirect user-data table

    }  m_vbTable;

    struct
    {
        UserDataTableState  state; // Tracks the state of the NGG state table
    }  m_nggTable;

    struct
    {
        UserDataTableState  stateCs;  // Tracks the state of the compute spill table
        UserDataTableState  stateGfx; // Tracks the state of the graphics spill table
    }  m_spillTable;

    struct
    {
        UserDataTableState  state;  // Tracks the state of the stream-out SRD table

        BufferSrd  srd[MaxStreamOutTargets];    // Current stream-out target SRD's
    }  m_streamOut;

    struct
    {
        UserDataTableState  state;         // Tracks the state of the SRD table
        ImageSrd            srd[MaxColorTargets];
        uint32              tableSizeDwords; // Size of the srd table in dwords, omitting unbound targets at the end
        uint32              maxColorTargets; // Maximum color targets bound by the shader
    }  m_uavExportTable;

    WorkaroundState          m_workaroundState;
    UniversalCmdBufferState  m_state; // State tracking for internal cmd buffer operations

    regVGT_DMA_INDEX_TYPE                    m_vgtDmaIndexType;   // Register setting for VGT_DMA_INDEX_TYPE
    regSPI_VS_OUT_CONFIG                     m_spiVsOutConfig;    // Register setting for VS_OUT_CONFIG
    regSPI_PS_IN_CONTROL                     m_spiPsInControl;    // Register setting for PS_IN_CONTROL
    regPA_SC_CONSERVATIVE_RASTERIZATION_CNTL m_paScConsRastCntl;  // Register setting for PA_SC_CONSERV_RAST_CNTL
    uint16                                   m_vertexOffsetReg;   // Register where the vertex start offset is written
    uint16                                   m_drawIndexReg;      // Register where the draw index is written
    RbPlusPm4Img                             m_rbPlusPm4Img;      // PM4 image for RB Plus register state

    const uint32  m_log2NumSes;
    const uint32  m_log2NumRbPerSe;

    uint32  m_depthBinSizeTagPart;    // Constant used in Depth PBB bin size formulas
    uint32  m_colorBinSizeTagPart;    // Constant used in Color PBB bin size formulas
    uint32  m_fmaskBinSizeTagPart;    // Constant used in Fmask PBB bin size formulas
    uint16  m_minBinSizeX;            // Minimum bin size(width) for PBB.
    uint16  m_minBinSizeY;            // Minimum bin size(height) for PBB.

    regCB_RMI_GL2_CACHE_CONTROL m_cbRmiGl2CacheControl; // Control CB cache policy and big page

    regPA_SC_BINNER_CNTL_0  m_paScBinnerCntl0;
    regPA_SC_BINNER_CNTL_0  m_savedPaScBinnerCntl0; // Value of PA_SC_BINNER_CNTL0 selected by settings
    uint32                  m_log2NumSamples;       // Last written value of PA_SC_AA_CONFIG.MSAA_NUM_SAMPLES.
    regDB_DFSM_CONTROL      m_dbDfsmControl;

    BinningMode      m_binningMode;      // Last value programmed into paScBinnerCntl0.BINNING_MODE
    BinningOverride  m_pbbStateOverride; // Sets PBB on/off as per dictated by the new bound pipeline.
    bool             m_enabledPbb;       // PBB is currently enabled or disabled.
    uint16           m_customBinSizeX;   // Custom bin sizes for PBB.  Zero indicates PBB is not using
    uint16           m_customBinSizeY;   // a custom bin size.
    Extent2d         m_currentBinSize;   // Current PBB bin size that has been chosen. This could be
                                         // equal to the custom bin size.

    union
    {
        struct
        {
            uint32 tossPointMode              :  3; // The currently enabled "TossPointMode" global setting
            uint32 hiDepthDisabled            :  1; // True if Hi-Depth is disabled by settings
            uint32 hiStencilDisabled          :  1; // True if Hi-Stencil is disabled by settings
            uint32 disableDfsm                :  1; // A copy of the disableDfsm setting.
            uint32 disableDfsmPsUav           :  1; // A copy of the disableDfsmPsUav setting.
            uint32 disableBatchBinning        :  1; // True if binningMode is disabled.
            uint32 disablePbbPsKill           :  1; // True if PBB should be disabled for pipelines using PS Kill
            uint32 disablePbbNoDb             :  1; // True if PBB should be disabled for pipelines with no DB
            uint32 disablePbbBlendingOff      :  1; // True if PBB should be disabled for pipelines with no blending
            uint32 disablePbbAppendConsume    :  1; // True if PBB should be disabled for pipelines with append/consume
            uint32 disableWdLoadBalancing     :  1; // True if wdLoadBalancingMode is disabled.
            uint32 ignoreCsBorderColorPalette :  1; // True if compute border-color palettes should be ignored
            uint32 blendOptimizationsEnable   :  1; // A copy of the blendOptimizationsEnable setting.
            uint32 outOfOrderPrimsEnable      :  2; // The out-of-order primitive rendering mode allowed by settings
            uint32 checkDfsmEqaaWa            :  1; // True if settings are such that the DFSM + EQAA workaround is on.
            uint32 scissorChangeWa            :  1; // True if the scissor register workaround is enabled
            uint32 issueSqttMarkerEvent       :  1; // True if settings are such that we need to issue SQ thread trace
                                                    // marker events on draw.
            uint32 enablePm4Instrumentation   :  1; // True if settings are such that we should enable detailed PM4
                                                    // instrumentation.
            uint32 batchBreakOnNewPs          :  1; // True if a BREAK_BATCH should be inserted when switching pixel
                                                    // shaders.
            uint32 padParamCacheSpace         :  1; // True if this command buffer should pad used param-cache space to
                                                    // reduce context rolls.
            uint32 describeDrawDispatch       :  1; // True if draws/dispatch shader IDs should be specified within the
                                                    // command stream for parsing by PktTools
            uint32 disableVertGrouping        :  1; // Disable VertexGrouping.
            uint32 prefetchIndexBufferForNgg  :  1; // Prefetch index buffers to workaround misses in UTCL2 with NGG
            uint32 waCeDisableIb2             :  1; // Disable IB2's on the constant engine to workaround HW bug
            uint32 reserved2                  :  1;
            uint32 reserved3                  :  1;
            uint32 pbbMoreThanOneCtxState     :  1;
            uint32 reserved                   :  2;
        };
        uint32 u32All;
    } m_cachedSettings;

    DrawTimeHwState  m_drawTimeHwState;  // Tracks certain bits of HW-state that might need to be updated per draw.
    NggState         m_nggState;

    // In order to prevent invalid query results if an app does Begin()/End(), Reset()/Begin()/End(), Resolve() on a
    // query slot in a command buffer (the first End() might overwrite values written by the Reset()), we have to
    // insert an idle before performing the Reset().  This has a high performance penalty.  This structure is used
    // to track memory ranges affected by outstanding End() calls in this command buffer so we can avoid the idle
    // during Reset() if the reset doesn't affect any pending queries.
    Util::IntervalTree<gpusize, bool, Platform>  m_activeOcclusionQueryWriteRanges;

    PAL_DISALLOW_DEFAULT_CTOR(UniversalCmdBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(UniversalCmdBuffer);
};

// Helper function for managing the logic controlling when to do CE/DE synchronization and invalidating the Kcache.
extern bool HandleCeRinging(
    UniversalCmdBufferState* pState,
    uint32                   currRingPos,
    uint32                   ringInstances,
    uint32                   ringSize);

} // Gfx9
} // Pal
