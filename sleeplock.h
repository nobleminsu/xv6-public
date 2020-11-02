// Long-term locks for processes
struct sleeplock {
  uint locked;       // Is the lock held?
  struct spinlock lk; // spinlock protecting this sleep lock
  struct proc *head;
  
  // For debugging:
  char *name;        // Name of lock.
  int pid;           // Process holding lock
};

