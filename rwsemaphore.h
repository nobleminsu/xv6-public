struct rwsemaphore
{
    int readingThreadCount;
    int isWriting;
    struct proc *writeWaitingHead;
    struct proc *readWaitingHead;
    struct spinlock dataLock;
};