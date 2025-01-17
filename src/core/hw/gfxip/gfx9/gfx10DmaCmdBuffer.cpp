/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/cmdStream.h"
#include "core/device.h"
#include "core/settingsLoader.h"
#include "core/hw/gfxip/gfx9/gfx9FormatInfo.h"
#include "core/hw/gfxip/gfx9/gfx9Image.h"
#include "core/hw/gfxip/gfx9/gfx10DmaCmdBuffer.h"
#include "core/addrMgr/addrMgr2/addrMgr2.h"
#include "chip/gfx10_merged_sdma_packets.h"
#include "marker_payload.h"
#include "palFormatInfo.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

constexpr size_t NopSizeDwords = sizeof(SDMA_PKT_NOP) / sizeof(uint32);

// The SDMA_PKT_WRITE_UNTILED definition contains space for one dword of data.  To make things a little simpler, we
// consider the packetHeader size to be the packet size without any associated data.
constexpr uint32 UpdateMemoryPacketHdrSizeInDwords = (sizeof(SDMA_PKT_WRITE_UNTILED) / sizeof(uint32)) - 1;

// =====================================================================================================================
DmaCmdBuffer::DmaCmdBuffer(
    Device&                    device,
    const CmdBufferCreateInfo& createInfo)
    :
    Pal::DmaCmdBuffer(device.Parent(), createInfo, 0)
{
    // Regarding copyOverlapHazardSyncs value in the constructor above:
    //   While GFX10 may execute sequences of small copies/writes asynchronously, the hardware should
    //   have automatic detection of hazards between these copies based on VA range comparison, so the
    //   driver does not itself need to do any manual synchronization.

    // Temporary note: The above description is not correct at the moment: there is a likely HW bug with the
    // the copy overlap feature and it is temporarily disabled.
}

// =====================================================================================================================
// Writes a packet that waits for the given GPU event to be set. Returns the next unused DWORD in pCmdSpace.
//
uint32* DmaCmdBuffer::WriteWaitEventSet(
    const GpuEvent& gpuEvent,
    uint32*         pCmdSpace
    ) const
{
    const size_t  packetDwords = Util::NumBytesToNumDwords(sizeof(SDMA_PKT_POLL_REGMEM));
    auto*const    pPacket      = reinterpret_cast<SDMA_PKT_POLL_REGMEM*>(pCmdSpace);
    const gpusize gpuVirtAddr  = gpuEvent.GetBoundGpuMemory().GpuVirtAddr();

    SDMA_PKT_POLL_REGMEM packet;

    packet.HEADER_UNION.DW_0_DATA   = 0;
    packet.HEADER_UNION.op          = SDMA_OP_POLL_REGMEM;
    packet.HEADER_UNION.func        = 0x3;                    // Equal
    packet.HEADER_UNION.mem_poll    = 1;                      // Memory space poll.

    packet.ADDR_LO_UNION.addr_31_0  = LowPart(gpuVirtAddr);
    packet.ADDR_HI_UNION.addr_63_32 = HighPart(gpuVirtAddr);

    packet.VALUE_UNION.value        = GpuEvent::SetValue;
    packet.MASK_UNION.mask          = 0xFFFFFFFF;

    packet.DW5_UNION.DW_5_DATA      = 0;
    packet.DW5_UNION.interval       = 0xA;                    // Wait 160 clocks before each retry.
    packet.DW5_UNION.retry_count    = 0xFFF;                  // Retry infinitely.

    *pPacket = packet;

    return pCmdSpace + packetDwords;
}

// =====================================================================================================================
// Helper function for writing the current GPU timestamp value into the specified memory.
void DmaCmdBuffer::WriteTimestampCmd(
    gpusize dstAddr)
{
    //     No need to issue a Fence prior to the timestamp command. The Timestamp itself can ensure previous commands
    //     all completed.

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    const size_t packetDwords = Util::NumBytesToNumDwords(sizeof(SDMA_PKT_TIMESTAMP_GET_GLOBAL));
    auto*const   pPacket      = reinterpret_cast<SDMA_PKT_TIMESTAMP_GET_GLOBAL*>(pCmdSpace);

    SDMA_PKT_TIMESTAMP_GET_GLOBAL packet;

    packet.HEADER_UNION.DW_0_DATA               = 0;
    packet.HEADER_UNION.op                      = SDMA_OP_TIMESTAMP;
    packet.HEADER_UNION.sub_op                  = SDMA_SUBOP_TIMESTAMP_GET_GLOBAL;
    packet.WRITE_ADDR_LO_UNION.DW_1_DATA        = LowPart(dstAddr);
    packet.WRITE_ADDR_HI_UNION.write_addr_63_32 = HighPart(dstAddr);

    *pPacket = packet;

    m_cmdStream.CommitCommands(pCmdSpace + packetDwords);
}

// =====================================================================================================================
// Writes the current GPU timestamp value into the specified memory.
void DmaCmdBuffer::CmdWriteTimestamp(
    HwPipePoint       pipePoint,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    const GpuMemory& gpuMemory = static_cast<const GpuMemory&>(dstGpuMemory);
    const gpusize    dstAddr   = gpuMemory.Desc().gpuVirtAddr + dstOffset;

    WriteTimestampCmd(dstAddr);
}

// =====================================================================================================================
// Adds a preamble to the start of a new command buffer.
Result DmaCmdBuffer::AddPreamble()
{
    // If this trips, it means that this isn't really the preamble -- i.e., somebody has inserted something into the
    // command stream before the preamble.  :-(
    PAL_ASSERT(m_cmdStream.IsEmpty());

    // Adding a NOP preamble ensures that we always have something to submit (i.e,. the app can't submit an empty
    // command buffer which causes problems to the submit routine).
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    pCmdSpace = WriteNops(pCmdSpace, 1);

    m_cmdStream.CommitCommands(pCmdSpace);

    return Result::Success;
}

// =====================================================================================================================
// Build a NOP packet.
uint32* DmaCmdBuffer::BuildNops(
    uint32* pCmdSpace,
    uint32  numDwords)
{
    // Starting with OSS4, the NOP packet is variable length.  Note that the count field is the size of the body of the
    // NOP excluding the 1 dword packet header.
    SDMA_PKT_NOP packet       = { };
    packet.HEADER_UNION.op    = SDMA_OP_NOP;
    packet.HEADER_UNION.count = numDwords - 1;

    *reinterpret_cast<SDMA_PKT_NOP*>(pCmdSpace) = packet;

    return pCmdSpace + numDwords;
}

// =====================================================================================================================
// Adds a postamble to the end of a new command buffer. This will add a mem_incr packet to increment the completion
// count of the command buffer when the GPU has finished executing it.
Result DmaCmdBuffer::AddPostamble()
{

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    if (m_cmdStream.GetFirstChunk()->BusyTrackerGpuAddr() != 0)
    {
        const gpusize gpuAddr = m_cmdStream.GetFirstChunk()->BusyTrackerGpuAddr();
        auto*const    pPacket = reinterpret_cast<SDMA_PKT_MEM_INCR*>(pCmdSpace);

        SDMA_PKT_MEM_INCR packet = {};

        // The GPU address for mem_incr must be 8 byte aligned.
        constexpr uint32 SemaphoreAlign = 8;
        PAL_ASSERT(Pow2Align(gpuAddr, SemaphoreAlign) == gpuAddr);

        packet.HEADER_UNION.op          = SDMA_OP_SEM;
        packet.HEADER_UNION.sub_op      = SDMA_SUBOP_MEM_INCR;
        packet.ADDR_LO_UNION.addr_31_0  = LowPart(gpuAddr);
        packet.ADDR_HI_UNION.addr_63_32 = HighPart(gpuAddr);

        *pPacket  = packet;
        pCmdSpace = reinterpret_cast<uint32*>(pPacket + 1);
    }

    m_cmdStream.CommitCommands(pCmdSpace);

    return Result::Success;
}

// =====================================================================================================================
void DmaCmdBuffer::BeginExecutionMarker(
    uint64 clientHandle)
{
    CmdBuffer::BeginExecutionMarker(clientHandle);
    PAL_ASSERT(m_executionMarkerAddr != 0);

    CmdWriteImmediate(HwPipePoint::HwPipeBottom,
                      m_executionMarkerCount,
                      ImmediateDataWidth::ImmediateData32Bit,
                      m_executionMarkerAddr);

    constexpr size_t BeginPayloadSize = sizeof(RgdExecutionBeginMarker) / sizeof(uint32);

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();
    BuildNops(pCmdSpace, BeginPayloadSize + NopSizeDwords);

    auto* pPayload          = reinterpret_cast<RgdExecutionBeginMarker*>(pCmdSpace + NopSizeDwords);
    pPayload->guard         = RGD_EXECUTION_BEGIN_MARKER_GUARD;
    pPayload->marker_buffer = m_executionMarkerAddr;
    pPayload->client_handle = clientHandle;
    pPayload->counter       = m_executionMarkerCount;

    pCmdSpace += BeginPayloadSize + NopSizeDwords;
    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
uint32 DmaCmdBuffer::CmdInsertExecutionMarker()
{
    uint32 returnVal = UINT_MAX;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 533
    if (m_buildFlags.enableExecutionMarkerSupport == 1)
    {
        PAL_ASSERT(m_executionMarkerAddr != 0);
        CmdWriteImmediate(HwPipePoint::HwPipeBottom,
                          ++m_executionMarkerCount,
                          ImmediateDataWidth::ImmediateData32Bit,
                          m_executionMarkerAddr);

        constexpr size_t MarkerPayloadSize = sizeof(RgdExecutionMarker) / sizeof(uint32);

        uint32* pCmdSpace = m_cmdStream.ReserveCommands();
        BuildNops(pCmdSpace, MarkerPayloadSize + NopSizeDwords);

        auto* pPayload          = reinterpret_cast<RgdExecutionMarker*>(pCmdSpace + NopSizeDwords);
        pPayload->guard         = RGD_EXECUTION_MARKER_GUARD;
        pPayload->counter       = m_executionMarkerCount;

        pCmdSpace += MarkerPayloadSize + NopSizeDwords;
        m_cmdStream.CommitCommands(pCmdSpace);

        returnVal = m_executionMarkerCount;
    }
#endif
    return returnVal;
}

// =====================================================================================================================
void DmaCmdBuffer::EndExecutionMarker()
{
    PAL_ASSERT(m_executionMarkerAddr != 0);
    CmdWriteImmediate(HwPipePoint::HwPipeBottom,
                      ++m_executionMarkerCount,
                      ImmediateDataWidth::ImmediateData32Bit,
                      m_executionMarkerAddr);

    constexpr size_t EndPayloadSize = sizeof(RgdExecutionEndMarker) / sizeof(uint32);

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();
    BuildNops(pCmdSpace, EndPayloadSize + NopSizeDwords);

    auto* pPayload          = reinterpret_cast<RgdExecutionEndMarker*>(pCmdSpace + NopSizeDwords);
    pPayload->guard         = RGD_EXECUTION_END_MARKER_GUARD;
    pPayload->counter       = m_executionMarkerCount;

    pCmdSpace += EndPayloadSize + NopSizeDwords;
    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Writes a COND_EXE packet to predicate the next packets based on a memory value. Returns the next unused DWORD in
// pCmdSpace.
//
uint32* DmaCmdBuffer::WritePredicateCmd(
    size_t  predicateDwords,
    uint32* pCmdSpace
    ) const
{
    const size_t packetDwords = Util::NumBytesToNumDwords(sizeof(SDMA_PKT_COND_EXE));
    auto*const   pPacket      = reinterpret_cast<SDMA_PKT_COND_EXE*>(pCmdSpace);

    SDMA_PKT_COND_EXE packet;

    PAL_NOT_TESTED();

    packet.HEADER_UNION.DW_0_DATA      = 0;
    packet.HEADER_UNION.op             = SDMA_OP_COND_EXE;
    packet.ADDR_LO_UNION.addr_31_0     = LowPart(m_predMemAddress);
    packet.ADDR_HI_UNION.addr_63_32    = HighPart(m_predMemAddress);
    packet.REFERENCE_UNION.reference   = 1;
    packet.EXEC_COUNT_UNION.DW_4_DATA  = 0;
    packet.EXEC_COUNT_UNION.exec_count = predicateDwords;

    *pPacket = packet;

    return pCmdSpace + packetDwords;
}

// =====================================================================================================================
// Patches a COND_EXE packet with the given predication size.
//
void DmaCmdBuffer::PatchPredicateCmd(
    size_t predicateDwords,
    void*  pPredicateCmd
    ) const
{
    auto*const pPacket = reinterpret_cast<SDMA_PKT_COND_EXE*>(pPredicateCmd);

    pPacket->EXEC_COUNT_UNION.exec_count = predicateDwords;
}

// =====================================================================================================================
// Copies "copySize" bytes from srcAddr to dstAddr. This function will transfer as much as it can, but it is the
// caller's responsibility to keep calling this function until all the requested data has been copied. Returns the next
// unused DWORD in pCmdSpace.
//
uint32* DmaCmdBuffer::WriteCopyGpuMemoryCmd(
    gpusize      srcGpuAddr,
    gpusize      dstGpuAddr,
    gpusize      copySize,
    DmaCopyFlags copyFlags,
    uint32*      pCmdSpace,
    gpusize*     pBytesCopied // [out] How many bytes out of copySize this call was able to transfer.
    ) const
{
    // The count field of the copy packet is 22 bits wide.
    const uint32  maxCopyBits = 22;
    const gpusize maxCopySize = (1ull << maxCopyBits);

    *pBytesCopied = Min(copySize, maxCopySize);

    if (IsPow2Aligned(srcGpuAddr, sizeof(uint32)) &&
        IsPow2Aligned(dstGpuAddr, sizeof(uint32)) &&
        (*pBytesCopied >= sizeof(uint32)))
    {
        // If the source and destination are dword aligned and the size is at least one DWORD, then go ahead and do
        // DWORD copies.  Note that the SDMA microcode makes the switch between byte and DWORD copies automagically,
        // depending on the addresses being dword aligned and the size being a dword multiple.
        *pBytesCopied = Pow2AlignDown(*pBytesCopied, sizeof(uint32));
    }

    const size_t packetDwords = Util::NumBytesToNumDwords(sizeof(SDMA_PKT_COPY_LINEAR));
    auto*const   pPacket      = reinterpret_cast<SDMA_PKT_COPY_LINEAR*>(pCmdSpace);

    SDMA_PKT_COPY_LINEAR packet;

    packet.HEADER_UNION.DW_0_DATA           = 0;
    packet.HEADER_UNION.op                  = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op              = SDMA_SUBOP_COPY_LINEAR;
    packet.COUNT_UNION.DW_1_DATA            = 0;

    {
        packet.COUNT_UNION.nv10.count      = *pBytesCopied - 1;
    }

    packet.PARAMETER_UNION.DW_2_DATA        = 0;

    packet.SRC_ADDR_LO_UNION.src_addr_31_0  = LowPart(srcGpuAddr);
    packet.SRC_ADDR_HI_UNION.src_addr_63_32 = HighPart(srcGpuAddr);
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(dstGpuAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(dstGpuAddr);

    *pPacket = packet;

    return pCmdSpace + packetDwords;
}

// =====================================================================================================================
// Copies memory into the specified region of a typed buffer (linear image). Returns the next unused DWORD in pCmdSpace.
//
uint32* DmaCmdBuffer::WriteCopyTypedBuffer(
    const DmaTypedBufferCopyInfo& typedBufferInfo,
    uint32* pCmdSpace
    ) const
{
    const size_t packetDwords = Util::NumBytesToNumDwords(sizeof(SDMA_PKT_COPY_LINEAR_SUBWIN));
    auto*const   pPacket      = reinterpret_cast<SDMA_PKT_COPY_LINEAR_SUBWIN*>(pCmdSpace);

    SDMA_PKT_COPY_LINEAR_SUBWIN packet;

    packet.HEADER_UNION.DW_0_DATA     = 0;
    packet.HEADER_UNION.op            = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op        = SDMA_SUBOP_COPY_LINEAR_SUB_WIND;
    packet.HEADER_UNION.elementsize   = Log2(typedBufferInfo.dst.bytesPerElement);

    // Setup the source base address.
    packet.SRC_ADDR_LO_UNION.src_addr_31_0    = LowPart(typedBufferInfo.src.baseAddr);
    packet.SRC_ADDR_HI_UNION.src_addr_63_32   = HighPart(typedBufferInfo.src.baseAddr);

    // Setup the start of the source rect.
    // Offset is 0 since the base address is the actual address of the sub-region
    packet.DW_3_UNION.DW_3_DATA   = 0;
    packet.DW_4_UNION.DW_4_DATA   = 0;

    // Setup the source surface dimensions.
    packet.DW_4_UNION.src_pitch           = typedBufferInfo.src.linearRowPitch - 1;
    packet.DW_5_UNION.DW_5_DATA           = 0;
    packet.DW_5_UNION.src_slice_pitch     = typedBufferInfo.src.linearDepthPitch - 1;

    // Setup the destination base address.
    packet.DST_ADDR_LO_UNION.dst_addr_31_0    = LowPart(typedBufferInfo.dst.baseAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32   = HighPart(typedBufferInfo.dst.baseAddr);

    // Setup the start of the destination rectangle.
    // Offset is 0 since the base address is the actual address of the sub-region
    packet.DW_8_UNION.DW_8_DATA   = 0;
    packet.DW_9_UNION.DW_9_DATA   = 0;

    // Setup the destination surface dimensions.
    // The unit of linear pitch and linear slice is pixel number minus 1.
    packet.DW_9_UNION.dst_pitch           = typedBufferInfo.dst.linearRowPitch - 1;
    packet.DW_10_UNION.DW_10_DATA         = 0;
    packet.DW_10_UNION.dst_slice_pitch    = typedBufferInfo.dst.linearDepthPitch - 1;

    // Setup the rectangle dimensions.
    // rect_dx/dy (14b),  rect_dz(11b): rectangle width/height/depth minus 1.
    packet.DW_11_UNION.DW_11_DATA     = 0;
    packet.DW_11_UNION.rect_x         = typedBufferInfo.copyExtent.width - 1;
    packet.DW_11_UNION.rect_y         = typedBufferInfo.copyExtent.height - 1;
    packet.DW_12_UNION.DW_12_DATA     = 0;
    packet.DW_12_UNION.rect_z         = typedBufferInfo.copyExtent.depth - 1;

    *pPacket = packet;

    return pCmdSpace + packetDwords;
}

// =====================================================================================================================
// On GFX10 parts, we always program the base address to point at slice 0.  This means the "z" coordinate (for images
// that have slices) needs to specify the starting slice number.
uint32 DmaCmdBuffer::GetImageZ(
    const DmaImageInfo&  dmaImageInfo,
    uint32               offsetZ
    ) const
{
    const ImageType  imageType = GetImageType(*dmaImageInfo.pImage);
    uint32           imageZ    = 0;

    if (imageType == ImageType::Tex3d)
    {
        // 3D images can't have array slices, so just return the "z" offset.
        PAL_ASSERT(dmaImageInfo.pSubresInfo->subresId.arraySlice == 0);

        imageZ = offsetZ;
    }
    else
    {
        // For 2D image array, offsetZ represents the sliceIndex counted from the "start slice" whose base address
        // is DmaImageInfo::baseAddr, which is used by gfx6-gfx8. For gfx10, just ignore offsetZ and adopt the
        // start sliceIndex counted from "0".
        imageZ = dmaImageInfo.pSubresInfo->subresId.arraySlice;
    }

    return imageZ;
}

// =====================================================================================================================
// Copies the specified region between two linear images.
//
void DmaCmdBuffer::WriteCopyImageLinearToLinearCmd(
    const DmaImageCopyInfo& imageCopyInfo)
{
    uint32*      pCmdSpace    = m_cmdStream.ReserveCommands();
    const size_t packetDwords = Util::NumBytesToNumDwords(sizeof(SDMA_PKT_COPY_LINEAR_SUBWIN));
    auto*const   pPacket      = reinterpret_cast<SDMA_PKT_COPY_LINEAR_SUBWIN*>(pCmdSpace);

    SDMA_PKT_COPY_LINEAR_SUBWIN packet;

    packet.HEADER_UNION.DW_0_DATA   = 0;
    packet.HEADER_UNION.op          = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op      = SDMA_SUBOP_COPY_LINEAR_SUB_WIND;
    packet.HEADER_UNION.elementsize = Log2(imageCopyInfo.dst.bytesPerPixel);

    // Base addresses should be dword aligned.
    PAL_ASSERT (((imageCopyInfo.src.baseAddr & 0x3) == 0) && ((imageCopyInfo.dst.baseAddr & 0x3) == 0));

    // Setup the source base address.
    packet.SRC_ADDR_LO_UNION.src_addr_31_0  = LowPart(imageCopyInfo.src.baseAddr);
    packet.SRC_ADDR_HI_UNION.src_addr_63_32 = HighPart(imageCopyInfo.src.baseAddr);

    // Setup the start of the source rect.
    packet.DW_3_UNION.DW_3_DATA = 0;
    packet.DW_3_UNION.src_x     = imageCopyInfo.src.offset.x;
    packet.DW_3_UNION.src_y     = imageCopyInfo.src.offset.y;
    packet.DW_4_UNION.DW_4_DATA = 0;
    packet.DW_4_UNION.src_z     = GetImageZ(imageCopyInfo.src);

    // Setup the source surface dimensions.
    packet.DW_4_UNION.src_pitch       = GetLinearRowPitch(imageCopyInfo.src);
    packet.DW_5_UNION.DW_5_DATA       = 0;
    packet.DW_5_UNION.src_slice_pitch = GetLinearDepthPitch(imageCopyInfo.src);

    // Setup the destination base address.
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(imageCopyInfo.dst.baseAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(imageCopyInfo.dst.baseAddr);

    // Setup the start of the destination rectangle.
    packet.DW_8_UNION.DW_8_DATA = 0;
    packet.DW_8_UNION.dst_x     = imageCopyInfo.dst.offset.x;
    packet.DW_8_UNION.dst_y     = imageCopyInfo.dst.offset.y;
    packet.DW_9_UNION.DW_9_DATA = 0;
    packet.DW_9_UNION.dst_z     = GetImageZ(imageCopyInfo.dst);

    // Setup the destination surface dimensions.
    packet.DW_9_UNION.dst_pitch        = GetLinearRowPitch(imageCopyInfo.dst);
    packet.DW_10_UNION.DW_10_DATA      = 0;
    packet.DW_10_UNION.dst_slice_pitch = GetLinearDepthPitch(imageCopyInfo.dst);

    /// Setup the rectangle dimensions.
    packet.DW_11_UNION.DW_11_DATA = 0;
    packet.DW_11_UNION.rect_x     = imageCopyInfo.copyExtent.width  - 1;
    packet.DW_11_UNION.rect_y     = imageCopyInfo.copyExtent.height - 1;
    packet.DW_12_UNION.DW_12_DATA = 0;
    packet.DW_12_UNION.rect_z     = imageCopyInfo.copyExtent.depth  - 1;

    *pPacket = packet;

    m_cmdStream.CommitCommands(pCmdSpace + packetDwords);
}

// =====================================================================================================================
// Linear image to tiled image copy
void DmaCmdBuffer:: WriteCopyImageLinearToTiledCmd(
    const DmaImageCopyInfo& imageCopyInfo)
{
    uint32*  pCmdSpace = m_cmdStream.ReserveCommands();

    pCmdSpace = CopyImageLinearTiledTransform(imageCopyInfo, imageCopyInfo.src, imageCopyInfo.dst, false, pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Tiled image to linear image copy
void DmaCmdBuffer::WriteCopyImageTiledToLinearCmd(
    const DmaImageCopyInfo& imageCopyInfo)
{
    uint32*  pCmdSpace = m_cmdStream.ReserveCommands();

    pCmdSpace = CopyImageLinearTiledTransform(imageCopyInfo, imageCopyInfo.dst, imageCopyInfo.src, true, pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Returns true if the supplied image has any meta-data associated with it.
bool DmaCmdBuffer::ImageHasMetaData(
    const DmaImageInfo& imageInfo)
{
    const Pal::Image*  pPalImage = static_cast<const Pal::Image*>(imageInfo.pImage);
    const Image*       pGfxImage = static_cast<const Image*>(pPalImage->GetGfxImage());

    PAL_ASSERT((pGfxImage->HasDsMetadata() == false) ||
               (pPalImage->GetDevice()->GetPlatform()->IsEmulationEnabled() == false));

    return (pGfxImage->HasDccData() || pGfxImage->HasDsMetadata());
}

// =====================================================================================================================
// Tiled image to tiled image copy.
//
void DmaCmdBuffer::WriteCopyImageTiledToTiledCmd(
    const DmaImageCopyInfo& imageCopyInfo)
{
    const auto& src        = imageCopyInfo.src;
    const auto& dst        = imageCopyInfo.dst;
    const auto  srcSwizzle = GetSwizzleMode(src);
    const auto  dstSwizzle = GetSwizzleMode(dst);
    const auto& settings   = GetGfx9Settings(*m_pDevice);

    uint32*      pCmdSpace    = m_cmdStream.ReserveCommands();
    const size_t packetDwords = Util::NumBytesToNumDwords(sizeof(SDMA_PKT_COPY_T2T));

    SDMA_PKT_COPY_T2T packet = {};

    // Packet header
    packet.HEADER_UNION.DW_0_DATA = 0;
    packet.HEADER_UNION.op        = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op    = SDMA_SUBOP_COPY_T2T_SUB_WIND;

    // Setup the start, offset, and dimenions of the source surface.
    packet.SRC_ADDR_LO_UNION.src_addr_31_0  = LowPart(src.baseAddr);
    packet.SRC_ADDR_HI_UNION.src_addr_63_32 = HighPart(src.baseAddr);

    packet.DW_3_UNION.DW_3_DATA   = 0;
    packet.DW_3_UNION.src_x       = src.offset.x;
    packet.DW_3_UNION.src_y       = src.offset.y;
    packet.DW_4_UNION.DW_4_DATA   = 0;
    packet.DW_4_UNION.src_z       = GetImageZ(src);
    packet.DW_4_UNION.src_width   = src.extent.width - 1;
    packet.DW_5_UNION.DW_5_DATA   = 0;
    packet.DW_5_UNION.src_height  = src.extent.height - 1;
    packet.DW_5_UNION.src_depth   = src.extent.depth - 1;

    // Setup the tile mode of the destination surface.
    packet.DW_6_UNION.DW_6_DATA         = 0;
    packet.DW_6_UNION.src_element_size  = Log2(src.bytesPerPixel);
    packet.DW_6_UNION.src_swizzle_mode  = AddrMgr2::GetHwSwizzleMode(srcSwizzle);
    packet.DW_6_UNION.src_dimension     = GetHwDimension(src);

    // Setup the start, offset, and dimenions of the destination surface.
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(dst.baseAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(dst.baseAddr);

    packet.DW_9_UNION.DW_9_DATA   = 0;
    packet.DW_9_UNION.dst_x       = dst.offset.x;
    packet.DW_9_UNION.dst_y       = dst.offset.y;

    packet.DW_10_UNION.DW_10_DATA = 0;
    packet.DW_10_UNION.dst_z      = GetImageZ(dst);
    packet.DW_10_UNION.dst_width  = dst.extent.width - 1;

    packet.DW_11_UNION.DW_11_DATA  = 0;
    packet.DW_11_UNION.dst_height  = dst.extent.height - 1;
    packet.DW_11_UNION.dst_depth   = dst.extent.depth - 1;

    // Setup the tile mode of the destination surface.
    packet.DW_12_UNION.DW_12_DATA       = 0;
    packet.DW_12_UNION.dst_element_size = Log2(dst.bytesPerPixel);
    packet.DW_12_UNION.dst_swizzle_mode = AddrMgr2::GetHwSwizzleMode(dstSwizzle);
    packet.DW_12_UNION.dst_dimension    = GetHwDimension(dst);

    // Setup the size of the copy region.
    packet.DW_13_UNION.DW_13_DATA = 0;
    packet.DW_14_UNION.DW_14_DATA = 0;
    packet.DW_13_UNION.rect_x     = imageCopyInfo.copyExtent.width - 1;
    packet.DW_13_UNION.rect_y     = imageCopyInfo.copyExtent.height - 1;
    packet.DW_14_UNION.rect_z     = imageCopyInfo.copyExtent.depth - 1;

    // SDMA engine can either read a compressed source or write to a compressed destination, but not both.
    const bool  srcHasMetaData = ImageHasMetaData(src);
    const bool  dstHasMetaData = ImageHasMetaData(dst);
    const bool  metaIsSrc      = ((srcHasMetaData && dstHasMetaData && settings.sdmaPreferCompressedSource) ||
                                  (srcHasMetaData && (dstHasMetaData == false)));

    // If both surfaces are compressed and the panel requests compressed sources -or-
    // if only the source is compressed  -then-
    // setup the packet to use the source surface
    if (metaIsSrc)
    {
        SetupMetaData(src, &packet, false);
    }
    else if (dstHasMetaData)
    {
        // Just try with the dst surface here
        SetupMetaData(dst, &packet, true);
    }

    auto*const  pPacket = reinterpret_cast<SDMA_PKT_COPY_T2T*>(pCmdSpace);
    *pPacket   = packet;
    pCmdSpace += packetDwords;

    if (dstHasMetaData && (metaIsSrc == false))
    {
        // The copy packet wrote into a destination surface that has DCC / hTile, so we need to update our
        // tracking metadata to indicate that a decompression operation is useful again.
        pCmdSpace = UpdateImageMetaData(dst, pCmdSpace);
    }

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Returns true if scanline copies are required for a tiled-to-tiled image copy
bool DmaCmdBuffer::UseT2tScanlineCopy(
    const DmaImageCopyInfo& imageCopyInfo
    ) const
{
    const auto&            src           = imageCopyInfo.src;
    const auto&            dst           = imageCopyInfo.dst;
    const auto&            srcCreateInfo = src.pImage->GetImageCreateInfo();
    const auto&            dstCreateInfo = dst.pImage->GetImageCreateInfo();
    const AddrSwizzleMode  srcSwizzle    = GetSwizzleMode(src);

    // Assume, that by some miracle, all of the requirements for using the built-in T2T copy are actually met.
    bool useScanlineCopy = false;

    if ((srcCreateInfo.mipLevels >  1) || (dstCreateInfo.mipLevels > 1))
    {
        // The built in tiled-to-tiled image copy packet not only doesn't support mip level selection, it doesn't
        // even support specifying the number of mip levels the image has.  So if either the source or the destination
        // image has more than one mip level, we can't use it.
        useScanlineCopy = true;
    }
    else
    {
        // The alignment requirements for the offsets / rectangle sizes are format and image type dependent.
        static constexpr Extent3d  CopyAlignmentsFor2d[] =
        {
            { 16, 16, 1 }, // 1bpp
            { 16,  8, 1 }, // 2bpp
            {  8,  8, 1 }, // 4bpp
            {  8,  4, 1 }, // 8bpp
            {  4,  4, 1 }, // 16bpp
        };

        static constexpr Extent3d  CopyAlignmentsFor3d[] =
        {
            { 16, 8, 8 }, // 1bpp
            {  8, 8, 8 }, // 2bpp
            {  8, 8, 4 }, // 4bpp
            {  8, 4, 4 }, // 8bpp
            {  4, 4, 4 }, // 16bpp
        };

        const Pal::Image*  pPalSrcImg = static_cast<const Pal::Image*>(src.pImage);
        const ImageType    srcImgType = pPalSrcImg->GetGfxImage()->GetOverrideImageType();
        const Pal::Image*  pPalDstImg = static_cast<const Pal::Image*>(dst.pImage);

        // 1D images have to be linear, what are we doing here?
        PAL_ASSERT(srcImgType != ImageType::Tex1d);

        // This is a violation of the PAL API...
        PAL_ASSERT(srcImgType == pPalDstImg->GetGfxImage()->GetOverrideImageType());

        // SDMA engine can't do format conversions.
        PAL_ASSERT(src.bytesPerPixel == dst.bytesPerPixel);

        // 3D displayable swizzles map to the 2D tiling types, so use those copy alignments.
        const bool      is3d           = (srcCreateInfo.imageType == ImageType::Tex3d);
        const uint32    log2Bpp        = Util::Log2(src.bytesPerPixel);
        const Extent3d& copyAlignments = (((srcCreateInfo.imageType == ImageType::Tex2d) ||
                                          (is3d && AddrMgr2::IsDisplayableSwizzle(srcSwizzle)))
                                           ? CopyAlignmentsFor2d[log2Bpp]
                                           : CopyAlignmentsFor3d[log2Bpp]);

        // Have to use scanline copies unless the copy region and the src / dst offsets are properly aligned.
        useScanlineCopy = ((IsAlignedForT2t(imageCopyInfo.copyExtent, copyAlignments) == false) ||
                           (IsAlignedForT2t(src.offset,               copyAlignments) == false) ||
                           (IsAlignedForT2t(dst.offset,               copyAlignments) == false));
    }

    // Still using the built-in packet?  One final thing to check.
    if (useScanlineCopy == false)
    {
        const AddrSwizzleMode dstSwizzle = GetSwizzleMode(dst);

        //      Src and dest surfaces share the ... same swizzle mode (Z, S, D, R)  except HW rotation. The src and
        //      dst can have different block size (256B, 4KB, etc.) and different XOR mode
        // That said... what does "except HW rotation" mean? Until we know what it means just ignore it to be safe.
        if (AddrMgr2::GetMicroSwizzle(srcSwizzle) != AddrMgr2::GetMicroSwizzle(dstSwizzle))
        {
            useScanlineCopy = true;
        }
    }

    return useScanlineCopy;
}

// =====================================================================================================================
// Copies memory into the specified region of a linear image. Returns the next unused DWORD in pCmdSpace.
//
uint32* DmaCmdBuffer::WriteCopyMemToLinearImageCmd(
    const GpuMemory&             srcGpuMemory,
    const DmaImageInfo&          dstImage,
    const MemoryImageCopyRegion& rgn,
    uint32*                      pCmdSpace
    ) const
{
    const size_t packetDwords = Util::NumBytesToNumDwords(sizeof(SDMA_PKT_COPY_LINEAR_SUBWIN));
    auto*const   pPacket      = reinterpret_cast<SDMA_PKT_COPY_LINEAR_SUBWIN*>(pCmdSpace);

    SDMA_PKT_COPY_LINEAR_SUBWIN packet;

    packet.HEADER_UNION.DW_0_DATA   = 0;
    packet.HEADER_UNION.op          = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op      = SDMA_SUBOP_COPY_LINEAR_SUB_WIND;
    packet.HEADER_UNION.elementsize = Log2(dstImage.bytesPerPixel);

    // Setup the source base address.
    const gpusize srcBaseAddr = srcGpuMemory.Desc().gpuVirtAddr + rgn.gpuMemoryOffset;
    packet.SRC_ADDR_LO_UNION.src_addr_31_0  = LowPart(srcBaseAddr);
    packet.SRC_ADDR_HI_UNION.src_addr_63_32 = HighPart(srcBaseAddr);

    // Setup the start of the source rect (all zeros).
    packet.DW_3_UNION.DW_3_DATA = 0;
    packet.DW_4_UNION.DW_4_DATA = 0;

    // Setup the source surface dimensions.
    packet.DW_4_UNION.src_pitch       = GetLinearRowPitch(rgn.gpuMemoryRowPitch, dstImage.bytesPerPixel);
    packet.DW_5_UNION.DW_5_DATA       = 0;
    packet.DW_5_UNION.src_slice_pitch = GetLinearDepthPitch(rgn.gpuMemoryDepthPitch, dstImage.bytesPerPixel);

    // Setup the destination base address.
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(dstImage.baseAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(dstImage.baseAddr);

    // Setup the start of the destination rectangle.
    packet.DW_8_UNION.DW_8_DATA = 0;
    packet.DW_8_UNION.dst_x     = rgn.imageOffset.x;
    packet.DW_8_UNION.dst_y     = rgn.imageOffset.y;
    packet.DW_9_UNION.DW_9_DATA = 0;
    packet.DW_9_UNION.dst_z     = GetImageZ(dstImage, rgn.imageOffset.z);

    // Setup the destination surface dimensions.
    packet.DW_9_UNION.dst_pitch        = GetLinearRowPitch(dstImage);
    packet.DW_10_UNION.DW_10_DATA      = 0;
    packet.DW_10_UNION.dst_slice_pitch = GetLinearDepthPitch(dstImage);

    // Setup the rectangle dimensions.
    packet.DW_11_UNION.DW_11_DATA = 0;
    packet.DW_11_UNION.rect_x     = rgn.imageExtent.width  - 1;
    packet.DW_11_UNION.rect_y     = rgn.imageExtent.height - 1;
    packet.DW_12_UNION.DW_12_DATA = 0;
    packet.DW_12_UNION.rect_z     = rgn.imageExtent.depth  - 1;

    *pPacket = packet;

    return pCmdSpace + packetDwords;
}

// =====================================================================================================================
// Copies the specified region of a linear image into memory. Returns the next unused DWORD in pCmdSpace.
//
uint32* DmaCmdBuffer::WriteCopyLinearImageToMemCmd(
    const DmaImageInfo&          srcImage,
    const GpuMemory&             dstGpuMemory,
    const MemoryImageCopyRegion& rgn,
    uint32*                      pCmdSpace
    ) const
{
    const size_t packetDwords = Util::NumBytesToNumDwords(sizeof(SDMA_PKT_COPY_LINEAR_SUBWIN));
    auto*const   pPacket      = reinterpret_cast<SDMA_PKT_COPY_LINEAR_SUBWIN*>(pCmdSpace);

    SDMA_PKT_COPY_LINEAR_SUBWIN packet;

    packet.HEADER_UNION.DW_0_DATA   = 0;
    packet.HEADER_UNION.op          = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op      = SDMA_SUBOP_COPY_LINEAR_SUB_WIND;
    packet.HEADER_UNION.elementsize = Log2(srcImage.bytesPerPixel);

    // Setup the source base address.
    packet.SRC_ADDR_LO_UNION.src_addr_31_0  = LowPart(srcImage.baseAddr);
    packet.SRC_ADDR_HI_UNION.src_addr_63_32 = HighPart(srcImage.baseAddr);

    // Setup the start of the source rect.
    packet.DW_3_UNION.DW_3_DATA = 0;
    packet.DW_3_UNION.src_x     = rgn.imageOffset.x;
    packet.DW_3_UNION.src_y     = rgn.imageOffset.y;
    packet.DW_4_UNION.DW_4_DATA = 0;
    packet.DW_4_UNION.src_z     = GetImageZ(srcImage, rgn.imageOffset.z);

    // Setup the source surface dimensions.
    packet.DW_4_UNION.src_pitch       = GetLinearRowPitch(srcImage);
    packet.DW_5_UNION.DW_5_DATA       = 0;
    packet.DW_5_UNION.src_slice_pitch = GetLinearDepthPitch(srcImage);

    // Setup the destination base address.
    const gpusize dstBaseAddr = dstGpuMemory.Desc().gpuVirtAddr + rgn.gpuMemoryOffset;
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(dstBaseAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(dstBaseAddr);

    // Setup the start of the destination rectangle (all zeros).
    packet.DW_8_UNION.DW_8_DATA = 0;
    packet.DW_9_UNION.DW_9_DATA = 0;

    // Setup the destination surface dimensions.
    packet.DW_9_UNION.dst_pitch        = GetLinearRowPitch(rgn.gpuMemoryRowPitch, srcImage.bytesPerPixel);
    packet.DW_10_UNION.DW_10_DATA      = 0;
    packet.DW_10_UNION.dst_slice_pitch = GetLinearDepthPitch(rgn.gpuMemoryDepthPitch, srcImage.bytesPerPixel);

    /// Setup the rectangle dimensions.
    packet.DW_11_UNION.DW_11_DATA = 0;
    packet.DW_11_UNION.rect_x     = rgn.imageExtent.width  - 1;
    packet.DW_11_UNION.rect_y     = rgn.imageExtent.height - 1;
    packet.DW_12_UNION.DW_12_DATA = 0;
    packet.DW_12_UNION.rect_z     = rgn.imageExtent.depth  - 1;

    *pPacket = packet;

    return pCmdSpace + packetDwords;
}

// =====================================================================================================================
uint32*  DmaCmdBuffer::BuildUpdateMemoryPacket(
    gpusize        dstAddr,
    uint32         dwordsToWrite,
    const uint32*  pSrcData,
    uint32*        pCmdSpace)
{
    SDMA_PKT_WRITE_UNTILED packet = {};

    packet.HEADER_UNION.DW_0_DATA           = 0;
    packet.HEADER_UNION.op                  = SDMA_OP_WRITE;
    packet.HEADER_UNION.sub_op              = SDMA_SUBOP_WRITE_LINEAR;
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(dstAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(dstAddr);
    packet.DW_3_UNION.DW_3_DATA             = 0;
    packet.DW_3_UNION.count                 = dwordsToWrite - 1;

    memcpy(pCmdSpace, &packet, UpdateMemoryPacketHdrSizeInDwords * sizeof(uint32));

    pCmdSpace += UpdateMemoryPacketHdrSizeInDwords;

    // Copy the source data into the command stream as well.
    memcpy(pCmdSpace, pSrcData, dwordsToWrite * sizeof(uint32));

    return (pCmdSpace + dwordsToWrite);
}

// =====================================================================================================================
// Copies the data from "pData" into the dstGpuMemory.
//
void DmaCmdBuffer::CmdUpdateMemory(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           dataSize,
    const uint32*     pData)
{
    gpusize dstAddr = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;

    // Both the destination address and the dataSize need to be dword aligned, so verify that here.
    PAL_ASSERT(IsPow2Aligned(dstAddr,  sizeof(uint32)));
    PAL_ASSERT(IsPow2Aligned(dataSize, sizeof(uint32)));

    // The SDMA_PKT_WRITE_UNTILED definition contains space for one dword of data.  To make the math a little simpler
    // below, we consider the packetHeader size to be the packet size without any associated data.
    constexpr uint32 PacketMaxDataInDwords = (1u << 20) - 1;

    // Given that PacketMaxDataInDwords is quite large, we're likely limited by the size of the reserve buffer.
    const uint32 maxDataDwords = Min(m_cmdStream.ReserveLimit() - UpdateMemoryPacketHdrSizeInDwords,
                                     PacketMaxDataInDwords);

    // Loop until we've submitted enough packets to upload the whole src buffer.
    const uint32* pRemainingSrcData   = pData;
    uint32        remainingDataDwords = static_cast<uint32>(dataSize) / sizeof(uint32);
    while (remainingDataDwords > 0)
    {
        const uint32 packetDataDwords = Min(remainingDataDwords, maxDataDwords);

        // Setup the packet.
        uint32*    pCmdSpace = m_cmdStream.ReserveCommands();
        pCmdSpace = BuildUpdateMemoryPacket(dstAddr, packetDataDwords, pRemainingSrcData, pCmdSpace);

        m_cmdStream.CommitCommands(pCmdSpace);

        // Update all variable addresses and sizes.
        remainingDataDwords -= packetDataDwords;
        pRemainingSrcData   += packetDataDwords;
        dstAddr             += packetDataDwords * sizeof(uint32);
    }
}

// =====================================================================================================================
// Writes an immediate value to specified address.
void DmaCmdBuffer::CmdWriteImmediate(
    HwPipePoint        pipePoint,
    uint64             data,
    ImmediateDataWidth dataSize,
    gpusize            address)
{
    // Make sure our destination address is dword aligned.
    PAL_ASSERT(IsPow2Aligned(address, sizeof(uint32)));

    // We only support 32bit data
    PAL_ASSERT(dataSize == ImmediateDataWidth::ImmediateData32Bit);

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    constexpr size_t PacketDwords = sizeof(SDMA_PKT_FENCE) / sizeof(uint32);
    auto*const       pPacket = reinterpret_cast<SDMA_PKT_FENCE*>(pCmdSpace);

    SDMA_PKT_FENCE packet;

    packet.HEADER_UNION.DW_0_DATA   = 0;
    packet.HEADER_UNION.op          = SDMA_OP_FENCE;
    packet.HEADER_UNION.mtype       = MTYPE_UC;
    packet.ADDR_LO_UNION.addr_31_0  = LowPart(address);
    packet.ADDR_HI_UNION.addr_63_32 = HighPart(address);
    packet.DATA_UNION.DW_3_DATA     = LowPart(data);

    *pPacket = packet;

    m_cmdStream.CommitCommands(pCmdSpace + PacketDwords);
}

// =====================================================================================================================
// Performs a memset on the specified memory region using the specified "data" value. Returns the next unused DWORD in
// pCmdSpace.
//
uint32* DmaCmdBuffer::WriteFillMemoryCmd(
    gpusize  dstAddr,
    gpusize  byteSize,
    uint32   data,
    uint32*  pCmdSpace,
    gpusize* pBytesCopied // [out] How many bytes out of byteSize this call was able to transfer.
    ) const
{
    const size_t packetDwords = Util::NumBytesToNumDwords(sizeof(SDMA_PKT_CONSTANT_FILL));
    auto*const   pPacket      = reinterpret_cast<SDMA_PKT_CONSTANT_FILL*>(pCmdSpace);

    SDMA_PKT_CONSTANT_FILL packet;

    // Because we will set fillsize = 2, the low two bits of our "count" are ignored, but we still program this in
    // terms of bytes.
    constexpr gpusize MaxFillSize = ((1ul << 22ull) - 1ull) & (~0x3ull);
    *pBytesCopied = Min(byteSize, MaxFillSize);

    packet.HEADER_UNION.DW_0_DATA           = 0;
    packet.HEADER_UNION.op                  = SDMA_OP_CONST_FILL;
    packet.HEADER_UNION.fillsize            = 2;  // 2 size means that "count" is in dwords
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(dstAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(dstAddr);
    packet.DATA_UNION.src_data_31_0         = data;
    packet.COUNT_UNION.DW_4_DATA            = 0;
    packet.COUNT_UNION.count                = *pBytesCopied - 1;

    *pPacket = packet;

    return pCmdSpace + packetDwords;
}

// =====================================================================================================================
// Adds commands necessary to write "data" to the specified memory
//
void DmaCmdBuffer::WriteEventCmd(
    const BoundGpuMemory& boundMemObj,
    HwPipePoint           pipePoint,
    uint32                data)
{
    const gpusize dstAddr = boundMemObj.GpuVirtAddr();

    // Make sure our destination address is dword aligned.
    PAL_ASSERT(IsPow2Aligned(dstAddr, sizeof(uint32)));

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    const size_t packetDwords = Util::NumBytesToNumDwords(sizeof(SDMA_PKT_FENCE));
    auto*        pPacket      = reinterpret_cast<SDMA_PKT_FENCE*>(pCmdSpace);

    SDMA_PKT_FENCE packet;

    packet.HEADER_UNION.DW_0_DATA   = 0;
    packet.HEADER_UNION.op          = SDMA_OP_FENCE;
    packet.HEADER_UNION.mtype       = MTYPE_UC;
    packet.ADDR_HI_UNION.addr_63_32 = HighPart(dstAddr);
    packet.DATA_UNION.DW_3_DATA     = data;

    // Set remaining (unused) event slots as early as possible. GFX9 and above may have supportReleaseAcquireInterface=1
    // which enables multiple slots (one dword per slot) for a GpuEvent. If the interface is not enabled, PAL client can
    // still treat the GpuEvent as one dword, but PAL needs to handle the unused extra dwords internally by setting it
    // as early in the pipeline as possible.
    const uint32 numEventSlots = m_pDevice->ChipProperties().gfxip.numSlotsPerEvent;

    for (uint32 i = 0; i < numEventSlots; i++)
    {
        packet.ADDR_LO_UNION.addr_31_0 = LowPart(dstAddr + (i * sizeof(uint32)));
        *pPacket++ = packet;
    }

    m_cmdStream.CommitCommands(pCmdSpace + packetDwords);
}

// =====================================================================================================================
// Writes a NOP packet.
uint32* DmaCmdBuffer::WriteNops(
    uint32* pCmdSpace,
    uint32  numDwords
    ) const
{
    return BuildNops(pCmdSpace, numDwords);
}

// =====================================================================================================================
// The copy-tiled-subwindow packet has added support for understanding the concept of metadata, compressed surfaces
// etc.  Setup those fields here.
//
template <typename PacketName>
void DmaCmdBuffer::SetupMetaData(
    const DmaImageInfo&  image,
    PacketName*          pPacket,
    bool                 imageIsDst)
{
    const Pal::Image*   pPalImage  = static_cast<const Pal::Image*>(image.pImage);
    const Pal::Device*  pPalDevice = pPalImage->GetDevice();
    const auto&         settings   = GetGfx9Settings(*pPalDevice);

    // Verify that this device supports compression at all through the SDMA engine
    if ((settings.waSdmaPreventCompressedSurfUse == false)
       )
    {
        const auto&       createInfo = pPalImage->GetImageCreateInfo();
        const Image*      pGfxImage  = static_cast<const Image*>(pPalImage->GetGfxImage());
        const GfxIpLevel  gfxLevel   = pPalDevice->ChipProperties().gfxLevel;
        const auto*const  pFmtInfo   =
            Pal::Formats::Gfx9::MergedChannelFlatFmtInfoTbl(gfxLevel, &pPalDevice->GetPlatform()->PlatformSettings());
        const MaskRam*    pMaskRam   = nullptr;
        const bool        colorMeta  = pGfxImage->HasDccData();

        if (colorMeta)
        {
            const auto colorLayoutToState = pGfxImage->LayoutToColorCompressionState();
            const auto colorCompressState = ImageLayoutToColorCompressionState(colorLayoutToState,
                                                                               image.imageLayout);
            if (colorCompressState != ColorDecompressed)
            {
                const ChNumFormat                format     = createInfo.swizzledFormat.format;
                const regCB_COLOR0_DCC_CONTROL&  dccControl = pGfxImage->GetDcc()->GetControlReg();
                const SurfaceSwap                surfSwap   = Formats::Gfx9::ColorCompSwap(createInfo.swizzledFormat);

                pMaskRam = pGfxImage->GetDcc();

                pPacket->META_CONFIG_UNION.max_comp_block_size   = dccControl.bits.MAX_COMPRESSED_BLOCK_SIZE;
                pPacket->META_CONFIG_UNION.max_uncomp_block_size = dccControl.bits.MAX_UNCOMPRESSED_BLOCK_SIZE;
                pPacket->META_CONFIG_UNION.data_format           = Formats::Gfx9::HwColorFmt(pFmtInfo, format);
                pPacket->META_CONFIG_UNION.number_type           = Formats::Gfx9::ColorSurfNum(pFmtInfo, format);

                if (Pal::Formats::HasAlpha(createInfo.swizzledFormat) &&
                    (surfSwap != SWAP_STD_REV)                        &&
                    (surfSwap != SWAP_ALT_REV))
                {
                    pPacket->META_CONFIG_UNION.alpha_is_on_msb = 1;
                }

                pPacket->META_CONFIG_UNION.color_transform_disable = 0;
            }
        }
        else if (pGfxImage->HasDsMetadata())
        {
            const SubresId    baseSubResId    = { image.pSubresInfo->subresId.aspect, 0, 0 };
            const auto*       pBaseSubResInfo = pPalImage->SubresourceInfo(baseSubResId);
            const ChNumFormat fmt             = pBaseSubResInfo->format.format;
            const auto dsLayoutToState        = pGfxImage->LayoutToDepthCompressionState(baseSubResId);
            const auto dsCompressState        = ImageLayoutToDepthCompressionState(dsLayoutToState,
                                                                                   image.imageLayout);
            if (dsCompressState == DepthStencilCompressed)
            {
                pMaskRam = pGfxImage->GetHtile();

                // For depth/stencil image, using HwColorFmt() is correct because:
                // 1. This field is documented by SDMA spec as "the same as the color_format used by the CB".
                // 2. IMG_DATA_FORMAT enum texture engine uses is identical as ColorFormat enum CB uses.
                // 3. Experiment results indicate this is the correct way to program this field.
                pPacket->META_CONFIG_UNION.data_format = Formats::Gfx9::HwColorFmt(pFmtInfo, fmt);

                //  These fields "max_comp_block_size", "max_uncomp_block_size" and "number_type" ... do not
                //  matter for depth and stencil for the purpose of shader compress write
            }
        }

        // If this image doesn't have meta data, then there's nothing to do...
        if (pMaskRam != nullptr)
        {
            const gpusize maskRam256Addr = colorMeta ? pGfxImage->GetDcc256BAddr() : pGfxImage->GetHtile256BAddr();

            // Despite the name of this field, it apparently means that all of the other meta-data related fields
            // are meaningful and should therefore be set for any meta-data type, not just DCC.
            pPacket->HEADER_UNION.dcc = 1;

            pPacket->META_ADDR_LO_UNION.meta_addr_31_0  = LowPart(maskRam256Addr << 8);
            pPacket->META_ADDR_HI_UNION.meta_addr_63_32 = HighPart(maskRam256Addr << 8);

            // In HW, "Color-0, Z-1, Stencil-2, Fmask-3", which lines up nicely with our aspect enumerations.
            pPacket->META_CONFIG_UNION.surface_type = static_cast<uint32>(image.pSubresInfo->subresId.aspect);

            pPacket->META_CONFIG_UNION.write_compress_enable = (imageIsDst ? 1 : 0);
            pPacket->META_CONFIG_UNION.pipe_aligned          = pGfxImage->IsPipeAligned();
        }
    } // end check for emulation
}

// =====================================================================================================================
uint32* DmaCmdBuffer::UpdateImageMetaData(
    const DmaImageInfo&  image,
    uint32*              pCmdSpace)
{
    const Pal::Image*   pPalImage  = static_cast<const Pal::Image*>(image.pImage);
    const Image*        pGfxImage  = static_cast<const Image*>(pPalImage->GetGfxImage());
    const Pal::Device*  pPalDevice = pPalImage->GetDevice();
    const auto&         settings   = GetGfx9Settings(*pPalDevice);

    // Does this image have DCC data at all?
    if (pGfxImage->HasDccData()                            &&
        // Can the SDMA engine access it?
        (settings.waSdmaPreventCompressedSurfUse == false)
       )
    {
        MipDccStateMetaData metaData = { };
        metaData.isCompressed = 1;

        // Need to update the DCC compression bit for this mip level so that the next time a DCC decompress
        // operation occurs, we know it has something to do again.
        const SubresId&  subResId         = image.pSubresInfo->subresId;
        const gpusize    dccStateMetaAddr = pGfxImage->GetDccStateMetaDataAddr(subResId.mipLevel,
                                                                               subResId.arraySlice);

        pCmdSpace = BuildUpdateMemoryPacket(dccStateMetaAddr,
                                            Util::NumBytesToNumDwords(sizeof(metaData)),
                                            reinterpret_cast<const uint32*>(&metaData),
                                            pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Either copies a linear image into a tiled one (deTile == false) or vice versa. Returns the next unused DWORD in
// pCmdSpace.
//
uint32* DmaCmdBuffer::CopyImageLinearTiledTransform(
    const DmaImageCopyInfo& copyInfo,  // info on the images being copied
    const DmaImageInfo&     linearImg, // linear image, source if deTile==false
    const DmaImageInfo&     tiledImg,  // tiled image, source if deTile==true
    bool                    deTile,    // True for copying pTiledImg into pLinearImg
    uint32*                 pCmdSpace
    ) const
{
    const size_t PacketDwords = Util::NumBytesToNumDwords(sizeof(SDMA_PKT_COPY_TILED_SUBWIN));

    SDMA_PKT_COPY_TILED_SUBWIN packet = {};

    packet.HEADER_UNION.DW_0_DATA = 0;
    packet.HEADER_UNION.op        = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op    = SDMA_SUBOP_COPY_TILED_SUB_WIND;
    packet.HEADER_UNION.detile    = deTile;

    // Setup the tiled surface here.
    packet.TILED_ADDR_LO_UNION.tiled_addr_31_0  = LowPart(tiledImg.baseAddr);
    packet.TILED_ADDR_HI_UNION.tiled_addr_63_32 = HighPart(tiledImg.baseAddr);

    // Setup the tiled start location.
    packet.DW_3_UNION.DW_3_DATA = 0;
    packet.DW_3_UNION.tiled_x   = tiledImg.offset.x;
    packet.DW_3_UNION.tiled_y   = tiledImg.offset.y;

    packet.DW_4_UNION.DW_4_DATA = 0;
    packet.DW_4_UNION.tiled_z   = GetImageZ(tiledImg);
    packet.DW_4_UNION.width     = tiledImg.extent.width - 1;

    // Setup the tiled surface dimensions.
    packet.DW_5_UNION.DW_5_DATA  = 0;
    packet.DW_5_UNION.height     = tiledImg.extent.height - 1;
    packet.DW_5_UNION.depth      = tiledImg.extent.depth - 1;

    packet.DW_6_UNION.DW_6_DATA    = 0;
    packet.DW_6_UNION.element_size = Log2(tiledImg.bytesPerPixel);
    packet.DW_6_UNION.swizzle_mode = AddrMgr2::GetHwSwizzleMode(GetSwizzleMode(tiledImg));
    packet.DW_6_UNION.dimension    = GetHwDimension(tiledImg);
    packet.DW_6_UNION.mip_max      = GetMaxMip(tiledImg);
    packet.DW_6_UNION.mip_id       = tiledImg.pSubresInfo->subresId.mipLevel;

    // Setup the linear surface here.
    packet.LINEAR_ADDR_LO_UNION.linear_addr_31_0  = LowPart(linearImg.baseAddr);
    packet.LINEAR_ADDR_HI_UNION.linear_addr_63_32 = HighPart(linearImg.baseAddr);

    // Setup the linear start location.
    packet.DW_9_UNION.DW_9_DATA   = 0;
    packet.DW_9_UNION.linear_x    = linearImg.offset.x;
    packet.DW_9_UNION.linear_y    = linearImg.offset.y;
    packet.DW_10_UNION.DW_10_DATA = 0;
    packet.DW_10_UNION.linear_z   = GetImageZ(linearImg);

    // Linear is the source.
    packet.DW_10_UNION.linear_pitch       = GetLinearRowPitch(linearImg);
    packet.DW_11_UNION.DW_11_DATA         = 0;
    packet.DW_11_UNION.linear_slice_pitch = GetLinearDepthPitch(linearImg);

    // Setup the rectangle to copy.
    packet.DW_12_UNION.DW_12_DATA = 0;
    packet.DW_12_UNION.rect_x     = copyInfo.copyExtent.width - 1;
    packet.DW_12_UNION.rect_y     = copyInfo.copyExtent.height - 1;
    packet.DW_13_UNION.DW_13_DATA = 0;
    packet.DW_13_UNION.rect_z     = copyInfo.copyExtent.depth - 1;

    SetupMetaData(tiledImg, &packet, (deTile == false));

    auto*const   pPacket = reinterpret_cast<SDMA_PKT_COPY_TILED_SUBWIN*>(pCmdSpace);
    *pPacket   = packet;
    pCmdSpace += PacketDwords;

    if (deTile == false)
    {
        pCmdSpace = UpdateImageMetaData(tiledImg, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Either copies gpuMemory to image (deTile = false) or vice versa. Returns the next unused DWORD in pCmdSpace.
//
uint32* DmaCmdBuffer::CopyImageMemTiledTransform(
    const DmaImageInfo&          image,
    const GpuMemory&             gpuMemory,
    const MemoryImageCopyRegion& rgn,
    bool                         deTile,
    uint32*                      pCmdSpace
    ) const
{
    const size_t PacketDwords = Util::NumBytesToNumDwords(sizeof(SDMA_PKT_COPY_TILED_SUBWIN));

    SDMA_PKT_COPY_TILED_SUBWIN packet = {};

    packet.HEADER_UNION.DW_0_DATA = 0;
    packet.HEADER_UNION.op        = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op    = SDMA_SUBOP_COPY_TILED_SUB_WIND;
    packet.HEADER_UNION.detile    = deTile; // One packet handles both directions.

    // Setup the tiled surface here.
    packet.TILED_ADDR_LO_UNION.tiled_addr_31_0  = LowPart(image.baseAddr);
    packet.TILED_ADDR_HI_UNION.tiled_addr_63_32 = HighPart(image.baseAddr);

    // Setup the tiled start location.
    packet.DW_3_UNION.DW_3_DATA = 0;
    packet.DW_3_UNION.tiled_x   = rgn.imageOffset.x;
    packet.DW_3_UNION.tiled_y   = rgn.imageOffset.y;

    packet.DW_4_UNION.DW_4_DATA = 0;
    packet.DW_4_UNION.tiled_z   = GetImageZ(image, rgn.imageOffset.z);
    packet.DW_4_UNION.width     = image.extent.width - 1;

    // Setup the tiled surface dimensions.
    packet.DW_5_UNION.DW_5_DATA  = 0;
    packet.DW_5_UNION.height     = image.extent.height - 1;
    packet.DW_5_UNION.depth      = image.extent.depth - 1;

    packet.DW_6_UNION.DW_6_DATA    = 0;
    packet.DW_6_UNION.element_size = Log2(image.bytesPerPixel);
    packet.DW_6_UNION.swizzle_mode = AddrMgr2::GetHwSwizzleMode(GetSwizzleMode(image));
    packet.DW_6_UNION.dimension    = GetHwDimension(image);
    packet.DW_6_UNION.mip_max      = GetMaxMip(image);
    packet.DW_6_UNION.mip_id       = image.pSubresInfo->subresId.mipLevel;

    // Setup the linear surface here.
    const gpusize linearBaseAddr = gpuMemory.Desc().gpuVirtAddr + rgn.gpuMemoryOffset;
    packet.LINEAR_ADDR_LO_UNION.linear_addr_31_0  = LowPart(linearBaseAddr);
    packet.LINEAR_ADDR_HI_UNION.linear_addr_63_32 = HighPart(linearBaseAddr);

    // Setup the linear start location (all zeros).
    packet.DW_9_UNION.DW_9_DATA   = 0;
    packet.DW_10_UNION.DW_10_DATA = 0;

    // Setup the linear surface dimensions.
    packet.DW_10_UNION.linear_pitch       = GetLinearRowPitch(rgn.gpuMemoryRowPitch, image.bytesPerPixel);
    packet.DW_11_UNION.DW_11_DATA         = 0;
    packet.DW_11_UNION.linear_slice_pitch = GetLinearDepthPitch(rgn.gpuMemoryDepthPitch, image.bytesPerPixel);

    // Setup the rectangle to copy.
    packet.DW_12_UNION.DW_12_DATA = 0;
    packet.DW_12_UNION.rect_x     = rgn.imageExtent.width  - 1;
    packet.DW_12_UNION.rect_y     = rgn.imageExtent.height - 1;
    packet.DW_13_UNION.DW_13_DATA = 0;
    packet.DW_13_UNION.rect_z     = rgn.imageExtent.depth  - 1;

    SetupMetaData(image, &packet, (deTile == false));

    auto*const  pPacket = reinterpret_cast<SDMA_PKT_COPY_TILED_SUBWIN*>(pCmdSpace);
    *pPacket = packet;

    if (deTile == false)
    {
        pCmdSpace = UpdateImageMetaData(image, pCmdSpace);
    }

    return pCmdSpace + PacketDwords;
}

// =====================================================================================================================
// Returns the dimension (1D, 2D, 3D) of the specified surface as a HW enumeration
uint32 DmaCmdBuffer::GetHwDimension(
    const DmaImageInfo&  dmaImageInfo)
{
    Pal::ImageType  imageType = GetImageType(*dmaImageInfo.pImage);

    if ((imageType == ImageType::Tex1d) || (imageType == ImageType::Tex3d))
    {
        const AddrSwizzleMode  swizzleMode = GetSwizzleMode(dmaImageInfo);

        if ((swizzleMode == ADDR_SW_64KB_R_X) || (swizzleMode == ADDR_SW_64KB_Z_X))
        {
            imageType = ImageType::Tex2d;
        }
    }

    // The HW dimension enumerations match our image-type dimensions.  i.e., 0 = 1d, 1 = 2d, 2 = 3d.
    return static_cast<uint32>(imageType);
}

// =====================================================================================================================
uint32 DmaCmdBuffer::GetLinearRowPitch(
    gpusize  rowPitchInBytes,
    uint32   bytesPerPixel
    ) const
{
    PAL_ASSERT((rowPitchInBytes % bytesPerPixel) == 0);

    const uint32  rowPitchInPixels = static_cast<uint32>(rowPitchInBytes / bytesPerPixel);

    //  The alignment restriction of linear pitch is:
    //    Multiple of 4 for 8bpp
    //    Multiple of 2 for 16bpp
    //    Multiple of 1 for 32bpp
    PAL_ASSERT((rowPitchInPixels % Util::Max(1u, (4 / bytesPerPixel))) == 0);

    // The unit of linear pitch ... is pixel number minus 1
    return rowPitchInPixels - 1;
}

// =====================================================================================================================
// Returns the maximum number of mip levels that are associated with the specified image.  Doesn't count the base level
uint32 DmaCmdBuffer::GetMaxMip(
    const DmaImageInfo&  dmaImageInfo)
{
    const auto&  imageCreateInfo = dmaImageInfo.pImage->GetImageCreateInfo();

    return (imageCreateInfo.mipLevels - 1);
}

// =====================================================================================================================
// Returns the swizzle mode as a SW enumeration (AddrSwizzleMode) for the specified image
AddrSwizzleMode DmaCmdBuffer::GetSwizzleMode(
    const DmaImageInfo&  dmaImageInfo)
{
    const auto*const      pImage    = static_cast<const Pal::Image*>(dmaImageInfo.pImage);
    const GfxImage*const  pGfxImage = pImage->GetGfxImage();
    const AddrSwizzleMode tileMode  = static_cast<AddrSwizzleMode>(pGfxImage->GetSwTileMode(dmaImageInfo.pSubresInfo));

    return tileMode;
}

// =====================================================================================================================
// Returns the pipe/bank xor value for the specified image / subresource.
uint32 DmaCmdBuffer::GetPipeBankXor(
    const Pal::Image&   image,
    const SubresId&     subresource)
{
    const auto*  pTileInfo = AddrMgr2::GetTileInfo(&image, subresource);

    return pTileInfo->pipeBankXor;
}

// =====================================================================================================================
// Returns the base address for HW programming purposes of the specified sub-resource, complete with any pipe-bank-xor
// bits included.  Since in some situations the HW calculates the mip-level and array slice offsets itself, those may
// not be reflected in ther returned address.
gpusize DmaCmdBuffer::GetSubresourceBaseAddr(
    const Pal::Image&  image,
    const SubresId&    subresource
    ) const
{
    gpusize  baseAddr = 0;

    if (image.IsSubResourceLinear(subresource))
    {
        // GFX10 doesn't support mip-levels with linear surfaces.  They do, however, support slices.  We need to get
        // the starting offset of slice 0 of a given mip level.
        const SubresId  baseSubres = { subresource.aspect, subresource.mipLevel, 0 };

        // Verify that we don't have to take into account the pipe/bank xor value here.
        PAL_ASSERT(GetPipeBankXor(image, subresource) == 0);

        // Return the address of the subresource.
        baseAddr = image.GetSubresourceBaseAddr(baseSubres);
    }
    else
    {
        const GfxImage*  pGfxImage = image.GetGfxImage();

        baseAddr = pGfxImage->GetAspectBaseAddr(subresource.aspect);
    }

    return baseAddr;
}

// =====================================================================================================================
// Gfx10 assumes that tiled images will also be programmed with the dimensions of the base mip level, so retrieve those
// dimensions here.  It doesn't really matter for linear images since the extent information isn't used for linear
// images.  Besides, GFX10 doesn't support linear mip-mapped images anyway.
void DmaCmdBuffer::SetupDmaInfoExtent(
    DmaImageInfo*  pImageInfo
    ) const
{
    const Pal::Image*  pImage          = reinterpret_cast<const Pal::Image*>(pImageInfo->pImage);
    const SubresId     baseSubResId    = { pImageInfo->pSubresInfo->subresId.aspect, 0, 0 };
    const auto*        pBaseSubResInfo = pImage->SubresourceInfo(baseSubResId);
    const uint32       bytesPerPixel   = pBaseSubResInfo->bitsPerTexel / 8;
    const bool         nonPow2Bpp      = (IsPowerOfTwo(bytesPerPixel) == false);

    if (nonPow2Bpp || Formats::IsBlockCompressed(pImageInfo->pSubresInfo->format.format))
    {
        pImageInfo->extent       = pBaseSubResInfo->extentElements;
        pImageInfo->actualExtent = pBaseSubResInfo->actualExtentElements;
    }
    else
    {
        pImageInfo->extent       = pBaseSubResInfo->extentTexels;
        pImageInfo->actualExtent = pBaseSubResInfo->actualExtentTexels;
    }
}

// =====================================================================================================================
DmaCmdBuffer::DmaMemImageCopyMethod DmaCmdBuffer::GetMemImageCopyMethod(
    bool                         isLinearImg,
    const DmaImageInfo&          imageInfo,
    const MemoryImageCopyRegion& region
    ) const
{
    DmaMemImageCopyMethod copyMethod = DmaCmdBuffer::DmaMemImageCopyMethod::Native;

    // Before OSS-4.0, the x, rect_x, src/dst_pitch and src/dst_slice_pitch must be dword-aligned when
    // expressed in units of bytes on L2T copies only.
    if ((isLinearImg == false) && AreMemImageXParamsDwordAligned(imageInfo, region) == false)
    {
        copyMethod = DmaMemImageCopyMethod::DwordUnaligned;
    }

    return copyMethod;
}

} // Gfx9
} // Pal
