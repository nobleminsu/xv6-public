struct semaphore
{
    int maxThreadCount;
    int curThreadCount;
    struct spinlock dataLock;
    struct proc *waitingProcHead;

    // For debugging:
    char *name; // Name of lock.
    int pid;    // Process holding lock
};