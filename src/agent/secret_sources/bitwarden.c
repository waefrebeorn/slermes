#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"

/* PoP: agent_secret_sources_bitwarden_u_cache_key_str
 * @ agent/secret_sources/bitwarden.py:_cache_key_str */
/* "|".join(cache_key) over a JSON array of strings. */
char *agent_secret_sources_bitwarden_u_cache_key_str(const char *arg) {
    json_t *a = arg ? json_parse(arg, NULL) : NULL;
    if (!a || !json_is_array(a)) { if (a) json_free(a); return strdup(""); }
    size_t cap = 64, w = 0; char *out = malloc(cap); out[0] = 0;
    for (size_t i = 0; i < a->c.count; i++) {
        json_t *it = a->c.items[i];
        const char *s = (it && json_is_string(it) && it->str_val) ? it->str_val : "";
        size_t need = w + strlen(s) + 2;
        if (need > cap) { cap = need * 2; out = realloc(out, cap); }
        if (i) out[w++] = '|';
        memcpy(out + w, s, strlen(s)); w += strlen(s); out[w] = 0;
    }
    json_free(a);
    return out;
}

/* PoP: agent_secret_sources_bitwarden_u_classify_bws_error
 * @ agent/secret_sources/bitwarden.py:_classify_bws_error
 * classify_cli_error(message, _BWS_ERROR_RULES): lowercase, FIRST rule having any
 * token as a substring wins; no match -> ErrorKind.INTERNAL "internal". */
typedef struct { const char *kind; const char *tok[8]; int nt; } _bws_rule;
static const _bws_rule BWS_RULES[] = {
  {"timeout", {"timed out"}, 1},
  {"binary_missing", {"binary not available","failed to invoke"}, 2},
  {"auth_failed", {"unauthorized","invalid token","access token","401","403","invalid_client","invalid_grant"}, 7},
  {"network", {"network","connection","resolve","download","dns"}, 5},
};
char *agent_secret_sources_bitwarden_u_classify_bws_error(const char *arg) {
    const char *m = arg ? arg : "";
    char *low = malloc(strlen(m) + 1); size_t w = 0;
    for (const char *p = m; *p; p++) { char c = *p; low[w++] = (c>='A'&&c<='Z') ? (char)(c+32) : c; }
    low[w] = 0;
    for (int i = 0; i < (int)(sizeof(BWS_RULES)/sizeof(BWS_RULES[0])); i++)
        for (int k = 0; k < BWS_RULES[i].nt; k++)
            if (strstr(low, BWS_RULES[i].tok[k])) { free(low); return strdup(BWS_RULES[i].kind); }
    free(low);
    return strdup("internal");
}

/* PoP: agent_secret_sources_bitwarden_u_b64e @ agent/secret_sources/bitwarden.py:_b64e */
/* base64.b64encode(raw).decode("ascii") -- standard alphabet WITH padding. */
char *agent_secret_sources_bitwarden_u_b64e(const char *arg) {
    static const char *T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const unsigned char *p = (const unsigned char *)(arg ? arg : "");
    size_t n = strlen((const char *)p);
    size_t cap = ((n + 2) / 3) * 4 + 8;
    char *out = malloc(cap); size_t w = 0;
    for (size_t i = 0; i < n; i += 3) {
        unsigned v = (unsigned)p[i] << 16; int have = 1;
        if (i + 1 < n) { v |= (unsigned)p[i+1] << 8; have = 2; }
        if (i + 2 < n) { v |= (unsigned)p[i+2]; have = 3; }
        out[w++] = T[(v >> 18) & 63];
        out[w++] = T[(v >> 12) & 63];
        out[w++] = (have > 1) ? T[(v >> 6) & 63] : '=';
        out[w++] = (have > 2) ? T[v & 63] : '=';
    }
    out[w] = 0;
    return out;
}
