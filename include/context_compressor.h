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

#ifdef __cplusplus
}
#endif
#endif /* CONTEXT_COMPRESSOR_H */
