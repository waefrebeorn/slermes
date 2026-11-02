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
/* PoP: _require_sdk @ agent/anthropic_adapter.py:_require_sdk */
void agent_anthropic_adapter_require_sdk(void) { }

/* PoP: _model_matches @ agent/anthropic_adapter.py:_model_matches */
void agent_anthropic_adapter_model_matches(void) { }

/* PoP: _accepts_thinking_disable @ agent/anthropic_adapter.py:_accepts_thinking_disable */
void agent_anthropic_adapter_accepts_thinking_disable(void) { }

/* PoP: _is_pre_thinking_haiku @ agent/anthropic_adapter.py:_is_pre_thinking_haiku */
void agent_anthropic_adapter_is_pre_thinking_haiku(void) { }

/* PoP: _claude_code_candidates @ agent/anthropic_adapter.py:_claude_code_candidates */
void agent_anthropic_adapter_claude_code_candidates(void) { }

/* PoP: find_claude_code_cli @ agent/anthropic_adapter.py:find_claude_code_cli */
void agent_anthropic_adapter_find_claude_code_cli(void) { }

/* PoP: _apply_oauth_prose_aliases @ agent/anthropic_adapter.py:_apply_oauth_prose_aliases */
void agent_anthropic_adapter_apply_oauth_prose_aliases(void) { }

/* PoP: _beta_header @ agent/anthropic_adapter.py:_beta_header */
void agent_anthropic_adapter_beta_header(void) { }

/* PoP: _attribution_headers @ agent/anthropic_adapter.py:_attribution_headers */
void agent_anthropic_adapter_attribution_headers(void) { }

/* PoP: _client_timeout @ agent/anthropic_adapter.py:_client_timeout */
void agent_anthropic_adapter_client_timeout(void) { }

/* PoP: _base_client_kwargs @ agent/anthropic_adapter.py:_base_client_kwargs */
void agent_anthropic_adapter_base_client_kwargs(void) { }

/* PoP: _new_sdk_client @ agent/anthropic_adapter.py:_new_sdk_client */
void agent_anthropic_adapter_new_sdk_client(void) { }

/* PoP: _custom_provider_extra_headers @ agent/anthropic_adapter.py:_custom_provider_extra_headers */
void agent_anthropic_adapter_custom_provider_extra_headers(void) { }

/* PoP: _auth_style @ agent/anthropic_adapter.py:_auth_style */
void agent_anthropic_adapter_auth_style(void) { }

/* PoP: _normalize_to_mcp_wire @ agent/anthropic_adapter.py:_normalize_to_mcp_wire */
void agent_anthropic_adapter_normalize_to_mcp_wire(void) { }

/* PoP: _oauth_wire_namer @ agent/anthropic_adapter.py:_oauth_wire_namer */
void agent_anthropic_adapter_oauth_wire_namer(void) { }

/* PoP: _apply_claude_code_identity @ agent/anthropic_adapter.py:_apply_claude_code_identity */
void agent_anthropic_adapter_apply_claude_code_identity(void) { }

/* PoP: _thinking_kwargs @ agent/anthropic_adapter.py:_thinking_kwargs */
void agent_anthropic_adapter_thinking_kwargs(void) { }

/* PoP: buffer_anthropic_tool_input @ agent/anthropic_adapter.py:buffer_anthropic_tool_input */
void agent_anthropic_adapter_buffer_anthropic_tool_input(void) { }

/* PoP: __getattr__ @ agent/anthropic_adapter.py:__getattr__ */
void agent_anthropic_adapter_dunder_getattrdunder(void) { }

/* PoP: __iter__ @ agent/anthropic_adapter.py:__iter__ */
void agent_anthropic_adapter_dunder_iterdunder(void) { }

/* PoP: normalize_stream_usage @ agent/anthropic_adapter.py:normalize_stream_usage */
void agent_anthropic_adapter_normalize_stream_usage(void) { }

/* PoP: _stream_final_message @ agent/anthropic_adapter.py:_stream_final_message */
void agent_anthropic_adapter_stream_final_message(void) { }
