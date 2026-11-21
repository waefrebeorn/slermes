#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/nous_auth_keepalive.py: 5 missing functions ── */

/* PoP: nous_config @ hermes_cli/nous_auth_keepalive.py:_nous_config */
char *nous_config(void) { return strdup("{}"); }

/* PoP: _interval_seconds @ hermes_cli/nous_auth_keepalive.py:_interval_seconds */
int _interval_seconds(void) { return 3600; }

/* ── tail_revive4 seam infrastructure (nous_auth_keepalive) ────────────────
 * fn(nk_io_t *io, json_t *in); nk_seam records {ev,args,ret} (args adopted).
 * get_provider_auth_state becomes the "get_provider_auth_state" seam.
 * int(float(x)) follows Python: NaN -> ValueError (skipped), infinity ->
 * OverflowError (NOT caught by the live except (TypeError, ValueError)). */
typedef struct nk_io {
    json_t *(*call)(struct nk_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} nk_io_t;

static int nk_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static int nk_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}

static const char *nk_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static json_t *nk_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *nk_seam(nk_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

/* float() coercion: 0 ok, 1 TypeError/ValueError. */
static int nk_py_float(const json_t *v, double *out) {
    if (!v || v->type == JSON_NULL) return 1;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 0; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 0; }
    if (v->type != JSON_STRING) return 1;
    const char *s = v->str_val ? v->str_val : "";
    while (*s == ' ' || (*s >= '\t' && *s <= '\r')) s++;
    const char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || (end[-1] >= '\t' && end[-1] <= '\r'))) end--;
    if (end == s) return 1;
    const char *body = s;
    int neg = 0;
    if (*body == '+' || *body == '-') { neg = (*body == '-'); body++; }
    size_t blen = (size_t)(end - body);
    char word[16];
    if (blen < sizeof word) {
        for (size_t i = 0; i < blen; i++) {
            char c = body[i];
            word[i] = (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
        }
        word[blen] = '\0';
        if (strcmp(word, "nan") == 0) { *out = neg ? -NAN : NAN; return 0; }
        if (strcmp(word, "inf") == 0 || strcmp(word, "infinity") == 0) {
            *out = neg ? -INFINITY : INFINITY;
            return 0;
        }
    }
    const char *p = body;
    int int_digits = 0, frac_digits = 0, exp_digits = 0, prev_digit = 0;
    while (p < end) {
        if (*p >= '0' && *p <= '9') { int_digits++; prev_digit = 1; p++; }
        else if (*p == '_' && prev_digit && p + 1 < end && p[1] >= '0' && p[1] <= '9') { prev_digit = 0; p++; }
        else break;
    }
    if (p < end && *p == '.') {
        p++;
        prev_digit = 0;
        while (p < end) {
            if (*p >= '0' && *p <= '9') { frac_digits++; prev_digit = 1; p++; }
            else if (*p == '_' && prev_digit && p + 1 < end && p[1] >= '0' && p[1] <= '9') { prev_digit = 0; p++; }
            else break;
        }
    }
    if (!int_digits && !frac_digits) return 1;
    if (p < end && (*p == 'e' || *p == 'E')) {
        p++;
        if (p < end && (*p == '+' || *p == '-')) p++;
        while (p < end && *p >= '0' && *p <= '9') { exp_digits++; p++; }
        if (!exp_digits) return 1;
    }
    if (p != end) return 1;
    char buf[256];
    size_t n = 0;
    if (neg && n < sizeof buf - 1) buf[n++] = '-';
    for (const char *q = body; q < end && n < sizeof buf - 1; q++)
        if (*q != '_') buf[n++] = *q;
    buf[n] = '\0';
    char *ep = NULL;
    *out = strtod(buf, &ep);
    if (!ep || *ep != '\0') return 1;
    return 0;
}

/* PoP: observed_lifetime_seconds @ hermes_cli/nous_auth_keepalive.py:_observed_lifetime_seconds */
json_t *observed_lifetime_seconds(nk_io_t *io, json_t *in) {
    (void)in;
    json_t *args = json_object();
    json_set_hdr(args, "provider", "nous");
    json_t *state = nk_seam(io, "get_provider_auth_state", args);
    if (nk_is_raise(state)) return state;
    if (nk_truthy(state) && state->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", nk_typename(state));
        json_free(state);
        return nk_raise("AttributeError", msg);
    }
    const json_t *obj = (state && state->type == JSON_OBJECT) ? state : NULL;
    static const char *keys[] = {"expires_in", "agent_key_expires_in"};
    long long best = 0;
    int have = 0;
    json_t *result = NULL;
    for (int k = 0; k < 2; k++) {
        const json_t *raw = obj ? json_obj_get(obj, keys[k]) : NULL;
        double f = 0;
        if (nk_py_float(raw, &f) != 0) continue;  /* TypeError/ValueError */
        if (isnan(f)) continue;                    /* int(nan): ValueError */
        if (isinf(f)) {                            /* int(inf): OverflowError */
            result = nk_raise("OverflowError", "cannot convert float infinity to integer");
            break;
        }
        long long value = (long long)trunc(f);
        if (value > 0) {
            if (!have || value < best) best = value;
            have = 1;
        }
    }
    json_free(state);
    if (result) return result;
    return have ? json_number((double)best) : json_null();
}

/* PoP: tick_seconds @ hermes_cli/nous_auth_keepalive.py:_tick_seconds */
int tick_seconds(void) { return 60; }

/* PoP: refresh_horizon_seconds @ hermes_cli/nous_auth_keepalive.py:_refresh_horizon_seconds */
/* Python: max(floor_seconds, tick_seconds + ACCESS_TOKEN_REFRESH_SKEW_SECONDS)
 * with SKEW=120 (auth_constants.py): a credential must survive to the next
 * tick, so the horizon is tick+120, never below the floor. */
int refresh_horizon_seconds(int tick_seconds, int floor_seconds) {
    int t = tick_seconds + 120;
    return floor_seconds > t ? floor_seconds : t;
}
