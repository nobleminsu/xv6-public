#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  // cprintf("sbrk call addr=%p n=%d\n", addr, n);
  if (n < 0)
  {
    // growproc(n);
    pte_t* pte;
    uint pa;
    uint a = PGROUNDUP(myproc()->sz + n);
    // cprintf("newsize=%p\n",a);
    for (; a < myproc()->sz; a += PGSIZE)
    {
      // cprintf("dealloc %p\n", a);
      pte = walkpgdir(myproc()->pgdir, (char *)a, 0);
      if ((*pte & PTE_P) != 0)
      {
        pa = PTE_ADDR(*pte);
        if (pa == 0)
          panic("kfree");
        char *v = P2V(pa);
        kfree(v);
        *pte = 0;
      }
    }
    myproc()->oldsz = myproc()->sz + n;
    myproc()->sz += n;
  }
  else if (n > 0)
  {
    myproc()->oldsz = myproc()->sz;
    myproc()->sz += n;
  }
  // if(growproc(n) < 0)
  //   return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
