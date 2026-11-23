/*
 * connector_payload.c - Connector UI payload redaction (C11 port).
 *
 * PoP: conpayload_connector_ui_payload @ tui_gateway/connector_payload.py:connector_ui_payload
 *
 * Faithful port of the module's top-level function.  The dependency
 * agent.redact._key_has_secret_keyword is ported below (same word-boundary
 * policy), and redact_sensitive_text(force=True, redact_url_credentials=True)
 * is ported for the credential shapes the module actually faces on a payload
 * boundary: ENV/assignment secrets, Authorization headers and URL userinfo.
 * Non-matching strings pass through byte-identically, as upstream does.
 */
#include "noc_conpayload.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ---------- dynamic byte buffer ---------- */
typedef struct { char *p; size_t n, cap; } Buf;

static void bput(Buf *b, const char *s, size_t n) {
    if (!n) return;
    if (b->n + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->n + n + 1) nc *= 2;
        char *q = (char *)realloc(b->p, nc);
        if (!q) return;
        b->p = q; b->cap = nc;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n; b->p[b->n] = '\0';
}
static void bputs(Buf *b, const char *s) { if (s) bput(b, s, strlen(s)); }

/* ---------- agent.redact._key_has_secret_keyword ---------- */
static char *cp_lower_dup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    for (size_t i = 0; i < n; i++) o[i] = (char)tolower((unsigned char)s[i]);
    o[n] = '\0';
    return o;
}
static int cp_word_start(const char *s, size_t i) {
    if (i == 0) return 1;
    unsigned char prev = (unsigned char)s[i - 1];
    return !isalpha(prev); /* lowercased key: camelCase boundaries already folded */
}
static int cp_word_end(const char *s, size_t j) {
    size_t n = strlen(s);
    if (j >= n) return 1;
    unsigned char c = (unsigned char)s[j];
    if (!isalpha(c)) return 1;
    if (c == 's' && (j + 1 >= n || !isalpha((unsigned char)s[j + 1]))) return 1;
    return 0;
}
static int cp_kw_bounded(const char *s, const char *kw) {
    size_t k = strlen(kw), n = strlen(s);
    for (size_t i = 0; i + k <= n; i++)
        if (!strncmp(s + i, kw, k) && cp_word_start(s, i) && cp_word_end(s, i + k)) return 1;
    return 0;
}
static int cp_password_kw(const char *low) {
    const char *p = low;
    for (const char *w[] = {"password", "passwd", NULL}, **wp = w; *wp; wp++) {
        p = low;
        size_t lw = strlen(*wp);
        while ((p = strstr(p, *wp)) != NULL) {
            if (cp_word_end(low, (size_t)(p - low) + lw)) return 1;
            p++;
        }
    }
    return 0;
}
static int cp_combo_kw(const char *low) {
    static const char *pre[] = {"api", "auth", "access", "refresh", "session", "secret", NULL};
    static const char *suf[] = {"key", "token", NULL};
    size_t n = strlen(low);
    for (int a = 0; pre[a]; a++) {
        size_t lp = strlen(pre[a]);
        for (size_t i = 0; i + lp <= n; i++) {
            if (strncmp(low + i, pre[a], lp) || !cp_word_start(low, i)) continue;
            size_t j = i + lp;
            if (j < n && (low[j] == '_' || low[j] == ' ' || low[j] == '.' ||
                          low[j] == '-' || low[j] == '\\')) j++;
            for (int b = 0; suf[b]; b++) {
                size_t ls = strlen(suf[b]);
                if (j + ls <= n && !strncmp(low + j, suf[b], ls) && cp_word_end(low, j + ls)) return 1;
            }
        }
    }
    return 0;
}
static int cp_glued_env(const char *key) {
    const char *s = key, *e = key + strlen(key);
    while (s < e && (*s == '"' || *s == '\'')) s++;
    while (e > s && (e[-1] == '"' || e[-1] == '\'')) e--;
    size_t n = (size_t)(e - s);
    return (n == 10 && !strncasecmp(s, "PGPASSWORD", 10)) ||
           (n == 9  && !strncasecmp(s, "MYSQL_PWD", 9)) ||
           (n == 7  && !strncasecmp(s, "SSHPASS", 7));
}
static int cp_key_has_secret(const char *key) {
    if (!key) return 0;
    char *low = cp_lower_dup(key);
    if (!low) return cp_glued_env(key);
    int r = 0;
    static const char *KW[] = {"token", "secret", "key", "auth", "credential", "pass", "pw", NULL};
    for (int i = 0; KW[i]; i++) if (cp_kw_bounded(low, KW[i])) { r = 1; break; }
    if (!r && cp_combo_kw(low)) r = 1;
    if (!r && cp_password_kw(low)) r = 1;
    free(low);
    if (!r && cp_glued_env(key)) r = 1;
    return r;
}
static int cp_is_credential_header_key(const char *key) {
    if (!key) return 0;
    char *low = cp_lower_dup(key);
    if (!low) return 0;
    static const char *BAD[] = {"authorization", "proxy-authorization", "cookie",
                                "set-cookie", "credentials", NULL};
    int r = 0;
    for (int i = 0; BAD[i]; i++) if (!strcmp(low, BAD[i])) { r = 1; break; }
    free(low);
    return r;
}

/* ---------- redact_sensitive_text subset ---------- */
static int cp_env_key_qualifies(const char *tok) {
    int has_upper = 0, has_lower = 0;
    for (const char *p = tok; *p; p++) {
        if (isupper((unsigned char)*p)) has_upper = 1;
        if (islower((unsigned char)*p)) has_lower = 1;
    }
    char *low = cp_lower_dup(tok);
    if (!low) return 0;
    int r = 0;
    if (has_upper && !has_lower) {
        /* all-caps env names: keyword embedded anywhere */
        static const char *KW[] = {"apikey", "key", "token", "secret", "password",
                                   "passwd", "pass", "pw", "credential", "auth", NULL};
        for (int i = 0; KW[i]; i++) if (strstr(low, KW[i])) { r = 1; break; }
    } else {
        /* lowercase/mixed: credential word must sit at an underscore boundary */
        static const char *SUF[] = {"_key", "_pass", "_pw", "_token", "_secret",
                                    "_password", "_passwd", "_credential", "_auth", NULL};
        size_t n = strlen(low);
        for (int i = 0; SUF[i]; i++) {
            size_t l = strlen(SUF[i]);
            if (n > l && !strcmp(low + n - l, SUF[i])) { r = 1; break; }
        }
        /* line-anchored bare password-class key (password=hunter2) */
        if (!r && (!strcmp(low, "password") || !strcmp(low, "passwd") ||
                   !strcmp(low, "pass") || !strcmp(low, "pw"))) r = 1;
    }
    free(low);
    return r;
}
static char *cp_redact_env_assignments(const char *src) {
    Buf b = {0};
    const char *p = src;
    while (*p) {
        if (isalnum((unsigned char)*p) || *p == '_') {
            const char *st = p;
            while (*p && (isalnum((unsigned char)*p) || *p == '_')) p++;
            size_t tl = (size_t)(p - st);
            int qualifies = 0;
            if (*p == '=' && tl < 512) {
                char tok[512];
                memcpy(tok, st, tl); tok[tl] = '\0';
                qualifies = cp_env_key_qualifies(tok);
            }
            if (*p == '=' && qualifies) {
                const char *vs = p + 1;
                const char *v = vs;
                while (*v && !isspace((unsigned char)*v)) v++;
                if (v > vs) {
                    bput(&b, st, tl);
                    bputs(&b, "=***");
                    p = v;
                    continue;
                }
            }
            bput(&b, st, tl);
        } else {
            bput(&b, p, 1);
            p++;
        }
    }
    if (!b.p) return strdup(src ? src : "");
    return b.p;
}
static const char *cp_casestr(const char *h, const char *n) {
    size_t ln = strlen(n);
    for (; *h; h++) if (!strncasecmp(h, n, ln)) return h;
    return NULL;
}
static char *cp_redact_auth_header(const char *src) {
    Buf b = {0};
    const char *s = src;
    size_t i = 0, len = strlen(s);
    for (;;) {
        const char *p = cp_casestr(s + i, "authorization");
        if (!p) { bputs(&b, s + i); break; }
        size_t a = (size_t)(p - s);
        bput(&b, s + i, a - i);
        size_t r = a + 13; /* strlen("authorization") */
        while (s[r] == ' ' || s[r] == '\t') r++;
        if (s[r] != ':') { bput(&b, s + a, 13); i = a + 13; continue; }
        r++;
        while (s[r] == ' ' || s[r] == '\t') r++;
        size_t tok = r;
        while (s[tok] && !isspace((unsigned char)s[tok])) tok++;
        while (s[tok] && isspace((unsigned char)s[tok])) tok++;
        if (tok >= len || !s[tok]) { bputs(&b, s + a); break; }
        bput(&b, s + a, tok - a); /* keep "Authorization: Bearer " */
        bputs(&b, "***");
        size_t v = tok;
        while (s[v] && !isspace((unsigned char)s[v])) v++;
        i = v;
        (void)len;
    }
    if (!b.p) return strdup(src ? src : "");
    return b.p;
}
static char *cp_redact_url_userinfo(const char *src) {
    Buf b = {0};
    const char *s = src;
    size_t i = 0, len = strlen(s);
    while (i < len) {
        const char *at = strstr(s + i, "://");
        if (!at) { bputs(&b, s + i); break; }
        size_t scheme_end = (size_t)(at + 3 - s);
        bput(&b, s + i, scheme_end - i);
        size_t k = scheme_end;
        while (k < len && s[k] != '/' && !isspace((unsigned char)s[k])) k++;
        size_t amp = scheme_end;
        while (amp < k && s[amp] != '@') amp++;
        int colon = 0;
        for (size_t t = scheme_end; t < amp; t++) if (s[t] == ':') { colon = 1; break; }
        if (amp < k && colon) {
            bputs(&b, "***:***@");
            i = amp + 1;
        } else {
            bput(&b, s + scheme_end, k - scheme_end);
            i = k;
        }
    }
    if (!b.p) return strdup(src ? src : "");
    return b.p;
}
static char *cp_redact_text(const char *text) {
    if (!text || !*text) return strdup(text ? text : "");
    char *a = cp_redact_env_assignments(text);
    char *b = cp_redact_auth_header(a);
    char *c = cp_redact_url_userinfo(b);
    free(a); free(b);
    return c;
}

/* ---------- public entry point ---------- */
json_t *conpayload_connector_ui_payload(const json_t *value) {
    if (!value) return json_null();
    switch (value->type) {
        case JSON_OBJECT: {
            json_t *out = json_object();
            for (size_t i = 0; i < json_object_size(value); i++) {
                const char *k = json_object_get_key_at(value, i);
                json_t *item = json_object_get_at(value, i);
                json_t *v;
                int is_bool = item && item->type == JSON_BOOL;
                if (!is_bool && (cp_key_has_secret(k) || cp_is_credential_header_key(k))) {
                    v = json_string("[REDACTED]");
                } else if (k && !strcmp(k, "connect_url") && item && item->type == JSON_STRING) {
                    v = json_copy(item); /* authorization links survive verbatim */
                } else {
                    v = conpayload_connector_ui_payload(item);
                }
                json_set(out, k, v);
            }
            return out;
        }
        case JSON_ARRAY: {
            json_t *out = json_array();
            for (size_t i = 0; i < json_len(value); i++)
                json_append(out, conpayload_connector_ui_payload(json_get(value, i)));
            return out;
        }
        case JSON_STRING: {
            char *r = cp_redact_text(value->str_val ? value->str_val : "");
            json_t *o = json_string(r ? r : "");
            free(r);
            return o;
        }
        default:
            return json_copy(value);
    }
}
