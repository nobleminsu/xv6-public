#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  case T_PGFLT:
    cprintf("detected pf @%p, eip=%p\n", rcr2(), tf->eip);
    // heap segment
    if (rcr2() < myproc()->sz && myproc()->oldsz < myproc()->sz)
    {
      // pagefault occured in the current process, not yet allocated
      allocuvm(myproc()->pgdir, myproc()->oldsz, myproc()->sz, 1);
      myproc()->oldsz = myproc()->sz;
      switchuvm(myproc());
      break;
    }
    
    struct proc_segment_map* psegmap;
    int i;
    int hit;
    char *target_page;
    target_page = PGROUNDDOWN(rcr2());

    // text, data segment
    for (i = 0, psegmap = &myproc()->psegmaps[i]; i < myproc()->phnum; i++, psegmap = &myproc()->psegmaps[i])
    {
      if (psegmap->vaddr <= target_page && target_page <= psegmap->vaddr + psegmap->memsz)
      {
        cprintf("text, data seg hit on %p - %p for %p, file=%p - %p\n",
                psegmap->vaddr, psegmap->vaddr + psegmap->memsz, rcr2(),
                psegmap->off, psegmap->off + psegmap->filesz);
        hit = 1;
        // allocuvm(myproc()->pgdir, psegmap->vaddr, psegmap->vaddr + psegmap->memsz, 1);
        // loaduvm(myproc()->pgdir, (char *)psegmap->vaddr, myproc()->p_file, psegmap->off, psegmap->filesz);

        if (target_page + PGSIZE > psegmap->vaddr + psegmap->memsz)
        {
          // if remaining program segment is smaller than a page
          allocuvm(myproc()->pgdir, target_page, psegmap->vaddr + psegmap->memsz, 1);
          loaduvm(myproc()->pgdir, target_page, myproc()->p_file,
                  target_page - psegmap->vaddr + psegmap->off,
                  psegmap->filesz - ((uint)target_page - psegmap->vaddr));
        }
        else
        {
          allocuvm(myproc()->pgdir, target_page, target_page + PGSIZE, 1);
          loaduvm(myproc()->pgdir, target_page, myproc()->p_file,
                  target_page - psegmap->vaddr + psegmap->off, PGSIZE);
          cprintf("mapped %p-%p to %p-%p\n", target_page - psegmap->vaddr + psegmap->off,
                  target_page - psegmap->vaddr + psegmap->off + PGSIZE,
                  target_page, target_page + PGSIZE);
        }
        break;
      }
    }
    if (hit)
      break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
