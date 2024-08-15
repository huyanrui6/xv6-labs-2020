#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"
#include "kernel/sysinfo.h"

int main(int argc, char const *argv[])
{
    if (argc != 1)
    {
        fprintf(2, "sysinfo need not param\n", argv[0]);
        exit(0);
    }

    struct sysinfo info;
    if (sysinfo(&info) < 0){
        printf("FAIL: sysinfo failed\n");
        exit(1);
    }
    printf("freemem: %d, nproc: %d\n",info.freemem,info.nproc);
    
    exit(0);
}
