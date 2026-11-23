/*
 * occurrences.c -- faithful C11 port of upstream Hermes cron/occurrences.py.
 *
 * PoP: w6_occurrences_d831fa_scheduled_instant @ cron/occurrences.py:scheduled_instant
 */
#include "noc_w6_occurrences_d831fa.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_uint(const char **p, int min_d, int max_d, int *out) {
    int n = 0, v = 0;
    while (**p && isdigit((unsigned char)**p) && n < max_d) {
        v = v * 10 + (**p - '0');
        (*p)++; n++;
    }
    if (n < min_d) return 0;
    *out = v;
    return 1;
}

static int leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
static int mdays(int y, int m) {
    static const int d[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2 && leap(y)) return 29;
    return (m >= 1 && m <= 12) ? d[m] : 0;
}

/* Howard Hinnant's civil<->days algorithms (days since 1970-01-01). */
static long long days_from_civil(long long y, int m, int d) {
    y -= (m <= 2);
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}
static void civil_from_days(long long z, long long *y, int *m, int *d) {
    z += 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long yy = (long long)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp + (mp < 10 ? 3 : -9));
    *y = yy + (*m <= 2);
}

/* PoP: w6_occurrences_d831fa_scheduled_instant @ cron/occurrences.py:scheduled_instant */
char *w6_occurrences_d831fa_scheduled_instant(const char *value) {
    if (!value) return NULL;
    const char *p = value;
    int Y, M, D, hh, mm, ss;
    if (!parse_uint(&p, 4, 4, &Y) || *p++ != '-') return NULL;
    if (!parse_uint(&p, 2, 2, &M) || *p++ != '-') return NULL;
    if (!parse_uint(&p, 2, 2, &D)) return NULL;
    if (Y < 1 || Y > 9999 || M < 1 || M > 12 || D < 1 || D > mdays(Y, M)) return NULL;
    /* separator: 'T' or ' ' (Python fromisoformat accepts both) */
    if (*p != 'T' && *p != ' ') return NULL;
    p++;
    if (!parse_uint(&p, 2, 2, &hh) || *p++ != ':') return NULL;
    if (!parse_uint(&p, 2, 2, &mm) || *p++ != ':') return NULL;
    if (!parse_uint(&p, 2, 2, &ss)) return NULL;
    if (hh > 23 || mm > 59 || ss > 59) return NULL;

    /* fractional seconds -> microseconds (first 6 digits, pad/truncate) */
    int micros = 0;
    if (*p == '.' || *p == ',') {
        p++;
        int n = 0;
        if (!isdigit((unsigned char)*p)) return NULL;
        while (isdigit((unsigned char)*p)) {
            if (n < 6) micros = micros * 10 + (*p - '0');
            n++; p++;
        }
        while (n < 6) { micros *= 10; n++; }
    }

    /* timezone: required for a non-None result */
    long long off = 0; /* seconds east of UTC */
    if (*p == 'Z' || *p == 'z') {
        p++;
    } else if (*p == '+' || *p == '-') {
        int sign = (*p == '-') ? -1 : 1;
        p++;
        int oh = 0, om = 0, os = 0;
        if (!parse_uint(&p, 2, 2, &oh)) return NULL;
        if (oh > 23) return NULL;
        if (*p == ':') { p++; if (!parse_uint(&p, 2, 2, &om)) return NULL; }
        else if (isdigit((unsigned char)*p)) { if (!parse_uint(&p, 2, 2, &om)) return NULL; }
        if (om > 59) return NULL;
        if (*p == ':') { p++; if (!parse_uint(&p, 2, 2, &os)) return NULL; }
        else if (isdigit((unsigned char)*p)) { if (!parse_uint(&p, 2, 2, &os)) return NULL; }
        if (os > 59) return NULL;
        off = sign * ((long long)oh * 3600 + (long long)om * 60 + os);
    } else {
        return NULL; /* naive: Python returns None */
    }
    if (*p != '\0') return NULL;

    /* convert local wall clock to UTC */
    long long secs = (long long)hh * 3600 + (long long)mm * 60 + ss - off;
    long long day_delta = 0;
    while (secs < 0) { secs += 86400; day_delta--; }
    while (secs >= 86400) { secs -= 86400; day_delta++; }
    long long days = days_from_civil(Y, M, D) + day_delta;
    long long uy; int um, ud;
    civil_from_days(days, &uy, &um, &ud);
    int uh = (int)(secs / 3600), umin = (int)((secs % 3600) / 60), usec = (int)(secs % 60);

    char *out = (char *)malloc(48);
    if (!out) return NULL;
    if (micros)
        snprintf(out, 48, "%04lld-%02d-%02dT%02d:%02d:%02d.%06d+00:00",
                 uy, um, ud, uh, umin, usec, micros);
    else
        snprintf(out, 48, "%04lld-%02d-%02dT%02d:%02d:%02d+00:00",
                 uy, um, ud, uh, umin, usec);
    return out;
}
