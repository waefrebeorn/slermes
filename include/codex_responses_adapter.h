/*
 * codex_responses_adapter.h — Faithful C11 port of Python
 * agent/codex_responses_adapter.py (1625 lines).
 *
 * Pure format-conversion / normalization helpers for the OpenAI Responses API
 * (Codex, xAI, GitHub Models). Every Python function is ported 1:1:
 *
 *   _classify_responses_issuer            -> classify_responses_issuer
 *   _neutralize_harmony_tokens            -> neutralize_harmony_tokens
 *   _neutralize_harmony_structure         -> neutralize_harmony_structure
 *   _chat_content_to_responses_parts      -> chat_content_to_responses_parts
 *   _summarize_user_message_for_log       -> summarize_user_message_for_log
 *   _deterministic_call_id                -> deterministic_call_id
 *   _clamp_responses_call_id              -> clamp_responses_call_id
 *   _split_responses_tool_id              -> split_responses_tool_id
 *   _derive_responses_function_call_id    -> derive_responses_function_call_id
 *   _responses_tools                       -> responses_tools
 *   _normalize_responses_message_status   -> normalize_responses_message_status
 *   _chat_messages_to_responses_input      -> chat_messages_to_responses_input
 *   _preflight_codex_input_items          -> preflight_codex_input_items
 *   _preflight_codex_api_kwargs            -> preflight_codex_api_kwargs
 *   _extract_responses_message_text       -> extract_responses_message_text
 *   _extract_responses_reasoning_text     -> extract_responses_reasoning_text
 *   _format_responses_error               -> format_responses_error
 *   _normalize_codex_response             -> normalize_codex_response
 *
 * Conventions:
 *   - Functions returning json_node_t* allocate fresh nodes (caller frees).
 *   - Functions returning char* malloc() the string (caller free()s).
 *   - Strings that must round-trip through the Responses API are built with
 *     json_new_string / json_new_array / json_new_object / json_array_append.
 */
#ifndef CODEX_RESPONSES_ADAPTER_H
#define CODEX_RESPONSES_ADAPTER_H

#include "hermes_core_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_RESPONSES_ITEM_ID_LENGTH 64

/* issuer-kind classification */
const char *classify_responses_issuer(int is_xai, int is_github,
                                             int is_codex_backend,
                                             const char *base_url);

/* Harmony control-token neutralization (returns malloc'd string) */
char *neutralize_harmony_tokens(const char *text);
/* Neutralize a JSON-like value in place; returns a new json_node_t* (caller frees) */
json_node_t *neutralize_harmony_structure(const json_node_t *value);

/* Convert chat multimodal content list -> Responses parts array (caller frees) */
json_node_t *chat_content_to_responses_parts(const json_node_t *content,
                                                   const char *role);

/* Flatten message content to plain text for logging (returns malloc'd string) */
char *summarize_user_message_for_log(const json_node_t *content,
                                            const char *sep);

/* Deterministic call id (thin wrapper over message_sanitization) */
char *deterministic_call_id(const char *fn_name, const char *arguments,
                                  int index);

/* Clamp a call_id to 64 chars (returns malloc'd string) */
char *clamp_responses_call_id(const char *call_id);

/* Split "call_id|response_item_id" -> two malloc'd strings (either may be NULL) */
void split_responses_tool_id(const char *raw_id, char **out_call_id,
                                   char **out_response_item_id);

/* Build a valid Responses function_call.id (must start with fc_) (malloc'd) */
char *derive_responses_function_call_id(const char *call_id,
                                             const char *response_item_id);

/* Convert chat tool schemas -> Responses function-tool schemas (caller frees) */
json_node_t *responses_tools(const json_node_t *tools);

/* Normalize a message status string (returns malloc'd, default "completed") */
char *normalize_responses_message_status(const json_node_t *value,
                                               const char *default_status);

/* Extract assistant text from a Responses message item (returns malloc'd) */
char *extract_responses_message_text(const json_node_t *item);
/* Extract reasoning text from a Responses reasoning item (returns malloc'd) */
char *extract_responses_reasoning_text(const json_node_t *item);

/* Build human-readable error string (returns malloc'd) */
char *format_responses_error(const json_node_t *error_obj,
                                   const char *response_status);

/* Convert internal chat messages -> Responses input items (caller frees) */
json_node_t *chat_messages_to_responses_input(const json_node_t *messages,
                                                    int is_xai, int is_github,
                                                    int replay_reasoning,
                                                    const char *current_issuer_kind);

/* Validate + normalize input items (returns new array or NULL on error; err set) */
json_node_t *preflight_codex_input_items(const json_node_t *raw_items,
                                               int is_github, int sanitize_harmony,
                                               char *err_buf, size_t err_len);

/* Validate + normalize a full Responses request kwargs object */
json_node_t *preflight_codex_api_kwargs(const json_node_t *api_kwargs,
                                              int allow_stream, int is_github,
                                              int sanitize_harmony,
                                              char *err_buf, size_t err_len);

/* Normalize a Responses output object into an assistant_message-like node.
 * Returns a json_node_t* with keys: content, tool_calls, reasoning,
 * codex_reasoning_items, codex_message_items, finish_reason. Caller frees. */
json_node_t *normalize_codex_response(const json_node_t *response,
                                            const char *issuer_kind);

#ifdef __cplusplus
}
#endif

#endif /* CODEX_RESPONSES_ADAPTER_H */
