/*
 * results.c — faithful C11 port of upstream Hermes session_rules/results.py.
 * PoP: w8_results_38308c_reanchor @ session_rules/results.py:reanchor_current_turn_user_idx
 * PoP: w8_results_38308c_compact @ session_rules/results.py:compact_result
 * The user-originated-turn projection needed by the anchor search is inlined
 * (session_rules.turns.user_originated_turn_view) for the shapes the port covers.
 */
#include "hermes_json.h"
#include "noc_w8_results_38308c.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int jtruthy(const struct json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    return json_len(v) > 0 || json_object_size(v) > 0;
}
static int jeq(const struct json_t *a, const struct json_t *b) {
    if (a == b) return 1;
    if (!a || !b) {
        /* a missing object key and an explicit JSON null both read as Python None */
        return (!a && b && b->type == JSON_NULL) || (!b && a && a->type == JSON_NULL);
    }
    if (a->type != b->type) return 0;
    switch (a->type) {
        case JSON_NULL: return 1;
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return !strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
        case JSON_ARRAY: {
            size_t na = json_len(a), nb = json_len(b);
            if (na != nb) return 0;
            for (size_t i = 0; i < na; i++) if (!jeq(json_get(a, i), json_get(b, i))) return 0;
            return 1;
        }
        case JSON_OBJECT: {
            size_t na = json_object_size(a), nb = json_object_size(b);
            if (na != nb) return 0;
            for (size_t i = 0; i < na; i++) {
                const char *k = json_object_get_key_at(a, i);
                if (!k) continue;
                if (!jeq(json_obj_get(a, k), json_obj_get(b, k))) return 0;
            }
            return 1;
        }
    }
    return 0;
}
/* Projection of a user row to its live view content. Returns 1 when a live
 * user-originated view exists and stores its content; mirrors the subset of
 * session_rules.turns.user_originated_turn_view relevant to reanchoring. */
static int live_view_content(const struct json_t *msg, const struct json_t **content_out) {
    if (!msg || msg->type != JSON_OBJECT) return 0;
    const struct json_t *role = json_obj_get(msg, "role");
    if (!role || role->type != JSON_STRING || strcmp(role->str_val ? role->str_val : "", "user")) return 0;
    if (jtruthy(json_obj_get(msg, "_compressed_summary"))) return 0; /* composite handoff: no live fallback */
    const struct json_t *dk = json_obj_get(msg, "display_kind");
    if (dk && dk->type == JSON_STRING && strcmp(dk->str_val ? dk->str_val : "", "steer")) return 0;
    const struct json_t *c = json_obj_get(msg, "content");
    if (!jtruthy(c)) return 0; /* empty/missing content is not a real user turn */
    *content_out = c;
    return 1;
}

static long long reanchor_idx(const struct json_t *messages, const struct json_t *user_message) {
    long long fallback = -1;
    if (!messages || messages->type != JSON_ARRAY) return fallback;
    size_t n = json_len(messages);
    for (size_t i = n; i-- > 0;) {
        const struct json_t *msg = json_get(messages, i);
        if (!msg || msg->type != JSON_OBJECT) continue;
        const struct json_t *role = json_obj_get(msg, "role");
        if (!role || role->type != JSON_STRING || strcmp(role->str_val ? role->str_val : "", "user")) continue;
        if (jeq(json_obj_get(msg, "content"), user_message)) return (long long)i;
        const struct json_t *live = NULL;
        if (live_view_content(msg, &live) && jeq(live, user_message)) return (long long)i;
        if (live && fallback < 0) fallback = (long long)i;
    }
    return fallback;
}

struct json_t *w8_results_38308c_reanchor(const struct json_t *messages, const struct json_t *user_message) {
    return json_number((double)reanchor_idx(messages, user_message));
}

struct json_t *w8_results_38308c_compact(const struct json_t *result, const struct json_t *user_message) {
    struct json_t *out = result ? json_copy(result) : json_object();
    if (!out || out->type != JSON_OBJECT) return out;
    struct json_t *value = (struct json_t *)json_obj_get(out, "result");
    if (!value || value->type != JSON_OBJECT) return out;
    struct json_t *nv = json_copy(value);
    json_set(out, "result", nv);
    value = nv;
    json_obj_del(value, "tools");
    struct json_t *messages = (struct json_t *)json_obj_get(value, "messages");
    if (messages && messages->type == JSON_ARRAY
            && !jtruthy(json_obj_get(value, "_compressed"))
            && !jtruthy(json_obj_get(value, "_messages_are_turn_suffix"))) {
        long long anchor = -2;
        const struct json_t *a = json_obj_get(value, "current_turn_user_idx");
        size_t n = json_len(messages);
        int integral = a && a->type == JSON_NUMBER && a->num_val == floor(a->num_val);
        if (integral && a->num_val >= 0 && a->num_val < (double)n) {
            const struct json_t *m = json_get(messages, (size_t)a->num_val);
            const struct json_t *r = m && m->type == JSON_OBJECT ? json_obj_get(m, "role") : NULL;
            if (r && r->type == JSON_STRING && !strcmp(r->str_val ? r->str_val : "", "user"))
                anchor = (long long)a->num_val;
        }
        if (anchor == -2) anchor = reanchor_idx(messages, user_message);
        long long start = anchor + 1;
        if (start < 0) start = 0;
        struct json_t *row = NULL;
        if (anchor >= 0 && (size_t)anchor < n) {
            const struct json_t *m = json_get(messages, (size_t)anchor);
            if (m && m->type == JSON_OBJECT && jtruthy(json_obj_get(m, "api_content"))) {
                row = json_object();
                static const char *const keys[] = { "role", "content", "api_content" };
                for (size_t k = 0; k < 3; k++) {
                    const struct json_t *x = json_obj_get(m, keys[k]);
                    if (x) json_set(row, keys[k], json_copy(x));
                }
            }
        }
        struct json_t *sliced = json_array();
        for (size_t i = (size_t)start; i < n; i++) json_append(sliced, json_copy(json_get(messages, i)));
        json_set(value, "messages", sliced); /* frees the old array: `messages` is now dangling */
        if (row) json_set(value, "_turn_user_row", row);
        json_set(value, "_messages_are_turn_suffix", json_bool(true));
        json_obj_del(value, "current_turn_user_idx");
    }
    return out;
}