/* session_recovery_paths.c — path validation, fingerprinting, disk-space
 * preflight and source-bundle snapshot for offline session recovery.
 * Faithful port of hermes_cli/session_recovery.py (paths slice).
 */
#define _GNU_SOURCE
#include "session_recovery_internal.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <libgen.h>
#include <wordexp.h>

const char *SR_CANONICAL_TABLES[6] = {
    "sessions", "messages", "session_model_usage",
    "compression_locks", "gateway_routing", "async_delegations",
};
const char *SR_TOPIC_TABLES[2] = {
    "telegram_dm_topic_mode", "telegram_dm_topic_bindings",
};
/* sorted(_GENERATED_META_KEYS) */
const char *SR_GENERATED_META_KEYS[8] = {
    "fts_cjk_rebuild_high_water", "fts_cjk_rebuild_progress",
    "fts_cjk_stale", "fts_optimize_available",
    "fts_rebuild_high_water", "fts_rebuild_progress",
    "fts_storage_version", "telegram_dm_topic_schema_version",
};
const char *SR_SIDECAR_SUFFIXES[4] = { "", "-wal", "-shm", "-journal" };

void sr_set_err(char *err, size_t elen, const char *fmt, ...) {
    if (!err || !elen) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, elen, fmt, ap);
    va_end(ap);
}

bool sr_is_generated_meta_key(const char *key) {
    if (!key) return false;
    for (size_t i = 0; i < 8; i++)
        if (strcmp(SR_GENERATED_META_KEYS[i], key) == 0) return true;
    return false;
}

long long sr_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long long)st.st_size;
}

bool sr_path_lexists(const char *path) {
    struct stat st;
    return lstat(path, &st) == 0;
}

/* expanduser: only handles leading ~/ like Python's for our use-cases. */
static char *sr_expanduser(const char *path) {
    if (path && path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home && *home) {
            size_t n = strlen(home) + strlen(path);
            char *out = malloc(n + 1);
            if (out) snprintf(out, n + 1, "%s%s", home, path + 1);
            return out;
        }
    }
    return strdup(path ? path : "");
}

/* PoP: sr_sidecar_path @ hermes_cli/session_recovery.py:_sidecar_path */
char *sr_sidecar_path(const char *db_path, const char *suffix) {
    if (!suffix || !*suffix) return strdup(db_path);
    size_t n = strlen(db_path) + strlen(suffix);
    char *out = malloc(n + 1);
    if (out) snprintf(out, n + 1, "%s%s", db_path, suffix);
    return out;
}

/* PoP: sr_resolved_output_path @ hermes_cli/session_recovery.py:_resolved_output_path */
char *sr_resolved_output_path(const char *path, char *err, size_t elen) {
    char *expanded = sr_expanduser(path);
    if (!expanded) return NULL;
    /* parent.resolve(strict=True) */
    char *dup = strdup(expanded);
    char *parent = dirname(dup);
    char real_parent[4096];
    if (!realpath(parent, real_parent)) {
        sr_set_err(err, elen, "parent directory does not exist: %s", parent);
        free(dup);
        free(expanded);
        return NULL;
    }
    char *dup2 = strdup(expanded);
    char *name = basename(dup2);
    size_t n = strlen(real_parent) + 1 + strlen(name);
    char *out = malloc(n + 1);
    if (out) snprintf(out, n + 1, "%s/%s", real_parent, name);
    free(dup);
    free(dup2);
    free(expanded);
    return out;
}

/* resolve(strict=False): realpath if possible, else parent-resolved join. */
static char *sr_resolve_lenient(const char *path) {
    char buf[4096];
    if (realpath(path, buf)) return strdup(buf);
    return sr_resolved_output_path(path, NULL, 0);
}

/* PoP: sr_validate_paths @ hermes_cli/session_recovery.py:_validate_paths */
int sr_validate_paths(const char *source_path, const char *output_path,
                      const char *work_dir, char **source_out,
                      char **output_out, char **work_root_out,
                      char *err, size_t elen) {
    *source_out = NULL;
    *output_out = NULL;
    *work_root_out = NULL;

    char *src_exp = sr_expanduser(source_path);
    char src_real[4096];
    if (!src_exp || !realpath(src_exp, src_real)) {
        sr_set_err(err, elen, "Source does not exist: %s",
                   src_exp ? src_exp : source_path);
        free(src_exp);
        return SESSION_RECOVERY_SAFETY_ERROR;
    }
    free(src_exp);
    struct stat st;
    if (stat(src_real, &st) != 0 || !S_ISREG(st.st_mode)) {
        sr_set_err(err, elen, "Source is not a file: %s", src_real);
        return SESSION_RECOVERY_SAFETY_ERROR;
    }
    char *source = strdup(src_real);

    char *output = NULL;
    if (output_path) {
        output = sr_resolved_output_path(output_path, err, elen);
        if (!output) { free(source); return SESSION_RECOVERY_SAFETY_ERROR; }
        /* protected = resolved sidecars of source */
        char *out_resolved = sr_resolve_lenient(output);
        for (size_t i = 0; i < 4; i++) {
            char *sc = sr_sidecar_path(source, SR_SIDECAR_SUFFIXES[i]);
            char *sc_resolved = sr_resolve_lenient(sc);
            bool clash = out_resolved && sc_resolved &&
                         strcmp(out_resolved, sc_resolved) == 0;
            free(sc);
            free(sc_resolved);
            if (clash) {
                sr_set_err(err, elen,
                    "The recovery output must not be the source database or "
                    "one of its journal sidecars.");
                free(out_resolved);
                free(output);
                free(source);
                return SESSION_RECOVERY_SAFETY_ERROR;
            }
        }
        free(out_resolved);
        for (size_t i = 0; i < 4; i++) {
            char *cand = sr_sidecar_path(output, SR_SIDECAR_SUFFIXES[i]);
            bool exists = sr_path_lexists(cand);
            if (exists) {
                sr_set_err(err, elen,
                    "Refusing to overwrite existing recovery output: %s", cand);
                free(cand);
                free(output);
                free(source);
                return SESSION_RECOVERY_SAFETY_ERROR;
            }
            free(cand);
        }
    }

    /* work_root */
    char *work_root = NULL;
    if (work_dir) {
        char *wd_exp = sr_expanduser(work_dir);
        char wd_real[4096];
        if (!wd_exp || !realpath(wd_exp, wd_real)) {
            sr_set_err(err, elen, "Recovery work directory does not exist: %s",
                       wd_exp ? wd_exp : work_dir);
            free(wd_exp);
            free(output);
            free(source);
            return SESSION_RECOVERY_SAFETY_ERROR;
        }
        free(wd_exp);
        work_root = strdup(wd_real);
    } else {
        const char *base = output ? output : source;
        char *dup = strdup(base);
        work_root = strdup(dirname(dup));
        free(dup);
    }
    if (stat(work_root, &st) != 0 || !S_ISDIR(st.st_mode)) {
        sr_set_err(err, elen,
                   "Recovery work directory is not a directory: %s", work_root);
        free(work_root);
        free(output);
        free(source);
        return SESSION_RECOVERY_SAFETY_ERROR;
    }

    *source_out = source;
    *output_out = output;
    *work_root_out = work_root;
    return 0;
}

/* PoP: sr_source_fingerprint @ hermes_cli/session_recovery.py:_source_fingerprint */
json_t *sr_source_fingerprint(const char *source) {
    json_t *fp = json_object();
    for (size_t i = 0; i < 4; i++) {
        char *path = sr_sidecar_path(source, SR_SIDECAR_SUFFIXES[i]);
        struct stat st;
        if (stat(path, &st) == 0) {
            json_t *e = json_object();
            json_set(e, "size", json_number((double)st.st_size));
            json_set(e, "mtime_ns",
                     json_number((double)st.st_mtim.tv_sec * 1e9 +
                                 (double)st.st_mtim.tv_nsec));
            json_set(fp, SR_SIDECAR_SUFFIXES[i][0] ? SR_SIDECAR_SUFFIXES[i]
                                                   : "main", e);
        }
        free(path);
    }
    return fp;
}

/* PoP: session_recovery_format_bytes @ hermes_cli/session_recovery.py:_format_bytes */
char *session_recovery_format_bytes(long long value) {
    static const char *units[] = { "B", "KiB", "MiB", "GiB", "TiB" };
    double amount = (double)value;
    char buf[64];
    for (size_t i = 0; i < 5; i++) {
        if (amount < 1024.0 || i == 4) {
            snprintf(buf, sizeof(buf), "%.1f %s", amount, units[i]);
            return strdup(buf);
        }
        amount /= 1024.0;
    }
    snprintf(buf, sizeof(buf), "%lld B", value);
    return strdup(buf);
}

/* PoP: sr_same_filesystem @ hermes_cli/session_recovery.py:_same_filesystem */
bool sr_same_filesystem(const char *left, const char *right) {
    struct stat a, b;
    if (stat(left, &a) == 0 && stat(right, &b) == 0)
        return a.st_dev == b.st_dev;
    /* defensive fallback: compare anchors case-insensitively (Python parity;
     * on POSIX every absolute path anchors at "/"). */
    return (left[0] == '/' ) == (right[0] == '/');
}
