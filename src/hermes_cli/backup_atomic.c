/*
 * backup_atomic.c — C home for upstream Hermes hermes_cli/backup_atomic.py.
 *
 * PoP: w10_backupatom_f4248f_normalize_private_modes @ hermes_cli/backup_atomic.py:_normalize_private_modes
 * PoP: w10_backupatom_f4248f_fsync_staged_dir @ hermes_cli/backup_atomic.py:_fsync_staged_dir
 *
 * Faithful C11 replay of the two decision cores. Filesystem calls are injected
 * seams: the caller supplies the OS name, the staged tree inventory and the
 * errno set (the live module's _DIR_FSYNC_UNSUPPORTED), and the C replay emits
 * the same chmod call sequence / fsync disposition as the live functions.
 */
#include "noc_w10_backupatom_f4248f.h"

#include "hermes_json.h"

#include <stdio.h>
#include <string.h>

#define W10_MODE_OWNER_DIR 448  /* 0o700 */
#define W10_MODE_OWNER_FILE 384 /* 0o600 = stat.S_IREAD | stat.S_IWRITE */

static const char *w10_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

static void w10_add_chmod(json_t *out, const char *path, int mode) {
    json_t *rec = json_object();
    json_set(rec, "path", json_string(path));
    json_set(rec, "mode", json_number(mode));
    json_append(out, rec);
}

struct json_t *w10_backupatom_f4248f_normalize_modes(const struct json_t *os_name,
                                                      const struct json_t *root,
                                                      const struct json_t *staging,
                                                      const struct json_t *entries) {
    json_t *out = json_array();
    int is_nt = strcmp(w10_str(os_name), "nt") == 0;
    size_t n = (entries && entries->type == JSON_ARRAY) ? json_len(entries) : 0;

    if (is_nt) {
        /* Windows: only clear read-only on regular, non-symlink files. */
        for (size_t i = 0; i < n; ++i) {
            json_t *e = json_get(entries, i);
            const char *kind = w10_str(e ? json_obj_get(e, "kind") : NULL);
            if (strcmp(kind, "file") == 0)
                w10_add_chmod(out, w10_str(e ? json_obj_get(e, "path") : NULL), W10_MODE_OWNER_FILE);
        }
        return out;
    }

    /* POSIX: root and staging dir owner-only, then every staged entry. */
    w10_add_chmod(out, w10_str(root), W10_MODE_OWNER_DIR);
    w10_add_chmod(out, w10_str(staging), W10_MODE_OWNER_DIR);
    for (size_t i = 0; i < n; ++i) {
        json_t *e = json_get(entries, i);
        const char *kind = w10_str(e ? json_obj_get(e, "kind") : NULL);
        if (strcmp(kind, "dir") == 0)
            w10_add_chmod(out, w10_str(e ? json_obj_get(e, "path") : NULL), W10_MODE_OWNER_DIR);
        else if (strcmp(kind, "file") == 0)
            w10_add_chmod(out, w10_str(e ? json_obj_get(e, "path") : NULL), W10_MODE_OWNER_FILE);
        /* symlink/other: untouched */
    }
    return out;
}

struct json_t *w10_backupatom_f4248f_fsync_dir(const struct json_t *os_name,
                                                const struct json_t *err,
                                                const struct json_t *unsupported) {
    if (strcmp(w10_str(os_name), "nt") == 0) return json_number(0); /* Windows: no-op */
    if (!err || err->type == JSON_NULL) return json_number(1);       /* clean fsync */
    long long e = (long long)err->num_val;
    size_t n = (unsupported && unsupported->type == JSON_ARRAY) ? json_len(unsupported) : 0;
    for (size_t i = 0; i < n; ++i) {
        json_t *v = json_get(unsupported, i);
        if (v && v->type == JSON_NUMBER && (long long)v->num_val == e)
            return json_number(2); /* EINVAL/ENOTSUP/EOPNOTSUPP: suppressed */
    }
    return json_number(3); /* real I/O error: re-raised */
}
