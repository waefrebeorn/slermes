/*
 * status_auth.c — C home for upstream Hermes hermes_cli/status_auth.py.
 * PoP: w8_statusauth_924779_qwen_expiry @ hermes_cli/status_auth.py:_qwen_expiry
 * PoP: w8_statusauth_924779_format_iso_timestamp @ hermes_cli/status_auth.py:_format_iso_timestamp
 */
#include "noc_w8_statusauth_924779.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Howard Hinnant's civil <-> days algorithms (proleptic Gregorian). */
static long long days_from_civil(long long y, unsigned m, unsigned d) {
    y -= (m <= 2);
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}
static void civil_from_days(long long z, long long *y, unsigned *m, unsigned *d) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long yy = (long long)yoe + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp + (mp < 10 ? 3 : -9);
    *y = yy + (*m <= 2);
}
static void epoch_to_utc(long long secs, long long *y, unsigned *mo, unsigned *d,
                         unsigned *hh, unsigned *mi, unsigned *ss) {
    long long days = secs / 86400;
    long long rem = secs % 86400;
    if (rem < 0) { rem += 86400; days -= 1; }
    civil_from_days(days, y, mo, d);
    *hh = (unsigned)(rem / 3600); rem %= 3600;
    *mi = (unsigned)(rem / 60); *ss = (unsigned)(rem % 60);
}
static int readn(const char **pp, int n, long long *out) {
    long long v = 0;
    for (int i = 0; i < n; i++) {
        if (!isdigit((unsigned char)**pp)) return 0;
        v = v * 10 + (**pp - '0');
        (*pp)++;
    }
    *out = v;
    return 1;
}
/* Accepts the fromisoformat shapes used by status output:
 * YYYY-MM-DD[T ]HH:MM:SS[.ffffff][Z|(+|-)HH:MM]; naive => UTC. */
static int parse_iso(const char *s, long long *epoch, int *micro) {
    if (!s) return 0;
    const char *p = s;
    long long y, mo, d, hh, mi, ss;
    if (!readn(&p, 4, &y) || *p++ != '-') return 0;
    if (!readn(&p, 2, &mo) || *p++ != '-') return 0;
    if (!readn(&p, 2, &d)) return 0;
    if (*p != 'T' && *p != ' ') return 0;
    p++;
    if (!readn(&p, 2, &hh) || *p++ != ':') return 0;
    if (!readn(&p, 2, &mi) || *p++ != ':') return 0;
    if (!readn(&p, 2, &ss)) return 0;
    int us = 0;
    if (*p == '.') {
        p++; int k = 0;
        while (isdigit((unsigned char)*p) && k < 6) { us = us * 10 + (*p - '0'); p++; k++; }
        while (isdigit((unsigned char)*p)) p++;
        while (k < 6) { us *= 10; k++; }
    }
    long long off = 0;
    if (*p == 'Z' || *p == 'z') {
        p++;
    } else if (*p == '+' || *p == '-') {
        int sign = (*p == '-') ? -1 : 1; p++;
        long long oh = 0, om = 0;
        if (!readn(&p, 2, &oh)) return 0;
        if (*p == ':') p++;
        if (!readn(&p, 2, &om)) return 0;
        off = sign * (oh * 3600 + om * 60);
    } else if (*p != '\0') {
        return 0;
    }
    if (*p != '\0') return 0;
    long long secs = days_from_civil(y, (unsigned)mo, (unsigned)d) * 86400
                   + hh * 3600 + mi * 60 + ss - off;
    *epoch = secs; *micro = us;
    return 1;
}

struct json_t *w8_statusauth_924779_qwen_expiry(const char *expires_at_ms) {
    long long ms = expires_at_ms ? strtoll(expires_at_ms, NULL, 10) : 0;
    long long secs = ms / 1000;
    int us = (int)(ms - secs * 1000) * 1000;   /* millisecond -> microsecond */
    long long y; unsigned mo, d, hh, mi, ss;
    epoch_to_utc(secs, &y, &mo, &d, &hh, &mi, &ss);
    char out[80];
    if (us != 0)
        snprintf(out, sizeof(out), "%04lld-%02u-%02uT%02u:%02u:%02u.%06d+00:00",
                 y, mo, d, hh, mi, ss, us);
    else
        snprintf(out, sizeof(out), "%04lld-%02u-%02uT%02u:%02u:%02u+00:00",
                 y, mo, d, hh, mi, ss);
    return json_string(out);
}

struct json_t *w8_statusauth_924779_format_iso_timestamp(const struct json_t *value) {
    if (!value || value->type != JSON_STRING) return json_string("(unknown)");
    const char *raw = value->str_val;
    char stripped[2048];
    size_t n = strlen(raw);
    if (n >= sizeof(stripped)) return json_string(raw);
    memcpy(stripped, raw, n + 1);
    size_t a = 0, b = n;
    while (a < b && isspace((unsigned char)stripped[a])) a++;
    while (b > a && isspace((unsigned char)stripped[b - 1])) b--;
    stripped[b] = '\0';
    const char *text = stripped + a;
    if (*text == '\0') return json_string("(unknown)");

    char parsebuf[2060];
    size_t L = strlen(text);
    memcpy(parsebuf, text, L + 1);
    if (L && parsebuf[L - 1] == 'Z') { parsebuf[L - 1] = '+'; strcat(parsebuf, "00:00"); }
    long long epoch; int us;
    if (!parse_iso(parsebuf, &epoch, &us)) return json_string(raw);
    (void)us;
    long long y; unsigned mo, d, hh, mi, ss;
    epoch_to_utc(epoch, &y, &mo, &d, &hh, &mi, &ss);
    char out[96];
    /* Built with TZ=UTC in the oracle: astimezone() -> UTC, %Z -> "UTC". */
    snprintf(out, sizeof(out), "%04lld-%02u-%02u %02u:%02u:%02u UTC", y, mo, d, hh, mi, ss);
    return json_string(out);
}
