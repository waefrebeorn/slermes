#!/usr/bin/env python3
"""Annotate documented no-op seams with the standard audit comment."""
import re, sys

MARK = "/* Python default is also a no-op */"

SEAMS = {
 "src/agent/agent_runtime_helpers.c": ["iter_httpx_pool_objects","connection_candidates","cleanup_dead_connections","force_close_tcp_sockets"],
 "src/agent/i18n.c": ["i18n_set_locales_dir","reset_language_cache"],
 "src/agent/system_prompt.c": ["invalidate_system_prompt"],
 "src/agent/prompt_builder.c": ["clear_backend_probe_cache","skill_should_show"],
 "src/agent/auxiliary_client_helpers2.c": ["aux2_close"],
 "src/agent/copilot_acp_client_helpers2.c": ["cac2_close"],
 "src/agent/conversation_compression_helpers.c": ["cc_ctx_refresh_guards"],
 "src/agent/otlp_exporter.c": ["otlp_exporter_is_available"],
 "src/agent/outbound_webhooks.c": ["ow_redirect_request"],
 "src/agent/stream_diag.c": ["stream_diag_init"],
 "src/gateway/platforms/base.c": ["pb_supports_draft_streaming","pb_edit_message","pb_delete_message","pb_send_typing","pb_stop_typing"],
 "src/gateway/platforms/base_adapter.c": ["gw_base_platform_adapter_default_connect","gw_base_platform_adapter_default_disconnect","gw_base_platform_adapter_default_delete_message","gw_base_platform_adapter_default_send_typing","gw_base_platform_adapter_default_stop_typing","gw_base_platform_adapter_default_get_chat_info","gw_base_platform_adapter_acquire_lock","gw_base_platform_adapter_release_lock"],
 "src/gateway/platforms/qqbot/qqbot_adapter_ports.c": ["qqa_enforces_own_access_policy"],
 "src/gateway/platforms/weixin.c": ["weixin_check_requirements"],
 "src/gateway/weixin_ports.c": ["wx2_enforces_own_access_policy"],
 "src/gateway/yuanbao_ports.c": ["yb2_ws","yb2_enforces_own_access_policy"],
 "src/gateway/relay/relay_adapter_ports.c": ["rla_authorization_is_upstream"],
 "src/gateway/relay/relay_runtime.c": ["relay_noop_available","relay_noop_retain_managed_execution","relay_noop_release_managed_execution","relay_noop_managed_execution_enabled"],
 "src/gateway/run_pure2.c": ["gw_ensure_windows_gateway_venv_imports"],
 "src/gateway/session_state.c": ["dflt_null"],
 "src/hermes_cli/managed_uv_wrappers.c": ["muv_rebuild_venv","_reload_hermes_constants"],
 "src/tools/transcribe.c": ["registry_init_transcribe_impl"],
 "src/tools/registry_cache.c": ["check_fn_cache_scope"],
 "src/tools/browser_tool_helpers.c": ["browser__is_legacy_provider_registry_overridden"],
 "src/tools/vision_wrappers.c": ["vi_vision_concurrency_slot"],
 "src/tools/clarify_tool.c": ["clarify_check_requirements"],
 "src/tools/todo_tool.c": ["todo_tool_check_requirements"],
 "src/gateway/platforms/msgraph_webhook/msgraph_webhook_helpers.c": ["msgraph_webhook_check_requirements"],
 "src/hermes_cli/webhook_helpers.c": ["webhook_check_requirements"],
 "src/gateway/bluebubbles_wrappers.c": ["bb_check_bluebubbles_requirements"],
 "src/gateway/platforms/qqbot/qqbot_wrappers.c": ["qqbot_check_qq_requirements"],
 "src/agent/bitwarden_ports.c": [],
}

def find_fn(src, name):
    m = re.search(r'^[A-Za-z_].*\b%s\s*\(' % re.escape(name), src, re.M)
    if not m: return None
    i = src.find('{', m.end())
    return i

for path, names in SEAMS.items():
    try: src = open(path).read()
    except OSError: continue
    changed = False
    for n in names:
        i = find_fn(src, n)
        if i is None:
            print(f"MISS {path}:{n}"); continue
        # insert marker right after opening brace line
        endline = src.find('\n', i)
        seg = src[i:endline]
        if 'Python default is also a no-op' in src[max(0,i-400):endline+200]:
            continue
        src = src[:i+1] + " " + MARK + " " + src[i+1:]
        changed = True
    if changed:
        open(path,'w').write(src)
        print("annotated", path)
