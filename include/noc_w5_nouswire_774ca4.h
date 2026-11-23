#ifndef NOC_W5_NOUSWIRE_774CA4_H
#define NOC_W5_NOUSWIRE_774CA4_H

/*
 * Public C API for the port of agent/nous_wire.py (gate w5_nouswire_774ca4).
 * All exported symbols are nouswire_-prefixed to avoid collisions.
 *
 * The GMI native-wire flag (GMI_NATIVE_WIRE_CLEARED) is False in the live
 * source, so wire_for_upstream() always selects "chat_completions"; that is
 * mirrored exactly. maybe_switch takes resolved facts instead of mutating a
 * live agent (see below).
 */
#include "hermes_json.h"

/* Port of classify_upstream(response). `provider` and `rid` are the response's
 * `.provider` and `.id` (json string or null). Returns json string
 * "openrouter"/"gmi" or null (caller frees). */
json_t *nouswire_classify(const json_t *provider, const json_t *rid);

/* Port of wire_for_upstream(upstream). Returns json string (caller frees). */
json_t *nouswire_for_upstream(const json_t *upstream);

/*
 * Port of the decision in maybe_switch_wire_after_first_response().
 * `facts`: api_call_count (int), decided (bool), provider (str), model (str),
 * nous_wire_mode (str, the resolved _nous_anthropic_wire() value),
 * api_mode (str|null), response_provider (str|null), response_id (str|null).
 * Returns {"switched":bool, "decided":bool,
 *          "pending":[want, upstream] | null} (caller frees).
 */
json_t *nouswire_maybe_switch(const json_t *facts);

#endif /* NOC_W5_NOUSWIRE_774CA4_H */
