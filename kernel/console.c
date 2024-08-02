//
// Console input and output, to the uart.
// Reads are line at a time. 按行读取输入
// Implements special input characters:
//   newline -- end of line 换行符
//   control-h -- backspace
//   control-u -- kill line
//   control-d -- end of file
//   control-p -- print process list
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

#define BACKSPACE 0x100
#define C(x)  ((x)-'@')  // Control-x 将字符 x 转换为控制字符

//
// uartputc_sync (not interrupt, spin wait, send immediately)
// send one character to the uart.
// called by printf, and to echo input characters,
// but not from write().
//
void
consputc(int c)
{
  if(c == BACKSPACE){
    // if the user typed backspace, overwrite with a space.
    uartputc_sync('\b'); uartputc_sync(' '); uartputc_sync('\b');
  } else {
    uartputc_sync(c);
  }
}

// cons 结构体用于管理控制台的输入缓冲区 console input buf
struct {
  struct spinlock lock;
  
  // input
#define INPUT_BUF 128
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} cons;

//
// user write()s to the console.
// 用户向控制台写 从用户空间读取数据并通过 UART 发送。
  // int user_src: 源是1否0在用户空间。
  // uint64 src: 源数据的地址。
  // int n: 要写入的数据长度。
  // return i: 实际写入的字节数。
//
int
consolewrite(int user_src, uint64 src, int n)
{
  int i;

  acquire(&cons.lock);
  for(i = 0; i < n; i++){
    char c;
    if(either_copyin(&c, user_src, src+i, 1) == -1)
      break;
    uartputc(c);
  }
  release(&cons.lock);

  return i;
}

//
// 从控制台读取用户输入 将输入的一整行（最多 n 字节）复制到u/k目标缓冲区dst中。
// user read()s from the console.
// copy (up to n) a whole input line to dst.
// user_dst: 1 user 0 kernel address.
// 返回实际读取的字节数。读取过程中进程被杀死返回-1。
//
int
consoleread(int user_dst, uint64 dst, int n)
{
  uint target; //保存要读取的总字节数n
  int c; //存储从输入缓冲区读取的字符
  char cbuf; //存储即将复制到目标缓冲区的字符 src

  target = n;
  acquire(&cons.lock);
  while(n > 0){
    // 等待输入:
    // wait until interrupt handler has put some
    // input into cons.buffer.
    while(cons.r == cons.w){
      // 如果缓冲区为空（cons.r == cons.w），则进入等待状态sleep，直到有新的输入。
      // 如果当前进程被杀死，释放锁并返回-1。
      if(myproc()->killed){
        release(&cons.lock);
        return -1;
      }
      sleep(&cons.r, &cons.lock);
    }

    // 从缓冲区 cons.buf 中读取字符 c，并更新 cons.r。
    c = cons.buf[cons.r++ % INPUT_BUF];

    //如果读取到 EOF 字符（C('D')），检查是否读取部分数据。如果是，则将读取索引回退一位，并退出循环。
    if(c == C('D')){  // end-of-file
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        cons.r--;
      }
      break;
    }

    // copy the input byte to the user-space buffer.
    // 将读取的字符 c 复制到目标缓冲区 dst 中
    cbuf = c;
    if(either_copyout(user_dst, dst, &cbuf, 1) == -1)
      break;

    dst++;
    --n;

    if(c == '\n'){
      // a whole line has arrived, return to
      // the user-level read().
      break;
    }
  }
  release(&cons.lock);

  return target - n;
}

//
// the console input interrupt handler.
// job: accumulate input chars in cons.buf until a whole line arrives
// uartintr() calls this for input character.
// do erase/kill processing, append to cons.buf,
// wake up consoleread() if a whole line has arrived.
//
void
consoleintr(int c)
{
  acquire(&cons.lock);

  switch(c){
  case C('P'):  // Print process list.
    procdump();
    break;
  case C('U'):  // Kill line.
    // 删除当前行的所有字符，直到缓冲区为空或遇到换行符。
    // while(缓冲区不空&&不是换行符)
    while(cons.e != cons.w &&
          cons.buf[(cons.e-1) % INPUT_BUF] != '\n'){
      cons.e--;
      consputc(BACKSPACE);
    }
    break;
  case C('H'): // Ctrl+H 
  case '\x7f': // Backspace
    if(cons.e != cons.w){
      cons.e--;
      consputc(BACKSPACE);
    }
    break;
  default:
    // 如果字符 c != 0 且缓冲区未满，将字符存入缓冲区。
    if(c != 0 && cons.e-cons.r < INPUT_BUF){
      // if c == '\r' -> '\n' else c
      c = (c == '\r') ? '\n' : c;

      // echo back to the user.将字符回显给用户
      consputc(c);

      // store for consumption by consoleread().
      // 将字符c存入缓冲区cons.buf中，并更新cons.e
      cons.buf[cons.e++ % INPUT_BUF] = c;

      if(c == '\n' || c == C('D') || cons.e == cons.r+INPUT_BUF){
        // wake up consoleread() if a whole line '\n' or end-of-file Ctrl+D has arrived.
        cons.w = cons.e;
        wakeup(&cons.r);
      }
    }
    break;
  }
  
  release(&cons.lock);
}

void
consoleinit(void)
{
  initlock(&cons.lock, "cons");

  uartinit();

  // connect read and write system calls
  // to consoleread and consolewrite.
  // 将设备表 devsw 中控制台的读取和写入操作分别指向 consoleread 和 consolewrite 函数。
  devsw[CONSOLE].read = consoleread;
  devsw[CONSOLE].write = consolewrite;
}
