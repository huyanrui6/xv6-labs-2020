// redirect.c

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(){
    int pid;
    pid = fork();

    if(pid==0) {
        // IO redirection: stdout 1 -> op.txt
        close(1);
        open("op.txt", O_WRONLY|O_CREATE);

        char *argv[]={"echo","This","is","redirected","echo",0};
        exec("echo",argv);
        printf("exec failed\n");
        exit(1);
    }
    else{
        wait((int*)0);
    }
    exit(0);
    return 0;
}