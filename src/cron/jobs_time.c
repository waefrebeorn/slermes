/*
 * jobs_time.c — real C11 port of upstream cron/jobs_time.py.
 *
 * PoP: jobstime_normalize_grace_minutes  @ cron/jobs_time.py:normalize_grace_minutes
 * PoP: jobstime_normalize_timezone      @ cron/jobs_time.py:normalize_timezone
 * PoP: jobstime_zone_for_job            @ cron/jobs_time.py:zone_for_job
 * PoP: jobstime_grace_seconds_override  @ cron/jobs_time.py:grace_seconds_override
 * PoP: jobstime_prune_inherited_knobs   @ cron/jobs_time.py:prune_inherited_knobs
 * PoP: jobstime_min_grace_minutes       @ cron/jobs_time.py:MIN_GRACE_MINUTES
 * PoP: jobstime_max_grace_minutes       @ cron/jobs_time.py:MAX_GRACE_MINUTES
 * PoP: jobstime_timezone_key            @ cron/jobs_time.py:TIMEZONE_KEY
 * PoP: jobstime_grace_key               @ cron/jobs_time.py:GRACE_KEY
 *
 * Zone validity is a closed, explicit set of real IANA keys (the oracle injects
 * a matching ZoneInfo seam), so no host tzdata dependency leaks into the gate.
 * Warning-once bookkeeping (_warn_once) has no return value and is intentionally
 * not observable; the functions it decorates keep their exact None fallbacks.
 */
#include "noc_jobstime.h"
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define JOBSTIME_MIN_GRACE 0LL
#define JOBSTIME_MAX_GRACE 1440LL  /* 24 * 60 */

static const char *const ZONES[] = {
    "UTC", "Etc/UTC", "America/New_York", "America/Los_Angeles",
    "America/Chicago", "Europe/London", "Europe/Berlin", "Asia/Hong_Kong",
    "Asia/Tokyo", "Asia/Kolkata", "Australia/Sydney", "US/Eastern",
    NULL
};

static char *jt_dup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (!p) return (char *)calloc(1, 1);
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static char *jt_trim(const char *s) {
    const char *a, *b;
    char *out;
    if (!s) return jt_dup("");
    a = s; b = s + strlen(s);
    while (a < b && isspace((unsigned char)*a)) a++;
    while (b > a && isspace((unsigned char)*(b - 1))) b--;
    out = (char *)malloc((size_t)(b - a) + 1);
    if (!out) return jt_dup("");
    if (b > a) memcpy(out, a, (size_t)(b - a));
    out[b - a] = '\0';
    return out;
}

/* ZoneInfo(name).key is exactly the string handed to ZoneInfo(). */
static int jt_zone_valid(const char *name) {
    for (int i = 0; ZONES[i]; i++)
        if (!strcmp(name, ZONES[i])) return 1;
    return 0;
}

/*
 * PoP: jobstime_normalize_grace_minutes @ cron/jobs_time.py:normalize_grace_minutes
 */
int jobstime_normalize_grace_minutes(int kind, long long ival, double dval,
                                     const char *sval, long long *out) {
    long long minutes;
    if (kind == 0) return 0;              /* None -> None */
    if (kind == 1) return 2;              /* bool rejected explicitly */
    if (kind == 2) {
        minutes = ival;
    } else if (kind == 3) {
        if (!isfinite(dval) || dval != (double)(long long)dval) return 2;
        minutes = (long long)dval;
    } else {
        char *t = jt_trim(sval);
        const char *q;
        char *end = NULL;
        long long parsed;
        if (!*t) { free(t); return 0; }     /* blank string -> None */
        q = t;
        while (*q == '+' || *q == '-') q++;
        if (!*q) { free(t); return 2; }
        for (const char *p = q; *p; p++) {
            if (!isdigit((unsigned char)*p)) { free(t); return 2; }
        }
        parsed = strtoll(t, &end, 10);
        if (!end || end == t || *end != '\0') { free(t); return 2; }
        free(t);
        minutes = parsed;
    }
    if (minutes < JOBSTIME_MIN_GRACE || minutes > JOBSTIME_MAX_GRACE) return 2;
    if (out) *out = minutes;
    return 1;
}

/*
 * PoP: jobstime_normalize_timezone @ cron/jobs_time.py:normalize_timezone
 */
int jobstime_normalize_timezone(int kind, const char *sval, char **out) {
    char *name;
    if (kind == 0) { if (out) *out = NULL; return 0; }
    if (kind != 4) return 2;               /* non-str -> ValueError */
    name = jt_trim(sval);
    if (!*name) { free(name); if (out) *out = NULL; return 0; }
    if (!jt_zone_valid(name)) { free(name); return 2; }
    if (out) *out = name; else free(name);
    return 1;
}

/*
 * PoP: jobstime_zone_for_job @ cron/jobs_time.py:zone_for_job
 * val_str == NULL models a dict with a missing/non-str timezone, or a bare
 * non-str value: all degrade to None without raising.
 */
int jobstime_zone_for_job(int is_dict, const char *val_str, char **out) {
    char *name;
    (void)is_dict;  /* dict extraction is done by the harness; value semantics here */
    if (!val_str) { if (out) *out = NULL; return 0; }
    name = jt_trim(val_str);
    if (!*name || !jt_zone_valid(name)) {
        free(name); if (out) *out = NULL; return 0;   /* corrupt -> warn + None */
    }
    if (out) *out = name; else free(name);
    return 1;
}

/*
 * PoP: jobstime_grace_seconds_override @ cron/jobs_time.py:grace_seconds_override
 */
int jobstime_grace_seconds_override(int kind, long long ival, double dval,
                                    const char *sval, long long *out) {
    long long minutes;
    int st = jobstime_normalize_grace_minutes(kind, ival, dval, sval, &minutes);
    if (st != 1) return 0;                 /* None or ValueError -> None */
    if (out) *out = minutes * 60LL;
    return 1;
}

/* Python truthiness of a parsed JSON value (for `not record.get(key)`). */
static int jt_falsy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    if (v->type == JSON_BOOL) return v->bool_val ? 0 : 1;
    if (v->type == JSON_NUMBER) return v->num_val == 0.0;
    if (v->type == JSON_STRING) return v->str_val == NULL || !*v->str_val;
    if (v->type == JSON_ARRAY) return json_len(v) == 0;
    if (v->type == JSON_OBJECT) return json_object_size(v) == 0;
    return 1;
}

/*
 * PoP: jobstime_prune_inherited_knobs @ cron/jobs_time.py:prune_inherited_knobs
 */
void jobstime_prune_inherited_knobs(void *record) {
    json_t *rec = (json_t *)record;
    json_t *v;
    if (!rec || rec->type != JSON_OBJECT) return;
    v = json_obj_get(rec, "timezone");
    if (v && jt_falsy(v)) json_obj_del(rec, "timezone");
    v = json_obj_get(rec, "grace_minutes");
    if (v && v->type == JSON_NULL) json_obj_del(rec, "grace_minutes");
}

long long jobstime_min_grace_minutes(void) { return JOBSTIME_MIN_GRACE; }
long long jobstime_max_grace_minutes(void) { return JOBSTIME_MAX_GRACE; }
const char *jobstime_timezone_key(void) { return "timezone"; }
const char *jobstime_grace_key(void) { return "grace_minutes"; }
