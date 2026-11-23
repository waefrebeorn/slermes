/*
 * runtime_ownership.c — C port of upstream Hermes gateway/runtime_ownership.py.
 *
 * PoP: w12_runtimeown_e8fe0a_foreign_lock_recovery @ gateway/runtime_ownership.py:foreign_lock_recovery
 * PoP: w12_runtimeown_e8fe0a_same_inode @ gateway/runtime_ownership.py:_same_inode
 * PoP: w12_runtimeown_e8fe0a_canonical_home @ gateway/runtime_ownership.py:canonical_home
 *
 * Pure advisory-lock helpers. The process/OS lock traffic itself stays in
 * Python; these are the deterministic strings and comparisons the gate drives.
 */
#include "noc_w12_runtimeown_e8fe0a.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

json_t *w12_runtimeown_e8fe0a_foreign_lock_recovery(const struct json_t *path) {
    const char *p = (path && path->type == JSON_STRING && path->str_val) ? path->str_val : "";
    size_t n = strlen(p);
    /* literal overhead: prefix + two interleaved sentences + two path slots */
    size_t cap = n * 3 + 256;
    char *out = (char *)malloc(cap);
    if (!out) return json_string("");
    snprintf(out, cap,
             "foreign_stale_lock: %s is not readable by this user (left by a gateway run as root?) "
             "and cannot be proven unheld. If `sudo lsof %s` lists no process, run `sudo rm %s`.",
             p, p, p);
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* Python tuple equality is type-sensitive: "42" != 42. */
static int field_eq(const json_t *a, const json_t *b) {
    if (!a || !b || a->type != b->type) return 0;
    switch (a->type) {
        case JSON_NULL:   return 1;
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return a->str_val && b->str_val && !strcmp(a->str_val, b->str_val);
        default:          return 0;
    }
}

json_t *w12_runtimeown_e8fe0a_same_inode(const struct json_t *left, const struct json_t *right) {
    int same = left && right &&
               field_eq(json_obj_get(left, "dev"), json_obj_get(right, "dev")) &&
               field_eq(json_obj_get(left, "ino"), json_obj_get(right, "ino"));
    return json_bool(same ? 1 : 0);
}

/* Windows ntpath.normcase(Path(home).expanduser().resolve()):
 *   '/' -> '\\', collapse '.'/'..', lowercase. Absolute inputs only. */
json_t *w12_runtimeown_e8fe0a_canonical_home(const struct json_t *home) {
    const char *src = (home && home->type == JSON_STRING && home->str_val) ? home->str_val : "";
    size_t n = strlen(src);
    char *work = (char *)malloc(n + 1);
    if (!work) return json_string("");
    for (size_t i = 0; i < n; i++) work[i] = (src[i] == '/') ? '\\' : src[i];
    work[n] = '\0';

    char parts[64][512];
    size_t count = 0;
    char drive[8] = "";
    char *tok = strtok(work, "\\");
    while (tok && count < 64) {
        if (tok[1] == ':' && tok[2] == '\0') {
            snprintf(drive, sizeof(drive), "%s", tok);
        } else if (!strcmp(tok, ".") || !tok[0]) {
            /* skip */
        } else if (!strcmp(tok, "..")) {
            if (count > 0) count--;
        } else {
            snprintf(parts[count], sizeof(parts[count]), "%s", tok);
            count++;
        }
        tok = strtok(NULL, "\\");
    }

    size_t cap = 8 + count * 513 + 2;
    char *out = (char *)malloc(cap);
    if (!out) { free(work); return json_string(""); }
    size_t k = 0;
    if (drive[0]) {
        out[k++] = (char)tolower((unsigned char)drive[0]);
        out[k++] = ':';
        out[k++] = '\\';
    }
    for (size_t i = 0; i < count; i++) {
        if (i) out[k++] = '\\';
        for (size_t j = 0; parts[i][j]; j++) out[k++] = (char)tolower((unsigned char)parts[i][j]);
    }
    if (k == 0) out[k++] = '.';
    out[k] = '\0';
    free(work);
    json_t *r = json_string(out);
    free(out);
    return r;
}
