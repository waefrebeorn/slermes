#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/background_review.py: 2 missing functions ── */

/* PoP: _parent_can_emit_tool_calls @ agent/background_review.py:_parent_can_emit_tool_calls */
void _parent_can_emit_tool_calls(void) { }

/* PoP: build_cache_parity_fork @ agent/background_review.py:build_cache_parity_fork */
char *build_cache_parity_fork(void) { return strdup(""); }
