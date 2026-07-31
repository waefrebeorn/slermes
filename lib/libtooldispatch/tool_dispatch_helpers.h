#ifndef HERMES_TOOL_DISPATCH_HELPERS_H
#define HERMES_TOOL_DISPATCH_HELPERS_H

/*
 * tool_dispatch_helpers.h — Stateless tool-dispatch utility functions.
 *
 * Port of Python agent/tool_dispatch_helpers.py:
 *   - is_destructive_command: heuristic for terminal commands that modify files
 *   - extract_error_preview: pull a one-line error summary from tool result
 *   - extract_file_mutation_targets: extract file paths from write_file/patch args
 *   - is_multimodal_tool_result: check for _multimodal envelope
 *   - paths_overlap: check if two filesystem paths refer to same subtree
 *   - should_parallelize_tool_batch: parallel safety gating for tool dispatch
 *   - extract_parallel_scope_path: path extraction for parallel scope
 *   - multimodal_text_summary: text extraction from multimodal envelopes
 *   - trajectory_normalize_msg: strip image blobs for trajectory saving
 *   - make_tool_result_message: build tool-result message
 *   - is_untrusted_tool / maybe_wrap_untrusted: prompt injection defense
 *
 * All functions are thread-safe (no global state).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>   /* free() used by inline helpers */

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Destructive command detection
 * ================================================================ */

/* Heuristic: does this terminal command look like it modifies/deletes files?
 * Checks for rm, rmdir, cp, install, mv, sed -i, truncate, dd, shred,
 * git reset/clean/checkout, and output redirects (> single, not >>).
 * Returns true if the command appears destructive. */
bool is_destructive_command(const char *cmd);

/* ================================================================
 *  Error preview extraction
 * ================================================================ */

/* Extract a one-line error summary from a tool result string.
 * If the string is JSON with an "error" field, extracts that value.
 * Collapses whitespace, truncates to max_len.
 * Returns a malloc'd string (caller frees), or NULL if no error found. */
char *extract_error_preview(const char *result, size_t max_len);

/* ================================================================
 *  File mutation target extraction
 * ================================================================ */

/* Known mutating tool names */
#define MUTATING_TOOL_WRITE_FILE "write_file"
#define MUTATING_TOOL_PATCH      "patch"

/* Extract file paths from write_file/patch args JSON.
 * For write_file: extracts "path" field.
 * For patch in "replace" mode: extracts "path" field.
 * For patch in "patch" mode: parses patch body for "*** Update/Add/Delete File:" headers.
 * Returns malloc'd array of strings. count_out receives number of paths.
 * Caller must free each string and the array. */
char **extract_file_mutation_targets(const char *tool_name,
                                      const char *args_json,
                                      size_t *count_out);

/* Free results from extract_file_mutation_targets */
void free_mutation_targets(char **targets, size_t count);

/* ================================================================
 *  Multimodal tool result detection
 * ================================================================ */

/* Check if a JSON tool result string is a multimodal envelope
 * (has _multimodal=True and content array). */
bool is_multimodal_tool_result(const char *result_json);

/* ================================================================
 *  Path overlap detection
 * ================================================================ */

/* Return true when two paths may refer to the same subtree.
 * Both paths should be absolute or relative to same cwd.
 * Does NOT resolve symlinks or check file existence. */
bool paths_overlap(const char *left, const char *right);

/* ================================================================
 *  Parallel tool batch safety gating
 * ================================================================ */

/* Tool batches safe to run concurrently without path conflicts.
 * Read-only tools with no shared mutable session state. */
extern const char * const parallel_safe_tools[];
extern const int    parallel_safe_tools_count;

/* Tools that must never run concurrently (interactive / user-facing). */
extern const char * const never_parallel_tools[];
extern const int    never_parallel_tools_count;

/* File tools that can run concurrently when targeting independent paths. */
extern const char * const path_scoped_tools[];
extern const int    path_scoped_tools_count;

/* Parallel scope path extraction result.
 * On success: path is set to a malloc'd absolute path string.
 * On failure: path is NULL. */
typedef struct {
    char   *path;   /* malloc'd absolute path, or NULL */
    bool    ok;     /* true if extraction succeeded */
} parallel_scope_path_t;

/*
 * Decide whether a batch of tool calls is safe to run concurrently.
 *
 * Rules:
 *   - Single tool calls → not parallelizable (returns false)
 *   - Any tool in never_parallel_tools → false
 *   - Path-scoped tools: extract scope path, check for overlap with earlier
 *     path reservations. Overlap → false.
 *   - Unknown tools → false (safe default)
 *
 * @param tool_names     Array of tool name strings.
 * @param tool_args_json Array of JSON argument strings (same order).
 * @param count          Number of tool calls in the batch.
 * @return true if the batch can safely run in parallel.
 */
bool should_parallelize_tool_batch(const char **tool_names,
                                    const char **tool_args_json,
                                    int count);

/*
 * Extract the normalized absolute file target for path-scoped tools.
 * Currently supports "write_file" and "patch" (both modes).
 *
 * @param tool_name      Tool name string.
 * @param args_json      JSON argument string.
 * @return parallel_scope_path_t with path set on success.
 */
parallel_scope_path_t extract_parallel_scope_path(const char *tool_name,
                                                   const char *args_json);

/*
 * Free the path in a parallel_scope_path_t.
 */
static inline void free_parallel_scope_path(parallel_scope_path_t *psp) {
    if (psp && psp->path) {
        free(psp->path);
        psp->path = NULL;
    }
}

/* ================================================================
 *  Multimodal text summary
 * ================================================================ */

/*
 * Extract a plain text view of a multimodal tool result JSON string.
 *
 * If the JSON has _multimodal=true with text_summary, returns text_summary.
 * Otherwise joins text parts from the content array.
 * For non-multimodal strings, returns a copy of the input.
 *
 * @param result_json  JSON string representing a tool result.
 * @return malloc'd string with the text summary, or NULL on error.
 */
char *multimodal_text_summary_from_json(const char *result_json);

/* ================================================================
 *  Trajectory message normalization
 * ================================================================ */

/*
 * Strip image blobs from a tool result message for trajectory saving.
 * Returns a shallow-like copy: if the result is multimodal, replaces content
 * with its text_summary. If content has image parts, replaces them with
 * "[screenshot]" placeholders.
 *
 * For C, this operates on a JSON string in, JSON string out fashion.
 * Input: JSON object string with optional "content" key.
 * Output: JSON object string with image-stripped content.
 *
 * @param msg_json   JSON string representing a message object.
 * @return malloc'd JSON string with image data stripped, or NULL on error.
 */
char *trajectory_normalize_msg_json(const char *msg_json);

/* ================================================================
 *  Tool result message builder
 * ================================================================ */

/*
 * Build a tool result message JSON string.
 *
 * Creates: {"role":"tool","name":<name>,"tool_name":<name>,
 *           "content":<content_json>,"tool_call_id":<id>}
 *
 * Content from high-risk tools (web_extract, web_search, browser_*, mcp_*)
 * gets wrapped in <untrusted_tool_result> delimiters for prompt injection
 * defense (skipped for content shorter than 32 chars).
 *
 * @param name        Tool name.
 * @param content     Content string (will be JSON-escaped into the output).
 * @param tool_call_id  Tool call ID string.
 * @return malloc'd JSON string, or NULL on error.
 */
char *make_tool_result_message_json(const char *name,
                                     const char *content,
                                     const char *tool_call_id);

/* Port of Python tools/mcp_tool.py:is_mcp_tool_parallel_safe(). */
/** Check if an MCP tool is safe to run in parallel. Port of Python _is_mcp_tool_parallel_safe. */
bool is_mcp_tool_parallel_safe(const char *tool_name);

/** Append a subdirectory hint to a multimodal tool result. Port of Python _append_subdir_hint_to_multimodal. */
char *append_subdir_hint_to_multimodal(const char *result_json, const char *hint);

/* ================================================================
 *  Exported helpers (formerly static; reused by PoP wrappers)
 * ================================================================ */

/** Indirect-prompt-injection check. Port of Python _is_untrusted_tool. */
bool is_untrusted_tool(const char *name);

/** Wrap untrusted tool output in semantic delimiters. Port of Python _maybe_wrap_untrusted. */
char *maybe_wrap_untrusted(const char *name, const char *content);

/* ================================================================
 *  Risk metadata + batch segmentation
 * ================================================================ */

/** Indirect-prompt-injection risk scan of tool output. Port of Python _tool_output_risk_metadata.
 *  Returns malloc'd JSON metadata object, or NULL when nothing risky found. Caller frees. */
char *tool_output_risk_metadata(const char *tool_name, const char *content);

/** Parallel batch segment planner. Port of Python _plan_tool_batch_segments.
 *  arg is a JSON array of {name, arguments} objects. Returns malloc'd JSON
 *  array of [kind, [indices]] segments. Caller frees. */
char *plan_tool_batch_segments(const char *tool_calls_json,
                               const char *execution_cwd);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TOOL_DISPATCH_HELPERS_H */
