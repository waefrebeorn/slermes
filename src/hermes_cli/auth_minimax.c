/*
 * auth_minimax.c — C port of upstream Hermes hermes_cli/auth_minimax.py.
 *
 * PoP: w9_authminimax_6fbe9a_expired_in_looks_like_unix_ms @ hermes_cli/auth_minimax.py:_minimax_expired_in_looks_like_unix_ms
 * PoP: w9_authminimax_6fbe9a_resolve_token_expiry_unix @ hermes_cli/auth_minimax.py:_minimax_resolve_token_expiry_unix
 * PoP: w9_authminimax_6fbe9a_expiry_fields @ hermes_cli/auth_minimax.py:_minimax_expiry_fields
 *
 * Only the deterministic expiry arithmetic / formatting is ported. HTTP, PKCE
 * randomness and auth-store persistence are intentionally out of scope.
 */
#include "noc_w9_authminimax_6fbe9a.h"
#include "hermes_json.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int py_int(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_NULL || v->type == JSON_ARRAY || v->type == JSON_OBJECT) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isfinite(d)) return 0;
        *out = (long long)d;
        return 1;
    }
    if (v->type == JSON_STRING) {
        const char *p = v->str_val ? v->str_val : "";
        while (*p && isspace((unsigned char)*p)) p++;
        const char *e = p + strlen(p);
        while (e > p && isspace((unsigned char)e[-1])) e--;
        if (e == p) return 0;
        int neg = 0;
        if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
        char digs[256];
        size_t dn = 0;
        int prev_digit = 0, any = 0;
        for (; p < e && dn + 1 < sizeof(digs); p++) {
            if (*p == '_') {
                if (!prev_digit || p + 1 >= e || !isdigit((unsigned char)p[1])) return 0;
                prev_digit = 0;
                continue;
            }
            if (!isdigit((unsigned char)*p)) return 0;
            digs[dn++] = *p;
            prev_digit = 1;
            any = 1;
        }
        if (!any || p != e) return 0;
        digs[dn] = '\0';
        errno = 0;
        char *endp = NULL;
        long long val = strtoll(digs, &endp, 10);
        if (errno == ERANGE || (endp && *endp)) return 0;
        *out = neg ? -val : val;
        return 1;
    }
    return 0;
}

static long long floor_div2(long long v) {
    if (v >= 0) return v / 2;
    return -((-v + 1) / 2); /* Python // for negatives */
}

int w9_authminimax_6fbe9a_expired_in_looks_like_unix_ms(const json_t *expired_in,
                                                       const struct json_t *now_ms) {
    long long raw = 0, nms = 0;
    py_int(expired_in, &raw);
    py_int(now_ms, &nms);
    return raw > floor_div2(nms);
}

double w9_authminimax_6fbe9a_resolve_token_expiry_unix(const json_t *expired_in,
                                                       double now_seconds) {
    long long raw = 0;
    py_int(expired_in, &raw);
    long long now_ms = (long long)(now_seconds * 1000.0); /* int(now.timestamp()*1000) */
    if (raw > floor_div2(now_ms)) return (double)raw / 1000.0;
    long long extra = raw < 1 ? 1 : raw;
    return now_seconds + (double)extra;
}

/* Howard Hinnant's civil_from_days: days since 1970-01-01 -> y/m/d. */
static void civil_from_days(long long z, long long *yy, int *mm, int *dd) {
    z += 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    long long doe = z - era * 146097;
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long y = yoe + era * 400;
    long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long long mp = (5 * doy + 2) / 153;
    long long d = doy - (153 * mp + 2) / 5 + 1;
    long long m = mp + (mp < 10 ? 3 : -9);
    y += (m <= 2);
    *yy = y;
    *mm = (int)m;
    *dd = (int)d;
}

/* Python datetime.fromtimestamp(t, tz=UTC).isoformat(): UTC, no microseconds
 * when zero (fixtures use whole-second instants). */
static void fmt_iso(double t, char *buf, size_t cap) {
    long long secs = (long long)floor(t);
    long long micros = (long long)llround((t - (double)secs) * 1000000.0);
    if (micros >= 1000000) { micros -= 1000000; secs += 1; }
    long long days = (long long)floor((double)secs / 86400.0);
    long long sod = secs - days * 86400;
    if (sod < 0) { sod += 86400; days -= 1; }
    long long yy; int mo, da;
    civil_from_days(days, &yy, &mo, &da);
    int hh = (int)(sod / 3600);
    int mi = (int)((sod % 3600) / 60);
    int ss = (int)(sod % 60);
    if (micros) snprintf(buf, cap, "%04lld-%02d-%02dT%02d:%02d:%02d.%06lld+00:00",
                         yy, mo, da, hh, mi, ss, micros);
    else snprintf(buf, cap, "%04lld-%02d-%02dT%02d:%02d:%02d+00:00",
                  yy, mo, da, hh, mi, ss);
}

struct json_t *w9_authminimax_6fbe9a_expiry_fields(const json_t *expired_in,
                                                   double now_seconds) {
    double expiry = w9_authminimax_6fbe9a_resolve_token_expiry_unix(expired_in, now_seconds);
    char obtained[64], expires[64];
    fmt_iso(now_seconds, obtained, sizeof(obtained));
    fmt_iso(expiry, expires, sizeof(expires));
    double diff = expiry - now_seconds;
    long long expires_in = diff > 0 ? (long long)diff : 0;
    json_t *out = json_object();
    json_set(out, "obtained_at", json_string(obtained));
    json_set(out, "expires_at", json_string(expires));
    json_set(out, "expires_in", json_number((double)expires_in));
    return out;
}
