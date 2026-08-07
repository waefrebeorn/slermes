/*
 * message_sanitization_pure.h — Pure helpers from agent/message_sanitization.py.
 */
#ifndef MESSAGE_SANITIZATION_PURE_H
#define MESSAGE_SANITIZATION_PURE_H

#include <stdbool.h>
#include "libjson/json.h"

/* PoP: _family_rule @ agent/message_sanitization.py:_family_rule */
const void *msg_sanitize_family_rule(const char *family);

/* PoP: matches_reasoning_echo_family @ agent/message_sanitization.py:matches_reasoning_echo_family */
bool msg_sanitize_matches_reasoning_echo_family(
    const char *family, const char *provider, const char *model, const char *base_url);

/* PoP: reasoning_echo_family @ agent/message_sanitization.py:reasoning_echo_family */
const char *msg_sanitize_reasoning_echo_family(
    const char *provider, const char *model, const char *base_url);

/* PoP: needs_reasoning_echo @ agent/message_sanitization.py:needs_reasoning_echo */
bool msg_sanitize_needs_reasoning_echo(
    const char *provider, const char *model, const char *base_url);

/* PoP: deterministic_call_id @ agent/message_sanitization.py:deterministic_call_id
 * Returns "call_" + first 12 hex chars of sha256("fn:args:index"). Caller frees. */
char *msg_sanitize_deterministic_call_id(const char *fn_name,
                                          const char *arguments,
                                          int index);

/* PoP: coalesce_tool_call_id @ agent/message_sanitization.py:coalesce_tool_call_id */
const char *msg_sanitize_coalesce_tool_call_id(const json_t *tc);

/* PoP: apply_reasoning_content_policy @ agent/message_sanitization.py:apply_reasoning_content_policy */
/* Mutates api_msg JSON object in place. Returns 1 if reasoning_content added/changed, 0 otherwise. */
int msg_sanitize_apply_reasoning_content_policy(json_t *source_msg, json_t *api_msg, int needs_thinking_pad);

/* PoP: reapply_reasoning_echo @ agent/message_sanitization.py:reapply_reasoning_echo */
/* Reconciles assistant turns' reasoning_content for the active provider.
 * Mutates api_messages array in place. Returns count of changed turns. */
int msg_sanitize_reapply_reasoning_echo(json_t *api_messages, int needs_thinking_pad);

/* PoP: uniquify_tool_call_ids @ agent/message_sanitization.py:uniquify_tool_call_ids */
/* Mutates tool_calls array in place. Returns the list (unchanged pointer). */
json_t *msg_sanitize_uniquify_tool_call_ids(json_t *tool_calls);

#endif
