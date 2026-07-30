/**
 * @file copilot_acp_client.h
 * @brief GitHub Copilot ACP client helpers.
 *
 * Port of Python agent/copilot_acp_client.py (686 lines).
 * All 11 stateless functions ported to C. 3 SDK wrapper classes N/A.
 *
 * MIT License — WuBu Slermes Project
 */
#ifndef COPILOT_ACP_CLIENT_H
#define COPILOT_ACP_CLIENT_H

#include "hermes_core_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Port of Python: _is_gh_copilot_deprecation_message() — line 47 */
bool copilot_is_deprecation_message(const char *stderr_text);

/** Port of Python: _resolve_command() — line 56 */
const char *resolve_command(void);

/** Port of Python: _resolve_args() — line 64 — returns comma/space-sep string or "--acp --stdio" */
const char *resolve_args(void);

/** Port of Python: _resolve_home_dir() — line 71 */
const char *resolve_home_dir(void);

/** Port of Python: _build_subprocess_env() — line 106 — returns malloc'd JSON string */
char *build_subprocess_env(void);

/** Port of Python: _jsonrpc_error() — line 112 — returns malloc'd JSON string */
char *jsonrpc_error(int id, int code, const char *message);

/** Port of Python: _permission_denied() — line 123 — returns malloc'd JSON string */
char *permission_denied(int id);

/** Port of Python: _format_messages_as_prompt() — line 135 — returns malloc'd string */
char *copilot_format_messages_as_prompt(json_node_t *messages,
                                         const char *model,
                                         json_node_t *tools,
                                         const char *tool_choice);

/** Port of Python: _extract_tool_calls_from_text() — line 234 — returns malloc'd json array.
 *  cleaned_out is set to malloc'd text with tool_call blocks removed (or NULL). */
json_node_t *copilot_extract_tool_calls(const char *text, char **cleaned_out);

/** Port of Python: _ensure_path_within_cwd() — line 308 */
bool ensure_path_within_cwd(const char *path, const char *cwd);

/** Port of Python: _build_openai_tool_call() — line 236 — returns malloc'd json_t*
 *  (ChatCompletionMessageToolCall equivalent: id, call_id, response_item_id=null,
 *  type="function", function={name, arguments}). Caller frees. */
json_t *copilot_build_openai_tool_call(const char *call_id,
                                        const char *name,
                                        const char *arguments);

/** Port of Python: _completion_to_stream_chunks() — line 252 — converts a one-shot
 *  ACP completion json_t into OpenAI-style stream chunks. Returns a malloc'd
 *  json_t* array [data_chunk, usage_chunk]. Caller frees. */
json_t *copilot_completion_to_stream_chunks(const json_t *completion);

#ifdef __cplusplus
}
#endif

#endif /* COPILOT_ACP_CLIENT_H */
