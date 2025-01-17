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
/**
 ***********************************************************************************************************************
 * @file  palMutex.h
 * @brief PAL utility collection Mutex and MutexAuto class declarations.
 ***********************************************************************************************************************
 */

#pragma once

#include "palAssert.h"

#if   defined(__unix__)
#include <pthread.h>
#include <string.h>

#endif

namespace Util
{

/**
 ***********************************************************************************************************************
 * @brief Platform-agnostic mutex primitive.
 ***********************************************************************************************************************
 */
class Mutex
{
public:
#if   defined(__unix__)
    /// Defines MutexData as a unix pthread_mutex_t
    typedef pthread_mutex_t  MutexData;
#endif

    Mutex() : m_initialized(false) { memset(&m_osMutex, 0, sizeof(m_osMutex)); }
    ~Mutex();

    /// Initializes the mutex object.
    ///
    /// @returns Success if successful, otherwise an appropriate error.
    Result Init();

    /// Enters the critical section if it is not contended.  If it is contended, wait for the critical section to become
    /// available, then enter it.
    void Lock();

    /// Enters the critical section if it is not contended.  Does not wait for the critical section to become available
    /// if it is contended.
    ///
    /// @returns True if the critical section was entered, false otherwise.
    bool TryLock();

    /// Leaves the critical section.
    void Unlock();

    /// Returns the OS specific mutex data.
    MutexData* GetMutexData() { return &m_osMutex; }

private:
    MutexData m_osMutex;     ///< Opaque structure to the OS-specific Mutex data
    bool      m_initialized; ///< True indicates this mutex has been initialized

    PAL_DISALLOW_COPY_AND_ASSIGN(Mutex);
};

/**
 ***********************************************************************************************************************
 * @brief A "resource acquisition is initialization" (RAII) wrapper for the Mutex class.
 *
 * The RAII paradigm allows critical sections to be automatically acquired during this class' constructor, and
 * automatically released when a stack-allocated wrapper object goes out-of-scope.  As such, it only makes sense to use
 * this class for stack-allocated objects.
 *
 * This object will ensure that anything between when the object is allocated on the stack and when it goes out of scope
 * will be protected from access by multiple threads.  See the below example.
 *
 *     [Code not protected]
 *     {
 *         [Code not protected]
 *         MutexAuto lock(pPtrToMutex);
 *         [Code is protected]
 *     }
 *     [Code not protected]
 ***********************************************************************************************************************
 */
class MutexAuto
{
public:
    /// Locks the given Mutex.
    explicit MutexAuto(Mutex* pMutex) : m_pMutex(pMutex)
    {
        PAL_ASSERT(m_pMutex != nullptr);
        m_pMutex->Lock();
    }

    /// Unlocks the Mutex we locked in the constructor.
    ~MutexAuto()
    {
        m_pMutex->Unlock();
    }

private:
    Mutex* const  m_pMutex;  ///< The Mutex which this object wraps.

    PAL_DISALLOW_DEFAULT_CTOR(MutexAuto);
    PAL_DISALLOW_COPY_AND_ASSIGN(MutexAuto);
};

/**
 ***********************************************************************************************************************
 * @brief Platform-agnostic rw lock primitive.
 ***********************************************************************************************************************
 */
class RWLock
{
public:
    /// Enumerates the lock type of RWLockAuto
    enum LockType
    {
        ReadOnly = 0,  ///< Lock in readonly mode, in other words shared mode.
        ReadWrite      ///< Lock in readwrite mode, in other words exclusive mode.
    };

    RWLock() : m_initialized(false) { memset(&m_osRWLock, 0, sizeof(m_osRWLock)); }
    ~RWLock();

    /// Initializes the rwlock object.
    /// @returns Success if successful, otherwise an appropriate error.
    Result Init();

    /// Acquires a rw lock in shared mode if it is not contended in exclusive mode.
    /// If it is contended, wait for rw lock to become available, then enter it.
    void LockForRead();

    /// Acquires a rw lock in exclusive mode if it is not contended.
    /// If it is contended, wait for rw lock to become available, then enter it.
    void LockForWrite();

    /// Try to acquires a rw lock in shared mode if it is not contended in exclusive mode.
    /// Does not wait for the rw lock to become available.
    /// @returns True if the rw lock was acquired, false otherwise.
    bool TryLockForRead();

    /// Try to acquires a rw lock in exclusive mode if it is not contended.
    /// Does not wait for the rw lock to become available.
    /// @returns True if the rw lock was acquired, false otherwise.
    bool TryLockForWrite();

    /// Release the rw lock which is previously contended in shared mode.
    void UnlockForRead();

    /// Release the rw lock which is previously contended in exclusive mode.
    void UnlockForWrite();

private:
#if   defined(__unix__)
    /// Defines RWLockData as a unix pthread_rwlock_t
    typedef pthread_rwlock_t  RWLockData;
#endif

    RWLockData m_osRWLock;    ///< Opaque structure to the OS-specific RWLock data
    bool       m_initialized; ///< True indicates this RWLock has been initialized

    PAL_DISALLOW_COPY_AND_ASSIGN(RWLock);
};

/**
 ***********************************************************************************************************************
 * @brief A "resource acquisition is initialization" (RAII) wrapper for the RWLock class.
 *
 * The RAII paradigm allows rw lcok to be automatically acquired during this class' constructor, and
 * automatically released when a stack-allocated wrapper object goes out-of-scope.  As such, it only makes sense to use
 * this class for stack-allocated objects.
 *
 * This object will ensure that anything between when the object is allocated on the stack and when it goes out of scope
 * will be protected from access by multiple threads.  See the below example.
 *
 *     [Code not protected]
 *     {
 *         [Code not protected]
 *         RWLockAuto lock(pPtrToMutex, type);
 *         [Code is protected]
 *     }
 *     [Code not protected]
 ***********************************************************************************************************************
 */
template <RWLock::LockType type>
class RWLockAuto
{
public:
    /// Locks the given RWLock.
    explicit RWLockAuto(RWLock* pRWLock) : m_pRWLock(pRWLock)
    {
        PAL_ASSERT(m_pRWLock != nullptr);
        if (type == RWLock::ReadOnly)
        {
            m_pRWLock->LockForRead();
        }
        else
        {
            m_pRWLock->LockForWrite();
        }
    }

    /// Unlocks the RWLock we locked in the constructor.
    ~RWLockAuto()
    {
        if (type == RWLock::ReadOnly)
        {
            m_pRWLock->UnlockForRead();
        }
        else
        {
            m_pRWLock->UnlockForWrite();
        }
    }

private:
    RWLock* const m_pRWLock;  ///< The RWLock which this object wraps.

    PAL_DISALLOW_DEFAULT_CTOR(RWLockAuto);
    PAL_DISALLOW_COPY_AND_ASSIGN(RWLockAuto);
};

/// Yields the current thread to another thread in the ready state (if available).
extern void YieldThread();

/// Atomically increments the specified 32-bit unsigned integer.
///
/// @param [in,out] pValue Pointer to the value to be incremented.
///
/// @returns Result of the increment operation.
extern uint32 AtomicIncrement(volatile uint32* pValue);

/// Atomically increment a 64-bit-unsigned  integer
///
/// @param [in,out] pAddend Pointer to the value to be incremented
///
/// @returns Result of the increment operation.
extern uint64 AtomicIncrement64(volatile uint64* pAddend);

/// Atomically decrements the specified 32-bit unsigned integer.
///
/// @param [in,out] pValue Pointer to the value to be decremented.
///
/// @returns Result of the decrement operation.
extern uint32 AtomicDecrement(volatile uint32* pValue);

/// Performs an atomic compare and swap operation on two 32-bit unsigned integers. This operation compares *pTarget
/// with oldValue and replaces it with newValue if they match. If the values don't match, no action is taken.
/// The original value of *pTarget is returned as a result.
///
/// @param [in,out] pTarget  Pointer to the destination value of the operation.
/// @param [in]     oldValue Literal value to compare *pTarget to.
/// @param [in]     newValue Literal value to replace *pTarget with if *pTarget matches oldValue.
///
/// @returns Previous value at *pTarget.
extern uint32 AtomicCompareAndSwap(volatile uint32* pTarget, uint32 oldValue, uint32 newValue);

/// Atomically exchanges a pair of 32-bit unsigned integers.
///
/// @param [in,out] pTarget Pointer to the destination value of the operation.
/// @param [in]     value   New value to be stored in *pTarget.
///
/// @returns Previous value at *pTarget.
extern uint32 AtomicExchange(volatile uint32* pTarget, uint32 value);

/// Atomically exchanges a pair of 64-bit unsigned integers.
///
/// @param [in,out] pTarget Pointer to the destination value of the operation.
/// @param [in]     value   New value to be stored in *pTarget.
///
/// @returns Previous value at *pTarget.
extern uint64 AtomicExchange64(volatile uint64* pTarget, uint64 value);

/// Atomically exchanges a pair of pointers.
///
/// @param [in,out] ppTarget Pointer to the address to exchange.  The function sets the address pointed to by *ppTarget
///                          to pValue.
/// @param [in]     pValue   New pointer to be stored in *ppTarget.
///
/// @returns Previous value at *ppTarget.
extern void* AtomicExchangePointer(void*volatile* ppTarget, void* pValue);

/// Atomically add a literal to the specific 32-bit unsigned integer.
///
/// @param [in,out] pAddend Pointer to the value to be modified.
/// @param [in]     value   Literal value to add to *pAddend.
///
/// @returns Result of the add operation.
extern uint32 AtomicAdd(volatile uint32* pAddend, uint32 value);

/// Atomically add a literal to the specified 64-bit unsigned integer.
///
/// @param [in,out] pAddend Pointer to the value to be modified.
/// @param [in]     value   Literal value to add to *pAddend.
///
/// @returns Result of the add operation.
extern uint64 AtomicAdd64(volatile uint64* pAddend, uint64 value);

} // Util
