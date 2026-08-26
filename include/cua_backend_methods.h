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


/* ── REAL_GAP closure batch (config / launch-args / session-lock / transport) ── */
#include <pthread.h>
#include <sys/types.h>

/* PoP: _mcp_field @ tools/computer_use/cua_backend.py:_mcp_field */
json_t *cua_mcp_field(const json_t *obj, const char *snake, const char *camel,
                      json_t *default_val);
/* PoP: _cua_configured_permission_mode @ tools/computer_use/cua_backend.py:_cua_configured_permission_mode */
char *cua_configured_permission_mode(const json_t *computer_use_cfg);
/* PoP: _cua_capability_manifest @ tools/computer_use/cua_backend.py:_cua_capability_manifest */
char *cua_capability_manifest(const json_t *computer_use_cfg);
/* PoP: _cua_grant_existing_profile @ tools/computer_use/cua_backend.py:_cua_grant_existing_profile */
bool cua_grant_existing_profile(const json_t *computer_use_cfg);
/* PoP: _manifest_is_mode_independent @ tools/computer_use/cua_backend.py:_manifest_is_mode_independent */
bool cua_manifest_is_mode_independent(const char *path);
/* PoP: _standard_runtime_launch_args @ tools/computer_use/cua_backend.py:_standard_runtime_launch_args */
json_t *cua_standard_runtime_launch_args(const json_t *args,
                                         bool grant_existing_profile,
                                         const char *platform,
                                         const char *socket_path);
/* PoP: _linux_session_locked @ tools/computer_use/cua_backend.py:_linux_session_locked (1/0/-1) */
int cua_linux_session_locked(void);
/* PoP: _empty_discovery_reason @ tools/computer_use/cua_backend.py:_empty_discovery_reason */
char *cua_empty_discovery_reason(void);
/* PoP: _EmbeddedCuaDaemon.stop @ tools/computer_use/cua_backend.py:_EmbeddedCuaDaemon.stop */
void cua_daemon_stop(const char *command, const char *socket_path, pid_t owned_pid,
                     const json_t *child_env);
/* PoP: _AsyncBridge.stop @ tools/computer_use/cua_backend.py:_AsyncBridge.stop */
void cua_bridge_stop(pthread_t *thread, volatile int *loop_running,
                     void (*loop_stop_hook)(void *), void *loop_userdata);
/* PoP: _CuaDriverSession.stop @ tools/computer_use/cua_backend.py:_CuaDriverSession.stop */
void cua_session_stop(bool *started_flag,
                      void (*stop_lifecycle_locked)(void *), void *lifecycle_ud,
                      char **owned_socket_slot);
/* PoP: _CuaDriverSession.set_transport_reset_callback @ tools/computer_use/cua_backend.py:set_transport_reset_callback */
void cua_session_set_transport_reset_callback(
    void (**out_callback)(void *), void **out_userdata,
    void (*callback)(void *), void *userdata);
/* PoP: _CuaDriverSession._notify_transport_reset @ tools/computer_use/cua_backend.py:_notify_transport_reset */
void cua_session_notify_transport_reset(void (**callback)(void *), void **userdata);
/* PoP: _CuaDriverSession._stop_owned_standard_runtime_locked @ tools/computer_use/cua_backend.py:_stop_owned_standard_runtime_locked */
void cua_session_stop_owned_standard_runtime_locked(char **socket_path_slot,
                                                    char *driver_command_override);
/* PoP: _CuaDriverSession._restore_declared_session_after_transport_reset @ tools/computer_use/cua_backend.py:_restore_declared_session_after_transport_reset */
void cua_session_restore_declared_session_after_transport_reset(
    const char *declared_session_id, double timeout,
    json_t *(*call_tool)(const char *tool, const json_t *args, void *ud),
    void *ud);
/* PoP: _CuaDriverSession._transport_replay_is_safe @ tools/computer_use/cua_backend.py:_transport_replay_is_safe */
bool cua_transport_replay_is_safe(const char *name);
/* PoP: _CuaDriverSession._unknown_transport_outcome @ tools/computer_use/cua_backend.py:_unknown_transport_outcome */
json_t *cua_unknown_transport_outcome(const char *name, const char *exc_text);
/* PoP: _CuaDriverSession._timeout_outcome @ tools/computer_use/cua_backend.py:_timeout_outcome */
json_t *cua_timeout_outcome(const char *name, const char *exc_text);
/* installer hook for _maybe_repair_runtime_contract (lazy install_cua_driver) */
void cua_backend_set_contract_install_hook(int (*hook)(bool upgrade, bool show_installer_progress));
/* PoP: _maybe_repair_runtime_contract @ tools/computer_use/cua_backend.py:_maybe_repair_runtime_contract */
json_t *cua_maybe_repair_runtime_contract(const json_t *contract);
/* PoP: _is_placeholder_id @ tools/computer_use/cua_backend.py:_is_placeholder_id */
bool cua_is_placeholder_id(const json_t *value);
/* PoP: CuaDriverBackend._handle_transport_reset @ tools/computer_use/cua_backend.py:_handle_transport_reset */
void cua_backend_handle_transport_reset(void (*clear_active_target)(void *),
                                        void *clear_ud,
                                        json_t *typed_browser_route_state);
/* PoP: CuaDriverBackend.drag @ tools/computer_use/cua_backend.py:drag */
json_t *cua_backend_drag(long active_pid, long active_window_id,
                         long from_element, long to_element, int use_elements,
                         long from_x, long from_y, long to_x, long to_y,
                         int use_coordinates,
                         json_t *(*run_input_action)(const char *action,
                                                     const json_t *args,
                                                     void *ud),
                         void *ud);

#ifdef __cplusplus
}
#endif
#endif /* CUA_BACKEND_METHODS_H */
