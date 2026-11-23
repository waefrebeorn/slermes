/*
 * credential_pool_timestamps.c — C port of upstream Hermes agent/credential_pool_timestamps.py.
 * PoP: credtstamp_parse_absolute_timestamp @ agent/credential_pool_timestamps.py:_parse_absolute_timestamp
 *
 * Faithful ordering: None/"" -> None; int/float & bool (bool is an int in
 * Python) use the <=0 guard and the >1e12 millisecond rule; strings strip
 * whitespace, try float() first (WITHOUT the <=0 guard), then
 * datetime.fromisoformat().timestamp().
 */
#include "noc_credtstamp.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char *dup_trim(const char *s) {
    size_t n = s ? strlen(s) : 0, a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (b > a) memcpy(out, s + a, b - a);
    out[b - a] = 0;
    return out;
}
static int digits(const char *s, int n) {
    for (int i = 0; i < n; i++) if (!isdigit((unsigned char)s[i])) return 0;
    return 1;
}
static int two(const char *s) { return (s[0] - '0') * 10 + (s[1] - '0'); }

/* Howard Hinnant's days-from-civil: proleptic Gregorian days since 1970-01-01. */
static long long days_from_civil(int y, unsigned m, unsigned d) {
    y -= (m <= 2);
    unsigned era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (long long)era * 146097 + (long long)doe - 719468;
}

/* datetime.fromisoformat(raw).timestamp() for the supported ISO-8601 shapes. */
static int parse_iso(const char *raw, double *out) {
    size_t n = strlen(raw);
    if (n < 10 || !digits(raw, 4) || raw[4] != '-' || !digits(raw + 5, 2) ||
        raw[7] != '-' || !digits(raw + 8, 2)) return 0;
    int y = (raw[0]-'0')*1000 + (raw[1]-'0')*100 + (raw[2]-'0')*10 + (raw[3]-'0');
    int mo = two(raw + 5), d = two(raw + 8);
    if (mo < 1 || mo > 12 || d < 1 || d > 31) return 0;
    size_t i = 10;
    int hh = 0, mm = 0, ss = 0;
    double frac = 0.0;
    if (i < n && (raw[i] == 'T' || raw[i] == 't' || raw[i] == ' ')) {
        i++;
        if (i + 5 > n || !digits(raw + i, 2)) return 0;
        hh = two(raw + i); i += 2;
        if (i >= n || raw[i] != ':') return 0;
        i++;
        if (i + 2 > n || !digits(raw + i, 2)) return 0;
        mm = two(raw + i); i += 2;
        if (i < n && raw[i] == ':') {
            i++;
            if (i + 2 > n || !digits(raw + i, 2)) return 0;
            ss = two(raw + i); i += 2;
        }
        if (i < n && (raw[i] == '.' || raw[i] == ',')) {
            i++;
            size_t fs = i;
            while (i < n && isdigit((unsigned char)raw[i])) i++;
            if (i == fs || i - fs >= 32) return 0;
            char tmp[40];
            tmp[0] = '0'; tmp[1] = '.';
            memcpy(tmp + 2, raw + fs, i - fs);
            tmp[2 + (i - fs)] = 0;
            frac = strtod(tmp, NULL);
        }
    }
    long offset = 0;
    int aware = 0;
    if (i < n) {
        char z = raw[i];
        if (z == 'Z' || z == 'z') {
            aware = 1; i++;
        } else if (z == '+' || z == '-') {
            int sign = (z == '-') ? -1 : 1;
            i++;
            if (i + 2 > n || !digits(raw + i, 2)) return 0;
            int oh = two(raw + i); i += 2;
            int om = 0, os = 0;
            if (i < n && raw[i] == ':') i++;
            if (i + 2 <= n && digits(raw + i, 2)) { om = two(raw + i); i += 2; }
            if (i < n && raw[i] == ':') {
                i++;
                if (i + 2 > n || !digits(raw + i, 2)) return 0;
                os = two(raw + i); i += 2;
            }
            if (i != n || oh > 23 || om > 59 || os > 59) return 0;
            offset = sign * (oh * 3600 + om * 60 + os);
            aware = 1;
        } else {
            return 0;
        }
    }
    if (i != n || hh > 23 || mm > 59 || ss > 59) return 0;
    double epoch;
    if (aware) {
        epoch = (double)(days_from_civil(y, (unsigned)mo, (unsigned)d) * 86400LL
                         + hh * 3600 + mm * 60 + ss) - (double)offset + frac;
    } else {
        struct tm t;
        memset(&t, 0, sizeof(t));
        t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d;
        t.tm_hour = hh; t.tm_min = mm; t.tm_sec = ss; t.tm_isdst = -1;
        epoch = (double)mktime(&t) + frac;
    }
    *out = epoch;
    return 1;
}

int credtstamp_parse_absolute_timestamp(const json_t *value, double *out) {
    if (!value || value->type == JSON_NULL) return 0;
    double numeric;
    if (value->type == JSON_BOOL) { /* bool is an int subclass in Python */
        numeric = value->bool_val ? 1.0 : 0.0;
        if (numeric <= 0) return 0;
        *out = numeric;
        return 1;
    }
    if (value->type == JSON_NUMBER) {
        numeric = value->num_val;
        if (numeric <= 0) return 0;
        *out = numeric > 1000000000000.0 ? numeric / 1000.0 : numeric;
        return 1;
    }
    if (value->type == JSON_STRING) {
        char *raw = dup_trim(value->str_val ? value->str_val : "");
        if (!*raw) { free(raw); return 0; }
        char *end = NULL;
        double num = strtod(raw, &end);
        if (end != raw && end && *end == '\0') {
            /* String branch intentionally has no <=0 guard, matching Python. */
            *out = num > 1000000000000.0 ? num / 1000.0 : num;
            free(raw);
            return 1;
        }
        int ok = parse_iso(raw, out);
        free(raw);
        return ok;
    }
    return 0;
}
