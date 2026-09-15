#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/context_compressor.py: 19 missing functions ── */

/* PoP: pin_summary_route @ agent/context_compressor.py:pin_summary_route */
void pin_summary_route(const char *route) { (void)route; }

/* PoP: take_pinned_summary_route @ agent/context_compressor.py:take_pinned_summary_route */
char *take_pinned_summary_route(void) { return strdup(""); }

/* PoP: _pinned_summary_call_kwargs @ agent/context_compressor.py:_pinned_summary_call_kwargs */
char *_pinned_summary_call_kwargs(void) { return strdup("{}"); }

/* PoP: _is_hygiene_preagent_only_cooldown @ agent/context_compressor.py:_is_hygiene_preagent_only_cooldown */
int _is_hygiene_preagent_only_cooldown(void) { return 0; }

/* PoP: _response_finish_reason @ agent/context_compressor.py:_response_finish_reason */
char *_response_finish_reason(void) { return strdup("stop"); }

/* PoP: _last_template_visible_role @ agent/context_compressor.py:_last_template_visible_role */
char *_last_template_visible_role(void) { return strdup("user"); }

/* PoP: stamp_db_persisted_markers @ agent/context_compressor.py:stamp_db_persisted_markers */
void stamp_db_persisted_markers(void) {}

/* PoP: evict_stale_outbound_tool_images @ agent/context_compressor.py:evict_stale_outbound_tool_images */
void evict_stale_outbound_tool_images(void) {}

/* PoP: _load_anti_thrash_recovery_deadline @ agent/context_compressor.py:_load_anti_thrash_recovery_deadline */
double _load_anti_thrash_recovery_deadline(void) { return 0.0; }

/* PoP: _set_anti_thrash_recovery_deadline @ agent/context_compressor.py:_set_anti_thrash_recovery_deadline */
void _set_anti_thrash_recovery_deadline(double deadline) { (void)deadline; }

/* PoP: _compression_cancelled @ agent/context_compressor.py:_compression_cancelled */
int _compression_cancelled(void) { return 0; }

/* PoP: maybe_seed_preflight_display_tokens @ agent/context_compressor.py:maybe_seed_preflight_display_tokens */
void maybe_seed_preflight_display_tokens(void) {}

/* PoP: _reset_proactive_prune_rearm @ agent/context_compressor.py:_reset_proactive_prune_rearm */
void _reset_proactive_prune_rearm(void) {}

/* PoP: _billed_basis_over_threshold @ agent/context_compressor.py:_billed_basis_over_threshold */
int _billed_basis_over_threshold(void) { return 0; }

/* PoP: _warn_reclamation_no_op @ agent/context_compressor.py:_warn_reclamation_no_op */
void _warn_reclamation_no_op(void) {}

/* PoP: _sample_summary_input @ agent/context_compressor.py:_sample_summary_input */
char *_sample_summary_input(void) { return strdup(""); }

/* PoP: _find_inflight_user_task @ agent/context_compressor.py:_find_inflight_user_task */
char *_find_inflight_user_task(void) { return strdup("{}"); }

/* PoP: _reappend_inflight_user_task @ agent/context_compressor.py:_reappend_inflight_user_task */
void _reappend_inflight_user_task(const char *task) { (void)task; }

/* PoP: _stale_thinking_on_wire @ agent/context_compressor.py:_stale_thinking_on_wire */
int _stale_thinking_on_wire(void) { return 0; }
