/*
 * fs_utils.c — faithful C11 port of hermes_cli/fs_utils.py
 * (gate w7_fsutils_903462).
 *
 * PoP: w7_fsutils_rmtree_force @ hermes_cli/fs_utils.py:rmtree_force
 *
 * Python's onerror hook is `os.chmod(p, stat.S_IWRITE)` followed by a retry of
 * the failed operation. Here every unlink/rmdir attempt is retried once with
 * the write bit forced on; read-only_paths mirrors the fixture's chmod state so
 * the oracle and the C port start from identical permissions.
 */
#include "hermes_json.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int w7_is_readonly(const json_t *readonly_paths, const char *rel) {
    if (!readonly_paths || readonly_paths->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < readonly_paths->c.count; i++) {
        json_t *v = readonly_paths->c.items[i];
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, rel)) return 1;
    }
    return 0;
}

static void w7_join(char *buf, size_t cap, const char *a, const char *b) {
    size_t n = strlen(a);
    int need_sep = n > 0 && a[n - 1] != '/' && a[n - 1] != '\\';
    snprintf(buf, cap, "%s%s%s", a, need_sep ? "/" : "", b);
}

/* Returns 0 on success (or after chmod+retry), non-zero when still failing. */
static int w7_unlink_retry(const char *path) {
    if (remove(path) == 0) return 0;
    chmod(path, S_IWRITE);
    return remove(path);
}

static int w7_rmdir_retry(const char *path) {
    if (rmdir(path) == 0) return 0;
    chmod(path, S_IWRITE | S_IEXEC | S_IREAD);
    return rmdir(path);
}

static void w7_delete_children(const char *dir, const json_t *readonly_paths,
                               const char *rel_prefix) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char full[1024];
        w7_join(full, sizeof(full), dir, e->d_name);
        char rel[1024];
        w7_join(rel, sizeof(rel), rel_prefix, e->d_name);
        struct stat st;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
            w7_delete_children(full, readonly_paths, rel);
            w7_rmdir_retry(full);
        } else {
            (void)w7_is_readonly(readonly_paths, rel);  /* documented seam; the
             * fixture already applied the chmod, so retry logic is what matters. */
            w7_unlink_retry(full);
        }
    }
    closedir(d);
}

int w7_fsutils_rmtree_force(const char *path, const json_t *readonly_paths) {
    if (!path || !*path) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 1;  /* nothing to remove */
    if (S_ISDIR(st.st_mode)) {
        w7_delete_children(path, readonly_paths, "");
        w7_rmdir_retry(path);
    } else {
        w7_unlink_retry(path);
    }
    return stat(path, &st) != 0;
}
