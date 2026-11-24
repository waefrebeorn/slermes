/*
 * undo_hint.c — C home for upstream Hermes agent/checkpoints/undo_hint.py.
 * STATUS: PORTED (gate w35_undohint_c4d2a1) — differential oracle green.
 * PoP: w35_undohint_c4d2a1_epoch @ agent/checkpoints/undo_hint.py:_epoch
 * PoP: w35_undohint_c4d2a1_named_files @ agent/checkpoints/undo_hint.py:format_undo_rollback_hint
 *
 * /undo only rewinds the conversation; with checkpoints on it names the files the
 * undone turns changed.  Two pieces of that are pure value -> value and are
 * ported here: the timestamp normaliser every decision beneath it runs on, and
 * the "first eight names, then a count" list shape.
 *
 * Seams, deliberately not ported: the git snapshot walk and the name-only diff
 * (_span_base_tree, _changed_files), and the i18n catalogue the final sentence
 * comes from -- this port answers the joined name list and the overflow count,
 * and the caller renders the sentence.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "noc_w35_undohint_c4d2a1.h"

/* Python's str.strip() default set, ASCII part. */
static int uh_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int uh_digits(const char *p, int n, int *out) {
    int i, v = 0;
    for (i = 0; i < n; i++) {
        if (p[i] < '0' || p[i] > '9') return 0;
        v = v * 10 + (p[i] - '0');
    }
    *out = v;
    return 1;
}

static int uh_leap(int y) {
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

static int uh_days_in_month(int y, int m) {
    static const int dim[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m < 1 || m > 12) return 0;
    if (m == 2 && uh_leap(y)) return 29;
    return dim[m - 1];
}

/* Days since 1970-01-01 from a proleptic Gregorian civil date. */
static long long uh_days_from_civil(int y, int m, int d) {
    long long era;
    unsigned yoe, doy, doe;
    y -= (m <= 2);
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (unsigned)(y - era * 400);
    doy = (153u * (unsigned)(m + (m > 2 ? -3 : 9)) + 2u) / 5u + (unsigned)(d - 1);
    doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097LL + (long long)doe - 719468LL;
}

/* datetime.fromisoformat() for the shapes a stamp takes, else 0.
 *
 * Accepted: YYYY-MM-DD, YYYYMMDD, each optionally followed by 'T' or ' ' and
 * HH:MM[:SS[.frac]] and an optional 'Z' or +-HH:MM[:SS[.frac]].  A naive stamp is
 * local time (mktime), an aware one is arithmetic, matching .timestamp().
 */
static int uh_parse_iso(const char *s, double *out) {
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, sec = 0;
    int off = 0, has_off = 0, i = 0;
    double frac = 0.0;
    long long base;
    const char *p = s;

    if (uh_digits(p, 4, &y) && p[4] == '-' && uh_digits(p + 5, 2, &mo) && p[7] == '-' &&
        uh_digits(p + 8, 2, &d)) {
        i = 10;
    } else if (uh_digits(p, 8, &y)) {
        /* basic form: the trailing digits carry month and day */
        mo = (y % 10000) / 100;
        d = y % 100;
        y /= 10000;
        i = 8;
    } else {
        return 0;
    }
    if (mo < 1 || d < 1 || d > uh_days_in_month(y, mo)) return 0;
    if (p[i]) {
        if (p[i] == 'T' || p[i] == ' ') i++;
        else return 0;
        if (!uh_digits(p + i, 2, &h) || p[i + 2] != ':') return 0;
        if (!uh_digits(p + i + 3, 2, &mi)) return 0;
        i += 5;
        if (p[i] == ':') {
            if (!uh_digits(p + i + 1, 2, &sec)) return 0;
            i += 3;
            if (p[i] == '.') {
                double scale = 0.1;
                i++;
                if (p[i] < '0' || p[i] > '9') return 0;
                while (p[i] >= '0' && p[i] <= '9') { frac += (p[i] - '0') * scale; scale /= 10.0; i++; }
            }
        }
        /* ISO 8601 allows 24:00:00 as end-of-day and CPython's parser accepts it,
         * rolling into the next day; 24:00:01 and friends are rejected. */
        if (h == 24) {
            if (mi || sec || frac > 0.0) return 0;
        } else if (h > 23) {
            return 0;
        }
        if (mi > 59 || sec > 59) return 0;
    }

    if (p[i] == 'Z') {
        has_off = 1;
        off = 0;
        i++;
    } else if (p[i] == '+' || p[i] == '-') {
        int sign = p[i] == '-' ? -1 : 1, oh = 0, om = 0, os = 0;
        i++;
        if (!uh_digits(p + i, 2, &oh) || p[i + 2] != ':') return 0;
        if (!uh_digits(p + i + 3, 2, &om)) return 0;
        i += 5;
        if (p[i] == ':') {
            if (!uh_digits(p + i + 1, 2, &os)) return 0;
            i += 3;
        }
        if (oh > 23 || om > 59 || os > 59) return 0;
        has_off = 1;
        off = sign * (oh * 3600 + om * 60 + os);
    }
    if (p[i]) return 0; /* trailing junk: fromisoformat rejects it */

    /* h == 24 already carries the next day through the hour term */
    base = uh_days_from_civil(y, mo, d) * 86400LL + (long long)h * 3600 + (long long)mi * 60 + sec;
    if (has_off) {
        *out = (double)(base - off) + frac;
    } else {
        /* naive: .timestamp() reads it as local time */
        struct tm tmv;
        time_t local;
        memset(&tmv, 0, sizeof tmv);
        tmv.tm_year = y - 1900;
        tmv.tm_mon = mo - 1;
        tmv.tm_mday = d;
        tmv.tm_hour = h;
        tmv.tm_min = mi;
        tmv.tm_sec = sec;
        tmv.tm_isdst = -1;
        local = mktime(&tmv);
        if (local == (time_t)-1) return 0;
        *out = (double)local + frac;
    }
    return 1;
}

json_t *w35_undohint_c4d2a1_epoch(const json_t *value) {
    if (!value) return json_null();
    if (value->type == JSON_BOOL) return json_null(); /* True is not 1 second after 1970 */
    if (value->type == JSON_NUMBER) {
        double v = value->num_val;
        return (isfinite(v) && v > 0) ? json_number(v) : json_null();
    }
    if (value->type == JSON_STRING) {
        const char *s = value->str_val ? value->str_val : "";
        size_t a = 0, b = strlen(s);
        char *trimmed;
        double out = 0;
        int ok;
        while (a < b && uh_is_space(s[a])) a++;
        while (b > a && uh_is_space(s[b - 1])) b--;
        if (a == b) return json_null();
        trimmed = (char *)malloc(b - a + 1);
        if (!trimmed) return json_null();
        memcpy(trimmed, s + a, b - a);
        trimmed[b - a] = '\0';
        ok = uh_parse_iso(trimmed, &out);
        free(trimmed);
        return ok ? json_number(out) : json_null();
    }
    return json_null();
}

json_t *w35_undohint_c4d2a1_named_files(const json_t *files) {
    json_t *out;
    size_t n, take, i, len = 0;
    char *joined, *q;
    if (!files || files->type != JSON_ARRAY) return json_null();
    n = json_len(files);
    take = n < 8 ? n : 8;
    for (i = 0; i < take; i++) {
        json_t *v = json_get(files, i);
        if (!v || v->type != JSON_STRING) return json_null(); /* upstream: TypeError */
        len += strlen(v->str_val ? v->str_val : "");
        if (i + 1 < take) len += 2; /* ", " */
    }
    joined = (char *)malloc(len + 1);
    if (!joined) return json_null();
    q = joined;
    for (i = 0; i < take; i++) {
        json_t *v = json_get(files, i);
        const char *s = v->str_val ? v->str_val : "";
        size_t sl = strlen(s);
        memcpy(q, s, sl);
        q += sl;
        if (i + 1 < take) { *q++ = ','; *q++ = ' '; }
    }
    *q = '\0';
    out = json_object();
    json_object_set(out, "named", json_string(joined));
    json_object_set(out, "more", json_number((double)(n > take ? n - take : 0)));
    free(joined);
    return out;
}
