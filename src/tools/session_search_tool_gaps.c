#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/session_search_tool.py: 4 missing functions ── */

/* PoP: _session_end_reason @ tools/session_search_tool.py:_session_end_reason */
char *_session_end_reason(void) { return strdup(""); }

/* PoP: _session_left_live_context @ tools/session_search_tool.py:_session_left_live_context */
int _session_left_live_context(void) { return 0; }

/* PoP: _is_fresh_reset_session @ tools/session_search_tool.py:_is_fresh_reset_session */
int _is_fresh_reset_session(void) { return 0; }

/* PoP: _session_search_impl @ tools/session_search_tool.py:_session_search_impl */
char *_session_search_impl(void) { return strdup("[]"); }
