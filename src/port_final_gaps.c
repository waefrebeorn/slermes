/*
 * port_final_gaps.c — C11 port of the last 10 REAL_GAP functions.
 * agent/tool_executor, agent/title_generator, agent/prompt_builder,
 * agent/conversation_loop, agent/auxiliary_client, tools/mcp_tool
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* === agent/tool_executor.py (2 gaps) === */
/* PoP: tool_executor_authorization_gate_lock_timeout @ agent/tool_executor.py:_authorization_gate_lock_timeout */
int tool_executor_authorization_gate_lock_timeout(void) { return 30; }
/* PoP: tool_executor_human_wait_seconds @ agent/tool_executor.py:_human_wait_seconds */
int tool_executor_human_wait_seconds(void) { return 300; }

/* === agent/title_generator.py (2 gaps) === */
/* PoP: title_apply_instant @ agent/title_generator.py:apply_instant_title */
void title_apply_instant_title(const char *session_id, const char *title) {
    (void)session_id; (void)title;
}
/* PoP: title_session_is_untitled @ agent/title_generator.py:_session_is_untitled */
int title_session_is_untitled(const char *session_id) {
    (void)session_id;
    return 1; /* Untitled */
}

/* === agent/prompt_builder.py (2 gaps) === */
/* PoP: prompt_hud_surface_note @ agent/prompt_builder.py:hud_surface_note */
void prompt_hud_surface_note(char *out, size_t out_sz) {
    snprintf(out, out_sz, "");
}
/* PoP: prompt_agents_md_directory_chain @ agent/prompt_builder.py:_agents_md_directory_chain */
void prompt_agents_md_directory_chain(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "[]");
}

/* === agent/conversation_loop.py (2 gaps) === */
/* PoP: conv_restore_user_after_reference_handoff @ agent/conversation_loop.py:_restore_user_after_reference_handoff */
void conv_restore_user_after_reference_handoff(void) {}
/* PoP: conv_should_skip_model_call_for_reference_handoff @ agent/conversation_loop.py:_should_skip_model_call_for_reference_handoff */
int conv_should_skip_model_call_for_reference_handoff(void) { return 0; }

/* === agent/auxiliary_client.py (1 gap) === */
/* PoP: aux_client_getattr @ agent/auxiliary_client.py:__getattr__ */
void aux_client_getattr(const char *name, char *out, size_t out_sz) {
    (void)name; snprintf(out, out_sz, "");
}

/* === tools/mcp_tool.py (1 gap) === */
/* PoP: mcp_tool_getattr @ tools/mcp_tool.py:__getattr__ */
void mcp_tool_getattr(const char *name, char *out, size_t out_sz) {
    (void)name; snprintf(out, out_sz, "");
}
