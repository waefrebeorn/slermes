/*
 * gateway_server.c — real C11 port of the pure module-level helpers of upstream
 * Hermes acp_adapter/gateway_server.py (gate w13_gatewayser_dc3e2a).
 *
 * The GatewayACPAgent class is pure gateway/transport state (async locks, event
 * pump, snapshots); there is no pure computation to port from it. The three
 * module-level helpers below are ported faithfully.
 *
 * PoP: w13_gatewayser_dc3e2a_tools_changed_notice @ acp_adapter/gateway_server.py:_tools_changed_notice
 * PoP: w13_gatewayser_dc3e2a_bypass_route @ acp_adapter/gateway_server.py:_bypass_route
 * PoP: w13_gatewayser_dc3e2a_slash_command @ acp_adapter/gateway_server.py:_slash_command
 */
#include "noc_w13_gatewayser_dc3e2a.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *p; size_t len, cap; } gs_sb_t;

static void gs_init(gs_sb_t *b) {
    b->cap = 256; b->len = 0; b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}
static void gs_reserve(gs_sb_t *b, size_t need) {
    if (!b->p || b->len + need + 1 <= b->cap) return;
    size_t nc = b->cap;
    while (nc < b->len + need + 1) nc *= 2;
    char *np = (char *)realloc(b->p, nc);
    if (np) { b->p = np; b->cap = nc; }
}
static void gs_addn(gs_sb_t *b, const char *s, size_t n) {
    if (!b->p || !s || !n) return;
    gs_reserve(b, n);
    memcpy(b->p + b->len, s, n);
    b->len += n; b->p[b->len] = '\0';
}
static void gs_adds(gs_sb_t *b, const char *s) { if (s) gs_addn(b, s, strlen(s)); }
static void gs_addf(gs_sb_t *b, const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n > 0) { gs_reserve(b, (size_t)n); vsnprintf(b->p + b->len, (size_t)n + 1, fmt, ap2); b->len += (size_t)n; }
    va_end(ap2);
}
static char *gs_done(gs_sb_t *b) {
    if (!b->p) return (char *)calloc(1, 1);
    b->p[b->len] = '\0';
    return b->p;
}
static char *gs_dup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s); char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static int gs_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *gs_strip(const char *s) {
    if (!s) return gs_dup("");
    const char *a = s, *b = s + strlen(s);
    while (a < b && gs_is_ws((unsigned char)*a)) a++;
    while (b > a && gs_is_ws((unsigned char)*(b - 1))) b--;
    size_t n = (size_t)(b - a); char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, a, n); p[n] = '\0'; }
    return p;
}

/* ---- _tools_changed_notice ---------------------------------------------- */
char *w13_gatewayser_dc3e2a_tools_changed_notice(const json_t *payload) {
    const json_t *servers = (payload && payload->type == JSON_OBJECT)
                            ? json_obj_get(payload, "mcp_servers") : NULL;
    gs_sb_t joined; gs_init(&joined);
    if (servers && servers->type == JSON_ARRAY) {
        size_t n = json_len(servers);
        for (size_t i = 0; i < n; i++) {
            if (i) gs_adds(&joined, ", ");
            const json_t *it = json_get(servers, i);
            if (it && it->type == JSON_STRING) gs_adds(&joined, it->str_val ? it->str_val : "");
        }
    }
    char *j = gs_done(&joined);
    const char *srv = j[0] ? j : "none";   /* ", ".join(...) or "none" */
    gs_sb_t out; gs_init(&out);
    gs_addf(&out, "Editor MCP tools changed (servers: %s); the new tool list applies from the next turn.\n", srv);
    free(j);
    return gs_done(&out);
}

/* ---- _bypass_route (env injected explicitly) ----------------------------- */
json_t *w13_gatewayser_dc3e2a_bypass_route(const char *model,
        const char *inference_model, const char *provider) {
    json_t *out = json_object();
    char *mraw = gs_dup(model && model[0] ? model : (inference_model ? inference_model : ""));
    char *m = gs_strip(mraw); free(mraw);
    if (!m[0]) {
        json_set(out, "error", json_string(
            "--safe-mode / --ignore-user-config read no profile default model: launch with one, "
            "e.g. hermes -m <model> --provider <provider> --safe-mode acp"));
        free(m);
        return out;
    }
    char *praw = gs_dup(provider ? provider : "");
    char *p = gs_strip(praw); free(praw);
    if (!p[0]) {
        json_set(out, "error", json_string(
            "--safe-mode / --ignore-user-config read no profile provider: launch with --provider too, "
            "e.g. hermes -m <model> --provider <provider> --safe-mode acp"));
        free(m); free(p);
        return out;
    }
    json_set(out, "model", json_string(m));
    json_set(out, "provider", json_string(p));
    free(m); free(p);
    return out;
}

/* ---- _slash_command ------------------------------------------------------ */
char *w13_gatewayser_dc3e2a_slash_command(const char *text,
        const json_t *commands, const json_t *routes) {
    const char *s = text ? text : "";
    while (*s && gs_is_ws((unsigned char)*s)) s++;   /* text.lstrip() */
    if (*s != '/') return NULL;
    s++;                                             /* [1:] of the first token */
    const char *e = s;
    while (*e && !gs_is_ws((unsigned char)*e)) e++;
    size_t nlen = (size_t)(e - s);
    if (nlen == 0) return NULL;
    for (size_t i = 0; i < nlen; i++) if (s[i] == '/') return NULL;
    char *name = (char *)malloc(nlen + 1);
    memcpy(name, s, nlen); name[nlen] = '\0';

    const json_t *cmd = (commands && commands->type == JSON_OBJECT)
                        ? json_obj_get(commands, name) : NULL;
    if (cmd && cmd->type == JSON_STRING && cmd->str_val) {
        free(name);
        return gs_dup(cmd->str_val);                  /* command.name */
    }
    if (routes && routes->type == JSON_ARRAY) {
        size_t n = json_len(routes);
        for (size_t i = 0; i < n; i++) {
            const json_t *it = json_get(routes, i);
            if (it && it->type == JSON_STRING && it->str_val && !strcmp(it->str_val, name)) {
                free(name);
                return gs_dup(it->str_val);           /* name if name in ROUTES */
            }
        }
    }
    free(name);
    return NULL;
}
