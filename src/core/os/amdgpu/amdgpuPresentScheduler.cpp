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

#include "core/masterQueueSemaphore.h"
#include "core/os/amdgpu/amdgpuDevice.h"
#include "core/os/amdgpu/amdgpuImage.h"
#include "core/os/amdgpu/amdgpuPresentScheduler.h"
#include "core/os/amdgpu/amdgpuQueue.h"
#include "core/os/amdgpu/amdgpuSwapChain.h"
#include "core/os/amdgpu/amdgpuWindowSystem.h"

using namespace Util;

namespace Pal
{
namespace Amdgpu
{

// =====================================================================================================================
// This function assumes that pCreateInfo has been initialized to zero.
static void GetInternalQueueInfo(
    const Pal::Device& device,
    QueueCreateInfo*   pCreateInfo)
{
    const auto& engineProps = device.EngineProperties();

    // No need to optimize something just for semaphores and fences.
    pCreateInfo->submitOptMode = SubmitOptMode::Disabled;

    // The Linux present scheduler's internal signal and present queues both only need to support fences and semaphores.
    // Select the most light-weight queue that can meet those requirements.
    if (engineProps.perEngine[EngineTypeDma].numAvailable > 0)
    {
        pCreateInfo->queueType  = QueueTypeDma;
        pCreateInfo->engineType = EngineTypeDma;
    }
    else if (engineProps.perEngine[EngineTypeCompute].numAvailable > 0)
    {
        pCreateInfo->queueType  = QueueTypeCompute;
        pCreateInfo->engineType = EngineTypeCompute;
    }
    else if (engineProps.perEngine[EngineTypeUniversal].numAvailable > 0)
    {
        pCreateInfo->queueType  = QueueTypeUniversal;
        pCreateInfo->engineType = EngineTypeUniversal;
    }
    else
    {
        // We assume we can always find at least one queue to use.
        PAL_ASSERT_ALWAYS();
    }
}

// =====================================================================================================================
size_t PresentScheduler::GetSize(
    const Device& device,
    IDevice*const pSlaveDevices[],
    WsiPlatform   wsiPlatform)
{
    QueueCreateInfo queueInfo = {};
    GetInternalQueueInfo(device, &queueInfo);

    // We need space for the object, m_pSignalQueue, and m_pPresentQueues.
    size_t objectSize = (sizeof(PresentScheduler) + (2 * device.GetQueueSize(queueInfo, nullptr)));

    // Additional present queues for slave devices may have different create info/sizes.
    for (uint32 i = 0; i < (XdmaMaxDevices - 1); i++)
    {
        Pal::Device* pDevice = static_cast<Pal::Device*>(pSlaveDevices[i]);

        if (pDevice != nullptr)
        {
            GetInternalQueueInfo(*pDevice, &queueInfo);

            objectSize += pDevice->GetQueueSize(queueInfo, nullptr);
        }
    }

    return objectSize;
}

// =====================================================================================================================
Result PresentScheduler::Create(
    Device*                 pDevice,
    IDevice*const           pSlaveDevices[],
    WindowSystem*           pWindowSystem,
    void*                   pPlacementAddr,
    Pal::PresentScheduler** ppPresentScheduler)
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppPresentScheduler != nullptr));

    auto*const pScheduler = PAL_PLACEMENT_NEW(pPlacementAddr) PresentScheduler(pDevice, pWindowSystem);
    Result     result     = pScheduler->Init(pSlaveDevices, pScheduler + 1);

    if (result == Result::Success)
    {
        *ppPresentScheduler = pScheduler;
    }
    else
    {
        pScheduler->Destroy();
    }

    return result;
}

// =====================================================================================================================
PresentScheduler::PresentScheduler(
    Device*       pDevice,
    WindowSystem* pWindowSystem)
    :
    Pal::PresentScheduler(pDevice),
    m_pWindowSystem(pWindowSystem)
{
}

// =====================================================================================================================
Result PresentScheduler::Init(
    IDevice*const pSlaveDevices[],
    void*         pPlacementAddr)
{
    Result result = Result::Success;

    // Create the internal presentation queue as well as any additional internal queues for slave fullscreen presents
    QueueCreateInfo presentQueueInfo = {};
    Pal::Device*    pDevice          = m_pDevice;
    uint32          queueIndex       = 0;

    do
    {
        if (result == Result::Success)
        {
            GetInternalQueueInfo(*pDevice, &presentQueueInfo);

            if (pDevice->GetEngine(presentQueueInfo.engineType, presentQueueInfo.engineIndex) == nullptr)
            {
                // If the client didn't request this engine when they finalized the device, we need to create it.
                result = pDevice->CreateEngine(presentQueueInfo.engineType, presentQueueInfo.engineIndex);
            }
        }

        if (result == Result::Success)
        {
            result         = pDevice->CreateQueue(presentQueueInfo, pPlacementAddr, &m_pPresentQueues[queueIndex]);
            pPlacementAddr = VoidPtrInc(pPlacementAddr, pDevice->GetQueueSize(presentQueueInfo, nullptr));
        }

        pDevice = static_cast<Pal::Device*>(pSlaveDevices[queueIndex]);
        queueIndex++;
    }
    while ((pDevice != nullptr) && (queueIndex < XdmaMaxDevices));

    if (result == Result::Success)
    {
        QueueCreateInfo signalQueueInfo = {};

        GetInternalQueueInfo(*m_pDevice, &signalQueueInfo);

        PAL_ASSERT(m_pDevice->GetEngine(signalQueueInfo.engineType, signalQueueInfo.engineIndex) != nullptr);

        result         = m_pDevice->CreateQueue(signalQueueInfo, pPlacementAddr, &m_pSignalQueue);
        pPlacementAddr = VoidPtrInc(pPlacementAddr, m_pDevice->GetQueueSize(signalQueueInfo, nullptr));
    }

    if (result == Result::Success)
    {
        result = Pal::PresentScheduler::Init(pSlaveDevices, pPlacementAddr);
    }

    return result;
}

// =====================================================================================================================
// Queues a present followed by any necessary signals or waits on the given queue to reuse swap chain images.
// It will block the current thread if required to meet the requirements of the present (e.g., guarantee that the given
// image is displayed for at least one vblank).
//
// This function must do its best to continue to make progress even if an error occurs to keep the swap chain valid.
Result PresentScheduler::ProcessPresent(
    const PresentSwapChainInfo& presentInfo,
    IQueue*                     pQueue,
    bool                        isInline)
{
    // The Linux present scheduler doesn't support inline presents because it doesn't use queues to execute presents.
    PAL_ASSERT(isInline == false);

    SwapChain*const     pSwapChain    = static_cast<SwapChain*>(presentInfo.pSwapChain);
    const SwapChainMode swapChainMode = pSwapChain->CreateInfo().swapChainMode;

    // We only support these modes on Linux.
    PAL_ASSERT((swapChainMode == SwapChainMode::Immediate) ||
               (swapChainMode == SwapChainMode::Mailbox)   ||
               (swapChainMode == SwapChainMode::Fifo));

    // Ask the windowing system to present our image with the swap chain's idle fence. We don't need it to wait for
    // prior rendering because that was already done by our caller.
    PresentFence*const pIdleFence = pSwapChain->PresentIdleFence(presentInfo.imageIndex);
    Result             result     = m_pWindowSystem->Present(presentInfo,
                                                             nullptr,
                                                             pIdleFence);

    if (swapChainMode == SwapChainMode::Mailbox)
    {
        // The image has been submitted to the mailbox so we consider the present complete.
        const Result completedResult = pSwapChain->PresentComplete(pQueue, presentInfo.imageIndex);
        result = CollapseResults(result, completedResult);
    }
    else
    {
        if (swapChainMode == SwapChainMode::Fifo)
        {
            // Present returns as soon as the windowing system has queued our request. To meet FIFO's requirements we
            // must wait until that request has been submitted to hardware.
            const Result waitResult = m_pWindowSystem->WaitForLastImagePresented();
            result = CollapseResults(result, waitResult);
        }

        // Otherwise we must be doing a blit present and would rather wait for it to complete now so that the
        // application can reacquire the image as quickly as possible.
        const Result completedResult = pSwapChain->PresentComplete(pQueue, presentInfo.imageIndex);
        result = CollapseResults(result, completedResult);
    }

    return result;
}

// =====================================================================================================================
Result PresentScheduler::PreparePresent(
    IQueue*              pQueue,
    PresentSchedulerJob* pJob)
{
    Result result = Result::Success;

    SubmitInfo submitInfo = {};
    result = pQueue->Submit(submitInfo);

    return result;
}

// =====================================================================================================================
// Must clean up any dangling synchronization state in the event that we fail to queue a present job.
Result PresentScheduler::FailedToQueuePresentJob(
    const PresentSwapChainInfo& presentInfo,
    IQueue*                     pQueue)
{
    // We must signal the image's idle fence because we're about to wait on it.
    SwapChain*const pSwapChain = static_cast<SwapChain*>(presentInfo.pSwapChain);
    Result          result     = pSwapChain->PresentIdleFence(presentInfo.imageIndex)->Trigger();

    // Now call PresentComplete to fix the swap chain.
    const Result completedResult = pSwapChain->PresentComplete(pQueue, presentInfo.imageIndex);
    return CollapseResults(result, completedResult);
}

// =====================================================================================================================
Result PresentScheduler::SignalOnAcquire(
    IQueueSemaphore* pPresentComplete,
    IQueueSemaphore* pSemaphore,
    IFence* pFence)
{
    Result result = Result::Success;

    if (static_cast<Device*>(m_pDevice)->GetSemaphoreType() == SemaphoreType::SyncObj)
    {
        InternalSubmitInfo internalSubmitInfo = {};

        SubmitInfo submitInfo = {};
        submitInfo.pFence     = pFence;

        if ((result == Result::Success) && (pPresentComplete != nullptr))
        {
            result = m_pSignalQueue->WaitQueueSemaphore(pPresentComplete);
        }

        if (result == Result::Success)
        {
            if (pSemaphore != nullptr)
            {
                internalSubmitInfo.signalSemaphoreCount = 1;
                internalSubmitInfo.ppSignalSemaphores = &pSemaphore;
                static_cast<MasterQueueSemaphore*>(pSemaphore)->EarlySignal();
            }

            if (pFence != nullptr)
            {
                static_cast<Queue*>(m_pSignalQueue)->AssociateFenceWithContext(pFence);
            }

            result = static_cast<Queue*>(m_pSignalQueue)->OsSubmit(submitInfo, internalSubmitInfo);
            PAL_ASSERT(result == Result::Success);
        }
    }
    else
    {
        result = Pal::PresentScheduler::SignalOnAcquire(pPresentComplete, pSemaphore, pFence);
    }

    return result;
}
} // Amdgpu
} // Pal
