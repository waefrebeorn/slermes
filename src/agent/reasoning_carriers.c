/*
 * reasoning_carriers.c — C home for upstream Hermes agent/reasoning_carriers.py.
 *
 * Faithful C11 ports of provider-native assistant reasoning carriers: Gemini
 * thought-signature extraction, carrier-record capture, tool-call signature
 * stripping, replay-plan inference and wire shaping.  Provider-profile key
 * declarations and replay hook answers are injected through the args object.
 *
 * PoP: w10_reasoningc_80bddd_thought_signature @ agent/reasoning_carriers.py:thought_signature
 * PoP: w10_reasoningc_80bddd_carrier_record @ agent/reasoning_carriers.py:carrier_record
 * PoP: w10_reasoningc_80bddd_strip_function_signature @ agent/reasoning_carriers.py:_strip_function_signature
 * PoP: w10_reasoningc_80bddd_replay_plan @ agent/reasoning_carriers.py:_replay_plan
 * PoP: w10_reasoningc_80bddd_shape @ agent/reasoning_carriers.py:_shape
 */
#include "noc_w10_reasoningc_80bddd.h"

#include <string.h>

#define CARRIER_TYPE "hermes.native_assistant"
#define GEMINI_KEY "extra_content"

static char *py_strip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && (s[a]==' '||s[a]=='\t'||s[a]=='\n'||s[a]=='\r'||s[a]=='\f'||s[a]=='\v')) a++;
    while (b > a && (s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\n'||s[b-1]=='\r'||s[b-1]=='\f'||s[b-1]=='\v')) b--;
    char *r = (char *)malloc(b - a + 1);
    if (!r) return NULL;
    memcpy(r, s + a, b - a); r[b - a] = '\0'; return r;
}
static int json_str_arr_has(json_t *arr, const char *s) {
    if (!json_is_array(arr)) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *v = json_get(arr, i);
        if (json_is_string(v) && !strcmp(json_string_value(v), s)) return 1;
    }
    return 0;
}
static int json_val_eq(json_t *x, json_t *y) {
    if (!x || !y) return x == y;
    if (x->type != y->type) return 0;
    switch (x->type) {
        case JSON_NULL: return 1;
        case JSON_BOOL: return x->bool_val == y->bool_val;
        case JSON_NUMBER: return x->num_val == y->num_val;
        case JSON_STRING: return !strcmp(x->str_val, y->str_val);
        default: {
            char *a = json_dumps(x, 0), *b = json_dumps(y, 0);
            int r = a && b && !strcmp(a, b);
            free(a); free(b); return r;
        }
    }
}
static int json_truthy(json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

static json_t *field(json_t *obj, const char *name)
{
    if (!json_is_object(obj)) return NULL;
    json_t *v = json_obj_get(obj, name);
    if (v && !json_is_null(v)) return v;
    const char *sides[] = { "model_extra", "provider_data" };
    for (int i = 0; i < 2; i++) {
        json_t *side = json_obj_get(obj, sides[i]);
        if (json_is_object(side)) {
            json_t *x = json_obj_get(side, name);
            if (x && !json_is_null(x)) return x;
        }
    }
    return NULL;
}

static json_t *signature_of(json_t *extra)
{
    if (!json_is_object(extra)) return NULL;
    json_t *google = json_obj_get(extra, "google");
    json_t *sig = json_is_object(google)
        ? json_obj_get(google, "thought_signature")
        : json_obj_get(extra, "thought_signature");
    if (!json_is_string(sig)) return NULL;
    char *st = py_strip(json_string_value(sig));
    int nonblank = st && *st;
    free(st);
    return nonblank ? json_copy(sig) : NULL;
}

/* PoP: w10_reasoningc_80bddd_thought_signature @ agent/reasoning_carriers.py:thought_signature */
json_t *w10_reasoningc_80bddd_thought_signature(json_t *a)
{
    json_t *sig = signature_of(a ? json_obj_get(a, "extra") : NULL);
    return sig ? sig : json_null();
}

/* PoP: w10_reasoningc_80bddd_carrier_record @ agent/reasoning_carriers.py:carrier_record */
json_t *w10_reasoningc_80bddd_carrier_record(json_t *a)
{
    json_t *msg = a ? json_obj_get(a, "msg") : NULL;
    json_t *record = json_object();
    json_t *sig = signature_of(field(msg, GEMINI_KEY));
    if (sig) {
        json_t *google = json_object();
        json_set(google, "thought_signature", sig);
        json_t *inner = json_object();
        json_set(inner, "google", google);
        json_set(record, GEMINI_KEY, inner);
    }
    json_t *seen = json_array();
    const char *groups[] = { "opaque_keys", "text_keys" };
    for (int g = 0; g < 2; g++) {
        json_t *keys = a ? json_obj_get(a, groups[g]) : NULL;
        if (!json_is_array(keys)) continue;
        for (size_t i = 0; i < json_len(keys); i++) {
            json_t *kv = json_get(keys, i);
            if (!json_is_string(kv)) continue;
            const char *key = json_string_value(kv);
            if (json_str_arr_has(seen, key)) continue;
            json_append(seen, json_copy(kv));
            json_t *v = field(msg, key);
            if (json_is_string(v) && json_string_value(v) && json_string_value(v)[0])
                json_set(record, key, json_copy(v));
        }
    }
    json_free(seen);
    if (!json_object_size(record)) { json_free(record); return json_null(); }
    json_t *out = json_object();
    json_set(out, "type", json_string(CARRIER_TYPE));
    for (size_t i = 0; i < json_object_size(record); i++) {
        const char *k = json_object_get_key_at(record, i);
        if (k) json_set(out, k, json_copy(json_object_get_at(record, i)));
    }
    json_free(record);
    return out;
}

static json_t *strip_one(json_t *tc)
{
    if (!json_is_object(tc)) return json_copy(tc);
    json_t *fn = json_obj_get(tc, "function");
    if (!json_is_object(fn) || !json_has(fn, "thought_signature")) return json_copy(tc);
    json_t *newtc = json_object();
    for (size_t i = 0; i < json_object_size(tc); i++) {
        const char *k = json_object_get_key_at(tc, i);
        if (!k) continue;
        if (!strcmp(k, "function")) {
            json_t *newfn = json_object();
            for (size_t j = 0; j < json_object_size(fn); j++) {
                const char *fk = json_object_get_key_at(fn, j);
                if (fk && strcmp(fk, "thought_signature"))
                    json_set(newfn, fk, json_copy(json_object_get_at(fn, j)));
            }
            json_set(newtc, k, newfn);
        } else {
            json_set(newtc, k, json_copy(json_object_get_at(tc, i)));
        }
    }
    return newtc;
}

/* PoP: w10_reasoningc_80bddd_strip_function_signature @ agent/reasoning_carriers.py:_strip_function_signature */
json_t *w10_reasoningc_80bddd_strip_function_signature(json_t *a)
{
    json_t *tcs = a ? json_obj_get(a, "tool_calls") : NULL;
    json_t *out = json_array();
    if (!json_is_array(tcs)) return out;
    for (size_t i = 0; i < json_len(tcs); i++) json_append(out, strip_one(json_get(tcs, i)));
    return out;
}

/* PoP: w10_reasoningc_80bddd_replay_plan @ agent/reasoning_carriers.py:_replay_plan */
json_t *w10_reasoningc_80bddd_replay_plan(json_t *a)
{
    json_t *answer = a ? json_obj_get(a, "answer") : NULL;
    json_t *values = a ? json_obj_get(a, "values") : NULL;
    if (!json_is_object(answer)) return json_string("rewrites");
    json_t *plan = json_array();
    for (size_t i = 0; i < json_object_size(answer); i++) {
        const char *out_key = json_object_get_key_at(answer, i);
        json_t *out_val = json_object_get_at(answer, i);
        const char *source = NULL;
        int sources = 0;
        if (json_is_object(values)) {
            for (size_t j = 0; j < json_object_size(values); j++) {
                if (json_val_eq(json_object_get_at(values, j), out_val)) {
                    source = json_object_get_key_at(values, j);
                    sources++;
                }
            }
        }
        if (!sources) { json_free(plan); return json_string("rewrites"); }
        if (sources > 1) { json_free(plan); return json_null(); }
        json_t *pair = json_array();
        json_append(pair, json_string(out_key));
        json_append(pair, json_string(source));
        json_append(plan, pair);
    }
    return plan;
}

/* PoP: w10_reasoningc_80bddd_shape @ agent/reasoning_carriers.py:_shape */
json_t *w10_reasoningc_80bddd_shape(json_t *a)
{
    json_t *msg = a ? json_obj_get(a, "msg") : NULL;
    if (!json_is_object(msg)) return json_copy(msg ? msg : json_null());
    json_t *role = json_obj_get(msg, "role");
    if (!json_is_string(role) || strcmp(json_string_value(role), "assistant")) return json_copy(msg);

    json_t *route = json_obj_get(a, "route");
    int gemini = json_get_bool(route, "gemini", false) ? 1 : 0;
    int keep_fn_sig = json_get_bool(route, "keep_fn_sig", false) ? 1 : 0;
    int has_profile = json_get_bool(route, "profile", false) ? 1 : 0;
    json_t *keys = json_obj_get(route, "keys");

    json_t *details = json_obj_get(msg, "reasoning_details");
    if (!json_is_array(details)) details = NULL;
    size_t record_idx = 0;
    json_t *record = NULL;
    if (details) {
        for (size_t i = 0; i < json_len(details); i++) {
            json_t *d = json_get(details, i);
            json_t *ty = json_is_object(d) ? json_obj_get(d, "type") : NULL;
            if (json_is_string(ty) && !strcmp(json_string_value(ty), CARRIER_TYPE)) {
                record = d; record_idx = i; break;
            }
        }
    }
    json_t *tool_calls = json_obj_get(msg, "tool_calls");
    if (!json_is_array(tool_calls)) tool_calls = NULL;
    int fn_sig = 0;
    if (!keep_fn_sig && tool_calls) {
        for (size_t i = 0; i < json_len(tool_calls); i++) {
            json_t *tc = json_get(tool_calls, i);
            json_t *fn = json_is_object(tc) ? json_obj_get(tc, "function") : NULL;
            if (json_is_object(fn) && json_has(fn, "thought_signature")) { fn_sig = 1; break; }
        }
    }

    json_t *eff = json_array();
    if (json_is_array(keys)) for (size_t i = 0; i < json_len(keys); i++) json_append(eff, json_copy(json_get(keys, i)));
    if (record) {
        for (size_t i = 0; i < json_object_size(record); i++) {
            const char *k = json_object_get_key_at(record, i);
            if (k && strcmp(k, "type") && !json_str_arr_has(eff, k)) json_append(eff, json_string(k));
        }
    }
    if (!record && !fn_sig) {
        int any = 0;
        for (size_t i = 0; i < json_len(eff); i++) {
            const char *k = json_string_value(json_get(eff, i));
            if (k && json_has(msg, k)) { any = 1; break; }
        }
        if (!any) { json_free(eff); return json_copy(msg); }
    }

    json_t *out = json_object();
    for (size_t i = 0; i < json_object_size(msg); i++) {
        const char *k = json_object_get_key_at(msg, i);
        if (k && !json_str_arr_has(eff, k)) json_set(out, k, json_copy(json_object_get_at(msg, i)));
    }
    json_t *values = json_object();
    if (record) for (size_t i = 0; i < json_object_size(record); i++) {
        const char *k = json_object_get_key_at(record, i);
        if (k) json_set(values, k, json_copy(json_object_get_at(record, i)));
    }
    for (size_t i = 0; i < json_len(eff); i++) {
        const char *k = json_string_value(json_get(eff, i));
        if (k && json_has(msg, k)) json_set(values, k, json_copy(json_obj_get(msg, k)));
    }
    json_free(eff);

    if (record && details) {
        json_t *rest = json_array();
        for (size_t i = 0; i < json_len(details); i++)
            if (i != record_idx) json_append(rest, json_copy(json_get(details, i)));
        if (json_len(rest)) json_set(out, "reasoning_details", rest);
        else { json_free(rest); json_obj_del(out, "reasoning_details"); }
    }
    json_t *gem = json_obj_get(values, GEMINI_KEY);
    if (gemini && json_truthy(gem)) json_set(out, GEMINI_KEY, json_copy(gem));
    if (has_profile) {
        json_t *replay = a ? json_obj_get(a, "replay_result") : NULL;
        if (json_is_object(replay)) for (size_t i = 0; i < json_object_size(replay); i++) {
            const char *k = json_object_get_key_at(replay, i);
            if (k) json_set(out, k, json_copy(json_object_get_at(replay, i)));
        }
    }
    if (fn_sig && tool_calls) {
        json_t *stripped = json_array();
        for (size_t i = 0; i < json_len(tool_calls); i++) json_append(stripped, strip_one(json_get(tool_calls, i)));
        json_set(out, "tool_calls", stripped);
    }
    json_free(values);
    return out;
}
