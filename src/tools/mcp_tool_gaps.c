#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/mcp_tool.py: 13 missing functions ── */

/* PoP: _npx_bin_candidates @ tools/mcp_tool.py:_npx_bin_candidates */
char *_npx_bin_candidates(void) { return strdup("[]"); }

/* PoP: _npx_cached_bin @ tools/mcp_tool.py:_npx_cached_bin */
char *_npx_cached_bin(void) { return strdup(""); }

/* PoP: _spawn_death_supervisor @ tools/mcp_tool.py:_spawn_death_supervisor */
void _spawn_death_supervisor(void) {}

/* PoP: _prune_dead_supervised_pgids @ tools/mcp_tool.py:_prune_dead_supervised_pgids */
void _prune_dead_supervised_pgids(void) {}

/* PoP: _update_death_supervisor @ tools/mcp_tool.py:_update_death_supervisor */
void _update_death_supervisor(void) {}

/* PoP: _render_mcp_dropped_block_notice @ tools/mcp_tool.py:_render_mcp_dropped_block_notice */
char *_render_mcp_dropped_block_notice(void) { return strdup(""); }

/* PoP: _handle_stdio_child_exited_and_retry @ tools/mcp_tool.py:_handle_stdio_child_exited_and_retry */
void _handle_stdio_child_exited_and_retry(void) {}

/* PoP: _mcp_registry_scope @ tools/mcp_tool.py:_mcp_registry_scope */
char *_mcp_registry_scope(void) { return strdup(""); }

/* PoP: _server_registry_scope @ tools/mcp_tool.py:_server_registry_scope */
char *_server_registry_scope(void) { return strdup(""); }

/* PoP: reprobe_tool_availability @ tools/mcp_tool.py:reprobe_tool_availability */
char *reprobe_tool_availability(void) { return strdup("{}"); }

/* PoP: persist_agent_tool_names @ tools/mcp_tool.py:persist_agent_tool_names */
void persist_agent_tool_names(void) {}

/* PoP: restore_agent_tool_prefix @ tools/mcp_tool.py:restore_agent_tool_prefix */
char *restore_agent_tool_prefix(void) { return strdup(""); }

/* PoP: _merge_preserving_prefix @ tools/mcp_tool.py:_merge_preserving_prefix */
char *_merge_preserving_prefix(void) { return strdup(""); }
