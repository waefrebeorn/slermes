/*
 * turn_proactive_prune.c — C home for upstream Hermes agent/turn_proactive_prune.py.
 * PoP: w13_turnproact_a1162e_route_offload @ agent/turn_proactive_prune.py:route_change_offload
 * PoP: w13_turnproact_a1162e_offload_cold @ agent/turn_proactive_prune.py:offload_cold_history
 *
 * The turn-boundary prune decisions are pure gates around an expensive prune
 * helper (agent.turn_preflight._proactive_tool_result_prune).  That helper's
 * result arrives as an explicit fixture seam; everything the module itself
 * decides — window reason, compression enable, per-route dedup identity check,
 * cache-cold check and the length-preserving commit gate — is ported here.
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include "noc_w13_turnproact_a1162e.h"

static int tpp_truthy(json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    return v->c.count > 0;
}
static int tpp_streq(json_t *a, json_t *b) {
    const char *x = (a && a->type == JSON_STRING) ? a->str_val : NULL;
    const char *y = (b && b->type == JSON_STRING) ? b->str_val : NULL;
    if (!x || !y) return 0;
    return !strcmp(x, y);
}
static json_t *tpp_copy_or_null(json_t *v) {
    return json_copy(v ? v : (json_t *)json_null());
}
static size_t tpp_len(json_t *v) {
    return (v && v->type == JSON_ARRAY) ? v->c.count : 0;
}

json_t *w13_turnproact_a1162e_route_offload(json_t *window_reason, json_t *compression_enabled,
        json_t *has_previous, json_t *same_clock, json_t *route, json_t *prev_route,
        json_t *messages, json_t *pruned_messages) {
    json_t *out = json_object();
    json_set(out, "action", json_string("fallthrough"));
    int win_ok = window_reason && window_reason->type == JSON_STRING && window_reason->str_val &&
                 !strcmp(window_reason->str_val, "route_changed");
    if (!win_ok || !tpp_truthy(compression_enabled)) {
        json_set(out, "messages", tpp_copy_or_null(messages));
        return out;
    }
    /* previous is not None and previous[0] is seen[0] and previous[1] == seen[1] */
    int dedup = tpp_truthy(has_previous) && tpp_truthy(same_clock) && tpp_streq(prev_route, route);
    json_set(out, "messages", dedup ? tpp_copy_or_null(messages) : tpp_copy_or_null(pruned_messages));
    return out;
}

json_t *w13_turnproact_a1162e_offload_cold(json_t *messages, json_t *compression_enabled,
        json_t *cache_cold, json_t *pruned_messages, json_t *pruned_same) {
    size_t n = tpp_len(messages);
    if (n == 0 || !tpp_truthy(compression_enabled) || !tpp_truthy(cache_cold))
        return tpp_copy_or_null(messages);
    /* `pruned is not messages` is scripted by `pruned_same`; the length gate is real. */
    if (!tpp_truthy(pruned_same) && tpp_len(pruned_messages) == n)
        return tpp_copy_or_null(pruned_messages);
    return tpp_copy_or_null(messages);
}
