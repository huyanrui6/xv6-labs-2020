// sleep.c: sleep n seconds

#include"kernel/types.h"
#include"user/user.h"

int main(int argc, char *argv[]) {
    if(argc != 2){
        printf("Must 1 argument for sleep\n");
        exit(1);
    }
    int sleepNum = atoi(argv[1]);
    printtf("(nothing happens for a little while)");
    sleep(sleepNum);
    exit(0);
    return 0;
}