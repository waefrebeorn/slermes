#ifndef NOC_W12_RUNTRUNPR_AE010A_H
#define NOC_W12_RUNTRUNPR_AE010A_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w12_runturnpr_ae010a_repeat_count @ gateway/run_turn_progress.py:_append_progress_repeat_count
 * "(xN)" after a closing fenced-code line at the very end of body; otherwise " msg (xN)". */
struct json_t *w12_runturnpr_ae010a_repeat_count(const struct json_t *message, const struct json_t *occurrences);
/* PoP: w12_runturnpr_ae010a_compact @ gateway/run_turn_progress.py:GatewayTurnProgressMixin._TaskCardState._compact
 * re.sub(r"\s+", " ", str(value or "")).strip(), truncated to limit with "...". */
struct json_t *w12_runturnpr_ae010a_compact(const struct json_t *value, const struct json_t *limit);
/* PoP: w12_runturnpr_ae010a_progress_text @ gateway/run_turn_progress.py:GatewayTurnProgressMixin._progress_text
 * "\n".join(str(line) for line in lines). */
struct json_t *w12_runturnpr_ae010a_progress_text(const struct json_t *lines);
/* PoP: w12_runturnpr_ae010a_is_reset_marker @ gateway/run_turn_progress.py:GatewayTurnProgressMixin._is_reset_marker
 * tuple len>=1 and first element == "__reset__" (JSON arrays stand in for tuples). */
struct json_t *w12_runturnpr_ae010a_is_reset_marker(const struct json_t *raw);
/* PoP: w12_runturnpr_ae010a_absorb @ gateway/run_turn_progress.py:GatewayTurnProgressMixin._progress_absorb
 * Fold a queue item into the bubble buffer; returns {"buf":..., "rendered":...}. */
struct json_t *w12_runturnpr_ae010a_absorb(const struct json_t *progress_lines, const struct json_t *raw);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_RUNTRUNPR_AE010A_H */
