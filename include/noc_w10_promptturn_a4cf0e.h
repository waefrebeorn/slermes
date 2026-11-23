#ifndef NOC_W10_PROMPTTURN_A4CF0E_H
#define NOC_W10_PROMPTTURN_A4CF0E_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_promptturn_a4cf0e_result_status @ tui_gateway/prompt_turn.py:_result_status
 * Returns one of "interrupted" / "error" / "complete" (static string; do not free). */
const char *w10_promptturn_a4cf0e_result_status(const struct json_t *result);
/* PoP: w10_promptturn_a4cf0e_is_successful_goal_turn @ tui_gateway/prompt_turn.py:_is_successful_goal_turn
 * handoff is the is_max_iteration_handoff(result) seam; 1/0 for bool. */
int w10_promptturn_a4cf0e_is_successful_goal_turn(const struct json_t *result, const char *status,
                                                   const struct json_t *raw, int handoff);
/* PoP: w10_promptturn_a4cf0e_turn_outcome @ tui_gateway/prompt_turn.py:_turn_outcome
 * error_text / interrupt_prefix are seams for turn_error_text and
 * INTERRUPT_WAITING_FOR_MODEL_PREFIX. Returns the [raw, status, last_reasoning] array; caller frees. */
struct json_t *w10_promptturn_a4cf0e_turn_outcome(const struct json_t *result,
        const struct json_t *error_surface, const char *error_text, const char *interrupt_prefix);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_PROMPTTURN_A4CF0E_H */
