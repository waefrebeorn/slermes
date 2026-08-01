"""AUTO-GENERATED integration oracle for port_cua_backend_wrappers (gen_integration_oracle.py)."""
import os, sys, json, importlib.util

MODS = {}
def _load(rel):
    _repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    if _repo not in sys.path: sys.path.insert(0, _repo)
    for base in sys.path:
        cand = os.path.join(base, rel)
        try:
            spec = importlib.util.spec_from_file_location('live_' + rel.replace('/', '_').replace('.', '_'), cand)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
        except Exception:
            continue
    return None
MODS['tools.computer_use.cua_backend'] = _load('tools/computer_use/cua_backend.py')

DISPATCH = {
    'cua_u_action_result_from': ('tools.computer_use.cua_backend', '_action_result_from'),
    'cua_u_computer_use_cfg': ('tools.computer_use.cua_backend', '_computer_use_cfg'),
    'cua_u_cua_no_overlay': ('tools.computer_use.cua_backend', '_cua_no_overlay'),
    'cua_u_cua_telemetry_disabled': ('tools.computer_use.cua_backend', '_cua_telemetry_disabled'),
    'cua_u_computer_use_max_image_dimension': ('tools.computer_use.cua_backend', '_computer_use_max_image_dimension'),
    'cua_cua_driver_child_env': ('tools.computer_use.cua_backend', 'cua_driver_child_env'),
    'cua_u_z_index_uninformative': ('tools.computer_use.cua_backend', '_z_index_uninformative'),
    'cua_u_parse_xprop_net_active_window': ('tools.computer_use.cua_backend', '_parse_xprop_net_active_window'),
    'cua_u_linux_x11_active_window_id': ('tools.computer_use.cua_backend', '_linux_x11_active_window_id'),
    'cua_u_is_real_app_window': ('tools.computer_use.cua_backend', '_is_real_app_window'),
    'cua_u_select_capture_target': ('tools.computer_use.cua_backend', '_select_capture_target'),
    'cua_u_resolve_mcp_invocation': ('tools.computer_use.cua_backend', '_resolve_mcp_invocation'),
    'cua_u_mcp_args_with_overlay_flag': ('tools.computer_use.cua_backend', '_mcp_args_with_overlay_flag'),
    'cua_u_cua_driver_supports_no_overlay': ('tools.computer_use.cua_backend', '_cua_driver_supports_no_overlay'),
    'cua_u_has_path_separator': ('tools.computer_use.cua_backend', '_has_path_separator'),
    'cua_u_candidate_cua_driver_commands': ('tools.computer_use.cua_backend', '_candidate_cua_driver_commands'),
    'cua_resolve_cua_driver_cmd': ('tools.computer_use.cua_backend', 'resolve_cua_driver_cmd'),
    'cua_cua_driver_update_nudge': ('tools.computer_use.cua_backend', 'cua_driver_update_nudge'),
    'cua_cua_driver_install_hint': ('tools.computer_use.cua_backend', 'cua_driver_install_hint'),
    'cua_u_parse_elements_from_structured': ('tools.computer_use.cua_backend', '_parse_elements_from_structured'),
    'cua_u_require_started': ('tools.computer_use.cua_backend', '_require_started'),
    'cua_u_lifecycle_coro': ('tools.computer_use.cua_backend', '_lifecycle_coro'),
    'cua_u_populate_capabilities': ('tools.computer_use.cua_backend', '_populate_capabilities'),
    'cua_u_start_lifecycle_locked': ('tools.computer_use.cua_backend', '_start_lifecycle_locked'),
    'cua_u_stop_lifecycle_locked': ('tools.computer_use.cua_backend', '_stop_lifecycle_locked'),
    'cua_u_signal_shutdown_locked': ('tools.computer_use.cua_backend', '_signal_shutdown_locked'),
    'cua_u_call_tool_async': ('tools.computer_use.cua_backend', '_call_tool_async'),
    'cua_capabilities_discovered': ('tools.computer_use.cua_backend', 'capabilities_discovered'),
    'cua_capability_version': ('tools.computer_use.cua_backend', 'capability_version'),
    'cua_u_is_closed_session_error': ('tools.computer_use.cua_backend', '_is_closed_session_error'),
    'cua_u_is_transient_daemon_error': ('tools.computer_use.cua_backend', '_is_transient_daemon_error'),
    'cua_u_restart_session_locked': ('tools.computer_use.cua_backend', '_restart_session_locked'),
    'cua_u_call_tool_via_cli': ('tools.computer_use.cua_backend', '_call_tool_via_cli'),
    'cua_u_extract_tool_result': ('tools.computer_use.cua_backend', '_extract_tool_result'),
    'cua_u_image_from_tool_result': ('tools.computer_use.cua_backend', '_image_from_tool_result'),
    'cua_u_ingest_windows': ('tools.computer_use.cua_backend', '_ingest_windows'),
    'cua_u_clear_active_target': ('tools.computer_use.cua_backend', '_clear_active_target'),
    'cua_u_failed_capture': ('tools.computer_use.cua_backend', '_failed_capture'),
    'cua_u_call_capture_tool': ('tools.computer_use.cua_backend', '_call_capture_tool'),
    'cua_u_load_windows': ('tools.computer_use.cua_backend', '_load_windows'),
    'cua_u_match_windows_for_app': ('tools.computer_use.cua_backend', '_match_windows_for_app'),
    'cua_u_apply_delivery': ('tools.computer_use.cua_backend', '_apply_delivery'),
    'cua_set_value': ('tools.computer_use.cua_backend', 'set_value'),
    'cua_list_apps': ('tools.computer_use.cua_backend', 'list_apps'),
    'cua_list_windows': ('tools.computer_use.cua_backend', 'list_windows'),
    'cua_launch_app': ('tools.computer_use.cua_backend', 'launch_app'),
    'cua_kill_app': ('tools.computer_use.cua_backend', 'kill_app'),
    'cua_bring_to_front': ('tools.computer_use.cua_backend', 'bring_to_front'),
    'cua_get_cursor_position': ('tools.computer_use.cua_backend', 'get_cursor_position'),
    'cua_get_screen_size': ('tools.computer_use.cua_backend', 'get_screen_size'),
    'cua_zoom': ('tools.computer_use.cua_backend', 'zoom'),
    'cua_set_agent_cursor_enabled': ('tools.computer_use.cua_backend', 'set_agent_cursor_enabled'),
    'cua_set_agent_cursor_motion': ('tools.computer_use.cua_backend', 'set_agent_cursor_motion'),
    'cua_set_agent_cursor_style': ('tools.computer_use.cua_backend', 'set_agent_cursor_style'),
    'cua_get_agent_cursor_state': ('tools.computer_use.cua_backend', 'get_agent_cursor_state'),
    'cua_start_recording': ('tools.computer_use.cua_backend', 'start_recording'),
    'cua_stop_recording': ('tools.computer_use.cua_backend', 'stop_recording'),
    'cua_get_recording_state': ('tools.computer_use.cua_backend', 'get_recording_state'),
    'cua_replay_trajectory': ('tools.computer_use.cua_backend', 'replay_trajectory'),
    'cua_install_ffmpeg': ('tools.computer_use.cua_backend', 'install_ffmpeg'),
    'cua_u_maybe_attach_element_token': ('tools.computer_use.cua_backend', '_maybe_attach_element_token'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_cua_backend_wrappers.py <cases.json>\n"); return 2
    with open(sys.argv[1], 'r', encoding='utf-8') as f: cases = json.load(f)
    for c in cases:
        op = c.get('op'); value = c.get('value', '')
        d = DISPATCH.get(op)
        if not d: sys.stdout.write(json.dumps({'fn':op}, separators=(',',':')) + '\n'); continue
        pymod, pyfn = d
        mod = MODS.get(pymod)
        try:
            out = getattr(mod, pyfn)(value) if mod else None
        except Exception as e:
            out = 'PYERR:' + str(e)
        if isinstance(out, bool): out = bool(out)
        elif isinstance(out, (int, float)) and not isinstance(out, bool): out = int(out)
        elif out is None: out = ''
        else: out = str(out)
        sys.stdout.write(json.dumps({'fn':op,'out':out}, ensure_ascii=True, separators=(',',':')) + '\n')
    return 0

if __name__ == '__main__':
    sys.exit(main())
