/*
 * session_usage.c -- faithful C11 port of the pure helpers in
 *                    gateway/session_usage.py.
 *
 * PoP: w8_sessionusag_c7f1bb_finite                  @ gateway/session_usage.py:_finite
 * PoP: w8_sessionusag_c7f1bb_carried_compression_count @ gateway/session_usage.py:carried_compression_count
 * PoP: w8_sessionusag_c7f1bb_stored_stats            @ gateway/session_usage.py:_stored_stats
 * PoP: w8_sessionusag_c7f1bb_tui_usage               @ gateway/session_usage.py:tui_usage
 * PoP: w8_sessionusag_c7f1bb_usage_frame             @ gateway/session_usage.py:usage_frame
 */
#include "noc_w8_sessionusag_c7f1bb.h"
#include "hermes_json.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *w8su_get(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

/* Python truthiness of a JSON value. */
static int w8su_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default: return 0;
    }
}

/* isinstance(v, int) -- bool is a subclass of int in Python. */
static int w8su_is_int(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) return 1;
    return v->type == JSON_NUMBER && v->num_val == floor(v->num_val);
}

/* isinstance(v, (int, float)) -- bool included. */
static int w8su_is_num(const json_t *v) {
    return v && (v->type == JSON_NUMBER || v->type == JSON_BOOL);
}

static double w8su_number(const json_t *v) {
    if (!v) return 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_STRING && v->str_val) return strtod(v->str_val, NULL);
    return 0.0;
}

/* int(x or 0): truncation of numbers, decimal parse of strings. */
static long long w8su_int_or0(const json_t *v) {
    if (!w8su_truthy(v)) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_STRING && v->str_val) return strtoll(v->str_val, NULL, 10);
    return 0;
}

/* Python round(float(v), n): printf uses round-half-even on the exact binary value. */
static double w8su_round_n(double v, int n) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", n, v);
    return strtod(buf, NULL);
}

/* `a or b` where b is a literal empty string. */
static json_t *w8su_or_str(const json_t *a) {
    return w8su_truthy(a) ? json_copy(a) : json_string("");
}

/* `a or b or 0` (numeric/value shaped). */
static json_t *w8su_or_num2(const json_t *a, const json_t *b) {
    if (w8su_truthy(a)) return json_copy(a);
    if (w8su_truthy(b)) return json_copy(b);
    return json_number(0);
}

struct json_t *w8_sessionusag_c7f1bb_finite(const struct json_t *value) {
    if (!value || value->type == JSON_NULL) return json_null();
    double v = w8su_number(value);
    if (!(v == v) || !(0.0 < v && v < 1000000.0)) return json_null();
    return json_number(w8su_round_n(v, 2));
}

struct json_t *w8_sessionusag_c7f1bb_carried_compression_count(const struct json_t *live,
        const struct json_t *agent) {
    const json_t *cv = w8su_get(live, "compression_count");
    long long carried = (w8su_is_int(cv)) ? w8su_int_or0(cv) : 0;
    const json_t *cc = w8su_get(agent, "context_compressor");
    const json_t *ov = w8su_get(cc, "compression_count");
    long long own = (w8su_is_int(ov)) ? w8su_int_or0(ov) : 0;
    return json_number((double)(carried > own ? carried : own));
}

struct json_t *w8_sessionusag_c7f1bb_stored_stats(const struct json_t *row) {
    static const char *const COUNTERS[] = {
        "input_tokens", "output_tokens", "cache_read_tokens", "cache_write_tokens", "reasoning_tokens"
    };
    json_t *out = json_object();
    long long vals[5];
    for (int i = 0; i < 5; i++) {
        vals[i] = w8su_int_or0(w8su_get(row, COUNTERS[i]));
        json_set(out, COUNTERS[i], json_number((double)vals[i]));
    }
    json_set(out, "prompt_tokens", json_number((double)(vals[0] + vals[2] + vals[3])));
    json_set(out, "completion_tokens", json_number((double)vals[1]));
    json_set(out, "total_tokens", json_number((double)(vals[0] + vals[1])));
    json_set(out, "api_calls", json_number((double)w8su_int_or0(w8su_get(row, "api_call_count"))));
    json_set(out, "model", w8su_or_str(w8su_get(row, "model")));
    return out;
}

struct json_t *w8_sessionusag_c7f1bb_tui_usage(const struct json_t *stats) {
    json_t *out = json_object();
    json_set(out, "model", w8su_or_str(w8su_get(stats, "model")));
    json_set(out, "input", w8su_or_num2(w8su_get(stats, "input_tokens"), w8su_get(stats, "prompt_tokens")));
    json_set(out, "output", w8su_or_num2(w8su_get(stats, "output_tokens"), w8su_get(stats, "completion_tokens")));
    json_set(out, "reasoning", w8su_or_num2(w8su_get(stats, "reasoning_tokens"), NULL));
    json_set(out, "prompt", w8su_or_num2(w8su_get(stats, "prompt_tokens"), NULL));
    json_set(out, "completion", w8su_or_num2(w8su_get(stats, "completion_tokens"), NULL));
    json_set(out, "total", w8su_or_num2(w8su_get(stats, "total_tokens"), NULL));
    json_set(out, "calls", w8su_or_num2(w8su_get(stats, "api_calls"), NULL));

    double used = w8su_number(w8su_get(stats, "last_prompt_tokens"));
    double maximum = w8su_number(w8su_get(stats, "context_length"));
    if (used && maximum) {
        if (used > maximum) used = maximum;
        const char *source = json_get_str(stats, "context_source", "");
        if (!source) source = "";
        json_set(out, "context_used", json_number(used));
        json_set(out, "context_max", json_number(maximum));
        double pct = w8su_round_n(used / maximum * 100.0, 0);
        if (pct < 0.0) pct = 0.0;
        if (pct > 100.0) pct = 100.0;
        json_set(out, "context_percent", json_number(pct));
        json_set(out, "context_source", json_string(source));
        json_set(out, "context_estimated", json_bool(strcmp(source, "provider_usage") != 0));
    }
    const json_t *csv = w8su_get(stats, "context_source");
    if (csv && csv->type != JSON_NULL) {
        json_set(out, "compressions", w8su_or_num2(w8su_get(stats, "compression_count"), NULL));
    }
    double prompt_total = w8su_number(w8su_get(stats, "prompt_tokens"));
    double cache_read = w8su_number(w8su_get(stats, "cache_read_tokens"));
    if (prompt_total > 0.0 && cache_read > 0.0) {
        double pct = w8su_round_n(cache_read / prompt_total * 100.0, 0);
        if (pct < 0.0) pct = 0.0;
        if (pct > 100.0) pct = 100.0;
        json_set(out, "cache_hit_pct", json_number(pct));
    }
    const json_t *lat = w8su_get(stats, "avg_latency_s");
    if (w8su_is_num(lat)) json_set(out, "avg_latency_s", json_number(w8su_round_n(w8su_number(lat), 1)));
    const json_t *tps = w8su_get(stats, "tokens_per_s");
    if (w8su_is_num(tps)) json_set(out, "avg_tps", json_number(w8su_round_n(w8su_number(tps), 1)));
    const json_t *act = w8su_get(stats, "active_subagents");
    if (w8su_is_int(act)) json_set(out, "active_subagents", json_copy(act));
    const json_t *pg = w8su_get(stats, "plan_gauge");
    if (pg && pg->type == JSON_OBJECT) json_set(out, "plan_gauge", json_copy(pg));
    return out;
}

struct json_t *w8_sessionusag_c7f1bb_usage_frame(const struct json_t *stats) {
    json_t *out = json_object();
    json_set(out, "usage", w8_sessionusag_c7f1bb_tui_usage(stats));
    size_t n = json_object_size(stats);
    for (size_t i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(stats, i);
        if (k) json_set(out, k, json_copy(json_object_get_at(stats, i)));
    }
    return out;
}
