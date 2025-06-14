#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define RD 0
#define WR 1
const int END = -1;


// to avoid compiler error: infinite recursion detected
void sieve(int*) __attribute__((noreturn));
void sieve(int* lPipe)
{
    int basePrime;

    if (read(lPipe[RD], &basePrime, sizeof(int)) <= 0) {
        close(lPipe[RD]);
        exit(0);
    }
    printf("prime %d\n", basePrime);

    int rPipe[2], buf;
    pipe(rPipe);
    /**
     * can also put the for loop here, but it'll be sequential processing
     */
    /*
    for (; read(lPipe[RD], &buf, sizeof(int)) > 0; ) {
        if (buf % basePrime) {
            write(rPipe[WR], &buf, sizeof(int));
        }
    }
    */
    
    if (fork() == 0) {
        close(rPipe[WR]);
        sieve(rPipe);
    } else {
        close(rPipe[RD]);
        for (; read(lPipe[RD], &buf, sizeof(int)) > 0; ) {
            if (buf % basePrime) {
                write(rPipe[WR], &buf, sizeof(int));
            }
        }
        close(rPipe[WR]);
        wait(0);
        exit(0);
    }
}

int main(int argc, char** argv)
{
    int initPipe[2];
    pipe(initPipe);
    for (int i = 2; i < 36; ++i) {
        write(initPipe[WR], &i, sizeof(int));
    }

    if (fork() == 0) {
        close(initPipe[WR]);
        sieve(initPipe);
        exit(0);
    } else {
        close(initPipe[RD]);
        close(initPipe[WR]);
    }
    wait(0);
    exit(0);
}
