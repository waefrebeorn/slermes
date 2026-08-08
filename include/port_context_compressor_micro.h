#ifndef PORT_CONTEXT_COMPRESSOR_MICRO_H
#define PORT_CONTEXT_COMPRESSOR_MICRO_H

/* C11 port of the micro-compaction + init-summary + threshold-state methods
 * from agent/context_compressor.py.
 *
 * The micro-compaction state struct is opaque; callers allocate via
 * cc_micro_compact_state_new() and free via cc_micro_compact_state_free().
 * All methods that the Python ContextCompressor class exposes as
 * REAL_GAPs now have C counterparts here.
 */

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque micro-compaction state ──────────────────────────────────── */
typedef struct cc_micro_compact_state cc_micro_compact_state_t;

/* Allocate and zero-initialise a micro-compaction state handle.
 * Caller owns the result; free with cc_micro_compact_state_free(). */
cc_micro_compact_state_t *cc_micro_compact_state_new(void);
void cc_micro_compact_state_free(cc_micro_compact_state_t *st);

/* ── Model identity setters ─────────────────────────────────────────── */
void cc_micro_compact_set_model(cc_micro_compact_state_t *st, const char *model);
void cc_micro_compact_set_provider(cc_micro_compact_state_t *st, const char *provider);
void cc_micro_compact_set_base_url(cc_micro_compact_state_t *st, const char *base_url);
void cc_micro_compact_set_api_key(cc_micro_compact_state_t *st, const char *api_key);
void cc_micro_compact_set_summary_model(cc_micro_compact_state_t *st, const char *model);

/* ── Compression-threshold config setters ──────────────────────────── */
void cc_micro_compact_set_threshold_percent(cc_micro_compact_state_t *st, double pct);
void cc_micro_compact_set_base_threshold_percent(cc_micro_compact_state_t *st, double pct);
void cc_micro_compact_set_summary_target_ratio(cc_micro_compact_state_t *st, double ratio);
void cc_micro_compact_set_max_tokens(cc_micro_compact_state_t *st, long max_tokens);
void cc_micro_compact_set_config_context_length(cc_micro_compact_state_t *st, long len);
void cc_micro_compact_set_threshold_tokens_cap(cc_micro_compact_state_t *st, long cap);
void cc_micro_compact_set_protect_first_n(cc_micro_compact_state_t *st, int n);
void cc_micro_compact_set_protect_last_n(cc_micro_compact_state_t *st, int n);
void cc_micro_compact_set_min_tail_user_messages(cc_micro_compact_state_t *st, int n);
void cc_micro_compact_set_quiet_mode(cc_micro_compact_state_t *st, bool quiet);
void cc_micro_compact_set_compression_count(cc_micro_compact_state_t *st, int count);
void cc_micro_compact_set_previous_summary(cc_micro_compact_state_t *st, const char *summary);

/* ── Micro-compaction config ────────────────────────────────────────── */
void cc_micro_compact_enable(cc_micro_compact_state_t *st, bool enabled);
void cc_micro_compact_set_every_n_turns(cc_micro_compact_state_t *st, int n);
void cc_micro_compact_set_defrag_threshold(cc_micro_compact_state_t *st, int tokens);
void cc_micro_compact_set_summary_callback(cc_micro_compact_state_t *st,
                                           void *ctx,
                                           char *(*summarize_fn)(void *ctx,
                                                                  const char *prompt,
                                                                  long budget));

/* ── State reset ──────────────────────────────────────────────────────── */
/* PoP: on_session_end @ agent/context_compressor.py:on_session_end */
void cc_micro_compact_on_session_end(cc_micro_compact_state_t *st);

/* ── Init summary + context length resolution ────────────────────────── */
/* PoP: _emit_init_summary_once @ agent/context_compressor.py:_emit_init_summary_once */
void cc_micro_compact_emit_init_summary_once(cc_micro_compact_state_t *st);
/* PoP: _resolve_context_length @ agent/context_compressor.py:_resolve_context_length */
long cc_micro_compact_resolve_context_length(cc_micro_compact_state_t *st);

/* ── Properties (getter + setter) ───────────────────────────────────── */
/* PoP: context_length @ agent/context_compressor.py:context_length */
long  cc_micro_compact_context_length(cc_micro_compact_state_t *st);
void cc_micro_compact_set_context_length(cc_micro_compact_state_t *st, long value);
/* PoP: threshold_tokens @ agent/context_compressor.py:threshold_tokens */
long  cc_micro_compact_threshold_tokens(cc_micro_compact_state_t *st);
void cc_micro_compact_set_threshold_tokens(cc_micro_compact_state_t *st, long value);
/* PoP: tail_token_budget @ agent/context_compressor.py:tail_token_budget */
long  cc_micro_compact_tail_token_budget(cc_micro_compact_state_t *st);
void cc_micro_compact_set_tail_token_budget(cc_micro_compact_state_t *st, long value);
/* PoP: max_summary_tokens @ agent/context_compressor.py:max_summary_tokens */
long  cc_micro_compact_max_summary_tokens(cc_micro_compact_state_t *st);
void cc_micro_compact_set_max_summary_tokens(cc_micro_compact_state_t *st, long value);

/* ── Timeout failure recording ──────────────────────────────────────── */
/* PoP: record_timeout_failure @ agent/context_compressor.py:record_timeout_failure */
void cc_micro_compact_record_timeout_failure(cc_micro_compact_state_t *st,
                                              const char *error);

/* ── Micro-compaction pipeline ───────────────────────────────────────── */
/* PoP: _resolve_compact_cursor @ agent/context_compressor.py:_resolve_compact_cursor */
long cc_micro_compact_resolve_compact_cursor(cc_micro_compact_state_t *st,
                                              json_t *messages,
                                              long head_end, long tail_start);
/* PoP: _find_one_exchange @ agent/context_compressor.py:_find_one_exchange */
bool cc_micro_compact_find_one_exchange(cc_micro_compact_state_t *st,
                                         json_t *messages, long start, long tail_start,
                                         long *out_start, long *out_end);
/* PoP: _serialize_one_exchange @ agent/context_compressor.py:_serialize_one_exchange */
char *cc_micro_compact_serialize_one_exchange(json_t *messages,
                                               long start, long end);
/* PoP: _build_micro_summary_prompt @ agent/context_compressor.py:_build_micro_summary_prompt */
char *cc_micro_compact_build_micro_summary_prompt(const char *existing_summary,
                                                   const char *exchange_text);
/* PoP: _micro_summarize_one @ agent/context_compressor.py:_micro_summarize_one */
char *cc_micro_compact_micro_summarize_one(cc_micro_compact_state_t *st,
                                            const char *existing_summary,
                                            const char *exchange_text);
/* PoP: _needs_defrag @ agent/context_compressor.py:_needs_defrag */
bool cc_micro_compact_needs_defrag(cc_micro_compact_state_t *st);
/* PoP: _defrag_rolling_summary @ agent/context_compressor.py:_defrag_rolling_summary */
bool cc_micro_compact_defrag_rolling_summary(cc_micro_compact_state_t *st,
                                              json_t *messages);
/* PoP: _cursor_after_splice @ agent/context_compressor.py:_cursor_after_splice */
long cc_micro_compact_cursor_after_splice(cc_micro_compact_state_t *st,
                                           json_t *result, long fallback);
/* PoP: _splice_micro_compact_result @ agent/context_compressor.py:_splice_micro_compact_result */
json_t *cc_micro_compact_splice_micro_compact_result(cc_micro_compact_state_t *st,
                                                     json_t *messages,
                                                     long splice_start, long splice_end,
                                                     bool supersede);
/* PoP: _sync_micro_compact_to_db @ agent/context_compressor.py:_sync_micro_compact_to_db */
void cc_micro_compact_sync_to_db(cc_micro_compact_state_t *st,
                                 json_t *compacted_messages);
/* PoP: _emit_micro_compaction_telemetry @ agent/context_compressor.py:_emit_micro_compaction_telemetry */
void cc_micro_compact_emit_telemetry(cc_micro_compact_state_t *st,
                                      const char *outcome,
                                      long messages_before, long messages_after,
                                      long tokens_before, long tokens_after,
                                      long exchange_tokens, long duration_ms);
/* PoP: _micro_compact @ agent/context_compressor.py:_micro_compact */
json_t *cc_micro_compact_micro_compact(cc_micro_compact_state_t *st,
                                        json_t *messages,
                                        void (*on_overrun)(double waited, double ceiling));

#ifdef __cplusplus
}
#endif

#endif /* PORT_CONTEXT_COMPRESSOR_MICRO_H */
