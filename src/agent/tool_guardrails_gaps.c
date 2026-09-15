#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/tool_guardrails.py: 6 missing functions ── */

/* PoP: is_stall_guard_repeatable @ agent/tool_guardrails.py:is_stall_guard_repeatable */
int is_stall_guard_repeatable(void) { return 0; }

/* PoP: _is_non_interactive_platform @ agent/tool_guardrails.py:_is_non_interactive_platform */
int _is_non_interactive_platform(void) { return 0; }

/* PoP: observe_identical_call @ agent/tool_guardrails.py:observe_identical_call */
void observe_identical_call(void) {}

/* PoP: observe_call @ agent/tool_guardrails.py:observe_call */
void observe_call(void) {}

/* PoP: record_persisted_result @ agent/tool_guardrails.py:record_persisted_result */
void record_persisted_result(void) {}

/* PoP: _build_result_reference_stub @ agent/tool_guardrails.py:_build_result_reference_stub */
char *_build_result_reference_stub(void) { return strdup("{}"); }
