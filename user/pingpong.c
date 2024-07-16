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

        if(write(p2c[1],&buf, 1) !=1 ){
            fprintf(2,"parent write error\n");
            exit(1);
        }

        if(read(c2p[0], &buf, 1) != 1){
            fprintf(2,"parent read error\n");
            exit(1);
        }
        else printf("%d: received pong\n",getpid());
        wait(0);
        exit(0);
    }
    else{
        // child read then write
        close(p2c[1]);
        close(c2p[0]);

        if(read(p2c[0],&buf,1) != 1){
            fprintf(2,"child read error\n");
            exit(1);
        }
        else printf("%d: received ping\n",getpid());
        
        if(write(c2p[1],&buf,1) != 1){
            fprintf(2,"child write error\n");
            exit(1);
        }
        exit(0);
    }

}