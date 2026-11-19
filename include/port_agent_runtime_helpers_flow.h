/* port_agent_runtime_helpers_flow.h — FLOW-core port of agent/agent_runtime_helpers.py.
 *
 * Mission: match the program's FLOW (decision sequences, call order, branch
 * conditions, state transitions, error paths) of the runtime-helper
 * orchestration: the think/trajectory strip+extract chains, content
 * flatten/merge shape dispatch, duration/reset parsing and the entitlement
 * recovery gate. File/clock/process I/O is INJECTED through seams (a generic
 * callback dispatcher + json_t state), never performed inside the ported
 * decision core.
 *
 * Seam model
 * ----------
 * Every live external call the ported flows make goes through
 * `arhf_io_t.call(io, name, args)`. The dispatcher:
 *   - appends {"name": name, "args": args} to io->trace (ordered JSON array)
 *     so the decision SEQUENCE can be compared against live Python under
 *     scripted I/O;
 *   - returns the scripted result (a NEW json_t* the caller frees);
 *   - models a raised exception with {"__raise__": "<Type>", "message": "..."}.
 *
 * Live counterpart of each seam name:
 *   convert_scratchpad_to_think   agent.trajectory.convert_scratchpad_to_think
 *   logger.warning                module logger.warning (lazy %s formatting:
 *                                 args {"fmt": <fmt>, "args": [<values>]})
 *   parse_retry_after_seconds     agent.retry_utils.parse_retry_after_seconds
 *   _parse_absolute_timestamp     agent.credential_pool._parse_absolute_timestamp
 *   time.time                     time.time()
 *   agent._is_entitlement_failure agent._is_entitlement_failure(error_context,
 *                                 status_code)
 *
 * Data model / representation notes
 * ---------------------------------
 * - _UNMERGEABLE (a live object() sentinel) is modeled {"__unmergeable__": true}.
 * - Non-finite floats are modeled {"__float__": "inf"|"-inf"|"nan"} on BOTH
 *   sides (JSON has no inf/nan literals).
 * - Python str() of scalars: None->"None", True->"True", False->"False",
 *   integral floats/ints < 1e16 -> decimal, other floats -> shortest
 *   round-trip repr. Container repr() values are OUT of fixture scope for
 *   _flatten_content_text's dict branch and _is_entitlement_403's haystack
 *   (documented in the oracle).
 * - Embedded json.dumps(..., ensure_ascii=False) output (trajectory chains)
 *   is reproduced byte-exact: insertion-order keys, ", "/": " separators,
 *   Python escape set, integral doubles < 1e16 as integers. Fixture JSON
 *   number literals avoid integral-float forms ("1.0") and leading zeros:
 *   libjson parses every number as a double (documented exception).
 * - str.strip()/rstrip()/lower() are ASCII-scoped in the port (whitespace set
 *   space \t \n \r \v \f \x1c-\x1f); fixtures stay ASCII (documented).
 * - _duration_string_seconds's regex surface is small ((\d+(?:\.\d+)?)(ms|h|m|s)
 *   plus a float() pre-pass) and is hand-transcribed as a tiling scanner;
 *   fixtures use ASCII digits only (Python \d is unicode-wide — documented).
 *
 * Ownership: every arhf_* function returns a NEW json_t* (caller frees).
 * Args passed to arhf_call are borrowed (the dispatcher copies what it
 * records). Inputs are borrowed. State objects (context) are mutated in
 * place by the *_set_reset_* ports; the harness owns them.
 */
#ifndef PORT_AGENT_RUNTIME_HELPERS_FLOW_H
#define PORT_AGENT_RUNTIME_HELPERS_FLOW_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct arhf_io_t {
    /* Dispatch a live call by name. Returns a NEW json_t* (caller frees).
     * A result object carrying "__raise__" models a raised exception. */
    json_t *(*call)(struct arhf_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of {"name","args"}: ordered seam sequence */
    void   *ctx;     /* harness backend (scripted results) */
} arhf_io_t;

/* Dispatch a seam call: records {"name","args"} on io->trace, then calls
 * io->call (NULL call => json_null()). args may be NULL (recorded as {}). */
json_t *arhf_call(arhf_io_t *io, const char *name, json_t *args);
/* True when a seam result models a raised exception. */
bool arhf_raised(const json_t *result);

/* Uniform port contract: json in -> json out (NEW node, caller frees). */
json_t *arhf_with_think_block(arhf_io_t *io, json_t *in);
json_t *arhf_flatten_content_text(arhf_io_t *io, json_t *in);
json_t *arhf_merge_user_content(arhf_io_t *io, json_t *in);
json_t *arhf_trajectory_gpt_prefix(arhf_io_t *io, json_t *in);
json_t *arhf_trajectory_tool_call_turn(arhf_io_t *io, json_t *in);
json_t *arhf_trajectory_tool_responses(arhf_io_t *io, json_t *in);
json_t *arhf_duration_string_seconds(arhf_io_t *io, json_t *in);
/* State-mutating ports: in.context is mutated in place (reset_at). */
json_t *arhf_set_reset_from_retry_after(arhf_io_t *io, json_t *in);
json_t *arhf_set_reset_from_vendor_headers(arhf_io_t *io, json_t *in);
json_t *arhf_is_entitlement_403(arhf_io_t *io, json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* PORT_AGENT_RUNTIME_HELPERS_FLOW_H */
