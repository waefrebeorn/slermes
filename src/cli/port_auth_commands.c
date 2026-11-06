/*
 * port_auth_commands.c — _display_source + _classify_exhausted_status.
 *
 * _classify_exhausted_status: `code in codes or any(t in reason for t in
 * reason_tokens) or any(t in message for t in message_tokens)` — first-match
 * wins over _EXHAUSTED_CLASSES; reason/message lowercased+stripped first.
 * `code in codes` uses a real code (JSON int, passed as a numeric string) and
 * succeeds only when it is one of {429} / {401,403}; a NULL/non-numeric code is
 * absent and can only win via the reason/message tokens.
 */

#include "port_auth_commands.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *acm_strndup(const char *s, size_t n)
{
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* lowercased copy trimmed of leading/trailing whitespace */
static char *acm_lower(const char *s)
{
    size_t n;
    char *o;
    if (!s) return acm_strndup("", 0);
    while (*s && isspace((unsigned char)*s)) s++;
    n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    o = malloc(n + 1);
    if (!o) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        o[i] = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
    }
    o[n] = '\0';
    return o;
}

static int acm_substr(const char *hay, const char *const *toks, size_t nt)
{
    if (!hay) hay = "";
    for (size_t i = 0; i < nt; i++)
        if (strstr(hay, toks[i])) return 1;
    return 0;
}

/* parsed code from the numeric string; returns -1 when absent/non-numeric */
static long acm_code(const char *code)
{
    char *end = NULL;
    long v;
    if (!code || !code[0]) return -1;
    v = strtol(code, &end, 10);
    if (end == code || *end != '\0') return -1;
    return v;
}

/* PoP: _display_source @ hermes_cli/auth_commands.py:_display_source */
char *acm_display_source(const char *source)
{
    const char *s = source ? source : "";
    if (strncmp(s, "manual:", 7) == 0)
        return acm_strndup(s + 7, strlen(s) - 7);
    return acm_strndup(s, strlen(s));
}

/* PoP: _classify_exhausted_status @ hermes_cli/auth_commands.py:_classify_exhausted_status */
char *acm_classify_exhausted(const char *code, const char *reason, const char *message)
{
    static const char *const RL_REASON[] = { "rate_limit", "usage_limit", "quota", "exhausted" };
    static const char *const RL_MSG[] = { "rate limit", "usage limit", "quota", "too many requests" };
    static const char *const AF_REASON[] = { "invalid_token", "invalid_grant", "unauthorized", "forbidden", "auth" };
    static const char *const AF_MSG[] = { "unauthorized", "forbidden", "expired", "revoked", "invalid token", "authentication" };
    long c = acm_code(code);
    char *rl = acm_lower(reason), *ml = acm_lower(message);
    char *out = NULL;

    /* rate-limited: code 429 or token in reason/message */
    if (c == 429 || acm_substr(rl, RL_REASON, 4) || acm_substr(ml, RL_MSG, 4)) {
        out = acm_strndup("rate-limited,1", 14);
        goto done;
    }
    /* auth failed: code 401/403 or token in reason/message */
    if (c == 401 || c == 403 ||
        acm_substr(rl, AF_REASON, 5) || acm_substr(ml, AF_MSG, 6)) {
        out = acm_strndup("auth failed,0", 13);
        goto done;
    }
    out = acm_strndup("exhausted,1", 11);
done:
    free(rl);
    free(ml);
    return out;
}
