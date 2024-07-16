// forkexec.c: fork then exec

#include"kernel/types.h"
#include"user/user.h"

int main(){
    int pid, status;
    pid = fork();

    if(pid == 0){
        char *argv[] = {"echo","THIS","IS","ECHO",0};
        exec("echo", argv);
        printf("echo failed\n");
        exit(1);
    }
    else {
        printf("parent waiting\n");
        wait(&status);
        printf("child exited with status %d\n",status);
    }
}