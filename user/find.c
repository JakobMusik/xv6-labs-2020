#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

static char buf[512];

void find(char* path, char* target) {
    struct dirent de;
    struct stat st;
    int fd;
    char* p;

    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "open %s failed\n", path);
        return;
    }
    if (fstat(fd, &st) < 0) {
        fprintf(2, "fstat %s failed\n", path);
        return;
    }

    switch (st.type) {
    case T_FILE:
        if (strcmp(path + strlen(path) - strlen(target), target) == 0) {
            printf("%s\n", path);
        }
        break;
    case T_DIR:
        if (strlen(path) + DIRSIZ + 2 > sizeof(buf)) {
            fprintf(2, "path too long\n");
            return;
        }
        memcpy(buf, path, strlen(path));
        p = buf + strlen(path);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0) continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if (stat(buf, &st) < 0) {
                fprintf(2, "stat %s failed\n", buf);
                return;
            }
            if (strcmp(buf + strlen(buf) - 2, "/.") != 0 && 
                strcmp(buf + strlen(buf) - 3, "/..") != 0) {
                find(buf, target);
            }

        }
        break;
    }
    close(fd);
}

int main(int argc, char** argv) {

    if (argc < 3) {
        printf("not enough arguments\n");
        exit(0);
    }
    
    char target[512];
    target[0] = '/';
    strcpy(target + 1, argv[2]);
    find(argv[1], target);

    exit(0);

}
