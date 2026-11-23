/*
 * extra_fire.c — real C11 port of upstream Hermes cron/extra_fire.py.
 *
 * PoP: extrafire_queued @ cron/extra_fire.py:queued
 * PoP: extrafire_restore_target @ cron/extra_fire.py:restore_target
 * PoP: extrafire_is_extra @ cron/extra_fire.py:is_extra
 * PoP: extrafire_held_markers @ cron/extra_fire.py:held_markers
 * PoP: extrafire_pending_slot @ cron/extra_fire.py:pending_slot
 * PoP: extrafire_stamp_claim @ cron/extra_fire.py:stamp_claim
 * PoP: extrafire_reschedule @ cron/extra_fire.py:reschedule
 */
#include "noc_w5_extrafire_54bebe.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RESTORE_KEY  "manual_run_restore_at"
#define CLAIM_KEY    "extra_slot"

static json_t *jget(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT ? json_obj_get(obj, key) : NULL;
}

static int json_equal(const json_t *a, const json_t *b) {
    char *sa, *sb;
    int r;
    if (!a && !b) return 1;
    if (!a || !b) return 0;
    sa = json_dumps(a, 0);
    sb = json_dumps(b, 0);
    r = sa && sb && strcmp(sa, sb) == 0;
    free(sa);
    free(sb);
    return r;
}

static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

static int queued_flag(const json_t *job) {
    json_t *marker = jget(job, "manual_run_at");
    if (!marker || marker->type == JSON_NULL) return 0;
    if (!json_equal(marker, jget(job, "next_run_at"))) return 0;
    if (!jget(job, RESTORE_KEY)) return 0;
    return 1;
}

json_t *extrafire_queued(const json_t *job) {
    return json_bool(queued_flag(job) ? true : false);
}

json_t *extrafire_restore_target(const json_t *job) {
    json_t *v = queued_flag(job) ? jget(job, RESTORE_KEY) : jget(job, "next_run_at");
    return v ? json_copy(v) : json_null();
}

json_t *extrafire_is_extra(const json_t *job) {
    json_t *claim = jget(job, "fire_claim");
    if (!claim || claim->type != JSON_OBJECT) return json_bool(false);
    return json_bool(py_truthy(jget(claim, CLAIM_KEY)) ? true : false);
}

json_t *extrafire_held_markers(const json_t *job) {
    static const char *markers[] = {"manual_run_at", RESTORE_KEY, "manual_run_prompt"};
    json_t *out = json_object();
    size_t i;
    if (queued_flag(job)) {
        for (i = 0; i < sizeof(markers) / sizeof(markers[0]); i++) {
            json_t *v = jget(job, markers[i]);
            if (v) json_set(out, markers[i], json_copy(v));
        }
    }
    return out;
}

/* Days from civil date (Howard Hinnant), proleptic Gregorian. */
static long long days_from_civil(long long y, int m, int d) {
    long long era;
    unsigned yoe, doy, doe;
    y -= (m <= 2);
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (unsigned)(y - era * 400);
    doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}

/* Parse an ISO-8601 instant to UTC seconds. Returns 1 on success. */
static int parse_iso(const char *s, double *out) {
    int Y = 0, M = 0, D = 0, h = 0, mi = 0, se = 0;
    long off = 0;
    const char *p;
    if (!s) return 0;
    if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &Y, &M, &D, &h, &mi, &se) != 6) return 0;
    if (M < 1 || M > 12 || D < 1 || D > 31) return 0;
    p = s + 10; /* just after YYYY-MM-DD; scan for fractional / zone marker */
    while (*p && *p != '.' && *p != '+' && *p != '-' && *p != 'Z' && *p != 'z') p++;
    if (*p == '.') { while (*p && *p >= '0' && *p <= '9') p++; }
    if (*p == 'Z' || *p == 'z') { off = 0; }
    else if (*p == '+' || *p == '-') {
        int sign = (*p == '-') ? -1 : 1;
        int oh = 0, om = 0;
        p++;
        if (sscanf(p, "%2d:%2d", &oh, &om) < 1) return 0;
        off = sign * (oh * 3600L + om * 60L);
    } else if (*p != '\0') {
        return 0;
    }
    *out = (double)(days_from_civil(Y, M, D) * 86400LL + h * 3600 + mi * 60 + se - off);
    return 1;
}

static json_t *future_slot(const json_t *v, double now_e) {
    double e;
    if (v && v->type == JSON_STRING && v->str_val) {
        if (parse_iso(v->str_val, &e) && e > now_e) return json_copy(v);
    }
    return json_null();
}

json_t *extrafire_pending_slot(const json_t *job, const char *now, int manual_fire) {
    double now_e;
    json_t *stored, *marker;
    if (!parse_iso(now, &now_e)) return json_null();
    stored = jget(job, "next_run_at");
    marker = jget(job, "manual_run_at");
    if (json_equal(marker, stored)) return future_slot(jget(job, RESTORE_KEY), now_e);
    if (manual_fire) return future_slot(stored, now_e);
    return json_null();
}

static json_t *copy_job(const json_t *job) {
    return job && job->type == JSON_OBJECT ? json_copy(job) : json_object();
}

json_t *extrafire_stamp_claim(const json_t *job, const char *slot) {
    json_t *out = copy_job(job);
    json_t *claim = jget(out, "fire_claim");
    if (!claim || claim->type != JSON_OBJECT) {
        claim = json_object();
        json_set(out, "fire_claim", claim);
    }
    json_set(claim, CLAIM_KEY, json_string(slot));
    json_set(out, "next_run_at", json_string(slot));
    json_obj_del(out, RESTORE_KEY);
    return out;
}

json_t *extrafire_reschedule(const json_t *job, const char *at) {
    json_t *out = copy_job(job);
    if (queued_flag(job)) json_set(out, "manual_run_at", json_string(at));
    json_set(out, "next_run_at", json_string(at));
    return out;
}
