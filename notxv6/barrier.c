#include <stdlib.h>
#include <unistd.h> // POSIX API
#include <stdio.h>
#include <assert.h> //包含断言功能，用于调试
#include <pthread.h> //POSIX线程库，用于创建和管理多线程

static int nthread = 1;
static int round = 0;

struct barrier {
  pthread_mutex_t barrier_mutex; //用于保护共享资源的互斥锁
  pthread_cond_t barrier_cond; //条件变量，用于实现线程等待和唤醒机制
  int nthread;      // 记录已经到达屏障的线程数量 Number of threads that have reached this round of the barrier
  int round;     // Barrier round 记录当前屏障的轮次，用于在多次屏障同步中标识当前轮次
} bstate;

static void
barrier_init(void)
{
  assert(pthread_mutex_init(&bstate.barrier_mutex, NULL) == 0);
  assert(pthread_cond_init(&bstate.barrier_cond, NULL) == 0);
  bstate.nthread = 0;
}

static void 
barrier()
{
  // YOUR CODE HERE
  // 屏障函数，实际的同步逻辑需要在这里实现。目的是当所有线程都调用到 barrier() 时才继续执行，并且更新 round 以表示进入下一轮同步
  // Block until all threads have called barrier() and then increment bstate.round

  // 1 使用互斥锁保护对 bstate.nthread 的访问。
  // 2 每当一个线程到达屏障时，将 bstate.nthread 递增。
  // 3 如果线程还没有全部到达屏障，它们应该在条件变量上等待，直到被唤醒。
  // 4 如果 bstate.nthread 达到总线程数 nthread，说明所有线程都到达了屏障：
  //   a 更新 bstate.round。 
  //   c 重置 bstate.nthread 以准备下一轮屏障同步。
  //   b 通过条件变量唤醒所有等待的线程。

  pthread_mutex_lock(&bstate.barrier_mutex);
  if(++bstate.nthread < nthread){
    pthread_cond_wait(&bstate.barrier_cond,&bstate.barrier_mutex);
  }
  else{ // 所有线程已到达
    bstate.round++;
    bstate.nthread = 0;
    pthread_cond_broadcast(&bstate.barrier_cond);
  }
  pthread_mutex_unlock(&bstate.barrier_mutex);
}

// 每个线程的主函数，模拟了一个循环任务。
// 在每一轮循环中，线程都会调用 barrier()，进行一个随机的延迟操作（通过 usleep() 实现）。
// 断言 assert(i == t); 检查每个线程是否在预期的轮次运行
static void *
thread(void *xa)
{
  long n = (long) xa;
  long delay;
  int i;

  for (i = 0; i < 20000; i++) {
    int t = bstate.round;
    assert (i == t);
    barrier();
    usleep(random() % 100);
  }

  return 0;
}

// 主函数负责解析命令行参数（线程数量）、创建线程、等待线程完成并最终输出结果
int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  long i;
  double t1, t0;

  if (argc < 2) {
    fprintf(stderr, "%s: %s nthread\n", argv[0], argv[0]);
    exit(-1);
  }
  nthread = atoi(argv[1]);
  tha = malloc(sizeof(pthread_t) * nthread);
  srandom(0);

  barrier_init();

  for(i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, thread, (void *) i) == 0);
  }
  for(i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  printf("OK; passed\n");
}
