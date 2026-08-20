/* tool_result_classification.h — classify a tool result as retryable (transient)
 * vs permanent, so the agent loop can decide whether to retry.
 *
 * Port of Python agent/tool_result_classification.py. The C implementation lives
 * here (the sibling tool_result_classification.c was a name-parity stub).
 *
 * Design contract:
 *   - A result is an ERROR when its JSON starts with {"error"  (LIVE shape).
 *   - An error carrying an explicit "retryable" field wins (true->TRANSIENT,
 *     false->PERMANENT) — that is the authoritative signal a tool author sets.
 *   - Otherwise we fall back to keyword/heuristic classification of the message
 *     and any "code"/"status" field. Default is PERMANENT (fail-closed: do not
 *     blindly retry an unknown error — retries are expensive and can double-side-
 *     effect).
 */
#ifndef TOOL_RESULT_CLASSIFICATION_H
#define TOOL_RESULT_CLASSIFICATION_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TOOL_ERR_NONE = 0,       /* not an error (success result) */
    TOOL_ERR_TRANSIENT,      /* retryable: timeout/network/rate-limit/5xx */
    TOOL_ERR_PERMANENT       /* do not retry: 4xx/not-found/permission/invalid */
} tool_error_class_t;

/* Classify a tool result JSON string. NULL or non-error -> TOOL_ERR_NONE. */
tool_error_class_t tool_classify_error(const char *result_json);

/* True when the result JSON is an error shape ({"error":...}). */
bool tool_result_is_error(const char *result_json);

/* Extract the "recovery_hint" field from an error result, or "" when absent.
 * Returns a pointer into the parsed JSON (do not free); "" when not parseable. */
const char *tool_error_recovery_hint(const char *result_json);

/* Append a "_retry_class":"transient|permanent" field to an error result JSON
 * (non-destructive to other fields) so the agent loop can decide whether to
 * retry. Returns a malloc'd string (caller frees), or NULL on OOM. When the
 * input is not an error JSON, returns a strdup of it. */
char *tool_error_with_class(const char *result_json, tool_error_class_t c);

/* Human-readable class name (for logs/trace). */
const char *tool_error_class_name(tool_error_class_t c);

#ifdef __cplusplus
}
#endif

#endif /* TOOL_RESULT_CLASSIFICATION_H */
