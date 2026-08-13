/* context_compressor.h — Opaque ContextCompressor handle + orchestration API
 * for port_agent_context_compressor.c. Mirrors agent.context_compressor.py. */

#ifndef CONTEXT_COMPRESSOR_H
#define CONTEXT_COMPRESSOR_H

#include "hermes_json.h"   /* json_t (opaque struct) */
#include "context_compressor_pure.h"  /* budget + guard structs */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct context_compressor_t context_compressor_t;

/* Construct / free. Strings are copied (owned). Pass NULL for optional fields. */
context_compressor_t *context_compressor_new(const char *model,
                                             const char *summary_model,
                                             const char *base_url, const char *api_key,
                                             const char *provider, const char *api_mode,
                                             int config_context_length,
                                             double threshold_percent);
void context_compressor_free(context_compressor_t *cc);

/* ── micro-compaction orchestration (pure array/struct ops) ───────────── */
/* PoP: _resolve_compact_cursor @ agent/context_compressor.py:_resolve_compact_cursor */
long cc_resolve_compact_cursor(context_compressor_t *cc, const json_t *messages,
                               long head_end, long tail_start);
/* PoP: _find_one_exchange @ agent/context_compressor.py:_find_one_exchange */
bool cc_find_one_exchange(context_compressor_t *cc, const json_t *messages,
                         long start, long tail_start,
                         long *out_start, long *out_end);
/* PoP: _serialize_one_exchange @ agent/context_compressor.py:_serialize_one_exchange */
char *cc_serialize_one_exchange(context_compressor_t *cc, const json_t *messages,
                                long start, long end);
/* PoP: _splice_micro_compact_result @ agent/context_compressor.py:_splice_micro_compact_result
 * Returns a NEW array (caller frees via json_free). */
json_t *cc_splice_micro_compact_result(context_compressor_t *cc, const json_t *messages,
                                       long splice_start, long splice_end, bool supersede);
/* PoP: _needs_defrag @ agent/context_compressor.py:_needs_defrag */
bool cc_needs_defrag(const context_compressor_t *cc);
/* PoP: _emit_micro_compaction_telemetry @ agent/context_compressor.py:_emit_micro_compaction_telemetry
 * Pass -1 for optional long token fields. */
void cc_emit_micro_compaction_telemetry(context_compressor_t *cc,
        const char *outcome, int messages_before, int messages_after,
        long tokens_before, long tokens_after, long exchange_tokens,
        long duration_ms);

/* setters for micro-compaction state */
void cc_set_rolling_summary(context_compressor_t *cc, const char *s);
const char *cc_get_rolling_summary(const context_compressor_t *cc);
void cc_set_micro_compact_cursor(context_compressor_t *cc, long v);
void cc_set_micro_compact_enabled(context_compressor_t *cc, int every_n_turns);
void cc_set_micro_compact_tokens_saved_total(context_compressor_t *cc, long v);

/* PoP: _load_proactive_prune_rearm_tokens @ agent/context_compressor.py:_load_proactive_prune_rearm_tokens
 * Restore the durable proactive-prune-rearm runway token count from the
 * session's model_config. No-op when no session_db/session_id. */
void cc_load_proactive_prune_rearm_tokens(context_compressor_t *cc);
/* PoP: _clear_durable_proactive_prune_rearm @ agent/context_compressor.py:_clear_durable_prune_rearm
 * Remove the persisted proactive-prune-rearm runway key. No-op when
 * standalone. */
void cc_clear_durable_proactive_prune_rearm(context_compressor_t *cc);
/* Arm / disarm the session_db / session_id on a compressor. */
void cc_compressor_set_session(context_compressor_t *cc,
                               const char *session_id, void *session_db);

/* ── micro-summation prompt builder (pure, no IO) ──────────────────────── */
/* PoP: _build_micro_summary_prompt @ agent/context_compressor.py:_build_micro_summary_prompt */
/* Build the prompt messages (a 2-element JSON array: system + user) for a
 * single-exchange micro-summary. Caller frees the returned JSON via json_free. */
json_t *cc_build_micro_summary_prompt(const char *existing_summary,
                                       const char *exchange_text);

/* PoP: _emit_init_summary_once @ agent/context_compressor.py:_emit_init_summary_once */
/* NOTE: cc_note_request_rough_estimate (note_request_rough_estimate) is a pure
 * helper on cc_request_tracker_t, declared + impl in context_compressor_pure. */
void cc_arm_init_summary(context_compressor_t *cc);
void cc_emit_init_summary_once(context_compressor_t *cc);

#ifdef __cplusplus
}
#endif
#endif /* CONTEXT_COMPRESSOR_H */
