#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/review_idle_queue.py: 10 missing functions ── */

/* PoP: defer_mode @ agent/review_idle_queue.py:defer_mode */
char *defer_mode(void) { return strdup(""); }

/* ── tail_revive4 seam infrastructure (review_idle_queue) ──────────────────
 * fn(rq_io_t *io, json_t *in); rq_seam records {ev,args,ret} (args adopted).
 * _MAX_AGE_DEFAULT_S = 30.0 * 60.0. defer_max_age_s is pure; _quiet_for reads
 * self.quiet_period() through the "quiet_period" seam (its tuple result is a
 * JSON array; empty -> IndexError, non-subscriptable -> TypeError). */
typedef struct rq_io {
    json_t *(*call)(struct rq_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} rq_io_t;

#define RQ_MAX_AGE_DEFAULT_S 1800.0

static int rq_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static int rq_truthy(const json_t *v) {
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

static const char *rq_typename(const json_t *v) {
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

static json_t *rq_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *rq_seam(rq_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

/* Python float() coercion: 0 ok, 1 TypeError/ValueError (both fall back to
 * the default in defer_max_age_s). Handles sign, inner underscores, decimal
 * and exponent forms, and nan/inf literals. */
static int rq_py_float(const json_t *v, double *out) {
    if (!v || v->type == JSON_NULL) return 1;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 0; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 0; }
    if (v->type != JSON_STRING) return 1;
    const char *s = v->str_val ? v->str_val : "";
    while (*s == ' ' || (*s >= '\t' && *s <= '\r')) s++;
    const char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || (end[-1] >= '\t' && end[-1] <= '\r'))) end--;
    if (end == s) return 1;
    /* nan / inf literals (case-insensitive, optional sign) */
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
        if (strcmp(word, "nan") == 0) {
            *out = neg ? -NAN : NAN;
            return 0;
        }
        if (strcmp(word, "inf") == 0 || strcmp(word, "infinity") == 0) {
            *out = neg ? -INFINITY : INFINITY;
            return 0;
        }
    }
    /* numeric form: digits[_digits][.digits][_digits]][e[+-]digits] */
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

/* PoP: defer_max_age_s @ agent/review_idle_queue.py:defer_max_age_s */
json_t *defer_max_age_s(void *io, json_t *in) {
    (void)io;
    json_t *cfg = json_obj_get(in, "task_cfg");
    const json_t *raw = NULL;
    if (rq_truthy(cfg)) {
        if (cfg->type != JSON_OBJECT) {
            char msg[160];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", rq_typename(cfg));
            return rq_raise("AttributeError", msg);
        }
        raw = json_obj_get(cfg, "defer_max_age_s");
    }
    /* (task_cfg or {}).get("defer_max_age_s", _MAX_AGE_DEFAULT_S) */
    double value = RQ_MAX_AGE_DEFAULT_S;
    if (raw) {
        double v = 0;
        if (rq_py_float(raw, &v) == 0) value = v;
    }
    return json_number(value > 0 ? value : RQ_MAX_AGE_DEFAULT_S);
}

/* PoP: _quiet_for @ agent/review_idle_queue.py:_quiet_for */
json_t *_quiet_for(rq_io_t *io, json_t *in) {
    (void)in;
    json_t *r = rq_seam(io, "quiet_period", NULL);
    if (rq_is_raise(r)) return r;
    if (!r || r->type == JSON_NULL)
        return rq_raise("TypeError", "'NoneType' object is not subscriptable");
    if (r->type == JSON_ARRAY) {
        if (r->c.count == 0) {
            json_free(r);
            return rq_raise("IndexError", "tuple index out of range");
        }
        json_t *first = json_copy(r->c.items[0]);
        json_free(r);
        return first;
    }
    if (r->type == JSON_STRING) {
        const char *s = r->str_val ? r->str_val : "";
        if (!s[0]) {
            json_free(r);
            return rq_raise("IndexError", "string index out of range");
        }
        /* first codepoint */
        const char *p = s;
        const unsigned char *u = (const unsigned char *)s;
        size_t adv = 1;
        if (u[0] >= 0xC0 && u[0] < 0xE0) adv = 2;
        else if (u[0] >= 0xE0 && u[0] < 0xF0) adv = 3;
        else if (u[0] >= 0xF0) adv = 4;
        (void)p;
        json_t *first = json_string("");
        free(first->str_val);
        first->str_val = malloc(adv + 1);
        memcpy(first->str_val, s, adv);
        first->str_val[adv] = '\0';
        json_free(r);
        return first;
    }
    if (r->type == JSON_OBJECT) {
        json_free(r);
        return rq_raise("KeyError", "0");
    }
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object is not subscriptable", rq_typename(r));
    json_free(r);
    return rq_raise("TypeError", msg);
}

/* PoP: review_targets_managed_local @ agent/review_idle_queue.py:review_targets_managed_local */
char *review_targets_managed_local(void) { return strdup("[]"); }




/* PoP: pop_dispatchable @ agent/review_idle_queue.py:_pop_dispatchable */
char *pop_dispatchable(void) { return strdup("{}"); }

/* PoP: still_enabled @ agent/review_idle_queue.py:_still_enabled */
int still_enabled(void) { return 1; }

/* PoP: managed_server_idle @ agent/review_idle_queue.py:_managed_server_idle */
int managed_server_idle(void) { return 1; }
