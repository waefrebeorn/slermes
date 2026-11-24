/*
 * update_fleet_scope.c — C home for upstream Hermes hermes_cli/update_fleet_scope.py.
 *
 * PoP: w17_updateflee_94f003_describe_skipped_runtime @ hermes_cli/update_fleet_scope.py:describe_skipped_runtime
 * PoP: w17_updateflee_94f003_home_in_update_scope @ hermes_cli/update_fleet_scope.py:home_in_update_scope
 *
 * The first is the single notice formatter every launchd/systemd/gateway
 * restart path shares; the second is the pure membership test over an injected
 * scope (the live module's other helpers all read process/filesystem state).
 */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w17_updateflee_94f003.h"

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

/* concat: join up to four owned-or-borrowed strings. */
static char *cat4(const char *a, const char *b, const char *c, const char *d) {
    const char *v[4] = { a, b, c, d };
    size_t n = 0;
    for (int i = 0; i < 4; i++) if (v[i]) n += strlen(v[i]);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    size_t at = 0;
    for (int i = 0; i < 4; i++) {
        if (!v[i]) continue;
        size_t k = strlen(v[i]);
        memcpy(out + at, v[i], k);
        at += k;
    }
    out[at] = '\0';   /* write NUL at the final length */
    return out;
}

char *w17_updateflee_94f003_describe_skipped_runtime(const char *kind, const char *name,
                                                     const char *home, const char *checkout) {
    const char *k = kind ? kind : "";
    const char *n = name ? name : "";
    /* Unicode is passed through verbatim: "  ↷ " (U+21B7) and " — " (U+2014). */
    char *prefix = cat4("  \xe2\x86\xb7 ", cat4(n, ": ", k, ""), NULL, NULL);
    char *msg;
    if (checkout != NULL) {
        msg = cat4(prefix, " of another Hermes checkout (", checkout, ") — left alone (not restarted)");
    } else if (home == NULL) {
        msg = cat4(prefix, " whose Hermes home could not be read — left alone (not restarted)", NULL, NULL);
    } else {
        msg = cat4(prefix, " of another Hermes home (", home, ") — left alone (not restarted)");
    }
    free(prefix);
    return msg;
}

/* norm_path: model Path(str(path)).expanduser().resolve() on Windows for the
 * clean absolute paths the fixture drives: slash->backslash, ASCII lowercase,
 * collapse '.' / '..', strip trailing separators. Never frees an interior ptr. */
static char *norm_path(const char *in) {
    if (!in || !*in) return NULL;
    size_t n = strlen(in);
    char *work = (char *)malloc(n + 1);
    if (!work) return NULL;
    for (size_t i = 0; i < n; i++) {
        char ch = in[i];
        if (ch == '/') ch = '\\';
        if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
        work[i] = ch;
    }
    work[n] = '\0';

    char **parts = (char **)calloc(n + 1, sizeof(char *));
    size_t count = 0;
    char *tok = work;
    while (*tok) {
        char *sep = strchr(tok, '\\');
        if (sep) *sep = '\0';
        if (*tok && strcmp(tok, ".") != 0) {
            if (strcmp(tok, "..") == 0) {
                if (count > 0 && strcmp(parts[count - 1], "..") != 0) count--;
                else parts[count++] = tok;
            } else {
                parts[count++] = tok;
            }
        }
        if (!sep) break;
        tok = sep + 1;
    }
    size_t total = 0;
    for (size_t i = 0; i < count; i++) total += strlen(parts[i]) + 1;
    char *out = (char *)malloc(total + 1);
    if (!out) { free(work); free(parts); return NULL; }
    size_t at = 0;
    for (size_t i = 0; i < count; i++) {
        size_t k = strlen(parts[i]);
        memcpy(out + at, parts[i], k);
        at += k;
        if (i + 1 < count) out[at++] = '\\';
    }
    out[at] = '\0';   /* write NUL at the new length after shortening */
    free(parts);
    free(work);
    return out;
}

bool w17_updateflee_94f003_home_in_update_scope(const char *home, json_t *scope) {
    if (!home || home[0] == '\0') return false;   /* `if not home` */
    char *resolved = norm_path(home);
    if (!resolved) return false;
    bool found = false;
    if (json_is_array(scope)) {
        for (size_t i = 0; i < json_len(scope); i++) {
            json_t *item = json_get(scope, i);
            const char *raw = (item && item->type == JSON_STRING) ? item->str_val : NULL;
            char *cand = norm_path(raw);
            if (cand && !strcmp(resolved, cand)) { free(cand); found = true; break; }
            free(cand);
        }
    }
    free(resolved);
    return found;
}
