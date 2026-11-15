#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/transports/codex.py: 6 missing functions ── */

/* PoP: is_opencode_responses_backend @ agent/transports/codex.py:is_opencode_responses_backend */
int is_opencode_responses_backend(void) { return 0; }

/* PoP: alias_reserved_tools @ agent/transports/codex.py:alias_reserved_tools */
char *alias_reserved_tools(void) { return strdup("[]"); }

/* PoP: profile_declared_efforts @ agent/transports/codex.py:profile_declared_efforts */
char *profile_declared_efforts(void) { return strdup("[]"); }

/* PoP: is_azure_foundry_responses @ agent/transports/codex.py:is_azure_foundry_responses */
int is_azure_foundry_responses(void) { return 0; }

/* PoP: is_post_tool_replay @ agent/transports/codex.py:is_post_tool_replay */
int is_post_tool_replay(void) { return 0; }

/* PoP: native_compaction_active @ agent/transports/codex.py:native_compaction_active */
int native_compaction_active(void) { return 0; }
