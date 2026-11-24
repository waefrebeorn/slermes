/*
 * conventions.c -- real C11 port of the stored-instant decoding rules of
 * upstream Hermes persistence/core/conventions.py.
 *
 * PoP: w22_convention_a3be4b_ensure_utc @ persistence/core/conventions.py:ensure_utc
 * PoP: w22_convention_a3be4b_stored_epoch @ persistence/core/conventions.py:stored_instant
 * PoP: w22_convention_a3be4b_visibility @ persistence/core/conventions.py:RowVisibility
 * PoP: w22_convention_a3be4b_limits @ persistence/core/conventions.py:_STORED_EPOCH_MIN
 *
 * The `logger.warning(...)` sink (which names the session so the corrupt row
 * can be found) and the caller's `datetime` objects are the seam: this file
 * holds the window check, the numeric-string decoding and the UTC
 * normalization that decide what a stored cell reads as.
 */
#include "hermes_json.h"
#include "noc_w22_convention_a3be4b.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CV5_EPOCH_FLOOR 0.0
#define CV5_EPOCH_CEIL  4200000000.0

/* --- civil <-> epoch days (proleptic Gregorian, no libc time/TZ) ---------- */

static long long cv5_days_from_civil(int y, int m, int d) {
    long long era, yoe, doy, doe;
    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void cv5_civil_from_days(long long z, int *year, int *month, int *day) {
    long long era, yoe, doy, mp, yy;
    long long doe;
    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = z - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    yy = yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    *day = (int)(doy - (153 * mp + 2) / 5 + 1);
    *month = (int)(mp + (mp < 10 ? 3 : -9));
    *year = (int)(yy + (*month <= 2));
}

/* --- ISO-8601 (the shapes datetime.fromisoformat accepts) ------------------ */

typedef struct {
    int valid;
    int has_tz;
    int year, month, day, hour, minute, second, usec, offset;
} cv5_iso;

static int cv5_days_in_month(int y, int m) {
    static const int DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2) {
        int leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
        return leap ? 29 : 28;
    }
    return DAYS[m - 1];
}

static int cv5_num(const char **p, int width, int *out) {
    int i, value = 0;
    for (i = 0; i < width; i++) {
        char c = (*p)[i];
        if (c < '0' || c > '9') return 0;
        value = value * 10 + (c - '0');
    }
    *p += width;
    *out = value;
    return 1;
}

static int cv5_iso_parse(const char *text, cv5_iso *iso) {
    const char *p = text;
    int y = 0, mo = 0, d = 0, hh = 0, mi = 0, ss = 0, us = 0;

    memset(iso, 0, sizeof(*iso));
    if (!text) return 0;
    while (*p == ' ' || *p == '\t') p++;
    if (!cv5_num(&p, 4, &y)) return 0;
    if (*p++ != '-') return 0;
    if (!cv5_num(&p, 2, &mo)) return 0;
    if (*p++ != '-') return 0;
    if (!cv5_num(&p, 2, &d)) return 0;

    if (*p == 'T' || *p == 't' || *p == ' ') {
        p++;
        if (!cv5_num(&p, 2, &hh)) return 0;
        if (*p++ != ':') return 0;
        if (!cv5_num(&p, 2, &mi)) return 0;
        if (*p == ':') {
            p++;
            if (!cv5_num(&p, 2, &ss)) return 0;
        }
        if (*p == '.' || *p == ',') {
            int digits = 0;
            p++;
            while (*p >= '0' && *p <= '9') {
                if (digits < 6) us = us * 10 + (*p - '0');
                digits++;
                p++;
            }
            if (digits == 0) return 0;
            while (digits < 6) { us *= 10; digits++; }
        }
    } else if (*p) {
        return 0;
    }

    if (mo < 1 || mo > 12) return 0;
    if (d < 1 || d > cv5_days_in_month(y, mo)) return 0;
    if (hh > 23 || mi > 59 || ss > 59) return 0;

    if (*p == 'Z' || *p == 'z') {
        p++;
        iso->has_tz = 1;
    } else if (*p == '+' || *p == '-') {
        int sign = (*p == '-') ? -1 : 1;
        int oh = 0, om = 0;
        p++;
        if (!cv5_num(&p, 2, &oh)) return 0;
        if (*p == ':') {
            p++;
            if (!cv5_num(&p, 2, &om)) return 0;
        } else if (*p >= '0' && *p <= '9') {
            if (!cv5_num(&p, 2, &om)) return 0;
        }
        if (oh > 23 || om > 59) return 0;
        iso->has_tz = 1;
        iso->offset = sign * (oh * 3600 + om * 60);
    }
    while (*p == ' ') p++;
    if (*p) return 0;

    iso->valid = 1;
    iso->year = y;
    iso->month = mo;
    iso->day = d;
    iso->hour = hh;
    iso->minute = mi;
    iso->second = ss;
    iso->usec = us;
    return 1;
}

static long long cv5_local_seconds(const cv5_iso *iso) {
    long long days = cv5_days_from_civil(iso->year, iso->month, iso->day);
    return days * 86400 + iso->hour * 3600LL + iso->minute * 60LL + iso->second;
}

/* --- ensure_utc ---------------------------------------------------------- */

static json_t *cv5_reply(int ok, const char *value, const char *error) {
    json_t *out = json_object();
    json_set(out, "ok", json_bool(ok ? 1 : 0));
    json_set(out, "value", value ? json_string(value) : json_null());
    json_set(out, "error", error ? json_string(error) : json_null());
    return out;
}

json_t *w22_convention_a3be4b_ensure_utc(const char *iso_text) {
    cv5_iso iso;
    long long utc, days, rem;
    int y, mo, d, hh, mi, ss;
    char buf[64];

    if (!cv5_iso_parse(iso_text, &iso)) return cv5_reply(0, NULL, "parse");
    if (!iso.has_tz) return cv5_reply(0, NULL, "naive");

    utc = cv5_local_seconds(&iso) - iso.offset;
    days = utc / 86400;
    rem = utc % 86400;
    if (rem < 0) {
        rem += 86400;
        days -= 1;
    }
    cv5_civil_from_days(days, &y, &mo, &d);
    hh = (int)(rem / 3600);
    mi = (int)((rem % 3600) / 60);
    ss = (int)(rem % 60);
    if (iso.usec) {
        snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%06d+00:00",
                 y, mo, d, hh, mi, ss, iso.usec);
    } else {
        snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d+00:00",
                 y, mo, d, hh, mi, ss);
    }
    return cv5_reply(1, buf, NULL);
}

/* --- Python float() for a stored string cell ----------------------------- */

static int cv5_ieq_n(const char *a, const char *b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        if (tolower((unsigned char)a[i]) != b[i]) return 0;
    }
    return 1;
}

static int cv5_float(const char *text, double *out) {
    char clean[128];
    size_t i, n, w = 0;
    const char *p;
    int prev_digit = 0;
    char *end = NULL;

    if (!text) return 0;
    p = text;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
    n = strlen(p);
    while (n && (p[n - 1] == ' ' || p[n - 1] == '\t' || p[n - 1] == '\n' ||
                 p[n - 1] == '\r' || p[n - 1] == '\f' || p[n - 1] == '\v')) n--;
    if (n == 0 || n >= sizeof(clean)) return 0;

    /* inf / infinity / nan, with the optional sign */
    {
        size_t start = 0;
        if (p[0] == '+' || p[0] == '-') start = 1;
        if (n - start == 3 && cv5_ieq_n(p + start, "inf", 3)) {
            *out = (p[0] == '-') ? -INFINITY : INFINITY;
            return 1;
        }
        if (n - start == 8 && cv5_ieq_n(p + start, "infinity", 8)) {
            *out = (p[0] == '-') ? -INFINITY : INFINITY;
            return 1;
        }
        if (n - start == 3 && cv5_ieq_n(p + start, "nan", 3)) {
            *out = NAN;
            return 1;
        }
    }

    /* 0x10 is not a float in Python, but strtod would take it */
    {
        size_t start = 0;
        if (p[0] == '+' || p[0] == '-') start = 1;
        if (n - start >= 2 && p[start] == '0' && (p[start + 1] == 'x' || p[start + 1] == 'X')) return 0;
    }

    for (i = 0; i < n; i++) {
        char c = p[i];
        if (c == '_') {
            /* underscores are only legal between two digits */
            if (!prev_digit || i + 1 >= n || p[i + 1] < '0' || p[i + 1] > '9') return 0;
            prev_digit = 0;
            continue;
        }
        if (c >= '0' && c <= '9') prev_digit = 1;
        else prev_digit = 0;
        clean[w++] = c;
    }
    clean[w] = '\0';
    *out = strtod(clean, &end);
    if (!end || *end != '\0' || end == clean) return 0;
    return 1;
}

/* --- stored_instant ------------------------------------------------------ */

json_t *w22_convention_a3be4b_stored_epoch(const json_t *value) {
    double epoch = 0.0;

    if (!value || value->type == JSON_NULL) return json_null();          /* unset */
    if (value->type == JSON_STRING && value->str_val && value->str_val[0] == '\0') {
        return json_null();                                              /* "" is unset */
    }
    if (value->type == JSON_BOOL) {
        epoch = value->bool_val ? 1.0 : 0.0;
    } else if (value->type == JSON_NUMBER) {
        epoch = value->num_val;
    } else if (value->type == JSON_STRING) {
        if (!cv5_float(value->str_val, &epoch)) return json_null();
    } else if (value->type == JSON_OBJECT) {
        /* the caller's aware datetime, carried as {"__dt__": "<iso8601>"} */
        json_t *marker = json_obj_get(value, "__dt__");
        cv5_iso iso;
        if (!marker || marker->type != JSON_STRING) return json_null();
        if (!cv5_iso_parse(marker->str_val, &iso) || !iso.has_tz) return json_null();
        epoch = (double)(cv5_local_seconds(&iso) - iso.offset) + (double)iso.usec / 1e6;
    } else {
        return json_null();
    }
    /* `_MIN <= epoch <= _MAX` is False for nan too */
    if (!(epoch >= CV5_EPOCH_FLOOR && epoch <= CV5_EPOCH_CEIL)) return json_null();
    return json_number(epoch);
}

/* --- tables -------------------------------------------------------------- */

json_t *w22_convention_a3be4b_visibility(void) {
    static const char *const NAMES[] = {"ACTIVE", "REWOUND", "COMPACTION_ARCHIVED"};
    static const char *const VALUES[] = {"active", "rewound", "compaction_archived"};
    json_t *arr = json_array();
    int i;
    for (i = 0; i < 3; i++) {
        json_t *row = json_object();
        json_set(row, "name", json_string(NAMES[i]));
        json_set(row, "value", json_string(VALUES[i]));
        json_append(arr, row);
    }
    return arr;
}

json_t *w22_convention_a3be4b_limits(void) {
    json_t *out = json_object();
    json_set(out, "floor", json_number(CV5_EPOCH_FLOOR));
    json_set(out, "ceiling", json_number(CV5_EPOCH_CEIL));
    json_set(out, "untrusted_epoch", json_number(CV5_EPOCH_FLOOR));
    json_set(out, "untrusted_iso", json_string("1970-01-01T00:00:00+00:00"));
    return out;
}
