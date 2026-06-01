#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void die(const char *message) {
    perror(message);
    exit(1);
}

static void print_file(int fd) {
    char buffer[256];
    ssize_t count;

    while ((count = read(fd, buffer, sizeof(buffer))) > 0) {
        if (fwrite(buffer, 1, (size_t) count, stdout) != (size_t) count) {
            die("fwrite");
        }
    }

    if (count < 0) {
        die("read");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s MODE ARGS...\n", argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "read") == 0) {
        int fd = open(argv[2], O_RDONLY);
        if (fd < 0) {
            die("open");
        }
        print_file(fd);
        close(fd);
        return 0;
    }

    if (strcmp(argv[1], "access") == 0) {
        if (access(argv[2], R_OK) != 0) {
            die("access");
        }
        puts("ok");
        return 0;
    }

    if (strcmp(argv[1], "stat") == 0) {
        struct stat st;
        if (stat(argv[2], &st) != 0) {
            die("stat");
        }
        printf("%lld\n", (long long) st.st_size);
        return 0;
    }

    if (strcmp(argv[1], "openat-read") == 0 && argc >= 4) {
        int dirfd = open(argv[2], O_RDONLY | O_DIRECTORY);
        int fd;

        if (dirfd < 0) {
            die("open dir");
        }

        fd = openat(dirfd, argv[3], O_RDONLY);
        if (fd < 0) {
            die("openat");
        }
        print_file(fd);
        close(fd);
        close(dirfd);
        return 0;
    }

    fprintf(stderr, "unknown mode: %s\n", argv[1]);
    return 2;
}
