#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "rwsemaphore.h"

void initrwsema(struct rwsemaphore *lk)
{
    initlock(&lk->dataLock, "sema_dataLock");
    lk->readingThreadCount = 0;
    lk->isWriting = 0;
    lk->readWaitingHead = 0;
    lk->writeWaitingHead = 0;
}

void downreadsema(struct rwsemaphore *lk)
{
    acquire(&lk->dataLock);
    struct proc *curproc = myproc();

    // if write is waiting, don't quick start reading
    while (lk->isWriting || lk->writeWaitingHead != 0)
    {
        // register in the waitingProc list for later use.
        if (!lk->readWaitingHead)
        {
            lk->readWaitingHead = curproc;
        }
        else
        {
            struct proc *node = lk->readWaitingHead;
            while (node->rwsemaNext)
            {
                node = node->rwsemaNext;
            }
            node->rwsemaNext = curproc;
        }
        sleep(lk, &lk->dataLock);
    }

    lk->readingThreadCount++;
    release(&lk->dataLock);
}

void upreadsema(struct rwsemaphore *lk)
{
    acquire(&lk->dataLock);
    lk->readingThreadCount--;
    if (lk->readingThreadCount == 0 && (!lk->isWriting && lk->writeWaitingHead))
    {
        lk->writeWaitingHead->state = RUNNABLE;
        lk->writeWaitingHead = lk->writeWaitingHead->rwsemaNext;
    }
    release(&lk->dataLock);
}

void downwritesema(struct rwsemaphore *lk)
{
    acquire(&lk->dataLock);
    struct proc *curproc = myproc();

    while (lk->isWriting || lk->readingThreadCount > 0)
    {
        // register in the waitingProc list for later use.
        if (!lk->writeWaitingHead)
        {
            lk->writeWaitingHead = curproc;
        }
        else
        {
            struct proc *node = lk->writeWaitingHead;
            while (node->rwsemaNext)
            {
                node = node->rwsemaNext;
            }
            node->rwsemaNext = curproc;
        }
        sleep(lk, &lk->dataLock);
    }

    lk->isWriting = 1;
    release(&lk->dataLock);
}

void upwritesema(struct rwsemaphore *lk)
{
    acquire(&lk->dataLock);
    lk->isWriting = 0;
    if (lk->writeWaitingHead)
    {
        lk->writeWaitingHead->state = RUNNABLE;
        lk->writeWaitingHead = lk->writeWaitingHead->rwsemaNext;
    }
    else if (lk->readWaitingHead)
    {
        // wake all reading thread
        struct proc *node = lk->readWaitingHead;
        node->state = RUNNABLE;
        while (node->rwsemaNext)
        {
            node = node->rwsemaNext;
            node->state = RUNNABLE;
        }
        lk->readWaitingHead = 0;
    }
    release(&lk->dataLock);
}