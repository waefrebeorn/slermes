from pathlib import Path
import load_dev_module as _ldm
import os
#!/usr/bin/env python3
"""
sta_oracle_kanban_helpers.py — Python oracle for the PURE kanban helpers port
(src/hermes_cli/kanban_format.c). Re-imports the REAL hermes_cli/kanban.py and
evaluates every fixture line through the canonical Python functions, reading the
SAME env vars the C harness sets, so the runner can diff C vs PY output.

Fields are '|'-separated (matching the C harness) so titles/bodies may contain
spaces. Emits the SAME flat JSON shapes the C harness emits (task_dict and
run_state as flat top-level fields), so the runner can diff them line-by-line.

Run by tests/oracle/runners/run_oracle.sh as the "kanban_helpers" group.
"""
import sys
import json

_HERE = os.path.dirname(os.path.abspath(__file__))
for _c in (os.path.join(_HERE, "..", ".."), os.environ.get("HERMES_AGENT_DIR", "")):
    if _c and os.path.isdir(os.path.join(_c, "hermes_cli")):
        sys.path.insert(0, os.path.abspath(_c))
        break

import hermes_cli.kanban as K   # noqa: E402
_ldm.rebind_used(K, 'K', ["hermes_cli.__init__", "hermes_cli._early_recovery", "hermes_cli._early_recovery_zip", "hermes_cli._install_repair", "hermes_cli._launchers", "hermes_cli._old_updater", "hermes_cli._parser", "hermes_cli._scan_venv_blockers", "hermes_cli._secrets_common", "hermes_cli._startup_fast", "hermes_cli._subprocess_compat", "hermes_cli._update_takeover", "hermes_cli.active_sessions", "hermes_cli.agent_plugins", "hermes_cli.anon_auth", "hermes_cli.anon_challenge", "hermes_cli.anon_sign_in", "hermes_cli.anon_sign_in_cli", "hermes_cli.approval_mode", "hermes_cli.approval_transport", "hermes_cli.approvals_suggest", "hermes_cli.approvals_test", "hermes_cli.archive_safe", "hermes_cli.auth", "hermes_cli.auth_codex", "hermes_cli.auth_codex_browser", "hermes_cli.auth_commands", "hermes_cli.auth_constants", "hermes_cli.auth_device_flow", "hermes_cli.auth_error_copy", "hermes_cli.auth_external_process", "hermes_cli.auth_minimax", "hermes_cli.auth_model_picker", "hermes_cli.auth_nous", "hermes_cli.auth_oauth_grants", "hermes_cli.auth_oauth_pkce_plugin", "hermes_cli.auth_openrouter", "hermes_cli.auth_plugin_providers", "hermes_cli.auth_qwen", "hermes_cli.auth_xai", "hermes_cli.auth_zai_kimi", "hermes_cli.azure_detect", "hermes_cli.backend_retirement", "hermes_cli.backup", "hermes_cli.backup_cron_prompts", "hermes_cli.backup_restore", "hermes_cli.backup_sqlite", "hermes_cli.bang_shell", "hermes_cli.banner", "hermes_cli.blueprint_cmd", "hermes_cli.boot_bootstrap", "hermes_cli.browser_connect", "hermes_cli.browser_credential", "hermes_cli.browser_runtime", "hermes_cli.build_info", "hermes_cli.bundled_app", "hermes_cli.bundles", "hermes_cli.callbacks", "hermes_cli.chat_catalog", "hermes_cli.checkpoints", "hermes_cli.cli_agent_setup_mixin", "hermes_cli.cli_auto_maintenance", "hermes_cli.cli_billing_mixin", "hermes_cli.cli_browser_gateway", "hermes_cli.cli_chat_error_copy", "hermes_cli.cli_chat_turn_mixin", "hermes_cli.cli_commands_mixin", "hermes_cli.cli_commands_session_tools", "hermes_cli.cli_commands_worktree", "hermes_cli.cli_config_load", "hermes_cli.cli_cron_lines", "hermes_cli.cli_enter_burst", "hermes_cli.cli_footer_split", "hermes_cli.cli_info_lines", "hermes_cli.cli_info_mixin", "hermes_cli.cli_init_mixin", "hermes_cli.cli_loops_mixin", "hermes_cli.cli_modal_mixin", "hermes_cli.cli_model_switch_mixin", "hermes_cli.cli_modern_layout", "hermes_cli.cli_output", "hermes_cli.cli_panel", "hermes_cli.cli_process_dock", "hermes_cli.cli_process_notifications", "hermes_cli.cli_render", "hermes_cli.cli_session_dock", "hermes_cli.cli_session_mixin", "hermes_cli.cli_session_switch", "hermes_cli.cli_shutdown", "hermes_cli.cli_single_query", "hermes_cli.cli_snapshot", "hermes_cli.cli_status_bar_mixin", "hermes_cli.cli_stream_mixin", "hermes_cli.cli_subagent_monitor", "hermes_cli.cli_terminal_input", "hermes_cli.cli_terminal_mixin", "hermes_cli.cli_tui_mixin", "hermes_cli.cli_tui_runtime_mixin", "hermes_cli.cli_tui_wrap", "hermes_cli.cli_unknown_command", "hermes_cli.cli_voice_mixin", "hermes_cli.clipboard", "hermes_cli.codex_models", "hermes_cli.codex_runtime_plugin_migration", "hermes_cli.codex_runtime_switch", "hermes_cli.colors", "hermes_cli.commands", "hermes_cli.commands_completion", "hermes_cli.commands_platforms", "hermes_cli.completion", "hermes_cli.config", "hermes_cli.config_backend", "hermes_cli.config_backups", "hermes_cli.config_check_diagnostics", "hermes_cli.config_defaults", "hermes_cli.config_defaults_auxiliary", "hermes_cli.config_defaults_env", "hermes_cli.config_effective", "hermes_cli.config_env_routing", "hermes_cli.config_home", "hermes_cli.config_issues", "hermes_cli.config_key_notices", "hermes_cli.config_language", "hermes_cli.config_migrations", "hermes_cli.config_migrations_browser", "hermes_cli.config_provider_env", "hermes_cli.config_providers", "hermes_cli.config_read_errors", "hermes_cli.config_runtime_keys", "hermes_cli.config_section_guard", "hermes_cli.config_show", "hermes_cli.config_value_checks", "hermes_cli.config_version_stamp", "hermes_cli.config_worker_save", "hermes_cli.connectors_cmd", "hermes_cli.console_engine", "hermes_cli.container_boot", "hermes_cli.context_switch_guard", "hermes_cli.credential_lifecycle", "hermes_cli.cron", "hermes_cli.curator", "hermes_cli.curses_ui", "hermes_cli.dashboard_procs", "hermes_cli.dashboard_register", "hermes_cli.data_cleanup", "hermes_cli.data_cleanup_holders", "hermes_cli.debug", "hermes_cli.debug_redaction", "hermes_cli.default_soul", "hermes_cli.desktop_build_lock", "hermes_cli.desktop_build_reuse", "hermes_cli.desktop_console", "hermes_cli.desktop_plugin_halves", "hermes_cli.desktop_plugin_install", "hermes_cli.desktop_release_holders", "hermes_cli.desktop_update_verify", "hermes_cli.diagnostics_upload", "hermes_cli.dingtalk_auth", "hermes_cli.doctor", "hermes_cli.doctor_config", "hermes_cli.doctor_connectivity", "hermes_cli.doctor_live", "hermes_cli.doctor_platform", "hermes_cli.doctor_report", "hermes_cli.doctor_state", "hermes_cli.doctor_tools", "hermes_cli.dump", "hermes_cli.env_loader", "hermes_cli.fallback_cmd", "hermes_cli.fallback_config", "hermes_cli.focus_view", "hermes_cli.foreign_sessions", "hermes_cli.foreign_sessions_browser", "hermes_cli.free_tier_bootstrap", "hermes_cli.free_tier_offer", "hermes_cli.fs_utils", "hermes_cli.gateway", "hermes_cli.gateway_agent", "hermes_cli.gateway_automation", "hermes_cli.gateway_chat", "hermes_cli.gateway_chat_commands", "hermes_cli.gateway_chat_listing", "hermes_cli.gateway_chat_startup", "hermes_cli.gateway_chat_view", "hermes_cli.gateway_cli_commands", "hermes_cli.gateway_client", "hermes_cli.gateway_command_errors", "hermes_cli.gateway_connection", "hermes_cli.gateway_enroll", "hermes_cli.gateway_host_record", "hermes_cli.gateway_launchd", "hermes_cli.gateway_launchd_runtime", "hermes_cli.gateway_lifecycle_guards", "hermes_cli.gateway_migrate", "hermes_cli.gateway_migrate_guards", "hermes_cli.gateway_multiplex_mode", "hermes_cli.gateway_multiplex_s6", "hermes_cli.gateway_multiplex_served", "hermes_cli.gateway_mutations", "hermes_cli.gateway_plugin_commands", "hermes_cli.gateway_profile_lifecycle", "hermes_cli.gateway_queue", "hermes_cli.gateway_replay_gap", "hermes_cli.gateway_rollback", "hermes_cli.gateway_runtime", "hermes_cli.gateway_runtime_cli", "hermes_cli.gateway_runtime_discovery", "hermes_cli.gateway_runtime_multiplex", "hermes_cli.gateway_runtime_replace", "hermes_cli.gateway_runtime_service", "hermes_cli.gateway_runtime_service_identity", "hermes_cli.gateway_runtime_start", "hermes_cli.gateway_service_owner", "hermes_cli.gateway_session_store", "hermes_cli.gateway_setting_commands", "hermes_cli.gateway_setup_service", "hermes_cli.gateway_setup_wizard", "hermes_cli.gateway_side", "hermes_cli.gateway_status_notes", "hermes_cli.gateway_supervised_restart", "hermes_cli.gateway_tools", "hermes_cli.gateway_tools_notify", "hermes_cli.gateway_windows", "hermes_cli.gateway_windows_legacy", "hermes_cli.git_commit_graph", "hermes_cli.git_credentials", "hermes_cli.git_pack_tidy", "hermes_cli.github_api", "hermes_cli.gitlock", "hermes_cli.goal_command", "hermes_cli.goals", "hermes_cli.guest_handoff", "hermes_cli.gui_uninstall", "hermes_cli.heartbeat", "hermes_cli.home_data_layout", "hermes_cli.hooks", "hermes_cli.image_provenance", "hermes_cli.init_command", "hermes_cli.input_sanitize", "hermes_cli.install_identity", "hermes_cli.install_state", "hermes_cli.inventory", "hermes_cli.journey", "hermes_cli.kanban_boards", "hermes_cli.kanban_db", "hermes_cli.kanban_db_boards", "hermes_cli.kanban_db_connect", "hermes_cli.kanban_db_context", "hermes_cli.kanban_db_dispatch", "hermes_cli.kanban_db_graph", "hermes_cli.kanban_db_notify", "hermes_cli.kanban_db_workspace", "hermes_cli.kanban_decompose", "hermes_cli.kanban_diagnostics", "hermes_cli.kanban_ops", "hermes_cli.kanban_output", "hermes_cli.kanban_owner_recovery", "hermes_cli.kanban_parser", "hermes_cli.kanban_pr_acceptance", "hermes_cli.kanban_pr_acceptance_store", "hermes_cli.kanban_resume_guard", "hermes_cli.kanban_specify", "hermes_cli.kanban_swarm", "hermes_cli.kanban_transfer", "hermes_cli.kanban_worker_client", "hermes_cli.kanban_worker_log", "hermes_cli.kanban_workflow", "hermes_cli.left_core_migration", "hermes_cli.lifecycle", "hermes_cli.linux_desktop_entry", "hermes_cli.logs", "hermes_cli.loops", "hermes_cli.macos_signing", "hermes_cli.macos_tcc_anchor", "hermes_cli.main", "hermes_cli.main_agent_cmds", "hermes_cli.main_container", "hermes_cli.main_dashboard", "hermes_cli.main_dep_hints", "hermes_cli.main_desktop", "hermes_cli.main_desktop_apparmor", "hermes_cli.main_desktop_tcc", "hermes_cli.main_install_repair", "hermes_cli.main_platform_setup", "hermes_cli.main_profile_recovery", "hermes_cli.main_provider_setup", "hermes_cli.main_query_file", "hermes_cli.main_resume_args", "hermes_cli.main_tui_launch", "hermes_cli.main_web_build", "hermes_cli.managed_scope", "hermes_cli.managed_uv", "hermes_cli.mcp_catalog", "hermes_cli.mcp_config", "hermes_cli.mcp_picker", "hermes_cli.mcp_security", "hermes_cli.mcp_startup", "hermes_cli.mem_trim", "hermes_cli.memory_oauth", "hermes_cli.memory_provider_migration", "hermes_cli.memory_setup", "hermes_cli.middleware", "hermes_cli.migrate", "hermes_cli.moa_cmd", "hermes_cli.moa_config", "hermes_cli.model_catalog", "hermes_cli.model_cost_guard", "hermes_cli.model_data_policy_guard", "hermes_cli.model_normalize", "hermes_cli.model_picker_usage", "hermes_cli.model_search", "hermes_cli.model_selection_guards", "hermes_cli.model_setup", "hermes_cli.model_setup_flows", "hermes_cli.model_setup_flows_azure", "hermes_cli.model_setup_flows_bedrock", "hermes_cli.model_setup_flows_common", "hermes_cli.model_setup_flows_custom", "hermes_cli.model_setup_flows_local", "hermes_cli.model_switch", "hermes_cli.model_switch_providers", "hermes_cli.models", "hermes_cli.models_cache_lock", "hermes_cli.models_catalog_static", "hermes_cli.models_detect", "hermes_cli.models_local", "hermes_cli.models_pricing", "hermes_cli.models_profile_cache", "hermes_cli.models_reasoning_caps", "hermes_cli.models_validate", "hermes_cli.nous_account", "hermes_cli.nous_auth_keepalive", "hermes_cli.nous_billing", "hermes_cli.nous_subscription", "hermes_cli.npm_engine", "hermes_cli.old_updater_deps", "hermes_cli.old_updater_main", "hermes_cli.onboarding_due", "hermes_cli.onboarding_run", "hermes_cli.onepassword_secrets_cli", "hermes_cli.oneshot", "hermes_cli.pairing", "hermes_cli.partial_compress", "hermes_cli.personality", "hermes_cli.pets", "hermes_cli.platform_actions", "hermes_cli.platforms", "hermes_cli.plugin_capabilities", "hermes_cli.plugin_catalog", "hermes_cli.plugin_catalog_presence", "hermes_cli.plugin_choices", "hermes_cli.plugin_command_dispatch", "hermes_cli.plugin_compat", "hermes_cli.plugin_dev", "hermes_cli.plugin_events", "hermes_cli.plugin_host", "hermes_cli.plugin_host_child", "hermes_cli.plugin_host_http_auth", "hermes_cli.plugin_host_profiles", "hermes_cli.plugin_host_wire", "hermes_cli.plugin_install_phase", "hermes_cli.plugin_isolation", "hermes_cli.plugin_isolation_audit", "hermes_cli.plugin_messages", "hermes_cli.plugin_oauth", "hermes_cli.plugin_packs", "hermes_cli.plugin_provider_requests", "hermes_cli.plugin_session", "hermes_cli.plugin_validate", "hermes_cli.plugin_validate_core_override", "hermes_cli.plugin_validate_desktop", "hermes_cli.plugin_validate_locales", "hermes_cli.plugin_validate_private_imports", "hermes_cli.plugins", "hermes_cli.plugins_activation", "hermes_cli.plugins_activation_live", "hermes_cli.plugins_admission", "hermes_cli.plugins_aux_tasks", "hermes_cli.plugins_cadence", "hermes_cli.plugins_capability_ctx", "hermes_cli.plugins_cmd", "hermes_cli.plugins_cmd_capabilities", "hermes_cli.plugins_cmd_catalog", "hermes_cli.plugins_cmd_git", "hermes_cli.plugins_cmd_inspect", "hermes_cli.plugins_cmd_install", "hermes_cli.plugins_cmd_listing", "hermes_cli.plugins_cmd_remove", "hermes_cli.plugins_cmd_toggle", "hermes_cli.plugins_cmd_update", "hermes_cli.plugins_cmd_update_all", "hermes_cli.plugins_content", "hermes_cli.plugins_discovery", "hermes_cli.plugins_dispatch", "hermes_cli.plugins_injection", "hermes_cli.plugins_ledger", "hermes_cli.plugins_loader", "hermes_cli.plugins_manifest", "hermes_cli.plugins_provenance", "hermes_cli.plugins_registrars", "hermes_cli.plugins_settings", "hermes_cli.plugins_state", "hermes_cli.plugins_transaction", "hermes_cli.plugins_updates", "hermes_cli.portal_cli", "hermes_cli.post_update", "hermes_cli.process_identity", "hermes_cli.profile_channels", "hermes_cli.profile_cmd", "hermes_cli.profile_describer", "hermes_cli.profile_distribution", "hermes_cli.profile_identity", "hermes_cli.profile_memory_config", "hermes_cli.profile_model_seed", "hermes_cli.profiles", "hermes_cli.profiles_cache", "hermes_cli.profiles_config", "hermes_cli.profiles_honcho", "hermes_cli.profiles_initialization", "hermes_cli.profiles_remove", "hermes_cli.profiles_service_cleanup", "hermes_cli.projects_cmd", "hermes_cli.projects_db", "hermes_cli.prompt_size", "hermes_cli.prompt_stash", "hermes_cli.provider_catalog", "hermes_cli.providers", "hermes_cli.proxy_cli", "hermes_cli.prune_advisor", "hermes_cli.psutil_android", "hermes_cli.pt_input_extras", "hermes_cli.pty_bridge", "hermes_cli.pty_session", "hermes_cli.quiet_single_query", "hermes_cli.relaunch", "hermes_cli.relay_plugin_cutover", "hermes_cli.relay_plugin_migrate", "hermes_cli.release_channels", "hermes_cli.resource_limits", "hermes_cli.route_identity", "hermes_cli.runtime_paths", "hermes_cli.runtime_provider", "hermes_cli.runtime_provider_backends", "hermes_cli.runtime_provider_custom", "hermes_cli.runtime_readiness", "hermes_cli.runtime_state", "hermes_cli.sandbox_image_switch", "hermes_cli.secret_prompt", "hermes_cli.secrets_cli", "hermes_cli.security_advisories", "hermes_cli.security_audit", "hermes_cli.security_audit_startup", "hermes_cli.send_cmd", "hermes_cli.service_manager", "hermes_cli.session_export", "hermes_cli.session_export_html", "hermes_cli.session_export_md", "hermes_cli.session_filters", "hermes_cli.session_listing", "hermes_cli.session_lost_and_found", "hermes_cli.session_recap", "hermes_cli.session_recovery", "hermes_cli.session_reset_retirement", "hermes_cli.session_schema_history", "hermes_cli.sessions_adopt", "hermes_cli.sessions_cmd", "hermes_cli.sessions_cmd_browse", "hermes_cli.sessions_cmd_journal_mode", "hermes_cli.sessions_cmd_repair_profiles", "hermes_cli.sessions_discard", "hermes_cli.sessions_owner", "hermes_cli.sessions_repair_profiles", "hermes_cli.setup", "hermes_cli.setup_agent_import", "hermes_cli.setup_hidden_env", "hermes_cli.setup_migration", "hermes_cli.setup_platforms", "hermes_cli.setup_quick", "hermes_cli.setup_summary", "hermes_cli.setup_terminal", "hermes_cli.setup_tts", "hermes_cli.shared_profile_warning", "hermes_cli.shared_session_attach", "hermes_cli.sizefmt", "hermes_cli.skills_config", "hermes_cli.skills_hub", "hermes_cli.skin_cmd", "hermes_cli.skin_engine", "hermes_cli.slack_cli", "hermes_cli.slash_exec", "hermes_cli.source_build", "hermes_cli.source_check", "hermes_cli.source_completion", "hermes_cli.source_releases", "hermes_cli.source_stamp", "hermes_cli.sqlite_runtime", "hermes_cli.sqlite_safe_read", "hermes_cli.sqlite_util", "hermes_cli.ssh_workspace_fs", "hermes_cli.staged_uploads", "hermes_cli.stale_modules", "hermes_cli.status", "hermes_cli.status_auth", "hermes_cli.status_bar_git", "hermes_cli.status_report", "hermes_cli.stderr_timestamp", "hermes_cli.stdio", "hermes_cli.steward", "hermes_cli.stream_json", "hermes_cli.suggestions_cmd", "hermes_cli.telegram_managed_bot", "hermes_cli.terminal_breadcrumbs", "hermes_cli.terminal_notify", "hermes_cli.timefmt", "hermes_cli.timeouts", "hermes_cli.tips", "hermes_cli.tool_availability_notices", "hermes_cli.tools_config", "hermes_cli.tools_config_cua", "hermes_cli.tools_config_mcp", "hermes_cli.tools_config_post_setup", "hermes_cli.tools_config_providers", "hermes_cli.toolset_scope", "hermes_cli.toolset_validation", "hermes_cli.tui_worktree_resume", "hermes_cli.turn_exit", "hermes_cli.ui_preferences", "hermes_cli.uninstall", "hermes_cli.update_abort_recovery", "hermes_cli.update_channel", "hermes_cli.update_cmd", "hermes_cli.update_cmd_check", "hermes_cli.update_cmd_commit", "hermes_cli.update_cmd_common", "hermes_cli.update_cmd_config", "hermes_cli.update_cmd_drain_report", "hermes_cli.update_cmd_fleet", "hermes_cli.update_cmd_fleet_checkout", "hermes_cli.update_cmd_fleet_gatewayless", "hermes_cli.update_cmd_fleet_unit_budget", "hermes_cli.update_cmd_fleet_verify", "hermes_cli.update_cmd_git", "hermes_cli.update_cmd_maint", "hermes_cli.update_cmd_pause_gate", "hermes_cli.update_cmd_posix_pause", "hermes_cli.update_cmd_stale_survivors", "hermes_cli.update_cmd_stash", "hermes_cli.update_cmd_validation", "hermes_cli.update_cmd_windows", "hermes_cli.update_cmd_zip", "hermes_cli.update_completion", "hermes_cli.update_contract", "hermes_cli.update_custody", "hermes_cli.update_downgrade_guard", "hermes_cli.update_finish", "hermes_cli.update_fleet_scope", "hermes_cli.update_handoff", "hermes_cli.update_host_obligation", "hermes_cli.update_inventory", "hermes_cli.update_lock", "hermes_cli.update_owning_install", "hermes_cli.update_pause_record", "hermes_cli.update_proxy_env", "hermes_cli.update_receipt", "hermes_cli.update_restart_recovery", "hermes_cli.update_serve_obligations", "hermes_cli.update_serve_resume", "hermes_cli.update_stage", "hermes_cli.update_windows_acl", "hermes_cli.url_utils", "hermes_cli.urllib_security", "hermes_cli.vault", "hermes_cli.venv_sync", "hermes_cli.vercel_auth", "hermes_cli.verify_cmd", "hermes_cli.version_info", "hermes_cli.voice", "hermes_cli.web_asset_mime_types", "hermes_cli.web_build_limits", "hermes_cli.web_deps", "hermes_cli.web_git", "hermes_cli.web_host_terminal", "hermes_cli.web_host_terminal_sessions", "hermes_cli.web_models", "hermes_cli.web_read_coalescing", "hermes_cli.web_server", "hermes_cli.web_server_app", "hermes_cli.web_server_boundary", "hermes_cli.web_server_chat", "hermes_cli.web_server_config", "hermes_cli.web_server_cron", "hermes_cli.web_server_dashboard", "hermes_cli.web_server_file_response", "hermes_cli.web_server_file_tickets", "hermes_cli.web_server_files", "hermes_cli.web_server_gateway", "hermes_cli.web_server_idle_exit", "hermes_cli.web_server_idle_proof", "hermes_cli.web_server_lifecycle", "hermes_cli.web_server_mcp", "hermes_cli.web_server_memory", "hermes_cli.web_server_messaging", "hermes_cli.web_server_oauth", "hermes_cli.web_server_owner_exit", "hermes_cli.web_server_plugin_profiles", "hermes_cli.web_server_profiles", "hermes_cli.web_server_remote_media", "hermes_cli.web_server_session_token", "hermes_cli.web_server_sessions", "hermes_cli.web_server_skew_exit", "hermes_cli.web_server_static_assets", "hermes_cli.web_server_surface", "hermes_cli.web_session_sockets", "hermes_cli.web_upload_limit", "hermes_cli.web_whatsapp_session", "hermes_cli.webapp", "hermes_cli.webapp_access", "hermes_cli.webapp_launcher", "hermes_cli.webapp_page_policy", "hermes_cli.webapp_previews", "hermes_cli.webapp_sign_in", "hermes_cli.webhook", "hermes_cli.welcome_trial", "hermes_cli.win_pty_bridge", "hermes_cli.windows_appinstaller_update", "hermes_cli.windows_ssh_runtime", "hermes_cli.windows_store_update", "hermes_cli.worktree_archive", "hermes_cli.worktree_cmd", "hermes_cli.worktree_exit", "hermes_cli.worktree_gc", "hermes_cli.worktree_ops", "hermes_cli.worktree_registry", "hermes_cli.worktree_retention", "hermes_cli.worktree_sweep", "hermes_cli.write_approval_commands", "hermes_cli.xai_retirement"], Path(__file__))


def split_pipe(rest, n):
    out = ["" for _ in range(n)]
    if not rest:
        return out
    parts = rest.split("|")
    for i in range(min(len(parts), n)):
        out[i] = parts[i]
    return out


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: %s <cases.txt>\n" % sys.argv[0])
        return 2

    with open(sys.argv[1], "r") as f:
        for raw in f:
            line = raw.rstrip("\n")
            if not line.strip():
                continue
            op, _, rest = line.partition(" ")
            rest = rest.strip()
            a = split_pipe(rest, 32)

            if op == "fmt_ts":
                ts = int(rest) if rest else 0
                out = K._fmt_ts(ts) if ts else ""
                print(json.dumps({"op": "fmt_ts", "ts": ts, "out": out},
                                 separators=(",", ":"), ensure_ascii=False))
            elif op == "fmt_task_line":
                parts = split_pipe(rest, 5)

                class T:
                    pass
                t = T()
                t.id = parts[0]; t.status = parts[1]; t.assignee = parts[2]
                t.tenant = parts[3]; t.title = parts[4]
                out = K._fmt_task_line(t)
                print(json.dumps({"op": "fmt_task_line", "out": out},
                                 separators=(",", ":"), ensure_ascii=False))
            elif op == "task_dict":
                parts = split_pipe(rest, 19)

                class T:
                    pass
                t = T()
                t.id = parts[0]; t.title = parts[1]; t.body = parts[2]
                t.assignee = parts[3]; t.status = parts[4]
                t.priority = parts[5] if parts[5] else "0"
                t.tenant = parts[6]; t.workspace_kind = ""
                t.workspace_path = parts[7]; t.branch_name = parts[8]
                t.project_id = parts[9]; t.created_by = parts[10]
                t.created_at = int(parts[11]) if parts[11] else 0
                t.started_at = int(parts[12]) if parts[12] else 0
                t.completed_at = int(parts[13]) if parts[13] else 0
                t.result = parts[14]
                t.skills = (json.loads(parts[15]) if parts[15] and parts[15] != "[]" else [])
                t.max_retries = int(parts[16]) if parts[16] else 0
                t.session_id = parts[17]; t.workflow_template_id = parts[18]
                t.current_step_key = parts[18]
                t.model_override = ""
                t.provider_override = ""
                d = K._task_to_dict(t)
                print(json.dumps({
                    "op": "task_dict",
                    "id": d.get("id", ""),
                    "title": d.get("title", ""),
                    "body": d.get("body", ""),
                    "assignee": d.get("assignee", ""),
                    "status": d.get("status", ""),
                    "priority": d.get("priority", ""),
                    "tenant": d.get("tenant", ""),
                    "workspace_path": d.get("workspace_path", ""),
                    "branch_name": d.get("branch_name", ""),
                    "project_id": d.get("project_id", ""),
                    "created_by": d.get("created_by", ""),
                    "created_at": d.get("created_at", 0),
                    "started_at": d.get("started_at", 0),
                    "completed_at": d.get("completed_at", 0),
                    "result": d.get("result", ""),
                    "max_retries": d.get("max_retries", 0),
                    "session_id": d.get("session_id", ""),
                    "workflow_template_id": d.get("workflow_template_id", ""),
                    "current_step_key": d.get("current_step_key", ""),
                    "skills": d.get("skills", []),
                }, separators=(",", ":"), ensure_ascii=False))
            elif op == "run_state":
                parts = split_pipe(rest, 2)
                st = parts[0] if parts[0] else None
                sn = parts[1] if parts[1] else None

                class A:
                    pass
                args = A()
                args.state_type = st
                args.state_name = sn
                try:
                    r = K._run_state_kwargs(args)
                except Exception:
                    r = "MISMATCH"
                if r == "MISMATCH" or r is None:
                    print(json.dumps({"op": "run_state", "state": None},
                                     separators=(",", ":"), ensure_ascii=False))
                elif r == {}:
                    print(json.dumps({"op": "run_state", "state": {}},
                                     separators=(",", ":"), ensure_ascii=False))
                else:
                    print(json.dumps({"op": "run_state",
                                      "state_name": r.get("state_name", ""),
                                      "state_type": r.get("state_type", "")},
                                     separators=(",", ":"), ensure_ascii=False))
            elif op == "ws_flag":
                try:
                    kind, path = K._parse_workspace_flag(rest if rest else "")
                    print(json.dumps({"op": "ws_flag", "ok": True, "kind": kind,
                                     "path": path or "", "err": ""},
                                     separators=(",", ":"), ensure_ascii=False))
                except Exception as e:
                    print(json.dumps({"op": "ws_flag", "ok": False, "kind": "",
                                     "path": "", "err": str(e)},
                                     separators=(",", ":"), ensure_ascii=False))
            elif op == "branch_flag":
                try:
                    b = K._parse_branch_flag(rest if rest else None)
                    print(json.dumps({"op": "branch_flag", "ok": bool(b), "branch": b or "",
                                     "err": ""}, separators=(",", ":"), ensure_ascii=False))
                except Exception as e:
                    print(json.dumps({"op": "branch_flag", "ok": False, "branch": "",
                                     "err": str(e)}, separators=(",", ":"), ensure_ascii=False))
            elif op == "profile_author":
                print(json.dumps({"op": "profile_author", "author": K._profile_author()},
                                 separators=(",", ":"), ensure_ascii=False))
            elif op == "duration":
                try:
                    r = K._parse_duration(rest if rest else "")
                    print(json.dumps({"op": "duration", "seconds": -2 if r is None else r},
                                     separators=(",", ":"), ensure_ascii=False))
                except Exception:
                    print(json.dumps({"op": "duration", "seconds": -1},
                                     separators=(",", ":"), ensure_ascii=False))
            elif op == "worker_run":
                rid = K._worker_run_id_for(rest)
                print(json.dumps({"op": "worker_run", "task_id": rest,
                                 "run_id": -1 if rid is None else rid},
                                 separators=(",", ":"), ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())