/*
 * anthropic_adapter.c — Port of Python agent/anthropic_adapter.py
 *
 * Python API → C implementation mapping:
 *   anthropic_adapter_process_message()  → provider_anthropic.c
 *   anthropic_adapter_stream()           → provider_anthropic.c
 *   anthropic_adapter_parse_response()   → provider_anthropic.c
 *   anthropic_adapter_build_request()    → provider_anthropic.c
 *   anthropic_adapter_count_tokens()     → provider_anthropic.c
 *   anthropic_adapter_handle_error()     → provider_anthropic.c
 *   anthropic_adapter_convert_messages() → provider_anthropic.c
 *   anthropic_adapter_extract_content()  → provider_anthropic.c
 *
 * Many pure-utility functions that don't require the Python SDK are
 * ported to C in src/cli/port_agent_anthropic_adapter.c (see:
 * _resolve_positive_anthropic_max_tokens, _supports_adaptive_thinking,
 * _supports_xhigh_effort, _forbids_sampling_params, _supports_fast_mode,
 * _to_plain_data, _sanitize_replay_block, _get_anthropic_max_output).
 *
 * This file holds the remaining name-parity wrappers that either:
 * a) Call through to the Python SDK (cannot be fully ported to C), or
 * b) Are pure utilities small enough to live here.
 */

#include "hermes_core_types.h"
#include "hermes_logger.h"
#include <string.h>
#include <stdlib.h>

/* Port of Python agent/anthropic_adapter.py:_to_oauth_wire_name
 *
 * Prefix bare tool names with "mcp__" for the Anthropic OAuth wire format.
 * - Already "mcp__" → return as-is (don't double-prefix)
 * - "mcp_" (single underscore native MCP tool) → promote to "mcp__"
 * - Anything else → prepend "mcp__"
 *
 * Returns malloc'd string that the caller must free, or NULL on allocation
 * failure.
 */
void* cli_agent_anthropic_adapter__to_oauth_wire_name(void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p2; (void)p3; (void)p4; (void)p5;
    if (!p1) return NULL;

    const char *name = (const char*)p1;

    /* Already has the double-underscore prefix — return as-is */
    if (strncmp(name, "mcp__", 5) == 0) {
        char *result = strdup(name);
        return result;
    }

    /* Single-underscore native MCP tool "mcp_<rest>" → "mcp__<rest>" */
    if (strncmp(name, "mcp_", 4) == 0) {
        size_t rest_len = strlen(name) - 4;  /* skip "mcp_" prefix */
        char *result = (char*)malloc(6 + rest_len);  /* "mcp__" + rest + '\0' */
        if (!result) return NULL;
        memcpy(result, "mcp__", 5);
        memcpy(result + 5, name + 4, rest_len + 1);  /* copy rest + NUL */
        return result;
    }

    /* Bare name — prepend "mcp__" */
    size_t name_len = strlen(name);
    char *result = (char*)malloc(5 + name_len + 1);  /* "mcp__" + name + NUL */
    if (!result) return NULL;
    memcpy(result, "mcp__", 5);
    memcpy(result + 5, name, name_len + 1);
    return result;
}

/* Port of Python agent/anthropic_adapter.py:create_anthropic_message
 *
 * This function wraps the Python `anthropic` SDK's messages.create() /
 * messages.stream().get_final_message(). Since the actual SDK call
 * happens at the Python level, the C role is to:
 *
 * 1. Build the kwargs JSON that the Python bridge processes
 * 2. Dispatch to the provider_anthropic module which calls Python
 *
 * In the current architecture, this is a Python SDK operation.
 * The equivalent C-level message creation happens through:
 *   provider_anthropic.c -> provider_anthropic_create_message()
 */
void* cli_agent_anthropic_adapter_create_anthropic_message(void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "anthropic_adapter",
               "create_anthropic_message: dispatches to provider_anthropic "
               "(Python SDK call - use provider_anthropic_create_message in C)");
    return NULL;
}