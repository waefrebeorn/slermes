/*
 * hermes_constants_scratch.c — faithful C11 port of the pure predicates and
 * idle-scan decision in upstream Hermes hermes_constants_scratch.py
 * (gate w5_hermesconst_d195ca).
 *
 * PoP: w5_hermesconst_d195ca_under @ hermes_constants_scratch.py:_under
 * PoP: w5_hermesconst_d195ca_same_birth @ hermes_constants_scratch.py:_same_birth
 * PoP: w5_hermesconst_d195ca_scan_subtree @ hermes_constants_scratch.py:_scan_subtree
 * PoP: w5_hermesconst_d195ca_subtree_touch_state @ hermes_constants_scratch.py:subtree_touch_state
 *
 * Filesystem listing/stat and process lookup are injected from the fixture so
 * the stop-at-first-recent walk, hardlink exclusion and symlink rules can be
 * compared deterministically without touching disk or processes.
 */
#include "noc_w5_hermesconst_d195ca.h"
#include "hermes_json.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct json_t *w5_hermesconst_d195ca_under(const char *path, const char *root, const char *sep) {
    char rbuf[8192], prefix[8192];
    size_t n, plen;
    char sc;
    if (!path) path = "";
    if (!root) root = "";
    sc = (sep && *sep) ? sep[0] : '/';
    if (!strcmp(path, root)) return json_bool(1);
    n = strlen(root);
    if (n >= sizeof(rbuf)) n = sizeof(rbuf) - 1;
    memcpy(rbuf, root, n);
    rbuf[n] = '\0';
    while (n > 0 && rbuf[n-1] == sc) rbuf[--n] = '\0';
    plen = (size_t)snprintf(prefix, sizeof(prefix), "%s%c", rbuf, sc);
    if (plen >= sizeof(prefix)) return json_bool(0);
    return json_bool(strncmp(path, prefix, plen) == 0 ? 1 : 0);
}

struct json_t *w5_hermesconst_d195ca_same_birth(double now, double started, int has_proc) {
    if (has_proc) return json_bool(now == started ? 1 : 0);
    return json_bool(fabs(now - started) < 2.0 ? 1 : 0);
}

/* Mirrors _scan_subtree: (state, denied_dir). */
static void w5c_scan(const struct json_t *in, const char **out_state, const char **out_denied) {
    const struct json_t *entries = json_obj_get((struct json_t *)in, "entries");
    const char *path = json_get_str(in, "path", "");
    double cutoff = json_get_num(in, "cutoff", 0.0);
    int root_ok = json_get_bool(in, "root_stat_ok", true) ? 1 : 0;
    double root_mtime = json_get_num(in, "root_mtime", 0.0);
    int root_dir = json_get_bool(in, "root_is_dir", true) ? 1 : 0;
    int root_symlink = json_get_bool(in, "root_is_symlink", false) ? 1 : 0;
    const char *stack[512];
    size_t sp = 0;

    *out_denied = NULL;
    if (!root_ok) { *out_state = "scan-failed"; return; }
    if (root_mtime >= cutoff) { *out_state = "touched"; return; }
    if (!root_dir || root_symlink) { *out_state = "idle"; return; }

    stack[sp++] = path;
    while (sp > 0) {
        const char *current = stack[--sp];
        for (size_t i = 0; entries && entries->type == JSON_ARRAY && i < json_len(entries); i++) {
            const struct json_t *e = json_get(entries, i);
            const char *edir;
            if (!e || e->type != JSON_OBJECT) continue;
            edir = json_get_str(e, "dir", "");
            if (strcmp(edir, current)) continue;
            if (json_get_bool(e, "scandir_fail", false)) {
                *out_state = "scan-failed";
                *out_denied = json_get_bool(e, "perm_err", false) ? current : NULL;
                return;
            }
            if (!json_get_bool(e, "stat_ok", true)) {
                *out_state = "scan-failed";
                *out_denied = json_get_bool(e, "perm_err", false) ? current : NULL;
                return;
            }
            {
                double mtime = json_get_num(e, "mtime", 0.0);
                int nlink = (int)json_get_num(e, "nlink", 1.0);
                int is_file = json_get_bool(e, "is_file", false) ? 1 : 0;
                int is_dir = json_get_bool(e, "is_dir", false) ? 1 : 0;
                if (mtime >= cutoff && !(nlink > 1 && is_file)) {
                    *out_state = "touched";
                    *out_denied = NULL;
                    return;
                }
                if (is_dir && sp < 511) stack[sp++] = json_get_str(e, "path", "");
            }
        }
    }
    *out_state = "idle";
}

struct json_t *w5_hermesconst_d195ca_scan_subtree(const struct json_t *in) {
    const char *state = "idle", *denied = NULL;
    struct json_t *out;
    w5c_scan(in, &state, &denied);
    out = json_object();
    json_set(out, "state", json_string(state));
    json_set(out, "denied_dir", denied ? json_string(denied) : json_null());
    return out;
}

struct json_t *w5_hermesconst_d195ca_subtree_touch_state(const struct json_t *in) {
    const char *state = "idle", *denied = NULL;
    (void)denied;
    w5c_scan(in, &state, &denied);
    return json_string(state);
}
