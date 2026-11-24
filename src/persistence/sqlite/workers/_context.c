/*
 * _context.c - C11 port of upstream persistence/sqlite/workers/_context.py
 * (gate w26_context_00d980).
 *
 * PoP: w26_context_00d980_validate @ persistence/sqlite/workers/_context.py:worker_context
 * PoP: w26_context_00d980_validate @ persistence/sqlite/workers/_context.py:worker_prompt
 * PoP: w26_context_00d980_validate @ persistence/sqlite/workers/_context.py:worker_sidecars
 * PoP: w26_context_00d980_validate @ persistence/sqlite/workers/_context.py:worker_tool_names
 * PoP: w26_context_00d980_validate @ persistence/sqlite/workers/_context.py:worker_tail_role
 * PoP: w26_context_00d980_validate @ persistence/sqlite/workers/_context.py:worker_row_role
 * PoP: w26_context_00d980_stamp @ persistence/sqlite/workers/_context.py:stamp_cache_generation
 * PoP: w26_context_00d980_constants @ persistence/sqlite/workers/_context.py:<payload vocabulary>
 *
 * The six closed operations are `set(payload)` gates over the transaction
 * connection; the connection, the holder and the session row are seams. What is
 * ported is the gate itself, plus `stamp_cache_generation`, whose only foreign
 * input is the durable archived-row count (a fixed input here).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_context_00d980.h"

/* SIDECAR_KEYS: the model_config keys a worker may patch (kept sorted). */
static const char *const CTX_SIDECAR_KEYS[4] = {
    "_anthropic_rejected_thinking", "_proactive_prune_cold_credit",
    "_proactive_prune_rearm_tokens", "_usage_anchor",
};

/* Python truthiness of a JSON value. */
static int ctx_truthy(const json_t *v) {
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

/* Python len() of a str: code points, not bytes. */
static size_t ctx_strlen(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    for (; *s; s++) if (((unsigned char)*s & 0xC0) != 0x80) n++;
    return n;
}

/* Python set(mapping-or-sequence) membership: an object contributes its keys,
 * an array its elements, a null nothing. Only strings can match a key name. */
static int ctx_set_has(const json_t *p, const char *key) {
    size_t i, n;
    if (!p || p->type == JSON_NULL) return 0;
    if (p->type == JSON_OBJECT) {
        n = json_object_size(p);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(p, i);
            if (k && !strcmp(k, key)) return 1;
        }
        return 0;
    }
    if (p->type == JSON_ARRAY) {
        n = json_len(p);
        for (i = 0; i < n; i++) {
            const json_t *e = json_get(p, i);
            if (e && e->type == JSON_STRING && e->str_val && !strcmp(e->str_val, key)) return 1;
        }
        return 0;
    }
    return 0;
}

/* set(payload) == {key}: at least one member, and every member is `key`. */
static int ctx_set_equals1(const json_t *p, const char *key) {
    size_t i, n;
    if (!p || p->type == JSON_NULL) return 0;
    if (p->type == JSON_OBJECT) return json_object_size(p) == 1 && ctx_set_has(p, key);
    if (p->type == JSON_ARRAY) {
        n = json_len(p);
        if (n == 0) return 0;
        for (i = 0; i < n; i++) {
            const json_t *e = json_get(p, i);
            if (!e || e->type != JSON_STRING || !e->str_val) return 0;
            if (strcmp(e->str_val, key)) return 0;
        }
        return 1;
    }
    return 0;
}

/* set(payload) - allowed is empty: every member is one of `allowed`. */
static int ctx_set_subset(const json_t *p, const char *const *allowed, size_t na) {
    size_t i, j, n;
    if (!p || p->type == JSON_NULL) return 1;
    if (p->type == JSON_OBJECT) {
        n = json_object_size(p);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(p, i);
            int hit = 0;
            for (j = 0; j < na; j++) if (k && !strcmp(k, allowed[j])) { hit = 1; break; }
            if (!hit) return 0;
        }
        return 1;
    }
    if (p->type == JSON_ARRAY) {
        n = json_len(p);
        for (i = 0; i < n; i++) {
            const json_t *e = json_get(p, i);
            int hit = 0;
            if (!e || e->type != JSON_STRING || !e->str_val) return 0;
            for (j = 0; j < na; j++) if (!strcmp(e->str_val, allowed[j])) { hit = 1; break; }
            if (!hit) return 0;
        }
        return 1;
    }
    return 0;
}

/* An integral JSON number stands for a Python int (libjson keeps no int tag). */
static int ctx_is_int(const json_t *v) {
    return v && v->type == JSON_NUMBER;
}

static int ctx_str_or_null(const json_t *v) {
    return !v || v->type == JSON_NULL || v->type == JSON_STRING;
}

/* worker_tool_names: the tools[] pin, content-addressed later by the holder. */
static int ctx_pin_ok(const json_t *pin) {
    size_t i, n;
    if (!pin || pin->type == JSON_NULL) return 1;
    if (pin->type == JSON_ARRAY) {
        n = json_len(pin);
        for (i = 0; i < n; i++) {
            const json_t *e = json_get(pin, i);
            size_t len;
            if (!e || e->type != JSON_STRING) return 0;
            len = ctx_strlen(e->str_val);
            if (len == 0 || len > W26_CONTEXT_00D980_MAX_TOOL_NAME) return 0;
        }
        return 1;
    }
    if (pin->type == JSON_OBJECT) {
        const json_t *version = json_obj_get(pin, "version");
        const json_t *tools = json_obj_get(pin, "tools");
        const json_t *backend = json_obj_get(pin, "delegation_backend");
        static const char *const PIN_KEYS[3] = {"version", "tools", "delegation_backend"};
        size_t sz = json_object_size(pin);
        if (sz < 2) return 0;
        if (!ctx_set_subset(pin, PIN_KEYS, 3)) return 0;
        if (!ctx_set_has(pin, "version") || !ctx_set_has(pin, "tools")) return 0;
        if (!version || version->type != JSON_STRING) return 0;
        if (!tools || tools->type != JSON_ARRAY) return 0;
        if (backend && backend->type != JSON_NULL) {
            if (backend->type != JSON_STRING) return 0;
            if (ctx_strlen(backend->str_val) > W26_CONTEXT_00D980_MAX_BACKEND) return 0;
        }
        return 1;
    }
    return 0;
}

/* The encoded-size ceiling is a byte count over the pin's JSON encoding. */
static int ctx_pin_bounded(const json_t *pin) {
    char *s;
    size_t len;
    if (!pin || pin->type == JSON_NULL) return 1;
    s = json_serialize(pin);
    if (!s) return 0;
    len = strlen(s);
    free(s);
    return len <= (size_t)W26_CONTEXT_00D980_PAYLOAD_MAX_BYTES;
}

json_t *w26_context_00d980_validate(const json_t *req) {
    const char *op;
    const json_t *payload, *v;

    if (!req || req->type != JSON_OBJECT) return json_string("invalid_params");
    op = json_get_str(req, "op", "");
    payload = json_obj_get(req, "payload");
    if (!op) op = "";

    if (!strcmp(op, "worker_context") || !strcmp(op, "worker_tail_role")
        || !strcmp(op, "worker_finish")) {
        /* if payload: raise AuthorityRefusal('invalid_params') */
        return ctx_truthy(payload) ? json_string("invalid_params") : json_null();
    }
    if (!strcmp(op, "worker_prompt")) {
        if (!ctx_set_equals1(payload, "system_prompt")) return json_string("invalid_params");
        v = payload->type == JSON_OBJECT ? json_obj_get(payload, "system_prompt") : NULL;
        return ctx_str_or_null(v) ? json_null() : json_string("invalid_params");
    }
    if (!strcmp(op, "worker_sidecars")) {
        const json_t *patch;
        if (!ctx_set_equals1(payload, "patch")) return json_string("invalid_params");
        patch = payload->type == JSON_OBJECT ? json_obj_get(payload, "patch") : NULL;
        if (!patch || patch->type != JSON_OBJECT) return json_string("invalid_params");
        return ctx_set_subset(patch, CTX_SIDECAR_KEYS, 4) ? json_null()
                                                          : json_string("invalid_params");
    }
    if (!strcmp(op, "worker_tool_names")) {
        const json_t *pin;
        if (!ctx_set_equals1(payload, "tool_names")) return json_string("invalid_params");
        pin = payload->type == JSON_OBJECT ? json_obj_get(payload, "tool_names") : NULL;
        if (!ctx_pin_ok(pin)) return json_string("invalid_params");
        if (!ctx_pin_bounded(pin)) return json_string("invalid_params");
        return json_null();
    }
    if (!strcmp(op, "worker_row_role")) {
        if (!ctx_set_equals1(payload, "row_id")) return json_string("invalid_params");
        v = payload->type == JSON_OBJECT ? json_obj_get(payload, "row_id") : NULL;
        return ctx_is_int(v) ? json_null() : json_string("invalid_params");
    }
    return json_string("UNKNOWN_OP");
}

json_t *w26_context_00d980_stamp(const json_t *req) {
    const json_t *clock = req ? json_obj_get(req, "clock") : NULL;
    const char *text = (clock && clock->type == JSON_STRING) ? clock->str_val : NULL;
    double generation = req ? json_get_num(req, "generation", 0.0) : 0.0;
    json_t *out = json_object();
    json_t *event;

    if (!text) {
        json_set(out, "parsed", json_bool(false));
        json_set(out, "value", json_null());
        return out;
    }
    event = json_parse(text, NULL);
    if (!event || event->type != JSON_OBJECT) {
        if (event) json_free(event);
        json_set(out, "parsed", json_bool(false));
        json_set(out, "value", json_string(text));
        return out;
    }
    json_set(event, "generation", json_number((double)(long long)generation));
    json_set(out, "parsed", json_bool(true));
    json_set(out, "value", event);
    return out;
}

json_t *w26_context_00d980_constants(void) {
    json_t *o = json_object();
    json_t *keys = json_array();
    json_t *ops = json_array();
    size_t i;
    static const char *const OPS[7] = {
        "worker_context", "worker_finish", "worker_prompt", "worker_row_role",
        "worker_sidecars", "worker_tail_role", "worker_tool_names",
    };
    for (i = 0; i < 4; i++) json_append(keys, json_string(CTX_SIDECAR_KEYS[i]));
    for (i = 0; i < 7; i++) json_append(ops, json_string(OPS[i]));
    json_set(o, "sidecar_keys", keys);
    json_set(o, "operations", ops);
    json_set(o, "payload_max_bytes", json_number((double)W26_CONTEXT_00D980_PAYLOAD_MAX_BYTES));
    json_set(o, "max_tool_name", json_number((double)W26_CONTEXT_00D980_MAX_TOOL_NAME));
    json_set(o, "max_backend", json_number((double)W26_CONTEXT_00D980_MAX_BACKEND));
    return o;
}
