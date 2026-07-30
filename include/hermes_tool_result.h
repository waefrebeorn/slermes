#ifndef HERMES_TOOL_RESULT_H
#define HERMES_TOOL_RESULT_H

/*
 * hermes_tool_result.h — Tool result classification + truncation helpers.
 * Port of Python agent/tool_result_classification.py (26 lines),
 * agent/context_compressor._truncate_tool_call_args_json(),
 * and agent/chat_completion_helpers.estimate_request_context_tokens().
 *
 * Also provides tool_error() and tool_result_obj() helpers, port of
 * Python tools/registry.py tool_error() and tool_result().
 *
 * Provides:
 *   - file_mutation_result_landed: check if a write_file/patch succeeded
 *   - tool_call_args_truncate: shrink long string values in tool-call args JSON
 *   - estimate_payload_context_tokens: estimate tokens from API payload JSON
 *   - tool_error: create a JSON error string with optional extra fields
 *   - tool_result_obj: create a JSON success/result string
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Check if a tool result indicates a successful file mutation.
 * Returns true if the tool_name is write_file or patch and the result
 * contains success indicators.
 * tool_name: "write_file" or "patch"
 * result: the JSON result string from the tool handler */
bool file_mutation_result_landed(const char *tool_name, const char *result);

/* Truncate long string values in a tool-call arguments JSON blob while
 * preserving JSON validity.
 *
 * Parses the args JSON, walks the tree recursively, truncates any string
 * value longer than head_chars to "prefix...[truncated]", and re-serializes.
 * Non-string values (numbers, booleans, null) are preserved intact.
 *
 * Returns a malloc'd JSON string (caller frees), or NULL if args is NULL,
 * empty, or not valid JSON (caller should use original args in that case).
 *
 * Port of Python agent/context_compressor._truncate_tool_call_args_json(). */
char *tool_call_args_truncate(const char *args, size_t head_chars);

/* Estimate context tokens from an API payload JSON string.

 * Handles multiple API shapes:
 *   - Bare array -> treat as Chat Completions messages
 *   - Dict with "messages" -> Chat Completions (+ "tools" if present)
 *   - Dict with "input" -> Responses API (+ "instructions"/"tools")
 *   - Any other dict -> sum all string values
 *   - Invalid JSON -> fall back to raw char count / 4
 *
 * Counts characters in all string values and divides by 4 for a
 * rough token estimate (same heuristic as Python's estimate).
 *
 * Port of Python agent/chat_completion_helpers.estimate_request_context_tokens(). */
size_t estimate_payload_context_tokens(const char *payload_json);

/* Create a JSON error string: {"error": "<message>", ...extra}
 * Port of Python tools/registry.py tool_error().
 * message: human-readable error description (required)
 * ...: optional key=value pairs as const char* args, terminated by NULL.
 * Returns malloc'd JSON string (caller frees).
 * Example: tool_error("file not found", "path", "/tmp/x", NULL)
 *          -> "{\"error\":\"file not found\",\"path\":\"/tmp/x\"}" */
char *tool_error(const char *message, ...);

/* Create a JSON success/result string: {"success":true, ...fields}
 * Port of Python tools/registry.py tool_result().
 * Fields are key=value pairs as const char* args, terminated by NULL.
 * Returns malloc'd JSON string (caller frees).
 * Example: tool_result_obj("path", "/tmp/x", "bytes", "42", NULL)
 *          -> "{\"success\":true,\"path\":\"/tmp/x\",\"bytes\":42}" */
char *tool_result_obj(const char *first_key, ...);

/* Create cancelled tool result: {"error":"Tool execution cancelled by <reason>","status":"cancelled"}
 * Port of Python agent/tool_executor.py:_cancelled_tool_result(). */
char *cancelled_tool_result(const char *reason);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TOOL_RESULT_H */
