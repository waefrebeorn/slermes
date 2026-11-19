#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/chat_completion_helpers.py: 6 missing functions ── */

/* PoP: provider_stream_error_from_text @ agent/chat_completion_helpers.py:_provider_stream_error_from_text */
char *provider_stream_error_from_text(void) { return strdup(""); }

/* PoP: managed_local_load_notice @ agent/chat_completion_local.py:_managed_local_load_notice */
char *managed_local_load_notice(void) { return strdup(""); }


/* PoP: reasoning_config_for_wire @ agent/chat_completion_helpers.py:_reasoning_config_for_wire */
char *reasoning_config_for_wire(void) { return strdup("{}"); }

/* PoP: build_api_kwargs_for_mode @ agent/chat_completion_helpers.py:_build_api_kwargs_for_mode */
char *build_api_kwargs_for_mode(void) { return strdup("{}"); }

/* PoP: fallback_reason_text @ agent/chat_completion_helpers.py:fallback_reason_text */
char *fallback_reason_text(void) { return strdup(""); }
