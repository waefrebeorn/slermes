/*
 * idle_schedule.c — C port of upstream Hermes agent/idle_schedule.py.
 *
 * PoP: w6_idleschedul_b709d3_parse_iso_ok @ agent/idle_schedule.py:parse_iso
 * PoP: w6_idleschedul_b709d3_interval_due @ agent/idle_schedule.py:interval_due
 *
 * Only the pure, observable interval-gate logic is ported: parse_iso (as a
 * validity predicate) and interval_due (bool + in-place state seeding).
 * The cross-process file/PID claim helpers depend on OS state and are not
 * exercised by this differential gate.
 */
#include "noc_w6_idleschedul_b709d3.h"
#include "hermes_json.h"
#include <ctype.h>
#include <string.h>

typedef struct { long long secs; int aware; } isoT;

/* Howard Hinnant's days-from-civil: days since 1970-01-01 for a proleptic
 * Gregorian (y, m, d). */
static long long days_from_civil(long long y, unsigned m, unsigned d) {
    y -= (m <= 2);
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}

/* Parse the ISO-8601 subset produced by datetime.isoformat()/fromisoformat():
 *   YYYY-MM-DD[ T HH:MM:SS[.ffffff][ Z | +HH:MM | -HH:MM]]
 * Returns 1 on success. Naive timestamps are treated as UTC seconds. */
static int parse_iso(const char *s, isoT *out) {
    if (!s) return 0;
    const char *p = s;
    int Y = 0, M = 0, D = 0, h = 0, mi = 0, se = 0, n;
    #define DIGITS(dst, cnt) do { (dst)=0; n=0; while (n<(cnt) && *p>='0' && *p<='9') { (dst)=(dst)*10+(*p-'0'); p++; n++; } if (n!=(cnt)) return 0; } while (0)
    DIGITS(Y, 4); if (*p != '-') return 0; p++;
    DIGITS(M, 2); if (*p != '-') return 0; p++;
    DIGITS(D, 2);
    if (M < 1 || M > 12 || D < 1 || D > 31) return 0;
    long long secs = days_from_civil(Y, (unsigned)M, (unsigned)D) * 86400LL;
    if (*p == '\0') { out->secs = secs; out->aware = 0; return 1; }
    if (*p == 'T' || *p == 't' || *p == ' ') p++; else return 0;
    DIGITS(h, 2); if (*p != ':') return 0; p++;
    DIGITS(mi, 2); if (*p != ':') return 0; p++;
    DIGITS(se, 2);
    if (h > 23 || mi > 59 || se > 60) return 0;
    secs += (long long)h * 3600 + (long long)mi * 60 + se;
    /* optional fractional seconds (ignored for whole-second fixtures) */
    if (*p == '.') {
        p++; int got = 0;
        while (*p >= '0' && *p <= '9') { p++; got++; }
        if (!got) return 0;
    }
    int aware = 0; long long off = 0;
    if (*p == 'Z' || *p == 'z') { p++; aware = 1; }
    else if (*p == '+' || *p == '-') {
        int sign = (*p == '-') ? -1 : 1; p++;
        int oh = 0, om = 0;
        DIGITS(oh, 2);
        if (*p == ':') p++; else return 0;
        DIGITS(om, 2);
        off = sign * ((long long)oh * 3600 + (long long)om * 60);
        aware = 1; secs -= off;
    }
    #undef DIGITS
    if (*p != '\0') return 0;
    out->secs = secs; out->aware = aware;
    return 1;
}

int w6_idleschedul_b709d3_parse_iso_ok(const char *ts) {
    if (!ts || !*ts) return 0;          /* parse_iso returns None for falsy ts */
    isoT t; return parse_iso(ts, &t) ? 1 : 0;
}

json_t *w6_idleschedul_b709d3_interval_due(const json_t *state, double interval_hours,
                                           const char *now_iso, const char *seed_summary) {
    json_t *st = state ? json_copy(state) : json_object();
    json_t *lrv = json_obj_get(st, "last_run_at");
    const char *lasts = (lrv && lrv->type == JSON_STRING) ? lrv->str_val : NULL;
    isoT last;
    int have_last = lasts && parse_iso(lasts, &last);
    json_t *res = json_object();
    if (!have_last) {
        /* First observation seeds now (as now.isoformat()) and defers one interval. */
        json_set(st, "last_run_at", json_string(now_iso ? now_iso : ""));
        json_set(st, "last_run_summary", json_string(seed_summary ? seed_summary : ""));
        json_set(res, "due", json_bool(false));
    } else {
        isoT now;
        int due = 0;
        if (parse_iso(now_iso, &now)) {
            double delta = (double)(now.secs - last.secs);
            due = delta >= interval_hours * 3600.0;
        }
        json_set(res, "due", json_bool(due ? true : false));
    }
    json_set(res, "state", st);
    return res;
}
