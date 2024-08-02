// 栈和中断寄存器：为每个 CPU 分配独立的 stack0 栈和 mscratch 区域。
// 启动函数 start：prepare for mret 初始化 CPU 的寄存器和状态，并跳转到 main 函数。
// 定时器初始化 timeinit：设置定时器中断，使每个 CPU 定期触发定时器中断。

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main();
void timerinit();

// entry.S needs one stack per CPU.
// 每个 CPU 都有一个对齐到 16 字节的 4096 字节的栈
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// scratch area for timer interrupt, one per CPU.
// 每个 CPU 的定时器中断都有一个单独的 mscratch 区域
uint64 mscratch0[NCPU * 32];

// assembly code in kernelvec.S for machine-mode timer interrupt.
extern void timervec();

// entry.S jumps here in machine mode on stack0.
void
start()
{
  // set M Previous Privilege mode to Supervisor (MPP -> S), for mret.
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // set M Exception Program Counter to main, for mret.
  // requires gcc -mcmodel=medany
  w_mepc((uint64)main);

  // disable paging for now.
  w_satp(0);

  // delegate all interrupts and exceptions to supervisor mode.
  // 将所有中断和异常委托给S，并启用S的external timer software interrupts
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // ask for clock interrupts.
  timerinit();

  // stroe each CPU's hartid in its tp register, for cpuid().
  int id = r_mhartid();
  w_tp(id);

  // switch to supervisor mode and jump to main().
  // 内联汇编指令，用于在start的最后切换 CPU 的特权级别to S，并跳转到 main 函数
  // asm 关键字用于在 C 代码中插入汇编代码
  // volatile 关键字告诉编译器不要优化这条指令，因为它有重要的副作用
  // mret 是 RISC-V 指令集中的一条指令，表示从机器模式（M模式）返回。
  // mret 指令会从 mepc 读取地址并跳转到该地址执行, 同时根据 mstatus 切换到指定的特权级别。
  asm volatile("mret");
}

// timerinit 设置计时器中断，使其能够在机器模式下运行
// 当计时器中断发生时，它会调用 kernelvec.S 的 timervec 函数
// timervec 函数将计时器中断转换为软件中断
// 软件中断会由 trap.c 文件中的 devintr 函数处理
// set up to receive timer interrupts in machine mode,
// which arrive at timervec in kernelvec.S,
// which turns them into software interrupts for devintr() in trap.c.
void
timerinit()
{
  // each CPU has a separate source of timer interrupts.
  int id = r_mhartid();

  // ask the CLINT for a timer interrupt.
  int interval = 1000000; // cycles; about 1/10th second in qemu.
  *(uint64*)CLINT_MTIMECMP(id) = *(uint64*)CLINT_MTIME + interval;

  // prepare information in scratch[] for timervec. scratch ~ trapframe
  // scratch[0..3] : space for timervec to save registers.
  // scratch[4] : address of CLINT MTIMECMP register.
  // scratch[5] : desired interval (in cycles) between timer interrupts.
  uint64 *scratch = &mscratch0[32 * id];
  scratch[4] = CLINT_MTIMECMP(id);
  scratch[5] = interval;
  w_mscratch((uint64)scratch);

  // set the machine-mode trap handler.
  w_mtvec((uint64)timervec);

  // enable machine-mode interrupts.
  w_mstatus(r_mstatus() | MSTATUS_MIE);

  // enable machine-mode timer interrupts.
  w_mie(r_mie() | MIE_MTIE);
}
