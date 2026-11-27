/*
 * datetime.c — C datetime library (J05: Python datetime port).
 *
 * Implements ISO 8601 parsing/formatting, relative time descriptions,
 * date math, and calendar-day queries built on time_t + localtime_r/gmtime_r.
 */

#include "datetime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/* Format time_t to ISO 8601 using local timezone.
 * Returns buf on success, NULL on error. */
static char *ts_to_iso(time_t ts, char *buf, size_t cap) {
    struct tm result;
    if (!localtime_r(&ts, &result))
        return NULL;
    if (strftime(buf, cap, "%Y-%m-%dT%H:%M:%S", &result) == 0)
        return NULL;
    return buf;
}

/* Format time_t to ISO 8601 using UTC. */
static char *ts_to_iso_utc(time_t ts, char *buf, size_t cap) {
    struct tm result;
    if (!gmtime_r(&ts, &result))
        return NULL;
    if (strftime(buf, cap, "%Y-%m-%dT%H:%M:%SZ", &result) == 0)
        return NULL;
    return buf;
}

/* Flatten struct tm to calendar day start (midnight). */
static time_t day_start(time_t ts) {
    struct tm result;
    if (!localtime_r(&ts, &result))
        return ts;
    result.tm_sec = 0;
    result.tm_min = 0;
    result.tm_hour = 0;
    return mktime(&result);
}

/* ================================================================
 *  Constructors
 * ================================================================ */

char *datetime_now(void) {
    return datetime_from_time_t(time(NULL));
}

char *datetime_from_time_t(time_t ts) {
    char buf[DATETIME_ISO8601_LEN];
    if (!ts_to_iso(ts, buf, sizeof(buf)))
        return NULL;
    return strdup(buf);
}

char *datetime_from_time_t_utc(time_t ts) {
    char buf[DATETIME_ISO8601_LEN];
    if (!ts_to_iso_utc(ts, buf, sizeof(buf)))
        return NULL;
    return strdup(buf);
}

time_t datetime_parse_iso8601(const char *str) {
    if (!str || !*str) return (time_t)-1;

    struct tm tm_buf;
    memset(&tm_buf, 0, sizeof(tm_buf));

    /* Try full ISO 8601: 2026-05-22T14:30:00 or 2026-05-22T14:30:00Z
     * Also handles: 2026-05-22 14:30:00 (space separator) */
    int n = 0;
    char t_sep = 0;
    int scanned = sscanf(str, "%d-%d-%d%c%d:%d:%d%n",
        &tm_buf.tm_year, &tm_buf.tm_mon, &tm_buf.tm_mday,
        &t_sep, &tm_buf.tm_hour, &tm_buf.tm_min, &tm_buf.tm_sec, &n);

    if (scanned >= 7 && (t_sep == 'T' || t_sep == ' ') && n > 0) {
        tm_buf.tm_year -= 1900;  /* struct tm: years since 1900 */
        tm_buf.tm_mon -= 1;      /* struct tm: 0-based month */
        tm_buf.tm_isdst = -1;    /* auto-detect DST */

        /* Skip fractional seconds (RFC 3339: .fraction after seconds) */
        const char *remaining = str + n;
        if (*remaining == '.') {
            remaining++;
            while (*remaining >= '0' && *remaining <= '9') remaining++;
        }

        /* Check trailing Z for UTC; treat as local if absent */
        while (*remaining == ' ' || *remaining == '\t') remaining++;
        if (*remaining == 'Z' || *remaining == 'z') {
            /* Parse as UTC — use timegm on systems that have it */
#ifdef _GNU_SOURCE
            return timegm(&tm_buf);
#else
            /* Fallback: set TZ=UTC, mktime, restore */
            char *tz = getenv("TZ");
            setenv("TZ", "UTC", 1);
            tzset();
            time_t result = mktime(&tm_buf);
            if (tz)
                setenv("TZ", tz, 1);
            else
                unsetenv("TZ");
            tzset();
            return result;
#endif
        }

        /* Also handle +HH:MM / -HH:MM timezone offset after time */
        if (*remaining == '+' || *remaining == '-') {
            int tz_h = 0, tz_m = 0;
            if (sscanf(remaining, "%d:%d", &tz_h, &tz_m) >= 1) {
                int offset_secs = tz_h * 3600 + tz_m * 60;
                if (*remaining == '-') offset_secs = -offset_secs;
                /* Parse as UTC then offset */
#ifdef _GNU_SOURCE
                time_t utc = timegm(&tm_buf);
#else
                char *tz = getenv("TZ");
                setenv("TZ", "UTC", 1);
                tzset();
                time_t utc = mktime(&tm_buf);
                if (tz)
                    setenv("TZ", tz, 1);
                else
                    unsetenv("TZ");
                tzset();
#endif
                return utc - offset_secs;
            }
        }

        return mktime(&tm_buf);
    }

    /* Try date-only: 2026-05-22 */
    memset(&tm_buf, 0, sizeof(tm_buf));
    if (sscanf(str, "%d-%d-%d", &tm_buf.tm_year, &tm_buf.tm_mon, &tm_buf.tm_mday) >= 3) {
        tm_buf.tm_year -= 1900;
        tm_buf.tm_mon -= 1;
        tm_buf.tm_isdst = -1;
        time_t result = mktime(&tm_buf);
        /* Only accept if the full date string was consumed */
        char check[32];
        snprintf(check, sizeof(check), "%04d-%02d-%02d",
                 tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday);
        if (strncmp(str, check, strlen(check)) == 0)
            return result;
    }

    return (time_t)-1; /* parse failed */
}

time_t datetime_parse_rfc3339(const char *str) {
    return datetime_parse_iso8601(str);
}

time_t datetime_now_ts(void) {
    return time(NULL);
}

/* ================================================================
 *  Formatting
 * ================================================================ */

char *datetime_format(time_t ts, const char *fmt) {
    if (!fmt) return NULL;
    struct tm result;
    if (!localtime_r(&ts, &result))
        return NULL;
    char buf[256];
    if (strftime(buf, sizeof(buf), fmt, &result) == 0)
        return NULL;
    return strdup(buf);
}

char *datetime_format_utc(time_t ts, const char *fmt) {
    if (!fmt) return NULL;
    struct tm result;
    if (!gmtime_r(&ts, &result))
        return NULL;
    char buf[256];
    if (strftime(buf, sizeof(buf), fmt, &result) == 0)
        return NULL;
    return strdup(buf);
}

/* ─── Timezone ─────────────────────────────────────────── */


/* ================================================================
 *  Named-zone offsets that do not depend on the CRT's TZ support
 * ================================================================
 *
 * Measured on Windows 11 / mingw64 / msvcrt (tests/win_semantics/probe.c):
 *   TZ=America/New_York  -> offset -3600  (EDT is -14400)
 *   TZ=UTC               -> local hour 9  (UTC hour was 14)
 * i.e. _tzset() ignores the TZ variable here ENTIRELY, so every
 * setenv("TZ",name)+tzset()+localtime()/mktime() sequence silently uses the
 * machine zone. datetime_tz_offset()/datetime_format_tz() were built on exactly
 * that idiom, so a named zone rendered the machine's wall clock.
 *
 * The table below carries the standard offset plus one of four DST rules. It is
 * deliberately small: it covers the zones this program offers (the cron timezone
 * list and the common aliases). An unknown zone still falls back to the old TZ
 * path, which is correct on POSIX and no worse than before on Windows.
 */
typedef enum { TZR_NONE = 0, TZR_US, TZR_EU, TZR_AU } tz_rule_t;

typedef struct {
    const char *name;
    int std_off;   /* seconds east of UTC */
    int dst_off;
    tz_rule_t rule;
} tz_entry_t;

static const tz_entry_t WC_TZ[] = {
    {"UTC", 0, 0, TZR_NONE},
    {"Etc/UTC", 0, 0, TZR_NONE},
    {"GMT", 0, 0, TZR_NONE},
    {"America/New_York", -18000, -14400, TZR_US},
    {"US/Eastern", -18000, -14400, TZR_US},
    {"America/Chicago", -21600, -18000, TZR_US},
    {"US/Central", -21600, -18000, TZR_US},
    {"America/Denver", -25200, -21600, TZR_US},
    {"US/Mountain", -25200, -21600, TZR_US},
    {"America/Los_Angeles", -28800, -25200, TZR_US},
    {"US/Pacific", -28800, -25200, TZR_US},
    {"America/Phoenix", -25200, -25200, TZR_NONE},
    {"Europe/London", 0, 3600, TZR_EU},
    {"Europe/Berlin", 3600, 7200, TZR_EU},
    {"Europe/Paris", 3600, 7200, TZR_EU},
    {"Europe/Madrid", 3600, 7200, TZR_EU},
    {"Europe/Rome", 3600, 7200, TZR_EU},
    {"Asia/Kolkata", 19800, 19800, TZR_NONE},
    {"Asia/Hong_Kong", 28800, 28800, TZR_NONE},
    {"Asia/Shanghai", 28800, 28800, TZR_NONE},
    {"Asia/Singapore", 28800, 28800, TZR_NONE},
    {"Asia/Tokyo", 32400, 32400, TZR_NONE},
    {"Asia/Dubai", 14400, 14400, TZR_NONE},
    {"Australia/Sydney", 36000, 39600, TZR_AU},
    {"Australia/Melbourne", 36000, 39600, TZR_AU},
    {"America/Sao_Paulo", -10800, -10800, TZR_NONE},
};

/* Days since 1970-01-01 for a civil date (Howard Hinnant's algorithm). */
static long days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097L + (long)doe - 719468L;
}

/* 0 = Sunday. */
static int weekday_of(int y, int m, int d) {
    long days = days_from_civil(y, m, d);
    return (int)((days % 7 + 11) % 7);  /* 1970-01-01 was a Thursday */
}

/* Day-of-month of the nth (1-based) Sunday, or the LAST Sunday when nth == 0. */
static int nth_sunday(int y, int m, int nth) {
    int first_wd = weekday_of(y, m, 1);      /* 0 = Sun */
    int first_sun = 1 + ((7 - first_wd) % 7);
    if (nth == 0) {
        static const int mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        int last = mdays[m - 1];
        if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) last = 29;
        while (first_sun + 7 <= last) first_sun += 7;
        return first_sun;
    }
    return first_sun + 7 * (nth - 1);
}

static long utc_of(int y, int m, int d, int hh) {
    return days_from_civil(y, m, d) * 86400L + hh * 3600L;
}

/* UTC instant -> seconds east of UTC for this zone, DST included. */
static int wc_tz_offset_utc(const tz_entry_t *e, time_t t) {
    if (e->rule == TZR_NONE || e->std_off == e->dst_off)
        return e->std_off;

    /* Civil year of the instant in standard time decides the rule's dates. */
    time_t std_local = t + e->std_off;
    struct tm lt;
    if (!gmtime_r(&std_local, &lt))
        return e->std_off;
    int y = lt.tm_year + 1900;

    if (e->rule == TZR_US) {
        /* 2nd Sunday March 02:00 std -> 1st Sunday November 02:00 dst (both local). */
        long start = utc_of(y, 3, nth_sunday(y, 3, 2), 2) - e->std_off;
        long end   = utc_of(y, 11, nth_sunday(y, 11, 1), 2) - e->dst_off;
        return ((long)t >= start && (long)t < end) ? e->dst_off : e->std_off;
    }
    if (e->rule == TZR_EU) {
        /* Last Sunday March/October at 01:00 UTC. */
        long start = utc_of(y, 3, nth_sunday(y, 3, 0), 1);
        long end   = utc_of(y, 10, nth_sunday(y, 10, 0), 1);
        return ((long)t >= start && (long)t < end) ? e->dst_off : e->std_off;
    }
    /* TZR_AU: first Sunday October 02:00 std -> first Sunday April 03:00 dst,
     * i.e. the DST window wraps the new year. */
    {
        long start = utc_of(y, 10, nth_sunday(y, 10, 1), 2) - e->std_off;
        long end   = utc_of(y, 4, nth_sunday(y, 4, 1), 3) - e->dst_off;
        return ((long)t >= start || (long)t < end) ? e->dst_off : e->std_off;
    }
}

static const tz_entry_t *wc_tz_lookup(const char *name) {
    if (!name || !*name)
        return NULL;
    for (size_t i = 0; i < sizeof(WC_TZ) / sizeof(WC_TZ[0]); i++) {
        if (strcmp(WC_TZ[i].name, name) == 0)
            return &WC_TZ[i];
    }
    return NULL;
}

int datetime_localtime_offset(void) {
    time_t now = time(NULL);
    struct tm tm_utc;
    if (!gmtime_r(&now, &tm_utc)) return 0;
    tm_utc.tm_isdst = -1;
    time_t as_utc = mktime(&tm_utc);
    return (int)difftime(as_utc, now);
}

int datetime_tz_offset(const char *tz_name) {
    if (!tz_name) return 0;

    /* Named zones are resolved from the table first: on Windows the CRT ignores
     * TZ, so the old path returned the machine offset for every zone. */
    const tz_entry_t *e = wc_tz_lookup(tz_name);
    if (e)
        return wc_tz_offset_utc(e, time(NULL));

    time_t now = time(NULL);
    struct tm tm_utc;
    if (!gmtime_r(&now, &tm_utc)) return 0;
    tm_utc.tm_isdst = -1;

    char *saved_tz = getenv("TZ");
    char *saved_copy = saved_tz ? strdup(saved_tz) : NULL;
    setenv("TZ", tz_name, 1);
    tzset();
    time_t    t_tz = mktime(&tm_utc);
    if (saved_copy) { setenv("TZ", saved_copy, 1); } else { unsetenv("TZ"); }
    tzset();
    free(saved_copy);
    return (int)difftime(now, t_tz);
}

char *datetime_format_tz(time_t ts, const char *tz_name, const char *fmt) {
    if (!tz_name || !fmt) return NULL;

    /* Table path: shift the instant and format as UTC. Independent of the CRT's
     * TZ support, which is broken on Windows (measured: TZ=UTC ignored). */
    const tz_entry_t *e = wc_tz_lookup(tz_name);
    if (e) {
        int off = wc_tz_offset_utc(e, ts);
        time_t shifted = ts + off;
        struct tm result;
        char buf[256];
        if (!gmtime_r(&shifted, &result)) return NULL;
        result.tm_isdst = (off != e->std_off);
        if (strftime(buf, sizeof(buf), fmt, &result) == 0) return NULL;
        return strdup(buf);
    }

    char *saved_tz = getenv("TZ");
    char *saved_copy = saved_tz ? strdup(saved_tz) : NULL;
    setenv("TZ", tz_name, 1);
    tzset();
    struct tm result;
    char buf[256];
    char *ret = NULL;
    if (localtime_r(&ts, &result) && strftime(buf, sizeof(buf), fmt, &result) > 0)
        ret = strdup(buf);
    if (saved_copy) { setenv("TZ", saved_copy, 1); } else { unsetenv("TZ"); }
    tzset();
    free(saved_copy);
    return ret;
}

/* ================================================================
 *  Relative time
 * ================================================================ */

char *datetime_describe(time_t ts) {
    double secs = difftime(datetime_now_ts(), ts);

    if (secs < 0) {
        secs = -secs;
        if (secs < 60) return strdup("just now");
        if (secs < 3600) {
            int mins = (int)(secs / 60);
            char buf[64];
            snprintf(buf, sizeof(buf), "in %d minute%s", mins, mins == 1 ? "" : "s");
            return strdup(buf);
        }
        if (secs < 86400) {
            int hrs = (int)(secs / 3600);
            char buf[64];
            snprintf(buf, sizeof(buf), "in %d hour%s", hrs, hrs == 1 ? "" : "s");
            return strdup(buf);
        }
        int days = (int)(secs / 86400);
        char buf[64];
        snprintf(buf, sizeof(buf), "in %d day%s", days, days == 1 ? "" : "s");
        return strdup(buf);
    }

    if (secs < 5)   return strdup("just now");
    if (secs < 60)  return strdup("less than a minute ago");
    if (secs < 120) return strdup("1 minute ago");
    if (secs < 3600) {
        int mins = (int)(secs / 60);
        char buf[64];
        snprintf(buf, sizeof(buf), "%d minutes ago", mins);
        return strdup(buf);
    }
    if (secs < 7200) return strdup("1 hour ago");
    if (secs < 86400) {
        int hrs = (int)(secs / 3600);
        char buf[64];
        snprintf(buf, sizeof(buf), "%d hours ago", hrs);
        return strdup(buf);
    }
    if (secs < 172800) return strdup("yesterday");
    if (secs < 604800) { /* 7 days */
        int days = (int)(secs / 86400);
        char buf[64];
        snprintf(buf, sizeof(buf), "%d days ago", days);
        return strdup(buf);
    }

    /* Older than 7 days: return date string */
    return datetime_from_time_t(ts);
}

double datetime_age_seconds(time_t ts) {
    return difftime(datetime_now_ts(), ts);
}

double datetime_age_minutes(time_t ts) {
    return datetime_age_seconds(ts) / 60.0;
}

double datetime_age_hours(time_t ts) {
    return datetime_age_seconds(ts) / 3600.0;
}

double datetime_age_days(time_t ts) {
    return datetime_age_seconds(ts) / 86400.0;
}

/* ================================================================
 *  Date math
 * ================================================================ */

time_t datetime_add_days(time_t ts, int days) {
    struct tm result;
    if (!localtime_r(&ts, &result))
        return ts;
    result.tm_mday += days;
    result.tm_isdst = -1;
    return mktime(&result);
}

time_t datetime_add_hours(time_t ts, int hours) {
    return ts + (time_t)hours * 3600;
}

time_t datetime_add_seconds(time_t ts, int secs) {
    return ts + (time_t)secs;
}

bool datetime_is_expired(time_t ts, int ttl_seconds) {
    return difftime(datetime_now_ts(), ts) >= (double)ttl_seconds;
}

/* ================================================================
 *  Queries
 * ================================================================ */

bool datetime_is_today(time_t ts) {
    return datetime_same_day(ts, datetime_now_ts());
}

bool datetime_is_yesterday(time_t ts) {
    time_t yesterday = datetime_add_days(datetime_now_ts(), -1);
    return datetime_same_day(ts, yesterday);
}

bool datetime_same_day(time_t a, time_t b) {
    return day_start(a) == day_start(b);
}

time_t datetime_day_start(time_t ts) {
    return day_start(ts);
}

/* ================================================================
 *  Utilities
 * ================================================================ */

char *datetime_buffer(time_t ts, char *buf, size_t cap) {
    if (!buf || cap < DATETIME_ISO8601_LEN)
        return NULL;
    if (!ts_to_iso(ts, buf, cap))
        return NULL;
    return buf;
}

/* ================================================================
 *  Duration Formatting
 * ================================================================ */

/* Format duration in seconds as human-friendly label.
 * Ported from Python gateway/platforms/signal_rate_limit._format_wait().
 * < 90s → "Xs", >= 90s → "Y min". */
char *datetime_format_duration(double seconds) {
    double s = seconds < 0.0 ? 0.0 : seconds;
    if (s < 90.0) {
        int secs = (int)(s + 0.5);  /* round to nearest */
        char *out = (char *)malloc(16);
        if (!out) return NULL;
        snprintf(out, 16, "%ds", secs);
        return out;
    }
    int mins = (int)(s / 60.0 + 0.5);
    if (mins < 1) mins = 1;
    char *out = (char *)malloc(16);
    if (!out) return NULL;
    snprintf(out, 16, "%d min", mins);
    return out;
}
