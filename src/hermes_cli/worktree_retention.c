/*
 * worktree_retention.c — C port of upstream Hermes hermes_cli/worktree_retention.py.
 *
 * PoP: w17_worktreere_5152ed_named @ hermes_cli/worktree_retention.py:_named
 * PoP: w17_worktreere_5152ed_constants @ hermes_cli/worktree_retention.py:KEEP_UNARCHIVABLE/KEEP_WORKTREE_REFS/KEEP_LARGE_UNTRACKED/KEEP_SUBMODULE_WORK/_NAMED/_MAX_ENTRIES
 */
#include "noc_w17_worktreere_5152ed.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const KEEP_UNARCHIVABLE = "keep-unarchivable";
static const char *const KEEP_WORKTREE_REFS = "keep-worktree-refs";
static const char *const KEEP_LARGE_UNTRACKED = "keep-large-untracked";
static const char *const KEEP_SUBMODULE_WORK = "keep-submodule-work";
static const long long NAMED = 5;
static const long long MAX_ENTRIES = 200000;

json_t *w17_worktreere_5152ed_named(const json_t *paths, bool more) {
    size_t n = (paths && paths->type == JSON_ARRAY) ? json_len(paths) : 0;
    size_t take = n < (size_t)NAMED ? n : (size_t)NAMED;
    size_t cap = 32;
    for (size_t i = 0; i < take; i++) {
        json_t *v = json_get(paths, i);
        cap += (v && v->str_val) ? strlen(v->str_val) + 2 : 4;
    }
    char *out = (char *)calloc(1, cap);
    size_t used = 0;
    if (out) {
        for (size_t i = 0; i < take; i++) {
            json_t *v = json_get(paths, i);
            const char *s = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
            if (i) { out[used++] = ','; out[used++] = ' '; }
            size_t len = strlen(s);
            memcpy(out + used, s, len);
            used += len;
        }
        long long extra = (long long)n - NAMED;
        if (extra > 0) {
            int k = snprintf(out + used, cap - used, " (+%lld more)", extra);
            if (k > 0) used += (size_t)k;
        } else if (more) {
            int k = snprintf(out + used, cap - used, " (+more)");
            if (k > 0) used += (size_t)k;
        }
        out[used] = '\0';
    }
    json_t *r = json_string(out ? out : "");
    free(out);
    return r;
}

json_t *w17_worktreere_5152ed_constants(void) {
    json_t *out = json_object();
    json_set(out, "KEEP_UNARCHIVABLE", json_string(KEEP_UNARCHIVABLE));
    json_set(out, "KEEP_WORKTREE_REFS", json_string(KEEP_WORKTREE_REFS));
    json_set(out, "KEEP_LARGE_UNTRACKED", json_string(KEEP_LARGE_UNTRACKED));
    json_set(out, "KEEP_SUBMODULE_WORK", json_string(KEEP_SUBMODULE_WORK));
    json_set(out, "_NAMED", json_number((double)NAMED));
    json_set(out, "_MAX_ENTRIES", json_number((double)MAX_ENTRIES));
    return out;
}
