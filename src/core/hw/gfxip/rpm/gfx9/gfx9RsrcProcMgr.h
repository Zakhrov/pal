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

#include "core/hw/gfxip/rpm/rsrcProcMgr.h"

namespace Pal
{

class  GfxCmdBuffer;
class  QueryPool;
struct DepthStencilViewCreateInfo;

namespace Gfx9
{

class      CmdUtil;
class      Device;
class      Gfx9Htile;
class      Gfx9MaskRam;
class      Image;
struct     SyncReqs;
enum class DccClearPurpose : uint32;

// =====================================================================================================================
// GFX9+10 common hardware layer implementation of the Resource Processing Manager. It is most known for handling
// GFX9+10-specific resource operations like DCC decompression.
class RsrcProcMgr : public Pal::RsrcProcMgr
{
public:

    static constexpr bool ForceGraphicsFillMemoryPath = false;

    void CmdCopyMemory(
        GfxCmdBuffer*           pCmdBuffer,
        const GpuMemory&        srcGpuMemory,
        const GpuMemory&        dstGpuMemory,
        uint32                  regionCount,
        const MemoryCopyRegion* pRegions) const;

    void CmdCloneImageData(
        GfxCmdBuffer* pCmdBuffer,
        const Image&  srcImage,
        const Image&  dstImage) const;

    void CmdUpdateMemory(
        GfxCmdBuffer*    pCmdBuffer,
        const GpuMemory& dstMem,
        gpusize          dstOffset,
        gpusize          dataSize,
        const uint32*    pData) const;

    void CmdResolveQuery(
        GfxCmdBuffer*         pCmdBuffer,
        const Pal::QueryPool& queryPool,
        QueryResultFlags      flags,
        QueryType             queryType,
        uint32                startQuery,
        uint32                queryCount,
        const GpuMemory&      dstGpuMemory,
        gpusize               dstOffset,
        gpusize               dstStride) const;

    void DccDecompress(
        GfxCmdBuffer*                pCmdBuffer,
        Pal::CmdStream*              pCmdStream,
        const Image&                 image,
        const IMsaaState*            pMsaaState,
        const MsaaQuadSamplePattern* pQuadSamplePattern,
        const SubresRange&           range) const;

    void FmaskColorExpand(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       image,
        const SubresRange& range) const;

    void FmaskDecompress(
        GfxCmdBuffer*                pCmdBuffer,
        Pal::CmdStream*              pCmdStream,
        const Image&                 image,
        const IMsaaState*            pMsaaState,
        const MsaaQuadSamplePattern* pQuadSamplePattern,
        const SubresRange&           range) const;

    bool InitMaskRam(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& range) const;

    void BuildHtileLookupTable(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range) const;

    virtual bool FastClearEliminate(
        GfxCmdBuffer*                pCmdBuffer,
        Pal::CmdStream*              pCmdStream,
        const Image&                 image,
        const IMsaaState*            pMsaaState,
        const MsaaQuadSamplePattern* pQuadSamplePattern,
        const SubresRange&           range) const;

    virtual void ExpandDepthStencil(
        GfxCmdBuffer*                pCmdBuffer,
        const Pal::Image&            image,
        const IMsaaState*            pMsaaState,
        const MsaaQuadSamplePattern* pQuadSamplePattern,
        const SubresRange&           range) const override;

    virtual void CmdCopyMemoryToImage(
        GfxCmdBuffer*                pCmdBuffer,
        const GpuMemory&             srcGpuMemory,
        const Pal::Image&            dstImage,
        ImageLayout                  dstImageLayout,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions,
        bool                         includePadding) const override;

    virtual void CmdCopyImageToMemory(
        GfxCmdBuffer*                pCmdBuffer,
        const Pal::Image&            srcImage,
        ImageLayout                  srcImageLayout,
        const GpuMemory&             dstGpuMemory,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions,
        bool                         includePadding) const override;

    bool WillDecompressWithCompute(
        const GfxCmdBuffer* pCmdBuffer,
        const Image&        gfxImage,
        const SubresRange&  range) const;

protected:
    explicit RsrcProcMgr(Device* pDevice);
    virtual ~RsrcProcMgr() {}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 478
    virtual void HwlCreateDecompressResolveSafeImageViewSrds(
        uint32                numSrds,
        const ImageViewInfo*  pImageView,
        void*                 pSrdTable) const override;
#endif

    virtual const Pal::GraphicsPipeline* GetGfxPipelineByTargetIndexAndFormat(
        RpmGfxPipeline basePipeline,
        uint32         targetIndex,
        SwizzledFormat format) const override;

    virtual bool CopyImageUseMipLevelInSrd(bool isCompressed) const override
        { return (RsrcProcMgr::UseMipLevelInSrd && (isCompressed == false)); }

    bool ClearDcc(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange,
        uint8              clearCode,
        DccClearPurpose    clearPurpose,
        const uint32*      pPackedClearColor = nullptr) const;

    virtual void ClearDccCompute(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange,
        uint8              clearCode,
        DccClearPurpose    clearPurpose,
        const uint32*      pPackedClearColor = nullptr) const = 0;

    ImageAspect DecodeImageViewSrdAspect(
        const Pal::Image&  image,
        gpusize            srdBaseAddr) const;

    static uint32 ExpandClearCodeToDword(uint8  clearCode);

    virtual void FastDepthStencilClearCompute(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        uint32             htileValue,
        uint32             clearMask,
        uint8              stencil) const = 0;

    void FastDepthStencilClearComputeCommon(
        GfxCmdBuffer*      pCmdBuffer,
        const Pal::Image*  pPalImage,
        uint32             clearMask) const;

    uint32 GetClearDepth(
        const Image&       dstImage,
        const SubresRange& clearRange,
        uint32             mipLevel) const;

    virtual bool HwlUseOptimizedImageCopy(
        const Pal::Image&  srcImage,
        const Pal::Image&  dstImage) const override;

    virtual void HwlUpdateDstImageFmaskMetaData(
        GfxCmdBuffer*          pCmdBuffer,
        const Pal::Image&      srcImage,
        const Pal::Image&      dstImage,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32                 flags) const override;

    virtual bool HwlImageUsesCompressedWrites(
        const uint32* pImageSrd) const override { return false; }

    virtual void HwlUpdateDstImageStateMetaData(
        GfxCmdBuffer*          pCmdBuffer,
        const Pal::Image&      dstImage,
        const SubresRange&     range) const override {}

    virtual void HwlFixupResolveDstImage(
        GfxCmdBuffer*             pCmdBuffer,
        const GfxImage&           dstImage,
        ImageLayout               dstImageLayout,
        const ImageResolveRegion* pRegions,
        uint32                    regionCount,
        bool                      computeResolve
        ) const override;

    static void MetaDataDispatch(
        GfxCmdBuffer*       pCmdBuffer,
        const Gfx9MaskRam*  pMaskRam,
        uint32              width,
        uint32              height,
        uint32              depth,
        const uint32*       pThreadsPerGroup);

    void CommitBeginEndGfxCopy(
        Pal::CmdStream*  pCmdStream,
        uint32           paScTileSteeringOverride) const;

    Device*const   m_pDevice;
    const CmdUtil& m_cmdUtil;

private:
    void CmdCopyMemoryFromToImageViaPixels(
        GfxCmdBuffer*                 pCmdBuffer,
        const Pal::Image&             image,
        const GpuMemory&              memory,
        const MemoryImageCopyRegion&  region,
        bool                          includePadding,
        bool                          imageIsSrc) const;

    static Extent3d GetCopyViaSrdCopyDims(
        const Pal::Image&  image,
        const SubresId&    subResId,
        bool               includePadding);

    static bool UsePixelCopy(
        const Pal::Image&             image,
        const MemoryImageCopyRegion&  region);

    virtual void HwlFastColorClear(
        GfxCmdBuffer*      pCmdBuffer,
        const GfxImage&    dstImage,
        const uint32*      pConvertedColor,
        const SubresRange& clearRange) const override;

    virtual void HwlDepthStencilClear(
        GfxCmdBuffer*      pCmdBuffer,
        const GfxImage&    dstImage,
        ImageLayout        depthLayout,
        ImageLayout        stencilLayout,
        float              depth,
        uint8              stencil,
        uint32             rangeCount,
        const SubresRange* pRanges,
        bool               fastClear,
        bool               needComputeSync,
        uint32             boxCnt,
        const Box*         pBox) const override;

    virtual bool HwlCanDoFixedFuncResolve(
        const Pal::Image&         srcImage,
        const Pal::Image&         dstImage,
        ResolveMode               resolveMode,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions) const override;

    virtual bool HwlCanDoDepthStencilCopyResolve(
        const Pal::Image&         srcImage,
        const Pal::Image&         dstImage,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions) const override;

    void ClearFmask(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& clearRange,
        uint64             clearValue) const;

    virtual void InitCmask(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       image,
        const SubresRange& range) const = 0;

    virtual void InitHtile(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange) const = 0;

    void ClearHiSPretestsMetaData(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& range) const;

    void InitDepthClearMetaData(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& range) const;

    void InitColorClearMetaData(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& range) const;

    void DepthStencilClearGraphics(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        float              depth,
        uint8              stencil,
        uint32             clearMask,
        bool               fastClear,
        ImageLayout        depthLayout,
        ImageLayout        stencilLayout,
        uint32             boxCnt,
        const Box*         pBox) const;

    uint32* UpdateBoundFastClearColor(
        GfxCmdBuffer*   pCmdBuffer,
        const GfxImage& dstImage,
        uint32          startMip,
        uint32          numMips,
        const uint32    color[4],
        CmdStream*      pStream,
        uint32*         pCmdSpace) const;

    void UpdateBoundFastClearDepthStencil(
        GfxCmdBuffer*      pCmdBuffer,
        const GfxImage&    dstImage,
        const SubresRange& range,
        uint32             metaDataClearFlags,
        float              depth,
        uint8              stencil) const;

    void DccDecompressOnCompute(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       image,
        const SubresRange& range) const;

    void CmdResolveQueryComputeShader(
        GfxCmdBuffer*         pCmdBuffer,
        const Pal::QueryPool& queryPool,
        QueryResultFlags      flags,
        QueryType             queryType,
        uint32                startQuery,
        uint32                queryCount,
        const GpuMemory&      dstGpuMemory,
        gpusize               dstOffset,
        gpusize               dstStride) const;

    const SPI_SHADER_EX_FORMAT DeterminePsExportFmt(
        SwizzledFormat format,
        bool           blendEnabled,
        bool           shaderExportsAlpha,
        bool           blendSrcAlphaToColor,
        bool           enableAlphaToCoverage) const;

    PAL_DISALLOW_DEFAULT_CTOR(RsrcProcMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(RsrcProcMgr);
};

// =====================================================================================================================
// GFX9 specific implementation of RPM.
class Gfx9RsrcProcMgr : public Pal::Gfx9::RsrcProcMgr
{
public:
    explicit Gfx9RsrcProcMgr(Device* pDevice) : Pal::Gfx9::RsrcProcMgr(pDevice) {}
    virtual ~Gfx9RsrcProcMgr() {}

    void HwlResummarizeHtileCompute(
        GfxCmdBuffer*      pCmdBuffer,
        const GfxImage&    image,
        const SubresRange& range) const override;

protected:
    void ClearDccCompute(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange,
        uint8              clearCode,
        DccClearPurpose    clearPurpose,
        const uint32*      pPackedClearColor = nullptr) const override;

    void FastDepthStencilClearCompute(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        uint32             htileValue,
        uint32             clearMask,
        uint8              stencil) const override;

    virtual const Pal::ComputePipeline* GetCmdGenerationPipeline(
        const Pal::IndirectCmdGenerator& generator,
        const CmdBuffer&                 cmdBuffer) const override;

    void HwlDecodeBufferViewSrd(
        const void*     pBufferViewSrd,
        BufferViewInfo* pViewInfo) const override;

    void HwlDecodeImageViewSrd(
        const void*       pImageViewSrd,
        const Pal::Image& dstImage,
        SwizzledFormat*   pSwizzledFormat,
        SubresRange*      pSubresRange) const override;

    void InitCmask(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       image,
        const SubresRange& range) const override;

    void InitHtile(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange) const override;

private:
    void ClearHtileAllBytes(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        uint32             htileValue) const;

    void ClearHtileSelectedBytes(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        uint32             htileValue,
        uint32             htileMask) const;

    void DoFastClear(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange,
        uint8              clearCode,
        DccClearPurpose    clearPurpose) const;

    void DoOptimizedCmaskInit(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       image,
        const SubresRange& range,
        uint8              initValue
        ) const;

    void DoOptimizedFastClear(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange,
        uint8              clearCode,
        DccClearPurpose    clearPurpose) const;

    void DoOptimizedHtileFastClear(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        uint32             htileValue,
        uint32             htileMask) const;

    void ExecuteHtileEquation(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        uint32             htileValue,
        uint32             htileMask) const;

    void HwlHtileCopyAndFixUp(
        GfxCmdBuffer*             pCmdBuffer,
        const Pal::Image&         srcImage,
        const Pal::Image&         dstImage,
        ImageLayout               dstImageLayout,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        bool                      computeResolve) const override;

    void HwlUpdateDstImageFmaskMetaData(
        GfxCmdBuffer*          pCmdBuffer,
        const Pal::Image&      srcImage,
        const Pal::Image&      dstImage,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32                 flags) const override;

    virtual uint32 HwlBeginGraphicsCopy(
        Pal::GfxCmdBuffer*           pCmdBuffer,
        const Pal::GraphicsPipeline* pPipeline,
        const Pal::Image&            dstImage,
        uint32                       bpp) const override;

    virtual void HwlEndGraphicsCopy(
        Pal::CmdStream* pCmdStream,
        uint32          restoreMask) const override;

    PAL_DISALLOW_DEFAULT_CTOR(Gfx9RsrcProcMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx9RsrcProcMgr);
};

// =====================================================================================================================
// GFX10 specific implementation of RPM.
class Gfx10RsrcProcMgr : public Pal::Gfx9::RsrcProcMgr
{
public:
    explicit Gfx10RsrcProcMgr(Device* pDevice) : Pal::Gfx9::RsrcProcMgr(pDevice) {}
    virtual ~Gfx10RsrcProcMgr() {}

    virtual void HwlResummarizeHtileCompute(
        GfxCmdBuffer*      pCmdBuffer,
        const GfxImage&    image,
        const SubresRange& range) const override;

    virtual bool FastClearEliminate(
        GfxCmdBuffer*                pCmdBuffer,
        Pal::CmdStream*              pCmdStream,
        const Image&                 image,
        const IMsaaState*            pMsaaState,
        const MsaaQuadSamplePattern* pQuadSamplePattern,
        const SubresRange&           range) const override;

protected:
    virtual void ClearDccCompute(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange,
        uint8              clearCode,
        DccClearPurpose    clearPurpose,
        const uint32*      pPackedClearColor = nullptr) const override;

    virtual void FastDepthStencilClearCompute(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        uint32             htileValue,
        uint32             clearMask,
        uint8              stencil) const override;

    virtual const Pal::ComputePipeline* GetCmdGenerationPipeline(
        const Pal::IndirectCmdGenerator& generator,
        const CmdBuffer&                 cmdBuffer) const override;

    virtual void HwlDecodeBufferViewSrd(
        const void*     pBufferViewSrd,
        BufferViewInfo* pViewInfo) const override;

    virtual void HwlDecodeImageViewSrd(
        const void*       pImageViewSrd,
        const Pal::Image& dstImage,
        SwizzledFormat*   pSwizzledFormat,
        SubresRange*      pSubresRange) const override;

    virtual void InitCmask(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       image,
        const SubresRange& range) const override;

    virtual void InitHtile(
        GfxCmdBuffer*      pCmdBuffer,
        Pal::CmdStream*    pCmdStream,
        const Image&       dstImage,
        const SubresRange& clearRange) const override;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 478
    virtual void HwlCreateDecompressResolveSafeImageViewSrds(
        uint32                numSrds,
        const ImageViewInfo*  pImageView,
        void*                 pSrdTable) const override;
#endif

private:
    void InitHtileData(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        uint32             hTileValue,
        uint32             hTileMask) const;

    void WriteHtileData(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        const SubresRange& range,
        uint32             hTileValue,
        uint32             hTileMask,
        uint8              stencil) const;

    void ClearDccComputeSetFirstPixelOfBlock(
        GfxCmdBuffer*      pCmdBuffer,
        const Image&       dstImage,
        uint32             absMipLevel,
        uint32             bytesPerPixel,
        const uint32*      pPackedClearColor) const;

    virtual bool HwlCanDoDepthStencilCopyResolve(
        const Pal::Image&         srcImage,
        const Pal::Image&         dstImage,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions) const override;

    virtual void HwlHtileCopyAndFixUp(
        GfxCmdBuffer*             pCmdBuffer,
        const Pal::Image&         srcImage,
        const Pal::Image&         dstImage,
        ImageLayout               dstImageLayout,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        bool                      computeResolve) const override;

    virtual void HwlUpdateDstImageFmaskMetaData(
        GfxCmdBuffer*          pCmdBuffer,
        const Pal::Image&      srcImage,
        const Pal::Image&      dstImage,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32                 flags) const override;

    virtual void HwlUpdateDstImageStateMetaData(
        GfxCmdBuffer*          pCmdBuffer,
        const Pal::Image&      dstImage,
        const SubresRange&     range) const override;

    virtual bool HwlImageUsesCompressedWrites(
        const uint32* pImageSrd) const override;

    virtual uint32 HwlBeginGraphicsCopy(
        Pal::GfxCmdBuffer*           pCmdBuffer,
        const Pal::GraphicsPipeline* pPipeline,
        const Pal::Image&            dstImage,
        uint32                       bpp) const override;

    virtual void HwlEndGraphicsCopy(
        Pal::CmdStream* pCmdStream,
        uint32          restoreMask) const override;

    PAL_DISALLOW_DEFAULT_CTOR(Gfx10RsrcProcMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx10RsrcProcMgr);
};

} // Gfx9
} // Pal
