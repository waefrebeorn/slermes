/*
 * port_turn_lease.c — pure helpers from gateway/turn_lease.py.
 */

#include "port_turn_lease.h"

#include <fenv.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python repr() of a str: single-quoted, escapes backslash, quote, and
 * common control chars. */
static char *tls_py_repr(const char *s)
{
    size_t len = s ? strlen(s) : 0;
    char *out = malloc(len * 4 + 3);
    size_t j = 0, i;
    int has_sq = 0, has_dq = 0;
    char qc;
    if (!out) return NULL;
    for (i = 0; i < len; i++) {
        if (s[i] == '\'') has_sq = 1;
        if (s[i] == '"') has_dq = 1;
    }
    /* Python picks double quotes when there is a single quote but no double
     * quote, to minimize escapes. */
    qc = (has_sq && !has_dq) ? '"' : '\'';
    out[j++] = qc;
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
        case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
        case '\n': out[j++] = '\\'; out[j++] = 'n'; break;
        case '\r': out[j++] = '\\'; out[j++] = 'r'; break;
        case '\t': out[j++] = '\\'; out[j++] = 't'; break;
        default:
            if (c == (unsigned char)qc) { out[j++] = '\\'; out[j++] = (char)c; }
            else if (c < 32 || c == 127) {
                snprintf((char *)&out[j], 8, "\\x%02x", c);
                j += 4;
            } else {
                out[j++] = (char)c;
            }
        }
    }
    out[j++] = qc;
    out[j] = '\0';
    return out;
}

char *tls_holder_desc(const char *owner_key, long long generation, int present)
{
    char buf[1024];
    if (present) {
        const char *s = owner_key ? owner_key : "";
        size_t j = 0, i;
        buf[j++] = '[';
        buf[j++] = '"';
        for (i = 0; s[i] && j + 4 < sizeof(buf); i++) {
            unsigned char c = (unsigned char)s[i];
            if (c == '"' || c == '\\') { buf[j++] = '\\'; buf[j++] = (char)c; }
            else if (c < 32) {
                snprintf(&buf[j], sizeof(buf) - j, "\\u%04x", c);
                j += 6;
            } else {
                buf[j++] = (char)c;
            }
        }
        buf[j++] = '"';
        buf[j++] = ',';
        j += snprintf(&buf[j], sizeof(buf) - j, "%lld", generation);
        buf[j++] = ']';
        buf[j] = '\0';
    } else {
        snprintf(buf, sizeof(buf), "[\"?\",\"?\"]");
    }
    return strdup(buf);
}

char *tls_token_repr(const char *session_id, const char *owner_key,
                     long long generation, int released)
{
    char *s = tls_py_repr(session_id ? session_id : "");
    char *k = tls_py_repr(owner_key ? owner_key : "");
    size_t cap = strlen(s) + strlen(k) + 16 + 64 + 16 + 32;
    char *out = malloc(cap);
    if (!out) { free(s); free(k); return NULL; }
    snprintf(out, cap, "TurnLeaseToken(session_id=%s, owner_key=%s, "
             "generation=%lld, released=%s)",
             s, k, generation, released ? "True" : "False");
    free(s);
    free(k);
    return out;
}

char *tls_timeout_msg(const char *session_id, const char *owner_key,
                      long long generation, double wait_seconds)
{
    /* Python f"{x:.0f}" rounds half to even (banker's). nearbyint under
     * FE_TONEAREST matches; assert the rounding mode is the default. */
    long whole;
    char buf[512];
    fesetround(FE_TONEAREST);
    whole = (long)nearbyint(wait_seconds);
    snprintf(buf, sizeof(buf), "turn lease wait timed out after %lds on session "
             "%s for routing key %s (gen %lld)",
             whole, session_id ? session_id : "", owner_key ? owner_key : "",
             generation);
    return strdup(buf);
}
