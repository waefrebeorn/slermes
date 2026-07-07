#ifndef SLERMES_PORT_BROWSER_TOOL_H
#define SLERMES_PORT_BROWSER_TOOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct json_t json_t;
typedef struct port_browser_tool_state port_browser_tool_state_t;

/* Lifecycle */
port_browser_tool_state_t *port_browser_tool_init(void);
void port_browser_tool_cleanup(port_browser_tool_state_t *state);

/* Environment & config */
char *port_browser_tool_build_browser_env(port_browser_tool_state_t *state);
int port_browser_tool_get_command_timeout(port_browser_tool_state_t *state);
int port_browser_tool_safe_command_timeout(port_browser_tool_state_t *state);
int port_browser_tool_get_open_command_timeout(port_browser_tool_state_t *state, bool first_open);
int port_browser_tool_get_session_inactivity_timeout(port_browser_tool_state_t *state);
char *port_browser_tool_get_dialog_policy_config(port_browser_tool_state_t *state);
char *port_browser_tool_get_browser_engine(port_browser_tool_state_t *state);

/* URL & security */
char *port_browser_tool_sanitize_url_for_logs(port_browser_tool_state_t *state, const char *value);
char *port_browser_tool_redact_browser_output(port_browser_tool_state_t *state, const char *value_json);
bool port_browser_tool_needs_chromium_sandbox_bypass(port_browser_tool_state_t *state);
bool port_browser_tool_eval_ssrf_guard_active(port_browser_tool_state_t *state, const char *effective_task_id);
bool port_browser_tool_is_always_blocked_url(port_browser_tool_state_t *state, const char *url);
bool port_browser_tool_is_safe_url(port_browser_tool_state_t *state, const char *url);
bool port_browser_tool_url_is_private(port_browser_tool_state_t *state, const char *url);
char *port_browser_tool_blocked_private_page_action(port_browser_tool_state_t *state, const char *effective_task_id, const char *action);
char *port_browser_tool_current_page_private_url(port_browser_tool_state_t *state, const char *effective_task_id);
char *port_browser_tool_expression_targets_private_url(port_browser_tool_state_t *state, const char *expression);
bool port_browser_tool_auto_local_for_private_urls(port_browser_tool_state_t *state);

/* Platform detection */
bool port_browser_tool_running_in_docker(port_browser_tool_state_t *state);
bool port_browser_tool_is_local_mode(port_browser_tool_state_t *state);
bool port_browser_tool_is_local_backend(port_browser_tool_state_t *state);
bool port_browser_tool_is_local_sidecar_key(port_browser_tool_state_t *state, const char *session_key);
bool port_browser_tool_allow_private_urls(port_browser_tool_state_t *state);
bool port_browser_tool_is_camofox_mode(port_browser_tool_state_t *state);
bool port_browser_tool_chromium_installed(port_browser_tool_state_t *state);
bool port_browser_tool_maybe_autoinstall_chromium(port_browser_tool_state_t *state);
bool port_browser_tool_requires_real_termux_browser_install(port_browser_tool_state_t *state);
char *port_browser_tool_termux_browser_install_error(port_browser_tool_state_t *state);
bool port_browser_tool_should_inject_engine(port_browser_tool_state_t *state);
bool port_browser_tool_using_lightpanda_engine(port_browser_tool_state_t *state);
bool port_browser_tool_needs_lightpanda_fallback(port_browser_tool_state_t *state);
char *port_browser_tool_lightpanda_fallback_reason(port_browser_tool_state_t *state);
void port_browser_tool_annotate_lightpanda_fallback(port_browser_tool_state_t *state, const char *context);
void port_browser_tool_copy_fallback_warning(port_browser_tool_state_t *state, char *dest, size_t dest_size);
char *port_browser_tool_run_chrome_fallback_command(port_browser_tool_state_t *state, const char *cmd);
char *port_browser_tool_chrome_fallback_screenshot(port_browser_tool_state_t *state);

/* JS evaluation safety */
bool port_browser_tool_allow_unsafe_browser_evaluate(port_browser_tool_state_t *state);
char *port_browser_tool_decode_js_string_literal(port_browser_tool_state_t *state, const char *literal);
char **port_browser_tool_decoded_js_string_literals(port_browser_tool_state_t *state, const char *expression, int *out_count);
char *port_browser_tool_sensitive_browser_eval_token_reason(port_browser_tool_state_t *state, const char *expression);
char *port_browser_tool_risky_browser_eval_reason(port_browser_tool_state_t *state, const char *expression);
char *port_browser_tool_enforce_browser_eval_policy(port_browser_tool_state_t *state, const char *expression);

/* Command I/O */
void port_browser_tool_read_command_output_files(port_browser_tool_state_t *state, const char *stdout_path, const char *stderr_path, char **out_stdout, char **out_stderr);
void port_browser_tool_unlink_command_output_files(port_browser_tool_state_t *state, int count, const char **paths);
char *port_browser_tool_format_timeout_error(port_browser_tool_state_t *state, const char *command, int timeout, const char *stdout_text, const char *stderr_text);
bool port_browser_tool_agent_browser_candidate_present(port_browser_tool_state_t *state, const char *path);
bool port_browser_tool_verify_reapable_browser_daemon(port_browser_tool_state_t *state, int daemon_pid, const char *socket_dir, const char *session_name);

/* Session management */
char *port_browser_tool_bare_task_id_for_session_key(port_browser_tool_state_t *state, const char *session_key);
bool port_browser_tool_session_info_owned_by_task(port_browser_tool_state_t *state, const char *session_info_json, const char *task_id, const char *session_key);
json_t *port_browser_tool_ensure_cdp_supervisor(port_browser_tool_state_t *state, const char *task_id);
json_t *port_browser_tool_stop_cdp_supervisor(port_browser_tool_state_t *state, const char *task_id);
bool port_browser_tool_is_legacy_provider_registry_overridden(port_browser_tool_state_t *state);
void port_browser_tool_ensure_browser_plugins_loaded(port_browser_tool_state_t *state);
char *port_browser_tool_get_cloud_provider(port_browser_tool_state_t *state);
char *port_browser_tool_browser_install_hint(port_browser_tool_state_t *state);
char *port_browser_tool_navigation_session_key(port_browser_tool_state_t *state, const char *task_id);
char *port_browser_tool_last_session_key(port_browser_tool_state_t *state);
char *port_browser_tool_socket_safe_tmpdir(port_browser_tool_state_t *state);
void port_browser_tool_emergency_cleanup_all_sessions(port_browser_tool_state_t *state);
void port_browser_tool_cleanup_inactive_browser_sessions(port_browser_tool_state_t *state);
bool port_browser_tool_write_owner_pid(port_browser_tool_state_t *state, const char *session_key);
void port_browser_tool_reap_orphaned_browser_sessions(port_browser_tool_state_t *state);
void *port_browser_tool_cleanup_thread_worker(port_browser_tool_state_t *state, void *arg);
bool port_browser_tool_start_browser_cleanup_thread(port_browser_tool_state_t *state);
void port_browser_tool_stop_browser_cleanup_thread(port_browser_tool_state_t *state);
void port_browser_tool_update_session_activity(port_browser_tool_state_t *state, const char *session_key);
json_t *port_browser_tool_create_local_session(port_browser_tool_state_t *state, const char *task_id);
json_t *port_browser_tool_create_cdp_session(port_browser_tool_state_t *state, const char *task_id, const char *cdp_url);
json_t *port_browser_tool_get_session_info(port_browser_tool_state_t *state, const char *session_key);

/* Path discovery */
char *port_browser_tool_discover_homebrew_node_dirs(port_browser_tool_state_t *state);
char *port_browser_tool_browser_candidate_path_dirs(port_browser_tool_state_t *state);
char *port_browser_tool_merge_browser_path(port_browser_tool_state_t *state, const char *existing_path);
char *port_browser_tool_find_agent_browser(port_browser_tool_state_t *state);
char *port_browser_tool_chromium_search_roots(port_browser_tool_state_t *state);

/* Model/endpoint */
char *port_browser_tool_get_vision_model(port_browser_tool_state_t *state);
char *port_browser_tool_get_extraction_model(port_browser_tool_state_t *state);
char *port_browser_tool_resolve_cdp_override(port_browser_tool_state_t *state, const char *cdp_url);
char *port_browser_tool_get_cdp_override(port_browser_tool_state_t *state);

/* Snapshot & eval */
char *port_browser_tool_extract_screenshot_path_from_text(port_browser_tool_state_t *state, const char *text);
char *port_browser_tool_run_browser_command(port_browser_tool_state_t *state, const char *expression, const char *task_id);
char *port_browser_tool_browser_eval(port_browser_tool_state_t *state, const char *expression, const char *task_id);
char *port_browser_tool_camofox_eval(port_browser_tool_state_t *state, const char *expression, const char *task_id);
char *port_browser_tool_extract_relevant_content(port_browser_tool_state_t *state, const char *snapshot, const char *task);
char *port_browser_tool_truncate_snapshot(port_browser_tool_state_t *state, const char *snapshot, size_t max_chars);

/* Recording & cleanup */
void port_browser_tool_maybe_start_recording(port_browser_tool_state_t *state, const char *task_id);
void port_browser_tool_maybe_stop_recording(port_browser_tool_state_t *state, const char *task_id);
void port_browser_tool_cleanup_old_screenshots(port_browser_tool_state_t *state);
void port_browser_tool_cleanup_old_recordings(port_browser_tool_state_t *state);
void port_browser_tool_cleanup_single_browser_session(port_browser_tool_state_t *state, const char *session_key);

/* Requirements */
json_t *port_browser_tool_check_browser_requirements(port_browser_tool_state_t *state);
json_t *port_browser_tool_check_browser_vision_requirements(port_browser_tool_state_t *state);

#endif /* SLERMES_PORT_BROWSER_TOOL_H */