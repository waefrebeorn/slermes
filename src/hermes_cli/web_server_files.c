/*
 * web_server_files.c — C port of upstream Hermes hermes_cli/web_server_files.py.
 *
 * PoP: w15_webserver_995db6_path_text @ hermes_cli/web_server_files.py:_path_text
 * PoP: w15_webserver_995db6_is_container_cwd @ hermes_cli/web_server_files.py:_is_container_cwd
 * PoP: w15_webserver_995db6_path_is_under @ hermes_cli/web_server_files.py:_path_is_under
 *
 * pathlib containment is modelled lexically: both paths are split on '/' and
 * '\\' (Windows Path treats both as separators), empty and '.' components are
 * dropped while '..' is retained, exactly as PurePath.parts does. Both operands
 * come from the same platform in the oracle, so the comparison is faithful.
 */
#include "noc_w15_webserver_995db6.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

struct json_t *w15_webserver_995db6_path_text(const char *raw, int has_nul) {
    if (has_nul) {
        struct json_t *err = json_object();
        json_set(err, "error", json_string("Invalid path"));
        json_set(err, "status", json_number(400));
        return err;
    }
    const char *p = raw ? raw : "";
    const char *e = p + strlen(p);
    while (p < e && isspace((unsigned char)*p)) p++;
    while (e > p && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - p);
    char *text = (char *)malloc(n + 1);
    if (!text) return json_null();
    memcpy(text, p, n);
    text[n] = '\0';                 /* NUL at the new, shortened length */
    struct json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    json_set(out, "value", json_string(text));
    free(text);
    return out;
}

int w15_webserver_995db6_is_container_cwd(const char *host) {
    if (!host) return 0;
    const char *roots[] = {"/workspace", "/root"};
    for (int i = 0; i < 2; i++) {
        size_t n = strlen(roots[i]);
        if (strcmp(host, roots[i]) == 0) return 1;
        if (strncmp(host, roots[i], n) == 0 && host[n] == '/') return 1;
    }
    return 0;
}

/* ---- lexical pathlib component vector ---------------------------------- */
typedef struct { char **v; size_t n, cap; } Parts;
static void parts_free(Parts *p) {
    for (size_t i = 0; i < p->n; i++) free(p->v[i]);
    free(p->v); p->v = NULL; p->n = p->cap = 0;
}
static void parts_pushn(Parts *p, const char *s, size_t n) {
    if (p->n == p->cap) {
        size_t nc = p->cap ? p->cap * 2 : 8;
        char **nv = (char **)realloc(p->v, nc * sizeof(char *));
        if (!nv) return;
        p->v = nv; p->cap = nc;
    }
    char *copy = (char *)malloc(n + 1);
    if (!copy) return;
    memcpy(copy, s, n); copy[n] = '\0';
    p->v[p->n++] = copy;
}
static void path_parts(const char *path, Parts *out) {
    if (!path) return;
    const char *p = path;
    if (*p == '/' || *p == '\\') {
        parts_pushn(out, "/", 1);       /* platform root, normalized to one token */
        while (*p == '/' || *p == '\\') p++;
    }
    while (*p) {
        while (*p == '/' || *p == '\\') p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != '/' && *p != '\\') p++;
        size_t n = (size_t)(p - start);
        if (n == 1 && start[0] == '.') continue;   /* '.' components are dropped */
        parts_pushn(out, start, n);               /* '..' retained */
    }
}

int w15_webserver_995db6_path_is_under(const char *root, const char *target) {
    Parts r = {0}, t = {0};
    path_parts(root, &r);
    path_parts(target, &t);
    int result = 0;
    if (r.n <= t.n) {
        int same = 1;
        for (size_t i = 0; i < r.n; i++) {
            if (strcmp(r.v[i], t.v[i]) != 0) { same = 0; break; }
        }
        /* target == root (equal lengths) or root is a proper ancestor. */
        if (same) result = 1;
    }
    parts_free(&r);
    parts_free(&t);
    return result;
}
