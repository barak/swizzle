#define _GNU_SOURCE

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(const char *argv0) {
    fprintf(stderr, "usage: %s SRC:DST [SRC:DST ...] -- PROGRAM [ARG ...]\n", argv0);
}

static int set_mapping_env(int index, const char *mapping) {
    const char *separator = strchr(mapping, ':');
    char name[64];
    char *source;

    if (separator == NULL || separator == mapping || separator[1] == '\0') {
        return -1;
    }

    source = strndup(mapping, (size_t) (separator - mapping));
    if (source == NULL) {
        return -1;
    }

    snprintf(name, sizeof(name), "SWIZZLE_SRC_%d", index);
    if (setenv(name, source, 1) != 0) {
        free(source);
        return -1;
    }

    snprintf(name, sizeof(name), "SWIZZLE_DST_%d", index);
    if (setenv(name, separator + 1, 1) != 0) {
        free(source);
        return -1;
    }

    free(source);
    return 0;
}

static int set_preload(void) {
    char exe_path[PATH_MAX];
    char lib_path[PATH_MAX];
    char *slash;
    ssize_t length;
    const char *existing;
    char *combined;

    length = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (length < 0) {
        return -1;
    }
    exe_path[length] = '\0';

    slash = strrchr(exe_path, '/');
    if (slash == NULL) {
        errno = EINVAL;
        return -1;
    }
    *slash = '\0';

    if (snprintf(lib_path, sizeof(lib_path), "%s/libswizzle.so", exe_path) >= (int) sizeof(lib_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    existing = getenv("LD_PRELOAD");
    if (existing == NULL || existing[0] == '\0') {
        return setenv("LD_PRELOAD", lib_path, 1);
    }

    combined = malloc(strlen(lib_path) + strlen(existing) + 2);
    if (combined == NULL) {
        return -1;
    }

    sprintf(combined, "%s:%s", lib_path, existing);
    if (setenv("LD_PRELOAD", combined, 1) != 0) {
        free(combined);
        return -1;
    }

    free(combined);
    return 0;
}

int main(int argc, char **argv) {
    int separator = -1;
    int mapping_count;
    int i;
    char count_buffer[32];

    if (argc < 4) {
        usage(argv[0]);
        return 2;
    }

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--") == 0) {
            separator = i;
            break;
        }
    }

    if (separator < 2 || separator == argc - 1) {
        usage(argv[0]);
        return 2;
    }

    mapping_count = separator - 1;
    for (i = 0; i < mapping_count; ++i) {
        if (set_mapping_env(i, argv[i + 1]) != 0) {
            fprintf(stderr, "invalid mapping: %s\n", argv[i + 1]);
            return 2;
        }
    }

    snprintf(count_buffer, sizeof(count_buffer), "%d", mapping_count);
    if (setenv("SWIZZLE_MAP_COUNT", count_buffer, 1) != 0 || set_preload() != 0) {
        perror("swizzle");
        return 1;
    }

    execvp(argv[separator + 1], &argv[separator + 1]);
    perror(argv[separator + 1]);
    return 127;
}
