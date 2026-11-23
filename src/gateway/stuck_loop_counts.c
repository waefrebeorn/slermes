/*
 * stuck_loop_counts.c — C port of upstream Hermes gateway/stuck_loop_counts.py.
 *
 * PoP: w12_stuckloop_9d2c5d_increment @ gateway/stuck_loop_counts.py:increment_restart_failure_counts
 * PoP: w12_stuckloop_9d2c5d_clear @ gateway/stuck_loop_counts.py:clear_restart_failure_count
 *
 * The Python functions wrap a read/mutate/write under _COUNTS_LOCK. The pure,
 * deterministic core (the dict comprehension and the delete/empty decision) is
 * exposed here; the file I/O and lock are the caller's responsibility, exactly
 * as the ORACLE injects an in-memory `utils` for them.
 */
#include "noc_w12_stuckloop_9d2c5d.h"
#include "hermes_json.h"

json_t *w12_stuckloop_9d2c5d_increment(const json_t *counts, const json_t *keys) {
    json_t *out = json_object();
    if (!json_is_array(keys)) return out;
    for (size_t i = 0; i < json_len(keys); i++) {
        json_t *k = json_get(keys, i);
        if (!k || k->type != JSON_STRING) continue;
        const char *key = k->str_val;
        if (json_obj_get(out, key)) continue;   /* active_session_keys is a set */
        json_t *old = json_is_object(counts) ? json_obj_get(counts, key) : NULL;
        double base = (old && old->type == JSON_NUMBER) ? old->num_val : 0.0;
        json_set(out, key, json_number(base + 1.0));
    }
    return out;
}

json_t *w12_stuckloop_9d2c5d_clear(const json_t *counts, const char *session_key) {
    json_t *result = json_object();
    json_t *new_counts = json_is_object(counts) ? json_copy(counts) : json_object();
    int changed = (session_key && session_key[0]) && json_obj_get(new_counts, session_key) != NULL;
    if (changed) json_obj_del(new_counts, session_key);
    int unlink = changed && (json_object_size(new_counts) == 0);
    json_set(result, "counts", new_counts);          /* transferred */
    json_set(result, "changed", json_bool(changed ? true : false));
    json_set(result, "unlink", json_bool(unlink ? true : false));
    return result;
}
