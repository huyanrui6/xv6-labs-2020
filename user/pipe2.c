// inter processes communication

#include "kernel/types.h"
#include "user/user.h"

int main(){
    int fds[2];
    char buf[100];
    int n, pid;

    pipe(fds);

    pid = fork();
    if(pid==0){ // child
        write(fds[1],"This is pipe2\n",14);
    }
    else{
        n = read(fds[0],buf,sizeof(buf));
        write(1,buf,n);
    }

    exit(0);
}