/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2007-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

/**
************************************************************************************************************************
* @file  gfx10addrlib.cpp
* @brief Contain the implementation for the Gfx10Lib class.
************************************************************************************************************************
*/

#include "gfx10addrlib.h"
#include "gfx10_gb_reg.h"
#include "gfx10SwizzlePattern.h"

#include "amdgpu_asic_addr.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Addr
{
/**
************************************************************************************************************************
*   Gfx10HwlInit
*
*   @brief
*       Creates an Gfx10Lib object.
*
*   @return
*       Returns an Gfx10Lib object pointer.
************************************************************************************************************************
*/
Addr::Lib* Gfx10HwlInit(const Client* pClient)
{
    return V2::Gfx10Lib::CreateObj(pClient);
}

namespace V2
{

////////////////////////////////////////////////////////////////////////////////////////////////////
//                               Static Const Member
////////////////////////////////////////////////////////////////////////////////////////////////////

const SwizzleModeFlags Gfx10Lib::SwizzleModeTable[ADDR_SW_MAX_TYPE] =
{//Linear 256B  4KB  64KB   Var    Z    Std   Disp  Rot   XOR    T    RtOpt
    {1,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}, // ADDR_SW_LINEAR
    {0,    1,    0,    0,    0,    0,    1,    0,    0,    0,    0,   0}, // ADDR_SW_256B_S
    {0,    1,    0,    0,    0,    0,    0,    1,    0,    0,    0,   0}, // ADDR_SW_256B_D
    {0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}, // Reserved

    {0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}, // Reserved
    {0,    0,    1,    0,    0,    0,    1,    0,    0,    0,    0,   0}, // ADDR_SW_4KB_S
    {0,    0,    1,    0,    0,    0,    0,    1,    0,    0,    0,   0}, // ADDR_SW_4KB_D
    {0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}, // Reserved

    {0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}, // Reserved
    {0,    0,    0,    1,    0,    0,    1,    0,    0,    0,    0,   0}, // ADDR_SW_64KB_S
    {0,    0,    0,    1,    0,    0,    0,    1,    0,    0,    0,   0}, // ADDR_SW_64KB_D
    {0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}, // Reserved

    {0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}, // Reserved
    {0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}, // Reserved
    {0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}, // Reserved
    {0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}, // Reserved

    {0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}, // Reserved
    {0,    0,    0,    1,    0,    0,    1,    0,    0,    1,    1,   0}, // ADDR_SW_64KB_S_T
    {0,    0,    0,    1,    0,    0,    0,    1,    0,    1,    1,   0}, // ADDR_SW_64KB_D_T
    {0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}, // Reserved

    {0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}, // Reserved
    {0,    0,    1,    0,    0,    0,    1,    0,    0,    1,    0,   0}, // ADDR_SW_4KB_S_X
    {0,    0,    1,    0,    0,    0,    0,    1,    0,    1,    0,   0}, // ADDR_SW_4KB_D_X
    {0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}, // Reserved

    {0,    0,    0,    1,    0,    1,    0,    0,    0,    1,    0,   0}, // ADDR_SW_64KB_Z_X
    {0,    0,    0,    1,    0,    0,    1,    0,    0,    1,    0,   0}, // ADDR_SW_64KB_S_X
    {0,    0,    0,    1,    0,    0,    0,    1,    0,    1,    0,   0}, // ADDR_SW_64KB_D_X
    {0,    0,    0,    1,    0,    0,    0,    0,    0,    1,    0,   1}, // ADDR_SW_64KB_R_X

    {0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}, // Reserved
    {0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}, // Reserved
    {0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}, // Reserved
    {0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}, // Reserved
    {1,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}, // ADDR_SW_LINEAR_GENERAL
};

const Dim3d Gfx10Lib::Block256_3d[] = {{8, 4, 8}, {4, 4, 8}, {4, 4, 4}, {4, 2, 4}, {2, 2, 4}};

const Dim3d Gfx10Lib::Block64K_3d[] = {{64, 32, 32}, {32 , 32, 32}, {32, 32, 16}, {32, 16, 16}, {16, 16, 16}};
const Dim3d Gfx10Lib::Block4K_3d[]  = {{16, 16, 16}, {8, 16, 16}, {8, 16, 8}, {8, 8, 8}, {4, 8, 8}};

const Dim2d Gfx10Lib::Block64K_2d[] = {{256, 256}, {256 , 128}, {128, 128}, {128, 64}, {64, 64}};
const Dim2d Gfx10Lib::Block4K_2d[]  = {{64, 64}, {64, 32}, {32, 32}, {32, 16}, {16, 16}};

const Dim3d Gfx10Lib::Block64K_Log2_3d[] = {{6, 5, 5}, {5, 5, 5}, {5, 5, 4}, {5, 4, 4}, {4, 4, 4}};
const Dim3d Gfx10Lib::Block4K_Log2_3d[]  = {{4, 4, 4}, {3, 4, 4}, {3, 4, 3}, {3, 3, 3}, {2, 3, 3}};

const Dim2d Gfx10Lib::Block64K_Log2_2d[] = {{8, 8}, {8, 7}, {7, 7}, {7, 6}, {6, 6}};
const Dim2d Gfx10Lib::Block4K_Log2_2d[]  = {{6, 6}, {6, 5}, {5, 5}, {5, 4}, {4, 4}};

/**
************************************************************************************************************************
*   Gfx10Lib::Gfx10Lib
*
*   @brief
*       Constructor
*
************************************************************************************************************************
*/
Gfx10Lib::Gfx10Lib(const Client* pClient)
    :
    Lib(pClient),
    m_numEquations(0),
    m_colorBaseIndex(0),
    m_xmaskBaseIndex(0),
    m_dccBaseIndex(0)
{
    m_class = AI_ADDRLIB;
    memset(&m_settings, 0, sizeof(m_settings));
    memcpy(m_swizzleModeTable, SwizzleModeTable, sizeof(SwizzleModeTable));
}

/**
************************************************************************************************************************
*   Gfx10Lib::~Gfx10Lib
*
*   @brief
*       Destructor
************************************************************************************************************************
*/
Gfx10Lib::~Gfx10Lib()
{
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlComputeHtileInfo
*
*   @brief
*       Interface function stub of AddrComputeHtilenfo
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::HwlComputeHtileInfo(
    const ADDR2_COMPUTE_HTILE_INFO_INPUT* pIn,    ///< [in] input structure
    ADDR2_COMPUTE_HTILE_INFO_OUTPUT*      pOut    ///< [out] output structure
    ) const
{
    ADDR_E_RETURNCODE ret = ADDR_OK;

    if ((pIn->swizzleMode            != ADDR_SW_64KB_Z_X) ||
        (pIn->hTileFlags.pipeAligned != TRUE))
    {
        ret = ADDR_INVALIDPARAMS;
    }
    else
    {
        Dim3d         metaBlk     = {0};
        const UINT_32 metaBlkSize = GetMetaBlkSize(Gfx10DataDepthStencil,
                                                   ADDR_RSRC_TEX_2D,
                                                   ADDR_SW_64KB_Z_X,
                                                   0,
                                                   0,
                                                   TRUE,
                                                   &metaBlk);

        pOut->pitch         = PowTwoAlign(pIn->unalignedWidth,  metaBlk.w);
        pOut->height        = PowTwoAlign(pIn->unalignedHeight, metaBlk.h);
        pOut->baseAlign     = Max(metaBlkSize, 1u << (m_pipesLog2 + 11u));
        pOut->metaBlkWidth  = metaBlk.w;
        pOut->metaBlkHeight = metaBlk.h;

        if (pIn->numMipLevels > 1)
        {
            ADDR_ASSERT(pIn->firstMipIdInTail <= pIn->numMipLevels);

            UINT_32 offset = (pIn->firstMipIdInTail == pIn->numMipLevels) ? 0 : metaBlkSize;

            for (INT_32 i = static_cast<INT_32>(pIn->firstMipIdInTail) - 1; i >=0; i--)
            {
                UINT_32 mipWidth, mipHeight;

                GetMipSize(pIn->unalignedWidth, pIn->unalignedHeight, 1, i, &mipWidth, &mipHeight);

                mipWidth  = PowTwoAlign(mipWidth,  metaBlk.w);
                mipHeight = PowTwoAlign(mipHeight, metaBlk.h);

                const UINT_32 pitchInM     = mipWidth  / metaBlk.w;
                const UINT_32 heightInM    = mipHeight / metaBlk.h;
                const UINT_32 mipSliceSize = pitchInM * heightInM * metaBlkSize;

                if (pOut->pMipInfo != NULL)
                {
                    pOut->pMipInfo[i].inMiptail = FALSE;
                    pOut->pMipInfo[i].offset    = offset;
                    pOut->pMipInfo[i].sliceSize = mipSliceSize;
                }

                offset += mipSliceSize;
            }

            pOut->sliceSize          = offset;
            pOut->metaBlkNumPerSlice = offset / metaBlkSize;
            pOut->htileBytes         = pOut->sliceSize * pIn->numSlices;

            if (pOut->pMipInfo != NULL)
            {
                for (UINT_32 i = pIn->firstMipIdInTail; i < pIn->numMipLevels; i++)
                {
                    pOut->pMipInfo[i].inMiptail = TRUE;
                    pOut->pMipInfo[i].offset    = 0;
                    pOut->pMipInfo[i].sliceSize = 0;
                }

                if (pIn->firstMipIdInTail != pIn->numMipLevels)
                {
                    pOut->pMipInfo[pIn->firstMipIdInTail].sliceSize = metaBlkSize;
                }
            }
        }
        else
        {
            const UINT_32 pitchInM  = pOut->pitch  / metaBlk.w;
            const UINT_32 heightInM = pOut->height / metaBlk.h;

            pOut->metaBlkNumPerSlice    = pitchInM * heightInM;
            pOut->sliceSize             = pOut->metaBlkNumPerSlice * metaBlkSize;
            pOut->htileBytes            = pOut->sliceSize * pIn->numSlices;

            if (pOut->pMipInfo != NULL)
            {
                pOut->pMipInfo[0].inMiptail = FALSE;
                pOut->pMipInfo[0].offset    = 0;
                pOut->pMipInfo[0].sliceSize = pOut->sliceSize;
            }
        }
    }

    return ret;
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlComputeCmaskInfo
*
*   @brief
*       Interface function stub of AddrComputeCmaskInfo
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::HwlComputeCmaskInfo(
    const ADDR2_COMPUTE_CMASK_INFO_INPUT* pIn,    ///< [in] input structure
    ADDR2_COMPUTE_CMASK_INFO_OUTPUT*      pOut    ///< [out] output structure
    ) const
{
    ADDR_E_RETURNCODE ret = ADDR_OK;

    if ((pIn->resourceType           != ADDR_RSRC_TEX_2D) ||
        (pIn->cMaskFlags.pipeAligned != TRUE))
    {
        ret = ADDR_INVALIDPARAMS;
    }
    else
    {
        Dim3d         metaBlk     = {0};
        const UINT_32 metaBlkSize = GetMetaBlkSize(Gfx10DataFmask,
                                                   ADDR_RSRC_TEX_2D,
                                                   ADDR_SW_64KB_Z_X,
                                                   0,
                                                   0,
                                                   TRUE,
                                                   &metaBlk);

        pOut->pitch         = PowTwoAlign(pIn->unalignedWidth,  metaBlk.w);
        pOut->height        = PowTwoAlign(pIn->unalignedHeight, metaBlk.h);
        pOut->baseAlign     = metaBlkSize;
        pOut->metaBlkWidth  = metaBlk.w;
        pOut->metaBlkHeight = metaBlk.h;

        if (pIn->numMipLevels > 1)
        {
            ADDR_ASSERT(pIn->firstMipIdInTail <= pIn->numMipLevels);

            UINT_32 metaBlkPerSlice = (pIn->firstMipIdInTail == pIn->numMipLevels) ? 0 : 1;

            for (INT_32 i = static_cast<INT_32>(pIn->firstMipIdInTail) - 1; i >= 0; i--)
            {
                UINT_32 mipWidth, mipHeight;

                GetMipSize(pIn->unalignedWidth, pIn->unalignedHeight, 1, i, &mipWidth, &mipHeight);

                mipWidth  = PowTwoAlign(mipWidth,  metaBlk.w);
                mipHeight = PowTwoAlign(mipHeight, metaBlk.h);

                const UINT_32 pitchInM  = mipWidth  / metaBlk.w;
                const UINT_32 heightInM = mipHeight / metaBlk.h;

                if (pOut->pMipInfo != NULL)
                {
                    pOut->pMipInfo[i].inMiptail = FALSE;
                    pOut->pMipInfo[i].offset    = metaBlkPerSlice * metaBlkSize;
                    pOut->pMipInfo[i].sliceSize = pitchInM * heightInM * metaBlkSize;
                }

                metaBlkPerSlice += pitchInM * heightInM;
            }

            pOut->metaBlkNumPerSlice = metaBlkPerSlice;

            if (pOut->pMipInfo != NULL)
            {
                for (UINT_32 i = pIn->firstMipIdInTail; i < pIn->numMipLevels; i++)
                {
                    pOut->pMipInfo[i].inMiptail = TRUE;
                    pOut->pMipInfo[i].offset    = 0;
                    pOut->pMipInfo[i].sliceSize = 0;
                }

                if (pIn->firstMipIdInTail != pIn->numMipLevels)
                {
                    pOut->pMipInfo[pIn->firstMipIdInTail].sliceSize = metaBlkSize;
                }
            }
        }
        else
        {
            const UINT_32 pitchInM  = pOut->pitch  / metaBlk.w;
            const UINT_32 heightInM = pOut->height / metaBlk.h;

            pOut->metaBlkNumPerSlice = pitchInM * heightInM;

            if (pOut->pMipInfo != NULL)
            {
                pOut->pMipInfo[0].inMiptail = FALSE;
                pOut->pMipInfo[0].offset    = 0;
                pOut->pMipInfo[0].sliceSize = pOut->metaBlkNumPerSlice * metaBlkSize;
            }
        }

        pOut->sliceSize  = pOut->metaBlkNumPerSlice * metaBlkSize;
        pOut->cmaskBytes = pOut->sliceSize * pIn->numSlices;
    }

    return ret;
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlComputeDccInfo
*
*   @brief
*       Interface function to compute DCC key info
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::HwlComputeDccInfo(
    const ADDR2_COMPUTE_DCCINFO_INPUT* pIn,    ///< [in] input structure
    ADDR2_COMPUTE_DCCINFO_OUTPUT*      pOut    ///< [out] output structure
    ) const
{
    ADDR_E_RETURNCODE ret = ADDR_OK;

    if (IsLinear(pIn->swizzleMode) || IsBlock256b(pIn->swizzleMode))
    {
        // Hardware support dcc for 256 swizzle mode, but address lib will not support it because we only
        // select 256 swizzle mode for small surface, and it's not helpful to enable dcc for small surface.
        ret = ADDR_INVALIDPARAMS;
    }
    else if (m_settings.dccUnsup3DSwDis && IsTex3d(pIn->resourceType) && IsDisplaySwizzle(pIn->swizzleMode))
    {
        // DCC is not supported on 3D Display surfaces for GFX10.0 and GFX10.1
        ret = ADDR_INVALIDPARAMS;
    }
    else
    {
        // only SW_*_R_X surfaces may be DCC compressed when attached to the CB
        ADDR_ASSERT(IsRtOptSwizzle(pIn->swizzleMode));

        Dim3d         metaBlk     = {0};
        const UINT_32 elemLog2    = Log2(pIn->bpp >> 3);
        const UINT_32 numFragLog2 = Log2(pIn->numFrags);
        const UINT_32 metaBlkSize = GetMetaBlkSize(Gfx10DataColor,
                                                   pIn->resourceType,
                                                   pIn->swizzleMode,
                                                   elemLog2,
                                                   numFragLog2,
                                                   pIn->dccKeyFlags.pipeAligned,
                                                   &metaBlk);
        const BOOL_32 isThick     = IsThick(pIn->resourceType, pIn->swizzleMode);

        pOut->compressBlkWidth  = isThick ? Block256_3d[elemLog2].w : Block256_2d[elemLog2].w;
        pOut->compressBlkHeight = isThick ? Block256_3d[elemLog2].h : Block256_2d[elemLog2].h;
        pOut->compressBlkDepth  = isThick ? Block256_3d[elemLog2].d : 1;

        pOut->dccRamBaseAlign   = metaBlkSize;
        pOut->metaBlkWidth      = metaBlk.w;
        pOut->metaBlkHeight     = metaBlk.h;
        pOut->metaBlkDepth      = metaBlk.d;

        pOut->pitch             = PowTwoAlign(pIn->unalignedWidth,  metaBlk.w);
        pOut->height            = PowTwoAlign(pIn->unalignedHeight, metaBlk.h);
        pOut->depth             = PowTwoAlign(pIn->numSlices,       metaBlk.d);

        if (pIn->numMipLevels > 1)
        {
            ADDR_ASSERT(pIn->firstMipIdInTail <= pIn->numMipLevels);

            UINT_32 offset = (pIn->firstMipIdInTail == pIn->numMipLevels) ? 0 : metaBlkSize;

            for (INT_32 i = static_cast<INT_32>(pIn->firstMipIdInTail) - 1; i >= 0; i--)
            {
                UINT_32 mipWidth, mipHeight;

                GetMipSize(pIn->unalignedWidth, pIn->unalignedHeight, 1, i, &mipWidth, &mipHeight);

                mipWidth  = PowTwoAlign(mipWidth,  metaBlk.w);
                mipHeight = PowTwoAlign(mipHeight, metaBlk.h);

                const UINT_32 pitchInM     = mipWidth  / metaBlk.w;
                const UINT_32 heightInM    = mipHeight / metaBlk.h;
                const UINT_32 mipSliceSize = pitchInM * heightInM * metaBlkSize;

                if (pOut->pMipInfo != NULL)
                {
                    pOut->pMipInfo[i].inMiptail = FALSE;
                    pOut->pMipInfo[i].offset    = offset;
                    pOut->pMipInfo[i].sliceSize = mipSliceSize;
                }

                offset += mipSliceSize;
            }

            pOut->dccRamSliceSize    = offset;
            pOut->metaBlkNumPerSlice = offset / metaBlkSize;
            pOut->dccRamSize         = pOut->dccRamSliceSize * (pOut->depth  / metaBlk.d);

            if (pOut->pMipInfo != NULL)
            {
                for (UINT_32 i = pIn->firstMipIdInTail; i < pIn->numMipLevels; i++)
                {
                    pOut->pMipInfo[i].inMiptail = TRUE;
                    pOut->pMipInfo[i].offset    = 0;
                    pOut->pMipInfo[i].sliceSize = 0;
                }

                if (pIn->firstMipIdInTail != pIn->numMipLevels)
                {
                    pOut->pMipInfo[pIn->firstMipIdInTail].sliceSize = metaBlkSize;
                }
            }
        }
        else
        {
            const UINT_32 pitchInM  = pOut->pitch  / metaBlk.w;
            const UINT_32 heightInM = pOut->height / metaBlk.h;

            pOut->metaBlkNumPerSlice = pitchInM * heightInM;
            pOut->dccRamSliceSize    = pOut->metaBlkNumPerSlice * metaBlkSize;
            pOut->dccRamSize         = pOut->dccRamSliceSize * (pOut->depth  / metaBlk.d);

            if (pOut->pMipInfo != NULL)
            {
                pOut->pMipInfo[0].inMiptail = FALSE;
                pOut->pMipInfo[0].offset    = 0;
                pOut->pMipInfo[0].sliceSize = pOut->dccRamSliceSize;
            }
        }
    }

    return ret;
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlComputeCmaskAddrFromCoord
*
*   @brief
*       Interface function stub of AddrComputeCmaskAddrFromCoord
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::HwlComputeCmaskAddrFromCoord(
    const ADDR2_COMPUTE_CMASK_ADDRFROMCOORD_INPUT* pIn,    ///< [in] input structure
    ADDR2_COMPUTE_CMASK_ADDRFROMCOORD_OUTPUT*      pOut)   ///< [out] output structure
{
    // Only support pipe aligned CMask
    ADDR_ASSERT(pIn->cMaskFlags.pipeAligned == TRUE);

    ADDR2_COMPUTE_CMASK_INFO_INPUT input = {};
    input.size            = sizeof(input);
    input.cMaskFlags      = pIn->cMaskFlags;
    input.colorFlags      = pIn->colorFlags;
    input.unalignedWidth  = Max(pIn->unalignedWidth,  1u);
    input.unalignedHeight = Max(pIn->unalignedHeight, 1u);
    input.numSlices       = Max(pIn->numSlices,       1u);
    input.swizzleMode     = pIn->swizzleMode;
    input.resourceType    = pIn->resourceType;

    ADDR2_COMPUTE_CMASK_INFO_OUTPUT output = {};
    output.size = sizeof(output);

    ADDR_E_RETURNCODE returnCode = ComputeCmaskInfo(&input, &output);

    if (returnCode == ADDR_OK)
    {
        const UINT_32  fmaskBpp      = GetFmaskBpp(pIn->numSamples, pIn->numFrags);
        const UINT_32  fmaskElemLog2 = Log2(fmaskBpp >> 3);
        const UINT_32  pipeMask      = (1 << m_pipesLog2) - 1;
        const UINT_32  index         = m_xmaskBaseIndex + fmaskElemLog2;
        const UINT_16* patIdxTable   = m_settings.supportRbPlus ? CMASK_64K_RBPLUS_PATIDX : CMASK_64K_PATIDX;

        const UINT_32  blkSizeLog2  = Log2(output.metaBlkWidth) + Log2(output.metaBlkHeight) - 7;
        const UINT_32  blkMask      = (1 << blkSizeLog2) - 1;
        const UINT_32  blkOffset    = ComputeOffsetFromSwizzlePattern(CMASK_64K_SW_PATTERN[patIdxTable[index]],
                                                                      blkSizeLog2 + 1, // +1 for nibble offset
                                                                      pIn->x,
                                                                      pIn->y,
                                                                      pIn->slice,
                                                                      0);
        const UINT_32 xb       = pIn->x / output.metaBlkWidth;
        const UINT_32 yb       = pIn->y / output.metaBlkHeight;
        const UINT_32 pb       = output.pitch / output.metaBlkWidth;
        const UINT_32 blkIndex = (yb * pb) + xb;
        const UINT_32 pipeXor  = ((pIn->pipeXor & pipeMask) << m_pipeInterleaveLog2) & blkMask;

        pOut->addr = (output.sliceSize * pIn->slice) +
                     (blkIndex * (1 << blkSizeLog2)) +
                     ((blkOffset >> 1) ^ pipeXor);
        pOut->bitPosition = (blkOffset & 1) << 2;
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlComputeHtileAddrFromCoord
*
*   @brief
*       Interface function stub of AddrComputeHtileAddrFromCoord
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::HwlComputeHtileAddrFromCoord(
    const ADDR2_COMPUTE_HTILE_ADDRFROMCOORD_INPUT* pIn,    ///< [in] input structure
    ADDR2_COMPUTE_HTILE_ADDRFROMCOORD_OUTPUT*      pOut)   ///< [out] output structure
{
    ADDR_E_RETURNCODE returnCode = ADDR_OK;

    if (pIn->numMipLevels > 1)
    {
        returnCode = ADDR_NOTIMPLEMENTED;
    }
    else
    {
        ADDR2_COMPUTE_HTILE_INFO_INPUT input = {0};
        input.size            = sizeof(input);
        input.hTileFlags      = pIn->hTileFlags;
        input.depthFlags      = pIn->depthflags;
        input.swizzleMode     = pIn->swizzleMode;
        input.unalignedWidth  = Max(pIn->unalignedWidth,  1u);
        input.unalignedHeight = Max(pIn->unalignedHeight, 1u);
        input.numSlices       = Max(pIn->numSlices,       1u);
        input.numMipLevels    = 1;

        ADDR2_COMPUTE_HTILE_INFO_OUTPUT output = {0};
        output.size = sizeof(output);

        returnCode = ComputeHtileInfo(&input, &output);

        if (returnCode == ADDR_OK)
        {
            const UINT_32  numSampleLog2 = Log2(pIn->numSamples);
            const UINT_32  pipeMask      = (1 << m_pipesLog2) - 1;
            const UINT_32  index         = m_xmaskBaseIndex + numSampleLog2;
            const UINT_16* patIdxTable   = m_settings.supportRbPlus ? HTILE_64K_RBPLUS_PATIDX : HTILE_64K_PATIDX;

            const UINT_32  blkSizeLog2   = Log2(output.metaBlkWidth) + Log2(output.metaBlkHeight) - 4;
            const UINT_32  blkMask       = (1 << blkSizeLog2) - 1;
            const UINT_32  blkOffset     = ComputeOffsetFromSwizzlePattern(HTILE_64K_SW_PATTERN[patIdxTable[index]],
                                                                           blkSizeLog2 + 1, // +1 for nibble offset
                                                                           pIn->x,
                                                                           pIn->y,
                                                                           pIn->slice,
                                                                           0);
            const UINT_32 xb       = pIn->x / output.metaBlkWidth;
            const UINT_32 yb       = pIn->y / output.metaBlkHeight;
            const UINT_32 pb       = output.pitch / output.metaBlkWidth;
            const UINT_32 blkIndex = (yb * pb) + xb;
            const UINT_32 pipeXor  = ((pIn->pipeXor & pipeMask) << m_pipeInterleaveLog2) & blkMask;

            pOut->addr = (static_cast<UINT_64>(output.sliceSize) * pIn->slice) +
                         (blkIndex * (1 << blkSizeLog2)) +
                         ((blkOffset >> 1) ^ pipeXor);
        }
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlComputeHtileCoordFromAddr
*
*   @brief
*       Interface function stub of AddrComputeHtileCoordFromAddr
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::HwlComputeHtileCoordFromAddr(
    const ADDR2_COMPUTE_HTILE_COORDFROMADDR_INPUT* pIn,    ///< [in] input structure
    ADDR2_COMPUTE_HTILE_COORDFROMADDR_OUTPUT*      pOut)   ///< [out] output structure
{
    ADDR_NOT_IMPLEMENTED();

    return ADDR_OK;
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlComputeDccAddrFromCoord
*
*   @brief
*       Interface function stub of AddrComputeDccAddrFromCoord
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::HwlComputeDccAddrFromCoord(
    const ADDR2_COMPUTE_DCC_ADDRFROMCOORD_INPUT* pIn,  ///< [in] input structure
    ADDR2_COMPUTE_DCC_ADDRFROMCOORD_OUTPUT*      pOut) ///< [out] output structure
{
    ADDR_E_RETURNCODE returnCode = ADDR_OK;

    if ((pIn->resourceType       != ADDR_RSRC_TEX_2D) ||
        (pIn->swizzleMode        != ADDR_SW_64KB_R_X) ||
        (pIn->dccKeyFlags.linear == TRUE)             ||
        (pIn->numFrags           >  1)                ||
        (pIn->numMipLevels       >  1)                ||
        (pIn->mipId              >  0))
    {
        returnCode = ADDR_NOTSUPPORTED;
    }
    else
    {
        ADDR2_COMPUTE_DCCINFO_INPUT input = {0};
        input.size            = sizeof(input);
        input.dccKeyFlags     = pIn->dccKeyFlags;
        input.colorFlags      = pIn->colorFlags;
        input.swizzleMode     = pIn->swizzleMode;
        input.resourceType    = pIn->resourceType;
        input.bpp             = pIn->bpp;
        input.unalignedWidth  = Max(pIn->unalignedWidth,  1u);
        input.unalignedHeight = Max(pIn->unalignedHeight, 1u);
        input.numSlices       = Max(pIn->numSlices,       1u);
        input.numFrags        = Max(pIn->numFrags,        1u);
        input.numMipLevels    = Max(pIn->numMipLevels,    1u);

        ADDR2_COMPUTE_DCCINFO_OUTPUT output = {0};
        output.size = sizeof(output);

        returnCode = ComputeDccInfo(&input, &output);

        if (returnCode == ADDR_OK)
        {
            const UINT_32  elemLog2    = Log2(pIn->bpp >> 3);
            const UINT_32  numPipeLog2 = m_pipesLog2;
            const UINT_32  pipeMask    = (1 << numPipeLog2) - 1;
            UINT_32        index       = m_dccBaseIndex + elemLog2;
            const UINT_16* patIdxTable;

            if (m_settings.supportRbPlus)
            {
                patIdxTable = DCC_64K_R_X_RBPLUS_PATIDX;

                if (pIn->dccKeyFlags.pipeAligned)
                {
                    index += MaxNumOfBpp;

                    if (m_numPkrLog2 < 2)
                    {
                        index += m_pipesLog2 * MaxNumOfBpp;
                    }
                    else
                    {
                        // 4 groups for "m_numPkrLog2 < 2" case
                        index += 4 * MaxNumOfBpp;

                        const UINT_32 dccPipePerPkr = 3;

                        index += (m_numPkrLog2 - 2) * dccPipePerPkr * MaxNumOfBpp +
                                 (m_pipesLog2 - m_numPkrLog2) * MaxNumOfBpp;
                    }
                }
            }
            else
            {
                patIdxTable = DCC_64K_R_X_PATIDX;

                if (pIn->dccKeyFlags.pipeAligned)
                {
                    index += (numPipeLog2 + UnalignedDccType) * MaxNumOfBpp;
                }
                else
                {
                    index += Min(numPipeLog2, UnalignedDccType - 1) * MaxNumOfBpp;
                }
            }

            const UINT_32  blkSizeLog2 = Log2(output.metaBlkWidth) + Log2(output.metaBlkHeight) + elemLog2 - 8;
            const UINT_32  blkMask     = (1 << blkSizeLog2) - 1;
            const UINT_32  blkOffset   = ComputeOffsetFromSwizzlePattern(DCC_64K_R_X_SW_PATTERN[patIdxTable[index]],
                                                                         blkSizeLog2 + 1, // +1 for nibble offset
                                                                         pIn->x,
                                                                         pIn->y,
                                                                         pIn->slice,
                                                                         0);
            const UINT_32 xb       = pIn->x / output.metaBlkWidth;
            const UINT_32 yb       = pIn->y / output.metaBlkHeight;
            const UINT_32 pb       = output.pitch / output.metaBlkWidth;
            const UINT_32 blkIndex = (yb * pb) + xb;
            const UINT_32 pipeXor  = ((pIn->pipeXor & pipeMask) << m_pipeInterleaveLog2) & blkMask;

            pOut->addr = (static_cast<UINT_64>(output.dccRamSliceSize) * pIn->slice) +
                         (blkIndex * (1 << blkSizeLog2)) +
                         ((blkOffset >> 1) ^ pipeXor);
        }
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlInitGlobalParams
*
*   @brief
*       Initializes global parameters
*
*   @return
*       TRUE if all settings are valid
*
************************************************************************************************************************
*/
BOOL_32 Gfx10Lib::HwlInitGlobalParams(
    const ADDR_CREATE_INPUT* pCreateIn) ///< [in] create input
{
    BOOL_32        valid = TRUE;
    GB_ADDR_CONFIG gbAddrConfig;

    gbAddrConfig.u32All = pCreateIn->regValue.gbAddrConfig;

    // These values are copied from CModel code
    switch (gbAddrConfig.bits.NUM_PIPES)
    {
        case ADDR_CONFIG_1_PIPE:
            m_pipes     = 1;
            m_pipesLog2 = 0;
            break;
        case ADDR_CONFIG_2_PIPE:
            m_pipes     = 2;
            m_pipesLog2 = 1;
            break;
        case ADDR_CONFIG_4_PIPE:
            m_pipes     = 4;
            m_pipesLog2 = 2;
            break;
        case ADDR_CONFIG_8_PIPE:
            m_pipes     = 8;
            m_pipesLog2 = 3;
            break;
        case ADDR_CONFIG_16_PIPE:
            m_pipes     = 16;
            m_pipesLog2 = 4;
            break;
        case ADDR_CONFIG_32_PIPE:
            m_pipes     = 32;
            m_pipesLog2 = 5;
            break;
        case ADDR_CONFIG_64_PIPE:
            m_pipes     = 64;
            m_pipesLog2 = 6;
            break;
        default:
            ADDR_ASSERT_ALWAYS();
            valid = FALSE;
            break;
    }

    switch (gbAddrConfig.bits.PIPE_INTERLEAVE_SIZE)
    {
        case ADDR_CONFIG_PIPE_INTERLEAVE_256B:
            m_pipeInterleaveBytes = ADDR_PIPEINTERLEAVE_256B;
            m_pipeInterleaveLog2  = 8;
            break;
        case ADDR_CONFIG_PIPE_INTERLEAVE_512B:
            m_pipeInterleaveBytes = ADDR_PIPEINTERLEAVE_512B;
            m_pipeInterleaveLog2  = 9;
            break;
        case ADDR_CONFIG_PIPE_INTERLEAVE_1KB:
            m_pipeInterleaveBytes = ADDR_PIPEINTERLEAVE_1KB;
            m_pipeInterleaveLog2  = 10;
            break;
        case ADDR_CONFIG_PIPE_INTERLEAVE_2KB:
            m_pipeInterleaveBytes = ADDR_PIPEINTERLEAVE_2KB;
            m_pipeInterleaveLog2  = 11;
            break;
        default:
            ADDR_ASSERT_ALWAYS();
            valid = FALSE;
            break;
    }

    // Addr::V2::Lib::ComputePipeBankXor()/ComputeSlicePipeBankXor() requires pipe interleave to be exactly 8 bits, and
    // any larger value requires a post-process (left shift) on the output pipeBankXor bits.
    // And more importantly, SW AddrLib doesn't support sw equation/pattern for PI != 256 case.
    ADDR_ASSERT(m_pipeInterleaveBytes == ADDR_PIPEINTERLEAVE_256B);

    switch (gbAddrConfig.bits.MAX_COMPRESSED_FRAGS)
    {
        case ADDR_CONFIG_1_MAX_COMPRESSED_FRAGMENTS:
            m_maxCompFrag     = 1;
            m_maxCompFragLog2 = 0;
            break;
        case ADDR_CONFIG_2_MAX_COMPRESSED_FRAGMENTS:
            m_maxCompFrag     = 2;
            m_maxCompFragLog2 = 1;
            break;
        case ADDR_CONFIG_4_MAX_COMPRESSED_FRAGMENTS:
            m_maxCompFrag     = 4;
            m_maxCompFragLog2 = 2;
            break;
        case ADDR_CONFIG_8_MAX_COMPRESSED_FRAGMENTS:
            m_maxCompFrag     = 8;
            m_maxCompFragLog2 = 3;
            break;
        default:
            ADDR_ASSERT_ALWAYS();
            valid = FALSE;
            break;
    }

    {
        // Skip unaligned case
        m_xmaskBaseIndex += MaxNumOfAA;

        m_xmaskBaseIndex += m_pipesLog2 * MaxNumOfAA;
        m_colorBaseIndex += m_pipesLog2 * MaxNumOfBpp;

        if (m_settings.supportRbPlus)
        {
            m_numPkrLog2 = gbAddrConfig.bits.NUM_PKRS;
            m_numSaLog2  = (m_numPkrLog2 > 0) ? (m_numPkrLog2 - 1) : 0;

            ADDR_ASSERT((m_numPkrLog2 <= m_pipesLog2) && ((m_pipesLog2 - m_numPkrLog2) <= 2));

            ADDR_C_ASSERT(sizeof(HTILE_64K_RBPLUS_PATIDX) / sizeof(HTILE_64K_RBPLUS_PATIDX[0]) ==
                          sizeof(CMASK_64K_RBPLUS_PATIDX) / sizeof(CMASK_64K_RBPLUS_PATIDX[0]));

            if (m_numPkrLog2 >= 2)
            {
                m_colorBaseIndex += (2 * m_numPkrLog2 - 2) * MaxNumOfBpp;
                m_xmaskBaseIndex += (m_numPkrLog2 - 1) * 3 * MaxNumOfAA;
            }
        }
        else
        {
            const UINT_32 numPipeType = static_cast<UINT_32>(ADDR_CONFIG_64_PIPE) -
                                        static_cast<UINT_32>(ADDR_CONFIG_1_PIPE)  +
                                        1;

            ADDR_C_ASSERT(sizeof(HTILE_64K_PATIDX) / sizeof(HTILE_64K_PATIDX[0]) == (numPipeType + 1) * MaxNumOfAA);

            ADDR_C_ASSERT(sizeof(HTILE_64K_PATIDX) / sizeof(HTILE_64K_PATIDX[0]) ==
                          sizeof(CMASK_64K_PATIDX) / sizeof(CMASK_64K_PATIDX[0]));
        }
    }

    if (valid)
    {
        InitEquationTable();
    }

    return valid;
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlConvertChipFamily
*
*   @brief
*       Convert familyID defined in atiid.h to ChipFamily and set m_chipFamily/m_chipRevision
*   @return
*       ChipFamily
************************************************************************************************************************
*/
ChipFamily Gfx10Lib::HwlConvertChipFamily(
    UINT_32 chipFamily,        ///< [in] chip family defined in atiih.h
    UINT_32 chipRevision)      ///< [in] chip revision defined in "asic_family"_id.h
{
    ChipFamily family = ADDR_CHIP_FAMILY_NAVI;

    m_settings.dccUnsup3DSwDis = 1;

    switch (chipFamily)
    {
        case FAMILY_NV:
            m_settings.isDcn2 = 1;

            break;

        default:
            ADDR_ASSERT(!"Unknown chip family");
            break;
    }

    m_settings.dsMipmapHtileFix = 1;

    if (ASICREV_IS_NAVI10_P(chipRevision))
    {
        m_settings.dsMipmapHtileFix = 0;
    }

    m_configFlags.use32bppFor422Fmt = TRUE;

    return family;
}

/**
************************************************************************************************************************
*   Gfx10Lib::GetBlk256SizeLog2
*
*   @brief
*       Get block 256 size
*
*   @return
*       N/A
************************************************************************************************************************
*/
void Gfx10Lib::GetBlk256SizeLog2(
    AddrResourceType resourceType,      ///< [in] Resource type
    AddrSwizzleMode  swizzleMode,       ///< [in] Swizzle mode
    UINT_32          elemLog2,          ///< [in] element size log2
    UINT_32          numSamplesLog2,    ///< [in] number of samples
    Dim3d*           pBlock             ///< [out] block size
    ) const
{
    if (IsThin(resourceType, swizzleMode))
    {
        UINT_32 blockBits = 8 - elemLog2;

        if (IsZOrderSwizzle(swizzleMode))
        {
            blockBits -= numSamplesLog2;
        }

        pBlock->w = (blockBits >> 1) + (blockBits & 1);
        pBlock->h = (blockBits >> 1);
        pBlock->d = 0;
    }
    else
    {
        ADDR_ASSERT(IsThick(resourceType, swizzleMode));

        UINT_32 blockBits = 8 - elemLog2;

        pBlock->d = (blockBits / 3) + (((blockBits % 3) > 0) ? 1 : 0);
        pBlock->w = (blockBits / 3) + (((blockBits % 3) > 1) ? 1 : 0);
        pBlock->h = (blockBits / 3);
    }
}

/**
************************************************************************************************************************
*   Gfx10Lib::GetCompressedBlockSizeLog2
*
*   @brief
*       Get compress block size
*
*   @return
*       N/A
************************************************************************************************************************
*/
void Gfx10Lib::GetCompressedBlockSizeLog2(
    Gfx10DataType    dataType,          ///< [in] Data type
    AddrResourceType resourceType,      ///< [in] Resource type
    AddrSwizzleMode  swizzleMode,       ///< [in] Swizzle mode
    UINT_32          elemLog2,          ///< [in] element size log2
    UINT_32          numSamplesLog2,    ///< [in] number of samples
    Dim3d*           pBlock             ///< [out] block size
    ) const
{
    if (dataType == Gfx10DataColor)
    {
        GetBlk256SizeLog2(resourceType, swizzleMode, elemLog2, numSamplesLog2, pBlock);
    }
    else
    {
        ADDR_ASSERT((dataType == Gfx10DataDepthStencil) || (dataType == Gfx10DataFmask));
        pBlock->w = 3;
        pBlock->h = 3;
        pBlock->d = 0;
    }
}

/**
************************************************************************************************************************
*   Gfx10Lib::GetMetaOverlapLog2
*
*   @brief
*       Get meta block overlap
*
*   @return
*       N/A
************************************************************************************************************************
*/
INT_32 Gfx10Lib::GetMetaOverlapLog2(
    Gfx10DataType    dataType,          ///< [in] Data type
    AddrResourceType resourceType,      ///< [in] Resource type
    AddrSwizzleMode  swizzleMode,       ///< [in] Swizzle mode
    UINT_32          elemLog2,          ///< [in] element size log2
    UINT_32          numSamplesLog2     ///< [in] number of samples
    ) const
{
    Dim3d compBlock;
    Dim3d microBlock;

    GetCompressedBlockSizeLog2(dataType, resourceType, swizzleMode, elemLog2, numSamplesLog2, &compBlock);
    GetBlk256SizeLog2(resourceType, swizzleMode, elemLog2, numSamplesLog2, &microBlock);

    const INT_32 compSizeLog2   = compBlock.w  + compBlock.h  + compBlock.d;
    const INT_32 blk256SizeLog2 = microBlock.w + microBlock.h + microBlock.d;
    const INT_32 maxSizeLog2    = Max(compSizeLog2, blk256SizeLog2);
    const INT_32 numPipesLog2   = GetEffectiveNumPipes();
    INT_32       overlap        = numPipesLog2 - maxSizeLog2;

    if ((numPipesLog2 > 1) && m_settings.supportRbPlus)
    {
        overlap++;
    }

    // In 16Bpp 8xaa, we lose 1 overlap bit because the block size reduction eats into a pipe anchor bit (y4)
    if ((elemLog2 == 4) && (numSamplesLog2 == 3))
    {
        overlap--;
    }
    overlap = Max(overlap, 0);
    return overlap;
}

/**
************************************************************************************************************************
*   Gfx10Lib::Get3DMetaOverlapLog2
*
*   @brief
*       Get 3d meta block overlap
*
*   @return
*       N/A
************************************************************************************************************************
*/
INT_32 Gfx10Lib::Get3DMetaOverlapLog2(
    AddrResourceType resourceType,      ///< [in] Resource type
    AddrSwizzleMode  swizzleMode,       ///< [in] Swizzle mode
    UINT_32          elemLog2           ///< [in] element size log2
    ) const
{
    Dim3d microBlock;
    GetBlk256SizeLog2(resourceType, swizzleMode, elemLog2, 0, &microBlock);

    INT_32 overlap = GetEffectiveNumPipes() - static_cast<INT_32>(microBlock.w);

    if (m_settings.supportRbPlus)
    {
        overlap++;
    }

    if ((overlap < 0) || (IsStandardSwizzle(resourceType, swizzleMode) == TRUE))
    {
        overlap = 0;
    }
    return overlap;
}

/**
************************************************************************************************************************
*   Gfx10Lib::GetPipeRotateAmount
*
*   @brief
*       Get pipe rotate amount
*
*   @return
*       Pipe rotate amount
************************************************************************************************************************
*/

INT_32 Gfx10Lib::GetPipeRotateAmount(
    AddrResourceType resourceType,      ///< [in] Resource type
    AddrSwizzleMode  swizzleMode        ///< [in] Swizzle mode
    ) const
{
    INT_32 amount = 0;

    if (m_settings.supportRbPlus && (m_pipesLog2 >= (m_numSaLog2 + 1)) && (m_pipesLog2 > 1))
    {
        amount = ((m_pipesLog2 == (m_numSaLog2 + 1)) && IsRbAligned(resourceType, swizzleMode)) ?
                 1 : m_pipesLog2 - (m_numSaLog2 + 1);
    }

    return amount;
}

/**
************************************************************************************************************************
*   Gfx10Lib::GetMetaBlkSize
*
*   @brief
*       Get metadata block size
*
*   @return
*       Meta block size
************************************************************************************************************************
*/
UINT_32 Gfx10Lib::GetMetaBlkSize(
    Gfx10DataType    dataType,          ///< [in] Data type
    AddrResourceType resourceType,      ///< [in] Resource type
    AddrSwizzleMode  swizzleMode,       ///< [in] Swizzle mode
    UINT_32          elemLog2,          ///< [in] element size log2
    UINT_32          numSamplesLog2,    ///< [in] number of samples
    BOOL_32          pipeAlign,         ///< [in] pipe align
    Dim3d*           pBlock             ///< [out] block size
    ) const
{
    INT_32       metablkSizeLog2;
    const INT_32 metaElemSizeLog2   = GetMetaElementSizeLog2(dataType);
    const INT_32 metaCacheSizeLog2  = GetMetaCacheSizeLog2(dataType);
    const INT_32 compBlkSizeLog2    = (dataType == Gfx10DataColor) ? 8 : 6 + numSamplesLog2 + elemLog2;
    const INT_32 metaBlkSamplesLog2 = (dataType == Gfx10DataDepthStencil) ?
                                      numSamplesLog2 : Min(numSamplesLog2, m_maxCompFragLog2);
    const INT_32 dataBlkSizeLog2    = GetBlockSizeLog2(swizzleMode);
    INT_32       numPipesLog2       = m_pipesLog2;

    if (IsThin(resourceType, swizzleMode))
    {
        if ((pipeAlign == FALSE) ||
            (IsStandardSwizzle(resourceType, swizzleMode) == TRUE) ||
            (IsDisplaySwizzle(resourceType, swizzleMode)  == TRUE))
        {
            if (pipeAlign)
            {
                metablkSizeLog2 = Max(static_cast<INT_32>(m_pipeInterleaveLog2) + numPipesLog2, 12);
                metablkSizeLog2 = Min(metablkSizeLog2, dataBlkSizeLog2);
            }
            else
            {
                metablkSizeLog2 = Min(dataBlkSizeLog2, 12);
            }
        }
        else
        {
            if (m_settings.supportRbPlus && (m_pipesLog2 == m_numSaLog2 + 1) && (m_pipesLog2 > 1))
            {
                numPipesLog2++;
            }

            INT_32 pipeRotateLog2 = GetPipeRotateAmount(resourceType, swizzleMode);

            if (numPipesLog2 >= 4)
            {
                INT_32 overlapLog2 = GetMetaOverlapLog2(dataType, resourceType, swizzleMode, elemLog2, numSamplesLog2);

                // In 16Bpe 8xaa, we have an extra overlap bit
                if ((pipeRotateLog2 > 0)  &&
                    (elemLog2 == 4)       &&
                    (numSamplesLog2 == 3) &&
                    (IsZOrderSwizzle(swizzleMode) || (GetEffectiveNumPipes() > 3)))
                {
                    overlapLog2++;
                }

                metablkSizeLog2 = metaCacheSizeLog2 + overlapLog2 + numPipesLog2;
                metablkSizeLog2 = Max(metablkSizeLog2, static_cast<INT_32>(m_pipeInterleaveLog2) + numPipesLog2);

                if (m_settings.supportRbPlus    &&
                    IsRtOptSwizzle(swizzleMode) &&
                    (numPipesLog2 == 6)         &&
                    (numSamplesLog2 == 3)       &&
                    (m_maxCompFragLog2 == 3)    &&
                    (metablkSizeLog2 < 15))
                {
                    metablkSizeLog2 = 15;
                }
            }
            else
            {
                metablkSizeLog2 = Max(static_cast<INT_32>(m_pipeInterleaveLog2) + numPipesLog2, 12);
            }

            if (dataType == Gfx10DataDepthStencil)
            {
                // For htile surfaces, pad meta block size to 2K * num_pipes
                metablkSizeLog2 = Max(metablkSizeLog2, 11 + numPipesLog2);
            }

            const INT_32 compFragLog2 = Min(m_maxCompFragLog2, numSamplesLog2);

            if  (IsRtOptSwizzle(swizzleMode) && (compFragLog2 > 1) && (pipeRotateLog2 >= 1))
            {
                const INT_32 tmp = 8 + m_pipesLog2 + Max(pipeRotateLog2, compFragLog2 - 1);

                metablkSizeLog2 = Max(metablkSizeLog2, tmp);
            }
        }

        const INT_32 metablkBitsLog2 =
            metablkSizeLog2 + compBlkSizeLog2 - elemLog2 - metaBlkSamplesLog2 - metaElemSizeLog2;
        pBlock->w = 1 << ((metablkBitsLog2 >> 1) + (metablkBitsLog2 & 1));
        pBlock->h = 1 << (metablkBitsLog2 >> 1);
        pBlock->d = 1;
    }
    else
    {
        ADDR_ASSERT(IsThick(resourceType, swizzleMode));

        if (pipeAlign)
        {
            if (m_settings.supportRbPlus         &&
                (m_pipesLog2 == m_numSaLog2 + 1) &&
                (m_pipesLog2 > 1)                &&
                IsRbAligned(resourceType, swizzleMode))
            {
                numPipesLog2++;
            }

            const INT_32 overlapLog2 = Get3DMetaOverlapLog2(resourceType, swizzleMode, elemLog2);

            metablkSizeLog2 = metaCacheSizeLog2 + overlapLog2 + numPipesLog2;
            metablkSizeLog2 = Max(metablkSizeLog2, static_cast<INT_32>(m_pipeInterleaveLog2) + numPipesLog2);
            metablkSizeLog2 = Max(metablkSizeLog2, 12);
        }
        else
        {
            metablkSizeLog2 = 12;
        }

        const INT_32 metablkBitsLog2 =
            metablkSizeLog2 + compBlkSizeLog2 - elemLog2 - metaBlkSamplesLog2 - metaElemSizeLog2;
        pBlock->w = 1 << ((metablkBitsLog2 / 3) + (((metablkBitsLog2 % 3) > 0) ? 1 : 0));
        pBlock->h = 1 << ((metablkBitsLog2 / 3) + (((metablkBitsLog2 % 3) > 1) ? 1 : 0));
        pBlock->d = 1 << (metablkBitsLog2 / 3);
    }

    return (1 << static_cast<UINT_32>(metablkSizeLog2));
}

/**
************************************************************************************************************************
*   Gfx10Lib::ConvertSwizzlePatternToEquation
*
*   @brief
*       Convert swizzle pattern to equation.
*
*   @return
*       N/A
************************************************************************************************************************
*/
VOID Gfx10Lib::ConvertSwizzlePatternToEquation(
    UINT_32          elemLog2,  ///< [in] element bytes log2
    AddrResourceType rsrcType,  ///< [in] resource type
    AddrSwizzleMode  swMode,    ///< [in] swizzle mode
    const UINT_64*   pPattern,  ///< [in] swizzle pattern
    ADDR_EQUATION*   pEquation) ///< [out] equation converted from swizzle pattern
    const
{
    const ADDR_BIT_SETTING* pSwizzle      = reinterpret_cast<const ADDR_BIT_SETTING*>(pPattern);
    const UINT_32           blockSizeLog2 = GetBlockSizeLog2(swMode);

    pEquation->numBits            = blockSizeLog2;
    pEquation->stackedDepthSlices = FALSE;

    for (UINT_32 i = 0; i < elemLog2; i++)
    {
        pEquation->addr[i].channel = 0;
        pEquation->addr[i].valid   = 1;
        pEquation->addr[i].index   = i;
    }

    if (IsXor(swMode) == FALSE)
    {
        for (UINT_32 i = elemLog2; i < blockSizeLog2; i++)
        {
            ADDR_ASSERT(IsPow2(pSwizzle[i].value));

            if (pSwizzle[i].x != 0)
            {
                ADDR_ASSERT(IsPow2(static_cast<UINT_32>(pSwizzle[i].x)));

                pEquation->addr[i].channel = 0;
                pEquation->addr[i].valid   = 1;
                pEquation->addr[i].index   = Log2(pSwizzle[i].x) + elemLog2;
            }
            else if (pSwizzle[i].y != 0)
            {
                ADDR_ASSERT(IsPow2(static_cast<UINT_32>(pSwizzle[i].y)));

                pEquation->addr[i].channel = 1;
                pEquation->addr[i].valid   = 1;
                pEquation->addr[i].index   = Log2(pSwizzle[i].y);
            }
            else
            {
                ADDR_ASSERT(pSwizzle[i].z != 0);
                ADDR_ASSERT(IsPow2(static_cast<UINT_32>(pSwizzle[i].z)));

                pEquation->addr[i].channel = 2;
                pEquation->addr[i].valid   = 1;
                pEquation->addr[i].index   = Log2(pSwizzle[i].z);
            }

            pEquation->xor1[i].value = 0;
            pEquation->xor2[i].value = 0;
        }
    }
    else if (IsThin(rsrcType, swMode))
    {
        const UINT_32 blkXLog2 = (blockSizeLog2 == 12) ? Block4K_Log2_2d[elemLog2].w : Block64K_Log2_2d[elemLog2].w;
        const UINT_32 blkYLog2 = (blockSizeLog2 == 12) ? Block4K_Log2_2d[elemLog2].h : Block64K_Log2_2d[elemLog2].h;
        const UINT_32 blkXMask = (1 << blkXLog2) - 1;
        const UINT_32 blkYMask = (1 << blkYLog2) - 1;

        ADDR_BIT_SETTING swizzle[ADDR_MAX_EQUATION_BIT];
        UINT_32          xMask = 0;
        UINT_32          yMask = 0;
        UINT_32          bMask = (1 << elemLog2) - 1;

        for (UINT_32 i = elemLog2; i < blockSizeLog2; i++)
        {
            if (IsPow2(pSwizzle[i].value))
            {
                if (pSwizzle[i].x != 0)
                {
                    ADDR_ASSERT((xMask & pSwizzle[i].x) == 0);
                    xMask |= pSwizzle[i].x;

                    const UINT_32 xLog2 = Log2(pSwizzle[i].x);

                    ADDR_ASSERT(xLog2 < blkXLog2);

                    pEquation->addr[i].channel = 0;
                    pEquation->addr[i].valid   = 1;
                    pEquation->addr[i].index   = xLog2 + elemLog2;
                }
                else
                {
                    ADDR_ASSERT(pSwizzle[i].y != 0);
                    ADDR_ASSERT((yMask & pSwizzle[i].y) == 0);
                    yMask |= pSwizzle[i].y;

                    pEquation->addr[i].channel = 1;
                    pEquation->addr[i].valid   = 1;
                    pEquation->addr[i].index   = Log2(pSwizzle[i].y);

                    ADDR_ASSERT(pEquation->addr[i].index < blkYLog2);
                }

                swizzle[i].value = 0;
                bMask |= 1 << i;
            }
            else
            {
                if (pSwizzle[i].z != 0)
                {
                    ADDR_ASSERT(IsPow2(static_cast<UINT_32>(pSwizzle[i].z)));

                    pEquation->xor2[i].channel = 2;
                    pEquation->xor2[i].valid   = 1;
                    pEquation->xor2[i].index   = Log2(pSwizzle[i].z);
                }

                swizzle[i].x = pSwizzle[i].x;
                swizzle[i].y = pSwizzle[i].y;
                swizzle[i].z = swizzle[i].s = 0;

                ADDR_ASSERT(IsPow2(swizzle[i].value) == FALSE);

                const UINT_32 xHi = swizzle[i].x & (~blkXMask);

                if (xHi != 0)
                {
                    ADDR_ASSERT(IsPow2(xHi));
                    ADDR_ASSERT(pEquation->xor1[i].value == 0);

                    pEquation->xor1[i].channel = 0;
                    pEquation->xor1[i].valid   = 1;
                    pEquation->xor1[i].index   = Log2(xHi) + elemLog2;

                    swizzle[i].x &= blkXMask;
                }

                const UINT_32 yHi = swizzle[i].y & (~blkYMask);

                if (yHi != 0)
                {
                    ADDR_ASSERT(IsPow2(yHi));

                    if (xHi == 0)
                    {
                        ADDR_ASSERT(pEquation->xor1[i].value == 0);
                        pEquation->xor1[i].channel = 1;
                        pEquation->xor1[i].valid   = 1;
                        pEquation->xor1[i].index   = Log2(yHi);
                    }
                    else
                    {
                        ADDR_ASSERT(pEquation->xor2[i].value == 0);
                        pEquation->xor2[i].channel = 1;
                        pEquation->xor2[i].valid   = 1;
                        pEquation->xor2[i].index   = Log2(yHi);
                    }

                    swizzle[i].y &= blkYMask;
                }

                if (swizzle[i].value == 0)
                {
                    bMask |= 1 << i;
                }
            }
        }

        const UINT_32 pipeIntMask = (1 << m_pipeInterleaveLog2) - 1;
        const UINT_32 blockMask   = (1 << blockSizeLog2) - 1;

        ADDR_ASSERT((bMask & pipeIntMask) == pipeIntMask);

        while (bMask != blockMask)
        {
            for (UINT_32 i = m_pipeInterleaveLog2; i < blockSizeLog2; i++)
            {
                if ((bMask & (1 << i)) == 0)
                {
                    if (IsPow2(swizzle[i].value))
                    {
                        if (swizzle[i].x != 0)
                        {
                            ADDR_ASSERT((xMask & swizzle[i].x) == 0);
                            xMask |= swizzle[i].x;

                            const UINT_32 xLog2 = Log2(swizzle[i].x);

                            ADDR_ASSERT(xLog2 < blkXLog2);

                            pEquation->addr[i].channel = 0;
                            pEquation->addr[i].valid   = 1;
                            pEquation->addr[i].index   = xLog2 + elemLog2;
                        }
                        else
                        {
                            ADDR_ASSERT(swizzle[i].y != 0);
                            ADDR_ASSERT((yMask & swizzle[i].y) == 0);
                            yMask |= swizzle[i].y;

                            pEquation->addr[i].channel = 1;
                            pEquation->addr[i].valid   = 1;
                            pEquation->addr[i].index   = Log2(swizzle[i].y);

                            ADDR_ASSERT(pEquation->addr[i].index < blkYLog2);
                        }

                        swizzle[i].value = 0;
                        bMask |= 1 << i;
                    }
                    else
                    {
                        const UINT_32 x = swizzle[i].x & xMask;
                        const UINT_32 y = swizzle[i].y & yMask;

                        if (x != 0)
                        {
                            ADDR_ASSERT(IsPow2(x));

                            if (pEquation->xor1[i].value == 0)
                            {
                                pEquation->xor1[i].channel = 0;
                                pEquation->xor1[i].valid   = 1;
                                pEquation->xor1[i].index   = Log2(x) + elemLog2;
                            }
                            else
                            {
                                ADDR_ASSERT(pEquation->xor2[i].value == 0);
                                pEquation->xor2[i].channel = 0;
                                pEquation->xor2[i].valid   = 1;
                                pEquation->xor2[i].index   = Log2(x) + elemLog2;
                            }
                        }

                        if (y != 0)
                        {
                            ADDR_ASSERT(IsPow2(y));

                            if (pEquation->xor1[i].value == 0)
                            {
                                pEquation->xor1[i].channel = 1;
                                pEquation->xor1[i].valid   = 1;
                                pEquation->xor1[i].index   = Log2(y);
                            }
                            else
                            {
                                ADDR_ASSERT(pEquation->xor2[i].value == 0);
                                pEquation->xor2[i].channel = 1;
                                pEquation->xor2[i].valid   = 1;
                                pEquation->xor2[i].index   = Log2(y);
                            }
                        }

                        swizzle[i].x &= ~x;
                        swizzle[i].y &= ~y;
                    }
                }
            }
        }

        ADDR_ASSERT((xMask == blkXMask) && (yMask == blkYMask));
    }
    else if (IsEquationCompatibleThick(rsrcType, swMode))
    {
        const UINT_32 blkXLog2 = (blockSizeLog2 == 12) ? Block4K_Log2_3d[elemLog2].w : Block64K_Log2_3d[elemLog2].w;
        const UINT_32 blkYLog2 = (blockSizeLog2 == 12) ? Block4K_Log2_3d[elemLog2].h : Block64K_Log2_3d[elemLog2].h;
        const UINT_32 blkZLog2 = (blockSizeLog2 == 12) ? Block4K_Log2_3d[elemLog2].d : Block64K_Log2_3d[elemLog2].d;
        const UINT_32 blkXMask = (1 << blkXLog2) - 1;
        const UINT_32 blkYMask = (1 << blkYLog2) - 1;
        const UINT_32 blkZMask = (1 << blkZLog2) - 1;

        ADDR_BIT_SETTING swizzle[ADDR_MAX_EQUATION_BIT];
        UINT_32          xMask = 0;
        UINT_32          yMask = 0;
        UINT_32          zMask = 0;
        UINT_32          bMask = (1 << elemLog2) - 1;

        for (UINT_32 i = elemLog2; i < blockSizeLog2; i++)
        {
            if (IsPow2(pSwizzle[i].value))
            {
                if (pSwizzle[i].x != 0)
                {
                    ADDR_ASSERT((xMask & pSwizzle[i].x) == 0);
                    xMask |= pSwizzle[i].x;

                    const UINT_32 xLog2 = Log2(pSwizzle[i].x);

                    ADDR_ASSERT(xLog2 < blkXLog2);

                    pEquation->addr[i].channel = 0;
                    pEquation->addr[i].valid   = 1;
                    pEquation->addr[i].index   = xLog2 + elemLog2;
                }
                else if (pSwizzle[i].y != 0)
                {
                    ADDR_ASSERT((yMask & pSwizzle[i].y) == 0);
                    yMask |= pSwizzle[i].y;

                    pEquation->addr[i].channel = 1;
                    pEquation->addr[i].valid   = 1;
                    pEquation->addr[i].index   = Log2(pSwizzle[i].y);

                    ADDR_ASSERT(pEquation->addr[i].index < blkYLog2);
                }
                else
                {
                    ADDR_ASSERT(pSwizzle[i].z != 0);
                    ADDR_ASSERT((zMask & pSwizzle[i].z) == 0);
                    zMask |= pSwizzle[i].z;

                    pEquation->addr[i].channel = 2;
                    pEquation->addr[i].valid   = 1;
                    pEquation->addr[i].index   = Log2(pSwizzle[i].z);

                    ADDR_ASSERT(pEquation->addr[i].index < blkZLog2);
                }

                swizzle[i].value = 0;
                bMask |= 1 << i;
            }
            else
            {
                swizzle[i].x = pSwizzle[i].x;
                swizzle[i].y = pSwizzle[i].y;
                swizzle[i].z = pSwizzle[i].z;
                swizzle[i].s = 0;

                ADDR_ASSERT(IsPow2(swizzle[i].value) == FALSE);

                const UINT_32 xHi = swizzle[i].x & (~blkXMask);
                const UINT_32 yHi = swizzle[i].y & (~blkYMask);
                const UINT_32 zHi = swizzle[i].z & (~blkZMask);

                ADDR_ASSERT((xHi == 0) || (yHi== 0) || (zHi == 0));

                if (xHi != 0)
                {
                    ADDR_ASSERT(IsPow2(xHi));
                    ADDR_ASSERT(pEquation->xor1[i].value == 0);

                    pEquation->xor1[i].channel = 0;
                    pEquation->xor1[i].valid   = 1;
                    pEquation->xor1[i].index   = Log2(xHi) + elemLog2;

                    swizzle[i].x &= blkXMask;
                }

                if (yHi != 0)
                {
                    ADDR_ASSERT(IsPow2(yHi));

                    if (pEquation->xor1[i].value == 0)
                    {
                        pEquation->xor1[i].channel = 1;
                        pEquation->xor1[i].valid   = 1;
                        pEquation->xor1[i].index   = Log2(yHi);
                    }
                    else
                    {
                        ADDR_ASSERT(pEquation->xor2[i].value == 0);
                        pEquation->xor2[i].channel = 1;
                        pEquation->xor2[i].valid   = 1;
                        pEquation->xor2[i].index   = Log2(yHi);
                    }

                    swizzle[i].y &= blkYMask;
                }

                if (zHi != 0)
                {
                    ADDR_ASSERT(IsPow2(zHi));

                    if (pEquation->xor1[i].value == 0)
                    {
                        pEquation->xor1[i].channel = 2;
                        pEquation->xor1[i].valid   = 1;
                        pEquation->xor1[i].index   = Log2(zHi);
                    }
                    else
                    {
                        ADDR_ASSERT(pEquation->xor2[i].value == 0);
                        pEquation->xor2[i].channel = 2;
                        pEquation->xor2[i].valid   = 1;
                        pEquation->xor2[i].index   = Log2(zHi);
                    }

                    swizzle[i].z &= blkZMask;
                }

                if (swizzle[i].value == 0)
                {
                    bMask |= 1 << i;
                }
            }
        }

        const UINT_32 pipeIntMask = (1 << m_pipeInterleaveLog2) - 1;
        const UINT_32 blockMask   = (1 << blockSizeLog2) - 1;

        ADDR_ASSERT((bMask & pipeIntMask) == pipeIntMask);

        while (bMask != blockMask)
        {
            for (UINT_32 i = m_pipeInterleaveLog2; i < blockSizeLog2; i++)
            {
                if ((bMask & (1 << i)) == 0)
                {
                    if (IsPow2(swizzle[i].value))
                    {
                        if (swizzle[i].x != 0)
                        {
                            ADDR_ASSERT((xMask & swizzle[i].x) == 0);
                            xMask |= swizzle[i].x;

                            const UINT_32 xLog2 = Log2(swizzle[i].x);

                            ADDR_ASSERT(xLog2 < blkXLog2);

                            pEquation->addr[i].channel = 0;
                            pEquation->addr[i].valid   = 1;
                            pEquation->addr[i].index   = xLog2 + elemLog2;
                        }
                        else if (swizzle[i].y != 0)
                        {
                            ADDR_ASSERT((yMask & swizzle[i].y) == 0);
                            yMask |= swizzle[i].y;

                            pEquation->addr[i].channel = 1;
                            pEquation->addr[i].valid   = 1;
                            pEquation->addr[i].index   = Log2(swizzle[i].y);

                            ADDR_ASSERT(pEquation->addr[i].index < blkYLog2);
                        }
                        else
                        {
                            ADDR_ASSERT(swizzle[i].z != 0);
                            ADDR_ASSERT((zMask & swizzle[i].z) == 0);
                            zMask |= swizzle[i].z;

                            pEquation->addr[i].channel = 2;
                            pEquation->addr[i].valid   = 1;
                            pEquation->addr[i].index   = Log2(swizzle[i].z);

                            ADDR_ASSERT(pEquation->addr[i].index < blkZLog2);
                        }

                        swizzle[i].value = 0;
                        bMask |= 1 << i;
                    }
                    else
                    {
                        const UINT_32 x = swizzle[i].x & xMask;
                        const UINT_32 y = swizzle[i].y & yMask;
                        const UINT_32 z = swizzle[i].z & zMask;

                        if (x != 0)
                        {
                            ADDR_ASSERT(IsPow2(x));

                            if (pEquation->xor1[i].value == 0)
                            {
                                pEquation->xor1[i].channel = 0;
                                pEquation->xor1[i].valid   = 1;
                                pEquation->xor1[i].index   = Log2(x) + elemLog2;
                            }
                            else
                            {
                                ADDR_ASSERT(pEquation->xor2[i].value == 0);
                                pEquation->xor2[i].channel = 0;
                                pEquation->xor2[i].valid   = 1;
                                pEquation->xor2[i].index   = Log2(x) + elemLog2;
                            }
                        }

                        if (y != 0)
                        {
                            ADDR_ASSERT(IsPow2(y));

                            if (pEquation->xor1[i].value == 0)
                            {
                                pEquation->xor1[i].channel = 1;
                                pEquation->xor1[i].valid   = 1;
                                pEquation->xor1[i].index   = Log2(y);
                            }
                            else
                            {
                                ADDR_ASSERT(pEquation->xor2[i].value == 0);
                                pEquation->xor2[i].channel = 1;
                                pEquation->xor2[i].valid   = 1;
                                pEquation->xor2[i].index   = Log2(y);
                            }
                        }

                        if (z != 0)
                        {
                            ADDR_ASSERT(IsPow2(z));

                            if (pEquation->xor1[i].value == 0)
                            {
                                pEquation->xor1[i].channel = 2;
                                pEquation->xor1[i].valid   = 1;
                                pEquation->xor1[i].index   = Log2(z);
                            }
                            else
                            {
                                ADDR_ASSERT(pEquation->xor2[i].value == 0);
                                pEquation->xor2[i].channel = 2;
                                pEquation->xor2[i].valid   = 1;
                                pEquation->xor2[i].index   = Log2(z);
                            }
                        }

                        swizzle[i].x &= ~x;
                        swizzle[i].y &= ~y;
                        swizzle[i].z &= ~z;
                    }
                }
            }
        }

        ADDR_ASSERT((xMask == blkXMask) && (yMask == blkYMask) && (zMask == blkZMask));
    }
}

/**
************************************************************************************************************************
*   Gfx10Lib::InitEquationTable
*
*   @brief
*       Initialize Equation table.
*
*   @return
*       N/A
************************************************************************************************************************
*/
VOID Gfx10Lib::InitEquationTable()
{
    memset(m_equationTable, 0, sizeof(m_equationTable));

    for (UINT_32 rsrcTypeIdx = 0; rsrcTypeIdx < MaxRsrcType; rsrcTypeIdx++)
    {
        const AddrResourceType rsrcType = static_cast<AddrResourceType>(rsrcTypeIdx + ADDR_RSRC_TEX_2D);

        for (UINT_32 swModeIdx = 0; swModeIdx < MaxSwMode; swModeIdx++)
        {
            const AddrSwizzleMode swMode = static_cast<AddrSwizzleMode>(swModeIdx);

            for (UINT_32 elemLog2 = 0; elemLog2 < MaxElementBytesLog2; elemLog2++)
            {
                UINT_32        equationIndex = ADDR_INVALID_EQUATION_INDEX;
                const UINT_64* pPattern      = GetSwizzlePattern(swMode, rsrcType, elemLog2, 1);

                if (pPattern != NULL)
                {
                    ADDR_EQUATION equation = {};

                    ConvertSwizzlePatternToEquation(elemLog2, rsrcType, swMode, pPattern, &equation);

                    equationIndex = m_numEquations;
                    ADDR_ASSERT(equationIndex < EquationTableSize);

                    m_equationTable[equationIndex] = equation;

                    m_numEquations++;
                }

                m_equationLookupTable[rsrcTypeIdx][swModeIdx][elemLog2] = equationIndex;
            }
        }
    }
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlGetEquationIndex
*
*   @brief
*       Interface function stub of GetEquationIndex
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
UINT_32 Gfx10Lib::HwlGetEquationIndex(
    const ADDR2_COMPUTE_SURFACE_INFO_INPUT* pIn,    ///< [in] input structure
    ADDR2_COMPUTE_SURFACE_INFO_OUTPUT*      pOut    ///< [out] output structure
    ) const
{
    UINT_32 equationIdx = ADDR_INVALID_EQUATION_INDEX;

    if ((pIn->resourceType == ADDR_RSRC_TEX_2D) ||
        (pIn->resourceType == ADDR_RSRC_TEX_3D))
    {
        const UINT_32 rsrcTypeIdx = static_cast<UINT_32>(pIn->resourceType) - 1;
        const UINT_32 swModeIdx   = static_cast<UINT_32>(pIn->swizzleMode);
        const UINT_32 elemLog2    = Log2(pIn->bpp >> 3);

        equationIdx = m_equationLookupTable[rsrcTypeIdx][swModeIdx][elemLog2];
    }

    if (pOut->pMipInfo != NULL)
    {
        for (UINT_32 i = 0; i < pIn->numMipLevels; i++)
        {
            pOut->pMipInfo[i].equationIndex = equationIdx;
        }
    }

    return equationIdx;
}

/**
************************************************************************************************************************
*   Gfx10Lib::IsValidDisplaySwizzleMode
*
*   @brief
*       Check if a swizzle mode is supported by display engine
*
*   @return
*       TRUE is swizzle mode is supported by display engine
************************************************************************************************************************
*/
BOOL_32 Gfx10Lib::IsValidDisplaySwizzleMode(
    const ADDR2_COMPUTE_SURFACE_INFO_INPUT* pIn     ///< [in] input structure
    ) const
{
    ADDR_ASSERT(pIn->resourceType == ADDR_RSRC_TEX_2D);

    BOOL_32 support = FALSE;

    if (m_settings.isDcn2)
    {
        switch (pIn->swizzleMode)
        {
            case ADDR_SW_4KB_D:
            case ADDR_SW_4KB_D_X:
            case ADDR_SW_64KB_D:
            case ADDR_SW_64KB_D_T:
            case ADDR_SW_64KB_D_X:
                support = (pIn->bpp == 64);
                break;

            case ADDR_SW_LINEAR:
            case ADDR_SW_4KB_S:
            case ADDR_SW_4KB_S_X:
            case ADDR_SW_64KB_S:
            case ADDR_SW_64KB_S_T:
            case ADDR_SW_64KB_S_X:
            case ADDR_SW_64KB_R_X:
                support = (pIn->bpp <= 64);
                break;

            default:
                break;
        }
    }
    else
    {
        ADDR_NOT_IMPLEMENTED();
    }

    return support;
}

/**
************************************************************************************************************************
*   Gfx10Lib::GetMaxNumMipsInTail
*
*   @brief
*       Return max number of mips in tails
*
*   @return
*       Max number of mips in tails
************************************************************************************************************************
*/
UINT_32 Gfx10Lib::GetMaxNumMipsInTail(
    UINT_32 blockSizeLog2,     ///< block size log2
    BOOL_32 isThin             ///< is thin or thick
    ) const
{
    UINT_32 effectiveLog2 = blockSizeLog2;

    if (isThin == FALSE)
    {
        effectiveLog2 -= (blockSizeLog2 - 8) / 3;
    }

    return (effectiveLog2 <= 11) ? (1 + (1 << (effectiveLog2 - 9))) : (effectiveLog2 - 4);
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlComputePipeBankXor
*
*   @brief
*       Generate a PipeBankXor value to be ORed into bits above pipeInterleaveBits of address
*
*   @return
*       PipeBankXor value
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::HwlComputePipeBankXor(
    const ADDR2_COMPUTE_PIPEBANKXOR_INPUT* pIn,     ///< [in] input structure
    ADDR2_COMPUTE_PIPEBANKXOR_OUTPUT*      pOut     ///< [out] output structure
    ) const
{
    if (IsNonPrtXor(pIn->swizzleMode))
    {
        const UINT_32 blockBits  = GetBlockSizeLog2(pIn->swizzleMode);
        const UINT_32 pipeBits   = GetPipeXorBits(blockBits);
        const UINT_32 bankBits   = GetBankXorBits(blockBits);

        UINT_32 pipeXor = 0;
        UINT_32 bankXor = 0;

        if (bankBits != 0)
        {
            if (blockBits == 16)
            {
                const UINT_32        XorPatternLen = 8;
                static const UINT_32 XorBank1b[XorPatternLen] = {0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80};
                static const UINT_32 XorBank2b[XorPatternLen] = {0x00, 0x80, 0x40, 0xC0, 0x80, 0x00, 0xC0, 0x40};
                static const UINT_32 XorBank3b[XorPatternLen] = {0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0};

                const UINT_32 index = pIn->surfIndex % XorPatternLen;

                if (bankBits == 1)
                {
                    bankXor = XorBank1b[index];
                }
                else if (bankBits == 2)
                {
                    bankXor = XorBank2b[index];
                }
                else
                {
                    bankXor = XorBank3b[index];

                    if (bankBits == 4)
                    {
                        bankXor >>= (2 - pipeBits);
                    }
                }
            }
        }

        pOut->pipeBankXor = bankXor | pipeXor;
    }
    else
    {
        pOut->pipeBankXor = 0;
    }

    return ADDR_OK;
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlComputeSlicePipeBankXor
*
*   @brief
*       Generate slice PipeBankXor value based on base PipeBankXor value and slice id
*
*   @return
*       PipeBankXor value
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::HwlComputeSlicePipeBankXor(
    const ADDR2_COMPUTE_SLICE_PIPEBANKXOR_INPUT* pIn,   ///< [in] input structure
    ADDR2_COMPUTE_SLICE_PIPEBANKXOR_OUTPUT*      pOut   ///< [out] output structure
    ) const
{
    if (IsNonPrtXor(pIn->swizzleMode))
    {
        const UINT_32 blockBits = GetBlockSizeLog2(pIn->swizzleMode);
        const UINT_32 pipeBits  = GetPipeXorBits(blockBits);
        const UINT_32 pipeXor   = ReverseBitVector(pIn->slice, pipeBits);

        pOut->pipeBankXor = pIn->basePipeBankXor ^ pipeXor;
    }
    else
    {
        pOut->pipeBankXor = 0;
    }

    return ADDR_OK;
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlComputeSubResourceOffsetForSwizzlePattern
*
*   @brief
*       Compute sub resource offset to support swizzle pattern
*
*   @return
*       Offset
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::HwlComputeSubResourceOffsetForSwizzlePattern(
    const ADDR2_COMPUTE_SUBRESOURCE_OFFSET_FORSWIZZLEPATTERN_INPUT* pIn,    ///< [in] input structure
    ADDR2_COMPUTE_SUBRESOURCE_OFFSET_FORSWIZZLEPATTERN_OUTPUT*      pOut    ///< [out] output structure
    ) const
{
    ADDR_ASSERT(IsThin(pIn->resourceType, pIn->swizzleMode));

    pOut->offset = pIn->slice * pIn->sliceSize + pIn->macroBlockOffset;

    return ADDR_OK;
}

/**
************************************************************************************************************************
*   Gfx10Lib::ValidateNonSwModeParams
*
*   @brief
*       Validate compute surface info params except swizzle mode
*
*   @return
*       TRUE if parameters are valid, FALSE otherwise
************************************************************************************************************************
*/
BOOL_32 Gfx10Lib::ValidateNonSwModeParams(
    const ADDR2_COMPUTE_SURFACE_INFO_INPUT* pIn) const
{
    BOOL_32 valid = TRUE;

    if ((pIn->bpp == 0) || (pIn->bpp > 128) || (pIn->width == 0) || (pIn->numFrags > 8) || (pIn->numSamples > 16))
    {
        ADDR_ASSERT_ALWAYS();
        valid = FALSE;
    }

    if (pIn->resourceType >= ADDR_RSRC_MAX_TYPE)
    {
        ADDR_ASSERT_ALWAYS();
        valid = FALSE;
    }

    const ADDR2_SURFACE_FLAGS flags    = pIn->flags;
    const AddrResourceType    rsrcType = pIn->resourceType;
    const BOOL_32             mipmap   = (pIn->numMipLevels > 1);
    const BOOL_32             msaa     = (pIn->numFrags > 1);
    const BOOL_32             display  = flags.display;
    const BOOL_32             tex3d    = IsTex3d(rsrcType);
    const BOOL_32             tex2d    = IsTex2d(rsrcType);
    const BOOL_32             tex1d    = IsTex1d(rsrcType);
    const BOOL_32             stereo   = flags.qbStereo;

    // Resource type check
    if (tex1d)
    {
        if (msaa || display || stereo)
        {
            ADDR_ASSERT_ALWAYS();
            valid = FALSE;
        }
    }
    else if (tex2d)
    {
        if ((msaa && mipmap) || (stereo && msaa) || (stereo && mipmap))
        {
            ADDR_ASSERT_ALWAYS();
            valid = FALSE;
        }
    }
    else if (tex3d)
    {
        if (msaa || display || stereo)
        {
            ADDR_ASSERT_ALWAYS();
            valid = FALSE;
        }
    }
    else
    {
        ADDR_ASSERT_ALWAYS();
        valid = FALSE;
    }

    return valid;
}

/**
************************************************************************************************************************
*   Gfx10Lib::ValidateSwModeParams
*
*   @brief
*       Validate compute surface info related to swizzle mode
*
*   @return
*       TRUE if parameters are valid, FALSE otherwise
************************************************************************************************************************
*/
BOOL_32 Gfx10Lib::ValidateSwModeParams(
    const ADDR2_COMPUTE_SURFACE_INFO_INPUT* pIn) const
{
    BOOL_32 valid = TRUE;

    if (pIn->swizzleMode >= ADDR_SW_MAX_TYPE)
    {
        ADDR_ASSERT_ALWAYS();
        valid = FALSE;
    }

    const ADDR2_SURFACE_FLAGS flags       = pIn->flags;
    const AddrResourceType    rsrcType    = pIn->resourceType;
    const AddrSwizzleMode     swizzle     = pIn->swizzleMode;
    const BOOL_32             msaa        = (pIn->numFrags > 1);
    const BOOL_32             zbuffer     = flags.depth || flags.stencil;
    const BOOL_32             color       = flags.color;
    const BOOL_32             display     = flags.display;
    const BOOL_32             tex3d       = IsTex3d(rsrcType);
    const BOOL_32             tex2d       = IsTex2d(rsrcType);
    const BOOL_32             tex1d       = IsTex1d(rsrcType);
    const BOOL_32             thin3d      = flags.view3dAs2dArray;
    const BOOL_32             linear      = IsLinear(swizzle);
    const BOOL_32             blk256B     = IsBlock256b(swizzle);
    const BOOL_32             isNonPrtXor = IsNonPrtXor(swizzle);
    const BOOL_32             prt         = flags.prt;

    // Misc check
    if ((pIn->numFrags > 1) &&
        (GetBlockSize(swizzle) < (m_pipeInterleaveBytes * pIn->numFrags)))
    {
        // MSAA surface must have blk_bytes/pipe_interleave >= num_samples
        ADDR_ASSERT_ALWAYS();
        valid = FALSE;
    }

    if (display && (IsValidDisplaySwizzleMode(pIn) == FALSE))
    {
        ADDR_ASSERT_ALWAYS();
        valid = FALSE;
    }

    if ((pIn->bpp == 96) && (linear == FALSE))
    {
        ADDR_ASSERT_ALWAYS();
        valid = FALSE;
    }

    const UINT_32 swizzleMask = 1 << swizzle;

    // Resource type check
    if (tex1d)
    {
        if ((swizzleMask & Gfx10Rsrc1dSwModeMask) == 0)
        {
            ADDR_ASSERT_ALWAYS();
            valid = FALSE;
        }
    }
    else if (tex2d)
    {
        if (((swizzleMask & Gfx10Rsrc2dSwModeMask) == 0) ||
            (prt && ((swizzleMask & Gfx10Rsrc2dPrtSwModeMask) == 0)))
        {
            ADDR_ASSERT_ALWAYS();
            valid = FALSE;
        }

    }
    else if (tex3d)
    {
        if (((swizzleMask & Gfx10Rsrc3dSwModeMask) == 0) ||
            (prt && ((swizzleMask & Gfx10Rsrc3dPrtSwModeMask) == 0)) ||
            (thin3d && ((swizzleMask & Gfx10Rsrc3dThinSwModeMask) == 0)))
        {
            ADDR_ASSERT_ALWAYS();
            valid = FALSE;
        }
    }

    // Swizzle type check
    if (linear)
    {
        if (zbuffer || msaa || (pIn->bpp == 0) || ((pIn->bpp % 8) != 0))
        {
            ADDR_ASSERT_ALWAYS();
            valid = FALSE;
        }
    }
    else if (IsZOrderSwizzle(swizzle))
    {
        if ((pIn->bpp > 64)                         ||
            (msaa && (color || (pIn->bpp > 32)))    ||
            ElemLib::IsBlockCompressed(pIn->format) ||
            ElemLib::IsMacroPixelPacked(pIn->format))
        {
            ADDR_ASSERT_ALWAYS();
            valid = FALSE;
        }
    }
    else if (IsStandardSwizzle(rsrcType, swizzle))
    {
        if (zbuffer || msaa)
        {
            ADDR_ASSERT_ALWAYS();
            valid = FALSE;
        }
    }
    else if (IsDisplaySwizzle(rsrcType, swizzle))
    {
        if (zbuffer || msaa)
        {
            ADDR_ASSERT_ALWAYS();
            valid = FALSE;
        }
    }
    else if (IsRtOptSwizzle(swizzle))
    {
        if (zbuffer)
        {
            ADDR_ASSERT_ALWAYS();
            valid = FALSE;
        }
    }
    else
    {
        ADDR_ASSERT_ALWAYS();
        valid = FALSE;
    }

    // Block type check
    if (blk256B)
    {
        if (zbuffer || tex3d || msaa)
        {
            ADDR_ASSERT_ALWAYS();
            valid = FALSE;
        }
    }

    return valid;
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlComputeSurfaceInfoSanityCheck
*
*   @brief
*       Compute surface info sanity check
*
*   @return
*       Offset
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::HwlComputeSurfaceInfoSanityCheck(
    const ADDR2_COMPUTE_SURFACE_INFO_INPUT* pIn     ///< [in] input structure
    ) const
{
    return ValidateNonSwModeParams(pIn) && ValidateSwModeParams(pIn) ? ADDR_OK : ADDR_INVALIDPARAMS;
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlGetPreferredSurfaceSetting
*
*   @brief
*       Internal function to get suggested surface information for cliet to use
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::HwlGetPreferredSurfaceSetting(
    const ADDR2_GET_PREFERRED_SURF_SETTING_INPUT* pIn,  ///< [in] input structure
    ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT*      pOut  ///< [out] output structure
    ) const
{
    ADDR_E_RETURNCODE returnCode = ADDR_OK;

    if (pIn->flags.fmask)
    {
        pOut->swizzleMode                 = ADDR_SW_64KB_Z_X;
        pOut->resourceType                = ADDR_RSRC_TEX_2D;
        pOut->validBlockSet.value         = 0;
        pOut->validBlockSet.macroThin64KB = 1;
        pOut->canXor                      = TRUE;
        pOut->validSwTypeSet.value        = AddrSwSetZ;
        pOut->clientPreferredSwSet        = pOut->validSwTypeSet;
        pOut->validSwModeSet.value        = Gfx10ZSwModeMask;
    }
    else
    {
        UINT_32 bpp    = pIn->bpp;
        UINT_32 width  = Max(pIn->width, 1u);
        UINT_32 height = Max(pIn->height, 1u);

        // Set format to INVALID will skip this conversion
        if (pIn->format != ADDR_FMT_INVALID)
        {
            ElemMode elemMode = ADDR_UNCOMPRESSED;
            UINT_32 expandX, expandY;

            // Get compression/expansion factors and element mode which indicates compression/expansion
            bpp = GetElemLib()->GetBitsPerPixel(pIn->format,
                                                &elemMode,
                                                &expandX,
                                                &expandY);

            UINT_32 basePitch = 0;
            GetElemLib()->AdjustSurfaceInfo(elemMode,
                                            expandX,
                                            expandY,
                                            &bpp,
                                            &basePitch,
                                            &width,
                                            &height);
        }

        const UINT_32 numSlices    = Max(pIn->numSlices,    1u);
        const UINT_32 numMipLevels = Max(pIn->numMipLevels, 1u);
        const UINT_32 numSamples   = Max(pIn->numSamples,   1u);
        const UINT_32 numFrags     = (pIn->numFrags == 0) ? numSamples : pIn->numFrags;
        const BOOL_32 msaa         = (numFrags > 1) || (numSamples > 1);

        // Pre sanity check on non swizzle mode parameters
        ADDR2_COMPUTE_SURFACE_INFO_INPUT localIn = {};
        localIn.flags        = pIn->flags;
        localIn.resourceType = pIn->resourceType;
        localIn.format       = pIn->format;
        localIn.bpp          = bpp;
        localIn.width        = width;
        localIn.height       = height;
        localIn.numSlices    = numSlices;
        localIn.numMipLevels = numMipLevels;
        localIn.numSamples   = numSamples;
        localIn.numFrags     = numFrags;

        if (ValidateNonSwModeParams(&localIn))
        {
            // Forbid swizzle mode(s) by client setting
            ADDR2_SWMODE_SET allowedSwModeSet = {};
            allowedSwModeSet.value |= pIn->forbiddenBlock.linear ? 0 : Gfx10LinearSwModeMask;
            allowedSwModeSet.value |= pIn->forbiddenBlock.micro  ? 0 : Gfx10Blk256BSwModeMask;
            allowedSwModeSet.value |=
                pIn->forbiddenBlock.macroThin4KB ? 0 :
                ((pOut->resourceType == ADDR_RSRC_TEX_3D) ? 0 : Gfx10Blk4KBSwModeMask);
            allowedSwModeSet.value |=
                pIn->forbiddenBlock.macroThick4KB ? 0 :
                ((pOut->resourceType == ADDR_RSRC_TEX_3D) ? Gfx10Rsrc3dThick4KBSwModeMask : 0);
            allowedSwModeSet.value |=
                pIn->forbiddenBlock.macroThin64KB ? 0 :
                ((pOut->resourceType == ADDR_RSRC_TEX_3D) ? Gfx10Rsrc3dThinSwModeMask : Gfx10Blk64KBSwModeMask);
            allowedSwModeSet.value |=
                pIn->forbiddenBlock.macroThick64KB ? 0 :
                ((pOut->resourceType == ADDR_RSRC_TEX_3D) ? Gfx10Rsrc3dThick64KBSwModeMask : 0);

            if (pIn->preferredSwSet.value != 0)
            {
                allowedSwModeSet.value &= pIn->preferredSwSet.sw_Z ? ~0 : ~Gfx10ZSwModeMask;
                allowedSwModeSet.value &= pIn->preferredSwSet.sw_S ? ~0 : ~Gfx10StandardSwModeMask;
                allowedSwModeSet.value &= pIn->preferredSwSet.sw_D ? ~0 : ~Gfx10DisplaySwModeMask;
                allowedSwModeSet.value &= pIn->preferredSwSet.sw_R ? ~0 : ~Gfx10RenderSwModeMask;
            }

            if (pIn->noXor)
            {
                allowedSwModeSet.value &= ~Gfx10XorSwModeMask;
            }

            if (pIn->maxAlign > 0)
            {
                if (pIn->maxAlign < Size64K)
                {
                    allowedSwModeSet.value &= ~Gfx10Blk64KBSwModeMask;
                }

                if (pIn->maxAlign < Size4K)
                {
                    allowedSwModeSet.value &= ~Gfx10Blk4KBSwModeMask;
                }

                if (pIn->maxAlign < Size256)
                {
                    allowedSwModeSet.value &= ~Gfx10Blk256BSwModeMask;
                }
            }

            // Filter out invalid swizzle mode(s) by image attributes and HW restrictions
            switch (pIn->resourceType)
            {
                case ADDR_RSRC_TEX_1D:
                    allowedSwModeSet.value &= Gfx10Rsrc1dSwModeMask;
                    break;

                case ADDR_RSRC_TEX_2D:
                    allowedSwModeSet.value &= pIn->flags.prt ? Gfx10Rsrc2dPrtSwModeMask : Gfx10Rsrc2dSwModeMask;

                    break;

                case ADDR_RSRC_TEX_3D:
                    allowedSwModeSet.value &= pIn->flags.prt ? Gfx10Rsrc3dPrtSwModeMask : Gfx10Rsrc3dSwModeMask;

                    if (m_settings.supportRbPlus)
                    {
                        allowedSwModeSet.value &= ~Gfx10DisplaySwModeMask;
                    }

                    if (pIn->flags.view3dAs2dArray)
                    {
                        allowedSwModeSet.value &= Gfx10Rsrc3dThinSwModeMask;
                    }
                    break;

                default:
                    ADDR_ASSERT_ALWAYS();
                    allowedSwModeSet.value = 0;
                    break;
            }

            if (ElemLib::IsBlockCompressed(pIn->format)  ||
                ElemLib::IsMacroPixelPacked(pIn->format) ||
                (bpp > 64)                               ||
                (msaa && ((bpp > 32) || pIn->flags.color || pIn->flags.unordered)))
            {
                allowedSwModeSet.value &= ~Gfx10ZSwModeMask;
            }

            if (pIn->format == ADDR_FMT_32_32_32)
            {
                allowedSwModeSet.value &= Gfx10LinearSwModeMask;
            }

            if (msaa)
            {
                allowedSwModeSet.value &= Gfx10MsaaSwModeMask;
            }

            if (pIn->flags.depth || pIn->flags.stencil)
            {
                allowedSwModeSet.value &= Gfx10ZSwModeMask;
            }

            if (pIn->flags.display)
            {
                if (m_settings.isDcn2)
                {
                    allowedSwModeSet.value &= (bpp == 64) ? Dcn2Bpp64SwModeMask : Dcn2NonBpp64SwModeMask;
                }
                else
                {
                    ADDR_NOT_IMPLEMENTED();
                }
            }

            if (allowedSwModeSet.value != 0)
            {
#if DEBUG
                // Post sanity check, at least AddrLib should accept the output generated by its own
                UINT_32 validateSwModeSet = allowedSwModeSet.value;

                for (UINT_32 i = 0; validateSwModeSet != 0; i++)
                {
                    if (validateSwModeSet & 1)
                    {
                        localIn.swizzleMode = static_cast<AddrSwizzleMode>(i);
                        ADDR_ASSERT(ValidateSwModeParams(&localIn));
                    }

                    validateSwModeSet >>= 1;
                }
#endif

                pOut->resourceType   = pIn->resourceType;
                pOut->validSwModeSet = allowedSwModeSet;
                pOut->canXor         = (allowedSwModeSet.value & Gfx10XorSwModeMask) ? TRUE : FALSE;
                pOut->validBlockSet  = GetAllowedBlockSet(allowedSwModeSet, pOut->resourceType);
                pOut->validSwTypeSet = GetAllowedSwSet(allowedSwModeSet);

                pOut->clientPreferredSwSet = pIn->preferredSwSet;

                if (pOut->clientPreferredSwSet.value == 0)
                {
                    pOut->clientPreferredSwSet.value = AddrSwSetAll;
                }

                if (allowedSwModeSet.value == Gfx10LinearSwModeMask)
                {
                    pOut->swizzleMode = ADDR_SW_LINEAR;
                }
                else
                {
                    // Always ignore linear swizzle mode if there is other choice.
                    allowedSwModeSet.swLinear = 0;

                    ADDR2_BLOCK_SET allowedBlockSet = GetAllowedBlockSet(allowedSwModeSet, pOut->resourceType);

                    // Determine block size if there is 2 or more block type candidates
                    if (IsPow2(allowedBlockSet.value) == FALSE)
                    {
                        AddrSwizzleMode swMode[AddrBlockMaxTiledType] = { ADDR_SW_LINEAR };

                        if (pOut->resourceType == ADDR_RSRC_TEX_3D)
                        {
                            swMode[AddrBlockThick4KB]  = ADDR_SW_4KB_S;
                            swMode[AddrBlockThin64KB]  = ADDR_SW_64KB_R_X;
                            swMode[AddrBlockThick64KB] = ADDR_SW_64KB_S;
                        }
                        else
                        {
                            swMode[AddrBlockMicro]    = ADDR_SW_256B_S;
                            swMode[AddrBlockThin4KB]  = ADDR_SW_4KB_S;
                            swMode[AddrBlockThin64KB] = ADDR_SW_64KB_S;
                        }

                        Dim3d   blkDim[AddrBlockMaxTiledType]  = {{0}, {0}, {0}, {0}, {0}};
                        Dim3d   padDim[AddrBlockMaxTiledType]  = {{0}, {0}, {0}, {0}, {0}};
                        UINT_64 padSize[AddrBlockMaxTiledType] = {0};

                        const UINT_32 ratioLow           = pIn->flags.minimizeAlign ? 1 : (pIn->flags.opt4space ? 3 : 2);
                        const UINT_32 ratioHi            = pIn->flags.minimizeAlign ? 1 : (pIn->flags.opt4space ? 2 : 1);
                        const UINT_64 sizeAlignInElement = Max(NextPow2(pIn->minSizeAlign) / (bpp >> 3), 1u);
                        UINT_32       minSizeBlk         = AddrBlockMicro;
                        UINT_64       minSize            = 0;

                        for (UINT_32 i = AddrBlockMicro; i < AddrBlockMaxTiledType; i++)
                        {
                            if (allowedBlockSet.value & (1 << i))
                            {
                                ComputeBlockDimensionForSurf(&blkDim[i].w,
                                                             &blkDim[i].h,
                                                             &blkDim[i].d,
                                                             bpp,
                                                             numFrags,
                                                             pOut->resourceType,
                                                             swMode[i]);

                                padSize[i] = ComputePadSize(&blkDim[i], width, height, numSlices, &padDim[i]);
                                padSize[i] = PowTwoAlign(padSize[i], sizeAlignInElement);

                                if ((minSize == 0) ||
                                    ((padSize[i] * ratioHi) <= (minSize * ratioLow)))
                                {
                                    minSize    = padSize[i];
                                    minSizeBlk = i;
                                }
                            }
                        }

                        if ((allowedBlockSet.micro == TRUE)      &&
                            (width  <= blkDim[AddrBlockMicro].w) &&
                            (height <= blkDim[AddrBlockMicro].h))
                        {
                            minSizeBlk = AddrBlockMicro;
                        }

                        if (minSizeBlk == AddrBlockMicro)
                        {
                            ADDR_ASSERT(pOut->resourceType != ADDR_RSRC_TEX_3D);
                            allowedSwModeSet.value &= Gfx10Blk256BSwModeMask;
                        }
                        else if (minSizeBlk == AddrBlockThick4KB)
                        {
                            ADDR_ASSERT(pOut->resourceType == ADDR_RSRC_TEX_3D);
                            allowedSwModeSet.value &= Gfx10Rsrc3dThick4KBSwModeMask;
                        }
                        else if (minSizeBlk == AddrBlockThin4KB)
                        {
                            ADDR_ASSERT(pOut->resourceType != ADDR_RSRC_TEX_3D);
                            allowedSwModeSet.value &= Gfx10Blk4KBSwModeMask;
                        }
                        else if (minSizeBlk == AddrBlockThick64KB)
                        {
                            ADDR_ASSERT(pOut->resourceType == ADDR_RSRC_TEX_3D);
                            allowedSwModeSet.value &= Gfx10Rsrc3dThick64KBSwModeMask;
                        }
                        else
                        {
                            ADDR_ASSERT(minSizeBlk == AddrBlockThin64KB);
                            allowedSwModeSet.value &= (pOut->resourceType == ADDR_RSRC_TEX_3D) ?
                                                      Gfx10Rsrc3dThinSwModeMask : Gfx10Blk64KBSwModeMask;
                        }
                    }

                    // Block type should be determined.
                    ADDR_ASSERT(IsPow2(GetAllowedBlockSet(allowedSwModeSet, pOut->resourceType).value));

                    ADDR2_SWTYPE_SET allowedSwSet = GetAllowedSwSet(allowedSwModeSet);

                    // Determine swizzle type if there is 2 or more swizzle type candidates
                    if (IsPow2(allowedSwSet.value) == FALSE)
                    {
                        if (ElemLib::IsBlockCompressed(pIn->format))
                        {
                            if (allowedSwSet.sw_D)
                            {
                                allowedSwModeSet.value &= Gfx10DisplaySwModeMask;
                            }
                            else if (allowedSwSet.sw_S)
                            {
                                allowedSwModeSet.value &= Gfx10StandardSwModeMask;
                            }
                            else
                            {
                                ADDR_ASSERT(allowedSwSet.sw_R);
                                allowedSwModeSet.value &= Gfx10RenderSwModeMask;
                            }
                        }
                        else if (ElemLib::IsMacroPixelPacked(pIn->format))
                        {
                            if (allowedSwSet.sw_S)
                            {
                                allowedSwModeSet.value &= Gfx10StandardSwModeMask;
                            }
                            else if (allowedSwSet.sw_D)
                            {
                                allowedSwModeSet.value &= Gfx10DisplaySwModeMask;
                            }
                            else
                            {
                                ADDR_ASSERT(allowedSwSet.sw_R);
                                allowedSwModeSet.value &= Gfx10RenderSwModeMask;
                            }
                        }
                        else if (pIn->resourceType == ADDR_RSRC_TEX_3D)
                        {
                            if (pIn->flags.color &&
                                GetAllowedBlockSet(allowedSwModeSet, pOut->resourceType).macroThick64KB &&
                                allowedSwSet.sw_D)
                            {
                                allowedSwModeSet.value &= Gfx10DisplaySwModeMask;
                            }
                            else if (allowedSwSet.sw_S)
                            {
                                allowedSwModeSet.value &= Gfx10StandardSwModeMask;
                            }
                            else if (allowedSwSet.sw_R)
                            {
                                allowedSwModeSet.value &= Gfx10RenderSwModeMask;
                            }
                            else
                            {
                                ADDR_ASSERT(allowedSwSet.sw_Z);
                                allowedSwModeSet.value &= Gfx10ZSwModeMask;
                            }
                        }
                        else
                        {
                            if (allowedSwSet.sw_R)
                            {
                                allowedSwModeSet.value &= Gfx10RenderSwModeMask;
                            }
                            else if (allowedSwSet.sw_D)
                            {
                                allowedSwModeSet.value &= Gfx10DisplaySwModeMask;
                            }
                            else if (allowedSwSet.sw_S)
                            {
                                allowedSwModeSet.value &= Gfx10StandardSwModeMask;
                            }
                            else
                            {
                                ADDR_ASSERT(allowedSwSet.sw_Z);
                                allowedSwModeSet.value &= Gfx10ZSwModeMask;
                            }
                        }
                    }

                    // Swizzle type should be determined.
                    ADDR_ASSERT(IsPow2(GetAllowedSwSet(allowedSwModeSet).value));

                    // Determine swizzle mode now. Always select the "largest" swizzle mode for a given block type +
                    // swizzle type combination. E.g, for AddrBlockThin64KB + ADDR_SW_S, select SW_64KB_S_X(25) if it's
                    // available, or otherwise select SW_64KB_S_T(17) if it's available, or otherwise select SW_64KB_S(9).
                    pOut->swizzleMode = static_cast<AddrSwizzleMode>(Log2NonPow2(allowedSwModeSet.value));
                }
            }
            else
            {
                // Invalid combination...
                ADDR_ASSERT_ALWAYS();
                returnCode = ADDR_INVALIDPARAMS;
            }
        }
        else
        {
            // Invalid combination...
            ADDR_ASSERT_ALWAYS();
            returnCode = ADDR_INVALIDPARAMS;
        }
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Gfx10Lib::ComputeStereoInfo
*
*   @brief
*       Compute height alignment and right eye pipeBankXor for stereo surface
*
*   @return
*       Error code
*
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::ComputeStereoInfo(
    const ADDR2_COMPUTE_SURFACE_INFO_INPUT* pIn,        ///< Compute surface info
    UINT_32                                 blkHeight,  ///< Block height
    UINT_32*                                pAlignY,    ///< Stereo requested additional alignment in Y
    UINT_32*                                pRightXor   ///< Right eye xor
    ) const
{
    ADDR_E_RETURNCODE ret = ADDR_OK;

    *pAlignY   = 1;
    *pRightXor = 0;

    if (IsNonPrtXor(pIn->swizzleMode))
    {
        const UINT_32 blkSizeLog2 = GetBlockSizeLog2(pIn->swizzleMode);
        const UINT_32 elemLog2    = Log2(pIn->bpp >> 3);
        const UINT_32 rsrcType    = static_cast<UINT_32>(pIn->resourceType) - 1;
        const UINT_32 swMode      = static_cast<UINT_32>(pIn->swizzleMode);
        const UINT_32 eqIndex     = m_equationLookupTable[rsrcType][swMode][elemLog2];

        if (eqIndex != ADDR_INVALID_EQUATION_INDEX)
        {
            UINT_32 yMax = 0;
            UINT_32 yPos = 0;

            for (UINT_32 i = m_pipeInterleaveLog2; i < blkSizeLog2; i++)
            {
                if (m_equationTable[eqIndex].xor1[i].value == 0)
                {
                    break;
                }

                ADDR_ASSERT(m_equationTable[eqIndex].xor1[i].valid == 1);

                if ((m_equationTable[eqIndex].xor1[i].channel == 1) &&
                    (m_equationTable[eqIndex].xor1[i].index > yMax))
                {
                    yMax = m_equationTable[eqIndex].xor1[i].index;
                    yPos = i;
                }
            }

            const UINT_32 additionalAlign = 1 << yMax;

            if (additionalAlign >= blkHeight)
            {
                *pAlignY *= (additionalAlign / blkHeight);

                const UINT_32 alignedHeight = PowTwoAlign(pIn->height, additionalAlign);

                if ((alignedHeight >> yMax) & 1)
                {
                    *pRightXor = 1 << (yPos - m_pipeInterleaveLog2);
                }
            }
        }
        else
        {
            ret = ADDR_INVALIDPARAMS;
        }
    }

    return ret;
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlComputeSurfaceInfoTiled
*
*   @brief
*       Internal function to calculate alignment for tiled surface
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::HwlComputeSurfaceInfoTiled(
     const ADDR2_COMPUTE_SURFACE_INFO_INPUT* pIn,    ///< [in] input structure
     ADDR2_COMPUTE_SURFACE_INFO_OUTPUT*      pOut    ///< [out] output structure
     ) const
{
    ADDR_E_RETURNCODE ret;

    if (IsBlock256b(pIn->swizzleMode))
    {
        ret = ComputeSurfaceInfoMicroTiled(pIn, pOut);
    }
    else
    {
        ret = ComputeSurfaceInfoMacroTiled(pIn, pOut);
    }

    return ret;
}

/**
************************************************************************************************************************
*   Gfx10Lib::ComputeSurfaceInfoMicroTiled
*
*   @brief
*       Internal function to calculate alignment for micro tiled surface
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::ComputeSurfaceInfoMicroTiled(
     const ADDR2_COMPUTE_SURFACE_INFO_INPUT* pIn,    ///< [in] input structure
     ADDR2_COMPUTE_SURFACE_INFO_OUTPUT*      pOut    ///< [out] output structure
     ) const
{
    ADDR_E_RETURNCODE ret = ComputeBlockDimensionForSurf(&pOut->blockWidth,
                                                         &pOut->blockHeight,
                                                         &pOut->blockSlices,
                                                         pIn->bpp,
                                                         pIn->numFrags,
                                                         pIn->resourceType,
                                                         pIn->swizzleMode);

    if (ret == ADDR_OK)
    {
        pOut->mipChainPitch    = 0;
        pOut->mipChainHeight   = 0;
        pOut->mipChainSlice    = 0;
        pOut->epitchIsHeight   = FALSE;
        pOut->mipChainInTail   = FALSE;
        pOut->firstMipIdInTail = pIn->numMipLevels;

        const UINT_32 blockSize = GetBlockSize(pIn->swizzleMode);

        pOut->pitch     = PowTwoAlign(pIn->width,  pOut->blockWidth);
        pOut->height    = PowTwoAlign(pIn->height, pOut->blockHeight);
        pOut->numSlices = pIn->numSlices;
        pOut->baseAlign = blockSize;

        if (pIn->numMipLevels > 1)
        {
            const UINT_32 mip0Width    = pIn->width;
            const UINT_32 mip0Height   = pIn->height;
            UINT_64       mipSliceSize = 0;

            for (INT_32 i = static_cast<INT_32>(pIn->numMipLevels) - 1; i >= 0; i--)
            {
                UINT_32 mipWidth, mipHeight;

                GetMipSize(mip0Width, mip0Height, 1, i, &mipWidth, &mipHeight);

                const UINT_32 mipActualWidth  = PowTwoAlign(mipWidth,  pOut->blockWidth);
                const UINT_32 mipActualHeight = PowTwoAlign(mipHeight, pOut->blockHeight);

                if (pOut->pMipInfo != NULL)
                {
                    pOut->pMipInfo[i].pitch            = mipActualWidth;
                    pOut->pMipInfo[i].height           = mipActualHeight;
                    pOut->pMipInfo[i].depth            = 1;
                    pOut->pMipInfo[i].offset           = mipSliceSize;
                    pOut->pMipInfo[i].mipTailOffset    = 0;
                    pOut->pMipInfo[i].macroBlockOffset = mipSliceSize;
                }

                mipSliceSize += mipActualWidth * mipActualHeight * (pIn->bpp >> 3);
            }

            pOut->sliceSize = mipSliceSize;
            pOut->surfSize  = mipSliceSize * pOut->numSlices;
        }
        else
        {
            pOut->sliceSize = static_cast<UINT_64>(pOut->pitch) * pOut->height * (pIn->bpp >> 3);
            pOut->surfSize  = pOut->sliceSize * pOut->numSlices;

            if (pOut->pMipInfo != NULL)
            {
                pOut->pMipInfo[0].pitch            = pOut->pitch;
                pOut->pMipInfo[0].height           = pOut->height;
                pOut->pMipInfo[0].depth            = 1;
                pOut->pMipInfo[0].offset           = 0;
                pOut->pMipInfo[0].mipTailOffset    = 0;
                pOut->pMipInfo[0].macroBlockOffset = 0;
            }
        }

    }

    return ret;
}

/**
************************************************************************************************************************
*   Gfx10Lib::ComputeSurfaceInfoMacroTiled
*
*   @brief
*       Internal function to calculate alignment for macro tiled surface
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::ComputeSurfaceInfoMacroTiled(
     const ADDR2_COMPUTE_SURFACE_INFO_INPUT* pIn,    ///< [in] input structure
     ADDR2_COMPUTE_SURFACE_INFO_OUTPUT*      pOut    ///< [out] output structure
     ) const
{
    ADDR_E_RETURNCODE returnCode = ComputeBlockDimensionForSurf(&pOut->blockWidth,
                                                                &pOut->blockHeight,
                                                                &pOut->blockSlices,
                                                                pIn->bpp,
                                                                pIn->numFrags,
                                                                pIn->resourceType,
                                                                pIn->swizzleMode);

    if (returnCode == ADDR_OK)
    {
        UINT_32 heightAlign = pOut->blockHeight;

        if (pIn->flags.qbStereo)
        {
            UINT_32 rightXor = 0;
            UINT_32 alignY   = 1;

            returnCode = ComputeStereoInfo(pIn, heightAlign, &alignY, &rightXor);

            if (returnCode == ADDR_OK)
            {
                pOut->pStereoInfo->rightSwizzle = rightXor;

                heightAlign *= alignY;
            }
        }

        if (returnCode == ADDR_OK)
        {
            // Mip chain dimesion and epitch has no meaning in GFX10, set to default value
            pOut->mipChainPitch    = 0;
            pOut->mipChainHeight   = 0;
            pOut->mipChainSlice    = 0;
            pOut->epitchIsHeight   = FALSE;
            pOut->mipChainInTail   = FALSE;
            pOut->firstMipIdInTail = pIn->numMipLevels;

            const UINT_32 blockSizeLog2 = GetBlockSizeLog2(pIn->swizzleMode);
            const UINT_32 blockSize     = 1 << blockSizeLog2;

            pOut->pitch     = PowTwoAlign(pIn->width,     pOut->blockWidth);
            pOut->height    = PowTwoAlign(pIn->height,    heightAlign);
            pOut->numSlices = PowTwoAlign(pIn->numSlices, pOut->blockSlices);
            pOut->baseAlign = blockSize;

            if (pIn->numMipLevels > 1)
            {
                const Dim3d  tailMaxDim         = GetMipTailDim(pIn->resourceType,
                                                                pIn->swizzleMode,
                                                                pOut->blockWidth,
                                                                pOut->blockHeight,
                                                                pOut->blockSlices);
                const UINT_32 mip0Width         = pIn->width;
                const UINT_32 mip0Height        = pIn->height;
                const BOOL_32 isThin            = IsThin(pIn->resourceType, pIn->swizzleMode);
                const UINT_32 mip0Depth         = isThin ? 1 : pIn->numSlices;
                const UINT_32 maxMipsInTail     = GetMaxNumMipsInTail(blockSizeLog2, isThin);
                const UINT_32 index             = Log2(pIn->bpp >> 3);
                UINT_32       firstMipInTail    = pIn->numMipLevels;
                UINT_64       mipChainSliceSize = 0;
                UINT_64       mipSize[MaxMipLevels];
                UINT_64       mipSliceSize[MaxMipLevels];

                Dim3d fixedTailMaxDim = tailMaxDim;

                if (m_settings.dsMipmapHtileFix && IsZOrderSwizzle(pIn->swizzleMode) && (index <= 1))
                {
                    fixedTailMaxDim.w /= Block256_2d[index].w / Block256_2d[2].w;
                    fixedTailMaxDim.h /= Block256_2d[index].h / Block256_2d[2].h;
                }

                for (UINT_32 i = 0; i < pIn->numMipLevels; i++)
                {
                    UINT_32 mipWidth, mipHeight, mipDepth;

                    GetMipSize(mip0Width, mip0Height, mip0Depth, i, &mipWidth, &mipHeight, &mipDepth);

                    if (IsInMipTail(fixedTailMaxDim, maxMipsInTail, mipWidth, mipHeight, pIn->numMipLevels - i))
                    {
                        firstMipInTail     = i;
                        mipChainSliceSize += blockSize / pOut->blockSlices;
                        break;
                    }
                    else
                    {
                        const UINT_32 pitch     = PowTwoAlign(mipWidth,  pOut->blockWidth);
                        const UINT_32 height    = PowTwoAlign(mipHeight, pOut->blockHeight);
                        const UINT_32 depth     = PowTwoAlign(mipDepth,  pOut->blockSlices);
                        const UINT_64 sliceSize = static_cast<UINT_64>(pitch) * height * (pIn->bpp >> 3);

                        mipSize[i]         = sliceSize * depth;
                        mipSliceSize[i]    = sliceSize * pOut->blockSlices;
                        mipChainSliceSize += sliceSize;

                        if (pOut->pMipInfo != NULL)
                        {
                            pOut->pMipInfo[i].pitch  = pitch;
                            pOut->pMipInfo[i].height = height;
                            pOut->pMipInfo[i].depth  = depth;
                        }
                    }
                }

                pOut->sliceSize        = mipChainSliceSize;
                pOut->surfSize         = mipChainSliceSize * pOut->numSlices;
                pOut->mipChainInTail   = (firstMipInTail == 0) ? TRUE : FALSE;
                pOut->firstMipIdInTail = firstMipInTail;

                if (pOut->pMipInfo != NULL)
                {
                    UINT_64 offset         = 0;
                    UINT_64 macroBlkOffset = 0;
                    UINT_32 tailMaxDepth   = 0;

                    if (firstMipInTail != pIn->numMipLevels)
                    {
                        UINT_32 mipWidth, mipHeight;

                        GetMipSize(mip0Width, mip0Height, mip0Depth, firstMipInTail,
                                   &mipWidth, &mipHeight, &tailMaxDepth);

                        offset         = blockSize * PowTwoAlign(tailMaxDepth, pOut->blockSlices) / pOut->blockSlices;
                        macroBlkOffset = blockSize;
                    }

                    for (INT_32 i = firstMipInTail - 1; i >= 0; i--)
                    {
                        pOut->pMipInfo[i].offset           = offset;
                        pOut->pMipInfo[i].macroBlockOffset = macroBlkOffset;
                        pOut->pMipInfo[i].mipTailOffset    = 0;

                        offset         += mipSize[i];
                        macroBlkOffset += mipSliceSize[i];
                    }

                    UINT_32 pitch  = tailMaxDim.w;
                    UINT_32 height = tailMaxDim.h;
                    UINT_32 depth  = isThin ? 1 : PowTwoAlign(tailMaxDepth, Block256_3d[index].d);

                    tailMaxDepth = isThin ? 1 : (depth / Block256_3d[index].d);

                    for (UINT_32 i = firstMipInTail; i < pIn->numMipLevels; i++)
                    {
                        const UINT_32 m         = maxMipsInTail - 1 - (i - firstMipInTail);
                        const UINT_32 mipOffset = (m > 6) ? (16 << m) : (m << 8);

                        pOut->pMipInfo[i].offset           = mipOffset * tailMaxDepth;
                        pOut->pMipInfo[i].mipTailOffset    = mipOffset;
                        pOut->pMipInfo[i].macroBlockOffset = 0;

                        pOut->pMipInfo[i].pitch  = pitch;
                        pOut->pMipInfo[i].height = height;
                        pOut->pMipInfo[i].depth  = depth;

                        UINT_32 mipX = ((mipOffset >> 9)  & 1)  |
                                       ((mipOffset >> 10) & 2)  |
                                       ((mipOffset >> 11) & 4)  |
                                       ((mipOffset >> 12) & 8)  |
                                       ((mipOffset >> 13) & 16) |
                                       ((mipOffset >> 14) & 32);
                        UINT_32 mipY = ((mipOffset >> 8)  & 1)  |
                                       ((mipOffset >> 9)  & 2)  |
                                       ((mipOffset >> 10) & 4)  |
                                       ((mipOffset >> 11) & 8)  |
                                       ((mipOffset >> 12) & 16) |
                                       ((mipOffset >> 13) & 32);

                        if (blockSizeLog2 & 1)
                        {
                            const UINT_32 temp = mipX;
                            mipX = mipY;
                            mipY = temp;

                            if (index & 1)
                            {
                                mipY = (mipY << 1) | (mipX & 1);
                                mipX = mipX >> 1;
                            }
                        }

                        if (isThin)
                        {
                            pOut->pMipInfo[i].mipTailCoordX = mipX * Block256_2d[index].w;
                            pOut->pMipInfo[i].mipTailCoordY = mipY * Block256_2d[index].h;
                            pOut->pMipInfo[i].mipTailCoordZ = 0;

                            pitch  = Max(pitch  >> 1, Block256_2d[index].w);
                            height = Max(height >> 1, Block256_2d[index].h);
                            depth  = 1;
                        }
                        else
                        {
                            pOut->pMipInfo[i].mipTailCoordX = mipX * Block256_3d[index].w;
                            pOut->pMipInfo[i].mipTailCoordY = mipY * Block256_3d[index].h;
                            pOut->pMipInfo[i].mipTailCoordZ = 0;

                            pitch  = Max(pitch  >> 1, Block256_3d[index].w);
                            height = Max(height >> 1, Block256_3d[index].h);
                            depth  = PowTwoAlign(Max(depth  >> 1, 1u), Block256_3d[index].d);
                        }
                    }
                }
            }
            else
            {
                pOut->sliceSize = static_cast<UINT_64>(pOut->pitch) * pOut->height * (pIn->bpp >> 3) * pIn->numFrags;
                pOut->surfSize  = pOut->sliceSize * pOut->numSlices;

                if (pOut->pMipInfo != NULL)
                {
                    pOut->pMipInfo[0].pitch            = pOut->pitch;
                    pOut->pMipInfo[0].height           = pOut->height;
                    pOut->pMipInfo[0].depth            = IsTex3d(pIn->resourceType)? pOut->numSlices : 1;
                    pOut->pMipInfo[0].offset           = 0;
                    pOut->pMipInfo[0].mipTailOffset    = 0;
                    pOut->pMipInfo[0].macroBlockOffset = 0;
                    pOut->pMipInfo[0].mipTailCoordX    = 0;
                    pOut->pMipInfo[0].mipTailCoordY    = 0;
                    pOut->pMipInfo[0].mipTailCoordZ    = 0;
                }
            }
        }
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlComputeSurfaceAddrFromCoordTiled
*
*   @brief
*       Internal function to calculate address from coord for tiled swizzle surface
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::HwlComputeSurfaceAddrFromCoordTiled(
     const ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_INPUT* pIn,    ///< [in] input structure
     ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_OUTPUT*      pOut    ///< [out] output structure
     ) const
{
    ADDR_E_RETURNCODE ret;

    if (IsBlock256b(pIn->swizzleMode))
    {
        ret = ComputeSurfaceAddrFromCoordMicroTiled(pIn, pOut);
    }
    else
    {
        ret = ComputeSurfaceAddrFromCoordMacroTiled(pIn, pOut);
    }

    return ret;
}

/**
************************************************************************************************************************
*   Gfx10Lib::ComputeOffsetFromEquation
*
*   @brief
*       Compute offset from equation
*
*   @return
*       Offset
************************************************************************************************************************
*/
UINT_32 Gfx10Lib::ComputeOffsetFromEquation(
    const ADDR_EQUATION* pEq,   ///< Equation
    UINT_32              x,     ///< x coord in bytes
    UINT_32              y,     ///< y coord in pixel
    UINT_32              z      ///< z coord in slice
    ) const
{
    UINT_32 offset = 0;

    for (UINT_32 i = 0; i < pEq->numBits; i++)
    {
        UINT_32 v = 0;

        if (pEq->addr[i].valid)
        {
            if (pEq->addr[i].channel == 0)
            {
                v ^= (x >> pEq->addr[i].index) & 1;
            }
            else if (pEq->addr[i].channel == 1)
            {
                v ^= (y >> pEq->addr[i].index) & 1;
            }
            else
            {
                ADDR_ASSERT(pEq->addr[i].channel == 2);
                v ^= (z >> pEq->addr[i].index) & 1;
            }
        }

        if (pEq->xor1[i].valid)
        {
            if (pEq->xor1[i].channel == 0)
            {
                v ^= (x >> pEq->xor1[i].index) & 1;
            }
            else if (pEq->xor1[i].channel == 1)
            {
                v ^= (y >> pEq->xor1[i].index) & 1;
            }
            else
            {
                ADDR_ASSERT(pEq->xor1[i].channel == 2);
                v ^= (z >> pEq->xor1[i].index) & 1;
            }
        }

        if (pEq->xor2[i].valid)
        {
            if (pEq->xor2[i].channel == 0)
            {
                v ^= (x >> pEq->xor2[i].index) & 1;
            }
            else if (pEq->xor2[i].channel == 1)
            {
                v ^= (y >> pEq->xor2[i].index) & 1;
            }
            else
            {
                ADDR_ASSERT(pEq->xor2[i].channel == 2);
                v ^= (z >> pEq->xor2[i].index) & 1;
            }
        }

        offset |= (v << i);
    }

    return offset;
}

/**
************************************************************************************************************************
*   Gfx10Lib::ComputeOffsetFromSwizzlePattern
*
*   @brief
*       Compute offset from swizzle pattern
*
*   @return
*       Offset
************************************************************************************************************************
*/
UINT_32 Gfx10Lib::ComputeOffsetFromSwizzlePattern(
    const UINT_64* pPattern,    ///< Swizzle pattern
    UINT_32        numBits,     ///< Number of bits in pattern
    UINT_32        x,           ///< x coord in pixel
    UINT_32        y,           ///< y coord in pixel
    UINT_32        z,           ///< z coord in slice
    UINT_32        s            ///< sample id
    ) const
{
    UINT_32                 offset          = 0;
    const ADDR_BIT_SETTING* pSwizzlePattern = reinterpret_cast<const ADDR_BIT_SETTING*>(pPattern);

    for (UINT_32 i = 0; i < numBits; i++)
    {
        UINT_32 v = 0;

        if (pSwizzlePattern[i].x != 0)
        {
            UINT_16 mask  = pSwizzlePattern[i].x;
            UINT_32 xBits = x;

            while (mask != 0)
            {
                if (mask & 1)
                {
                    v ^= xBits & 1;
                }

                xBits >>= 1;
                mask  >>= 1;
            }
        }

        if (pSwizzlePattern[i].y != 0)
        {
            UINT_16 mask  = pSwizzlePattern[i].y;
            UINT_32 yBits = y;

            while (mask != 0)
            {
                if (mask & 1)
                {
                    v ^= yBits & 1;
                }

                yBits >>= 1;
                mask  >>= 1;
            }
        }

        if (pSwizzlePattern[i].z != 0)
        {
            UINT_16 mask  = pSwizzlePattern[i].z;
            UINT_32 zBits = z;

            while (mask != 0)
            {
                if (mask & 1)
                {
                    v ^= zBits & 1;
                }

                zBits >>= 1;
                mask  >>= 1;
            }
        }

        if (pSwizzlePattern[i].s != 0)
        {
            UINT_16 mask  = pSwizzlePattern[i].s;
            UINT_32 sBits = s;

            while (mask != 0)
            {
                if (mask & 1)
                {
                    v ^= sBits & 1;
                }

                sBits >>= 1;
                mask  >>= 1;
            }
        }

        offset |= (v << i);
    }

    return offset;
}

/**
************************************************************************************************************************
*   Gfx10Lib::GetSwizzlePattern
*
*   @brief
*       Get swizzle pattern
*
*   @return
*       Swizzle pattern
************************************************************************************************************************
*/
const UINT_64* Gfx10Lib::GetSwizzlePattern(
    AddrSwizzleMode  swizzleMode,       ///< Swizzle mode
    AddrResourceType resourceType,      ///< Resource type
    UINT_32          elemLog2,          ///< Element size in bytes log2
    UINT_32          numFrag            ///< Number of fragment
    ) const
{
    const UINT_32  index           = IsXor(swizzleMode) ? (m_colorBaseIndex + elemLog2) : elemLog2;
    const UINT_64* pSwizzlePattern = NULL;
    const UINT_32  swizzleMask     = 1 << swizzleMode;

    if (IsLinear(swizzleMode))
    {
        pSwizzlePattern = NULL;
    }
    else if (resourceType == ADDR_RSRC_TEX_3D)
    {
        ADDR_ASSERT(numFrag == 1);

        if ((swizzleMask & Gfx10Rsrc3dSwModeMask) == 0)
        {
            pSwizzlePattern = NULL;
        }
        else if (IsRtOptSwizzle(swizzleMode))
        {
            pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_R_X_1xaa_RBPLUS_PATIDX[index]] :
                                                         GFX10_SW_PATTERN[SW_64K_R_X_1xaa_PATIDX[index]];
        }
        else if (IsZOrderSwizzle(swizzleMode))
        {
            pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_Z_X_1xaa_RBPLUS_PATIDX[index]] :
                                                         GFX10_SW_PATTERN[SW_64K_Z_X_1xaa_PATIDX[index]];
        }
        else if (IsDisplaySwizzle(resourceType, swizzleMode))
        {
            ADDR_ASSERT(swizzleMode == ADDR_SW_64KB_D_X);
            pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_D3_X_RBPLUS_PATIDX[index]] :
                                                         GFX10_SW_PATTERN[SW_64K_D3_X_PATIDX[index]];
        }
        else
        {
            ADDR_ASSERT(IsStandardSwizzle(resourceType, swizzleMode));

            if (IsBlock4kb(swizzleMode))
            {
                if (swizzleMode == ADDR_SW_4KB_S)
                {
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_4K_S3_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_4K_S3_PATIDX[index]];
                }
                else
                {
                    ADDR_ASSERT(swizzleMode == ADDR_SW_4KB_S_X);
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_4K_S3_X_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_4K_S3_X_PATIDX[index]];
                }
            }
            else
            {
                if (swizzleMode == ADDR_SW_64KB_S)
                {
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_S3_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_64K_S3_PATIDX[index]];
                }
                else if (swizzleMode == ADDR_SW_64KB_S_X)
                {
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_S3_X_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_64K_S3_X_PATIDX[index]];
                }
                else
                {
                    ADDR_ASSERT(swizzleMode == ADDR_SW_64KB_S_T);
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_S3_T_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_64K_S3_T_PATIDX[index]];
                }
            }
        }
    }
    else
    {
        if ((swizzleMask & Gfx10Rsrc2dSwModeMask) == 0)
        {
            pSwizzlePattern = NULL;
        }
        else if (IsBlock256b(swizzleMode))
        {
            if (swizzleMode == ADDR_SW_256B_S)
            {
                pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_256_S_RBPLUS_PATIDX[index]] :
                                                             GFX10_SW_PATTERN[SW_256_S_PATIDX[index]];
            }
            else
            {
                ADDR_ASSERT(swizzleMode == ADDR_SW_256B_D);
                pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_256_D_RBPLUS_PATIDX[index]] :
                                                             GFX10_SW_PATTERN[SW_256_D_PATIDX[index]];
            }
        }
        else if (IsBlock4kb(swizzleMode))
        {
            if (IsStandardSwizzle(resourceType, swizzleMode))
            {
                if (swizzleMode == ADDR_SW_4KB_S)
                {
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_4K_S_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_4K_S_PATIDX[index]];
                }
                else
                {
                    ADDR_ASSERT(swizzleMode == ADDR_SW_4KB_S_X);
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_4K_S_X_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_4K_S_X_PATIDX[index]];
                }
            }
            else
            {
                if (swizzleMode == ADDR_SW_4KB_D)
                {
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_4K_D_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_4K_D_PATIDX[index]];
                }
                else
                {
                    ADDR_ASSERT(swizzleMode == ADDR_SW_4KB_D_X);
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_4K_D_X_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_4K_D_X_PATIDX[index]];
                }
            }
        }
        else
        {
            if (IsRtOptSwizzle(swizzleMode))
            {
                if (numFrag == 1)
                {
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_R_X_1xaa_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_64K_R_X_1xaa_PATIDX[index]];
                }
                else if (numFrag == 2)
                {
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_R_X_2xaa_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_64K_R_X_2xaa_PATIDX[index]];
                }
                else if (numFrag == 4)
                {
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_R_X_4xaa_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_64K_R_X_4xaa_PATIDX[index]];
                }
                else
                {
                    ADDR_ASSERT(numFrag == 8);
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_R_X_8xaa_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_64K_R_X_8xaa_PATIDX[index]];
                }
            }
            else if (IsZOrderSwizzle(swizzleMode))
            {
                if (numFrag == 1)
                {
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_Z_X_1xaa_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_64K_Z_X_1xaa_PATIDX[index]];
                }
                else if (numFrag == 2)
                {
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_Z_X_2xaa_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_64K_Z_X_2xaa_PATIDX[index]];
                }
                else if (numFrag == 4)
                {
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_Z_X_4xaa_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_64K_Z_X_4xaa_PATIDX[index]];
                }
                else
                {
                    ADDR_ASSERT(numFrag == 8);
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_Z_X_8xaa_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_64K_Z_X_8xaa_PATIDX[index]];
                }
            }
            else if (IsDisplaySwizzle(resourceType, swizzleMode))
            {
                if (swizzleMode == ADDR_SW_64KB_D)
                {
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_D_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_64K_D_PATIDX[index]];
                }
                else if (swizzleMode == ADDR_SW_64KB_D_X)
                {
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_D_X_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_64K_D_X_PATIDX[index]];
                }
                else
                {
                    ADDR_ASSERT(swizzleMode == ADDR_SW_64KB_D_T);
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_D_T_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_64K_D_T_PATIDX[index]];
                }
            }
            else
            {
                if (swizzleMode == ADDR_SW_64KB_S)
                {
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_S_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_64K_S_PATIDX[index]];
                }
                else if (swizzleMode == ADDR_SW_64KB_S_X)
                {
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_S_X_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_64K_S_X_PATIDX[index]];
                }
                else
                {
                    ADDR_ASSERT(swizzleMode == ADDR_SW_64KB_S_T);
                    pSwizzlePattern = m_settings.supportRbPlus ? GFX10_SW_PATTERN[SW_64K_S_T_RBPLUS_PATIDX[index]] :
                                                                 GFX10_SW_PATTERN[SW_64K_S_T_PATIDX[index]];
                }
            }
        }
    }

    return pSwizzlePattern;
}

/**
************************************************************************************************************************
*   Gfx10Lib::ComputeSurfaceAddrFromCoordMicroTiled
*
*   @brief
*       Internal function to calculate address from coord for micro tiled swizzle surface
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::ComputeSurfaceAddrFromCoordMicroTiled(
     const ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_INPUT* pIn,    ///< [in] input structure
     ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_OUTPUT*      pOut    ///< [out] output structure
     ) const
{
    ADDR2_COMPUTE_SURFACE_INFO_INPUT  localIn  = {0};
    ADDR2_COMPUTE_SURFACE_INFO_OUTPUT localOut = {0};
    ADDR2_MIP_INFO                    mipInfo[MaxMipLevels];

    localIn.swizzleMode  = pIn->swizzleMode;
    localIn.flags        = pIn->flags;
    localIn.resourceType = pIn->resourceType;
    localIn.bpp          = pIn->bpp;
    localIn.width        = Max(pIn->unalignedWidth,  1u);
    localIn.height       = Max(pIn->unalignedHeight, 1u);
    localIn.numSlices    = Max(pIn->numSlices,       1u);
    localIn.numMipLevels = Max(pIn->numMipLevels,    1u);
    localIn.numSamples   = Max(pIn->numSamples,      1u);
    localIn.numFrags     = Max(pIn->numFrags,        1u);
    localOut.pMipInfo    = mipInfo;

    ADDR_E_RETURNCODE ret = ComputeSurfaceInfoMicroTiled(&localIn, &localOut);

    if (ret == ADDR_OK)
    {
        const UINT_32 elemLog2 = Log2(pIn->bpp >> 3);
        const UINT_32 rsrcType = static_cast<UINT_32>(pIn->resourceType) - 1;
        const UINT_32 swMode   = static_cast<UINT_32>(pIn->swizzleMode);
        const UINT_32 eqIndex  = m_equationLookupTable[rsrcType][swMode][elemLog2];

        if (eqIndex != ADDR_INVALID_EQUATION_INDEX)
        {
            const UINT_32 pb           = mipInfo[pIn->mipId].pitch / localOut.blockWidth;
            const UINT_32 yb           = pIn->y / localOut.blockHeight;
            const UINT_32 xb           = pIn->x / localOut.blockWidth;
            const UINT_32 blockIndex   = yb * pb + xb;
            const UINT_32 blockSize    = 256;
            const UINT_32 blk256Offset = ComputeOffsetFromEquation(&m_equationTable[eqIndex],
                                                                   pIn->x << elemLog2,
                                                                   pIn->y,
                                                                   0);
            pOut->addr = localOut.sliceSize * pIn->slice +
                         mipInfo[pIn->mipId].macroBlockOffset +
                         (blockIndex * blockSize) +
                         blk256Offset;
        }
        else
        {
            ret = ADDR_INVALIDPARAMS;
        }
    }

    return ret;
}

/**
************************************************************************************************************************
*   Gfx10Lib::ComputeSurfaceAddrFromCoordMacroTiled
*
*   @brief
*       Internal function to calculate address from coord for macro tiled swizzle surface
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::ComputeSurfaceAddrFromCoordMacroTiled(
     const ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_INPUT* pIn,    ///< [in] input structure
     ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_OUTPUT*      pOut    ///< [out] output structure
     ) const
{
    ADDR2_COMPUTE_SURFACE_INFO_INPUT  localIn  = {0};
    ADDR2_COMPUTE_SURFACE_INFO_OUTPUT localOut = {0};
    ADDR2_MIP_INFO                    mipInfo[MaxMipLevels];

    localIn.swizzleMode  = pIn->swizzleMode;
    localIn.flags        = pIn->flags;
    localIn.resourceType = pIn->resourceType;
    localIn.bpp          = pIn->bpp;
    localIn.width        = Max(pIn->unalignedWidth,  1u);
    localIn.height       = Max(pIn->unalignedHeight, 1u);
    localIn.numSlices    = Max(pIn->numSlices,       1u);
    localIn.numMipLevels = Max(pIn->numMipLevels,    1u);
    localIn.numSamples   = Max(pIn->numSamples,      1u);
    localIn.numFrags     = Max(pIn->numFrags,        1u);
    localOut.pMipInfo    = mipInfo;

    ADDR_E_RETURNCODE ret = ComputeSurfaceInfoMacroTiled(&localIn, &localOut);

    if (ret == ADDR_OK)
    {
        const UINT_32 elemLog2    = Log2(pIn->bpp >> 3);
        const UINT_32 blkSizeLog2 = GetBlockSizeLog2(pIn->swizzleMode);
        const UINT_32 blkMask     = (1 << blkSizeLog2) - 1;
        const UINT_32 pipeMask    = (1 << m_pipesLog2) - 1;
        const UINT_32 bankMask    = ((1 << GetBankXorBits(blkSizeLog2)) - 1) << (m_pipesLog2 + ColumnBits);
        const UINT_32 pipeBankXor = IsXor(pIn->swizzleMode) ?
                                    (((pIn->pipeBankXor & (pipeMask | bankMask)) << m_pipeInterleaveLog2) & blkMask) : 0;

        if (localIn.numFrags > 1)
        {
            const UINT_64* pPattern = GetSwizzlePattern(pIn->swizzleMode,
                                                        pIn->resourceType,
                                                        elemLog2,
                                                        localIn.numFrags);

            if (pPattern != NULL)
            {
                const UINT_32 pb        = localOut.pitch / localOut.blockWidth;
                const UINT_32 yb        = pIn->y / localOut.blockHeight;
                const UINT_32 xb        = pIn->x / localOut.blockWidth;
                const UINT_64 blkIdx    = yb * pb + xb;
                const UINT_32 blkOffset = ComputeOffsetFromSwizzlePattern(pPattern,
                                                                          blkSizeLog2,
                                                                          pIn->x,
                                                                          pIn->y,
                                                                          pIn->slice,
                                                                          pIn->sample);
                pOut->addr = (localOut.sliceSize * pIn->slice) +
                             (blkIdx << blkSizeLog2) +
                             (blkOffset ^ pipeBankXor);
            }
            else
            {
                ret = ADDR_INVALIDPARAMS;
            }
        }
        else
        {
            const UINT_32 rsrcIdx = (pIn->resourceType == ADDR_RSRC_TEX_3D) ? 1 : 0;
            const UINT_32 swMode  = static_cast<UINT_32>(pIn->swizzleMode);
            const UINT_32 eqIndex = m_equationLookupTable[rsrcIdx][swMode][elemLog2];

            if (eqIndex != ADDR_INVALID_EQUATION_INDEX)
            {
                const BOOL_32 inTail    = (mipInfo[pIn->mipId].mipTailOffset != 0) ? TRUE : FALSE;
                const BOOL_32 isThin    = IsThin(pIn->resourceType, pIn->swizzleMode);
                const UINT_64 sliceSize = isThin ? localOut.sliceSize : (localOut.sliceSize * localOut.blockSlices);
                const UINT_32 sliceId   = isThin ? pIn->slice : (pIn->slice / localOut.blockSlices);
                const UINT_32 x         = inTail ? (pIn->x     + mipInfo[pIn->mipId].mipTailCoordX) : pIn->x;
                const UINT_32 y         = inTail ? (pIn->y     + mipInfo[pIn->mipId].mipTailCoordY) : pIn->y;
                const UINT_32 z         = inTail ? (pIn->slice + mipInfo[pIn->mipId].mipTailCoordZ) : pIn->slice;
                const UINT_32 pb        = mipInfo[pIn->mipId].pitch / localOut.blockWidth;
                const UINT_32 yb        = pIn->y / localOut.blockHeight;
                const UINT_32 xb        = pIn->x / localOut.blockWidth;
                const UINT_64 blkIdx    = yb * pb + xb;
                const UINT_32 blkOffset = ComputeOffsetFromEquation(&m_equationTable[eqIndex],
                                                                    x << elemLog2,
                                                                    y,
                                                                    z);
                pOut->addr = sliceSize * sliceId +
                             mipInfo[pIn->mipId].macroBlockOffset +
                             (blkIdx << blkSizeLog2) +
                             (blkOffset ^ pipeBankXor);
            }
            else
            {
                ret = ADDR_INVALIDPARAMS;
            }
        }
    }

    return ret;
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlComputeMaxBaseAlignments
*
*   @brief
*       Gets maximum alignments
*   @return
*       maximum alignments
************************************************************************************************************************
*/
UINT_32 Gfx10Lib::HwlComputeMaxBaseAlignments() const
{
    return Size64K;
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlComputeMaxMetaBaseAlignments
*
*   @brief
*       Gets maximum alignments for metadata
*   @return
*       maximum alignments for metadata
************************************************************************************************************************
*/
UINT_32 Gfx10Lib::HwlComputeMaxMetaBaseAlignments() const
{
    // Max base alignment for Htile
    Dim3d         metaBlk     = {0};
    const UINT_32 metaBlkSize = GetMetaBlkSize(Gfx10DataDepthStencil,
                                               ADDR_RSRC_TEX_2D,
                                               ADDR_SW_64KB_Z_X,
                                               0,
                                               0,
                                               TRUE,
                                               &metaBlk);

    const  UINT_32 maxBaseAlignHtile = Max(metaBlkSize, 1u << (m_pipesLog2 + 11u));

    // Max base alignment for Cmask
    const UINT_32 maxBaseAlignCmask = GetMetaBlkSize(Gfx10DataFmask,
                                                     ADDR_RSRC_TEX_2D,
                                                     ADDR_SW_64KB_Z_X,
                                                     0,
                                                     0,
                                                     TRUE,
                                                     &metaBlk);

    // Max base alignment for 2D Dcc
    const AddrSwizzleMode ValidSwizzleModeForDcc2D[] =
    {
        ADDR_SW_64KB_S_X,
        ADDR_SW_64KB_D_X,
        ADDR_SW_64KB_R_X,
    };

    UINT_32 maxBaseAlignDcc2D = 0;

    for (UINT_32 swIdx = 0; swIdx < sizeof(ValidSwizzleModeForDcc2D) / sizeof(ValidSwizzleModeForDcc2D[0]); swIdx++)
    {
        for (UINT_32 bppLog2 = 0; bppLog2 < MaxNumOfBpp; bppLog2++)
        {
            for (UINT_32 numFragLog2 = 0; numFragLog2 < 4; numFragLog2++)
            {
                const UINT_32 metaBlkSize2D = GetMetaBlkSize(Gfx10DataColor,
                                                             ADDR_RSRC_TEX_2D,
                                                             ValidSwizzleModeForDcc2D[swIdx],
                                                             bppLog2,
                                                             numFragLog2,
                                                             TRUE,
                                                             &metaBlk);

                maxBaseAlignDcc2D = Max(maxBaseAlignDcc2D, metaBlkSize2D);
            }
        }
    }

    // Max base alignment for 3D Dcc
    const AddrSwizzleMode ValidSwizzleModeForDcc3D[] =
    {
        ADDR_SW_64KB_Z_X,
        ADDR_SW_64KB_S_X,
        ADDR_SW_64KB_D_X,
        ADDR_SW_64KB_R_X,
    };

    UINT_32 maxBaseAlignDcc3D = 0;

    for (UINT_32 swIdx = 0; swIdx < sizeof(ValidSwizzleModeForDcc3D) / sizeof(ValidSwizzleModeForDcc3D[0]); swIdx++)
    {
        for (UINT_32 bppLog2 = 0; bppLog2 < MaxNumOfBpp; bppLog2++)
        {
            const UINT_32 metaBlkSize3D = GetMetaBlkSize(Gfx10DataColor,
                                                         ADDR_RSRC_TEX_3D,
                                                         ValidSwizzleModeForDcc3D[swIdx],
                                                         bppLog2,
                                                         0,
                                                         TRUE,
                                                         &metaBlk);

            maxBaseAlignDcc3D = Max(maxBaseAlignDcc3D, metaBlkSize3D);
        }
    }

    return Max(Max(maxBaseAlignHtile, maxBaseAlignCmask), Max(maxBaseAlignDcc2D, maxBaseAlignDcc3D));
}

/**
************************************************************************************************************************
*   Gfx10Lib::GetMetaElementSizeLog2
*
*   @brief
*       Gets meta data element size log2
*   @return
*       Meta data element size log2
************************************************************************************************************************
*/
INT_32 Gfx10Lib::GetMetaElementSizeLog2(
    Gfx10DataType dataType) ///< Data surface type
{
    INT_32 elemSizeLog2 = 0;

    if (dataType == Gfx10DataColor)
    {
        elemSizeLog2 = 0;
    }
    else if (dataType == Gfx10DataDepthStencil)
    {
        elemSizeLog2 = 2;
    }
    else
    {
        ADDR_ASSERT(dataType == Gfx10DataFmask);
        elemSizeLog2 = -1;
    }

    return elemSizeLog2;
}

/**
************************************************************************************************************************
*   Gfx10Lib::GetMetaCacheSizeLog2
*
*   @brief
*       Gets meta data cache line size log2
*   @return
*       Meta data cache line size log2
************************************************************************************************************************
*/
INT_32 Gfx10Lib::GetMetaCacheSizeLog2(
    Gfx10DataType dataType) ///< Data surface type
{
    INT_32 cacheSizeLog2 = 0;

    if (dataType == Gfx10DataColor)
    {
        cacheSizeLog2 = 6;
    }
    else if (dataType == Gfx10DataDepthStencil)
    {
        cacheSizeLog2 = 8;
    }
    else
    {
        ADDR_ASSERT(dataType == Gfx10DataFmask);
        cacheSizeLog2 = 8;
    }
    return cacheSizeLog2;
}

/**
************************************************************************************************************************
*   Gfx10Lib::HwlComputeSurfaceInfoLinear
*
*   @brief
*       Internal function to calculate alignment for linear surface
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Gfx10Lib::HwlComputeSurfaceInfoLinear(
     const ADDR2_COMPUTE_SURFACE_INFO_INPUT* pIn,    ///< [in] input structure
     ADDR2_COMPUTE_SURFACE_INFO_OUTPUT*      pOut    ///< [out] output structure
     ) const
{
    ADDR_E_RETURNCODE returnCode = ADDR_OK;

    if (IsTex1d(pIn->resourceType) && (pIn->height > 1))
    {
        returnCode = ADDR_INVALIDPARAMS;
    }
    else
    {
        const UINT_32 elementBytes = pIn->bpp >> 3;
        const UINT_32 pitchAlign   = (pIn->swizzleMode == ADDR_SW_LINEAR_GENERAL) ? 1 : (256 / elementBytes);
        const UINT_32 mipDepth     = (pIn->resourceType == ADDR_RSRC_TEX_3D) ? pIn->numSlices : 1;
        UINT_32       pitch        = PowTwoAlign(pIn->width, pitchAlign);
        UINT_32       actualHeight = pIn->height;
        UINT_64       sliceSize    = 0;

        if (pIn->numMipLevels > 1)
        {
            for (INT_32 i = static_cast<INT_32>(pIn->numMipLevels) - 1; i >= 0; i--)
            {
                UINT_32 mipWidth, mipHeight;

                GetMipSize(pIn->width, pIn->height, 1, i, &mipWidth, &mipHeight);

                const UINT_32 mipActualWidth = PowTwoAlign(mipWidth, pitchAlign);

                if (pOut->pMipInfo != NULL)
                {
                    pOut->pMipInfo[i].pitch            = mipActualWidth;
                    pOut->pMipInfo[i].height           = mipHeight;
                    pOut->pMipInfo[i].depth            = mipDepth;
                    pOut->pMipInfo[i].offset           = sliceSize;
                    pOut->pMipInfo[i].mipTailOffset    = 0;
                    pOut->pMipInfo[i].macroBlockOffset = sliceSize;
                }

                sliceSize += static_cast<UINT_64>(mipActualWidth) * mipHeight * elementBytes;
            }
        }
        else
        {
            returnCode = ApplyCustomizedPitchHeight(pIn, elementBytes, pitchAlign, &pitch, &actualHeight);

            if (returnCode == ADDR_OK)
            {
                sliceSize = static_cast<UINT_64>(pitch) * actualHeight * elementBytes;

                if (pOut->pMipInfo != NULL)
                {
                    pOut->pMipInfo[0].pitch            = pitch;
                    pOut->pMipInfo[0].height           = actualHeight;
                    pOut->pMipInfo[0].depth            = mipDepth;
                    pOut->pMipInfo[0].offset           = 0;
                    pOut->pMipInfo[0].mipTailOffset    = 0;
                    pOut->pMipInfo[0].macroBlockOffset = 0;
                }
            }
        }

        if (returnCode == ADDR_OK)
        {
            pOut->pitch          = pitch;
            pOut->height         = actualHeight;
            pOut->numSlices      = pIn->numSlices;
            pOut->sliceSize      = sliceSize;
            pOut->surfSize       = sliceSize * pOut->numSlices;
            pOut->baseAlign      = (pIn->swizzleMode == ADDR_SW_LINEAR_GENERAL) ? elementBytes : 256;
            pOut->blockWidth     = pitchAlign;
            pOut->blockHeight    = 1;
            pOut->blockSlices    = 1;

            // Following members are useless on GFX10
            pOut->mipChainPitch  = 0;
            pOut->mipChainHeight = 0;
            pOut->mipChainSlice  = 0;
            pOut->epitchIsHeight = FALSE;

            // Post calculation validate
            ADDR_ASSERT(pOut->sliceSize > 0);
        }
    }

    return returnCode;
}

} // V2
} // Addr
