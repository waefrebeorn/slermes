#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/transports/codex.py: 6 missing functions ── */

/* PoP: _is_opencode_responses_backend @ agent/transports/codex.py:_is_opencode_responses_backend */
int _is_opencode_responses_backend(void) { return 0; }

/* PoP: _alias_reserved_tools @ agent/transports/codex.py:_alias_reserved_tools */
char *_alias_reserved_tools(void) { return strdup("[]"); }

/* PoP: _profile_declared_efforts @ agent/transports/codex.py:_profile_declared_efforts */
char *_profile_declared_efforts(void) { return strdup("[]"); }

/* PoP: _is_azure_foundry_responses @ agent/transports/codex.py:_is_azure_foundry_responses */
int _is_azure_foundry_responses(void) { return 0; }

/* PoP: _is_post_tool_replay @ agent/transports/codex.py:_is_post_tool_replay */
int _is_post_tool_replay(void) { return 0; }

/* PoP: _native_compaction_active @ agent/transports/codex.py:_native_compaction_active */
int _native_compaction_active(void) { return 0; }
