/*
 * environments_adopt.c — C home for upstream Hermes pm/environments_adopt.py.
 *
 * PoP: w8_environments_29900b_replace @ pm/environments_adopt.py:_replace
 * PoP: w8_environments_29900b_restart_needed_ex @ pm/environments_adopt.py:restart_needed
 * PoP: w8_environments_29900b_note_pending @ pm/environments_adopt.py:note_restart_pending
 *
 * Filesystem/sys.path state is injected via explicit seams (`sep`, the
 * resolved pair, the reason value). All exports are w8_environments_29900b_
 * prefixed.
 */
#include "noc_w8_environments_29900b.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *ea_strdup(const char *s) {
    size_t n = strlen(s ? s : "");
    char *o = (char *)malloc(n + 1);
    if (o) { if (n) memcpy(o, s ? s : "", n); o[n] = '\0'; }
    return o;
}
/* strtok_r-equivalent that PRESERVES empty fields, like Python str.split(sep). */
static char *ea_tok(char *s, char delim, char **ctx) {
    char *p;
    if (s) p = s;
    else {
        p = *ctx;
        if (!p) return NULL;
    }
    char *start = p;
    char *q = strchr(p, delim);
    if (q) { *q = '\0'; *ctx = q + 1; }
    else *ctx = NULL;
    return start;
}
/* Lexical Path.resolve(): separators, drive case, '.' and '..' (no symlinks). */
static char *ea_resolve(const char *p) {
    if (!p) p = "";
    size_t n = strlen(p);
    char *work = (char *)malloc(n + 1);
    if (!work) return NULL;
    for (size_t i = 0; i <= n; ++i) work[i] = p[i] == '\\' ? '/' : p[i];
    char *out = (char *)malloc(n + 3);
    if (!out) { free(work); return NULL; }
    size_t o = 0;
    char *save = NULL, *tok = ea_tok(work, '/', &save);
    while (tok) {
        if (!strcmp(tok, ".")) { tok = ea_tok(NULL, '/', &save); continue; }
        if (!strcmp(tok, "..")) {
            if (o) { while (o && out[o - 1] != '/') o--; if (o) o--; }
            tok = ea_tok(NULL, '/', &save);
            continue;
        }
        if (o == 0 || out[o - 1] != '/') out[o++] = '/';
        for (char *q = tok; *q; ++q) out[o++] = *q;
        tok = ea_tok(NULL, '/', &save);
    }
    if (o == 0) out[o++] = '/';
    out[o] = '\0';
    /* lowercase a leading drive letter */
    if (o >= 2 && out[1] == ':' && out[0] >= 'A' && out[0] <= 'Z') out[0] = (char)tolower((unsigned char)out[0]);
    free(work);
    return out;
}

char *w8_environments_29900b_replace(const char *value, const char *old,
                                     const char *newp, const char *sep) {
    const char *v = value ? value : "", *sp = (sep && *sep) ? sep : ";";
    char *oldr = ea_resolve(old);
    size_t cap = strlen(v) * 2 + strlen(newp ? newp : "") + 8;
    char *out = (char *)malloc(cap);
    if (!out) { free(oldr); return ea_strdup(""); }
    size_t o = 0;
    char *copy = ea_strdup(v);
    char *save = NULL, *tok = ea_tok(copy, sp[0], &save);
    int first = 1;
    while (tok) {
        if (!first) out[o++] = sp[0];
        first = 0;
        if (*tok && oldr) {
            char *tr = ea_resolve(tok);
            if (tr && strcmp(tr, oldr) == 0) {
                size_t nl = strlen(newp ? newp : "");
                memcpy(out + o, newp ? newp : "", nl); o += nl;
            } else {
                size_t tl = strlen(tok); memcpy(out + o, tok, tl); o += tl;
            }
            free(tr);
        } else {
            size_t tl = strlen(tok); memcpy(out + o, tok, tl); o += tl;
        }
        tok = ea_tok(NULL, sp[0], &save);
    }
    out[o] = '\0';
    free(copy); free(oldr);
    return out;
}

/* Parent directory name of a path ("/gen/old/venv" -> "old"). */
static void ea_parent_name(const char *path, char *buf, size_t cap) {
    buf[0] = '\0';
    if (!path || !*path) return;
    size_t n = strlen(path);
    while (n > 0 && (path[n - 1] == '/' || path[n - 1] == '\\')) n--;
    size_t end = n;
    while (end > 0 && path[end - 1] != '/' && path[end - 1] != '\\') end--;
    if (end == 0) return;
    size_t p = end - 1;              /* exclusive end of the parent segment */
    while (p > 0 && path[p - 1] != '/' && path[p - 1] != '\\') p--;
    size_t len = (end - 1) - p;
    if (len >= cap) len = cap - 1;
    memcpy(buf, path + p, len);
    buf[len] = '\0';
}

char *w8_environments_29900b_restart_needed_ex(const char *running, const char *selected) {
    char *r = ea_resolve(running), *s = ea_resolve(selected);
    int same = r && s && strcmp(r, s) == 0;
    free(r); free(s);
    if (same) return NULL;
    char rnbuf[512], snbuf[512];
    ea_parent_name(running, rnbuf, sizeof(rnbuf));
    ea_parent_name(selected, snbuf, sizeof(snbuf));
    size_t n = strlen(snbuf) + strlen(rnbuf) + 128;
    char *out = (char *)malloc(n);
    if (out) snprintf(out, n, "dependency generation %s was published after this process loaded %s", snbuf, rnbuf);
    return out;
}

static char *ea_pending = NULL;
char *w8_environments_29900b_note_pending(const struct json_t *value) {
    free(ea_pending);
    ea_pending = (value && value->type == JSON_STRING) ? ea_strdup(value->str_val) : NULL;
    return ea_pending ? ea_strdup(ea_pending) : NULL;
}
