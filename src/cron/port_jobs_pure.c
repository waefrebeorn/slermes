/*
 * port_jobs_pure.c — C11 port of the pure helpers of cron/jobs.py
 *
 * Deterministic string/number/dict logic only: dispatch-lateness classification,
 * interval schedule shaping, recoverable-error detection, natural-language
 * schedule parsing (clock times / "every monday 9am"), toolset allowlist
 * normalization, in-place job record retirement/activation, one-shot error
 * copy, terminal-activation rejection, and fixed-offset instant math
 * (elapsed seconds, instant ordering, seconds-after, offset mismatch,
 * wall-clock future test).
 *
 * Upstream boundary: the LIVE module also carries croniter-backed cron
 * validation, file/sqlite/lock I/O and configured-clock reads; those stay in
 * the sibling cron ports. The instant helpers here accept ISO-8601 strings
 * with a FIXED numeric offset (or naive, where the LIVE predicate tolerates
 * it) — exactly the deterministic subset of datetime.astimezone(UTC) math.
 * Named-zone (ZoneInfo) inputs, croniter expressions and env-clock paths are
 * out of contract for this file.
 *
 * MIT License — Slermes Fork
 */

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>

/* cron/jobs.py:124 — ONESHOT_GRACE_SECONDS = 120 */
#define CP_ONESHOT_GRACE_SECONDS 120
/* cron/jobs.py:970 — _LATE_DISPATCH_TOLERANCE_SECONDS = 300 */
#define CP_LATE_DISPATCH_TOLERANCE_SECONDS 300

/* ── small shared helpers (ASCII subset of the Python str semantics) ── */

static bool cp_is_py_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* malloc'd copy with Python str.strip() whitespace removed from both ends. */
static char *cp_strip_dup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && cp_is_py_space((unsigned char)s[a])) a++;
    while (b > a && cp_is_py_space((unsigned char)s[b - 1])) b--;
    char *r = (char *)malloc(b - a + 1);
    if (!r) return NULL;
    memcpy(r, s + a, b - a);
    r[b - a] = '\0';
    return r;
}

static void cp_lower_ascii(char *s) {
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c >= 'A' && c <= 'Z') *s = (char)(c + 32);
    }
}

/* Python str(value) for the scalar JSON types the harness feeds these ports.
 * Containers are out of contract (Python repr is not libjson serialization);
 * they fall back to compact JSON so the seam never drops the value. */
static char *cp_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == (double)(long long)d) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%.15g", d);
        return strdup(buf);
    }
    char *ser = json_serialize(v);
    return ser ? ser : strdup("");
}

/* Python truthiness for the falsy set: None, False, 0, "", [], {}. */
static bool cp_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] != '\0';
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return false;
}

/* ── fixed-offset ISO-8601 instant math ── */

typedef struct {
    int64_t epoch_us;   /* microseconds since 1970-01-01T00:00:00 UTC */
    int offset_sec;     /* fixed UTC offset in seconds (0 when naive) */
    bool aware;
    bool ok;
} cp_dt;

/* Howard Hinnant's days_from_civil: days since 1970-01-01 for a civil date. */
static int64_t cp_days_from_civil(int64_t y, int m, int d) {
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static void cp_civil_from_days(int64_t z, int *y, int *m, int *d) {
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t yy = (int64_t)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned dd = doy - (153 * mp + 2) / 5 + 1;
    unsigned mm = mp + (mp < 10 ? 3 : -9);
    *y = (int)(yy + (mm <= 2));
    *m = (int)mm;
    *d = (int)dd;
}

static int64_t cp_floordiv(int64_t a, int64_t b) {
    int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
    return q;
}

static bool cp_read_digits(const char **p, int n, int *val) {
    int v = 0;
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)(*p)[i];
        if (c < '0' || c > '9') return false;
        v = v * 10 + (c - '0');
    }
    *p += n;
    *val = v;
    return true;
}

/* Parse "YYYY-MM-DDTHH:MM:SS[.ffffff][Z|±HH:MM]". Naive (no offset) is kept
 * as wall time with offset 0; aware values are normalized to UTC epoch µs. */
static cp_dt cp_parse_iso(const char *s) {
    cp_dt r = {0, 0, false, false};
    if (!s) return r;
    const char *p = s;
    int y, mo, d, h, mi, sec;
    if (!cp_read_digits(&p, 4, &y)) return r;
    if (*p++ != '-') return r;
    if (!cp_read_digits(&p, 2, &mo)) return r;
    if (*p++ != '-') return r;
    if (!cp_read_digits(&p, 2, &d)) return r;
    if (*p != 'T' && *p != 't') return r;
    p++;
    if (!cp_read_digits(&p, 2, &h)) return r;
    if (*p++ != ':') return r;
    if (!cp_read_digits(&p, 2, &mi)) return r;
    if (*p++ != ':') return r;
    if (!cp_read_digits(&p, 2, &sec)) return r;
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || sec > 59) return r;
    int us = 0;
    if (*p == '.') {
        p++;
        int digits = 0, frac = 0;
        while (*p >= '0' && *p <= '9') {
            if (digits < 6) frac = frac * 10 + (*p - '0');
            digits++;
            p++;
        }
        if (digits == 0) return r;
        while (digits < 6) { frac *= 10; digits++; }
        us = frac;
    }
    int off = 0;
    bool aware = false;
    if (*p == 'Z' || *p == 'z') {
        aware = true;
        p++;
    } else if (*p == '+' || *p == '-') {
        int sign = (*p == '-') ? -1 : 1;
        p++;
        int oh, om;
        if (!cp_read_digits(&p, 2, &oh)) return r;
        if (*p++ != ':') return r;
        if (!cp_read_digits(&p, 2, &om)) return r;
        if (oh > 23 || om > 59) return r;
        off = sign * (oh * 3600 + om * 60);
        aware = true;
    }
    if (*p != '\0') return r;
    int64_t wall_us = cp_days_from_civil(y, mo, d) * 86400000000LL
        + ((int64_t)h * 3600 + mi * 60 + sec) * 1000000LL + us;
    r.epoch_us = wall_us - (int64_t)off * 1000000LL;
    r.offset_sec = off;
    r.aware = aware;
    r.ok = true;
    return r;
}

/* Wall-clock microseconds (the naive datetime the LIVE predicates compare). */
static int64_t cp_wall_us(const cp_dt *dt) {
    return dt->epoch_us + (int64_t)dt->offset_sec * 1000000LL;
}

/* ── dispatch lateness / schedule shaping ── */

/* PoP: cronjobs_pure_classify_dispatch_lateness @ cron/jobs.py:_classify_dispatch_lateness */
const char *cronjobs_pure_classify_dispatch_lateness(double lateness_seconds, double grace_seconds) {
    if (lateness_seconds > grace_seconds) return "catch_up";
    if (lateness_seconds > CP_LATE_DISPATCH_TOLERANCE_SECONDS) return "late";
    return "on_time";
}

/* PoP: cronjobs_pure_interval_schedule @ cron/jobs.py:_interval_schedule */
json_t *cronjobs_pure_interval_schedule(long long minutes) {
    json_t *o = json_object();
    json_set(o, "kind", json_string("interval"));
    json_set(o, "minutes", json_number((double)minutes));
    char display[64];
    snprintf(display, sizeof display, "every %lldm", minutes);
    json_set(o, "display", json_string(display));
    return o;
}

/* PoP: cronjobs_pure_is_recoverable_error_job @ cron/jobs.py:_is_recoverable_error_job */
bool cronjobs_pure_is_recoverable_error_job(const json_t *job) {
    json_t *state = json_obj_get(job, "state");
    if (!state || state->type != JSON_STRING || strcmp(state->str_val, "error") != 0) return false;
    /* (job.get("schedule") or {}).get("kind") — falsy schedules read as {}.
     * A truthy non-dict schedule raises AttributeError upstream; out of
     * contract here, reported as not-recoverable. */
    json_t *sched = json_obj_get(job, "schedule");
    if (!cp_truthy(sched)) return false;
    if (sched->type != JSON_OBJECT) return false;
    json_t *kind = json_obj_get(sched, "kind");
    if (!kind || kind->type != JSON_STRING) return false;
    return strcmp(kind->str_val, "cron") == 0 || strcmp(kind->str_val, "interval") == 0;
}

/* ── natural-language schedule parsing ── */

/* PoP: cronjobs_pure_parse_clock_time @ cron/jobs.py:_parse_clock_time */
bool cronjobs_pure_parse_clock_time(const char *text, int *hour_out, int *minute_out) {
    if (hour_out) *hour_out = 0;
    if (minute_out) *minute_out = 0;
    char *stripped = cp_strip_dup(text);
    if (!stripped) return false;
    cp_lower_ascii(stripped);
    /* .replace(" ", "") — only literal spaces, other whitespace survives. */
    char *t = stripped;
    size_t w = 0;
    for (size_t i = 0; stripped[i]; i++) {
        if (stripped[i] != ' ') t[w++] = stripped[i];
    }
    t[w] = '\0';
    bool ok = false;
    int hour = 0, minute = 0;
    if (w == 0) goto done;
    if (strcmp(t, "noon") == 0 || strcmp(t, "midday") == 0) {
        hour = 12; minute = 0; ok = true; goto done;
    }
    if (strcmp(t, "midnight") == 0) {
        hour = 0; minute = 0; ok = true; goto done;
    }
    {
        /* ^(\d{1,2})(?::(\d{2}))?(am|pm)?$ with greedy-then-backtrack digits. */
        size_t ndig = 0;
        while (ndig < 2 && t[ndig] >= '0' && t[ndig] <= '9') ndig++;
        if (ndig == 0) goto done;
        for (size_t dl = ndig; dl >= 1; dl--) {
            const char *p = t + dl;
            int h = 0;
            for (size_t i = 0; i < dl; i++) h = h * 10 + (t[i] - '0');
            int m = 0;
            if (*p == ':') {
                p++;
                if (!(p[0] >= '0' && p[0] <= '9' && p[1] >= '0' && p[1] <= '9')) continue;
                m = (p[0] - '0') * 10 + (p[1] - '0');
                p += 2;
            }
            int mer = 0; /* 0 none, 1 am, 2 pm */
            if (p[0] == 'a' && p[1] == 'm' && p[2] == '\0') mer = 1;
            else if (p[0] == 'p' && p[1] == 'm' && p[2] == '\0') mer = 2;
            else if (p[0] != '\0') continue;
            if (mer) {
                if (h < 1 || h > 12) continue;
                h = h % 12 + (mer == 2 ? 12 : 0);
            }
            if (h > 23 || m > 59) continue;
            hour = h; minute = m; ok = true;
            break;
        }
    }
done:
    free(stripped);
    if (ok) {
        if (hour_out) *hour_out = hour;
        if (minute_out) *minute_out = minute;
    }
    return ok;
}

/* cron/jobs.py:695 _WEEKDAY_TO_CRON_DOW (0=Sunday … 6=Saturday). */
static const char *cp_weekday_dow(const char *tok) {
    static const struct { const char *name; const char *dow; } map[] = {
        {"sunday", "0"}, {"sun", "0"},
        {"monday", "1"}, {"mon", "1"},
        {"tuesday", "2"}, {"tue", "2"}, {"tues", "2"},
        {"wednesday", "3"}, {"wed", "3"}, {"weds", "3"},
        {"thursday", "4"}, {"thu", "4"}, {"thur", "4"}, {"thurs", "4"},
        {"friday", "5"}, {"fri", "5"},
        {"saturday", "6"}, {"sat", "6"},
    };
    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++) {
        if (strcmp(tok, map[i].name) == 0) return map[i].dow;
    }
    return NULL;
}

/* cron/jobs.py:706 _DAYSPEC_TO_CRON_DOW. */
static const char *cp_dayspec_dow(const char *tok) {
    static const struct { const char *name; const char *dow; } map[] = {
        {"day", "*"}, {"daily", "*"}, {"everyday", "*"},
        {"weekday", "1-5"}, {"weekdays", "1-5"},
        {"weekend", "0,6"}, {"weekends", "0,6"},
    };
    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++) {
        if (strcmp(tok, map[i].name) == 0) return map[i].dow;
    }
    return NULL;
}

/* PoP: cronjobs_pure_natural_every_to_cron @ cron/jobs.py:_natural_every_to_cron */
char *cronjobs_pure_natural_every_to_cron(const char *rest) {
    if (!rest) return NULL;
    char *work = strdup(rest);
    if (!work) return NULL;
    cp_lower_ascii(work);
    for (char *p = work; *p; p++) {
        if (*p == ',') *p = ' ';
    }
    /* .split() — whitespace runs, no empty tokens. */
    char **toks = NULL;
    size_t ntok = 0, cap = 0;
    char *save = NULL;
    for (char *tok = strtok_r(work, " \t\n\r\v\f", &save); tok;
         tok = strtok_r(NULL, " \t\n\r\v\f", &save)) {
        if (ntok == cap) {
            cap = cap ? cap * 2 : 16;
            char **nt = (char **)realloc(toks, cap * sizeof(char *));
            if (!nt) { free(toks); free(work); return NULL; }
            toks = nt;
        }
        toks[ntok++] = tok;
    }
    char *result = NULL;
    if (ntok == 0) goto done;
    const char *dow = cp_dayspec_dow(toks[0]);
    size_t idx = 1;
    char *days_joined = NULL;
    if (!dow) {
        const char *days[8];
        size_t ndays = 0;
        idx = ntok;
        for (size_t i = 0; i < ntok; i++) {
            if (strcmp(toks[i], "and") == 0) continue;
            const char *mapped = cp_weekday_dow(toks[i]);
            if (!mapped) { idx = i; break; }
            bool dup = false;
            for (size_t j = 0; j < ndays; j++) {
                if (strcmp(days[j], mapped) == 0) { dup = true; break; }
            }
            if (!dup && ndays < 8) days[ndays++] = mapped;
        }
        if (ndays == 0) goto done;
        size_t len = 0;
        for (size_t j = 0; j < ndays; j++) len += strlen(days[j]) + 1;
        days_joined = (char *)malloc(len + 1);
        if (!days_joined) goto done;
        days_joined[0] = '\0';
        for (size_t j = 0; j < ndays; j++) {
            if (j) strcat(days_joined, ",");
            strcat(days_joined, days[j]);
        }
        dow = days_joined;
    }
    {
        size_t tstart = idx;
        if (tstart < ntok && strcmp(toks[tstart], "at") == 0) tstart++;
        if (tstart >= ntok) goto done;
        size_t len = 0;
        for (size_t j = tstart; j < ntok; j++) len += strlen(toks[j]) + 1;
        char *time_text = (char *)malloc(len + 1);
        if (!time_text) goto done;
        time_text[0] = '\0';
        for (size_t j = tstart; j < ntok; j++) {
            if (j > tstart) strcat(time_text, " ");
            strcat(time_text, toks[j]);
        }
        int hour = 0, minute = 0;
        bool ok = cronjobs_pure_parse_clock_time(time_text, &hour, &minute);
        free(time_text);
        if (!ok) goto done;
        char buf[128];
        snprintf(buf, sizeof buf, "%d %d * * %s", minute, hour, dow);
        result = strdup(buf);
    }
done:
    free(days_joined);
    free(toks);
    free(work);
    return result;
}

/* ── job record normalization / mutation ── */

/* PoP: cronjobs_pure_normalize_enabled_toolsets @ cron/jobs.py:_normalize_enabled_toolsets */
json_t *cronjobs_pure_normalize_enabled_toolsets(const json_t *value, char **err_out) {
    if (err_out) *err_out = NULL;
    if (!value || value->type == JSON_NULL) return NULL;
    if (value->type != JSON_ARRAY) {
        if (err_out) *err_out = strdup("enabled_toolsets must be a list of toolset names (or [] for none).");
        return NULL;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < value->c.count; i++) {
        char *text = cp_pystr(value->c.items[i]);
        if (!text) continue;
        char *trimmed = cp_strip_dup(text);
        free(text);
        if (trimmed && trimmed[0] != '\0') json_append(out, json_string(trimmed));
        free(trimmed);
    }
    return out;
}

/* PoP: cronjobs_pure_complete_job_record @ cron/jobs.py:_complete_job_record */
void cronjobs_pure_complete_job_record(json_t *job) {
    if (!job || job->type != JSON_OBJECT) return;
    json_set(job, "enabled", json_bool(false));
    json_set(job, "state", json_string("completed"));
    json_set(job, "next_run_at", json_null());
}

/* PoP: cronjobs_pure_activate_job_record @ cron/jobs.py:_activate_job_record */
void cronjobs_pure_activate_job_record(json_t *job) {
    if (!job || job->type != JSON_OBJECT) return;
    json_set(job, "enabled", json_bool(true));
    json_set(job, "state", json_string("scheduled"));
    json_set(job, "paused_at", json_null());
    json_set(job, "paused_reason", json_null());
}

/* ── error copy / update guards ── */

/* PoP: cronjobs_pure_oneshot_past_grace_error @ cron/jobs.py:_oneshot_past_grace_error */
char *cronjobs_pure_oneshot_past_grace_error(const char *run_at) {
    const char *text = run_at ? run_at : "None";
    const char *fmt = "Requested one-shot time %s is more than %ds in the past and cannot be scheduled.";
    int n = snprintf(NULL, 0, fmt, text, CP_ONESHOT_GRACE_SECONDS);
    if (n < 0) return NULL;
    char *msg = (char *)malloc((size_t)n + 1);
    if (!msg) return NULL;
    snprintf(msg, (size_t)n + 1, fmt, text, CP_ONESHOT_GRACE_SECONDS);
    return msg;
}

static bool cp_state_is_terminal(const json_t *obj, const char *key) {
    json_t *v = json_obj_get(obj, key);
    if (!v || v->type != JSON_STRING) return false;
    return strcmp(v->str_val, "completed") == 0 || strcmp(v->str_val, "error") == 0;
}

/* PoP: cronjobs_pure_reject_terminal_activation @ cron/jobs.py:_reject_terminal_activation */
char *cronjobs_pure_reject_terminal_activation(const json_t *job, const json_t *updated, const char *job_id) {
    if (!cp_state_is_terminal(job, "state")) return NULL;
    if (cronjobs_pure_is_recoverable_error_job(job)) return NULL;
    json_t *enabled = json_obj_get(updated, "enabled");
    bool enabled_is_true = enabled && enabled->type == JSON_BOOL && enabled->bool_val;
    json_t *next_run = json_obj_get(updated, "next_run_at");
    bool next_run_set = next_run && next_run->type != JSON_NULL;
    if (cp_state_is_terminal(updated, "state") && !enabled_is_true && !next_run_set) return NULL;
    /* job.get('name', job_id): the default applies only when the key is absent;
     * a present null formats as "None" inside the f-string. */
    char *name = NULL;
    json_t *name_node = json_obj_get(job, "name");
    if (name_node) name = cp_pystr(name_node);
    else name = strdup(job_id ? job_id : "");
    if (!name) return NULL;
    const char *fmt = "Cannot activate terminal cron job '%s' through update_job; use cron resume --run-now or --at.";
    int n = snprintf(NULL, 0, fmt, name);
    char *msg = NULL;
    if (n >= 0) {
        msg = (char *)malloc((size_t)n + 1);
        if (msg) snprintf(msg, (size_t)n + 1, fmt, name);
    }
    free(name);
    return msg;
}

/* ── instant math (fixed-offset aware ISO inputs) ── */

/* PoP: cronjobs_pure_elapsed_seconds @ cron/jobs.py:_elapsed_seconds */
double cronjobs_pure_elapsed_seconds(const char *later, const char *earlier) {
    cp_dt a = cp_parse_iso(later);
    cp_dt b = cp_parse_iso(earlier);
    if (!a.ok || !b.ok) return NAN;
    int64_t diff = a.epoch_us - b.epoch_us;
    /* Reproduce timedelta normalization + total_seconds() float arithmetic:
     * (days*86400 + seconds) + microseconds/1e6, days floor-normalized. */
    int64_t days = cp_floordiv(diff, 86400000000LL);
    int64_t rem = diff - days * 86400000000LL;
    int64_t secs = rem / 1000000LL;
    int64_t us = rem % 1000000LL;
    return (double)(days * 86400 + secs) + (double)us / 1000000.0;
}

/* PoP: cronjobs_pure_instant_after @ cron/jobs.py:_instant_after */
bool cronjobs_pure_instant_after(const char *left, const char *right) {
    cp_dt a = cp_parse_iso(left);
    cp_dt b = cp_parse_iso(right);
    if (!a.ok || !b.ok) return false;
    return a.epoch_us > b.epoch_us;
}

/* PoP: cronjobs_pure_instant_at_or_before @ cron/jobs.py:_instant_at_or_before */
bool cronjobs_pure_instant_at_or_before(const char *left, const char *right) {
    cp_dt a = cp_parse_iso(left);
    cp_dt b = cp_parse_iso(right);
    if (!a.ok || !b.ok) return false;
    return a.epoch_us <= b.epoch_us;
}

/* PoP: cronjobs_pure_instant_before @ cron/jobs.py:_instant_before */
bool cronjobs_pure_instant_before(const char *left, const char *right) {
    cp_dt a = cp_parse_iso(left);
    cp_dt b = cp_parse_iso(right);
    if (!a.ok || !b.ok) return false;
    return a.epoch_us < b.epoch_us;
}

/* PoP: cronjobs_pure_seconds_after @ cron/jobs.py:_seconds_after */
char *cronjobs_pure_seconds_after(const char *dt_iso, double seconds) {
    cp_dt dt = cp_parse_iso(dt_iso);
    if (!dt.ok || !dt.aware) return NULL;
    /* timedelta(seconds=…) rounds to whole microseconds; the harness feeds
     * exactly-representable values, rint keeps the default half-even mode. */
    int64_t add_us = (int64_t)rint(seconds * 1000000.0);
    int64_t local_us = dt.epoch_us + add_us + (int64_t)dt.offset_sec * 1000000LL;
    int64_t days = cp_floordiv(local_us, 86400000000LL);
    int64_t rem = local_us - days * 86400000000LL;
    int y, mo, d;
    cp_civil_from_days(days, &y, &mo, &d);
    int h = (int)(rem / 3600000000LL);
    int mi = (int)((rem / 60000000LL) % 60);
    int sec = (int)((rem / 1000000LL) % 60);
    int us = (int)(rem % 1000000LL);
    int off = dt.offset_sec;
    char sign = off < 0 ? '-' : '+';
    int aoff = off < 0 ? -off : off;
    char buf[64];
    int n = snprintf(buf, sizeof buf, "%04d-%02d-%02dT%02d:%02d:%02d", y, mo, d, h, mi, sec);
    if (n < 0 || (size_t)n >= sizeof buf) return NULL;
    if (us != 0) {
        n += snprintf(buf + n, sizeof buf - (size_t)n, ".%06d", us);
        if (n < 0 || (size_t)n >= sizeof buf) return NULL;
    }
    snprintf(buf + n, sizeof buf - (size_t)n, "%c%02d:%02d", sign, aoff / 3600, (aoff % 3600) / 60);
    return strdup(buf);
}

/* PoP: cronjobs_pure_timezone_offset_mismatch @ cron/jobs.py:_timezone_offset_mismatch */
bool cronjobs_pure_timezone_offset_mismatch(const char *stored, const char *current) {
    cp_dt a = cp_parse_iso(stored);
    cp_dt b = cp_parse_iso(current);
    if (!a.ok || !b.ok) return false;
    if (!a.aware || !b.aware) return false;
    return a.offset_sec != b.offset_sec;
}

/* PoP: cronjobs_pure_stored_wall_clock_is_future @ cron/jobs.py:_stored_wall_clock_is_future */
bool cronjobs_pure_stored_wall_clock_is_future(const char *stored, const char *current) {
    cp_dt a = cp_parse_iso(stored);
    cp_dt b = cp_parse_iso(current);
    if (!a.ok || !b.ok) return false;
    return cp_wall_us(&a) > cp_wall_us(&b);
}
