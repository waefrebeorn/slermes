/*
 * noc_w27_apiservero_1a4ac2_p1.h - C surface for the turn-verdict half of
 * gateway/platforms/api_server_openai_routes.py (gate w27_apiservero_1a4ac2_p1).
 *
 * The pure algebra behind an OpenAI-compatible turn's outcome: the
 * (completed, partial, failed, error) tuple a result dict implies, the
 * iteration-budget verdict, the finish_reason an SDK sees, the Responses
 * status string, the `hermes` extras block, the settled-record predicate and
 * the Idempotency-Key fingerprint key list.  Nothing here touches a request,
 * a store or a stream -- those stay in the caller.
 */
#ifndef NOC_W27_APISERVERO_1A4AC2_P1_H
#define NOC_W27_APISERVERO_1A4AC2_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _result_flags(result) -> [completed, partial, failed, error];
 * non-object results report [true, false, false, null]. */
json_t *w27_apiservero_1a4ac2_p1_result_flags(const json_t *result);

/* _budget_exhausted(result) -> true when turn_exit_reason starts with
 * "budget_exhausted" or "max_iterations_reached(". */
json_t *w27_apiservero_1a4ac2_p1_budget_exhausted(const json_t *result);

/* _finish_reason(...) -> "length" | "error" | "cancelled" | "stop".
 * has_agent_error stands in for `agent_error is not None`. */
const char *w27_apiservero_1a4ac2_p1_finish_reason(int completed, int is_partial, int is_failed,
                                                   const char *err_msg, int has_agent_error,
                                                   int budget_exhausted);

/* _response_status(result) -> "cancelled" | "failed" | "incomplete" | "completed". */
json_t *w27_apiservero_1a4ac2_p1_response_status(const json_t *result);

/* _hermes_extras(...) -> {completed, partial, failed, error, error_code}. */
json_t *w27_apiservero_1a4ac2_p1_hermes_extras(int completed, int is_partial, int is_failed,
                                               const json_t *err_msg, const char *finish_reason,
                                               int budget_exhausted);

/* _settled(record) -> true when the record carries a non-null "response". */
json_t *w27_apiservero_1a4ac2_p1_settled(const json_t *record);

/* _responses_fingerprint_keys(body) -> the identity key list, with "truncation"
 * only when the body sent it. */
json_t *w27_apiservero_1a4ac2_p1_fingerprint_keys(const json_t *body);

#ifdef __cplusplus
}
#endif
#endif
