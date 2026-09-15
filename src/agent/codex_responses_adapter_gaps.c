#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/codex_responses_adapter.py: 4 missing functions ── */

/* PoP: _sanitize_replayed_fn_name @ agent/codex_responses_adapter.py:_sanitize_replayed_fn_name */
char *_sanitize_replayed_fn_name(void) { return strdup(""); }

/* PoP: _canonical_call_id_from_fc @ agent/codex_responses_adapter.py:_canonical_call_id_from_fc */
char *_canonical_call_id_from_fc(void) { return strdup(""); }

/* PoP: classify_responses_route @ agent/codex_responses_adapter.py:classify_responses_route */
char *classify_responses_route(void) { return strdup(""); }

/* PoP: estimate_native_responses_preflight_tokens @ agent/codex_responses_adapter.py:estimate_native_responses_preflight_tokens */
int estimate_native_responses_preflight_tokens(void) { return 0; }
