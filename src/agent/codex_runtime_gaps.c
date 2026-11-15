#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/codex_runtime.py: 4 missing functions ── */

/* PoP: codex_request_failure_details @ agent/codex_runtime.py:codex_request_failure_details */
char *codex_request_failure_details(void) { return strdup("{}"); }


/* PoP: sanitize_consumer_codex_request @ agent/codex_runtime.py:sanitize_consumer_codex_request */
char *sanitize_consumer_codex_request(void) { return strdup("{}"); }

/* PoP: bypass_sdk_request_transform @ agent/codex_runtime.py:bypass_sdk_request_transform */
int bypass_sdk_request_transform(void) { return 0; }
