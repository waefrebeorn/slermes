#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/gemini_native_adapter.py: 3 missing functions ── */

/* PoP: _gemini_major_version @ agent/gemini_native_adapter.py:_gemini_major_version */
int _gemini_major_version(void) { return 0; }

/* PoP: gemini_requires_tool_call_ids @ agent/gemini_native_adapter.py:gemini_requires_tool_call_ids */
int gemini_requires_tool_call_ids(void) { return 0; }

/* PoP: _looks_like_json_schema @ agent/gemini_native_adapter.py:_looks_like_json_schema */
int _looks_like_json_schema(void) { return 0; }
