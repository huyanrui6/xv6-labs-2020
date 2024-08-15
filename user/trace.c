/*该程序接收一个掩码和一个命令。掩码用于控制跟踪行为，而命令则是用户希望执行的被跟踪程序。
首先 检查输入的参数是否合法。如果不合法，给出提示并退出。
然后 调用系统调用 trace，传入掩码以启动跟踪。
接着 程序构造要执行的命令并使用 exec 执行它，这会替换当前进程映像为被跟踪程序。
一旦 被跟踪的命令执行完毕，trace 程序就会退出。
这个代码的核心是利用 trace 系统调用来启动并跟踪另一个程序。掩码（mask）可以用来指定需要跟踪的事件或行为（例如系统调用）。*/
//eg $ trace 32 grep hello README
//这意味着使用掩码 32 跟踪 grep hello README 命令的执行

#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int i;
  char *nargv[MAXARG];

  /*检查命令行参数的合法性。
  argc < 3：确保至少有三个参数（即掩码和命令）。
  (argv[1][0] < '0' || argv[1][0] > '9')：确保第二个参数（掩码）是一个数字字符。
  如果参数不符合要求，程序会输出一个错误信息并退出。*/
  if(argc < 3 || (argv[1][0] < '0' || argv[1][0] > '9')){
    fprintf(2, "Usage: %s mask command\n", argv[0]);
    exit(1);
  }

  /*trace(atoi(argv[1])) 调用一个跟踪系统调用 syscall trace，
  传入参数是将第二个命令行参数（掩码）转换为整数。
  如果 trace 系统调用失败（返回值小于 0），程序输出错误信息并退出。*/
  if (trace(atoi(argv[1])) < 0) {
    fprintf(2, "%s: trace failed\n", argv[0]);
    exit(1);
  }
  
  //将命令行中的参数argv复制到nargv中，同时删去前两个参数
  //exec(nargv[0], nargv) 执行指定的命令（例如 "grep hello README"），并替换当前进程映像。
  for(i = 2; i < argc && i < MAXARG; i++){
    nargv[i-2] = argv[i];
  }
  exec(nargv[0], nargv);
  exit(0);
}
