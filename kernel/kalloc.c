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
} kmem;

struct {
  struct spinlock lock; // 保证操作的原子性
  int ref_count[(PGROUNDUP(PHYSTOP)) / PGSIZE]; // KERNBASE～PHYSTOP是物理内存的大小，因为xv6的内核地址采用了直接映射，为了方便，这里直接使用PHYSTOP。PHYSTOP / PGSIZE则表示有多少个物理页
} kref;

// 内核在使用kinit()进行初始化时，需要初始化kref的锁，并设置引用计数数组的值
void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&kmem.lock, "kref");
  // 这里初始化时置为1是为了接下来的freerange在调用kfree时不会触发panic
  for (int i = 0; i < PGROUNDUP(PHYSTOP) / PGSIZE; i++) {
    kref.ref_count[i] = 1;
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
// 在kfree()中对物理页的引用计数来判断是否应该释放物理内存
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // 如果在还未对ref count操作前其值已经小于等于0，说明已有问题
  if (kref.ref_count[(uint64)pa / PGSIZE] <= 0) {
    panic("kref");
  }
  // 每次free一个page时，先将这个page的引用计数-1
  kref_dec(pa);
  // ref count - 1，如果结果还大于0，说明这个物理页还被其他进程引用，暂时不需要释放
  if (kref.ref_count[(uint64)pa / PGSIZE] > 0) {
    return;
  }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
// 每分配一个物理页时，该物理页的引用计数初始为1
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    // 这里赋值为1一定要在设置垃圾数值之后，否则会造成污染
    acquire(&kref.lock);
    kref.ref_count[(uint64)r / PGSIZE] = 1;
    release(&kref.lock);
  }
  return (void*)r;
}

// 为pa所在的page的引用+1
void kref_inc(void* pa) {
  acquire(&kref.lock);
  ++kref.ref_count[(uint64)pa / PGSIZE];
  release(&kref.lock);
}

// 为pa所在的page的引用-1
void kref_dec(void* pa) {
  acquire(&kref.lock);
  --kref.ref_count[(uint64)pa / PGSIZE];
  release(&kref.lock);
}

// page fault 时分配新内存:
// 当需要分配新内存时，要检测虚拟页的cow标志位是否有效。
// 在分配了内存后，拷贝原来的物理页中的数据到新的物理页，并将虚拟页的标志位中的cow去除、添加写标志，
// 最后取消虚拟页与原来物理页的映射关系，同时在解除时对引用计数-1，然后将虚拟页映射至新的物理页
// 当出现page fault时，进行cow操作
int cow_alloc(pagetable_t pagetable, uint64 va) {
  if (va >= MAXVA) {
    return -1;
  }

  if ((va % PGSIZE) != 0) {
    return -1;
  }

  pte_t *pte = walk(pagetable, va, 0);
  if (pte == 0)
    return -1;

  uint64 pa = PTE2PA(*pte);
  if (pa == 0)
    return -1;

  // 当page的cow标志位有效时才会重新分配内存
  if ((*pte & PTE_COW) && (*pte & PTE_V)) {
    char* mem = kalloc();
    if (mem == 0) {
      return -1;
    }
    uint64 flags = PTE_FLAGS(*pte);
    flags = (flags & ~PTE_COW) | PTE_W; // 去除COW标记，加上写权限标记
    memmove(mem, (char *)pa, PGSIZE);
    uvmunmap(pagetable, PGROUNDDOWN(va), 1, 1); // 解除之前的映射，并设置do_free为1，这样在kfree中可来将引用计数-1
    if (mappages(pagetable, va, PGSIZE, (uint64)mem, flags) < 0) {
      panic("cow_alloc");
    }
  }
  return 0;
}