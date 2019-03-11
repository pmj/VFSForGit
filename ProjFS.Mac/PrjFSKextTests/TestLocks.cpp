#include "../PrjFSKext/Locks.hpp"
#include <cassert>

struct __lck_rw_t__
{
};

RWLock RWLock_Alloc()
{
    return RWLock{ new __lck_rw_t__{} };
}

void RWLock_AcquireExclusive(RWLock& lock)
{
}

void RWLock_ReleaseExclusive(RWLock& lock)
{
}

void RWLock_AcquireShared(RWLock& lock)
{
}

void RWLock_DropExclusiveToShared(RWLock& lock)
{
}

void RWLock_ReleaseShared(RWLock& lock)
{
}

bool RWLock_IsValid(RWLock lock)
{
    return lock.p != nullptr;
}

void RWLock_FreeMemory(RWLock* rwLock)
{
    delete rwLock->p;
    rwLock->p = nullptr;
}

void Mutex_Acquire(Mutex mutex)
{
}
void Mutex_Release(Mutex mutex)
{
}
void Mutex_Sleep(int seconds, void* channel, Mutex* mutex)
{
    assert(false);
}
