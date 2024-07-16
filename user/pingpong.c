#include"kernel/types.h"
#include"user/user.h"

int main(){
    // create 2 pipes p[0]read p[1]write
    int p2c[2], c2p[2];
    pipe(p2c);
    pipe(c2p);
    char buf = 'a';

    if(fork()!=0){
        // parent write then read
        close(p2c[0]);
        close(c2p[1]);

        write(p2c[1],&buf, 1);

        read(c2p[0], &buf, 1);
        printf("%d: received pong\n",getpid());
        wait(0);
    }
    else{
        // child read then write
        close(p2c[1]);
        close(c2p[0]);

        read(p2c[0],&buf,1);
        printf("%d: received ping\n",getpid());
        write(c2p[1],&buf,1);
    }
    exit(0);
}