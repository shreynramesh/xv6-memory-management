#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "mmap.h"

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

void pgflt_handler(struct trapframe *tf) {
  // cprintf("Entering pg flt. handler...\n");
  struct proc* p = myproc();
  struct VMA* vma = p->vma;
  int fault_addr = rcr2();

  // Looping thorugh process vmas to find which vma to grow up, if applicable
  for(int i = 0; i < 32; i++) {
    // cprintf("Fault Addr: %pd, start: %p end: %p\n", fault_addr, vma[i].addr, vma[i].addr + vma[i].length);
    if(vma[i].used && vma[i].flags & MAP_GROWSUP) {
      if((fault_addr >= (vma[i].addr + vma[i].length)) && fault_addr < (vma[i].addr + vma[i].length + PGSIZE)) { // In range for a growup 
        // cprintf("Fault Addr: %p, start: %p end: %p, end w/ guard: %p\n", fault_addr, vma[i].addr, vma[i].addr + vma[i].length, vma[i].addr + vma[i].length + PGSIZE);

        // Check if there is space for a growup
        int va = PGROUNDDOWN(vma[i].addr + vma[i].length + PGSIZE);

        if(va >= KERNBASE) {
          cprintf("Segmentation Fault\n");
          exit();
        }

        for(int j = 0; j < 32; j++) {
          if(vma[j].used &&  j != i) {
            int nextpg = va + PGSIZE;
            // cprintf("Next Pg: %p, VMA start: %p end: %p\n", (void*) nextpg, vma[j].addr, vma[j].addr + vma[j].length);
            if(nextpg >= vma[j].addr) {
                cprintf("Segmentation Fault\n");
                exit();
            }
          }
        }      
        int mem = (int) kalloc();
        if(mem == 0) {
          return;
        }
        // int pa = PTE_ADDR(walkpgdir(p->pgdir, (void*) PGROUNDDOWN(vma[i].addr + vma[i].length), 0)) + PGSIZE;
        // int pa_test = PTE_ADDR(walkpgdir(p->pgdir, (void*) vma[i].addr, 0));
        // cprintf("PA: %p PA_Test: %p\n", (void*) pa, (void*) pa_test);
        // Throw Segmentation Fault if pa is != 0
        // if(pa != 0) {
        //   cprintf("Segmentation Fault\n");
        //   exit();
        // }

        // Map new page 
        // cprintf("Growing: va start: %p, end: %p pa start: %p end: %p\n", (void*) va, (void*) va + PGSIZE, (void*) V2P(mem), (void*) V2P(mem) + PGSIZE);
        mappages(p->pgdir, (void*) va, PGSIZE, V2P(mem), vma[i].prot | PTE_U);
        vma[i].length += PGSIZE;
        switchuvm(p);
        return;
      }
    }
  }

  // User did not access a valid address in gaurd pages
  cprintf("Segmentation Fault\n");
  exit();
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
    pgflt_handler(tf);
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
