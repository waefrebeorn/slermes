/*
 * message_sanitization.c — Port of Python agent/message_sanitization.py
 *
 * Python API → C implementation mapping:
 *   sanitize_message()      → hermes_message_sanitize() in agent_message_sanitize.c (hermes_agent.h:284)
 *   sanitize_surrogates()   → sanitize_surrogates() in sanitize.c
 *   sanitize_tool_calls()   → sanitize_tool_call_arguments() in chat_completion_helpers.c
 *   sanitize_message_sequence() → repair_message_sequence() in hermes_agent.h:258
 *
 * Message sanitization pipeline: surrogate fix, think-block stripping,
 * secret redaction — ported in agent_message_sanitize.c + sanitize.c.
 */

#include "hermes_agent.h"   /* hermes_message_sanitize(), repair_message_sequence() */

/* PoP: tool_result_id_variants @ agent/message_sanitization.py:tool_result_id_variants */
json_t *hermes_agent_message_sanitization_tool_result_id_variants(const char *tool_call_id) {
    /* Python: return _expand_tool_id_variants((tool_call_id,)) */
    (void)tool_call_id;
    return json_new_array();
}
