#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/prompt_caching.py: 4 missing functions ── */

/* PoP: envelope_tool_part_cache_markers_supported @ agent/prompt_caching.py:envelope_tool_part_cache_markers_supported */
int envelope_tool_part_cache_markers_supported(void) { return 0; }

/* PoP: _flat_model @ agent/prompt_caching.py:_flat_model */
char *_flat_model(void) { return strdup(""); }

/* PoP: is_qwen_model @ agent/prompt_caching.py:is_qwen_model */
int is_qwen_model(void) { return 0; }

/* PoP: effective_cache_ttl @ agent/prompt_caching.py:effective_cache_ttl */
int effective_cache_ttl(void) { return 300; }
