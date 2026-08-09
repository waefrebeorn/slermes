/*
 * cua_backend_methods.h — real ports of the pure-logic REAL_GAP methods from
 * tools/computer_use/cua_backend.py. Opaque, minimal includes.
 *
 * Depends on libjson (json_t) and cua_backend_helpers.h. No god header.
 */
#ifndef CUA_BACKEND_METHODS_H
#define CUA_BACKEND_METHODS_H

#include <stdbool.h>
#include <stddef.h>
#include <json.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: _positive_int @ tools/computer_use/cua_backend.py:_positive_int */
/* Faithful port of Python's _positive_int. Returns -1 for bools / non int-str
 * / non-positive, else the positive integer. */

/* PoP: _ingest_windows @ tools/computer_use/cua_backend.py:_ingest_windows */
/* Normalise list_windows entries. Caller frees with json_free. */
json_t *cua_ingest_windows(const json_t *raw_windows);

/* PoP: child_env @ tools/computer_use/cua_backend.py:_EmbeddedCuaDaemon.child_env */
/* Build env JSON with CUA_DRIVER_PERMISSION_MODE + DANGEROUSLY_BYPASS. */
json_t *cua_daemon_child_env(const json_t *base_env);

/* PoP: _drain_stderr @ tools/computer_use/cua_backend.py:_EmbeddedCuaDaemon._drain_stderr */
/* Append non-empty stripped lines from stderr_text (newline-separated) to the
 * deque-backed stderr_tail JSON array. */
void cua_daemon_drain_stderr(json_t *stderr_tail, const char *stderr_text);

/* PoP: proxy_invocation @ tools/computer_use/cua_backend.py:_EmbeddedCuaDaemon.proxy_invocation */
/* Serialize (command, args + embedded/socket) as "command\targ1,arg2,...".
 * Returns NULL if daemon not running (process==NULL / dead). */
char *cua_daemon_proxy_invocation(const char *command,
                                  const json_t *mcp_args,
                                  const char *socket_path);

/* PoP: supports_input_property @ tools/computer_use/cua_backend.py:_CuaDriverSession.supports_input_property */
bool cua_session_supports_input_property(const json_t *tool_schemas,
                                         const char *tool,
                                         const char *property_name);

/* PoP: _logical_error_text @ tools/computer_use/cua_backend.py:_CuaDriverSession._logical_error_text */
/* Flatten result["data"] + result["structuredContent"] into text. Caller frees. */
char *cua_session_logical_error_text(const json_t *result);

/* PoP: _is_ended_session_result @ tools/computer_use/cua_backend.py:_CuaDriverSession._is_ended_session_result */
bool cua_session_is_ended_session_result(const json_t *result);

/* PoP: _windows_from_tool_result @ tools/computer_use/cua_backend.py:_windows_from_tool_result */
/* Extract windows list across result shapes. Caller frees with json_free. */
json_t *cua_windows_from_tool_result(const json_t *result);

/* PoP: _apps_from_windows @ tools/computer_use/cua_backend.py:_apps_from_windows */
/* Dedupe normalised windows to {name, pid}. Caller frees with json_free. */
json_t *cua_apps_from_windows(const json_t *raw_windows);

#ifdef __cplusplus
}
#endif
#endif /* CUA_BACKEND_METHODS_H */
