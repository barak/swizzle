#define _GNU_SOURCE

#include <dlfcn.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
    char *source;
    char *destination;
    size_t source_length;
    size_t destination_length;
} mapping_t;

static mapping_t *mappings;
static size_t mappings_count;

static int (*real_open_fn)(const char *, int, ...);
static int (*real_open64_fn)(const char *, int, ...);
static int (*real_openat_fn)(int, const char *, int, ...);
static int (*real_openat64_fn)(int, const char *, int, ...);
static int (*real_access_fn)(const char *, int);
static int (*real_faccessat_fn)(int, const char *, int, int);
static int (*real_stat_fn)(const char *, struct stat *);
static int (*real_lstat_fn)(const char *, struct stat *);
static int (*real___xstat_fn)(int, const char *, struct stat *);
static int (*real___lxstat_fn)(int, const char *, struct stat *);
static int (*real_stat64_fn)(const char *, struct stat64 *);
static int (*real_lstat64_fn)(const char *, struct stat64 *);
static int (*real___xstat64_fn)(int, const char *, struct stat64 *);
static int (*real___lxstat64_fn)(int, const char *, struct stat64 *);
static DIR *(*real_opendir_fn)(const char *);

static char *normalize_absolute_path(const char *path) {
    size_t length = strlen(path);
    char *normalized = malloc(length + 2);
    size_t out = 0;
    const char *cursor = path;

    if (normalized == NULL) {
        return NULL;
    }

    if (path[0] != '/') {
        free(normalized);
        errno = EINVAL;
        return NULL;
    }

    normalized[out++] = '/';

    while (*cursor != '\0') {
        const char *segment;
        size_t segment_length;

        while (*cursor == '/') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }

        segment = cursor;
        while (*cursor != '\0' && *cursor != '/') {
            ++cursor;
        }
        segment_length = (size_t) (cursor - segment);

        if (segment_length == 1 && segment[0] == '.') {
            continue;
        }

        if (segment_length == 2 && segment[0] == '.' && segment[1] == '.') {
            if (out > 1) {
                --out;
                while (out > 0 && normalized[out - 1] != '/') {
                    --out;
                }
            }
            if (out == 0) {
                normalized[out++] = '/';
            }
            continue;
        }

        if (out > 1 && normalized[out - 1] != '/') {
            normalized[out++] = '/';
        }
        memcpy(normalized + out, segment, segment_length);
        out += segment_length;
    }

    if (out > 1 && normalized[out - 1] == '/') {
        --out;
    }
    normalized[out] = '\0';
    return normalized;
}

static char *join_and_normalize(const char *base, const char *path) {
    size_t base_length = strlen(base);
    size_t path_length = strlen(path);
    bool needs_slash = base_length == 0 || base[base_length - 1] != '/';
    char *combined = malloc(base_length + path_length + (needs_slash ? 2 : 1));
    char *normalized;

    if (combined == NULL) {
        return NULL;
    }

    memcpy(combined, base, base_length);
    if (needs_slash) {
        combined[base_length++] = '/';
    }
    memcpy(combined + base_length, path, path_length + 1);

    normalized = normalize_absolute_path(combined);
    free(combined);
    return normalized;
}

static char *resolve_absolute_path(int dirfd, const char *path) {
    char cwd[PATH_MAX];
    char link_path[64];
    char target[PATH_MAX];
    ssize_t length;

    if (path == NULL) {
        errno = EINVAL;
        return NULL;
    }

    if (path[0] == '/') {
        return normalize_absolute_path(path);
    }

    if (dirfd == AT_FDCWD) {
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            return NULL;
        }
        return join_and_normalize(cwd, path);
    }

    snprintf(link_path, sizeof(link_path), "/proc/self/fd/%d", dirfd);
    length = readlink(link_path, target, sizeof(target) - 1);
    if (length < 0) {
        return NULL;
    }
    target[length] = '\0';
    return join_and_normalize(target, path);
}

static bool matches_prefix(const mapping_t *mapping, const char *path) {
    if (strncmp(path, mapping->source, mapping->source_length) != 0) {
        return false;
    }

    return path[mapping->source_length] == '\0' || path[mapping->source_length] == '/';
}

static char *rewrite_resolved_path(const char *resolved_path) {
    size_t best_index = SIZE_MAX;
    size_t i;

    for (i = 0; i < mappings_count; ++i) {
        if (matches_prefix(&mappings[i], resolved_path) &&
            (best_index == SIZE_MAX || mappings[i].source_length > mappings[best_index].source_length)) {
            best_index = i;
        }
    }

    if (best_index == SIZE_MAX) {
        return NULL;
    }

    if (resolved_path[mappings[best_index].source_length] == '\0') {
        return strdup(mappings[best_index].destination);
    }

    if (strcmp(mappings[best_index].destination, "/") == 0) {
        return strdup(resolved_path + mappings[best_index].source_length);
    }

    {
        const char *suffix = resolved_path + mappings[best_index].source_length;
        size_t length = mappings[best_index].destination_length + strlen(suffix) + 1;
        char *rewritten = malloc(length);

        if (rewritten == NULL) {
            return NULL;
        }

        memcpy(rewritten, mappings[best_index].destination, mappings[best_index].destination_length);
        strcpy(rewritten + mappings[best_index].destination_length, suffix);
        return rewritten;
    }
}

static char *rewrite_path(const char *path) {
    char *resolved = resolve_absolute_path(AT_FDCWD, path);
    char *rewritten;

    if (resolved == NULL) {
        return NULL;
    }

    rewritten = rewrite_resolved_path(resolved);
    free(resolved);
    return rewritten;
}

static char *rewrite_path_at(int dirfd, const char *path) {
    char *resolved = resolve_absolute_path(dirfd, path);
    char *rewritten;

    if (resolved == NULL) {
        return NULL;
    }

    rewritten = rewrite_resolved_path(resolved);
    free(resolved);
    return rewritten;
}

static void load_mappings(void) __attribute__((constructor));

static void load_mappings(void) {
    const char *count_string = getenv("SWIZZLE_MAP_COUNT");
    char cwd[PATH_MAX];
    size_t i;

    if (count_string == NULL || count_string[0] == '\0') {
        return;
    }

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        return;
    }

    mappings_count = (size_t) strtoul(count_string, NULL, 10);
    if (mappings_count == 0) {
        return;
    }

    mappings = calloc(mappings_count, sizeof(*mappings));
    if (mappings == NULL) {
        mappings_count = 0;
        return;
    }

    for (i = 0; i < mappings_count; ++i) {
        char source_name[64];
        char destination_name[64];
        const char *source;
        const char *destination;

        snprintf(source_name, sizeof(source_name), "SWIZZLE_SRC_%zu", i);
        snprintf(destination_name, sizeof(destination_name), "SWIZZLE_DST_%zu", i);

        source = getenv(source_name);
        destination = getenv(destination_name);
        if (source == NULL || destination == NULL) {
            continue;
        }

        mappings[i].source = source[0] == '/' ? normalize_absolute_path(source) : join_and_normalize(cwd, source);
        mappings[i].destination = destination[0] == '/' ? normalize_absolute_path(destination) : join_and_normalize(cwd, destination);
        if (mappings[i].source == NULL || mappings[i].destination == NULL) {
            free(mappings[i].source);
            free(mappings[i].destination);
            mappings[i].source = NULL;
            mappings[i].destination = NULL;
            continue;
        }

        mappings[i].source_length = strlen(mappings[i].source);
        mappings[i].destination_length = strlen(mappings[i].destination);
    }
}

#define RESOLVE_SYMBOL(variable, symbol) \
    do { \
        if ((variable) == NULL) { \
            *(void **) (&(variable)) = dlsym(RTLD_NEXT, symbol); \
        } \
    } while (0)

int open(const char *pathname, int flags, ...) {
    mode_t mode = 0;
    char *rewritten = rewrite_path(pathname);
    const char *actual = rewritten != NULL ? rewritten : pathname;
    int result;

    RESOLVE_SYMBOL(real_open_fn, "open");

    if ((flags & O_CREAT) != 0) {
        va_list arguments;
        va_start(arguments, flags);
        mode = (mode_t) va_arg(arguments, int);
        va_end(arguments);
        result = real_open_fn(actual, flags, mode);
    } else {
        result = real_open_fn(actual, flags);
    }

    free(rewritten);
    return result;
}

int open64(const char *pathname, int flags, ...) {
    mode_t mode = 0;
    char *rewritten = rewrite_path(pathname);
    const char *actual = rewritten != NULL ? rewritten : pathname;
    int result;

    RESOLVE_SYMBOL(real_open64_fn, "open64");

    if ((flags & O_CREAT) != 0) {
        va_list arguments;
        va_start(arguments, flags);
        mode = (mode_t) va_arg(arguments, int);
        va_end(arguments);
        result = real_open64_fn(actual, flags, mode);
    } else {
        result = real_open64_fn(actual, flags);
    }

    free(rewritten);
    return result;
}

int openat(int dirfd, const char *pathname, int flags, ...) {
    mode_t mode = 0;
    char *rewritten = rewrite_path_at(dirfd, pathname);
    const char *actual = rewritten != NULL ? rewritten : pathname;
    int actual_dirfd = rewritten != NULL ? AT_FDCWD : dirfd;
    int result;

    RESOLVE_SYMBOL(real_openat_fn, "openat");

    if ((flags & O_CREAT) != 0) {
        va_list arguments;
        va_start(arguments, flags);
        mode = (mode_t) va_arg(arguments, int);
        va_end(arguments);
        result = real_openat_fn(actual_dirfd, actual, flags, mode);
    } else {
        result = real_openat_fn(actual_dirfd, actual, flags);
    }

    free(rewritten);
    return result;
}

int openat64(int dirfd, const char *pathname, int flags, ...) {
    mode_t mode = 0;
    char *rewritten = rewrite_path_at(dirfd, pathname);
    const char *actual = rewritten != NULL ? rewritten : pathname;
    int actual_dirfd = rewritten != NULL ? AT_FDCWD : dirfd;
    int result;

    RESOLVE_SYMBOL(real_openat64_fn, "openat64");

    if ((flags & O_CREAT) != 0) {
        va_list arguments;
        va_start(arguments, flags);
        mode = (mode_t) va_arg(arguments, int);
        va_end(arguments);
        result = real_openat64_fn(actual_dirfd, actual, flags, mode);
    } else {
        result = real_openat64_fn(actual_dirfd, actual, flags);
    }

    free(rewritten);
    return result;
}

int access(const char *pathname, int mode) {
    char *rewritten = rewrite_path(pathname);
    const char *actual = rewritten != NULL ? rewritten : pathname;
    int result;

    RESOLVE_SYMBOL(real_access_fn, "access");
    result = real_access_fn(actual, mode);
    free(rewritten);
    return result;
}

int faccessat(int dirfd, const char *pathname, int mode, int flags) {
    char *rewritten = rewrite_path_at(dirfd, pathname);
    const char *actual = rewritten != NULL ? rewritten : pathname;
    int actual_dirfd = rewritten != NULL ? AT_FDCWD : dirfd;
    int result;

    RESOLVE_SYMBOL(real_faccessat_fn, "faccessat");
    result = real_faccessat_fn(actual_dirfd, actual, mode, flags);
    free(rewritten);
    return result;
}

int stat(const char *pathname, struct stat *buffer) {
    char *rewritten = rewrite_path(pathname);
    const char *actual = rewritten != NULL ? rewritten : pathname;
    int result;

    RESOLVE_SYMBOL(real_stat_fn, "stat");
    result = real_stat_fn(actual, buffer);
    free(rewritten);
    return result;
}

int lstat(const char *pathname, struct stat *buffer) {
    char *rewritten = rewrite_path(pathname);
    const char *actual = rewritten != NULL ? rewritten : pathname;
    int result;

    RESOLVE_SYMBOL(real_lstat_fn, "lstat");
    result = real_lstat_fn(actual, buffer);
    free(rewritten);
    return result;
}

int __xstat(int version, const char *pathname, struct stat *buffer) {
    char *rewritten = rewrite_path(pathname);
    const char *actual = rewritten != NULL ? rewritten : pathname;
    int result;

    RESOLVE_SYMBOL(real___xstat_fn, "__xstat");
    result = real___xstat_fn(version, actual, buffer);
    free(rewritten);
    return result;
}

int __lxstat(int version, const char *pathname, struct stat *buffer) {
    char *rewritten = rewrite_path(pathname);
    const char *actual = rewritten != NULL ? rewritten : pathname;
    int result;

    RESOLVE_SYMBOL(real___lxstat_fn, "__lxstat");
    result = real___lxstat_fn(version, actual, buffer);
    free(rewritten);
    return result;
}

int stat64(const char *pathname, struct stat64 *buffer) {
    char *rewritten = rewrite_path(pathname);
    const char *actual = rewritten != NULL ? rewritten : pathname;
    int result;

    RESOLVE_SYMBOL(real_stat64_fn, "stat64");
    result = real_stat64_fn(actual, buffer);
    free(rewritten);
    return result;
}

int lstat64(const char *pathname, struct stat64 *buffer) {
    char *rewritten = rewrite_path(pathname);
    const char *actual = rewritten != NULL ? rewritten : pathname;
    int result;

    RESOLVE_SYMBOL(real_lstat64_fn, "lstat64");
    result = real_lstat64_fn(actual, buffer);
    free(rewritten);
    return result;
}

int __xstat64(int version, const char *pathname, struct stat64 *buffer) {
    char *rewritten = rewrite_path(pathname);
    const char *actual = rewritten != NULL ? rewritten : pathname;
    int result;

    RESOLVE_SYMBOL(real___xstat64_fn, "__xstat64");
    result = real___xstat64_fn(version, actual, buffer);
    free(rewritten);
    return result;
}

int __lxstat64(int version, const char *pathname, struct stat64 *buffer) {
    char *rewritten = rewrite_path(pathname);
    const char *actual = rewritten != NULL ? rewritten : pathname;
    int result;

    RESOLVE_SYMBOL(real___lxstat64_fn, "__lxstat64");
    result = real___lxstat64_fn(version, actual, buffer);
    free(rewritten);
    return result;
}

DIR *opendir(const char *name) {
    char *rewritten = rewrite_path(name);
    const char *actual = rewritten != NULL ? rewritten : name;
    DIR *result;

    RESOLVE_SYMBOL(real_opendir_fn, "opendir");
    result = real_opendir_fn(actual);
    free(rewritten);
    return result;
}
