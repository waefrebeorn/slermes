#ifndef NOC_W13_TURNPROACT_A1162E_H
#define NOC_W13_TURNPROACT_A1162E_H

#include "hermes_json.h"

/* PoP port of agent/turn_proactive_prune.py (gate w13_turnproact_a1162e). */

/* route_change_offload() gating: -> {"action":str, "messages":list}.
 * The downstream _proactive_tool_result_prune result is supplied as an explicit
 * seam (`pruned_messages`) so the module's own gate decisions are what is tested. */
json_t *w13_turnproact_a1162e_route_offload(json_t *window_reason, json_t *compression_enabled,
        json_t *has_previous, json_t *same_clock, json_t *route, json_t *prev_route,
        json_t *messages, json_t *pruned_messages);

/* offload_cold_history() -> list (same prune seam + length gate). */
json_t *w13_turnproact_a1162e_offload_cold(json_t *messages, json_t *compression_enabled,
        json_t *cache_cold, json_t *pruned_messages, json_t *pruned_same);

#endif /* NOC_W13_TURNPROACT_A1162E_H */
