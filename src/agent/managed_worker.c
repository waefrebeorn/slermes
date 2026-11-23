/*
 * managed_worker.c — C port of upstream Hermes agent/managed_worker.py.
 *
 * PoP: w7_managedwork_1ac730_scalar @ agent/managed_worker.py:_scalar
 * PoP: w7_managedwork_1ac730_structured @ agent/managed_worker.py:_structured
 * PoP: w7_managedwork_1ac730_result_frame @ agent/managed_worker.py:result_frame
 * PoP: w7_managedwork_1ac730_accept_result @ agent/managed_worker.py:accept_result
 * PoP: w7_managedwork_1ac730_activity_interval @ agent/managed_worker.py:activity_interval
 * PoP: w7_managedwork_1ac730_outbox_dir @ agent/managed_worker.py:outbox_dir
 * PoP: w7_managedwork_1ac730_activity_frame_interval @ agent/managed_worker.py:activity_frame_interval
 */
#include "hermes_json.h"
#include "noc_w7_managedwork_1ac730.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define ACTIVITY_FRAME_SECONDS 30.0
#define BOUND_TEXT 4096

static const char *const RESULT_FIELDS[] = {
    "final_response", "failed", "interrupted", "completed", "partial", "error", "turn_exit_reason",
    "api_calls", "model", "provider", "prompt_tokens", "completion_tokens", "total_tokens",
    "cache_read_tokens", "cache_write_tokens", "reasoning_tokens", "estimated_cost_usd", "cost_status",
    "cost_source", "service_tier", "failure_reason", "session_id", "requested_model", "served_model",
    "_turn_input_tokens", "_turn_output_tokens", "failure_retryable", "failure_resets_at", "billing_block",
    "free_tier", "usage_window_message", "refused_turn_boundary_row_id"
};
static const char *const BILLING_FIELDS[] = {
    "provider", "provider_label", "model", "billing_url", "is_nous", "message", "unverified"
};
static const char *const FREE_FIELDS[] = {"kind", "message"};

static int in_list(const char *s, const char *const *list, size_t n) {
    if (!s) return 0;
    for (size_t i = 0; i < n; i++) if (!strcmp(s, list[i])) return 1;
    return 0;
}
static int is_result_field(const char *s) {
    return in_list(s, RESULT_FIELDS, sizeof(RESULT_FIELDS)/sizeof(RESULT_FIELDS[0]));
}
static int is_structured_key(const char *s) {
    return !strcmp(s, "billing_block") || !strcmp(s, "free_tier");
}
static const char *const *allowed_fields(const char *key, size_t *n) {
    if (!strcmp(key, "billing_block")) { *n = sizeof(BILLING_FIELDS)/sizeof(BILLING_FIELDS[0]); return BILLING_FIELDS; }
    *n = sizeof(FREE_FIELDS)/sizeof(FREE_FIELDS[0]); return FREE_FIELDS;
}

/* Python: value is None or type(value) in (bool, int, float) or (str and len<=4096). */
static int scalar_node(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_NULL || v->type == JSON_BOOL || v->type == JSON_NUMBER) return 1;
    if (v->type == JSON_STRING) return strlen(v->str_val) <= BOUND_TEXT;
    return 0;
}
struct json_t *w7_managedwork_1ac730_scalar(json_t *value) {
    return json_bool(scalar_node(value) ? true : false);
}

/* Python: isinstance(value, dict) and no unknown keys and all(_scalar(values)). */
static int structured_node(const char *key, const json_t *value) {
    if (!value || value->type != JSON_OBJECT || !is_structured_key(key)) return 0;
    size_t n; const char *const *allowed = allowed_fields(key, &n);
    for (size_t i = 0; i < json_object_size(value); i++) {
        const char *k = json_object_get_key_at(value, i);
        if (!in_list(k, allowed, n)) return 0;
        if (!scalar_node(json_object_get_at(value, i))) return 0;
    }
    return 1;
}
struct json_t *w7_managedwork_1ac730_structured(const char *key, json_t *value) {
    return json_bool(structured_node(key, value) ? true : false);
}

static json_t *bounded_copy(const json_t *v, int keep_full_text) {
    if (!v) return json_null();
    if (v->type == JSON_STRING && !keep_full_text && strlen(v->str_val) > BOUND_TEXT) {
        char *s = (char *)malloc(BOUND_TEXT + 1);
        memcpy(s, v->str_val, BOUND_TEXT); s[BOUND_TEXT] = '\0';
        json_t *r = json_string(s); free(s); return r;
    }
    return json_copy(v);
}

/* Worker side: receipt fields of the turn result, scalars only, text bounded. */
struct json_t *w7_managedwork_1ac730_result_frame(json_t *result) {
    json_t *frame = json_object();
    if (!result || result->type != JSON_OBJECT) return frame;
    for (size_t i = 0; i < json_object_size(result); i++) {
        const char *k = json_object_get_key_at(result, i);
        json_t *v = json_object_get_at(result, i);
        if (!k || !is_result_field(k) || is_structured_key(k)) continue;
        if (v && (v->type == JSON_NULL || v->type == JSON_BOOL || v->type == JSON_NUMBER || v->type == JSON_STRING))
            json_set(frame, k, bounded_copy(v, !strcmp(k, "final_response")));
    }
    static const char *const order[] = {"billing_block", "free_tier"};
    for (size_t s = 0; s < 2; s++) {
        const char *key = order[s];
        json_t *block = json_obj_get(result, key);
        if (!block || block->type != JSON_OBJECT) continue;
        size_t n; const char *const *fields = allowed_fields(key, &n);
        json_t *out = json_object();
        for (size_t i = 0; i < json_object_size(block); i++) {
            const char *k = json_object_get_key_at(block, i);
            json_t *v = json_object_get_at(block, i);
            if (!in_list(k, fields, n)) continue;
            if (v && (v->type == JSON_NULL || v->type == JSON_BOOL || v->type == JSON_NUMBER || v->type == JSON_STRING))
                json_set(out, k, bounded_copy(v, 0));
        }
        json_set(frame, key, out);
    }
    return frame;
}

static json_t *error_obj(void) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "error", json_string("invalid_worker_result"));
    return o;
}
static json_t *valid_result(json_t *accepted, json_t *usage) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "result", accepted ? accepted : json_object());
    json_set(o, "usage", usage ? usage : json_object());
    return o;
}

/* Owner side: validate the worker frame and project (result, usage). */
struct json_t *w7_managedwork_1ac730_accept_result(json_t *result) {
    if (!result || result->type != JSON_OBJECT) return error_obj();
    for (size_t i = 0; i < json_object_size(result); i++) {
        const char *k = json_object_get_key_at(result, i);
        if (!is_result_field(k)) return error_obj();
    }
    json_t *fr = json_obj_get(result, "final_response");
    if (!fr || fr->type != JSON_STRING) return error_obj();
    static const char *const sk[] = {"billing_block", "free_tier"};
    for (size_t s = 0; s < 2; s++) {
        json_t *v = json_obj_get(result, sk[s]);
        if (v && !structured_node(sk[s], v)) return error_obj();
    }
    for (size_t i = 0; i < json_object_size(result); i++) {
        const char *k = json_object_get_key_at(result, i);
        if (!strcmp(k, "final_response") || is_structured_key(k)) continue;
        if (!scalar_node(json_object_get_at(result, i))) return error_obj();
    }
    json_t *accepted = json_copy(result);
    for (size_t s = 0; s < 2; s++) {
        json_t *v = json_obj_get(accepted, sk[s]);
        if (!v || v->type == JSON_NULL) json_obj_del(accepted, sk[s]);
    }
    json_t *turn_in = json_obj_get(accepted, "_turn_input_tokens");
    json_t *turn_out = json_obj_get(accepted, "_turn_output_tokens");
    if ((turn_in && (turn_in->type != JSON_NUMBER || turn_in->num_val < 0 || floor(turn_in->num_val) != turn_in->num_val)) ||
        (turn_out && (turn_out->type != JSON_NUMBER || turn_out->num_val < 0 || floor(turn_out->num_val) != turn_out->num_val))) {
        json_free(accepted); return error_obj();
    }
    json_obj_del(accepted, "_turn_input_tokens");
    json_obj_del(accepted, "_turn_output_tokens");
    json_t *incoming = json_obj_get(accepted, "prompt_tokens");
    json_t *outgoing = json_obj_get(accepted, "completion_tokens");
    /* json_obj_del frees the removed node: snapshot values before deleting. */
    int has_in = incoming && incoming->type == JSON_NUMBER && incoming->num_val != 0;
    int has_out = outgoing && outgoing->type == JSON_NUMBER && outgoing->num_val != 0;
    double in_val = has_in ? incoming->num_val : 0;
    double out_val = has_out ? outgoing->num_val : 0;
    json_obj_del(accepted, "prompt_tokens");
    json_obj_del(accepted, "completion_tokens");
    if ((!incoming || incoming->type == JSON_NULL) && (!outgoing || outgoing->type == JSON_NULL))
        return valid_result(accepted, json_object());
    double iv = (turn_in && turn_in->type == JSON_NUMBER) ? turn_in->num_val : in_val;
    double ov = (turn_out && turn_out->type == JSON_NUMBER) ? turn_out->num_val : out_val;
    json_t *usage = json_object();
    json_set(usage, "input_tokens", json_number(iv));
    json_set(usage, "output_tokens", json_number(ov));
    json_set(usage, "total_tokens", json_number(iv + ov));
    double pub_in = in_val, pub_out = out_val;
    json_set(accepted, "input_tokens", json_number(pub_in));
    json_set(accepted, "output_tokens", json_number(pub_out));
    return valid_result(accepted, usage);
}

struct json_t *w7_managedwork_1ac730_activity_interval(double wedge_timeout) {
    if (wedge_timeout <= 0) return json_number(ACTIVITY_FRAME_SECONDS);
    double v = wedge_timeout / 4.0;
    if (v < 0.05) v = 0.05;
    if (v > ACTIVITY_FRAME_SECONDS) v = ACTIVITY_FRAME_SECONDS;
    return json_number(v);
}

/* execution_id is an admission key: ':' -> '-', then [A-Za-z0-9_-]{1,200}. */
struct json_t *w7_managedwork_1ac730_outbox_dir(const char *home, const char *execution_id) {
    char name[512];
    if (!execution_id) name[0] = '\0';
    else {
        size_t n = strlen(execution_id); if (n >= sizeof(name)) n = sizeof(name) - 1;
        for (size_t i = 0; i < n; i++) name[i] = execution_id[i] == ':' ? '-' : execution_id[i];
        name[n] = '\0';
    }
    size_t len = strlen(name);
    if (len < 1 || len > 200) return error_obj();
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)name[i];
        if (!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-')) return error_obj();
    }
    size_t hn = home ? strlen(home) : 0;
    char *path = (char *)malloc(hn + len + 64);
    size_t p = 0;
    for (size_t i = 0; i < hn; i++) { path[p++] = home[i] == '/' ? '\\' : home[i]; }
    while (p > 0 && path[p-1] == '\\') p--;
    const char *sep = "\\worker-outboxes\\";
    size_t sl = strlen(sep);
    memcpy(path + p, sep, sl); p += sl;
    memcpy(path + p, name, len); p += len; path[p] = '\0';
    json_t *r = json_string(path); free(path); return r;
}

/* Owner's hydrated request asks for activity frames with a positive interval. */
struct json_t *w7_managedwork_1ac730_activity_frame_interval(const char *request_json) {
    if (!request_json || !*request_json) return json_null();
    char *err = NULL;
    json_t *doc = json_parse(request_json, &err);
    if (!doc || doc->type != JSON_OBJECT) { json_free(doc); return json_null(); }
    json_t *asked = json_obj_get(doc, "activity_frames_v1");
    json_t *r = json_null();
    if (asked && asked->type == JSON_OBJECT) {
        json_t *iv = json_obj_get(asked, "interval");
        if (iv && iv->type == JSON_NUMBER && iv->num_val > 0) r = json_number(iv->num_val);
    }
    json_free(doc); return r;
}
