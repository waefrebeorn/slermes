#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/conversation_loop.py: 9 missing functions ── */

/* PoP: _midturn_request_pressure_tokens @ agent/conversation_loop.py:_midturn_request_pressure_tokens */
int _midturn_request_pressure_tokens(void) { return 0; }

/* PoP: _maybe_inject_run_budget_wrapup @ agent/conversation_loop.py:_maybe_inject_run_budget_wrapup */
void _maybe_inject_run_budget_wrapup(void) {}

/* PoP: _is_interpreter_shutdown_error @ agent/conversation_loop.py:_is_interpreter_shutdown_error */
int _is_interpreter_shutdown_error(void) { return 0; }

/* PoP: _moa_client_consumes_prepared_request @ agent/conversation_loop.py:_moa_client_consumes_prepared_request */
int _moa_client_consumes_prepared_request(void) { return 0; }

/* PoP: _moa_reference_metrics_for_hook @ agent/conversation_loop.py:_moa_reference_metrics_for_hook */
char *_moa_reference_metrics_for_hook(void) { return strdup("{}"); }

/* PoP: _pressure_with_real_floor @ agent/conversation_loop.py:_pressure_with_real_floor */
int _pressure_with_real_floor(void) { return 0; }

/* PoP: _maybe_grow_local_window @ agent/conversation_loop.py:_maybe_grow_local_window */
int _maybe_grow_local_window(void) { return 0; }

/* PoP: _system_prompt_for_hooks @ agent/conversation_loop.py:_system_prompt_for_hooks */
char *_system_prompt_for_hooks(void) { return strdup(""); }

/* PoP: _provider_overflow_exhausted_result @ agent/conversation_loop.py:_provider_overflow_exhausted_result */
char *_provider_overflow_exhausted_result(void) { return strdup("{}"); }
