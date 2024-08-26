// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU]; // 为每个 CPU 分配独立的 freelist，并用独立的锁保护它。

void
kinit()
{
  // 初始化时要对每个kmem都初始锁
  char lockname[NCPU];
  for(int i=0; i<NCPU; i++){
    snprintf(lockname, sizeof(lockname), "kmem_%d",i);
    initlock(&kmem[i].lock, "lockname");
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // 使用cpuid()和它返回的结果时必须关中断
  push_off();
  int cid = cpuid();
  acquire(&kmem[cid].lock);
  r->next = kmem[cid].freelist;
  kmem[cid].freelist = r;
  release(&kmem[cid].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
// 修改kalloc，使得在当前CPU的空闲列表没有可分配内存时窃取其他内存的
void *
kalloc(void)
{
  struct run *r;
  push_off(); // 关中断
  int cid = cpuid();
  acquire(&kmem[cid].lock);
  r = kmem[cid].freelist;
  if(r) // 如果当前的freelist中还有内存块，则直接用
    kmem[cid].freelist = r->next;
  else{  
    // 没有？那就拿！steal
    // 遍历一下其他CPU的kmem，如果找到的freelist中还有内存块，就拿它的
    
    int aid; // another CPU id
    for(aid=0; aid<NCPU; aid++){ // 遍历所有CPU的空闲列表
      if(aid == cid)  continue;
      
      acquire(&kmem[aid].lock);
      r = kmem[aid].freelist;
      if(r){
        kmem[aid].freelist = r->next;
        release(&kmem[aid].lock);
        break;
      }
      release(&kmem[aid].lock);
    }
  }
  release(&kmem[cid].lock);
  pop_off();  //开中断
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
