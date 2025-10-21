#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAXARGS 64
#define MAXPARAMLEN 256

int main(char argc, char** argv)
{
    if (argc < 2) {
        fprintf(2, "Usage: xargs command [initial-arguments...]\n");
        exit(1);
    }

    char* new_args[MAXARGS];
    for (int i = 1; i < argc; ++i) {
      new_args[i - 1] = argv[i];
    }
    
    int arg_count = argc - 1;

    char param_buf[MAXPARAMLEN];
    int n = read(0, param_buf, sizeof(param_buf) - 1);
    if (n < 0) {
        fprintf(2, "xargs: read error\n");
        exit(1);
    }
    param_buf[n] = '\0';

    // parse parameters by whitespace
    char* p = param_buf;
    const int old_param_count = arg_count;
    while (p - param_buf < n) {
        // skip leading whitespace
        while (*p && (*p == ' ' || *p == '\n' || *p == '\t')) {
          p++;
        }
        if (*p == '\0') {
          p++;
          continue;
        }
        // save the start of a new parameter
        new_args[arg_count++] = p;

        // find the end of the parameter
        while (*p && (*p != ' ' && *p != '\n' && *p != '\t')) {
          p++;
        }
        if (*p == ' ' || *p == '\t' || *p == '\0') { // more parameters to come on this line
            *p = '\0';
            p++;
            continue;
        }
        if (*p == '\n') { // line break -> invoke exec
            *p = '\0';
            p++;
            if (fork() == 0) {
              new_args[arg_count] = 0;
              exec(new_args[0], new_args);
              fprintf(2, "xargs: exec %s failed\n", new_args[0]);
              exit(1);
            }
            wait(0);
            arg_count = old_param_count; // reset to initial arguments
        }
    }

    exit(0);
}
