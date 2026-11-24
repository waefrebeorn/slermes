/*
 * subscriptions.c - C11 port of upstream plugins/teams_pipeline/subscriptions.py
 * (gate w26_subscripti_920391).
 *
 * PoP: w26_subscripti_920391_expected_client_state @ plugins/teams_pipeline/subscriptions.py:expected_client_state
 * PoP: w26_subscripti_920391_utc_timestamp @ plugins/teams_pipeline/subscriptions.py:utc_timestamp
 * PoP: w26_subscripti_920391_is_managed_subscription @ plugins/teams_pipeline/subscriptions.py:is_managed_subscription
 *
 * The TeamsPipelineStore, the Graph client and the clock are seams: the store
 * lookup becomes the list of ids it already knows, and every base instant is a
 * fixed input.  Ported here is what those values decide -- the configured
 * clientState, Graph's second-precision expiration stamp, and the ownership
 * test every remote subscription is gated on.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "noc_w26_subscripti_920391.h"

static const char *sb_safe(const char *s) { return s ? s : ""; }

static int sb_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *sb_dup(const char *s) {
    size_t n = strlen(sb_safe(s));
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, sb_safe(s), n + 1);
    return out;
}

/* Python str.strip() over ASCII whitespace; returns a malloc'd copy. */
static char *sb_strip(const char *s) {
    size_t a = 0, b;
    char *out;
    s = sb_safe(s);
    b = strlen(s);
    while (a < b && sb_is_ws(s[a])) a++;
    while (b > a && sb_is_ws(s[b - 1])) b--;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

/* Python truthiness. */
static int sb_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* str(value or "") over the shapes a Graph payload holds. */
static char *sb_str_of(const json_t *v) {
    char buf[64];
    if (!sb_truthy(v)) return sb_dup("");
    switch (v->type) {
    case JSON_STRING:
        return sb_dup(v->str_val);
    case JSON_BOOL:
        return sb_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        snprintf(buf, sizeof buf, "%g", v->num_val);
        return sb_dup(buf);
    default:
        return sb_dup("");
    }
}

/* "x or y or ''", then str(), for the two payload id/state lookups. */
static char *sb_pick_str(const json_t *obj, const char *k1, const char *k2) {
    const json_t *v = json_obj_get(obj, k1);
    char *out;
    if (!sb_truthy(v)) v = json_obj_get(obj, k2);
    if (!sb_truthy(v)) return sb_dup("");
    out = sb_str_of(v);
    return out;
}

/* ---- expected_client_state --------------------------------------------- */

json_t *w26_subscripti_920391_expected_client_state(const json_t *raw) {
    char *joined, *stripped;
    json_t *out;

    if (!raw || raw->type == JSON_NULL) {
        const char *env = getenv(W26_SUBSCRIPTI_920391_ENV);
        stripped = sb_strip(env ? env : "");
    } else {
        joined = sb_str_of(raw);
        stripped = sb_strip(joined ? joined : "");
        free(joined);
    }
    if (!stripped || stripped[0] == '\0') {
        free(stripped);
        return json_null();
    }
    out = json_string(stripped);
    free(stripped);
    return out;
}

/* ---- utc_timestamp ------------------------------------------------------ */

static long long sb_days_from_civil(long long y, int m, int d) {
    long long era;
    int yoe, doy, doe;
    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (int)(y - era * 400);
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097LL + (long long)doe - 719468LL;
}

static void sb_civil_from_days(long long z, long long *yp, unsigned *mp, unsigned *dp) {
    long long era, doe, yoe, y;
    long long doy, mp_, d;
    int m;
    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = z - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp_ = (5 * doy + 2) / 153;
    d = doy - (153 * mp_ + 2) / 5 + 1;
    m = (int)mp_ + (mp_ < 10 ? 3 : -9);
    *yp = y + (m <= 2 ? 1 : 0);
    *mp = (unsigned)m;
    *dp = (unsigned)d;
}

static int sb_digits(const char *s, size_t n, int *out) {
    size_t i;
    int v = 0;
    for (i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        v = v * 10 + (s[i] - '0');
    }
    *out = v;
    return 1;
}

/* _parse_datetime: ISO-8601 with Z accepted and an explicit offset honoured. */
static int sb_parse_iso(const char *text, long long *epoch_out) {
    int y = 0, mo = 0, d = 0, hh = 0, mi = 0, ss = 0, off = 0;
    const char *p = text;
    size_t n = strlen(text);
    long long days;

    if (n < 10) return 0;
    if (!sb_digits(p, 4, &y) || p[4] != '-' || !sb_digits(p + 5, 2, &mo) || p[7] != '-' ||
        !sb_digits(p + 8, 2, &d)) return 0;
    p += 10;
    if (*p == 'T' || *p == 't' || *p == ' ') {
        p++;
        if (!sb_digits(p, 2, &hh) || p[2] != ':' || !sb_digits(p + 3, 2, &mi)) return 0;
        p += 5;
        if (*p == ':') {
            if (!sb_digits(p + 1, 2, &ss)) return 0;
            p += 3;
            if (*p == '.' || *p == ',') {
                p++;
                while (*p >= '0' && *p <= '9') p++;   /* replace(microsecond=0) */
            }
        }
    }
    if (*p == 'Z' || *p == 'z') {
        p++;
    } else if (*p == '+' || *p == '-') {
        int sign = (*p == '-') ? -1 : 1, oh = 0, om = 0;
        p++;
        if (!sb_digits(p, 2, &oh)) return 0;
        p += 2;
        if (*p == ':') p++;
        if (*p >= '0' && *p <= '9') {
            if (!sb_digits(p, 2, &om)) return 0;
            p += 2;
        }
        off = sign * (oh * 3600 + om * 60);
    }
    if (*p != '\0') return 0;
    if (mo < 1 || mo > 12 || d < 1 || d > 31) return 0;

    days = sb_days_from_civil(y, mo, d);
    *epoch_out = days * 86400LL + (long long)hh * 3600LL + (long long)mi * 60LL + ss - off;
    return 1;
}

json_t *w26_subscripti_920391_utc_timestamp(const json_t *hours_from_now, const json_t *base) {
    long long epoch, add, day;
    double hours = 0.0;
    long long y;
    unsigned mo, d;
    int hh, mi, ss;
    char buf[64];

    if (!hours_from_now || hours_from_now->type == JSON_NULL) hours = 0.0;
    else if (hours_from_now->type == JSON_NUMBER) hours = hours_from_now->num_val;
    else if (hours_from_now->type == JSON_BOOL) hours = hours_from_now->bool_val ? 1.0 : 0.0;
    else return json_null();   /* timedelta(hours="x") -> TypeError */

    if (!base || base->type != JSON_STRING) return json_null();
    if (!sb_parse_iso(sb_safe(base->str_val), &epoch)) return json_null();

    add = (long long)llround(hours * 3600.0);
    epoch += add;

    day = epoch / 86400;
    if (epoch % 86400 < 0) day -= 1;
    {
        long long sod = epoch - day * 86400;
        sb_civil_from_days(day, &y, &mo, &d);
        hh = (int)(sod / 3600);
        mi = (int)((sod % 3600) / 60);
        ss = (int)(sod % 60);
    }
    snprintf(buf, sizeof buf, "%04lld-%02u-%02uT%02d:%02d:%02dZ", y, mo, d, hh, mi, ss);
    return json_string(buf);
}

/* ---- is_managed_subscription ------------------------------------------- */

json_t *w26_subscripti_920391_is_managed_subscription(const json_t *known_ids,
                                                     const json_t *payload,
                                                     const json_t *expected_client_state_value) {
    char *raw, *sid, *state;
    size_t i, n;
    int managed = 0;

    if (!payload || payload->type != JSON_OBJECT) return json_null();  /* AttributeError */

    raw = sb_pick_str(payload, "subscription_id", "id");
    sid = sb_strip(raw ? raw : "");
    free(raw);

    if (sid && sid[0]) {
        n = (known_ids && known_ids->type == JSON_ARRAY) ? json_len(known_ids) : 0;
        for (i = 0; i < n; i++) {
            const json_t *k = json_get(known_ids, i);
            if (k && k->type == JSON_STRING && strcmp(sb_safe(k->str_val), sid) == 0) {
                managed = 1;
                break;
            }
        }
    }

    if (!managed) {
        raw = sb_pick_str(payload, "client_state", "clientState");
        state = sb_strip(raw ? raw : "");
        free(raw);
        if (sb_truthy(expected_client_state_value) &&
            expected_client_state_value->type == JSON_STRING &&
            state && strcmp(state, sb_safe(expected_client_state_value->str_val)) == 0)
            managed = 1;
        free(state);
    }
    free(sid);
    return json_bool(managed);
}
