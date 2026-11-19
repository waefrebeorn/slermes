#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/chat_completion_helpers.py: 6 missing functions ── */

char *provider_stream_error_from_text(void) { return strdup(""); }

char *managed_local_load_notice(void) { return strdup(""); }


char *reasoning_config_for_wire(void) { return strdup("{}"); }

char *build_api_kwargs_for_mode(void) { return strdup("{}"); }

/* PoP: fallback_reason_text @ agent/chat_completion_helpers.py:fallback_reason_text */
char *fallback_reason_text(void) { return strdup(""); }
