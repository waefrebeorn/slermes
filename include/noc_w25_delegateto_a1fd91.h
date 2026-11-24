/*
 * noc_w25_delegateto_a1fd91.h - C surface for upstream
 * tools/delegate_tool_prefix_gate.py (gate w25_delegateto_a1fd91).
 *
 * The prompt-cache warm-up gate for a batch of sibling subagents.  The
 * threading.Event, the monotonic clock and the poll loop are seams: what is
 * ported is every decision the gate makes from the children's own values --
 * whether a batch is gated at all, which sibling follows the leader's route,
 * and whether the leader's first response has started yet.
 */
#ifndef NOC_W25_DELEGATETO_A1FD91_H
#define NOC_W25_DELEGATETO_A1FD91_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _route_key's attribute tuple, in order. */
#define W25_DELEGATETO_A1FD91_ROUTE_ATTRS 4

/* The activity string that marks the first streamed chunk. */
#define W25_DELEGATETO_A1FD91_FIRST_CHUNK "receiving stream response"

/* child: {"provider","base_url","model","api_mode"} ->
 * [provider, base_url, model, api_mode], null where the attribute is absent. */
json_t *w25_delegateto_a1fd91_route_key(const json_t *child);

/* child -> getattr(child, "_use_prompt_caching", False) is True (identity). */
json_t *w25_delegateto_a1fd91_caches_prompts(const json_t *child);

/* child -> (session_api_calls or 0) >= 1 or _last_activity_desc == the chunk
 * activity string. */
json_t *w25_delegateto_a1fd91_first_response_started(const json_t *child);

/* children: [child, ...]; max_wait_s: number ->
 * {"gate": bool}.  A gate exists only when the wait is positive, there are at
 * least two children and the leader (children[0]) caches prompts. */
json_t *w25_delegateto_a1fd91_for_children(const json_t *children, const json_t *max_wait_s);

/* request: {"leader": child, "child": child, "same": bool} ->
 * {"follows": bool}: the child is a distinct object, caches prompts and
 * shares the leader's route key. */
json_t *w25_delegateto_a1fd91_follows(const json_t *request);

/* The module's literals: the activity marker and the poll interval. */
json_t *w25_delegateto_a1fd91_constants(void);

#ifdef __cplusplus
}
#endif
#endif
