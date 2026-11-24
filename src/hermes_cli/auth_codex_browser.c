/*
 * auth_codex_browser.c — real C11 (helper) port of upstream Hermes
 * hermes_cli/auth_codex_browser.py.
 *
 * PoP: w17_authcodex_050b10_authorize_url @ hermes_cli/auth_codex_browser.py:_codex_browser_authorize_url
 * PoP: w17_authcodex_050b10_login_flow   @ hermes_cli/auth_codex_browser.py:_codex_login_flow
 *
 * Only the deterministic helpers are ported: loopback/network/token-exchange
 * helpers are deliberately out of scope for a fixture-driven differential gate.
 */
#include "noc_w17_authcodex_050b10.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUTH_URL "https://auth.openai.com/oauth/authorize?"
#define CLIENT_ID "app_EMoamEEZ73f0CkXaXp7hrann"

/* Python urllib.parse.quote_plus with safe="": unreserved bytes pass through,
 * space becomes '+', everything else is %XX (uppercase hex) per UTF-8 byte. */
static char *noc_ac_quote_plus(const char *s) {
    static const char hex[] = "0123456789ABCDEF";
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n * 3 + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-' || c == '~') {
            out[j++] = (char)c;
        } else if (c == ' ') {
            out[j++] = '+';
        } else {
            out[j++] = '%';
            out[j++] = hex[c >> 4];
            out[j++] = hex[c & 15];
        }
    }
    out[j] = '\0';
    return out;
}

json_t *w17_authcodex_050b10_authorize_url(const char *redirect_uri, const char *state,
                                           const char *code_challenge) {
    const char *ru = redirect_uri ? redirect_uri : "";
    const char *st = state ? state : "";
    const char *cc = code_challenge ? code_challenge : "";
    char *q_ru = noc_ac_quote_plus(ru);
    char *q_st = noc_ac_quote_plus(st);
    char *q_cc = noc_ac_quote_plus(cc);
    if (!q_ru || !q_st || !q_cc) {
        free(q_ru); free(q_st); free(q_cc);
        return json_string("");
    }
    static const char p1[] = AUTH_URL "response_type=code&client_id=" CLIENT_ID "&redirect_uri=";
    static const char p2[] = "&scope=openid+profile+email+offline_access&code_challenge=";
    static const char p3[] = "&code_challenge_method=S256&id_token_add_organizations=true&state=";
    size_t total = strlen(p1) + strlen(q_ru) + strlen(p2) + strlen(q_cc) + strlen(p3) + strlen(q_st) + 1;
    char *buf = (char *)malloc(total);
    if (!buf) { free(q_ru); free(q_st); free(q_cc); return json_string(""); }
    char *w = buf;
    memcpy(w, p1, strlen(p1)); w += strlen(p1);
    memcpy(w, q_ru, strlen(q_ru)); w += strlen(q_ru);
    memcpy(w, p2, strlen(p2)); w += strlen(p2);
    memcpy(w, q_cc, strlen(q_cc)); w += strlen(q_cc);
    memcpy(w, p3, strlen(p3)); w += strlen(p3);
    memcpy(w, q_st, strlen(q_st)); w += strlen(q_st);
    *w = '\0';
    json_t *out = json_string(buf);
    free(buf); free(q_ru); free(q_st); free(q_cc);
    return out;
}

/* Python str(value) for the scalar types fixtures exercise. */
static const char *noc_ac_py_str(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) return "none";
    if (v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    if (v->type == JSON_BOOL) return v->bool_val ? "true" : "false";
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, cap, "%lld", (long long)v->num_val);
        else
            snprintf(buf, cap, "%g", v->num_val);
        return buf;
    }
    return "none";
}

json_t *w17_authcodex_050b10_login_flow(int browser, const json_t *config) {
    if (browser) return json_string("browser");
    const json_t *auth = json_is_object(config) ? json_obj_get(config, "auth") : NULL;
    const char *flow = "device_code";
    char numbuf[64];
    if (json_is_object(auth)) {
        const json_t *v = json_obj_get(auth, "codex_login_flow");
        if (v && v->type != JSON_NULL) flow = noc_ac_py_str(v, numbuf, sizeof numbuf);
    }
    /* flow.strip().lower() or "device_code" */
    const char *p = flow ? flow : "";
    while (*p && isspace((unsigned char)*p)) p++;
    const char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - p);
    char norm[256];
    if (n >= sizeof norm) n = sizeof norm - 1;
    for (size_t i = 0; i < n; i++) norm[i] = (char)tolower((unsigned char)p[i]);
    norm[n] = '\0';
    if (norm[0] == '\0') return json_string("device_code");
    if (!strcmp(norm, "browser")) return json_string("browser");
    if (!strcmp(norm, "device_code")) return json_string("device_code");
    return json_string("device_code");
}
