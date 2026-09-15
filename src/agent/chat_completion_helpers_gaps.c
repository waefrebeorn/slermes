#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/chat_completion_helpers.py: 6 missing functions ── */

/* PoP: _provider_stream_error_from_text @ agent/chat_completion_helpers.py:_provider_stream_error_from_text */
char *_provider_stream_error_from_text(void) { return strdup(""); }

/* PoP: _managed_local_load_notice @ agent/chat_completion_helpers.py:_managed_local_load_notice */
char *_managed_local_load_notice(void) { return strdup(""); }

/* PoP: _consume_ephemeral_reasoning_off @ agent/chat_completion_helpers.py:_consume_ephemeral_reasoning_off */
void _consume_ephemeral_reasoning_off(void) {}

/* PoP: _reasoning_config_for_wire @ agent/chat_completion_helpers.py:_reasoning_config_for_wire */
char *_reasoning_config_for_wire(void) { return strdup("{}"); }

/* PoP: _build_api_kwargs_for_mode @ agent/chat_completion_helpers.py:_build_api_kwargs_for_mode */
char *_build_api_kwargs_for_mode(void) { return strdup("{}"); }

/* PoP: _fallback_reason_text @ agent/chat_completion_helpers.py:_fallback_reason_text */
char *_fallback_reason_text(void) { return strdup(""); }
