/**********************************************************
        执行体同步文件KrlExecutorSync.c
***********************************************************
                彭东
**********************************************************/
#include "BaseType.h"
#include "Atomic.h"
#include "List.h"
#include "HalSync.h"
#include "KrlThread.h"
#include "KrlExecutorSync.h"

private void EWaitListHeadInit(EWaitListHead* init)
{
    IF_NULL_RETURN(init);
    INIT_OBJOFPTR_ZERO(init);
    ListInit(&init->Lists);
    return;
}

public void EWaitListInit(EWaitList* init)
{
    IF_NULL_RETURN(init);
    INIT_OBJOFPTR_ZERO(init);
    ListInit(&init->Lists);
    return;
}

private void ESyncInit(ESync* init)
{
    IF_NULL_RETURN(init);
    INIT_OBJOFPTR_ZERO(init);
    SPinLockInit(&init->Lock);
    RefCountInit(&init->SyncCount);
    EWaitListHeadInit(&init->WaitListHead);
    return;
}

public void EsemInit(Esem* init)
{
    IF_NULL_RETURN(init);
    INIT_OBJOFPTR_ZERO(init);
    ESyncInit(&init->Sync);
    return;
}

public void EMutexInit(EMutex* init)
{
    IF_NULL_RETURN(init);
    INIT_OBJOFPTR_ZERO(init);
    ESyncInit(&init->Sync);
    return;
}

private Bool EWaitListAddToEWaitListHead(EWaitListHead* head, EWaitList* wait, void* thread)
{
    IF_NULL_RETURN_FALSE(head);
    IF_NULL_RETURN_FALSE(wait);
    ListAdd(&wait->Lists, &head->Lists);
    head->WaitNR++;
    wait->Parent = head;
    wait->Thread = thread;
    return TRUE;
}


private Bool KrlExEMutexLockedRealizeCore(EMutex* mutex)
{
    CPUFlg cpuflags = 0;
    IF_NULL_RETURN_FALSE(mutex);

    ESyncSelfLockedCli(&mutex->Sync, &cpuflags);
    if(ESyncCountIsEQTOne(&mutex->Sync) == FALSE)
    {
        ESyncSelfUnLockSti(&mutex->Sync, &cpuflags);
        return FALSE;
    }
    
    if(ESyncCountDec(&mutex->Sync) == FALSE)
    {
        ESyncSelfUnLockSti(&mutex->Sync, &cpuflags);
        return FALSE;
    }
    
    ESyncSelfUnLockSti(&mutex->Sync, &cpuflags);
    return TRUE;
}

private Bool KrlExEMutexLockedFailEntryWait(EMutex* mutex, EWaitList* wait, Thread* thread)
{
    IF_NULL_RETURN_FALSE(mutex);
    IF_NULL_RETURN_FALSE(wait);
    IF_NULL_RETURN_FALSE(thread);
    
    IF_NEQ_RETURN(TRUE, EWaitListAddToEWaitListHead(&mutex->Sync.WaitListHead, wait, (void*)thread), FALSE);
    return TRUE;
}

private Bool KrlExEMutexLockedWaitRealizeCore(EMutex* mutex)
{
    CPUFlg cpuflags = 0;
    Thread* thread = NULL;
    IF_NULL_RETURN_FALSE(mutex);

start:
    ESyncSelfLockedCli(&mutex->Sync, &cpuflags);

    if(ESyncCountIsEQTZero(&mutex->Sync) == TRUE)
    {
        thread = KrlExGetCurrentThread();
        IF_NULL_RETURN_FALSE(thread);
        IF_NEQ_DEAD(TRUE, 
            KrlExEMutexLockedFailEntryWait(mutex, &thread->Affiliation.WaitList, thread), 
            "KrlExEMutexLockedFailEntryWait Is Fail\n");
        ESyncSelfUnLockSti(&mutex->Sync, &cpuflags);
        KrlExThreadWait(thread);
        goto start;
    }
    
    if(ESyncCountIsEQTOne(&mutex->Sync) == FALSE)
    {
        ESyncSelfUnLockSti(&mutex->Sync, &cpuflags);
        return FALSE;
    }
    
    if(ESyncCountDec(&mutex->Sync) == FALSE)
    {
        ESyncSelfUnLockSti(&mutex->Sync, &cpuflags);
        return FALSE;
    }
    
    ESyncSelfUnLockSti(&mutex->Sync, &cpuflags);
    return TRUE;
}

private Bool KrlExEMutexLockedRealize(EMutex* mutex, UInt flags)
{
    if(MUTEX_FLG_NOWAIT == flags)
    {
        mutex->Flags = flags;
        return KrlExEMutexLockedRealizeCore(mutex);
    }
    else if(MUTEX_FLG_WAIT == flags)
    {
        mutex->Flags = flags;
        return KrlExEMutexLockedWaitRealizeCore(mutex);
    }
    return FALSE;
}

public Bool KrlExEMutexLocked(EMutex* mutex, UInt flags)
{
    IF_NULL_RETURN_FALSE(mutex);
    return KrlExEMutexLockedRealize(mutex, flags);
}

private Bool KrlExEMutexUnLockRealizeCore(EMutex* mutex)
{
    CPUFlg cpuflags = 0;
    IF_NULL_RETURN_FALSE(mutex);

    ESyncSelfLockedCli(&mutex->Sync, &cpuflags);
    if(ESyncCountIsEQTZero(&mutex->Sync) == FALSE)
    {
        ESyncSelfUnLockSti(&mutex->Sync, &cpuflags);
        return FALSE;
    }
    
    if(ESyncCountInc(&mutex->Sync) == FALSE)
    {
        ESyncSelfUnLockSti(&mutex->Sync, &cpuflags);
        return FALSE;
    }
    
    ESyncSelfUnLockSti(&mutex->Sync, &cpuflags);
    return TRUE;
}

private Bool KrlExEMutexUnLockRealize(EMutex* mutex)
{
    return KrlExEMutexUnLockRealizeCore(mutex);
}

public Bool KrlExEMutexUnLock(EMutex* mutex)
{
    IF_NULL_RETURN_FALSE(mutex);
    return KrlExEMutexUnLockRealize(mutex);
}