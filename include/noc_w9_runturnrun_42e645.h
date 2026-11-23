#ifndef NOC_W9_RUNTRUNRUN_42E645_H
#define NOC_W9_RUNTRUNRUN_42E645_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_runturnrun_42e645_observed_persist_message @ gateway/run_turn_runner.py:_observed_turn_persist_message
 *
 * The durable user row for an observed-group turn.  A clean string override
 * wins; for a native image turn the pre-wrap content-part list is kept, and a
 * string override swaps the leading copy of the live text inside the first
 * text part.  Caller frees. */
struct json_t *w9_runturnrun_42e645_observed_persist_message(const struct json_t *run_message,
                                                             const struct json_t *override,
                                                             const struct json_t *live_text);

/* PoP: w9_runturnrun_42e645_merge_overrides @ gateway/run_turn_runner.py:TurnRunner._merge_turn_request_overrides
 *
 * Merge, never overwrite: drop the PREVIOUS turn's routing overrides from the
 * agent's init-time overrides, then layer this turn's overrides on top.
 * Returns {"overrides": <new request_overrides>,
 *          "prev": <new _gateway_turn_request_overrides>}.  Caller frees. */
struct json_t *w9_runturnrun_42e645_merge_overrides(const struct json_t *agent_overrides,
                                                    const struct json_t *prev_overrides,
                                                    const struct json_t *turn_overrides);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_RUNTRUNRUN_42E645_H */
