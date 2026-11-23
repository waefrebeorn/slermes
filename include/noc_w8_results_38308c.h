#ifndef NOC_W8_RESULTS_38308C_H
#define NOC_W8_RESULTS_38308C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_results_38308c_reanchor_current_turn_user_idx @ session_rules/results.py:reanchor_current_turn_user_idx
 * Returns a JSON integer index (may be -1). Caller frees. */
struct json_t *w8_results_38308c_reanchor(const struct json_t *messages, const struct json_t *user_message);

/* PoP: w8_results_38308c_compact_result @ session_rules/results.py:compact_result
 * Both arguments are JSON values; user_message may be a JSON null (Python None).
 * Caller frees the returned object. */
struct json_t *w8_results_38308c_compact(const struct json_t *result, const struct json_t *user_message);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_RESULTS_38308C_H */