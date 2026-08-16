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
