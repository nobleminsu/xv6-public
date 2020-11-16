#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "semaphore.h"

void initsema(struct semaphore *lk, int count)
{
    initlock(&lk->dataLock, "sema-dataLock");
    lk->maxThreadCount = count;
    lk->curThreadCount = 0;
    lk->waitingProcHead = 0;
}

int downsema(struct semaphore *lk)
{
    acquire(&lk->dataLock);
    struct proc *curproc = myproc();

    while (lk->curThreadCount == lk->maxThreadCount)
    {
        // register in the waitingProc list for later use.
        if (!lk->waitingProcHead)
        {
            lk->waitingProcHead = curproc;
        }
        else
        {
            struct proc *node = lk->waitingProcHead;
            while (node->semaNext)
            {
                node = node->semaNext;
            }
            node->semaNext = curproc;
        }
        sleep(lk, &lk->dataLock);
    }

    lk->curThreadCount++;
    release(&lk->dataLock);

    return lk->curThreadCount;
}

int upsema(struct semaphore *lk)
{
    acquire(&lk->dataLock);
    lk->curThreadCount--;
    if (lk->waitingProcHead && lk->waitingProcHead->state == SLEEPING)
    {
        lk->waitingProcHead->state = RUNNABLE;
        lk->waitingProcHead = lk->waitingProcHead->semaNext;
    }
    release(&lk->dataLock);

    return lk->curThreadCount;
}