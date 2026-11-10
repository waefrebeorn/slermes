/*
 * hermes_fs.h — shared filesystem helpers.
 *
 * mkdir_p (recursive mkdir, EEXIST-tolerant) was copy-pasted byte-identical
 * into cli/port_cli_profiles.c and cron/port_cron_jobs.c. Single source of
 * truth. Includes the POSIX bits it needs so it is self-contained.
 */
#ifndef HERMES_FS_H
#define HERMES_FS_H

#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>


/* Recursively mkdir each '/' component of path, ignoring errors (best-effort,
 * mirror of the plugin ensure_dir; hardcoded 0700). */
static inline void fs_ensure_dir(const char *path) {
    if (!path || !*path) return;
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0700); *p = '/'; }
    }
}

#ifdef __cplusplus
extern "C" {
#endif

/* Recursively create path (mkdir each component; EEXIST is fine). Returns 0
 * on success, -1 on error/empty/too-long. Behaviour matches the two folded
 * port copies exactly; a trailing '/' is tolerated. */
static inline int fs_mkdir_p(const char *path, mode_t mode) {
    if (!path || !*path) return -1;
    char tmp[PATH_MAX];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return -1;
    memcpy(tmp, path, len + 1);
    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* HERMES_FS_H */
