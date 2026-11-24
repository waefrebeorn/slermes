#ifndef NOC_W19_CONTEXTCOM_DA88C1_H
#define NOC_W19_CONTEXTCOM_DA88C1_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_contextcom_da88c1_safe_int @ agent/context_compressor_telemetry.py:_safe_int
 * int(value) or null (TypeError/ValueError -> None). Numbers truncate, bools count as ints,
 * strings take Python's int() text form (surrounding whitespace, sign, single underscores). */
struct json_t *w19_contextcom_da88c1_safe_int(struct json_t *value);

/* PoP: w19_contextcom_da88c1_compression_method @ agent/context_compressor_telemetry.py:CompressionTelemetryMixin._compression_method
 * "deterministic_fallback" | "aux_fallback_main" | "llm_summary". */
const char *w19_contextcom_da88c1_compression_method(int last_summary_fallback_used,
                                                     int has_aux_model_failure_error);

/* PoP: w19_contextcom_da88c1_note_aux_fallback @ agent/context_compressor_telemetry.py:CompressionTelemetryMixin._note_aux_route_fallback
 * Records a fallback route answer: {"telemetry","failure_model","failure_error"} where the
 * last two are set only for a "main-agent(" route, whose error text is capped at 220 chars. */
struct json_t *w19_contextcom_da88c1_note_aux_fallback(struct json_t *telemetry,
                                                       struct json_t *route);

/* PoP: w19_contextcom_da88c1_record_aux_call @ agent/context_compressor_telemetry.py:CompressionTelemetryMixin._record_aux_compression_call
 * Fills the aux-call fields. `rough_prompt_tokens` is
 * estimate_messages_tokens_rough(prompt_messages) for this attempt (the estimator seam). */
struct json_t *w19_contextcom_da88c1_record_aux_call(struct json_t *telemetry,
                                                     struct json_t *rough_prompt_tokens,
                                                     struct json_t *max_tokens,
                                                     struct json_t *duration_ms,
                                                     struct json_t *aux_provider,
                                                     struct json_t *aux_model,
                                                     struct json_t *effective_aux_context,
                                                     struct json_t *phase_timings);

/* PoP: w19_contextcom_da88c1_record_effect @ agent/context_compressor_telemetry.py:CompressionTelemetryMixin._record_compression_effect
 * What a finished rewrite did; `rough_tokens_after` is the estimator's answer for `compressed`. */
struct json_t *w19_contextcom_da88c1_record_effect(struct json_t *telemetry,
                                                   int last_summary_fallback_used,
                                                   int has_aux_model_failure_error,
                                                   struct json_t *messages_before,
                                                   struct json_t *compressed_len,
                                                   struct json_t *tokens_before,
                                                   struct json_t *tool_results_pruned,
                                                   struct json_t *reasoning_items_pruned,
                                                   struct json_t *last_dropped_count,
                                                   struct json_t *rough_tokens_after);

/* PoP: w19_contextcom_da88c1_record_committed_effect @ agent/context_compressor_telemetry.py:CompressionTelemetryMixin._record_committed_compression_effect
 * The exact transcript shape that crossed the commit boundary. */
struct json_t *w19_contextcom_da88c1_record_committed_effect(struct json_t *telemetry,
                                                             struct json_t *before_len,
                                                             struct json_t *after_len,
                                                             struct json_t *tokens_before,
                                                             struct json_t *tokens_after);

/* PoP: w19_contextcom_da88c1_record_regions @ agent/context_compressor_telemetry.py:CompressionTelemetryMixin._record_compression_regions
 * protection/middle/tail rough token estimates. */
struct json_t *w19_contextcom_da88c1_record_regions(struct json_t *telemetry,
                                                    struct json_t *head_tokens,
                                                    struct json_t *middle_tokens,
                                                    struct json_t *tail_tokens);

/* PoP: w19_contextcom_da88c1_begin_telemetry @ agent/context_compressor_telemetry.py:CompressionTelemetryMixin._begin_compression_telemetry
 * The seeded per-attempt telemetry dict. `seed` is the compressor's
 * _compression_telemetry_seed mapping (or null); an absent attempt_id is echoed as "". */
struct json_t *w19_contextcom_da88c1_begin_telemetry(struct json_t *attempt_id,
                                                     struct json_t *session_id,
                                                     struct json_t *trigger_source,
                                                     struct json_t *seed,
                                                     struct json_t *provider,
                                                     struct json_t *model,
                                                     struct json_t *context_length,
                                                     struct json_t *current_tokens,
                                                     struct json_t *threshold_tokens);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_CONTEXTCOM_DA88C1_H */
