/*
 * update_windows_acl.c — C home for upstream Hermes hermes_cli/update_windows_acl.py.
 *
 * PoP: w17_updatewind_e677b5_repair @ hermes_cli/update_windows_acl.py:repair_windows_home_acl
 *
 * Faithful port of the whole decision core of repair_windows_home_acl():
 *   - gated on elevated (explicit, else probed); not elevated => no-op None
 *   - granted empty + failed non-empty => "path: reason" RuntimeError
 *   - granted empty, no failed => "nothing was repaired (...)" RuntimeError
 *   - any granted => success (the logger.warning on partial failure is irrelevant
 *     to the function's return contract and is not reproduced).
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w17_updatewind_e677b5.h"

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

/* Copy a JSON string out of a node BEFORE the input tree is ever touched. */
static char *json_str_copy(const json_t *n) {
    if (!n || n->type != JSON_STRING) return NULL;
    return dupstr(n->str_val ? n->str_val : "");
}

static char *fmt(const char *fmt_str, const char *a, const char *b, const char *c) {
    /* format "{a}{b}{c}" without printf %s to keep allocation simple. */
    size_t na = a ? strlen(a) : 0;
    size_t nb = b ? strlen(b) : 0;
    size_t nc = c ? strlen(c) : 0;
    size_t need = na + nb + nc + 1;
    char *out = (char *)malloc(need);
    if (!out) return NULL;
    size_t at = 0;
    if (a && na) { memcpy(out + at, a, na); at += na; }
    if (b && nb) { memcpy(out + at, b, nb); at += nb; }
    if (c && nc) { memcpy(out + at, c, nc); at += nc; }
    out[at] = '\0';
    (void)fmt_str;
    return out;
}

char *w17_updatewind_e677b5_repair(int elevated, bool probe_elevated, const char *home,
                                   json_t *granted, json_t *skipped, json_t *failed) {
    bool eff = (elevated < 0) ? probe_elevated : (elevated != 0);
    if (!eff) return NULL;

    const char *h = home ? home : "";
    size_t ng = json_is_array(granted) ? json_len(granted) : 0;
    size_t ns = json_is_array(skipped) ? json_len(skipped) : 0;
    size_t nf = json_is_array(failed) ? json_len(failed) : 0;

    if (nf > 0 && ng == 0) {
        /* path, reason = failed[0] */
        json_t *pair = json_get(failed, 0);
        char *path = json_str_copy(json_is_array(pair) ? json_get(pair, 0) : NULL);
        char *reason = json_str_copy(json_is_array(pair) ? json_get(pair, 1) : NULL);
        char *mid = fmt("", "could not restore user access to ", h, ": ");
        char *tail = fmt("", path ? path : "", ": ", reason ? reason : "");
        char *msg = fmt("", mid, tail, "");
        free(mid); free(tail); free(path); free(reason);
        return msg;
    }
    if (ng == 0) {
        char *why;
        if (ns > 0) {
            char *first = json_str_copy(json_get(skipped, 0));
            why = fmt("", first ? first : "", " is a junction, symlink or hard link", "");
            free(first);
        } else {
            why = dupstr("nothing was walkable");
        }
        char *head = fmt("", "could not restore user access to ", h, ": nothing was repaired (");
        char *msg = fmt("", head, why, ")");
        free(head); free(why);
        return msg;
    }
    return NULL;
}
