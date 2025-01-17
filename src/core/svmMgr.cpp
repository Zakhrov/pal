/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/svmMgr.h"
#include "palBestFitAllocatorImpl.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
SvmMgr::SvmMgr(
    Device* pDevice)
    :
    m_pDevice(pDevice),
    m_vaStart(0),
    m_vaSize(0),
    m_pSubAllocator(nullptr)
{
}

// =====================================================================================================================
SvmMgr::~SvmMgr()
{
    PAL_DELETE(m_pSubAllocator, m_pDevice->GetPlatform());
}

// =====================================================================================================================
// Peforms extra initialization which needs to be done when the client is ready to start using the device.
// - Reserve CPU & GPU virtual address for SVM use.
Result SvmMgr::Init(
    VaRangeInfo* pSvmVaInfo)
{
    Result result = Result::Success;

    const GpuMemoryProperties& memProps = m_pDevice->MemoryProperties();
    const gpusize svmVaSize = m_pDevice->GetPlatform()->GetMaxSizeOfSvm();
    // Create and initialize the suballocator
    m_pSubAllocator = PAL_NEW(BestFitAllocator<Platform>, m_pDevice->GetPlatform(), AllocInternal)
                               (m_pDevice->GetPlatform(), svmVaSize, memProps.fragmentSize);

    if (m_pSubAllocator == nullptr)
    {
        result = Result::ErrorOutOfMemory;
    }
    if (result == Result::Success)
    {
        result = m_pSubAllocator->Init();
    }

    if (result == Result::Success)
    {
        if (m_pDevice->GetPlatform()->GetSvmRangeStart() == 0)
        {
            const gpusize vaStart = memProps.vaRange[static_cast<uint32>(VaPartition::Default)].baseVirtAddr;
            const gpusize vaEnd   = memProps.vaRange[static_cast<uint32>(VaPartition::Default)].size;
            const gpusize vaAlignment = (1ull << 32u);

            for (gpusize vaAddr = vaStart; vaAddr <= (vaEnd - svmVaSize); vaAddr += vaAlignment)
            {
                gpusize vaAllocated = 0u;
                result = VirtualReserve(static_cast<size_t>(svmVaSize),
                                        reinterpret_cast<void**>(&vaAllocated),
                                        reinterpret_cast<void*>(vaAddr));

                // Make sure we get the address that we requested
                if (vaAllocated != vaAddr)
                {
                    result = Result::ErrorOutOfMemory;
                }

                if (result == Result::Success)
                {
                    gpusize gpuVaAllocated = 0u;
                    result = m_pDevice->ReserveGpuVirtualAddress(VaPartition::Svm, vaAddr, svmVaSize, false,
                                                                 VirtualGpuMemAccessMode::Undefined, &gpuVaAllocated);
                    if (result == Result::Success)
                    {
                        pSvmVaInfo->baseVirtAddr = vaAddr;
                        pSvmVaInfo->size         = svmVaSize;
                        m_pDevice->GetPlatform()->SetSvmRangeStart(vaAddr);
                        break;
                    }
                    else
                    {
                        result = VirtualRelease(reinterpret_cast<void*>(vaAddr),
                                                static_cast<size_t>(svmVaSize));
                    }
                }
            }
        }
        else
        {
            pSvmVaInfo->baseVirtAddr = m_pDevice->GetPlatform()->GetSvmRangeStart();
            pSvmVaInfo->size         = svmVaSize;
        }
        if (result == Result::Success)
        {
            m_vaStart = pSvmVaInfo->baseVirtAddr;
            m_vaSize  = pSvmVaInfo->size;
        }
    }

    if (result == Result::Success)
    {
        result = m_allocFreeVaLock.Init();
    }

    return result;
}

// =====================================================================================================================
// Peforms extra initialization which needs to be done when the client is ready to start using the device.
// - Release CPU & GPU virtual address of SVM.
Result SvmMgr::Cleanup()
{
    Result result = Result::Success;

    if (m_pDevice->GetPlatform()->GetSvmRangeStart() != 0)
    {
        result = m_pDevice->FreeGpuVirtualAddress(m_vaStart, m_vaSize);
    }

    return result;
}

// =====================================================================================================================
Result SvmMgr::AllocVa(
    gpusize size,
    uint32  align,
    gpusize* pVirtualAddress)
{
    gpusize assignedVa = 0;
    MutexAuto lock(&m_allocFreeVaLock);

    Result result = m_pSubAllocator->Allocate(size, align, &assignedVa);
    *pVirtualAddress = assignedVa + m_vaStart;

    return result;
}

// =====================================================================================================================
void SvmMgr::FreeVa(
    gpusize virtualAddress)
{
    MutexAuto lock(&m_allocFreeVaLock);

    m_pSubAllocator->Free((virtualAddress - m_vaStart));
    return;
}
} // Pal
