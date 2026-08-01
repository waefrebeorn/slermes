"""AUTO-GENERATED integration oracle for port_auth_wrappers (gen_integration_oracle.py)."""
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
MODS['hermes_cli.auth'] = _load('hermes_cli/auth.py')

DISPATCH = {
    'auth_u_resolve_api_key_provider_secret': ('hermes_cli.auth', '_resolve_api_key_provider_secret'),
    'auth_detect_zai_endpoint': ('hermes_cli.auth', 'detect_zai_endpoint'),
    'auth_u_resolve_zai_base_url': ('hermes_cli.auth', '_resolve_zai_base_url'),
    'auth_u_format_nous_entitlement_auth_error': ('hermes_cli.auth', '_format_nous_entitlement_auth_error'),
    'auth_u_auth_lock_holder_for': ('hermes_cli.auth', '_auth_lock_holder_for'),
    'auth_u_get_config_hint_for_unknown_provider': ('hermes_cli.auth', '_get_config_hint_for_unknown_provider'),
    'auth_u_parse_iso_timestamp': ('hermes_cli.auth', '_parse_iso_timestamp'),
    'auth_u_read_qwen_cli_tokens': ('hermes_cli.auth', '_read_qwen_cli_tokens'),
    'auth_u_save_qwen_cli_tokens': ('hermes_cli.auth', '_save_qwen_cli_tokens'),
    'auth_u_refresh_qwen_cli_tokens': ('hermes_cli.auth', '_refresh_qwen_cli_tokens'),
    'auth_u_mark_qwen_oauth_active': ('hermes_cli.auth', '_mark_qwen_oauth_active'),
    'auth_resolve_qwen_runtime_credentials': ('hermes_cli.auth', 'resolve_qwen_runtime_credentials'),
    'auth_get_qwen_auth_status': ('hermes_cli.auth', 'get_qwen_auth_status'),
    'auth_u_make_spotify_callback_handler': ('hermes_cli.auth', '_make_spotify_callback_handler'),
    'auth_u_spotify_wait_for_callback': ('hermes_cli.auth', '_spotify_wait_for_callback'),
    'auth_u_spotify_token_payload_to_state': ('hermes_cli.auth', '_spotify_token_payload_to_state'),
    'auth_u_spotify_exchange_code_for_tokens': ('hermes_cli.auth', '_spotify_exchange_code_for_tokens'),
    'auth_u_refresh_spotify_oauth_state': ('hermes_cli.auth', '_refresh_spotify_oauth_state'),
    'auth_resolve_spotify_runtime_credentials': ('hermes_cli.auth', 'resolve_spotify_runtime_credentials'),
    'auth_get_spotify_auth_status': ('hermes_cli.auth', 'get_spotify_auth_status'),
    'auth_u_spotify_interactive_setup': ('hermes_cli.auth', '_spotify_interactive_setup'),
    'auth_login_spotify_command': ('hermes_cli.auth', 'login_spotify_command'),
    'auth_u_is_remote_session': ('hermes_cli.auth', '_is_remote_session'),
    'auth_u_can_open_graphical_browser': ('hermes_cli.auth', '_can_open_graphical_browser'),
    'auth_u_print_loopback_ssh_hint': ('hermes_cli.auth', '_print_loopback_ssh_hint'),
    'auth_u_read_codex_tokens': ('hermes_cli.auth', '_read_codex_tokens'),
    'auth_u_sync_codex_pool_entries': ('hermes_cli.auth', '_sync_codex_pool_entries'),
    'auth_u_save_codex_tokens': ('hermes_cli.auth', '_save_codex_tokens'),
    'auth_u_recover_codex_tokens_from_cli': ('hermes_cli.auth', '_recover_codex_tokens_from_cli'),
    'auth_refresh_codex_oauth_pure': ('hermes_cli.auth', 'refresh_codex_oauth_pure'),
    'auth_u_refresh_codex_auth_tokens': ('hermes_cli.auth', '_refresh_codex_auth_tokens'),
    'auth_u_import_codex_cli_tokens': ('hermes_cli.auth', '_import_codex_cli_tokens'),
    'auth_resolve_codex_runtime_credentials': ('hermes_cli.auth', 'resolve_codex_runtime_credentials'),
    'auth_u_is_codex_rate_limit_shaped': ('hermes_cli.auth', '_is_codex_rate_limit_shaped'),
    'auth_u_codex_usage_probe_url': ('hermes_cli.auth', '_codex_usage_probe_url'),
    'auth_u_probe_codex_quota_restored': ('hermes_cli.auth', '_probe_codex_quota_restored'),
    'auth_clear_codex_pool_quota_cooldowns': ('hermes_cli.auth', 'clear_codex_pool_quota_cooldowns'),
    'auth_u_pool_codex_access_token': ('hermes_cli.auth', '_pool_codex_access_token'),
    'auth_u_read_xai_oauth_tokens': ('hermes_cli.auth', '_read_xai_oauth_tokens'),
    'auth_u_save_xai_oauth_tokens': ('hermes_cli.auth', '_save_xai_oauth_tokens'),
    'auth_u_xai_access_token_is_expiring': ('hermes_cli.auth', '_xai_access_token_is_expiring'),
    'auth_u_xai_proactive_refresh_skew_seconds': ('hermes_cli.auth', '_xai_proactive_refresh_skew_seconds'),
    'auth_u_xai_validate_oauth_endpoint': ('hermes_cli.auth', '_xai_validate_oauth_endpoint'),
    'auth_u_xai_validate_inference_base_url': ('hermes_cli.auth', '_xai_validate_inference_base_url'),
    'auth_u_xai_oauth_discovery': ('hermes_cli.auth', '_xai_oauth_discovery'),
    'auth_refresh_xai_oauth_pure': ('hermes_cli.auth', 'refresh_xai_oauth_pure'),
    'auth_u_refresh_xai_oauth_tokens': ('hermes_cli.auth', '_refresh_xai_oauth_tokens'),
    'auth_resolve_xai_oauth_runtime_credentials': ('hermes_cli.auth', 'resolve_xai_oauth_runtime_credentials'),
    'auth_u_request_device_code': ('hermes_cli.auth', '_request_device_code'),
    'auth_u_poll_for_token': ('hermes_cli.auth', '_poll_for_token'),
    'auth_u_try_import_shared_nous_state': ('hermes_cli.auth', '_try_import_shared_nous_state'),
    'auth_u_refresh_access_token': ('hermes_cli.auth', '_refresh_access_token'),
    'auth_fetch_nous_models': ('hermes_cli.auth', 'fetch_nous_models'),
    'auth_resolve_nous_access_token': ('hermes_cli.auth', 'resolve_nous_access_token'),
    'auth_refresh_nous_oauth_pure': ('hermes_cli.auth', 'refresh_nous_oauth_pure'),
    'auth_refresh_nous_oauth_from_state': ('hermes_cli.auth', 'refresh_nous_oauth_from_state'),
    'auth_persist_nous_credentials': ('hermes_cli.auth', 'persist_nous_credentials'),
    'auth_u_sync_nous_pool_from_auth_store': ('hermes_cli.auth', '_sync_nous_pool_from_auth_store'),
    'auth_resolve_nous_runtime_credentials': ('hermes_cli.auth', 'resolve_nous_runtime_credentials'),
    'auth_u_snapshot_nous_pool_status': ('hermes_cli.auth', '_snapshot_nous_pool_status'),
    'auth_get_nous_auth_status': ('hermes_cli.auth', 'get_nous_auth_status'),
    'auth_u_compute_nous_auth_status': ('hermes_cli.auth', '_compute_nous_auth_status'),
    'auth_get_nous_session_validity': ('hermes_cli.auth', 'get_nous_session_validity'),
    'auth_get_codex_auth_status': ('hermes_cli.auth', 'get_codex_auth_status'),
    'auth_get_xai_oauth_auth_status': ('hermes_cli.auth', 'get_xai_oauth_auth_status'),
    'auth_get_api_key_provider_status': ('hermes_cli.auth', 'get_api_key_provider_status'),
    'auth_get_external_process_provider_status': ('hermes_cli.auth', 'get_external_process_provider_status'),
    'auth_u_get_azure_foundry_auth_status': ('hermes_cli.auth', '_get_azure_foundry_auth_status'),
    'auth_resolve_api_key_provider_credentials': ('hermes_cli.auth', 'resolve_api_key_provider_credentials'),
    'auth_resolve_external_process_provider_credentials': ('hermes_cli.auth', 'resolve_external_process_provider_credentials'),
    'auth_u_update_config_for_provider': ('hermes_cli.auth', '_update_config_for_provider'),
    'auth_u_confirm_expensive_model_selection': ('hermes_cli.auth', '_confirm_expensive_model_selection'),
    'auth_u_prompt_model_selection': ('hermes_cli.auth', '_prompt_model_selection'),
    'auth_u_login_openai_codex': ('hermes_cli.auth', '_login_openai_codex'),
    'auth_u_login_xai_oauth': ('hermes_cli.auth', '_login_xai_oauth'),
    'auth_u_xai_oauth_request_device_code': ('hermes_cli.auth', '_xai_oauth_request_device_code'),
    'auth_u_xai_oauth_poll_device_token': ('hermes_cli.auth', '_xai_oauth_poll_device_token'),
    'auth_u_xai_oauth_device_code_login': ('hermes_cli.auth', '_xai_oauth_device_code_login'),
    'auth_u_codex_device_code_login': ('hermes_cli.auth', '_codex_device_code_login'),
    'auth_u_minimax_pkce_pair': ('hermes_cli.auth', '_minimax_pkce_pair'),
    'auth_u_minimax_request_user_code': ('hermes_cli.auth', '_minimax_request_user_code'),
    'auth_u_minimax_poll_token': ('hermes_cli.auth', '_minimax_poll_token'),
    'auth_u_minimax_save_auth_state': ('hermes_cli.auth', '_minimax_save_auth_state'),
    'auth_u_minimax_oauth_login': ('hermes_cli.auth', '_minimax_oauth_login'),
    'auth_u_refresh_minimax_oauth_state': ('hermes_cli.auth', '_refresh_minimax_oauth_state'),
    'auth_u_minimax_oauth_quarantine_on_terminal_refresh': ('hermes_cli.auth', '_minimax_oauth_quarantine_on_terminal_refresh'),
    'auth_build_minimax_oauth_token_provider': ('hermes_cli.auth', 'build_minimax_oauth_token_provider'),
    'auth_resolve_minimax_oauth_runtime_credentials': ('hermes_cli.auth', 'resolve_minimax_oauth_runtime_credentials'),
    'auth_get_minimax_oauth_auth_status': ('hermes_cli.auth', 'get_minimax_oauth_auth_status'),
    'auth_u_login_minimax_oauth': ('hermes_cli.auth', '_login_minimax_oauth'),
    'auth_u_login_nous': ('hermes_cli.auth', '_login_nous'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_auth_wrappers.py <cases.json>\n"); return 2
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
