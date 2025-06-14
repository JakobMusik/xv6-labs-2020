#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main(int argc, char** argv)
{
    int p_down[2], p_up[2];
    const int RD = 0;
    const int WR = 1;
    const int buf_siz = 1;
    char buf[buf_siz];
    pipe(p_down);
    pipe(p_up);
    
    int pid = fork();
    if (pid < 0) {
        printf("fork() failed!\n");
        close(p_down[RD]);
        close(p_down[WR]);
        close(p_up[RD]);
        close(p_up[WR]);
        exit(1);
    } else if (pid == 0) {  // child
        read(p_down[RD], buf, sizeof(char));
        close(p_down[RD]);
        printf("%d: received ping\n", getpid());
        write(p_up[WR], "1", sizeof(char));
        close(p_up[WR]);
    } else {                // parent
        write(p_down[WR], "1", sizeof(char));
        close(p_down[WR]);
        read(p_up[RD], buf, sizeof(char));
        close(p_up[RD]);
        printf("%d: received pong\n", getpid());
    }
    exit(0);
}
