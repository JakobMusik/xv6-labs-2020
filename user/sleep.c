#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc != 2) {
    const char* str = "usage: sleep <time to sleep>\n";
    write(1, str, strlen(str));
    exit(1);
  }

  int sleepTime = atoi(argv[1]);
  if (sleep(sleepTime) == -1) {
    const char* str = "sleep failed\n";
    write(1, str, strlen(str));
  }

  exit(0);
}
