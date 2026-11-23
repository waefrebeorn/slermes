#ifndef NOC_W6_RUNTRUN_6F520A_H
#define NOC_W6_RUNTRUN_6F520A_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_runturn_is_context_overflow @ gateway/run_turn.py:is_context_overflow_failure_result
 * Returns the boolean verdict of is_context_overflow_failure_result(result, history_len). */
int w6_runturn_is_context_overflow(const struct json_t *result, long long history_len);

/* PoP: w6_runturn_bound_model_input @ gateway/run_turn.py:bound_model_input_without_hygiene
 * Returns the fail-closed in-context bound (a JSON array, caller frees). */
struct json_t *w6_runturn_bound_model_input(const struct json_t *history, long long limit);

/* PoP: w6_runturn_hygiene_no_commit_reason @ gateway/run_turn.py:hygiene_no_commit_reason
 * `attrs` is a JSON object of the agent attributes the Python getattr()s read.
 * Returns the reason string (caller frees). */
char *w6_runturn_hygiene_no_commit_reason(const struct json_t *attrs);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W6_RUNTRUN_6F520A_H */
