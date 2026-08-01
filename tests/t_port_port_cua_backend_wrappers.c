/* AUTO-GENERATED integration oracle harness for port_cua_backend_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_cua_backend_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int cua_u_action_result_from(const char *);
extern int cua_u_computer_use_cfg(const char *);
extern int cua_u_cua_no_overlay(const char *);
extern int cua_u_cua_telemetry_disabled(const char *);
extern int cua_u_computer_use_max_image_dimension(const char *);
extern int cua_cua_driver_child_env(const char *);
extern int cua_u_z_index_uninformative(const char *);
extern int cua_u_parse_xprop_net_active_window(const char *);
extern int cua_u_linux_x11_active_window_id(const char *);
extern int cua_u_is_real_app_window(const char *);
extern int cua_u_select_capture_target(const char *);
extern int cua_u_resolve_mcp_invocation(const char *);
extern int cua_u_mcp_args_with_overlay_flag(const char *);
extern int cua_u_cua_driver_supports_no_overlay(const char *);
extern int cua_u_has_path_separator(const char *);
extern int cua_u_candidate_cua_driver_commands(const char *);
extern int cua_resolve_cua_driver_cmd(const char *);
extern int cua_cua_driver_update_nudge(const char *);
extern int cua_cua_driver_install_hint(const char *);
extern int cua_u_parse_elements_from_structured(const char *);
extern int cua_u_require_started(const char *);
extern int cua_u_lifecycle_coro(const char *);
extern int cua_u_populate_capabilities(const char *);
extern int cua_u_start_lifecycle_locked(const char *);
extern int cua_u_stop_lifecycle_locked(const char *);
extern int cua_u_signal_shutdown_locked(const char *);
extern int cua_u_call_tool_async(const char *);
extern int cua_capabilities_discovered(const char *);
extern int cua_capability_version(const char *);
extern int cua_u_is_closed_session_error(const char *);
extern int cua_u_is_transient_daemon_error(const char *);
extern int cua_u_restart_session_locked(const char *);
extern int cua_u_call_tool_via_cli(const char *);
extern int cua_u_extract_tool_result(const char *);
extern int cua_u_image_from_tool_result(const char *);
extern int cua_u_ingest_windows(const char *);
extern int cua_u_clear_active_target(const char *);
extern int cua_u_failed_capture(const char *);
extern int cua_u_call_capture_tool(const char *);
extern int cua_u_load_windows(const char *);
extern int cua_u_match_windows_for_app(const char *);
extern int cua_u_apply_delivery(const char *);
extern int cua_set_value(const char *);
extern int cua_list_apps(const char *);
extern int cua_list_windows(const char *);
extern int cua_launch_app(const char *);
extern int cua_kill_app(const char *);
extern int cua_bring_to_front(const char *);
extern int cua_get_cursor_position(const char *);
extern int cua_get_screen_size(const char *);
extern int cua_zoom(const char *);
extern int cua_set_agent_cursor_enabled(const char *);
extern int cua_set_agent_cursor_motion(const char *);
extern int cua_set_agent_cursor_style(const char *);
extern int cua_get_agent_cursor_state(const char *);
extern int cua_start_recording(const char *);
extern int cua_stop_recording(const char *);
extern int cua_get_recording_state(const char *);
extern int cua_replay_trajectory(const char *);
extern int cua_install_ffmpeg(const char *);
extern int cua_u_maybe_attach_element_token(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_cua_u_action_result_from(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_action_result_from(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_action_result_from"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_computer_use_cfg(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_computer_use_cfg(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_computer_use_cfg"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_cua_no_overlay(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_cua_no_overlay(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_cua_no_overlay"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_cua_telemetry_disabled(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_cua_telemetry_disabled(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_cua_telemetry_disabled"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_computer_use_max_image_dimension(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_computer_use_max_image_dimension(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_computer_use_max_image_dimension"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_cua_driver_child_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_cua_driver_child_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_cua_driver_child_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_z_index_uninformative(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_z_index_uninformative(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_z_index_uninformative"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_parse_xprop_net_active_window(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_parse_xprop_net_active_window(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_parse_xprop_net_active_window"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_linux_x11_active_window_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_linux_x11_active_window_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_linux_x11_active_window_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_is_real_app_window(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_is_real_app_window(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_is_real_app_window"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_select_capture_target(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_select_capture_target(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_select_capture_target"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_resolve_mcp_invocation(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_resolve_mcp_invocation(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_resolve_mcp_invocation"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_mcp_args_with_overlay_flag(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_mcp_args_with_overlay_flag(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_mcp_args_with_overlay_flag"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_cua_driver_supports_no_overlay(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_cua_driver_supports_no_overlay(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_cua_driver_supports_no_overlay"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_has_path_separator(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_has_path_separator(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_has_path_separator"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_candidate_cua_driver_commands(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_candidate_cua_driver_commands(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_candidate_cua_driver_commands"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_resolve_cua_driver_cmd(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_resolve_cua_driver_cmd(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_resolve_cua_driver_cmd"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_cua_driver_update_nudge(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_cua_driver_update_nudge(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_cua_driver_update_nudge"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_cua_driver_install_hint(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_cua_driver_install_hint(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_cua_driver_install_hint"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_parse_elements_from_structured(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_parse_elements_from_structured(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_parse_elements_from_structured"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_require_started(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_require_started(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_require_started"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_lifecycle_coro(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_lifecycle_coro(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_lifecycle_coro"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_populate_capabilities(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_populate_capabilities(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_populate_capabilities"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_start_lifecycle_locked(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_start_lifecycle_locked(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_start_lifecycle_locked"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_stop_lifecycle_locked(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_stop_lifecycle_locked(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_stop_lifecycle_locked"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_signal_shutdown_locked(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_signal_shutdown_locked(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_signal_shutdown_locked"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_call_tool_async(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_call_tool_async(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_call_tool_async"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_capabilities_discovered(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_capabilities_discovered(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_capabilities_discovered"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_capability_version(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_capability_version(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_capability_version"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_is_closed_session_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_is_closed_session_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_is_closed_session_error"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_is_transient_daemon_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_is_transient_daemon_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_is_transient_daemon_error"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_restart_session_locked(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_restart_session_locked(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_restart_session_locked"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_call_tool_via_cli(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_call_tool_via_cli(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_call_tool_via_cli"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_extract_tool_result(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_extract_tool_result(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_extract_tool_result"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_image_from_tool_result(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_image_from_tool_result(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_image_from_tool_result"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_ingest_windows(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_ingest_windows(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_ingest_windows"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_clear_active_target(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_clear_active_target(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_clear_active_target"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_failed_capture(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_failed_capture(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_failed_capture"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_call_capture_tool(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_call_capture_tool(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_call_capture_tool"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_load_windows(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_load_windows(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_load_windows"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_match_windows_for_app(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_match_windows_for_app(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_match_windows_for_app"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_apply_delivery(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_apply_delivery(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_apply_delivery"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_set_value(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_set_value(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_set_value"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_list_apps(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_list_apps(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_list_apps"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_list_windows(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_list_windows(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_list_windows"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_launch_app(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_launch_app(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_launch_app"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_kill_app(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_kill_app(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_kill_app"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_bring_to_front(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_bring_to_front(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_bring_to_front"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_get_cursor_position(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_get_cursor_position(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_get_cursor_position"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_get_screen_size(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_get_screen_size(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_get_screen_size"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_zoom(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_zoom(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_zoom"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_set_agent_cursor_enabled(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_set_agent_cursor_enabled(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_set_agent_cursor_enabled"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_set_agent_cursor_motion(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_set_agent_cursor_motion(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_set_agent_cursor_motion"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_set_agent_cursor_style(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_set_agent_cursor_style(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_set_agent_cursor_style"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_get_agent_cursor_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_get_agent_cursor_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_get_agent_cursor_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_start_recording(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_start_recording(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_start_recording"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_stop_recording(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_stop_recording(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_stop_recording"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_get_recording_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_get_recording_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_get_recording_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_replay_trajectory(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_replay_trajectory(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_replay_trajectory"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_install_ffmpeg(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_install_ffmpeg(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_install_ffmpeg"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cua_u_maybe_attach_element_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cua_u_maybe_attach_element_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cua_u_maybe_attach_element_token"));
    json_set(o, "out", json_int(v)); return o;
}

int main(int argc, char **argv){
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    char *err = NULL; json_t *root = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }
    if (root->type != JSON_ARRAY) { fprintf(stderr, "fixture must be a JSON array\n"); free(input); return 2; }
    int n = json_array_size(root);
    for (int i = 0; i < n; i++){
        json_t *c = json_get(root, i);
        const char *op = json_get_str(c, "op", "");
        json_t *o = NULL;
        if (strcmp(op, "cua_u_action_result_from") == 0) o = emit_cua_u_action_result_from(c);
        if (strcmp(op, "cua_u_computer_use_cfg") == 0) o = emit_cua_u_computer_use_cfg(c);
        if (strcmp(op, "cua_u_cua_no_overlay") == 0) o = emit_cua_u_cua_no_overlay(c);
        if (strcmp(op, "cua_u_cua_telemetry_disabled") == 0) o = emit_cua_u_cua_telemetry_disabled(c);
        if (strcmp(op, "cua_u_computer_use_max_image_dimension") == 0) o = emit_cua_u_computer_use_max_image_dimension(c);
        if (strcmp(op, "cua_cua_driver_child_env") == 0) o = emit_cua_cua_driver_child_env(c);
        if (strcmp(op, "cua_u_z_index_uninformative") == 0) o = emit_cua_u_z_index_uninformative(c);
        if (strcmp(op, "cua_u_parse_xprop_net_active_window") == 0) o = emit_cua_u_parse_xprop_net_active_window(c);
        if (strcmp(op, "cua_u_linux_x11_active_window_id") == 0) o = emit_cua_u_linux_x11_active_window_id(c);
        if (strcmp(op, "cua_u_is_real_app_window") == 0) o = emit_cua_u_is_real_app_window(c);
        if (strcmp(op, "cua_u_select_capture_target") == 0) o = emit_cua_u_select_capture_target(c);
        if (strcmp(op, "cua_u_resolve_mcp_invocation") == 0) o = emit_cua_u_resolve_mcp_invocation(c);
        if (strcmp(op, "cua_u_mcp_args_with_overlay_flag") == 0) o = emit_cua_u_mcp_args_with_overlay_flag(c);
        if (strcmp(op, "cua_u_cua_driver_supports_no_overlay") == 0) o = emit_cua_u_cua_driver_supports_no_overlay(c);
        if (strcmp(op, "cua_u_has_path_separator") == 0) o = emit_cua_u_has_path_separator(c);
        if (strcmp(op, "cua_u_candidate_cua_driver_commands") == 0) o = emit_cua_u_candidate_cua_driver_commands(c);
        if (strcmp(op, "cua_resolve_cua_driver_cmd") == 0) o = emit_cua_resolve_cua_driver_cmd(c);
        if (strcmp(op, "cua_cua_driver_update_nudge") == 0) o = emit_cua_cua_driver_update_nudge(c);
        if (strcmp(op, "cua_cua_driver_install_hint") == 0) o = emit_cua_cua_driver_install_hint(c);
        if (strcmp(op, "cua_u_parse_elements_from_structured") == 0) o = emit_cua_u_parse_elements_from_structured(c);
        if (strcmp(op, "cua_u_require_started") == 0) o = emit_cua_u_require_started(c);
        if (strcmp(op, "cua_u_lifecycle_coro") == 0) o = emit_cua_u_lifecycle_coro(c);
        if (strcmp(op, "cua_u_populate_capabilities") == 0) o = emit_cua_u_populate_capabilities(c);
        if (strcmp(op, "cua_u_start_lifecycle_locked") == 0) o = emit_cua_u_start_lifecycle_locked(c);
        if (strcmp(op, "cua_u_stop_lifecycle_locked") == 0) o = emit_cua_u_stop_lifecycle_locked(c);
        if (strcmp(op, "cua_u_signal_shutdown_locked") == 0) o = emit_cua_u_signal_shutdown_locked(c);
        if (strcmp(op, "cua_u_call_tool_async") == 0) o = emit_cua_u_call_tool_async(c);
        if (strcmp(op, "cua_capabilities_discovered") == 0) o = emit_cua_capabilities_discovered(c);
        if (strcmp(op, "cua_capability_version") == 0) o = emit_cua_capability_version(c);
        if (strcmp(op, "cua_u_is_closed_session_error") == 0) o = emit_cua_u_is_closed_session_error(c);
        if (strcmp(op, "cua_u_is_transient_daemon_error") == 0) o = emit_cua_u_is_transient_daemon_error(c);
        if (strcmp(op, "cua_u_restart_session_locked") == 0) o = emit_cua_u_restart_session_locked(c);
        if (strcmp(op, "cua_u_call_tool_via_cli") == 0) o = emit_cua_u_call_tool_via_cli(c);
        if (strcmp(op, "cua_u_extract_tool_result") == 0) o = emit_cua_u_extract_tool_result(c);
        if (strcmp(op, "cua_u_image_from_tool_result") == 0) o = emit_cua_u_image_from_tool_result(c);
        if (strcmp(op, "cua_u_ingest_windows") == 0) o = emit_cua_u_ingest_windows(c);
        if (strcmp(op, "cua_u_clear_active_target") == 0) o = emit_cua_u_clear_active_target(c);
        if (strcmp(op, "cua_u_failed_capture") == 0) o = emit_cua_u_failed_capture(c);
        if (strcmp(op, "cua_u_call_capture_tool") == 0) o = emit_cua_u_call_capture_tool(c);
        if (strcmp(op, "cua_u_load_windows") == 0) o = emit_cua_u_load_windows(c);
        if (strcmp(op, "cua_u_match_windows_for_app") == 0) o = emit_cua_u_match_windows_for_app(c);
        if (strcmp(op, "cua_u_apply_delivery") == 0) o = emit_cua_u_apply_delivery(c);
        if (strcmp(op, "cua_set_value") == 0) o = emit_cua_set_value(c);
        if (strcmp(op, "cua_list_apps") == 0) o = emit_cua_list_apps(c);
        if (strcmp(op, "cua_list_windows") == 0) o = emit_cua_list_windows(c);
        if (strcmp(op, "cua_launch_app") == 0) o = emit_cua_launch_app(c);
        if (strcmp(op, "cua_kill_app") == 0) o = emit_cua_kill_app(c);
        if (strcmp(op, "cua_bring_to_front") == 0) o = emit_cua_bring_to_front(c);
        if (strcmp(op, "cua_get_cursor_position") == 0) o = emit_cua_get_cursor_position(c);
        if (strcmp(op, "cua_get_screen_size") == 0) o = emit_cua_get_screen_size(c);
        if (strcmp(op, "cua_zoom") == 0) o = emit_cua_zoom(c);
        if (strcmp(op, "cua_set_agent_cursor_enabled") == 0) o = emit_cua_set_agent_cursor_enabled(c);
        if (strcmp(op, "cua_set_agent_cursor_motion") == 0) o = emit_cua_set_agent_cursor_motion(c);
        if (strcmp(op, "cua_set_agent_cursor_style") == 0) o = emit_cua_set_agent_cursor_style(c);
        if (strcmp(op, "cua_get_agent_cursor_state") == 0) o = emit_cua_get_agent_cursor_state(c);
        if (strcmp(op, "cua_start_recording") == 0) o = emit_cua_start_recording(c);
        if (strcmp(op, "cua_stop_recording") == 0) o = emit_cua_stop_recording(c);
        if (strcmp(op, "cua_get_recording_state") == 0) o = emit_cua_get_recording_state(c);
        if (strcmp(op, "cua_replay_trajectory") == 0) o = emit_cua_replay_trajectory(c);
        if (strcmp(op, "cua_install_ffmpeg") == 0) o = emit_cua_install_ffmpeg(c);
        if (strcmp(op, "cua_u_maybe_attach_element_token") == 0) o = emit_cua_u_maybe_attach_element_token(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
