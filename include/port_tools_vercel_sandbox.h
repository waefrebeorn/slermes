/*
 * port_tools_vercel_sandbox.h — C11 port of pure helpers from
 * tools/environments/vercel_sandbox.py.
 *
 * Ports the deterministic, I/O-free helpers from the Vercel sandbox
 * environment: exception chain walking, status code extraction,
 * transient error classification, type coercion, result extraction,
 * and snapshot ID extraction.
 *
 * Memory: string-returning functions return malloc'd strings (caller
 * frees) or NULL. All other functions are pure value returns.
 */

#ifndef PORT_TOOLS_VERCEL_SANDBOX_H
#define PORT_TOOLS_VERCEL_SANDBOX_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_t json_t;

/* PoP: _exception_chain @ tools/environments/vercel_sandbox.py:_exception_chain */
/* Walk the __cause__/__context__ chain of an exception, deduplicating
 * by id() to break cycles. Returns a JSON array of exception dicts
 * with keys "type" (fully qualified class name) and "message"
 * (str(exc)). malloc'd. */
char *ve_exception_chain(const char *exc_json);

/* PoP: _extract_status_code @ tools/environments/vercel_sandbox.py:_extract_status_code */
/* Extract an HTTP status code from an exception or its .response
 * attribute. Returns the int code, or -1 if absent/not an int. */
int ve_extract_status_code(const char *exc_json);

/* PoP: _is_transient_vercel_error @ tools/environments/vercel_sandbox.py:_is_transient_vercel_error */
/* True if any exception in the chain has a transient status code
 * (408, 425, 429, 500, 502, 503, 504) or a ratelimit/servererror
 * class name. exc_json: {"type":"...","message":"...","status_code":...}
 * or {"type":"...","message":"...","response":{"status_code":...}} */
bool ve_is_transient_vercel_error(const char *exc_json);

/* PoP: _coerce_text @ tools/environments/vercel_sandbox.py:_coerce_text */
/* None → "", bytes → utf-8 decode (errors=replace), else str().
 * Input is a JSON value; output is a malloc'd C string. */
char *ve_coerce_text(const char *value_json);

/* PoP: _extract_result_output @ tools/environments/vercel_sandbox.py:_extract_result_output */
/* Try result.output() then str(result). Input is a JSON object with
 * optional "output" key (callable, returns string) and optional
 * "returncode"/"exit_code" keys. Returns the output string. */
char *ve_extract_result_output(const char *result_json);

/* PoP: _extract_result_returncode @ tools/environments/vercel_sandbox.py:_extract_result_returncode */
/* Extract exit code: tries exit_code then returncode, falls back to 1.
 * Returns the int (always >= 0). */
int ve_extract_result_returncode(const char *result_json);

/* PoP: _extract_snapshot_id @ tools/environments/vercel_sandbox.py:_extract_snapshot_id */
/* Extract a snapshot ID from an object or dict, checking keys
 * "snapshot_id", "snapshotId", "id" in that order. Returns a
 * malloc'd string or NULL if not found. */
char *ve_extract_snapshot_id(const char *snapshot_json);

#ifdef __cplusplus
}
#endif

#endif /* PORT_TOOLS_VERCEL_SANDBOX_H */
