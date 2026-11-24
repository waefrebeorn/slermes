/*
 * _shared.c — C11 port of upstream Hermes gateway/platforms/_shared.py
 * (gates w31_shared_97a29d_p1 / w31_shared_97a29d_p2).
 *
 * PoP: w31_shared_97a29d_p1_secrets_match @ gateway/platforms/_shared.py:secrets_match
 * PoP: w31_shared_97a29d_p1_timestamp_fresh @ gateway/platforms/_shared.py:timestamp_fresh
 * PoP: w31_shared_97a29d_p1_decode_json_list_literal @ gateway/platforms/_shared.py:decode_json_list_literal
 * PoP: w31_shared_97a29d_p1_coerce_port @ gateway/platforms/_shared.py:coerce_port
 * PoP: w31_shared_97a29d_p1_bridged_env_marker @ gateway/platforms/_shared.py:bridged_env_marker
 * PoP: w31_shared_97a29d_p1_request_never_left @ gateway/platforms/_shared.py:request_never_left
 * PoP: w31_shared_97a29d_p2_delivery_state @ gateway/platforms/_shared.py:delivery_state
 * PoP: w31_shared_97a29d_p2_delivered_message_ids @ gateway/platforms/_shared.py:delivered_message_ids
 * PoP: w31_shared_97a29d_p2_outcome_failure @ gateway/platforms/_shared.py:DeliveryOutcome.failure
 * PoP: w31_shared_97a29d_p2_split_fields @ gateway/platforms/_shared.py:DeliveryOutcome.split_fields
 * PoP: w31_shared_97a29d_p2_yaml_kind_applies @ gateway/platforms/_shared.py:_YAML_KINDS.<kind>[0]
 * PoP: w31_shared_97a29d_p2_yaml_bridge_seed @ gateway/platforms/_shared.py:apply_yaml_bridge
 *
 * What is ported is the value->value decision each helper makes. The seams
 * stay in the caller: every env read and write (get_scoped_secret,
 * platform_gate_env, yaml_env_setter), the profile secret scope, the live-gate
 * config reload, the clock (timestamp_fresh takes `now` instead of time.time()),
 * the exception objects themselves (request_never_left takes their MRO names),
 * the `json` YAML kind's json.dumps encode, and the redaction
 * gateway.outbound.senders._error applies. CPython semantics are spelled out
 * where C would otherwise differ: `int()` over str/float/bool, Python
 * truthiness, `is True`, `str()`, and `==`.
 */
#include "hermes_json.h"
#include "noc_w31_shared_97a29d_p1.h"
#include "noc_w31_shared_97a29d_p2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* WEBHOOK_REPLAY_WINDOW_SECONDS: the default drift allowance, either way. */
#define SH_REPLAY_WINDOW_SECONDS 300.0

#define SH_OK 1
#define SH_MISS 0

/* ------------------------------------------------------------------ helpers */

static char *sh_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) return NULL;
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* The ASCII whitespace CPython's str.strip()/lstrip() consume here. */
static int sh_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python truthiness of a JSON value. */
static int sh_truthy(const json_t *v) {
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

/* `value is exactly the string s` (Python's == is strcmp for two strs). */
static int sh_str_is(const json_t *v, const char *s) {
    if (!v || v->type != JSON_STRING || !v->str_val || !s) return 0;
    return strcmp(v->str_val, s) == 0;
}

/* Python `==` over the shapes these values hold.  A missing key and an explicit
 * null are the same element.  bool is an int in Python, so True == 1. */
static int sh_eq_py(const json_t *a, const json_t *b) {
    double x = 0.0, y = 0.0;
    size_t i, n;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if ((a->type == JSON_NUMBER || a->type == JSON_BOOL) &&
        (b->type == JSON_NUMBER || b->type == JSON_BOOL)) {
        x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!sh_eq_py(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k || !sh_eq_py(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
        }
        return 1;
    }
    return 0;
}

/* CPython int(str) for a JSON string: strip whitespace, optional sign, decimal
 * digits with single underscores between them, nothing else.  Values beyond the
 * C 64-bit range are outside this port's numeric domain and read as a failure
 * (the caller's default), never as a wrapped number. */
static int sh_int_from_str(const char *s, long long *out) {
    size_t i = 0, n;
    int neg = 0, digits = 0, prev_us = 0;
    unsigned long long acc = 0;
    if (!s || !out) return SH_MISS;
    n = strlen(s);
    while (i < n && sh_is_space(s[i])) i++;
    while (n > i && sh_is_space(s[n - 1])) n--;
    if (i >= n) return SH_MISS;
    if (s[i] == '+' || s[i] == '-') { neg = s[i] == '-'; i++; }
    if (i >= n) return SH_MISS;
    for (; i < n; i++) {
        char c = s[i];
        if (c == '_') {
            if (!digits || prev_us) return SH_MISS; /* no leading/double underscore */
            prev_us = 1;
            continue;
        }
        if (c < '0' || c > '9') return SH_MISS;
        prev_us = 0;
        acc = acc * 10u + (unsigned long long)(c - '0');
        if (acc > 9223372036854775807ULL) return SH_MISS;
        digits++;
    }
    if (!digits || prev_us) return SH_MISS; /* a trailing underscore is invalid */
    *out = neg ? -(long long)acc : (long long)acc;
    return SH_OK;
}

/* CPython int(value) over the values int() accepts here: bool, int/float
 * (truncated toward zero, non-finite and out-of-range read as a failure), and
 * a decimal string.  None/list/dict are TypeError -> failure. */
static int sh_int_py(const json_t *v, long long *out) {
    double d;
    if (!v || !out) return SH_MISS;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return SH_OK; }
    if (v->type == JSON_STRING) return sh_int_from_str(v->str_val, out);
    if (v->type != JSON_NUMBER) return SH_MISS;
    d = v->num_val;
    if (d != d) return SH_MISS;                                  /* NaN: ValueError */
    if (d >= 9223372036854775808.0 || d <= -9223372036854775808.0) return SH_MISS;
    *out = (long long)d;                                         /* trunc toward zero */
    return SH_OK;
}

/* CPython str(value) for the shapes an id or a scalar holds. */
static char *sh_str_py(const json_t *v) {
    char buf[64];
    double d;
    if (!v || v->type == JSON_NULL) return sh_dup("None");
    switch (v->type) {
    case JSON_BOOL: return sh_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return sh_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        d = v->num_val;
        if (d == (double)(long long)d && d < 9223372036854775807.0 && d > -9223372036854775807.0)
            snprintf(buf, sizeof buf, "%.0f", d);
        else
            snprintf(buf, sizeof buf, "%.17g", d);
        return sh_dup(buf);
    default:
        /* A container's str() is a repr; the port reports its JSON text. */
        return json_dumps(v, 0);
    }
}

static size_t sh_utf8_len(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

/* ----------------------------------------------------------------- gate p1 */

json_t *w31_shared_97a29d_p1_secrets_match(const json_t *provided, const json_t *expected) {
    if (!provided || provided->type != JSON_STRING) return json_bool(0);
    if (!expected || expected->type != JSON_STRING) return json_bool(0);
    if (!expected->str_val || expected->str_val[0] == '\0') return json_bool(0);
    /* UTF-8 with surrogatepass is injective, so the byte compare is the str
     * compare; compare_digest is about timing, not about the answer. */
    return json_bool(strcmp(provided->str_val ? provided->str_val : "", expected->str_val) == 0);
}

json_t *w31_shared_97a29d_p1_timestamp_fresh(const json_t *raw, const json_t *window,
                                             const json_t *now) {
    long long epoch;
    double w, t, delta;
    if (!window || (window->type != JSON_NUMBER && window->type != JSON_BOOL)) return json_bool(0);
    w = window->type == JSON_BOOL ? (window->bool_val ? 1.0 : 0.0) : window->num_val;
    if (!now || (now->type != JSON_NUMBER && now->type != JSON_BOOL)) return json_bool(0);
    t = now->type == JSON_BOOL ? (now->bool_val ? 1.0 : 0.0) : now->num_val;
    if (!sh_int_py(raw, &epoch)) return json_bool(0);
    delta = t - (double)epoch;
    if (delta < 0.0) delta = -delta;
    return json_bool(delta <= w);
}

json_t *w31_shared_97a29d_p1_decode_json_list_literal(const json_t *raw) {
    const char *s;
    json_t *parsed;
    if (raw && raw->type == JSON_STRING && raw->str_val) {
        s = raw->str_val;
        while (*s && sh_is_space(*s)) s++;
        if (*s == '[') {
            parsed = json_parse(raw->str_val, NULL);
            if (parsed && parsed->type == JSON_ARRAY) return parsed;
            json_free(parsed);
        }
    }
    return raw ? json_copy(raw) : json_null();
}

json_t *w31_shared_97a29d_p1_coerce_port(const json_t *value, const json_t *default_value) {
    long long iv;
    if (sh_int_py(value, &iv)) return json_number((double)iv);
    return default_value ? json_copy(default_value) : json_null();
}

json_t *w31_shared_97a29d_p1_bridged_env_marker(const json_t *name) {
    /* gateway/bridged_env.BRIDGED_ENV_MARKER_PREFIX */
    static const char PREFIX[] = "HERMES_BRIDGED_";
    char *body = sh_str_py(name);              /* f"{name}" str()s the value */
    char *buf;
    size_t need;
    json_t *out;
    if (!body) return json_string(PREFIX);
    need = sizeof PREFIX + strlen(body);       /* PREFIX already carries the NUL */
    buf = (char *)malloc(need);
    if (!buf) { free(body); return json_null(); }
    snprintf(buf, need, "%s%s", PREFIX, body);
    out = json_string(buf);
    free(buf);
    free(body);
    return out;
}

/* _NEVER_LEFT_ERRORS, matched by class __name__. */
static const char *const SH_NEVER_LEFT[] = {
    "ConnectError", "ConnectTimeout", "PoolTimeout",
    "ClientConnectorError", "ClientConnectorDNSError", "ClientProxyConnectionError",
    "ConnectionTimeoutError", "ConnectionRefusedError",
};
#define SH_NEVER_LEFT_N (sizeof SH_NEVER_LEFT / sizeof SH_NEVER_LEFT[0])

json_t *w31_shared_97a29d_p1_request_never_left(const json_t *chain) {
    size_t i, j, n, m;
    if (!chain || chain->type != JSON_ARRAY) return json_bool(0);
    n = json_len(chain);
    for (i = 0; i < n; i++) {
        const json_t *mro = json_get(chain, i);
        if (!mro || mro->type != JSON_ARRAY) continue;
        m = json_len(mro);
        for (j = 0; j < m; j++) {
            const json_t *name = json_get(mro, j);
            size_t k;
            if (!name || name->type != JSON_STRING || !name->str_val) continue;
            for (k = 0; k < SH_NEVER_LEFT_N; k++)
                if (strcmp(name->str_val, SH_NEVER_LEFT[k]) == 0) return json_bool(1);
        }
    }
    return json_bool(0);
}

/* ----------------------------------------------------------------- gate p2 */

json_t *w31_shared_97a29d_p2_delivery_state(const json_t *result) {
    const json_t *delivery, *retryable;
    if (!result || result->type != JSON_OBJECT) return json_string(W31_SHARED_97A29D_P2_DELIVERED);
    if (!sh_truthy(json_obj_get(result, "error")))
        return json_string(W31_SHARED_97A29D_P2_DELIVERED);
    delivery = json_obj_get(result, "delivery");
    if (sh_str_is(delivery, W31_SHARED_97A29D_P2_PARTIAL))
        return json_string(W31_SHARED_97A29D_P2_PARTIAL);
    if (sh_str_is(delivery, W31_SHARED_97A29D_P2_UNCERTAIN))
        return json_string(W31_SHARED_97A29D_P2_UNCERTAIN);
    if (sh_str_is(delivery, W31_SHARED_97A29D_P2_NONE))
        return json_string(W31_SHARED_97A29D_P2_NONE);
    retryable = json_obj_get(result, "retryable");
    if (retryable && retryable->type == JSON_BOOL && retryable->bool_val)
        return json_string(W31_SHARED_97A29D_P2_NONE);
    return json_string(W31_SHARED_97A29D_P2_UNCERTAIN);
}

json_t *w31_shared_97a29d_p2_delivered_message_ids(const json_t *result) {
    json_t *out = json_array();
    const json_t *ids;
    size_t i, n;
    if (!result || result->type != JSON_OBJECT) return out;
    ids = json_obj_get(result, "delivered_message_ids");
    if (!sh_truthy(ids)) return out;                 /* `ids or ()`: [] when falsy */
    if (ids->type == JSON_ARRAY) {
        n = json_len(ids);
        for (i = 0; i < n; i++) {
            const json_t *v = json_get(ids, i);
            char *s;
            if (!v || v->type == JSON_NULL) continue;  /* `if i is not None` */
            s = sh_str_py(v);
            if (s) { json_append(out, json_string(s)); free(s); }
        }
        return out;
    }
    if (ids->type == JSON_STRING) {
        /* Python iterates a str by code point: one single-character id each. */
        const char *s = ids->str_val ? ids->str_val : "";
        size_t len = strlen(s), pos = 0;
        while (pos < len) {
            size_t cl = sh_utf8_len((unsigned char)s[pos]);
            char buf[8];
            size_t k;
            if (pos + cl > len) cl = 1;
            if (cl >= sizeof buf) cl = sizeof buf - 1;
            for (k = 0; k < cl; k++) buf[k] = s[pos + k];
            buf[cl] = '\0';
            json_append(out, json_string(buf));
            pos += cl;
        }
        return out;
    }
    if (ids->type == JSON_OBJECT) {
        n = json_object_size(ids);
        for (i = 0; i < n; i++) {                     /* a dict iterates its keys */
            const char *k = json_object_get_key_at(ids, i);
            if (k) json_append(out, json_string(k));
        }
        return out;
    }
    /* A scalar is not iterable upstream (TypeError); no ids are known here. */
    return out;
}

json_t *w31_shared_97a29d_p2_outcome_failure(const json_t *error, const json_t *message_ids,
                                             const json_t *has_landed, const json_t *is_ambiguous,
                                             const json_t *never_sent) {
    const json_t *inner;
    json_t *ids = json_array();
    json_t *out = json_object();
    const char *state;
    int landed = sh_truthy(has_landed);
    int ambiguous = sh_truthy(is_ambiguous);
    int never = sh_truthy(never_sent);
    size_t i, n;

    if (!error || error->type != JSON_OBJECT) return out;

    /* ids = list(dict.fromkeys([*self.message_ids, *delivered_message_ids(error)])) */
    if (message_ids && message_ids->type == JSON_ARRAY) {
        n = json_len(message_ids);
        for (i = 0; i < n; i++) {
            const json_t *v = json_get(message_ids, i);
            size_t j, m = json_len(ids);
            int seen = 0;
            for (j = 0; j < m; j++)
                if (sh_eq_py(json_get(ids, j), v)) { seen = 1; break; }
            if (!seen) json_append(ids, v ? json_copy(v) : json_null());
        }
    }
    {
        json_t *own = w31_shared_97a29d_p2_delivered_message_ids(error);
        n = json_len(own);
        for (i = 0; i < n; i++) {
            const json_t *v = json_get(own, i);
            size_t j, m = json_len(ids);
            int seen = 0;
            for (j = 0; j < m; j++)
                if (sh_eq_py(json_get(ids, j), v)) { seen = 1; break; }
            if (!seen) json_append(ids, json_copy(v));
        }
        json_free(own);
    }

    inner = json_obj_get(error, "delivery");
    if (landed || json_len(ids) > 0 || sh_str_is(inner, W31_SHARED_97A29D_P2_PARTIAL)) {
        state = W31_SHARED_97A29D_P2_PARTIAL;
    } else if (ambiguous || sh_str_is(inner, W31_SHARED_97A29D_P2_UNCERTAIN) ||
               !(never || sh_str_is(inner, W31_SHARED_97A29D_P2_NONE))) {
        state = W31_SHARED_97A29D_P2_UNCERTAIN;
    } else {
        state = W31_SHARED_97A29D_P2_NONE;
    }

    /* out = {k: v for k, v in error.items() if k not in ("retryable",
     * "delivered_message_ids")} -- insertion order kept. */
    n = json_object_size(error);
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(error, i);
        if (!k) continue;
        if (strcmp(k, "retryable") == 0 || strcmp(k, "delivered_message_ids") == 0) continue;
        json_set(out, k, json_copy(json_object_get_at(error, i)));
    }
    json_set(out, "delivery", json_string(state));
    if (json_len(ids) > 0) json_set(out, "delivered_message_ids", json_copy(ids));
    if (strcmp(state, W31_SHARED_97A29D_P2_NONE) == 0) json_set(out, "retryable", json_bool(1));
    json_free(ids);
    return out;
}

json_t *w31_shared_97a29d_p2_split_fields(const json_t *message_ids) {
    json_t *out = json_object();
    if (message_ids && message_ids->type == JSON_ARRAY && json_len(message_ids) > 1)
        json_set(out, "delivered_message_ids", json_copy(message_ids));
    return out;
}

json_t *w31_shared_97a29d_p2_yaml_kind_applies(const json_t *cfg, const char *key,
                                               const char *kind) {
    const json_t *v;
    if (!cfg || cfg->type != JSON_OBJECT || !key) return json_bool(0);
    v = json_obj_get(cfg, key);
    if (!kind) return json_null();
    if (strcmp(kind, "lower") == 0 || strcmp(kind, "json") == 0)
        return json_bool(v != NULL);                       /* `key in cfg` */
    if (strcmp(kind, "str") == 0) {
        /* cfg.get(key) not in (None, "") -- 0/False are real values. */
        if (!v || v->type == JSON_NULL) return json_bool(0);
        if (v->type == JSON_STRING && (!v->str_val || v->str_val[0] == '\0')) return json_bool(0);
        return json_bool(1);
    }
    if (strcmp(kind, "csv") == 0)
        return json_bool(v && v->type != JSON_NULL);       /* cfg.get(key) is not None */
    return json_null();                                    /* KeyError upstream */
}

json_t *w31_shared_97a29d_p2_yaml_bridge_seed(const json_t *cfg, const json_t *spec) {
    json_t *seeded = json_object();
    size_t i, n;
    if (!cfg || cfg->type != JSON_OBJECT || !spec || spec->type != JSON_ARRAY)
        return seeded;
    n = json_len(spec);
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(spec, i);
        const json_t *key, *kind;
        json_t *applies;
        if (!row || row->type != JSON_ARRAY || json_len(row) < 3) continue;
        key = json_get(row, 0);
        kind = json_get(row, 2);
        if (!key || key->type != JSON_STRING || !key->str_val) continue;
        if (!kind || kind->type != JSON_STRING || !kind->str_val) continue;
        applies = w31_shared_97a29d_p2_yaml_kind_applies(cfg, key->str_val, kind->str_val);
        if (sh_truthy(applies))
            json_set(seeded, key->str_val, json_copy(json_obj_get(cfg, key->str_val)));
        json_free(applies);
    }
    if (json_object_size(seeded) == 0) { json_free(seeded); return json_null(); }
    return seeded;
}
