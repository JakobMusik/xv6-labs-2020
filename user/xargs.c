#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAXLINELEN 512 // maximum supported line length

void run(char** args) {
    if (fork() == 0) {
        exec(args[0], args);
        exit(0);
    }
    wait(0);
}

int main(char argc, char** argv)
{
    if (argc < 2) {
        fprintf(2, "Usage: xargs command [initial-arguments...]\n");
        exit(1);
    }
    char* newArgs[MAXARG] = {};
    for (int i = 1; i < argc; ++i) {
        newArgs[i - 1] = argv[i];
    }
    const int oldArgsCnt = argc - 1;

    char buf[MAXLINELEN * 2];
    char* const bufNow = buf + MAXLINELEN;
    // char* const bufLast = buf;
#define bufLast (bufNow - readSiz)
    int readSiz = 0;
    int newArgIdx = oldArgsCnt;
    int curArgStartIdx = 0;
    char hasArg = 0;
    while ((readSiz = read(0, bufNow, MAXLINELEN)) > 0) {
        for (int i = 0; ; ) {
            if (i >= readSiz) {
                break;
            }
            if (bufNow[i] == ' ' && i != curArgStartIdx) {
                bufNow[i] = '\0';
                newArgs[newArgIdx++] = bufNow + curArgStartIdx;
                do {
                    i++;
                } while (i < readSiz && bufNow[i] == ' ');
                curArgStartIdx = i;
            } else if (bufNow[i] == '\n' && hasArg) {
                bufNow[i] = '\0';
                newArgs[newArgIdx++] = bufNow + curArgStartIdx;
                do {
                    i++;
                } while (i < readSiz && (bufNow[i] == '\n' || bufNow[i] == ' '));
                curArgStartIdx = i;

                newArgs[newArgIdx] = 0; // NULL
                run(newArgs);
                newArgIdx = oldArgsCnt;
                hasArg = 0;
            } else {
                hasArg = 1;
                i++;
            }
        }
        memmove(bufLast, bufNow, readSiz);
        for (int i = oldArgsCnt; i < newArgIdx; ++i) {
            newArgs[i] -= MAXLINELEN;
        }
    }

    exit(0);
}
