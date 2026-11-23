/*
 * compression_automation.c — C home for upstream Hermes agent/compression_automation.py.
 *
 * PoP: w15_compression_8bc2d2_carry @ agent/compression_automation.py:carry_compression_automation
 *
 * The Python function is pure orchestration (returns None); its observable
 * behavior is the ordered sequence of carry and migrate calls. The C port emits
 * that exact sequence, including the reason='compression' kwargs on the two
 * callers that pass it and the early return for a RuntimeSessionStore owner.
 */
#include "noc_w15_compression_8bc2d2.h"
#include "hermes_json.h"

#include <string.h>

static void add_call(json_t *arr, const char *fn, const char *old,
                     const char *new, const char *reason) {
    json_t *o = json_object();
    json_set(o, "fn", json_string(fn));
    json_t *a = json_array();
    json_append(a, json_string(old ? old : ""));
    json_append(a, json_string(new ? new : ""));
    json_set(o, "args", a);
    if (reason) {
        json_t *kw = json_object();
        json_set(kw, "reason", json_string(reason));
        json_set(o, "kwargs", kw);
    }
    json_append(arr, o);
}

json_t *w15_compression_8bc2d2_carry(const char *old_session_id,
                                     const char *new_session_id,
                                     int is_runtime_store) {
    const char *old = old_session_id ? old_session_id : "";
    const char *nw = new_session_id ? new_session_id : "";
    json_t *arr = json_array();
    /* agent.bot_chain, agent.prompt_cache_lifecycle(.tier) — always. */
    add_call(arr, "carry_chain", old, nw, NULL);
    add_call(arr, "carry_missed_route", old, nw, NULL);
    add_call(arr, "carry_cache_tier", old, nw, NULL);
    /* Owner moved the sidecars atomically: nothing else to migrate. */
    if (is_runtime_store) return arr;
    /* The three guarded migrations, each with its own _swallow scope. */
    add_call(arr, "migrate_goal_to_session", old, nw, "compression");
    add_call(arr, "migrate_heartbeat_to_session", old, nw, NULL);
    add_call(arr, "migrate_loop_to_session", old, nw, "compression");
    return arr;
}
