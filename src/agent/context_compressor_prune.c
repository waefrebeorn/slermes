/*
 * context_compressor_prune.c — C port of upstream Hermes agent/context_compressor_prune.py.
 *
 * PoP: w18_contextcom_ac4e03_changed_rows @ agent/context_compressor_prune.py:_changed_rows
 * PoP: w18_contextcom_ac4e03_keep_running_turn @ agent/context_compressor_prune.py:_keep_running_turn
 * PoP: w18_contextcom_ac4e03_would_prune @ agent/context_compressor_prune.py:ProactivePruneMixin.would_proactively_prune
 * PoP: w18_contextcom_ac4e03_constants @ agent/context_compressor_prune.py:_COLD_CACHE_MIN_RECLAIM_TOKENS/prune_needs_session_store
 *
 * The store/DB-facing prune methods are deliberately out of scope: they need a
 * live persistence layer. The deterministic, state-free helpers are ported.
 */
#include "noc_w18_contextcom_ac4e03.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python value equality across JSON types (bool and number cross-compare). */
static bool json_equal(const json_t *a, const json_t *b) {
    if (!a || !b) return a == b;
    bool an = a->type == JSON_NUMBER || a->type == JSON_BOOL;
    bool bn = b->type == JSON_NUMBER || b->type == JSON_BOOL;
    if (an && bn) {
        double av = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_NULL:   return true;
        case JSON_STRING: return a->str_val && b->str_val && !strcmp(a->str_val, b->str_val);
        case JSON_ARRAY: {
            size_t n = json_len(a);
            if (n != json_len(b)) return false;
            for (size_t i = 0; i < n; i++) if (!json_equal(json_get(a, i), json_get(b, i))) return false;
            return true;
        }
        case JSON_OBJECT: {
            size_t n = json_object_size(a);
            if (n != json_object_size(b)) return false;
            for (size_t i = 0; i < n; i++) {
                const char *k = json_object_get_key_at(a, i);
                if (!k || !json_equal(json_obj_get(a, k), json_obj_get(b, k))) return false;
            }
            return true;
        }
        default: return false;
    }
}

static int changed_rows(const json_t *messages, const json_t *pruned) {
    if (!messages || messages->type != JSON_ARRAY || !pruned || pruned->type != JSON_ARRAY) return 0;
    size_t n = json_len(messages) < json_len(pruned) ? json_len(messages) : json_len(pruned);
    int count = 0;
    for (size_t i = 0; i < n; i++) {
        /* Distinct JSON nodes are always `is not`; only value inequality counts. */
        if (!json_equal(json_get(messages, i), json_get(pruned, i))) count++;
    }
    return count;
}

json_t *w18_contextcom_ac4e03_changed_rows(const json_t *messages, const json_t *pruned) {
    return json_number((double)changed_rows(messages, pruned));
}

json_t *w18_contextcom_ac4e03_keep_running_turn(const json_t *messages, const json_t *pruned,
                                                const json_t *turn_start) {
    bool has_msgs = messages && messages->type == JSON_ARRAY;
    bool has_pruned = pruned && pruned->type == JSON_ARRAY;
    size_t mlen = has_msgs ? json_len(messages) : 0;
    size_t plen = has_pruned ? json_len(pruned) : 0;

    if (!turn_start || turn_start->type == JSON_NULL) {
        json_t *out = json_object();
        json_set(out, "kept", has_pruned ? json_copy(pruned) : json_array());
        json_set(out, "restored", json_number(0));
        return out;
    }
    long long ts = -1;
    if (turn_start->type == JSON_NUMBER) ts = (long long)turn_start->num_val;
    else if (turn_start->type == JSON_BOOL) ts = turn_start->bool_val ? 1 : 0;  /* bool is an int in Python */
    bool valid = ts >= 0 && (size_t)ts < mlen;
    if (valid) {
        json_t *row = json_get(messages, (size_t)ts);
        const char *role = (row && row->type == JSON_OBJECT) ? json_get_str(row, "role", "") : "";
        if (strcmp(role, "user") != 0) valid = false;
    }
    if (!valid) return json_null();

    json_t *kept = json_array();
    for (size_t i = 0; i < (size_t)ts && i < plen; i++) json_append(kept, json_copy(json_get(pruned, i)));
    for (size_t i = (size_t)ts; i < mlen; i++) json_append(kept, json_copy(json_get(messages, i)));

    int restored = 0;
    for (size_t i = (size_t)ts; i < mlen && i < plen; i++)
        if (!json_equal(json_get(messages, i), json_get(pruned, i))) restored++;

    json_t *out = json_object();
    json_set(out, "kept", kept);
    json_set(out, "restored", json_number(restored));
    return out;
}

json_t *w18_contextcom_ac4e03_would_prune(const json_t *tokens, const json_t *current_tokens) {
    long long t = 0;
    if (tokens && (tokens->type == JSON_NUMBER || tokens->type == JSON_BOOL))
        t = tokens->type == JSON_BOOL ? (tokens->bool_val ? 1 : 0) : (long long)tokens->num_val;
    bool cur_none = !current_tokens || current_tokens->type == JSON_NULL;
    double cur = (current_tokens && current_tokens->type == JSON_NUMBER) ? current_tokens->num_val : 0.0;
    return json_bool(t > 0 && (cur_none || cur >= (double)t));
}

json_t *w18_contextcom_ac4e03_constants(void) {
    json_t *out = json_object();
    json_set(out, "_COLD_CACHE_MIN_RECLAIM_TOKENS", json_number(16000));
    json_set(out, "prune_needs_session_store", json_bool(true));
    return out;
}
