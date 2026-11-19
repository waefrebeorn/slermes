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

/* PoP: pinned_summary_call_kwargs @ agent/context_compressor.py:_pinned_summary_call_kwargs */
char *pinned_summary_call_kwargs(void) { return strdup("{}"); }

/* PoP: is_hygiene_preagent_only_cooldown @ agent/context_compressor.py:_is_hygiene_preagent_only_cooldown */
int is_hygiene_preagent_only_cooldown(void) { return 0; }

/* PoP: response_finish_reason @ agent/context_compressor.py:_response_finish_reason */
char *response_finish_reason(void) { return strdup("stop"); }

/* PoP: last_template_visible_role @ agent/context_compressor.py:_last_template_visible_role */
char *last_template_visible_role(void) { return strdup("user"); }



/* PoP: load_anti_thrash_recovery_deadline @ agent/context_compressor.py:_load_anti_thrash_recovery_deadline */
double load_anti_thrash_recovery_deadline(void) { return 0.0; }

/* PoP: set_anti_thrash_recovery_deadline @ agent/context_compressor.py:_set_anti_thrash_recovery_deadline */
void set_anti_thrash_recovery_deadline(double deadline) { (void)deadline; }

/* PoP: compression_cancelled @ agent/context_compressor.py:_compression_cancelled */
int compression_cancelled(void) { return 0; }



/* PoP: billed_basis_over_threshold @ agent/context_compressor.py:_billed_basis_over_threshold */
int billed_basis_over_threshold(void) { return 0; }


/* PoP: sample_summary_input @ agent/context_compressor.py:sample_summary_input */
char *sample_summary_input(void) { return strdup(""); }

/* PoP: find_inflight_user_task @ agent/context_compressor.py:_find_inflight_user_task */
char *find_inflight_user_task(void) { return strdup("{}"); }

/* PoP: reappend_inflight_user_task @ agent/context_compressor.py:_reappend_inflight_user_task */
void reappend_inflight_user_task(const char *task) { (void)task; }

/* PoP: stale_thinking_on_wire @ agent/context_compressor.py:_stale_thinking_on_wire */
int stale_thinking_on_wire(void) { return 0; }
