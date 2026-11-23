/*
 * web_server_session_token.c — real C11 port of upstream
 * hermes_cli/web_server_session_token.py.
 *
 * PoP: wsstoken_resolve @ hermes_cli/web_server_session_token.py:resolve_session_token
 *
 * The Python function reads one env var, and for a "file:" value reads a UTF-8
 * (BOM-stripping) text file and .strip()s it. Environment, fresh-token minting
 * and file I/O are injectable seams (see noc_wsstoken.h); all branching below is
 * the exact branch structure of resolve_session_token().
 */
#include "noc_wsstoken.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *wss_dup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (!p) return (char *)calloc(1, 1);
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* Python str.strip() whitespace set (ASCII portion). */
static int wss_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* utf-8-sig drops a leading BOM, then Python .strip() trims both ends. */
static char *wss_decode_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n;
    const char *a, *b;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB &&
        (unsigned char)p[2] == 0xBF)
        p += 3;
    n = strlen(p);
    a = p;
    b = p + n;
    while (a < b && wss_ws((unsigned char)*a)) a++;
    while (b > a && wss_ws((unsigned char)*(b - 1))) b--;
    {
        char *out = (char *)malloc((size_t)(b - a) + 1);
        if (!out) return wss_dup("");
        if (b > a) memcpy(out, a, (size_t)(b - a));
        out[b - a] = '\0';
        return out;
    }
}

static char *wss_msg(const char *prefix, const char *path, const char *suffix) {
    size_t cap = strlen(prefix) + strlen(path ? path : "") + strlen(suffix ? suffix : "") + 8;
    char *p = (char *)malloc(cap);
    if (!p) return wss_dup("");
    snprintf(p, cap, "%s%s%s", prefix, path ? path : "", suffix ? suffix : "");
    return p;
}

/*
 * PoP: wsstoken_resolve @ hermes_cli/web_server_session_token.py:resolve_session_token
 */
wsstoken_result_t wsstoken_resolve(const char *env_value,
                                   const char *file_content,
                                   const char *minted,
                                   const char *file_error) {
    wsstoken_result_t r;
    const char *env = env_value ? env_value : "";
    r.is_exit = 0;
    r.value = NULL;

    /* env_token = os.environ.get(...); if not env_token: return secrets.token_urlsafe(32) */
    if (!*env) {
        r.value = wss_dup(minted ? minted : "");
        return r;
    }
    /* if env_token.startswith("file:"): */
    if (strncmp(env, "file:", 5) == 0) {
        const char *token_file = env + 5;
        if (file_error) {
            r.is_exit = 1;
            r.value = wss_msg("HERMES_DASHBOARD_SESSION_TOKEN names an unreadable token file (",
                              token_file, "): ");
            {
                char *full = (char *)malloc(strlen(r.value) + strlen(file_error) + 2);
                if (full) {
                    snprintf(full, strlen(r.value) + strlen(file_error) + 2,
                             "%s%s", r.value, file_error);
                    free(r.value);
                    r.value = full;
                }
            }
            return r;
        }
        r.value = wss_decode_strip(file_content);
        /* if not token: raise SystemExit("...empty token file (...)") */
        if (!*r.value) {
            char *msg = wss_msg("HERMES_DASHBOARD_SESSION_TOKEN names an empty token file (",
                                token_file, ")");
            free(r.value);
            r.value = msg;
            r.is_exit = 1;
            return r;
        }
        return r;
    }
    /* return env_token */
    r.value = wss_dup(env);
    return r;
}

void wsstoken_result_free(wsstoken_result_t *r) {
    if (r && r->value) { free(r->value); r->value = NULL; }
}
