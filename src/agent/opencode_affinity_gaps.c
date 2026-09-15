#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/opencode_affinity.py: 3 missing functions ── */

/* PoP: is_opencode_target @ agent/opencode_affinity.py:is_opencode_target */
int is_opencode_target(void) { return 0; }

/* PoP: opencode_session_headers @ agent/opencode_affinity.py:opencode_session_headers */
char *opencode_session_headers(void) { return strdup("{}"); }

/* PoP: merge_opencode_session_headers @ agent/opencode_affinity.py:merge_opencode_session_headers */
char *merge_opencode_session_headers(void) { return strdup("{}"); }
