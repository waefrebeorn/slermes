/*
 * quota_hold.c — real C11 port of upstream Hermes cron/quota_hold.py.
 *
 * PoP: quotahold_hold_seconds @ cron/quota_hold.py:hold_seconds_from_failure
 * PoP: quotahold_hold_active @ cron/quota_hold.py:hold_active
 * PoP: quotahold_clear_state @ cron/quota_hold.py:clear_state
 * PoP: quotahold_is_recovery_fire @ cron/quota_hold.py:is_recovery_fire
 */
#include "noc_w5_quotahold_3054bf.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STATE_KEY       "quota_hold_until"
#define SCHEDULE_EXPR_KEY "quota_hold_cron_expr"

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

/* re.search(r"retry after (\d+)s", text, re.IGNORECASE) */
static int retry_hint(const char *text, double *out) {
    const char *p;
    if (!text) return 0;
    for (p = text; *p; p++) {
        if (strncasecmp(p, "retry after ", 12) == 0) {
            const char *q = p + 12;
            double v = 0.0;
            int digits = 0;
            while (*q >= '0' && *q <= '9') { v = v * 10.0 + (*q - '0'); q++; digits++; }
            if (digits > 0 && tolower((unsigned char)*q) == 's') { *out = v; return 1; }
        }
    }
    return 0;
}

json_t *quotahold_hold_seconds(const json_t *chain) {
    size_t i, n;
    if (!chain || chain->type != JSON_ARRAY) return json_null();
    n = chain->c.count;
    for (i = 0; i < n; i++) {
        json_t *e = json_get(chain, i);
        json_t *hint;
        double value;
        if (!e || !json_get_bool(e, "rate_limited", false)) continue;
        /* live returns here — a rate-limited AuthError is decisive. */
        hint = jget(e, "retry_after");
        if (hint && hint->type == JSON_NUMBER) {
            value = hint->num_val;
        } else if (!retry_hint(json_get_str(e, "text", ""), &value)) {
            return json_null();
        }
        return (value > 0.0) ? json_number(value) : json_null();
    }
    return json_null();
}

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

static int parse_iso(const char *s, double *out) {
    int Y = 0, M = 0, D = 0, h = 0, mi = 0, se = 0;
    long off = 0;
    const char *p;
    if (!s) return 0;
    if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &Y, &M, &D, &h, &mi, &se) != 6) return 0;
    if (M < 1 || M > 12 || D < 1 || D > 31) return 0;
    p = s + 10;
    while (*p && *p != '.' && *p != '+' && *p != '-' && *p != 'Z' && *p != 'z') p++;
    if (*p == '.') { while (*p && *p >= '0' && *p <= '9') p++; }
    if (*p == 'Z' || *p == 'z') off = 0;
    else if (*p == '+' || *p == '-') {
        int sign = (*p == '-') ? -1 : 1, oh = 0, om = 0;
        p++;
        if (sscanf(p, "%2d:%2d", &oh, &om) < 1) return 0;
        off = sign * (oh * 3600L + om * 60L);
    } else if (*p != '\0') return 0;
    *out = (double)(days_from_civil(Y, M, D) * 86400LL + h * 3600 + mi * 60 + se - off);
    return 1;
}

json_t *quotahold_hold_active(const json_t *job, const char *now) {
    json_t *until_v = jget(job, STATE_KEY);
    double until, now_e;
    if (!until_v || until_v->type != JSON_STRING || !until_v->str_val || !until_v->str_val[0])
        return json_bool(false);
    if (!parse_iso(until_v->str_val, &until)) return json_bool(false);
    if (!parse_iso(now, &now_e)) return json_bool(false);
    /* _instant_after(until, now): strictly after. */
    return json_bool(until > now_e ? true : false);
}

json_t *quotahold_clear_state(const json_t *job) {
    json_t *out = job && job->type == JSON_OBJECT ? json_copy(job) : json_object();
    json_obj_del(out, STATE_KEY);
    json_obj_del(out, SCHEDULE_EXPR_KEY);
    return out;
}

json_t *quotahold_is_recovery_fire(const json_t *job, const char *next_run) {
    json_t *schedule = jget(job, "schedule");
    json_t *kind, *expr, *next_v;
    if (!schedule || schedule->type != JSON_OBJECT) schedule = NULL;
    kind = schedule ? jget(schedule, "kind") : NULL;
    if (!kind || kind->type != JSON_STRING || !kind->str_val || strcmp(kind->str_val, "cron") != 0)
        return json_bool(false);
    next_v = json_string(next_run ? next_run : "");
    if (!json_equal(jget(job, STATE_KEY), next_v)) { json_free(next_v); return json_bool(false); }
    json_free(next_v);
    expr = schedule ? jget(schedule, "expr") : NULL;
    return json_bool(json_equal(jget(job, SCHEDULE_EXPR_KEY), expr) ? true : false);
}
