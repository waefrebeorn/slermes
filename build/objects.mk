# ── Object File Lists ──────────────────────────────────────────────
# build/objects.mk -- cupboards and jars.
#
# Each CUPBOARD_* variable holds one subsystem's object files (the jars).
# Add a new object to its subsystem's cupboard, never to a global list.
# CLI_OBJ and PORT_OBJ are concatenations of their cupboards.
#
# Regenerate: python scripts/organize_objects.py
# Verify only: python scripts/organize_objects.py --check
# Included by the top-level Makefile.


# ── CLI_OBJ ──
CLI_OBJ = $(CUPBOARD_HERMES_CLI) $(CUPBOARD_CLI) $(CUPBOARD_AGENT) \
 $(CUPBOARD_GATEWAY) $(CUPBOARD_TOOLS) $(CUPBOARD_CRON) $(CUPBOARD_MISC) \
 $(PORT_OBJ)

# ── PORT_OBJ ──
PORT_OBJ = $(CUPBOARD_PORT_HERMES_CLI) $(CUPBOARD_PORT_CLI) \
 $(CUPBOARD_PORT_AGENT) $(CUPBOARD_PORT_WIRED) $(CUPBOARD_PORT_GATEWAY) $(CUPBOARD_PORT_TOOLS) \
 $(CUPBOARD_PORT_CRON) $(CUPBOARD_PORT_PLUGINS) $(CUPBOARD_PORT_MISC) \
 $(PORTS_LINKED_OBJ) $(CUPBOARD_UNBUILT_MODS)

# ── Object File Lists ──────────────────────────────────────────────
# build/objects.mk -- cupboards and jars.
#
# Each CUPBOARD_* variable holds one subsystem's object files (the jars).
# Add a new object to its subsystem's cupboard, never to a global list.
# CLI_OBJ and PORT_OBJ are concatenations of their cupboards.
#
# Regenerate: python scripts/organize_objects.py
# Verify only: python scripts/organize_objects.py --check
# Included by the top-level Makefile.


# ── CLI_OBJ ──

# ── PORT_OBJ ──

# ── Object File Lists ──────────────────────────────────────────────
# build/objects.mk -- cupboards and jars.
#
# Each CUPBOARD_* variable holds one subsystem's object files (the jars).
# Add a new object to its subsystem's cupboard, never to a global list.
# CLI_OBJ and PORT_OBJ are concatenations of their cupboards.
#
# Regenerate: python scripts/organize_objects.py
# Verify only: python scripts/organize_objects.py --check
# Included by the top-level Makefile.
#
# Dead port-style twins that compile AND link without a collision on this
# platform. Regenerate per platform: python scripts/link_dead_ports.py
# POSIX-only twins (pthread_timedjoin_np, dirent d_type, sys/syscall.h) are
# picked up automatically on Linux/macOS.
-include build/ports_linked.mk


# ── CLI_OBJ ──

# ── PORT_OBJ ──
# ── Object File Lists ──────────────────────────────────────────────
# build/objects.mk -- cupboards and jars.
#
# Each CUPBOARD_* variable holds one subsystem's object files (the jars).
# Add a new object to its subsystem's cupboard, never to a global list.
# CLI_OBJ and PORT_OBJ are concatenations of their cupboards.
#
# Regenerate: python scripts/organize_objects.py
# Verify only: python scripts/organize_objects.py --check
# Included by the top-level Makefile.


# ── CLI_OBJ ──

# ── PORT_OBJ ──

# ── Object File Lists ──────────────────────────────────────────────
# build/objects.mk -- cupboards and jars.
#
# Each CUPBOARD_* variable holds one subsystem's object files (the jars).
# Add a new object to its subsystem's cupboard, never to a global list.
# CLI_OBJ and PORT_OBJ are concatenations of their cupboards.
#
# Regenerate: python scripts/organize_objects.py
# Verify only: python scripts/organize_objects.py --check
# Included by the top-level Makefile.


# ── CLI_OBJ ──
CUPBOARD_HERMES_CLI = src/hermes_cli/partial_compress.o \
 src/hermes_cli/tool_categories_data.o \
 src/hermes_cli/fs_replace.o \
 src/hermes_cli/setup_pure.o src/hermes_cli/dashboard_procs.o \
 src/hermes_cli/webhook.o src/hermes_cli/web_models.o \
 src/hermes_cli/vercel_auth.o src/hermes_cli/urllib_security.o \
 src/hermes_cli/telegram_managed_bot.o src/hermes_cli/suggestions_cmd.o \
 src/hermes_cli/stdio.o src/hermes_cli/sqlite_runtime.o \
 src/hermes_cli/slack_cli.o src/hermes_cli/skin_engine.o \
 src/hermes_cli/skin_cmd.o src/hermes_cli/skills_hub.o \
 src/hermes_cli/setup_whatsapp_cloud.o src/hermes_cli/setup_hidden_env.o \
 src/hermes_cli/sessions_cmd.o src/hermes_cli/session_filters.o \
 src/hermes_cli/session_export_html.o src/hermes_cli/send_cmd.o \
 src/hermes_cli/security_audit_startup.o src/hermes_cli/secrets_cli.o \
 src/hermes_cli/secret_prompt.o src/hermes_cli/route_identity.o \
 src/hermes_cli/relaunch.o src/hermes_cli/pty_session.o \
 src/hermes_cli/psutil_android.o src/hermes_cli/proxy_cli.o \
 src/hermes_cli/providers.o src/hermes_cli/projects_cmd.o \
 src/hermes_cli/profile_distribution.o src/hermes_cli/profile_describer.o \
 src/hermes_cli/portal_cli.o src/hermes_cli/pets.o \
 src/hermes_cli/pairing.o src/hermes_cli/onepassword_secrets_cli.o \
 src/hermes_cli/nous_auth_keepalive.o src/hermes_cli/model_search.o \
 src/hermes_cli/moa_cmd.o src/hermes_cli/memory_oauth.o \
 src/hermes_cli/mcp_startup.o src/hermes_cli/mcp_picker.o \
 src/hermes_cli/mcp_config.o src/hermes_cli/mcp_catalog.o \
 src/hermes_cli/managed_scope.o src/hermes_cli/lifecycle.o \
 src/hermes_cli/kanban_specify.o src/hermes_cli/journey.o \
 src/hermes_cli/inventory.o src/hermes_cli/init_command.o \
 src/hermes_cli/gui_uninstall.o src/hermes_cli/gateway_enroll.o \
 src/hermes_cli/env_loader.o src/hermes_cli/dingtalk_auth.o \
 src/hermes_cli/diagnostics_upload.o src/hermes_cli/dep_ensure.o \
 src/hermes_cli/debug.o src/hermes_cli/dashboard_register.o \
 src/hermes_cli/curator.o src/hermes_cli/cron.o \
 src/hermes_cli/credential_lifecycle.o src/hermes_cli/copilot_auth.o \
 src/hermes_cli/codex_runtime_switch.o \
 src/hermes_cli/codex_runtime_plugin_migration.o \
 src/hermes_cli/codex_models.o src/hermes_cli/cli_billing_mixin.o \
 src/hermes_cli/cli_agent_setup_mixin.o src/hermes_cli/claw.o \
 src/hermes_cli/checkpoints.o src/hermes_cli/bundles.o \
 src/hermes_cli/browser_connect.o src/hermes_cli/banner.o \
 src/hermes_cli/auth_commands.o src/hermes_cli/approval_mode.o \
 src/hermes_cli/active_sessions.o src/hermes_cli/_subprocess_compat.o \
 src/hermes_cli/_parser.o src/hermes_cli/_early_recovery.o \
 src/hermes_cli/approval_transport.o src/hermes_cli/config_env.o \
 src/hermes_cli/config_platforms.o src/hermes_cli/config_profile.o \
 src/hermes_cli/config_diff.o src/hermes_cli/config_io.o \
 src/hermes_cli/config_schema.o src/hermes_cli/config_migrate.o \
 src/hermes_cli/cli_output.o \
 src/plugins/platforms/qqbot/main.o src/hermes_cli/doctor.o src/hermes_cli/setup.o \
 src/hermes_cli/linux_desktop_entry.o src/hermes_cli/banner_pure.o \
 src/hermes_cli/banner_boot.o src/hermes_cli/context_switch_guard.o \
 src/hermes_cli/windows.o src/plugins/platforms/yuanbao/nous_billing.o \
 src/hermes_cli/nous_billing_ports.o \
 src/hermes_cli/nous_subscription_ports.o src/hermes_cli/proxy/adapters/proxy_xai_ports.o \
 src/hermes_cli/proxy/adapters/proxy_adapters_base_ports.o src/hermes_cli/proxy/server.o \
 src/hermes_cli/proxy/cli.o src/hermes_cli/console_engine_ports.o \
 src/hermes_cli/config_pure.o src/hermes_cli/status_helpers.o \
 src/hermes_cli/logs.o src/hermes_cli/write_approval_commands.o \
 src/hermes_cli/cli_cmd_help.o src/hermes_cli/cli_cmd_misc.o \
 src/hermes_cli/subcommands/whatsapp.o \
 src/hermes_cli/subcommands/webhook.o src/hermes_cli/subcommands/update.o \
 src/hermes_cli/subcommands/uninstall.o src/hermes_cli/subcommands/tools.o \
 src/hermes_cli/subcommands/sync.o src/hermes_cli/subcommands/status.o \
 src/hermes_cli/subcommands/slack.o src/hermes_cli/subcommands/skin.o \
 src/hermes_cli/subcommands/skills.o src/hermes_cli/subcommands/setup.o \
 src/hermes_cli/subcommands/security.o \
 src/hermes_cli/subcommands/prompt_size.o \
 src/hermes_cli/subcommands/profile.o src/hermes_cli/subcommands/plugins.o \
 src/hermes_cli/subcommands/pairing.o \
 src/hermes_cli/subcommands/monitoring.o \
 src/hermes_cli/subcommands/model.o src/hermes_cli/subcommands/memory.o \
 src/hermes_cli/subcommands/mcp.o src/hermes_cli/subcommands/logs.o \
 src/hermes_cli/subcommands/logout.o src/hermes_cli/subcommands/login.o \
 src/hermes_cli/subcommands/insights.o \
 src/hermes_cli/subcommands/import_cmd.o \
 src/hermes_cli/subcommands/import_agent.o \
 src/hermes_cli/subcommands/hooks.o src/hermes_cli/subcommands/gui.o \
 src/hermes_cli/subcommands/gateway.o src/hermes_cli/subcommands/dump.o \
 src/hermes_cli/subcommands/doctor.o src/hermes_cli/subcommands/debug.o \
 src/hermes_cli/subcommands/dashboard.o src/hermes_cli/subcommands/cron.o \
 src/hermes_cli/subcommands/console.o src/hermes_cli/subcommands/config.o \
 src/hermes_cli/subcommands/claw.o src/hermes_cli/subcommands/backup.o \
 src/hermes_cli/subcommands/auth.o src/hermes_cli/subcommands/approvals.o \
 src/hermes_cli/subcommands/acp.o src/hermes_cli/subcommands/_shared.o \
 src/hermes_cli/cli_cmd_skills.o src/hermes_cli/cli_cmd_tools.o \
 src/hermes_cli/cli_cmd_mcp.o src/hermes_cli/cli_cmd_kanban.o \
 src/hermes_cli/cli_cmd_display.o src/hermes_cli/cli_cmd_parity.o \
 src/hermes_cli/terminal_breadcrumbs_pure.o \
 src/hermes_cli/plugin_index_pure.o \
 src/hermes_cli/plugin_capabilities_pure.o \
 src/persistence/sqlite/admin/session_lost_and_found_pure.o \
 src/hermes_cli/foreign_sessions_pure.o \
 src/hermes_cli/subprocess_compat_pure.o \
 src/hermes_cli/plugin_packs_pure.o src/hermes_cli/plugins_cmd.o \
 src/hermes_cli/plugins_gaps.o src/hermes_cli/stderr_timestamp_pure.o \
 src/hermes_cli/kanban_db_pure.o src/hermes_cli/cli_kanban_goal_loop_q.o \
 src/hermes_cli/goals_pure.o src/hermes_cli/loops_pure.o \
 src/hermes_cli/loops_manager.o src/hermes_cli/console_engine_pure.o \
 src/hermes_cli/model_cost_guard.o src/hermes_cli/profiles.o \
 src/hermes_cli/web_server_status.o \
 src/hermes_cli/web_routers/web_server_managed_files.o src/hermes_cli/web_server_fs.o \
 src/hermes_cli/web_server_events.o src/hermes_cli/web_server_console.o \
 src/hermes_cli/web_server_themes.o src/hermes_cli/web_server_ws_auth.o \
 src/hermes_cli/dashboard_auth/hermes_cli_ws_tickets.o \
 src/hermes_cli/web_server_chat_argv.o \
 src/hermes_cli/web_server_cron_dash.o \
 src/hermes_cli/web_server_sessions_admin.o \
 src/hermes_cli/web_server_session_detail.o \
 src/hermes_cli/web_server_session_endpoints.o \
 src/hermes_cli/web_server_prune.o src/hermes_cli/default_soul.o \
 src/agent/gemini_cloudcode_adapter_methods.o \
 src/hermes_cli/dashboard_auth/dashboard_auth_prefix.o src/hermes_cli/hooks.o \
 src/hermes_cli/prompt_size.o src/hermes_cli/voice.o \
 src/hermes_cli/display_diff.o src/hermes_cli/gitlock.o \
 src/hermes_cli/model_data_policy_guard.o \
 src/hermes_cli/model_selection_guards.o src/hermes_cli/config_defaults.o \
 src/hermes_cli/web_server_paths.o src/gateway/pairing_helpers.o \
 src/gateway/platforms/webhook_helpers.o src/hermes_cli/plugin_dev.o \
 src/hermes_cli/platform_actions.o src/hermes_cli/verify_cmd.o

CUPBOARD_CLI = src/cli/cli_gaps_2026_08_25.o src/cli/cli.o \
 src/cli/cli_helpers2.o src/cli/cli_helpers3.o \
 src/cli/hermes_cli_agent_b2.o src/cli/hermes_cli_agent_b3.o \
 src/cli/hermes_cli_agent_b4.o src/cli/hermes_cli_agent_b5.o \
 src/cli/config.o src/hermes_cli/config_setup.o src/cli/paths.o \
 src/agent/display_core.o src/cli/cli_cmd_session.o src/cli/cli_cmd_config.o \
 src/cli/cli_cmd_gateway.o src/cli/cli_cmd_security.o \
 src/cli/cli_cmd_memory.o src/cli/vault_login_tokens.o \
 src/cli/port_anthropic_credentials.o src/agent/port_acp_client.o \
 src/cli/port_access_policy.o src/cli/config_env_file.o \
 src/cli/cli_hermes_cli_methods.o

CUPBOARD_AGENT = src/agent/turn_summary.o src/agent/redact_helpers.o \
 src/agent/repetition_guard.o src/agent/port_agent_turn_context.o \
 src/agent/tts_provider.o src/agent/interrupt_compat.o \
 src/agent/web_search_registry.o src/agent/video_gen_provider.o \
 src/agent/trace_upload.o src/agent/tool_result_classification.o \
 src/agent/thread_scoped_output.o src/agent/subscription_view.o \
 src/agent/stream_single_writer.o src/agent/rate_limit_tracker.o \
 src/agent/oneshot.o src/agent/moa_trace.o src/agent/kanban_stop.o \
 src/agent/jiter_preload.o src/agent/credential_pool.o \
 src/agent/bounded_response.o src/tools/account_usage.o \
 src/agent/omarchy_usage.o

CUPBOARD_GATEWAY = src/gateway/lifecycle_ledger.o src/gateway/memory_status.o \
 src/gateway/gateway_run_match.o src/gateway/platforms/yuanbao_proto.o \
 src/plugins/platforms/yuanbao/yuanbao_proto_ports.o \
 src/gateway/platforms/msgraph_webhook_helpers.o

CUPBOARD_TOOLS = src/tools/url_safety_helpers.o src/agent/browser_redact.o \
 src/tools/browser_tool_eval.o

CUPBOARD_CRON = src/cron/cron_jobs_time.o

CUPBOARD_MISC = src/battery.o


# ── PORT_OBJ ──
CUPBOARD_PORT_HERMES_CLI = src/hermes_cli/browser_connect_gaps.o src/hermes_cli/web_server_gaps.o src/hermes_cli/setup_gaps.o src/hermes_cli/web_routers/local_models_gaps.o src/hermes_cli/update_cmd_gaps.o src/hermes_cli/proxy/adapters/proxy_adapters_base.o src/hermes_cli/proxy/adapters/proxy_adapters_xai.o src/hermes_cli/proxy/adapters/proxy_adapters_nous_portal.o src/hermes_cli/anthropic_adapter.o src/hermes_cli/bedrock_adapter.o src/hermes_cli/web_server_simple.o src/hermes_cli/context_compressor.o src/hermes_cli/display.o src/hermes_cli/display_tool_preview.o src/hermes_cli/oneshot.o src/hermes_cli/cli.o src/hermes_cli/channel_directory.o src/plugins/platforms/feishu/platforms_feishu_meeting_invite.o src/hermes_cli/platforms_helpers.o src/plugins/platforms/qqbot/platforms_qqbot_crypto.o src/plugins/platforms/qqbot/platforms_qqbot_keyboards.o src/plugins/platforms/qqbot/platforms_qqbot_onboard.o src/plugins/platforms/qqbot/platforms_qqbot_utils.o src/plugins/platforms/signal/platforms_signal_rate_limit.o src/plugins/platforms/sms/platforms_sms.o src/hermes_cli/platforms_telegram_network.o src/plugins/platforms/whatsapp_cloud/platforms_wecom_callback.o src/plugins/platforms/wecom/platforms_wecom_crypto.o src/plugins/platforms/yuanbao/platforms_yuanbao_media.o src/plugins/platforms/yuanbao/platforms_yuanbao_sticker.o src/hermes_cli/scale_to_zero.o src/hermes_cli/signal_format.o src/hermes_cli/whatsapp_identity.o src/hermes_cli/shutdown_forensics.o src/hermes_cli/models.o src/hermes_cli/model_catalog.o src/hermes_cli/main_helpers.o src/hermes_cli/turn_summary_cli.o src/hermes_cli/main_cmds.o src/hermes_cli/approvals_suggest.o src/hermes_cli/checkpoints_format.o src/hermes_cli/backup.o src/hermes_cli/pty_clamp_helpers.o src/hermes_cli/gateway_platform.o src/hermes_cli/kanban_helpers.o src/hermes_cli/kanban_cmd_gaps.o src/hermes_cli/fallback_config.o src/hermes_cli/timeouts.o src/hermes_cli/session_listing.o src/hermes_cli/project_tree.o src/hermes_cli/models_helpers.o src/hermes_cli/models_net.o src/hermes_cli/models_pure_impl.o src/hermes_cli/models_pure.o src/hermes_cli/models_validate.o src/hermes_cli/models_gaps.o src/hermes_cli/shared_metrics.o src/hermes_cli/managed_scope_helpers.o src/agent/context_breakdown_helpers.o src/hermes_cli/verification_stop_helpers.o src/cron/blueprint_catalog_helpers.o src/agent/vertex_adapter_helpers.o src/hermes_cli/drain_control_helpers.o src/gateway/platforms/platforms_base_helpers.o src/hermes_cli/gateway_command_sanitize.o src/hermes_cli/blueprint_cmd.o src/hermes_cli/goals_data.o src/hermes_cli/goals_manager.o src/hermes_cli/goals_helpers.o src/hermes_cli/provider_meta.o src/hermes_cli/completion.o src/hermes_cli/completion_ports.o src/gateway/scale_to_zero_helpers.o src/cron/jobs_helpers.o src/hermes_cli/profiles_helpers.o src/plugins/platforms/yuanbao/yuanbao_proto_helpers.o src/agent/learning_graph_render_helpers.o src/tools/lazy_deps_helpers.o src/hermes_cli/model_switch_helpers.o src/hermes_cli/learning_graph_helpers.o src/agent/learning_graph.o src/agent/learning_graph_render.o src/agent/learning_mutations.o src/tools/file_state_helpers.o src/tools/fuzzy_match_helpers.o src/tools/file_tools_helpers.o src/tools/computer_use/cua_backend_helpers.o src/tools/delegate_tool_helpers.o src/hermes_cli/browser_camofox.o src/hermes_cli/discord_helpers.o src/pet/pet_render.o src/plugins/platforms/qqbot/whatsapp_common_helpers.o src/plugins/platforms/qqbot/whatsapp_cloud_helpers.o src/hermes_cli/clipboard_helpers.o src/hermes_cli/doctor_helpers.o src/tools/code_execution_helpers.o src/tools/environments/environments_daytona.o src/hermes_cli/environments_file_sync.o src/tools/environments/environments_managed_modal.o src/tools/environments/environments_modal_utils.o src/tools/environments/environments_singularity.o src/tools/environments/environments_ssh.o src/hermes_cli/managed_tool_gateway.o src/hermes_cli/mcp_oauth_manager.o src/plugins/teams_pipeline/microsoft_graph_auth.o src/hermes_cli/skills_guard.o src/hermes_cli/write_approval.o src/hermes_cli/auth.o src/hermes_cli/auth_store.o src/hermes_cli/mcp_security.o src/hermes_cli/security_advisories.o src/hermes_cli/kanban_db.o src/hermes_cli/web_server.o src/hermes_cli/web_routers/profiles.o src/hermes_cli/web_routers/cron.o src/hermes_cli/dashboard_auth/token_auth.o src/hermes_cli/dashboard_auth/routes.o src/hermes_cli/dashboard_auth/prefix.o src/hermes_cli/dashboard_auth/native_flow.o src/hermes_cli/dashboard_auth/middleware.o src/hermes_cli/dashboard_auth/login_page.o src/hermes_cli/dashboard_auth/cookies.o src/hermes_cli/dashboard_auth/base.o src/hermes_cli/yuanbao_tools.o src/hermes_cli/memory_setup.o src/hermes_cli/toolset_validation.o src/hermes_cli/extra.o src/hermes_cli/config.o src/hermes_cli/container_boot.o src/hermes_cli/dump.o src/hermes_cli/dump_ports.o src/hermes_cli/journey_ports.o src/hermes_cli/kanban_decompose_ports.o src/hermes_cli/pairing_ports.o src/hermes_cli/gateway.o src/hermes_cli/goals.o src/hermes_cli/goals_ports.o src/hermes_cli/profile_distribution_ports.o src/hermes_cli/projects_db_ports.o src/hermes_cli/pty_bridge_ports.o src/hermes_cli/win_pty_bridge_ports.o src/hermes_cli/service_manager_ports.o src/hermes_cli/active_sessions_ports.o src/hermes_cli/kanban_schema.o src/hermes_cli/kanban_model.o src/hermes_cli/kanban_tasks.o src/hermes_cli/kanban_lifecycle.o src/hermes_cli/kanban_notify.o src/hermes_cli/kanban_stats.o src/hermes_cli/kanban_runs.o src/hermes_cli/kanban_query.o src/hermes_cli/kanban_util.o src/hermes_cli/kanban_boards.o src/hermes_cli/kanban_decompose.o src/hermes_cli/kanban_workers.o src/hermes_cli/web_routers/memory_providers.o src/hermes_cli/dashboard_auth/dashboard_auth_audit.o src/hermes_cli/dashboard_auth/dashboard_auth_registry.o src/hermes_cli/dashboard_auth/dashboard_auth_cookies.o src/hermes_cli/kanban_diagnostics.o src/hermes_cli/kanban_diagnostics_ports.o src/hermes_cli/nous_account_ports.o src/hermes_cli/proxy/adapters/nous_portal_ports.o src/hermes_cli/uninstall_ports.o src/hermes_cli/proxy_cli_ports.o src/hermes_cli/windows_ssh_runtime_ports.o src/hermes_cli/hermes_state_helpers2.o src/hermes_cli/bang_shell_ports.o src/hermes_cli/commands_mixin_wrappers.o src/hermes_cli/config_ports.o src/hermes_cli/pet_input.o src/hermes_cli/config_py_io.o src/hermes_cli/util_helpers.o src/hermes_cli/pty_session_ports.o src/hermes_cli/startup_fast.o src/hermes_cli/auth_wrappers.o src/hermes_cli/plugins_cmd_wrappers.o src/hermes_cli/setup_ports.o src/hermes_cli/plugins_wrappers.o src/hermes_cli/tools_config_cua.o src/hermes_cli/tools_config_install.o src/hermes_cli/plugins_core.o src/hermes_cli/tools_config_providers.o src/hermes_cli/tools_config_toolsets.o src/hermes_cli/platform_tools.o src/hermes_cli/session_title.o src/hermes_cli/personality.o src/hermes_cli/helpers2.o src/hermes_cli/config_py_pure.o src/utils/utils_truthy.o src/hermes_cli/session_export_wrappers.o src/hermes_cli/moa_config.o src/hermes_cli/azure_detect.o src/hermes_cli/fallback_cmd.o src/hermes_cli/session_recap.o src/hermes_cli/middleware.o src/hermes_cli/curses_ui.o src/hermes_cli/security_audit.o src/hermes_cli/model_normalize.o src/hermes_cli/provider_catalog.o src/hermes_cli/model_switch.o src/hermes_cli/billing_json_helpers.o src/hermes_cli/runtime_provider.o src/hermes_cli/sqlite_util.o src/hermes_cli/projects_db.o src/hermes_cli/debug_cli.o src/hermes_cli/auth_helpers.o src/hermes_cli/kanban_format.o src/hermes_cli/prompt_stash.o src/hermes_cli/web_server_auth.o src/hermes_cli/web_server_schema_path.o src/plugins/platforms/yuanbao/web_git.o src/hermes_cli/web_routers/web_git_routes.o src/hermes_cli/web_update.o src/hermes_cli/update_cmd.o src/hermes_cli/update_cmd_more.o src/hermes_cli/gateway_gaps.o src/hermes_cli/auth_gaps.o src/hermes_cli/banner_gaps.o src/hermes_cli/profile_gaps.o src/hermes_cli/agent_import.o src/hermes_cli/plugin_manifest.o src/hermes_cli/config_helpers.o src/hermes_cli/command_registry.o src/hermes_cli/shared_metrics_contract.o src/hermes_cli/port_update_pause_record.o src/hermes_cli/port_left_core_migration.o src/hermes_cli/port_model_switch.o src/hermes_cli/observability/smc_event_extractors.o src/hermes_cli/heartbeat.o src/hermes_cli/focus_view.o src/hermes_cli/slash_exec.o src/hermes_cli/kanban_swarm.o src/hermes_cli/skills_config.o src/hermes_cli/_parser_gaps3.o src/hermes_cli/agent_plugins_gaps3.o src/hermes_cli/approvals_test_gaps3.o src/hermes_cli/azure_detect_gaps3.o src/hermes_cli/banner_gaps3.o src/hermes_cli/build_info_gaps3.o src/hermes_cli/bundles_gaps3.o src/hermes_cli/callbacks_gaps3.o src/hermes_cli/checkpoints_gaps3.o src/hermes_cli/cli_agent_setup_mixin_gaps3.o src/hermes_cli/cli_output_gaps3.o src/hermes_cli/clipboard_gaps3.o src/hermes_cli/codex_runtime_switch_gaps3.o src/hermes_cli/container_boot_gaps3.o src/hermes_cli/curator_gaps3.o src/hermes_cli/dashboard_auth/base_gaps3.o src/hermes_cli/dashboard_auth/routes_gaps3.o src/hermes_cli/dashboard_procs_gaps3.o src/hermes_cli/dep_ensure_gaps3.o src/hermes_cli/dingtalk_auth_gaps3.o src/hermes_cli/fallback_cmd_gaps3.o src/hermes_cli/gitlock_gaps3.o src/hermes_cli/image_provenance_gaps3.o src/hermes_cli/install_identity_gaps3.o src/hermes_cli/kanban_gaps3.o src/hermes_cli/kanban_db_gaps3.o src/hermes_cli/kanban_swarm_gaps3.o src/hermes_cli/local_runtime/capabilities_gaps3.o src/hermes_cli/local_runtime/detect_gaps3.o src/hermes_cli/managed_uv_gaps3.o src/hermes_cli/mcp_picker_gaps3.o src/hermes_cli/migrate_gaps3.o src/hermes_cli/model_catalog_gaps3.o src/hermes_cli/model_cost_guard_gaps3.o src/hermes_cli/models_gaps3.o src/hermes_cli/nous_account_gaps3.o src/hermes_cli/nous_billing_gaps3.o src/hermes_cli/nous_subscription_gaps3.o src/hermes_cli/observability/shared_metrics_gaps3.o src/hermes_cli/observability/shared_metrics_send_config_gaps3.o src/hermes_cli/oneshot_gaps3.o src/hermes_cli/partial_compress_gaps3.o src/hermes_cli/platforms_gaps3.o src/hermes_cli/plugin_capabilities_gaps3.o src/hermes_cli/plugin_index_gaps3.o src/hermes_cli/plugin_packs_gaps3.o src/hermes_cli/portal_cli_gaps3.o src/hermes_cli/process_identity_gaps3.o src/hermes_cli/profile_describer_gaps3.o src/hermes_cli/projects_db_gaps3.o src/hermes_cli/providers_gaps3.o src/hermes_cli/psutil_android_gaps3.o src/hermes_cli/pty_bridge_gaps3.o src/hermes_cli/relaunch_gaps3.o src/hermes_cli/resource_limits_gaps3.o src/hermes_cli/secrets_cli_gaps3.o src/hermes_cli/session_lost_and_found_gaps3.o src/hermes_cli/sessions_cmd_gaps3.o src/hermes_cli/skin_engine_gaps3.o src/hermes_cli/stdio_gaps3.o src/hermes_cli/subcommands/dashboard_gaps3.o src/hermes_cli/subcommands/peer_gaps3.o src/hermes_cli/telegram_managed_bot_gaps3.o src/hermes_cli/terminal_breadcrumbs_gaps3.o src/hermes_cli/toolset_validation_gaps3.o src/hermes_cli/uninstall_gaps3.o src/hermes_cli/update_contract_gaps3.o src/hermes_cli/update_inventory_gaps3.o src/hermes_cli/voice_gaps3.o src/hermes_cli/web_git_gaps3.o src/hermes_cli/web_routers/git_gaps3.o src/hermes_cli/web_routers/mcp_gaps3.o src/hermes_cli/web_routers/sessions_gaps3.o src/hermes_cli/web_routers/skills_gaps3.o src/hermes_cli/webhook_gaps3.o src/hermes_cli/win_pty_bridge_gaps3.o src/hermes_cli/xai_retirement_gaps3.o src/hermes_cli/port_config.o src/hermes_cli/port_config_nested.o src/hermes_cli/port_auth.o src/hermes_cli/provider_registry.o

CUPBOARD_PORT_CLI = \
 \
 \
 \
 \
 \
 \
 \
 src/cli/port_agent_provider.o src/agent/port_agent_skill_utils.o \
 src/agent/port_anthropic_message_convert.o src/cli/port_api_store_error.o \
 src/cli/port_approval_transport.o src/cli/port_audit.o \
 src/hermes_cli/port_auth_commands.o src/hermes_cli/port_auth_error_copy.o \
 src/cli/port_auth_providers.o src/hermes_cli/port_auth_qwen.o \
 src/cli/port_aux_outcome_event.o src/cli/port_aux_task_builder.o \
 src/hermes_cli/port_backup_sqlite.o src/hermes_cli/port_banner.o \
 src/cli/port_blueprint_plugins.o src/cli/port_bot_loop_guard.o \
 src/hermes_cli/port_browser_connect.o src/tools/port_browser_launcher.o \
 src/cli/port_callbacks.o src/cli/port_cli_config_load.o \
 src/cli/port_cli_process_dock.o src/hermes_cli/port_clipboard.o \
 src/hermes_cli/port_codex_models.o src/cli/port_comp_telemetry.o \
 src/agent/port_compression_facade.o src/tools/connectors/port_connectors_contract.o \
 src/cli/port_dashboard_auth_audit.o src/cli/port_models_validate.o \
 src/hermes_cli/port_models_pricing.o src/hermes_cli/port_model_picker_usage.o \
 src/hermes_cli/port_anon_auth.o src/hermes_cli/port_version_info.o \
 src/hermes_cli/port_container_boot.o src/cli/port_context_compressor_telemetry.o \
 src/cron/port_cron_notepad.o src/cli/port_cronjob_prompt_scan.o \
 src/hermes_cli/port_dashboard_register.o src/cli/port_delegation_registry.o \
 src/hermes_cli/port_dingtalk_auth.o src/hermes_cli/port_early_recovery.o \
 src/cli/port_fast_mode.o src/cli/port_file_operations_native.o \
 src/cli/port_first_time.o src/hermes_cli/port_gateway_command_errors.o \
 src/gateway/port_gateway_platform_activation.o \
 src/gateway/port_gateway_profile_routing.o src/gateway/port_gateway_status.o \
 src/gateway/port_gateway_wake.o src/agent/port_gemini_native_adapter.o \
 src/hermes_cli/port_hermes_auth.o src/hermes_cli/port_hermes_auth_nous.o \
 src/cli/port_hermes_cli_misc.o src/cli/port_hosted_room_driver.o \
 src/cli/port_hosted_rooms.o src/gateway/port_hosted_rooms_common.o \
 src/cli/port_idle_watch.o src/agent/port_image_token_cost.o \
 src/hermes_cli/port_input_sanitize.o src/hermes_cli/port_inventory.o \
 src/hermes_cli/port_kanban_db_dispatch.o src/tools/port_kanban_schemas.o \
 src/hermes_cli/web_routers/port_local_models.o src/gateway/relay/port_media_labels.o \
 src/gateway/port_memory_status.o src/cli/port_methods_session.o \
 src/tui_gateway/port_methods_tools.o src/cli/port_micro_env.o \
 src/hermes_cli/port_nous_subscription.o src/cli/port_parked_notice.o \
 src/cli/port_plugin_messages.o src/cli/port_portal_cli.o \
 src/cli/port_producer_lifecycle.o src/cli/port_profile_describer.o \
 src/agent/port_prompt_builder.o src/hermes_cli/port_pt_input_extras.o \
 src/cli/port_readiness.o src/hermes_cli/port_relaunch.o \
 src/agent/port_repetition_guard.o src/cli/port_rfb_filter.o \
 src/cli/port_run_inbound_unauthorized.o src/cli/port_scheduler_tick.o \
 src/hermes_cli/port_secrets_cli.o src/cli/port_send_cmd.o src/tui_gateway/port_server.o \
 src/agent/port_session_activity.o src/cli/port_session_managed_worker_log.o \
 src/cli/port_session_owned.o src/cli/port_session_server_requests.o \
 src/agent/port_side_question_request.o src/cli/port_sizefmt.o \
 src/agent/port_skill_bundles.o src/agent/port_skill_commands.o \
 src/cli/port_slack_cli.o src/persistence/sqlite/_state/port_state_repair.o \
 src/gateway/port_stream_consumer_think.o src/hermes_cli/port_telegram_managed_bot.o \
 src/tools/port_terminal_tool.o src/plugins/teams_pipeline/port_tools_microsoft_graph_client.o \
 src/tools/port_tools_xai_http.o src/cli/port_turn_lease.o \
 src/hermes_cli/port_update_cmd.o src/hermes_cli/port_update_receipt.o \
 src/cli/port_update_windows.o src/cli/port_url_utils.o \
 src/agent/port_vault_login_classifier.o src/agent/port_vault_login_js.o \
 src/cli/port_webhook_coalesce.o src/hermes_cli/port_worktree_ops.o

CUPBOARD_PORT_AGENT = src/cli/port_tui_dispatch_flow.o src/cli/port_gateway_agent_flow.o src/cli/port_gateway_windows_flow.o src/gateway/relay/relay_adapter_flow.o src/hermes_cli/observability/relay_shared_metrics_flow.o src/gateway/session_policy_flow.o src/gateway/run_startup_flow.o src/gateway/run_shutdown_flow.o src/cli/port_agent_runtime_helpers_flow.o src/cli/port_update_lock_flow.o src/cli/port_stream_consumer_flow.o src/tools/terminal_tool_lifecycle_flow.o src/cli/port_gw_service_lifecycle.o src/hermes_cli/config_load_save_flow.o src/hermes_cli/port_kanban_db_flow.o src/agent/port_credential_pool_flow.o src/gateway/platforms/base_flow.o src/agent/auxiliary_client_flow.o src/gateway/platforms/api_server_flow.o src/agent/port_conversation_compression_flow.o src/cron/port_scheduler_flow.o src/agent/port_chat_completion_helpers.o src/agent/context_compressor_flow.o src/gateway/run_flow.o src/agent/chat_completion_helpers_flow.o src/agent/verify/verify_recipes.o src/gateway/slash_commands.o src/agent/image_gen_provider_ports.o src/agent/image_gen_registry_ports.o src/agent/proxy_sources/iron_proxy_ports.o src/agent/redact_ports.o src/agent/skill_bundles_ports.o src/agent/verify/verify_environment.o src/hermes_cli/timefmt.o src/tools/patch.o src/agent/credential_pool_custom.o src/agent/anthropic_adapter.o src/agent/aux_accounting.o src/agent/background_review_ports.o src/agent/battery.o src/agent/bedrock_adapter_ports.o src/agent/billing_view_ports.o src/agent/chat_completions_ports.o src/agent/copilot_acp_client_ports.o src/agent/credential_pool_ports.o src/agent/credential_sources_ports.o src/gateway/platforms/media_cache.o src/hermes_cli/state_usage_db.o src/tools/media_cache.o src/tools/skills_hub.o src/agent/pet/pet_render.o src/agent/account_usage.o src/tools/threat_patterns.o src/agent/auxiliary_client_gaps.o src/agent/display_ports.o src/agent/error_classifier.o src/agent/google_oauth.o src/agent/web_search_provider.o src/persistence/sqlite/_state/hermes_state.o src/persistence/sqlite/_state/hermes_state_archive.o src/persistence/sqlite/_state/hermes_state_common.o src/persistence/sqlite/_state/hermes_state_db_identity.o src/persistence/sqlite/_state/hermes_state_fd_budget.o src/persistence/sqlite/_state/hermes_state_gaps.o src/persistence/sqlite/_state/hermes_state_journal.o src/persistence/sqlite/_state/hermes_state_lifecycle.o src/persistence/sqlite/_state/hermes_state_lineage.o src/persistence/sqlite/_state/hermes_state_locks.o src/persistence/sqlite/_state/hermes_state_messages.o src/persistence/sqlite/_state/hermes_state_misc.o src/persistence/sqlite/_state/hermes_state_msgs.o src/persistence/sqlite/_state/hermes_state_open.o src/persistence/sqlite/_state/hermes_state_ops.o src/persistence/sqlite/_state/hermes_state_portability.o src/persistence/sqlite/_state/hermes_state_ports.o src/persistence/sqlite/_state/hermes_state_repair.o src/persistence/sqlite/_state/hermes_state_schema.o src/persistence/sqlite/_state/hermes_state_search.o src/persistence/sqlite/_state/hermes_state_session.o src/persistence/sqlite/_state/hermes_state_session2.o src/persistence/sqlite/_state/hermes_state_tokens.o src/persistence/sqlite/_state/port_hermes_state_keys.o src/agent/skill_commands.o src/agent/context_compressor_pure.o src/agent/prompt_builder.o src/agent/context.o src/agent/provider_metadata.o src/agent/context_compressor_ports.o src/agent/agent_runtime_pure.o src/plugins/platforms/bluebubbles/redact.o src/agent/auxiliary_client_helpers.o src/agent/models_dev.o src/agent/agent_message_sanitize.o src/agent/vault.o src/agent/learn_prompt.o src/agent/file_safety.o src/agent/context_breakdown.o src/agent/estop.o src/agent/curator.o src/agent/plugin_ext.o src/agent/subagent_lifecycle.o src/agent/image_gen_provider.o src/agent/video_gen_provider_ports.o src/agent/browser_registry.o src/agent/budget_tracker.o src/agent/file_mutation_verifier.o src/agent/turn_finalizer.o src/agent/nous_rate_guard.o src/agent/hook_registry.o src/agent/image_routing.o src/agent/deadline.o src/agent/agent_message_repair.o src/agent/trajectory.o src/agent/skill_bundles.o src/agent/subdir_hints.o src/agent/system_prompt.o src/agent/todo_hydrate.o src/agent/conversation_loop.o src/agent/agent_loop.o src/agent/display_helpers.o src/agent/audit.o src/agent/checkpoint.o src/agent/tool_guardrails.o src/agent/markdown_render.o src/agent/insights.o src/agent/onboarding.o src/agent/outbound_webhooks.o src/plugins/platforms/qqbot/shell_hooks.o src/agent/usage_pricing.o src/agent/provider_profile_apply.o src/agent/codex_responses_adapter.o src/agent/api_error_summary.o src/agent/transports/transport_common.o src/agent/portal_tags.o src/agent/provider_profile.o src/agent/provider_anthropic.o src/agent/provider_azure.o src/agent/provider_bedrock.o src/agent/provider_codex_responses.o src/agent/provider_custom.o src/agent/provider_deepseek.o src/agent/provider_google.o src/agent/provider_openai.o src/agent/provider_openrouter.o src/agent/provider_xai.o src/agent/prompt_caching.o src/agent/credential_pool_engine.o src/agent/provider_profiles_builtin.o src/agent/sanitize.o src/agent/auxiliary_client.o src/agent/chat_completion_helpers.o src/agent/llm_client.o src/agent/provider.o src/agent/message_sanitization.o src/agent/message_metadata.o src/agent/anthropic_adapter_ports.o src/agent/logger.o src/agent/port_conversation_compression.o src/agent/port_model_metadata.o src/agent/chat_completion_helpers_pure.o src/agent/reasoning_timeouts.o src/agent/secret_sources/registry_gaps3.o src/agent/ssl_verify_gaps3.o src/agent/terminal_env_registry_gaps3.o src/agent/title_generator_gaps3.o src/agent/transports/chat_completions_gaps3.o src/agent/transports/types_gaps3.o src/agent/usage_pricing_gaps3.o src/agent/verification_evidence_gaps3.o src/agent/vertex_adapter_gaps3.o src/agent/web_search_registry_gaps3.o src/agent/port_error_surface.o src/agent/port_image_eviction_policy.o src/agent/port_compaction_events.o src/agent/port_markdown_tables.o src/agent/port_turn_liveness.o src/agent/azure_identity_adapter_ports.o src/agent/browser_provider_ports.o src/agent/browser_registry_ports.o src/agent/transports/codex_event_projector_ports.o src/agent/codex_responses_adapter_ports.o src/agent/context_engine_ports.o src/agent/credits_tracker_ports.o src/agent/file_safety_ports.o src/agent/transports/hermes_tools_mcp_server_ports.o src/agent/insights_ports.o src/agent/iteration_budget_ports.o src/agent/port_auxiliary_client.o src/agent/port_display.o src/agent/port_agent_secret_scope.o src/agent/credits_tracker.o

CUPBOARD_PORT_WIRED = src/gateway/platforms/platforms_helpers_flow.o src/agent/bedrock_adapter_flow.o src/gateway/run_busy_flow.o src/cli/port_ms_providers_flow.o src/agent/redact_flow.o src/gateway/platforms/openai_routes_flow.o src/cli/port_model_switch_flow.o src/cli/port_main_desktop_flow.o src/cli/port_gateway_migrate_flow.o src/tools/mcp_oauth_flow.o src/gateway/hosted_rooms_flow.o src/gateway/platforms/api_server_runs_flow.o src/tools/environments/local_env_flow.o src/gateway/run_adapters_flow.o src/agent/moa_loop_flow.o src/cron/cron_jobs_flow.o src/cli/port_main_cli_flow.o src/tools/browser_launcher_flow.o src/hermes_cli/config_migration_flow.o src/gateway/gateway_inbound_flow.o src/tools/transcribe.o src/gui_core.o src/gateway/platforms_base.o src/tools/skill_provenance.o src/gateway/platforms/api_server_session_json.o src/cli/port_gateway_status_flow.o src/cli/port_models_catalog_flow.o src/tools/process_registry_flow.o src/hermes_cli/kanban_db_ports.o src/tools/terminal_hints.o src/agent/secret_sources/secret_common.o src/hermes_cli/tools_config_reconfig.o src/tools/sandbox.o src/tools/vision_analysis_native.o src/desktop_app_common.o src/agent/codex_runtime_gaps.o src/agent/compaction_display.o src/agent/context_compressor_gaps.o src/agent/context_compressor_micro.o src/agent/context_compressor_wrappers.o src/agent/context_engine.o src/agent/context_references.o src/agent/context_references_gaps3.o src/agent/conversation_compression_gaps.o src/agent/conversation_compression_gaps2.o src/agent/conversation_compression_helpers.o src/agent/conversation_loop_gaps.o src/agent/conversation_loop_py_parity.o src/agent/copilot_acp_client_helpers2.o src/agent/core_helpers.o src/agent/credential_pool_gaps.o src/agent/credential_pool_sync.o src/agent/credential_sources.o src/agent/curator_ports.o src/agent/delegation_context.o src/agent/delegation_context_gaps3.o src/agent/empty_response_guard.o src/agent/error_surface.o src/agent/fast_mode_gaps3.o src/agent/gemini_native_helpers2.o src/agent/gemini_schema.o src/agent/i18n_languages.o src/agent/i18n_ports.o src/agent/image_gen_provider_py_final.o src/agent/image_routing_ports.o src/agent/init_ports.o src/agent/intent_ack.o src/agent/lsp/lsp_cli_ports.o src/agent/lsp/lsp_client.o src/agent/lsp/lsp_client_gaps.o src/agent/lsp/lsp_client_ports.o src/agent/lsp/lsp_eventlog.o src/agent/lsp/lsp_eventlog_ports.o src/agent/lsp/lsp_init_ports.o src/agent/lsp/lsp_install_ports.o src/agent/lsp/lsp_manager.o src/agent/lsp/lsp_manager_ports.o src/agent/lsp/lsp_protocol.o src/agent/lsp/lsp_range_shift.o src/agent/lsp/lsp_servers.o src/agent/models_dev_ports.o src/agent/monitoring/cron_health.o src/agent/monitoring/monitoring_events.o src/agent/monitoring/monitoring_policy.o src/agent/monitoring/policy.o src/agent/monitoring/redaction.o src/agent/moonshot_schema.o src/agent/moonshot_schema_ports.o src/agent/native_compaction.o src/agent/native_compaction_gaps.o src/agent/nous_rate_guard_ports.o src/agent/onboarding_ports.o src/agent/opencode_affinity_gaps.o src/agent/pet/generate/imagegen.o src/agent/pet/generate/orchestrate.o src/agent/pet/render_gaps3.o src/agent/plan_prompt_gaps3.o src/agent/plugin_llm_ports.o src/agent/plugin_stream_hooks.o src/agent/portal_tags_gaps.o src/agent/process_bootstrap_gaps.o src/agent/prompt_builder_gaps.o src/agent/prompt_builder_guidance.o src/agent/prompt_builder_ports.o src/agent/prompt_cache_boundary.o src/agent/prompt_cache_scope_gaps.o src/agent/prompt_caching_gaps.o src/agent/prompt_caching_plan.o src/agent/prompt_caching_strip.o src/agent/provider_projection_gaps3.o src/agent/proxy_sources/iron_proxy.o src/agent/reactions.o src/agent/reasoning_summaries.o src/agent/redact_gaps.o src/agent/redact_gaps3.o src/agent/relay_llm_gaps.o src/agent/relay_tools.o src/agent/retry_utils_ports.o src/agent/review_engine.o src/agent/review_engine_gaps3.o src/agent/review_idle_queue_gaps.o src/agent/runtime_cwd.o src/agent/runtime_cwd_gaps3.o src/agent/runtime_cwd_ports.o src/agent/runtime_helpers_ports.o src/agent/secret_sources/bitwarden_ports.o src/agent/secret_sources/onepassword_ports.o src/agent/secret_sources/secret_sources_base_ports.o src/agent/secret_sources/secret_sources_cache_ports.o src/agent/secret_sources/secret_sources_command_ports.o src/agent/secret_sources/secret_sources_registry_ports.o src/agent/secret_sources_helpers.o src/agent/shell_hooks_ports.o src/agent/side_question_gaps.o src/agent/skill_preprocessing_ports.o src/agent/skill_utils_gaps.o src/agent/skill_utils_wrappers.o src/cli_gaps.o src/cron/cron_locking.o src/cron/delivery_queue_gaps.o src/cron/executions.o src/cron/incidents_gaps.o src/cron/jobs_gaps.o src/cron/lifecycle_guard_gaps.o src/cron/monitor.o src/cron/port_jobs_pure.o src/cron/scheduler_preflight.o src/cron/scheduler_provider.o src/cron/scheduler_provider_ports.o src/cron/scheduler_toolsets.o src/cron/scripts/classify_items.o src/desktop_settings.o src/gateway/authz_mixin_gaps.o src/gateway/browser_control_artifacts.o src/gateway/turn_context.o src/gateway/turn_lease_ports.o src/gateway/whatsapp_identity_ports.o src/hermes_cli/_early_recovery_gaps.o src/hermes_cli/_install_repair_gaps.o src/hermes_cli/_scan_venv_blockers_gaps.o src/hermes_cli/_subprocess_compat_gaps.o src/hermes_cli/active_sessions_gaps.o src/hermes_cli/archive_safe_gaps.o src/hermes_cli/auth_commands_gaps.o src/hermes_cli/backup_gaps.o src/hermes_cli/billing_mixin_ports.o src/hermes_cli/cli_commands_mixin_gaps.o src/hermes_cli/cli_ports.o src/hermes_cli/clipboard.o src/hermes_cli/commands_gaps.o src/hermes_cli/commands_wrappers.o src/hermes_cli/config_gaps.o src/hermes_cli/copilot_auth_gaps.o src/hermes_cli/cron_gaps.o src/hermes_cli/dashboard_auth/cookies_gaps.o src/hermes_cli/dashboard_auth/dashboard_auth_public_paths.o src/hermes_cli/doctor_gaps.o src/hermes_cli/foreign_sessions_gaps.o src/hermes_cli/gateway_windows_gaps.o src/hermes_cli/helpers3.o src/hermes_cli/install_repair.o src/hermes_cli/inventory_gaps.o src/hermes_cli/journey_gaps.o src/hermes_cli/kanban_db_wrappers.o src/hermes_cli/kanban_transfer_gaps.o src/hermes_cli/linux_desktop_entry_gaps.o src/hermes_cli/local_runtime/binaries_gaps.o src/hermes_cli/local_runtime/bootstrap_gaps.o src/hermes_cli/local_runtime/catalog_gaps.o src/hermes_cli/local_runtime/context_policy_gaps.o src/hermes_cli/local_runtime/endpoint_gaps.o src/hermes_cli/local_runtime/estimator_gaps.o src/hermes_cli/local_runtime/gguf_gaps.o src/hermes_cli/local_runtime/growth_gaps.o src/hermes_cli/local_runtime/hardware_gaps.o src/hermes_cli/local_runtime/hf_browse_gaps.o src/hermes_cli/local_runtime/load_progress_gaps.o src/hermes_cli/local_runtime/presets_gaps.o src/hermes_cli/local_runtime/supervisor_gaps.o src/hermes_cli/macos_tcc_anchor_gaps.o src/hermes_cli/main_gaps.o src/hermes_cli/mcp_catalog_gaps.o src/hermes_cli/mcp_config_gaps.o src/hermes_cli/mcp_startup_gaps.o src/hermes_cli/mem_trim.o src/hermes_cli/migrate.o src/hermes_cli/moa_config_pure.o src/hermes_cli/moa_slash.o src/hermes_cli/model_setup_wrappers.o src/hermes_cli/model_switch_gaps.o src/hermes_cli/model_switch_wrappers.o src/hermes_cli/nous_account_wrappers.o src/hermes_cli/nous_auth_keepalive_gaps.o src/hermes_cli/nous_sub_wrappers.o src/hermes_cli/session_recovery_preflight.o src/hermes_cli/uninstall_wrappers.o src/hermes_cli/update_abort_recovery_gaps.o src/hermes_cli/update_lock.o src/hermes_cli/update_restart_recovery_gaps.o src/hermes_cli/web_routers/profiles_gaps.o src/hermes_cli/web_server_helpers2.o src/hermes_cli/windows_ssh_wrappers.o src/hermes_cli/windows_wrappers.o src/persistence/sqlite/_state/port_state_errors.o src/persistence/sqlite/_state/port_state_guard.o src/plugins/cron_providers/chronos/chronos_verify.o src/plugins/platforms/bluebubbles/bluebubbles_ports.o src/plugins/platforms/bluebubbles/bluebubbles_wrappers.o src/plugins/platforms/email/codex_app_server_client.o src/plugins/platforms/qqbot/api_server_adapter_handlers.o src/plugins/platforms/qqbot/chunked_upload.o src/plugins/platforms/qqbot/qqbot_adapter_ports.o src/plugins/platforms/qqbot/qqbot_constants.o src/plugins/platforms/qqbot/qqbot_keyboards_ports.o src/plugins/platforms/qqbot/qqbot_wrappers.o src/plugins/platforms/signal/signal_gaps.o src/plugins/platforms/signal/signal_helpers2.o src/plugins/platforms/signal/signal_ports.o src/plugins/platforms/signal/signal_rate_limit_ports.o src/plugins/platforms/wecom/wecom_callback.o src/plugins/platforms/weixin/vision_wrappers.o src/plugins/platforms/weixin/weixin_ports.o src/plugins/platforms/weixin/weixin_wrappers.o src/plugins/platforms/whatsapp_cloud/whatsapp_cloud.o src/plugins/platforms/yuanbao/api_server_adapter.o src/plugins/platforms/yuanbao/yuanbao_media.o src/plugins/platforms/yuanbao/yuanbao_media_ports.o src/plugins/platforms/yuanbao/yuanbao_ports.o src/plugins/platforms/yuanbao/yuanbao_proto_buf.o src/plugins/platforms/yuanbao/yuanbao_proto_encode.o src/plugins/platforms/yuanbao/yuanbao_wrappers.o src/tools/computer_use/computer_use_x11.o src/tools/computer_use/cua_backend_gaps.o src/tools/computer_use/cua_backend_methods.o src/tools/computer_use/cua_doctor.o src/tools/computer_use_windows.o src/tools/cronjob_tools_gaps.o src/tools/cua_backend_wrappers.o src/tools/debug_helpers_ports.o src/tools/delegate_tool_gaps.o src/tools/drive_preview_tool.o src/tools/env_local_wrappers.o src/tools/env_probe.o src/tools/environments/env_base_wrappers.o src/tools/environments/env_docker_wrappers.o src/tools/environments/env_ssh_ports.o src/tools/environments/local_gaps.o src/tools/environments/managed_modal.o src/tools/environments/modal.o src/tools/environments/vercel_sandbox_env.o src/tools/environments/vercel_sandbox_helpers.o src/tools/feishu_comment_rules.o src/tools/feishu_doc_tool_ports.o src/tools/feishu_drive_tool.o src/tools/feishu_drive_tool_ports.o src/tools/file.o src/tools/file_operations_search.o src/tools/file_state_ports.o src/tools/file_tools.o src/tools/file_tools_gaps.o src/tools/file_tools_helpers2.o src/tools/file_tools_wrappers.o src/tools/image_generation_tool_gaps.o src/tools/kanban_tools.o src/tools/kanban_tools_ports.o src/tools/lazy_deps_gaps.o src/tools/mcp_oauth_manager_ports.o src/tools/mcp_oauth_ports.o src/tools/mcp_oauth_wrappers.o src/tools/mcp_schema_cache.o src/tools/mcp_tool_gaps.o src/tools/moa_performance.o src/tools/online_research_ports.o src/tools/plugin_guard.o src/tools/tool_search_gaps.o src/tools/tour_tool.o src/tools/transcribe_helpers.o src/tools/transcription_tools_ports.o src/tools/transcription_wrappers.o src/tools/tts_tool_gaps.o src/tools/vercel_sandbox.o src/tools/vision.o src/tools/vision_analysis.o src/tools/vision_delegate.o src/tools/vision_helpers.o src/tools/vision_tools_gaps.o src/tools/web.o src/tools/web_base64_img.o src/tools/web_result_cache_gaps.o src/tools/web_tools.o src/tools/wecom_crypto.o src/trajectory_compressor.o src/tui_gateway/tui_gateway_compute_host.o src/utils/utils.o src/utils/utils_gaps.o src/hermes_cli/session_recovery_paths.o src/hermes_cli/session_recovery/session_recovery_verify.o src/hermes_cli/session_recovery_main.o src/hermes_cli/session_recovery_salvage.o src/acp_adapter/acp_adapter_entry.o src/acp_adapter/acp_adapter_server.o src/acp_adapter/acp_adapter_session.o src/acp_adapter/acp_adapter_tools.o src/agent/acp_openai_bridge_gaps.o src/agent/agent_gaps.o src/agent/agent_init.o src/agent/agent_message_payload.o src/agent/agent_runtime_helpers.o src/agent/agent_runtime_helpers_gaps.o src/agent/anthropic_credentials_gaps.o src/agent/async_utils.o src/agent/auxiliary_client_helpers2.o src/agent/auxiliary_client_runtime.o src/agent/azure_identity_adapter.o src/agent/background_review_gaps3.o src/agent/bedrock_adapter_gaps.o src/agent/browser_provider.o src/agent/chat_completion_helpers_gaps.o src/agent/checkpoints/checkpoint_manager_gaps.o src/agent/checkpoints/checkpoint_manager_ports.o src/agent/codex_event_projector.o src/agent/codex_headers_gaps.o src/agent/codex_responses_adapter_gaps.o src/agent/codex_runtime.o src/agent/conversation_loop_ports.o src/agent/lsp/lsp_servers_ports.o src/agent/memory_manager_gaps.o src/agent/memory_manager_helpers.o src/agent/memory_manager_ports.o src/agent/memory_manager_wrappers.o src/agent/memory_provider.o src/agent/memory_provider_gaps3.o src/agent/message_sanitization_ports.o src/agent/moa_loop_ports.o src/agent/moa_loop_wrappers.o src/agent/moa_trace_helpers.o src/agent/model_metadata_gaps.o src/agent/model_metadata_ports.o src/agent/ssl_guard.o src/agent/ssl_guard_ports.o src/agent/ssl_verify.o src/agent/stream_diag.o src/agent/stream_diag_ports.o src/agent/subdirectory_hints_ports.o src/agent/system_prompt_gaps.o src/agent/system_prompt_ports.o src/agent/think_scrubber.o src/agent/think_scrubber_helpers2.o src/agent/thinking_timeout_guidance.o src/agent/thread_scoped_output_ports.o src/agent/title.o src/agent/title_generator_ports.o src/agent/tool_dispatch_helpers_gaps.o src/agent/tool_executor_gaps.o src/agent/tool_guardrails_gaps.o src/agent/tool_guardrails_ports.o src/agent/trajectory_ports.o src/agent/transcript_repair_gaps.o src/agent/transcription_provider_ports.o src/agent/transports/codex_gaps.o src/agent/transports/transports_anthropic_ports.o src/agent/transports/transports_base_ports.o src/agent/transports/transports_bedrock_ports.o src/agent/transports/transports_codex_ports.o src/agent/transports/transports_init_ports.o src/agent/transports/transports_types_ports.o src/agent/tts_provider_ports.o src/agent/tts_registry_ports.o src/agent/turn_finalizer_gaps.o src/agent/turn_liveness_gaps.o src/agent/usage_pricing_ports.o src/agent/verif_evidence_wrappers.o src/agent/verify_hooks.o src/agent/video_gen_registry_ports.o src/agent/web_deps.o src/agent/web_search_registry_ports.o src/cli/port_agent_runtime_helpers_flow.o src/cli/port_gateway_agent_flow.o src/cli/port_gateway_windows_flow.o src/cli/port_tui_dispatch_flow.o src/cli/port_update_lock_flow.o src/gateway/cgroup_cleanup.o src/gateway/code_skew_ports.o src/gateway/config_helpers2.o src/gateway/cwd_placeholder.o src/gateway/delivery_ledger.o src/gateway/drain_control.o src/gateway/gateway_escape.o src/gateway/gateway_status_paths.o src/gateway/hosted_room_execution_policy_gaps.o src/gateway/hosted_room_links_gaps.o src/gateway/hosted_room_peer_gaps.o src/gateway/hosted_room_policy_checkpoint_gaps.o src/gateway/hosted_room_replicas_gaps.o src/gateway/hosted_rooms_gaps2.o src/gateway/hosted_rooms_gaps3.o src/gateway/kanban_watchers.o src/gateway/kanban_watchers_gaps.o src/gateway/media_policy.o src/gateway/media_repair_gaps.o src/gateway/memory_monitor.o src/gateway/message_timestamps_ports.o src/gateway/outbound/send_message_tool_gaps.o src/gateway/pairing_util.o src/gateway/platforms/api_server_gaps.o src/gateway/platforms/api_server_run_idempotency_gaps.o src/gateway/platforms/api_server_runs_gaps.o src/gateway/platforms/base_gaps.o src/gateway/platforms/msgraph_webhook_ports.o src/gateway/platforms/platforms_base_helpers2.o src/gateway/platforms/platforms_base_helpers3.o src/gateway/platforms/platforms_base_media.o src/gateway/platforms/platforms_helpers_ports.o src/gateway/platforms/webhook_filters.o src/gateway/platforms/webhook_ports.o src/gateway/profile_routing.o src/gateway/readiness.o src/gateway/relay/adapter.o src/gateway/relay/relay_adapter.o src/gateway/relay/relay_adapter_flow.o src/gateway/relay/relay_adapter_helpers.o src/gateway/relay/relay_adapter_ops.o src/gateway/relay/relay_adapter_ports.o src/gateway/relay/relay_auth.o src/gateway/relay/relay_auth_helpers2.o src/gateway/relay/relay_descriptor.o src/gateway/relay/relay_init_ports.o src/gateway/relay/relay_media.o src/gateway/relay/relay_transport.o src/gateway/relay/relay_transport_ports.o src/gateway/relay/relay_ws_transport.o src/gateway/relay/relay_ws_transport_ports.o src/gateway/relay/relay_ws_transport_py_parity.o src/gateway/relay/ws_transport.o src/gateway/restart.o src/gateway/restart_gaps.o src/gateway/rich_sent_store.o src/gateway/run_helpers.o src/gateway/run_ports.o src/gateway/session_context_ports.o src/gateway/session_helpers2.o src/gateway/session_ports.o src/gateway/session_stall.o src/gateway/shutdown_flush_ports.o src/gateway/shutdown_watchdog.o src/gateway/shutdown_watchdog_ports.o src/gateway/slash_command_guards.o src/gateway/slash_commands_gaps.o src/gateway/status_gaps.o src/gateway/status_ports.o src/gateway/status_wrappers.o src/gateway/sticker_cache.o src/gateway/stream_consumer_gaps.o src/gateway/stream_dispatch_ports.o src/gateway/streaming_tts_consumer.o src/gateway/systemd_notify_ports.o src/hermes_cli/npm_engine.o src/hermes_cli/observability/relay_shared_metrics_flow.o src/hermes_cli/observability/shared_metrics_sender_gaps.o src/hermes_cli/plugin_dev_gaps.o src/hermes_cli/plugins_ports.o src/hermes_cli/profiles_gaps.o src/hermes_cli/proxy/sse_done_gaps.o src/hermes_cli/runtime_provider_gaps.o src/hermes_cli/scan_venv_blockers.o src/hermes_cli/session_export_md_wrappers.o src/hermes_cli/session_recovery_copy.o src/hermes_cli/session_recovery_gaps.o src/hermes_cli/session_recovery_meta.o src/hermes_cli/session_recovery_orphans.o src/hermes_cli/sizefmt.o src/hermes_cli/sqlite_safe_read.o src/hermes_cli/subcommands/peer.o src/hermes_cli/tools_config_gaps.o src/hermes_cli/tools_config_more.o src/plugins/teams_pipeline/microsoft_graph_client_ports.o src/tools/approval_gaps.o src/tools/approval_wrappers.o src/tools/audio_container.o src/tools/binary_extensions.o src/tools/bot_relay_gaps.o src/tools/browser_lightpanda_gaps.o src/tools/browser_supervisor_redact.o src/tools/browser_tool_gaps.o src/tools/browser_use_cli_gaps.o src/tools/clarify_gateway_gaps.o src/tools/clarify_tool.o src/tools/close_preview_tool.o src/tools/close_terminal_tool.o src/tools/code_execution_tool.o src/tools/code_execution_tool_gaps.o src/tools/code_kernel_gaps.o src/tools/computer_use/computer_use_backend.o src/tools/computer_use/computer_use_doctor_mcp.o src/tools/computer_use/computer_use_noop.o src/tools/computer_use/computer_use_schema.o src/tools/computer_use/computer_use_wayland.o src/tools/process.o src/tools/read_extract_gaps.o src/tools/read_preview_tool.o src/tools/read_window_tool.o src/tools/registry_cache.o src/tools/research_helpers.o src/tools/self_repo_guard.o src/tools/session_search_tool.o src/tools/session_search_tool_gaps.o src/tools/shell_heredoc.o src/tools/skill_ledger_gaps.o src/tools/skill_prereqs.o src/tools/skill_usage_wrappers.o src/tools/skillevaluator_scan.o src/tools/skills_guard_gaps.o src/tools/skills_hub_gaps.o src/tools/skills_sync_ports.o src/tools/skills_tool_ports.o src/tools/subagent_worktree.o src/tools/terminal_scope_gaps.o src/tools/terminal_tool_gaps.o src/tools/terminal_tool_wrappers.o src/tools/todo.o src/tools/tool_backend_helpers.o src/agent/context_compressor_flow2.o src/agent/conversation_loop_flow.o src/agent/learning_graph_flow.o src/agent/prompt_cache_clock_flow.o src/agent/tool_executor_flow.o src/cli/cli_py_flow.o src/cli/cli_tui_mixin_flow.o src/cli/config_backend_flow.o src/cli/config_flow2.o src/cli/gateway_agent_cli_flow.o src/cli/gateway_cli_flow.o src/cli/gateway_windows_flow2.o src/cli/left_core_migration_flow.o src/cli/main_cli_flow2.o src/cli/models_flow2.o src/cli/update_lock_flow2.o src/cli/web_git_flow.o src/gateway/gateway_run_flow2.o src/gateway/gateway_session_flow.o src/gateway/platforms/buzz_adapter_flow.o src/gateway/platforms/discord_adapter_flow.o src/gateway/platforms/feishu_adapter_flow.o src/gateway/platforms/google_chat_adapter_flow.o src/gateway/platforms/matrix_adapter_flow.o src/gateway/platforms/photon_adapter_flow.o src/gateway/platforms/slack_adapter_flow.o src/gateway/platforms/telegram_adapter_flow.o src/gateway/platforms/whatsapp_adapter_flow.o src/gateway/run_inbound_flow.o src/gateway/run_notifications_flow.o src/gateway/run_shutdown_flow2.o src/gateway/session_authority_flow.o src/gateway/session_controls_flow.o src/gateway/tui_methods_flow.o src/gateway/tui_methods_tools_flow.o src/gateway/tui_server_flow.o src/gateway/turn_progress_flow.o src/hermes_cli/kanban_dashboard_flow.o src/hermes_cli/pm_packages_flow.o src/persistence/conversation_messages_flow.o src/persistence/conversation_sessions_flow.o src/persistence/sqlite_sessions_flow.o src/tools/browser_launcher_flow2.o src/tools/environments/docker_env_flow.o src/tools/kanban_tools_flow.o src/tools/terminal_tool_flow2.o src/cli/port_gw_session_store_flow.o src/cli/port_runtime_provider_flow.o src/cli/port_sessions_cmd_flow.o src/agent/display_flow.o src/gateway/hosted_room_driver_flow2.o src/cli/setup_flow2.o src/agent/agent_runtime_helpers_flow2.o src/cli/relay_shared_metrics_flow2.o src/gateway/platforms/platforms_base_flow2.o src/agent/conversation_compression_flow2.o src/cli/profiles_flow.o src/gateway/scale_to_zero_flow.o src/tools/code_execution_flow.o src/agent/transports/codex_transport_flow.o src/tools/vision_analysis_flow.o src/agent/auxiliary_client_flow2.o src/gateway/hosted_room_driver_flow.o src/agent/transports/anthropic_transport_flow.o src/tools/file_operations_flow.o src/agent/prompt_caching_flow.o src/cli/auth_flow.o src/cli/gateway_migrate_flow2.o src/tools/web_tools_flow.o src/tools/browser_tools_flow.o src/gateway/session_policy_flow2.o src/agent/moa_loop_flow2.o src/cli/curator_flow.o src/tools/skills_guard_flow.o src/gateway/platforms/openai_routes_flow2.o src/agent/anthropic_convert_flow2.o src/gateway/run_notifications_flow2.o src/cli/config_migrations_flow2.o src/agent/context_references_flow.o src/tools/sandbox_flow.o src/gateway/status_phrases_flow.o src/tools/terminal_hints_flow.o src/cli/model_metadata_flow.o src/gateway/run_startup_flow2.o src/cli/skill_usage_flow.o src/cli/update_cmd_flow.o src/agent/transports/openai_transport_flow.o src/tools/transcription_flow.o src/cli/models_detect_flow.o src/tools/lsp_tools_flow.o src/agent/tool_dispatch_flow.o src/cli/plugins_cmd_flow.o src/cli/web_server_flow.o src/cli/kanban_adapter_flow.o src/gateway/platforms/platform_registry_flow.o src/cli/relay_metrics_flow.o src/tools/mcp_oauth_flow2.o src/agent/scheduler_flow2.o src/cli/main_desktop_flow2.o src/gateway/tui_methods_sessions_flow.o src/gateway/platforms/signal_adapter_flow.o src/agent/transports/gemini_transport_flow.o src/gateway/relay_protocol_flow.o src/gateway/tui_methods_kanban_flow.o src/gateway/platforms/slack_adapter_flow2.o src/persistence/conversation_store_flow.o src/gateway/platforms/feishu_adapter_flow2.o src/cli/kanban_db_flow2.o src/gateway/run_inbound_flow3.o src/gateway/platforms/telegram_adapter_flow2.o src/gateway/tui_methods_config_flow.o src/persistence/sqlite_messages_flow.o src/gateway/platforms/irc_adapter_flow.o src/agent/tool_result_processing_flow.o src/cli/gateway_inbound_flow2.o src/gateway/tui_methods_settings_flow.o src/gateway/session_kanban_flow2.o src/persistence/store_rules_flow.o src/agent/error_classifier_flow.o src/cli/console_engine_flow.o src/cli/goals_flow.o src/cli/projects_db_flow.o src/gateway/platforms/buzz_adapter_flow2.o src/gateway/platforms/photon_adapter_flow2.o src/gateway/tui_methods_audit_flow.o src/agent/account_usage_flow2.o src/tools/image_tools_flow.o src/cli/doctor_flow.o src/agent/usage_tracking_flow.o src/cli/shared_metrics_flow.o src/agent/conversation_loop_flow2.o src/persistence/store_identity_flow.o src/gateway/tui_methods_goals_flow.o src/cli/main_cli_flow3.o src/gateway/gateway_run_flow3.o src/cli/kanban_cli_flow.o src/cli/config_backend_flow2.o src/gateway/tui_methods_projects_flow.o src/agent/transports/chat_completions_flow2.o src/gateway/platforms/google_chat_adapter_flow2.o src/cli/route_identity_flow.o src/gateway/platforms/matrix_adapter_flow2.o src/gateway/platforms/whatsapp_adapter_flow2.o src/cli/gateway_agent_flow2.o src/cli/cli_tui_mixin_flow2.o src/gateway/platforms/platforms_base_flow3.o src/cli/config_env_flow2.o src/gateway/session_context_flow2.o src/cli/observability_metrics_flow.o src/cli/config_defaults_flow.o src/agent/context_budget_flow.o src/cli/plugins_cmd_flow2.o src/gateway/tui_methods_cron_flow.o src/agent/moa_loop_flow3.o src/gateway/session_controls_flow2.o src/agent/transports/transport_types_flow2.o src/cli/gateway_cli_flow2.o src/cli/relay_metrics_flow2.o src/gateway/run_notifications_flow3.o src/cli/kanban_db_dispatch_flow2.o src/gateway/session_finite_flow2.o src/cli/process_identity_flow2.o src/agent/transports/bedrock_transport_flow2.o src/gateway/scale_to_zero_flow2.o src/cli/update_cmd_flow2.o src/cli/cli_output_flow.o src/cli/gateway_enroll_flow.o src/cli/config_migration_flow2.o src/gateway/tui_methods_status_flow.o src/agent/i18n_flow2.o src/agent/account_usage_nous_flow2.o

CUPBOARD_PORT_GATEWAY = src/gateway/run_gaps.o src/gateway/hosted_room_driver_gaps.o src/gateway/hosted_room_discussion_gaps.o src/gateway/authz_mixin.o src/gateway/wake.o src/gateway/turn_lease.o src/gateway/systemd_notify.o src/gateway/hooks.o src/gateway/platform_registry.o src/gateway/response_filters.o src/gateway/platforms/signal_na.o src/gateway/session.o src/gateway/stream_consumer.o src/gateway/session_context.o src/plugins/platforms/telegram/base_ext.o src/gateway/platforms/base_ext2.o src/gateway/gateway_lifecycle.o src/gateway/mirror.o src/gateway/platform_registry_ports.o src/gateway/platforms/msgraph_webhook_security.o src/plugins/platforms/qqbot/qqbot_chunked_upload_ports.o src/gateway/slash_access.o src/plugins/platforms/bluebubbles/bluebubbles.o src/plugins/platforms/dingtalk/dingtalk.o src/gateway/platforms/email.o src/plugins/platforms/feishu/feishu.o src/gateway/platforms/base_adapter.o src/gateway/run_pure2.o src/gateway/run.o src/gateway/status.o src/gateway/platforms/homeassistant.o src/gateway/platforms/matrix.o src/gateway/platforms/msgraph_webhook.o src/plugins/platforms/qqbot/qqbot.o src/gateway/hermes_constants_reasoning.o src/plugins/platforms/signal/signal.o src/gateway/platforms/slack.o src/gateway/telegram_filter.o src/plugins/platforms/wecom/wecom.o src/plugins/platforms/weixin/weixin.o src/plugins/platforms/yuanbao/yuanbao.o src/gateway/gw_setup.o src/gateway/gw_session.o src/gateway/config.o src/gateway/gateway_runner.o src/gateway/run_agent.o src/plugins/platforms/bluebubbles/run_pure.o src/gateway/gateway_runtime.o src/gateway/gw_logging.o src/gateway/run_deps.o src/gateway/gw_notifier.o src/gateway/gw_pollers.o src/gateway/platforms/discord.o src/gateway/server.o src/gateway/platforms/helpers.o src/gateway/gw_approval.o src/gateway/gw_thread.o src/gateway/gw_misc.o src/gateway/gw_format.o src/gateway/platforms/mattermost.o src/plugins/platforms/sms/sms.o src/plugins/platforms/telegram/telegram.o src/gateway/platforms/whatsapp.o src/agent/gw_dispatch.o src/gateway/platforms/webhook.o src/agent/relay_runtime.o src/agent/relay_llm.o src/gateway/api_server.o src/gateway/base.o src/gateway/signal.o src/gateway/signal_rate_limit.o src/gateway/webhook.o src/hermes_cli/observability/relay_shared_metrics.o src/gateway/relay/command_manifest.o src/gateway/channel_directory_gaps3.o src/gateway/config_gaps3.o src/gateway/control_socket_gaps3.o src/gateway/delivery_ledger_gaps3.o src/gateway/drain_control_gaps3.o src/gateway/lifecycle_ledger_gaps3.o src/gateway/pairing_gaps3.o src/gateway/platform_registry_gaps3.o src/gateway/platforms/api_server_room_grants_gaps3.o src/gateway/platforms/weixin_gaps3.o src/gateway/platforms/yuanbao_gaps3.o src/gateway/profile_routing_gaps3.o src/gateway/readiness_gaps3.o src/gateway/relay/adapter_gaps3.o src/gateway/relay/ws_transport_gaps3.o src/gateway/port_hosted_room_execution_policy.o src/gateway/run_gaps3.o src/gateway/scale_to_zero_gaps3.o src/gateway/session_gaps3.o src/gateway/session_db_recovery_gaps3.o src/gateway/shutdown_flush_gaps3.o src/gateway/shutdown_watchdog_gaps3.o src/gateway/wake_gaps3.o src/gateway/port_status.o src/gateway/port_inline_source.o

CUPBOARD_PORT_TOOLS = src/tools/file_operations_gaps.o src/cron/blueprints.o \
 src/tools/browser_cdp_tool.o src/tools/clarify_gateway.o \
 src/tools/computer_use/tool.o src/tools/computer_use/permissions.o \
 src/tools/computer_use/doctor.o src/tools/computer_use/backend.o \
 src/tools/computer_use/computer_use_vision_routing.o \
 src/tools/credential_files.o src/tools/debug_helpers.o \
 src/tools/env_passthrough.o src/tools/fal_common.o \
 src/tools/feishu_doc_tool.o src/tools/mixture_of_agents_tool.o \
 src/tools/openrouter_client.o src/tools/osv_check.o \
 src/tools/read_terminal_tool.o src/tools/schema_sanitizer.o \
 src/tools/skill_ledger.o src/tools/skills_ast_audit.o \
 src/tools/slash_confirm.o \
 src/tools/todo_tool.o src/tools/tool_result_storage.o \
 src/tools/fuzzy_match.o src/tools/video_generation_tool.o \
 src/tools/website_policy.o src/tools/memory_tool.o src/skills_hub.o \
 src/tools/skills_hub_ports.o src/tools/skill_manager_tool_ports.o \
 src/tools/skill_usage_ports.o src/tools/todo_tool_ports.o \
 src/tools/mcp_tool_ports.o src/tools/send_message_tool.o \
 src/tools/tts_tool.o src/tools/tts_tool_ports.o \
 src/tools/approval_ports.o src/tools/approval_helpers2.o \
 src/tools/computer_use/cua_backend_ports.o src/tools/voice_mode_ports.o \
 src/tools/environments/environments_base_ports.o src/tools/environments/environments_modal_ports.o \
 src/tools/environments/environments_docker_ports.o \
 src/tools/browser_supervisor_ports.o src/tools/tool_search_ports.o \
 src/tools/moa_performance_ports.o src/tools/tts_streaming_ports.o \
 src/tools/delegation_live_log_ports.o src/tools/memory_tool_ports.o \
 src/tools/computer_use/computer_use_tool_ports.o \
 src/tools/delegate_tool.o src/tools/file_operations.o \
 src/tools/file_operations_ports.o src/tools/image_generation_tool.o \
 src/tools/environments.o src/tools/skills.o src/tools/url_safety.o \
 src/tools/environments/daytona.o src/tools/online_research.o \
 src/tools/terminal.o src/tools/cron_prompt_sanitize.o \
 src/tools/cronjob_tools.o src/tools/mixture_of_agents.o \
 src/plugins/platforms/whatsapp_cloud/checkpoint_manager.o src/tools/voice_mode_pure.o \
 src/tools/tool_config.o src/tools/util_helpers.o src/tools/working_diff.o \
 src/tools/async_delegation_wrappers.o src/tools/path_security.o \
 src/tools/mcp_tool.o src/tools/image_gen.o src/tools/send_message.o \
 src/tools/skills_tool.o src/tools/voice_mode.o \
 src/tools/browser_tool_cdp.o src/tools/browser_tool_install.o \
 src/tools/browser_use_cli.o src/tools/image_gen_registry.o \
 src/tools/tts.o src/tools/video_gen_registry.o \
 src/tools/web_search_registry.o src/tools/browser_tool_platform.o \
 src/tools/clarify.o src/tools/memory_storage.o src/tools/tool_result.o \
 src/tools/tool_idempotency.o src/tools/browser_tool_env.o \
 src/tools/browser_tool_path.o src/tools/memory.o src/tools/registry.o \
 src/tools/tool_result_classification.o src/tools/approval.o \
 src/tools/async_delegation.o src/tools/browser_tool_helpers.o \
 src/tools/browser.o src/tools/delegate.o src/tools/process_registry.o \
 src/tools/session_crud.o src/tools/session_search.o \
 src/tools/terminal_env_registry.o src/plugins/platforms/weixin/tirith.o \
 src/tools/wake_word.o src/tools/lazy_deps.o \
 src/tools/browser_camofox_state.o src/tools/browser_supervisor.o \
 src/tools/computer_use/browser_route.o src/tools/port_environments/base.o \
 src/tools/port_browser_tool.o src/tools/binary_extensions_gaps3.o \
 src/tools/computer_use/doctor_gaps3.o src/tools/credential_files_gaps3.o \
 src/tools/delegation_output_schema_gaps3.o src/tools/desktop_ui_gaps3.o \
 src/tools/env_probe_gaps3.o src/tools/environments/docker_gaps3.o \
 src/tools/file_state_gaps3.o src/tools/fuzzy_match_gaps3.o \
 src/tools/image_source_gaps3.o src/tools/interpreter_shutdown_gaps3.o \
 src/tools/interrupt_gaps3.o src/tools/kanban_tools_gaps3.o \
 src/tools/mcp_death_supervisor_gaps3.o src/tools/preview_tool_gaps3.o \
 src/tools/project_tools_gaps3.o src/tools/react_to_message_tool_gaps3.o \
 src/tools/self_repo_guard_gaps3.o src/tools/skill_manager_tool_gaps3.o \
 src/tools/skills_sync_gaps3.o src/tools/tip_tool_gaps3.o \
 src/tools/todo_tool_gaps3.o src/tools/tool_backend_helpers_gaps3.o \
 src/tools/tour_tool_gaps3.o src/tools/transcription_tools_gaps3.o \
 src/tools/video_generation_tool_gaps3.o \
 src/tools/voice_client_config_gaps3.o src/tools/web_tools_gaps3.o

CUPBOARD_PORT_CRON = src/cron/scheduler_gaps.o src/cron/scheduler.o \
 src/cron/cron_suggestions.o src/cron/scheduler_prompt.o \
 src/cron/scheduler_script.o src/cron/cron_cli.o src/cron/notepad.o \
 src/cron/scheduler_ports.o src/cron/scheduler_runtime_impl.o \
 src/cron/scheduler_helpers.o src/cron/scheduler_delivery.o \
 src/cron/cron_extras.o src/cron/lifecycle_guard.o src/cron/cron_sqlite.o \
 src/cron/scheduler_runtime.o src/cron/jobs.o src/cron/executions_gaps3.o \
 src/cron/notepad_gaps3.o src/cron/scheduler_provider_gaps3.o \
 src/cron/suggestions_gaps3.o

CUPBOARD_PORT_PLUGINS = src/plugins/plugin_google_meet.o \
 src/plugins/web/port_web_common.o src/plugins/plugin_honcho.o

CUPBOARD_PORT_MISC = src/skills/skills_parser.o src/sandbox_escape.o \
 src/pet/pet_manifest.o src/agent/pet/pet_state.o src/hermes_cli/pty.o \
 src/web_dashboard.o src/pet/pet_commands.o src/agent/pet/pet_store.o \
 src/agent/pet/generate/pet_prompts.o src/agent/pet/pet_constants.o \
 src/acp/edit_approval.o src/file_ops.o src/secrets.o \
 src/hermes_tokenizer.o src/hermes_cli/xai_retirement.o src/hermes_env_keys.o \
 lib/libpngdec/pngdec.o lib/libwubuoffice/src/wubuzip/inflate.o \
 lib/libwubuoffice/src/wubuzip/huffman.o \
 lib/libwubuoffice/src/wubuzip/crc.o lib/libwubuoffice/src/wubuzip/bit.o \
 lib/libwubuoffice/src/wubuzip/block.o \
 lib/libwubuoffice/src/wubuzip/fixed.o \
 lib/libwubuoffice/src/wubuzip/canon.o \
 lib/libwubuoffice/src/wubuzip/fixedcode.o \
 lib/libwubuoffice/src/wubuzip/limitcode.o src/provider/token_exchange.o \
 src/hermes_logging.o src/infrastructure_stubs.o src/pm/port_pm_environments.o


# ── Object File Lists ──────────────────────────────────────────────
# All source file objects organized by subsystem
# Included by top-level Makefile



# PHASE5_OBJ: the slermes/tui/static links consume this (targets.mk). It was
# referenced but NEVER defined - empty for the whole CLI_OBJ era, which is why
# the entire interactive-CLI family (src/cli/* + the real cli_main) never
# linked and the binary ran a hollow-stub cli_main. Compose it from the
# subsystem object lists, matching the documented wiring convention.
PHASE5_OBJ = $(CLI_OBJ)
# Auto-generated hermes_cli/ port objects (empty — placeholder)
HERMES_CLI_PORT_OBJ = src/hermes_cli/service_manager.o src/hermes_cli/agent_plugins.o \
 src/hermes_cli/process_identity.o \
 src/hermes_cli/update_receipt.o \
 src/hermes_cli/update_inventory.o \
 src/hermes_cli/worktree_gc.o \
 src/hermes_cli/worktree_cmd.o \
 src/hermes_cli/relay_plugin_cutover.o \
 src/tools/browser_extension_router.o src/tools/xai_video_tools.o src/tools/x_search_tool.o src/tools/tts_streaming.o src/tools/tirith_security.o src/tools/thread_context.o src/tools/project_tools.o src/tools/open_preview_tool.o src/tools/mcp_stdio_watchdog.o src/tools/mcp_dashboard_oauth.o src/tools/image_source.o src/tools/hook_output_spill.o src/tools/homeassistant_tool.o src/tools/focus_pane_tool.o src/tools/file_state.o src/tools/desktop_ui.o src/tools/delegation_live_log.o src/tools/daemon_pool.o \
 src/tools/memory_entries.o \
 src/gateway/gw_encode.o \

HERMES_CLI_PORT_EXTRA_OBJ = \
 src/hermes_cli/tips.o \
 src/hermes_cli/subcommands/subcommands_parsers.o \

# ── Implemented but previously unbuilt modules ─────────────────────
# Exist as source and are referenced by oracle harnesses, but were in no
# build variable, so the linked binary did not contain them and every
# harness touching them failed to link. input_sanitize.c is excluded: it
# duplicates the already-linked .
CUPBOARD_UNBUILT_MODS = \
 lib/libwubuoffice/src/wubuoxml/docx_text.o \
 lib/libwubuoffice/src/wubuoxml/package.o \
 lib/libwubuoffice/src/wubuoxml/reader.o \
 lib/libwubuoffice/src/wubuoxml/rels_path.o \
 lib/libwubuoffice/src/wubuzip/bitw.o \
 lib/libwubuoffice/src/wubuzip/deflate.o \
 lib/libwubuoffice/src/wubuzip/lz77.o \
 lib/libwubuoffice/src/wubuzip/reader.o \
 lib/libwubuoffice/src/wubuzip/zip.o \
 src/hermes_cli/approvals_suggest_proposals.o \
 src/agent/backend_identity.o \
 src/agent/billing_links.o \
 src/agent/billing_pure.o \
 src/agent/coding_context.o \
 src/agent/command_token_source.o \
 src/agent/copilot_acp_client.o \
 src/agent/credential_persistence.o \
 src/agent/credential_pool_persistence.o \
 src/agent/monitoring/health.o \
 src/agent/message_sanitization_pure.o \
 src/agent/model_metadata.o \
 src/agent/monitoring/monitoring_emitter.o \
 src/agent/monitoring/monitoring_health_export.o \
 src/agent/monitoring/monitoring_health_export_pure.o \
 src/agent/monitoring/otlp_exporter.o \
 src/agent/plugin_prompt_sections.o \
 src/agent/process_bootstrap_pure.o \
 src/agent/prompt_cache_scope.o \
 src/agent/proxy_utils.o \
 src/agent/replay_cleanup.o \
 src/agent/retry_utils.o \
 src/agent/title_generator.o \
 src/agent/tool_executor.o \
 src/agent/turn_retry_state.o \
 src/cron/suggestion_catalog.o \
 src/gateway/dead_targets.o \
 src/gateway/disk_status.o \
 src/gateway/display_config.o \
 src/gateway/platforms/http_client_limits.o \
 src/gateway/platforms/platforms_helpers_md.o \
 src/plugins/platforms/yuanbao/yuanbao_md.o \
 src/gateway/restart_loop_guard.o \
 src/gateway/runtime_footer.o \
 src/gateway/session_state.o \
 src/hermes_cli/auth_helpers2.o \
 src/hermes_cli/status_pure.o \
 src/hermes_error.o \
 src/provider/google_oauth.o \
 \
 src/tools/apply_layout_tool.o \
 src/tools/bot_mode_dm.o \
 src/tools/bot_mode_probe.o \
 src/tools/bot_relay.o \
 src/tools/cua_backend_helpers.o \
 src/agent/curator_backup.o \
 src/tools/curator_tar.o \
 src/tools/file_fs_ops.o \
 src/tools/file_lint.o \
 src/tools/mcp_tool_pure.o \
 src/tools/memory_tool_pure.o \
 src/tools/patch_parser.o \
 src/tools/patch_parser_util.o \
 src/tools/read_extract.o \
 src/tools/setup_mcp_tool.o \
 src/tools/skill_linter.o \
 src/tools/spill_safety.o \
 src/tools/terminal_tool_ports.o \
 src/tools/tool_output_limits.o \
 src/tools/toolsets.o \
 src/tools/transcription_tools_pure.o \
 src/tools/url_safety_ssrf.o \
 src/tools/video_gen.o \
 src/tools/voice_client_config.o \
 src/tools/tts_text_normalize.o \
 src/agent/billing_usage.o \
 src/tools/file_ops_lint.o \
 src/tools/flux3_video_tool.o \
 src/tools/tool_search.o \
 src/tools/interrupt.o \
 src/tools/flux3_video.o \
 src/compat/net6_classify.o
