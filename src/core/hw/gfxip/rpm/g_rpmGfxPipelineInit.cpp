/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/internalMemMgr.h"
#include "core/hw/gfxip/graphicsPipeline.h"
#include "core/hw/gfxip/rpm/g_rpmGfxPipelineInit.h"
#include "core/hw/gfxip/rpm/g_rpmGfxPipelineBinaries.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
// Creates all graphics pipeline objects required by RsrcProcMgr.
Result CreateRpmGraphicsPipelines(
    GfxDevice*         pDevice,
    GraphicsPipeline** pPipelineMem)
{
    Result result = Result::Success;

    GraphicsPipelineCreateInfo               pipeInfo         = { };
    GraphicsPipelineInternalCreateInfo       internalInfo     = { };
    const GraphicsPipelineInternalCreateInfo NullInternalInfo = { };

    const GpuChipProperties& properties = pDevice->Parent()->ChipProperties();

    const PipelineBinary* pTable = nullptr;

    switch (properties.revision)
    {
    case AsicRevision::Tahiti:
    case AsicRevision::Pitcairn:
    case AsicRevision::Capeverde:
        pTable = rpmGfxBinaryTableTahiti;
        break;

    case AsicRevision::Oland:
    case AsicRevision::Hainan:
    case AsicRevision::Bonaire:
    case AsicRevision::Kalindi:
    case AsicRevision::Godavari:
    case AsicRevision::Spectre:
    case AsicRevision::Spooky:
        pTable = rpmGfxBinaryTableOland;
        break;

    case AsicRevision::HawaiiPro:
    case AsicRevision::Hawaii:
        pTable = rpmGfxBinaryTableHawaiiPro;
        break;

    case AsicRevision::Carrizo:
    case AsicRevision::Bristol:
    case AsicRevision::Stoney:
    case AsicRevision::Fiji:
    case AsicRevision::Polaris10:
    case AsicRevision::Polaris11:
    case AsicRevision::Polaris12:
        pTable = rpmGfxBinaryTableCarrizo;
        break;

    case AsicRevision::Iceland:
    case AsicRevision::Tonga:
        pTable = rpmGfxBinaryTableIceland;
        break;

    case AsicRevision::Vega10:
    case AsicRevision::Raven:
        pTable = rpmGfxBinaryTableVega10;
        break;

    case AsicRevision::Vega12:
        pTable = rpmGfxBinaryTableVega12;
        break;

    case AsicRevision::Vega20:
        pTable = rpmGfxBinaryTableVega20;
        break;

    case AsicRevision::Raven2:
        pTable = rpmGfxBinaryTableRaven2;
        break;

    case AsicRevision::Navi10:
        pTable = rpmGfxBinaryTableNavi10;
        break;

    default:
        result = Result::ErrorUnknown;
        PAL_NOT_IMPLEMENTED();
        break;
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[Copy2xMsaaDepth].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[Copy2xMsaaDepth].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy2xMsaaDepth],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[Copy2xMsaaDepthStencil].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[Copy2xMsaaDepthStencil].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy2xMsaaDepthStencil],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[Copy2xMsaaStencil].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[Copy2xMsaaStencil].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy2xMsaaStencil],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[Copy4xMsaaDepth].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[Copy4xMsaaDepth].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy4xMsaaDepth],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[Copy4xMsaaDepthStencil].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[Copy4xMsaaDepthStencil].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy4xMsaaDepthStencil],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[Copy4xMsaaStencil].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[Copy4xMsaaStencil].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy4xMsaaStencil],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[Copy8xMsaaDepth].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[Copy8xMsaaDepth].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy8xMsaaDepth],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[Copy8xMsaaDepthStencil].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[Copy8xMsaaDepthStencil].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy8xMsaaDepthStencil],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[Copy8xMsaaStencil].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[Copy8xMsaaStencil].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy8xMsaaStencil],
            AllocInternal);
    }

    if ((properties.gfxLevel >= GfxIpLevel::GfxIp8) && (result == Result::Success))
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[DccDecompress].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[DccDecompress].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        internalInfo = { };
        internalInfo.flags.dccDecompress = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[DccDecompress],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[DepthExpand].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[DepthExpand].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[DepthExpand],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[DepthResummarize].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[DepthResummarize].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[DepthResummarize],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[DepthSlowDraw].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[DepthSlowDraw].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[DepthSlowDraw],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[FastClearElim].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[FastClearElim].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        internalInfo = { };
        internalInfo.flags.fastClearElim = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[FastClearElim],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[FmaskDecompress].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[FmaskDecompress].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        internalInfo = { };
        internalInfo.flags.fmaskDecompress = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[FmaskDecompress],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[Copy_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[Copy_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[Copy_32GR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[Copy_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[Copy_32R].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[Copy_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[Copy_FP16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[Copy_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[Copy_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[Copy_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[Copy_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[Copy_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[Copy_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[Copy_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[Copy_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[Copy_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ResolveFixedFunc_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ResolveFixedFunc_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        internalInfo = { };
        internalInfo.flags.resolveFixedFunc = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[ResolveFixedFunc_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ResolveFixedFunc_32GR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ResolveFixedFunc_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        internalInfo = { };
        internalInfo.flags.resolveFixedFunc = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[ResolveFixedFunc_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ResolveFixedFunc_32R].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ResolveFixedFunc_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        internalInfo = { };
        internalInfo.flags.resolveFixedFunc = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[ResolveFixedFunc_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ResolveFixedFunc_FP16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ResolveFixedFunc_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        internalInfo = { };
        internalInfo.flags.resolveFixedFunc = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[ResolveFixedFunc_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ResolveFixedFunc_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ResolveFixedFunc_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        internalInfo = { };
        internalInfo.flags.resolveFixedFunc = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[ResolveFixedFunc_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ResolveFixedFunc_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ResolveFixedFunc_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        internalInfo = { };
        internalInfo.flags.resolveFixedFunc = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[ResolveFixedFunc_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ResolveFixedFunc_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ResolveFixedFunc_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        internalInfo = { };
        internalInfo.flags.resolveFixedFunc = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[ResolveFixedFunc_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ResolveFixedFunc_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ResolveFixedFunc_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        internalInfo = { };
        internalInfo.flags.resolveFixedFunc = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[ResolveFixedFunc_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ScaledCopy2d_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ScaledCopy2d_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy2d_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ScaledCopy2d_32GR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ScaledCopy2d_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy2d_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ScaledCopy2d_32R].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ScaledCopy2d_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy2d_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ScaledCopy2d_FP16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ScaledCopy2d_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy2d_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ScaledCopy2d_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ScaledCopy2d_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy2d_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ScaledCopy2d_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ScaledCopy2d_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy2d_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ScaledCopy2d_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ScaledCopy2d_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy2d_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ScaledCopy2d_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ScaledCopy2d_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy2d_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ScaledCopy3d_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ScaledCopy3d_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy3d_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ScaledCopy3d_32GR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ScaledCopy3d_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy3d_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ScaledCopy3d_32R].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ScaledCopy3d_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy3d_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ScaledCopy3d_FP16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ScaledCopy3d_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy3d_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ScaledCopy3d_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ScaledCopy3d_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy3d_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ScaledCopy3d_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ScaledCopy3d_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy3d_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ScaledCopy3d_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ScaledCopy3d_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy3d_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ScaledCopy3d_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ScaledCopy3d_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy3d_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear0_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear0_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear0_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear0_32GR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear0_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear0_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear0_32R].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear0_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear0_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear0_FP16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear0_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear0_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear0_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear0_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear0_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear0_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear0_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear0_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear0_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear0_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear0_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear0_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear0_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear0_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear1_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear1_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[1].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[1].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[1].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear1_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear1_32GR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear1_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[1].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[1].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[1].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear1_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear1_32R].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear1_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[1].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[1].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[1].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear1_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear1_FP16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear1_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[1].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[1].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[1].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear1_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear1_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear1_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[1].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[1].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[1].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear1_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear1_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear1_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[1].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[1].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[1].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear1_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear1_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear1_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[1].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[1].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[1].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear1_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear1_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear1_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[1].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[1].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[1].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear1_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear2_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear2_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[2].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[2].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[2].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear2_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear2_32GR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear2_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[2].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[2].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[2].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear2_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear2_32R].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear2_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[2].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[2].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[2].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear2_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear2_FP16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear2_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[2].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[2].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[2].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear2_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear2_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear2_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[2].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[2].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[2].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear2_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear2_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear2_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[2].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[2].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[2].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear2_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear2_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear2_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[2].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[2].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[2].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear2_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear2_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear2_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[2].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[2].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[2].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear2_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear3_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear3_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[3].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[3].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[3].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear3_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear3_32GR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear3_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[3].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[3].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[3].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear3_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear3_32R].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear3_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[3].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[3].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[3].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear3_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear3_FP16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear3_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[3].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[3].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[3].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear3_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear3_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear3_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[3].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[3].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[3].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear3_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear3_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear3_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[3].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[3].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[3].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear3_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear3_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear3_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[3].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[3].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[3].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear3_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear3_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear3_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[3].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[3].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[3].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear3_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear4_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear4_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[4].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[4].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[4].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear4_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear4_32GR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear4_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[4].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[4].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[4].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear4_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear4_32R].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear4_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[4].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[4].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[4].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear4_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear4_FP16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear4_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[4].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[4].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[4].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear4_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear4_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear4_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[4].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[4].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[4].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear4_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear4_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear4_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[4].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[4].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[4].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear4_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear4_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear4_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[4].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[4].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[4].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear4_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear4_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear4_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[4].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[4].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[4].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear4_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear5_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear5_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[5].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[5].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[5].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear5_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear5_32GR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear5_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[5].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[5].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[5].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear5_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear5_32R].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear5_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[5].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[5].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[5].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear5_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear5_FP16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear5_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[5].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[5].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[5].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear5_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear5_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear5_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[5].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[5].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[5].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear5_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear5_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear5_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[5].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[5].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[5].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear5_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear5_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear5_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[5].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[5].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[5].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear5_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear5_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear5_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[5].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[5].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[5].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear5_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear6_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear6_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[6].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[6].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[6].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear6_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear6_32GR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear6_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[6].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[6].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[6].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear6_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear6_32R].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear6_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[6].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[6].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[6].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear6_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear6_FP16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear6_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[6].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[6].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[6].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear6_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear6_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear6_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[6].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[6].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[6].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear6_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear6_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear6_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[6].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[6].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[6].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear6_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear6_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear6_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[6].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[6].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[6].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear6_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear6_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear6_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[6].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[6].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[6].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear6_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear7_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear7_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[7].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[7].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[7].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear7_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear7_32GR].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear7_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[7].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[7].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[7].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear7_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear7_32R].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear7_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[7].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[7].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[7].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear7_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear7_FP16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear7_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[7].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[7].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[7].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear7_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear7_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear7_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[7].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[7].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[7].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear7_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear7_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear7_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[7].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[7].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[7].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear7_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear7_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear7_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[7].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[7].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[7].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear7_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[SlowColorClear7_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[SlowColorClear7_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[7].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[7].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[7].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear7_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ResolveDepth].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ResolveDepth].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ResolveDepth],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ResolveDepthCopy].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ResolveDepthCopy].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32_Float;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp           = LogicOp::Copy;
        pipeInfo.rsState.binningOverride   = BinningOverride::Disable;
        pipeInfo.rsState.depthClampDisable = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ResolveDepthCopy],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ResolveStencil].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ResolveStencil].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ResolveStencil],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary    = pTable[ResolveStencilCopy].pBuffer;
        pipeInfo.pipelineBinarySize = pTable[ResolveStencilCopy].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x2;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ResolveStencilCopy],
            AllocInternal);
    }

    return result;
}

// Make sure interface changes have been propagated to RPM.
static_assert((
    (offsetof(StencilRefMaskParams, frontRef)       == 0) &&
    (offsetof(StencilRefMaskParams, frontReadMask)  == 1) &&
    (offsetof(StencilRefMaskParams, frontWriteMask) == 2) &&
    (offsetof(StencilRefMaskParams, frontOpValue)   == 3) &&
    (offsetof(StencilRefMaskParams, backRef)        == 4) &&
    (offsetof(StencilRefMaskParams, backReadMask)   == 5) &&
    (offsetof(StencilRefMaskParams, backWriteMask)  == 6) &&
    (offsetof(StencilRefMaskParams, backOpValue)    == 7) &&
    (offsetof(StencilRefMaskParams, flags)          == 8)),
    "StencilRefMaskParams interface change not propagated. Update this file to match interface changes.");

static_assert((offsetof(InputAssemblyStateParams, topology) == 0),
    "PrimitiveTopologyParams interface change not propagated. Update this file to match interface changes.");

static_assert((
    (offsetof(DepthBiasParams, depthBias)            == 0) &&
    (offsetof(DepthBiasParams, depthBiasClamp)       == sizeof(float)) &&
    (offsetof(DepthBiasParams, slopeScaledDepthBias) == (2 * sizeof(float)))),
    "DepthBiasParams interface change not propagated. Update this file to match interface changes.");

static_assert((
    (offsetof(PointLineRasterStateParams, pointSize) == 0) &&
    (offsetof(PointLineRasterStateParams, lineWidth) == sizeof(float))),
    "PointLineRasterStateParams interface change not propagated. Update this file to match interface changes.");

static_assert((
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 524
    (offsetof(TriangleRasterStateParams, fillMode)        == 0) &&
#else
    (offsetof(TriangleRasterStateParams, frontFillMode)   == 0) &&
#endif
    (offsetof(TriangleRasterStateParams, backFillMode)    == sizeof(Pal::FillMode)) &&
    (offsetof(TriangleRasterStateParams, cullMode)        == 2 * sizeof(Pal::FillMode)) &&
    (offsetof(TriangleRasterStateParams, frontFace)       == 2 * sizeof(Pal::FillMode) + sizeof(Pal::CullMode)) &&
    (offsetof(TriangleRasterStateParams, provokingVertex) == 2 * sizeof(Pal::FillMode) +
                                                             sizeof(Pal::CullMode) +
                                                             sizeof(Pal::FaceOrientation))),
    "TriangleRasterStateParams interface change not propagated. Update this file to match interface changes.");

} // Pal
