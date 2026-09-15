# ── Object File Lists ──────────────────────────────────────────────
# All source file objects organized by subsystem
# Included by top-level Makefile



CLI_OBJ = src/cli/cli_gaps_2026_08_25.o src/cli/cli.o src/cli/cli_helpers2.o src/cli/cli_helpers3.o src/cli/hermes_cli_agent_b2.o src/cli/hermes_cli_agent_b3.o src/cli/hermes_cli_agent_b4.o src/cli/hermes_cli_agent_b5.o src/hermes_cli/partial_compress.o src/hermes_cli/setup_pure.o src/hermes_cli/dashboard_procs.o src/hermes_cli/webhook.o src/hermes_cli/web_models.o src/hermes_cli/vercel_auth.o src/hermes_cli/urllib_security.o src/hermes_cli/telegram_managed_bot.o src/hermes_cli/suggestions_cmd.o src/hermes_cli/stdio.o src/hermes_cli/sqlite_runtime.o src/hermes_cli/slack_cli.o src/hermes_cli/skin_engine.o src/hermes_cli/skin_cmd.o src/hermes_cli/skills_hub.o src/hermes_cli/setup_whatsapp_cloud.o src/hermes_cli/setup_hidden_env.o src/hermes_cli/sessions_cmd.o src/hermes_cli/session_filters.o src/hermes_cli/session_export_html.o src/hermes_cli/send_cmd.o src/hermes_cli/security_audit_startup.o src/hermes_cli/secrets_cli.o src/hermes_cli/secret_prompt.o src/hermes_cli/route_identity.o src/hermes_cli/relaunch.o src/hermes_cli/pty_session.o src/hermes_cli/psutil_android.o src/hermes_cli/proxy_cli.o src/hermes_cli/providers.o src/hermes_cli/projects_cmd.o src/hermes_cli/profile_distribution.o src/hermes_cli/profile_describer.o src/hermes_cli/portal_cli.o src/hermes_cli/pets.o src/hermes_cli/pairing.o src/hermes_cli/onepassword_secrets_cli.o src/hermes_cli/nous_auth_keepalive.o src/hermes_cli/model_search.o src/hermes_cli/moa_cmd.o src/hermes_cli/memory_oauth.o src/hermes_cli/mcp_startup.o src/hermes_cli/mcp_picker.o src/hermes_cli/mcp_config.o src/hermes_cli/mcp_catalog.o src/hermes_cli/managed_scope.o src/hermes_cli/lifecycle.o src/hermes_cli/kanban_specify.o src/hermes_cli/journey.o src/hermes_cli/inventory.o src/hermes_cli/init_command.o src/hermes_cli/gui_uninstall.o src/hermes_cli/gateway_enroll.o src/hermes_cli/env_loader.o src/hermes_cli/dingtalk_auth.o src/hermes_cli/diagnostics_upload.o src/hermes_cli/dep_ensure.o src/hermes_cli/debug.o src/hermes_cli/dashboard_register.o src/hermes_cli/curator.o src/hermes_cli/cron.o src/hermes_cli/credential_lifecycle.o src/hermes_cli/copilot_auth.o src/hermes_cli/codex_runtime_switch.o src/hermes_cli/codex_runtime_plugin_migration.o src/hermes_cli/codex_models.o src/hermes_cli/cli_billing_mixin.o src/hermes_cli/cli_agent_setup_mixin.o src/hermes_cli/claw.o src/hermes_cli/checkpoints.o src/hermes_cli/bundles.o src/hermes_cli/browser_connect.o src/hermes_cli/banner.o src/hermes_cli/auth_commands.o src/hermes_cli/approval_mode.o src/hermes_cli/active_sessions.o src/hermes_cli/_subprocess_compat.o src/hermes_cli/_parser.o src/hermes_cli/_early_recovery.o src/hermes_cli/approval_transport.o src/cli/config.o src/hermes_cli/config_env.o src/hermes_cli/config_platforms.o src/hermes_cli/config_profile.o src/hermes_cli/config_diff.o src/hermes_cli/config_io.o src/hermes_cli/config_schema.o src/hermes_cli/config_migrate.o src/cli/config_setup.o src/hermes_cli/config_schema.o src/cli/paths.o src/hermes_cli/cli_output.o src/cli/display_core.o src/hermes_cli/main.o src/hermes_cli/doctor.o src/hermes_cli/setup.o src/hermes_cli/linux_desktop_entry.o src/hermes_cli/banner_pure.o src/hermes_cli/banner_boot.o src/hermes_cli/context_switch_guard.o src/hermes_cli/windows.o src/hermes_cli/nous_billing.o src/hermes_cli/nous_billing_ports.o src/hermes_cli/nous_subscription_ports.o src/hermes_cli/proxy_xai_ports.o src/hermes_cli/proxy_adapters_base_ports.o src/hermes_cli/proxy/server.o src/hermes_cli/proxy/cli.o src/hermes_cli/console_engine_ports.o src/hermes_cli/config_pure.o src/hermes_cli/status_helpers.o src/hermes_cli/logs.o src/hermes_cli/write_approval_commands.o src/hermes_cli/cli_cmd_help.o src/cli/cli_cmd_session.o src/cli/cli_cmd_config.o src/hermes_cli/cli_cmd_misc.o src/hermes_cli/subcommands/whatsapp.o src/hermes_cli/subcommands/webhook.o src/hermes_cli/subcommands/update.o src/hermes_cli/subcommands/uninstall.o src/hermes_cli/subcommands/tools.o src/hermes_cli/subcommands/sync.o src/hermes_cli/subcommands/status.o src/hermes_cli/subcommands/slack.o src/hermes_cli/subcommands/skin.o src/hermes_cli/subcommands/skills.o src/hermes_cli/subcommands/setup.o src/hermes_cli/subcommands/security.o src/hermes_cli/subcommands/prompt_size.o src/hermes_cli/subcommands/profile.o src/hermes_cli/subcommands/plugins.o src/hermes_cli/subcommands/pairing.o src/hermes_cli/subcommands/monitoring.o src/hermes_cli/subcommands/model.o src/hermes_cli/subcommands/memory.o src/hermes_cli/subcommands/mcp.o src/hermes_cli/subcommands/logs.o src/hermes_cli/subcommands/logout.o src/hermes_cli/subcommands/login.o src/hermes_cli/subcommands/insights.o src/hermes_cli/subcommands/import_cmd.o src/hermes_cli/subcommands/import_agent.o src/hermes_cli/subcommands/hooks.o src/hermes_cli/subcommands/gui.o src/hermes_cli/subcommands/gateway.o src/hermes_cli/subcommands/dump.o src/hermes_cli/subcommands/doctor.o src/hermes_cli/subcommands/debug.o src/hermes_cli/subcommands/dashboard.o src/hermes_cli/subcommands/cron.o src/hermes_cli/subcommands/console.o src/hermes_cli/subcommands/config.o src/hermes_cli/subcommands/claw.o src/hermes_cli/subcommands/backup.o src/hermes_cli/subcommands/auth.o src/hermes_cli/subcommands/approvals.o src/hermes_cli/subcommands/acp.o src/hermes_cli/subcommands/_shared.o src/cli/cli_cmd_gateway.o src/hermes_cli/cli_cmd_skills.o src/hermes_cli/cli_cmd_tools.o src/hermes_cli/cli_cmd_mcp.o src/hermes_cli/cli_cmd_kanban.o src/cli/cli_cmd_security.o src/cli/cli_cmd_memory.o src/hermes_cli/cli_cmd_display.o src/hermes_cli/cli_cmd_parity.o src/hermes_cli/terminal_breadcrumbs_pure.o src/hermes_cli/plugin_index_pure.o src/hermes_cli/plugin_capabilities_pure.o src/hermes_cli/session_lost_and_found_pure.o src/hermes_cli/foreign_sessions_pure.o src/hermes_cli/subprocess_compat_pure.o src/hermes_cli/plugin_packs_pure.o src/hermes_cli/plugins_cmd.o src/hermes_cli/plugins_gaps.o src/hermes_cli/stderr_timestamp_pure.o src/hermes_cli/kanban_db_pure.o src/hermes_cli/cli_kanban_goal_loop_q.o src/hermes_cli/goals_pure.o src/hermes_cli/loops_pure.o src/hermes_cli/loops_manager.o src/hermes_cli/console_engine_pure.o src/hermes_cli/model_cost_guard.o src/battery.o src/hermes_cli/profiles.o src/hermes_cli/web_server_status.o src/hermes_cli/web_server_managed_files.o src/hermes_cli/web_server_fs.o src/hermes_cli/web_server_events.o src/hermes_cli/web_server_console.o src/hermes_cli/web_server_themes.o src/hermes_cli/web_server_ws_auth.o src/hermes_cli/hermes_cli_ws_tickets.o src/hermes_cli/web_server_chat_argv.o src/hermes_cli/web_server_cron_dash.o src/hermes_cli/web_server_sessions_admin.o src/hermes_cli/web_server_session_detail.o src/hermes_cli/web_server_session_endpoints.o src/hermes_cli/web_server_prune.o \
src/hermes_cli/default_soul.o src/hermes_cli/gemini_cloudcode_adapter_methods.o src/agent/tts_provider.o src/agent/interrupt_compat.o src/agent/web_search_registry.o src/agent/video_gen_provider.o src/agent/trace_upload.o src/agent/tool_result_classification.o src/agent/thread_scoped_output.o src/agent/subscription_view.o src/agent/stream_single_writer.o src/agent/rate_limit_tracker.o src/agent/oneshot.o src/agent/moa_trace.o src/agent/kanban_stop.o src/agent/jiter_preload.o src/agent/credential_pool.o src/agent/bounded_response.o src/agent/account_usage.o src/hermes_cli/dashboard_auth_prefix.o src/hermes_cli/hooks.o src/hermes_cli/prompt_size.o src/hermes_cli/voice.o src/hermes_cli/display_diff.o src/hermes_cli/gitlock.o src/hermes_cli/model_data_policy_guard.o src/hermes_cli/model_selection_guards.o \
src/hermes_cli/config_defaults.o \
 src/hermes_cli/web_server_paths.o \
 src/gateway/platforms/msgraph_webhook/msgraph_webhook_helpers.o \
 src/hermes_cli/pairing_helpers.o \
 src/hermes_cli/webhook_helpers.o \
 src/hermes_cli/plugin_dev.o src/hermes_cli/platform_actions.o src/hermes_cli/verify_cmd.o src/cli/cli_hermes_cli_methods.o
# Auto-generated hermes_cli/ port objects (empty — placeholder)
HERMES_CLI_PORT_OBJ = src/hermes_cli/service_manager.o src/hermes_cli/agent_plugins.o \
 src/hermes_cli/process_identity.o \
 src/hermes_cli/update_receipt.o \
 src/hermes_cli/update_inventory.o \
 src/hermes_cli/worktree_gc.o \
 src/hermes_cli/worktree_cmd.o \
 src/hermes_cli/relay_plugin_cutover.o \
 src/tools/browser_extension_router.o src/tools/xai_video_tools.o src/tools/x_search_tool.o src/tools/tts_streaming.o src/tools/tirith_security.o src/tools/thread_context.o src/tools/project_tools.o src/tools/open_preview_tool.o src/tools/mcp_stdio_watchdog.o src/tools/mcp_dashboard_oauth.o src/tools/image_source.o src/tools/hook_output_spill.o src/tools/homeassistant_tool.o src/tools/focus_pane_tool.o src/tools/file_state.o src/tools/desktop_ui.o src/tools/delegation_live_log.o src/tools/daemon_pool.o \

HERMES_CLI_PORT_EXTRA_OBJ = \
 src/hermes_cli/tips.o \
 src/hermes_cli/subcommands_parsers.o \

PORT_OBJ = \
 src/hermes_cli/browser_connect_gaps.o \
 src/agent/auxiliary_client_gaps.o \
 \
 src/gateway/run_gaps.o \
 src/hermes_cli/web_server_gaps.o \
 src/hermes_cli/setup_gaps.o \
 src/tools/file_operations_gaps.o \
 src/gateway/hosted_room_driver_gaps.o \
 src/gateway/hosted_room_discussion_gaps.o \
 src/hermes_cli/web_routers/local_models_gaps.o \
 src/hermes_cli/update_cmd_gaps.o \
 src/cron/scheduler_gaps.o \
 src/hermes_cli/proxy_adapters_base.o \
 src/hermes_cli/proxy_adapters_xai.o \
 src/hermes_cli/proxy_adapters_nous_portal.o \
 src/hermes_cli/anthropic_adapter.o \
 src/hermes_cli/bedrock_adapter.o \
 src/hermes_cli/web_server_simple.o \
 \
 src/hermes_cli/context_compressor.o \
 \
 \
 \
 src/hermes_cli/display.o src/agent/display_ports.o \
 src/hermes_cli/display_tool_preview.o \
 src/agent/error_classifier.o \
 src/hermes_cli/oneshot.o \
 src/agent/google_oauth.o \
 \
 \
 \
 \
 \
 \
 \
 \
 \
 \
 src/agent/web_search_provider.o \
 src/hermes_cli/cli.o \
 src/gateway/authz_mixin.o src/gateway/wake.o src/gateway/turn_lease.o src/gateway/systemd_notify.o \
 src/hermes_cli/channel_directory.o \
 \
 \
 src/gateway/hooks.o \
 src/gateway/platform_registry.o \
 src/hermes_cli/platforms_feishu_meeting_invite.o \
 src/hermes_cli/platforms_helpers.o \
 src/hermes_cli/platforms_qqbot_crypto.o \
 src/hermes_cli/platforms_qqbot_keyboards.o \
 src/hermes_cli/platforms_qqbot_onboard.o \
 src/hermes_cli/platforms_qqbot_utils.o \
 src/hermes_cli/platforms_signal_rate_limit.o \
 src/hermes_cli/platforms_sms.o \
 src/hermes_cli/platforms_telegram_network.o \
 src/hermes_cli/platforms_wecom_callback.o \
 src/hermes_cli/platforms_wecom_crypto.o \
 src/hermes_cli/platforms_yuanbao_media.o \
 src/hermes_cli/platforms_yuanbao_sticker.o \
 src/gateway/response_filters.o \
 src/hermes_cli/scale_to_zero.o \
 src/hermes_cli/signal_format.o \
 src/hermes_cli/whatsapp_identity.o \
 src/hermes_cli/shutdown_forensics.o \
 \
 \
 src/hermes_cli/models.o \
 src/hermes_cli/model_catalog.o \
 src/hermes_cli/main_helpers.o \
 \
 src/hermes_cli/turn_summary_cli.o \
 src/hermes_cli/main_cmds.o \
 src/hermes_cli/approvals_suggest.o \
 src/hermes_cli/checkpoints_format.o \
 src/hermes_cli/backup.o \
 src/hermes_cli/pty_clamp_helpers.o \
 src/hermes_cli/gateway_platform.o \
 src/hermes_cli/kanban_helpers.o \
 src/hermes_cli/kanban_cmd_gaps.o \
 \
 src/hermes_cli/fallback_config.o \
 src/hermes_cli/timeouts.o \
 src/hermes_cli/session_listing.o \
 src/hermes_cli/project_tree.o \
 src/hermes_cli/models_helpers.o \
 src/hermes_cli/models_net.o src/hermes_cli/models_pure_impl.o \
 src/hermes_cli/models_pure.o \
 src/hermes_cli/models_validate.o \
 src/hermes_cli/models_gaps.o \
 src/hermes_cli/status_helpers.o \
 src/hermes_cli/shared_metrics.o \
 \
 src/hermes_cli/managed_scope_helpers.o \
 src/hermes_cli/context_breakdown_helpers.o \
 src/hermes_cli/verification_stop_helpers.o \
 src/hermes_cli/blueprint_catalog_helpers.o \
 src/hermes_cli/vertex_adapter_helpers.o \
 src/hermes_cli/drain_control_helpers.o \
 src/hermes_cli/platforms_base_helpers.o \
 \
 src/hermes_cli/gateway_command_sanitize.o \
 src/hermes_cli/blueprint_cmd.o \
 src/hermes_cli/goals_data.o \
 src/hermes_cli/goals_manager.o \
 src/hermes_cli/goals_helpers.o \
 src/hermes_cli/provider_meta.o \
 src/hermes_cli/completion.o src/hermes_cli/completion_ports.o \
 src/hermes_cli/scale_to_zero_helpers.o \
 src/hermes_cli/jobs_helpers.o \
 src/hermes_cli/profiles_helpers.o \
 src/hermes_cli/yuanbao_proto_helpers.o \
 src/hermes_cli/learning_graph_render_helpers.o \
 src/hermes_cli/lazy_deps_helpers.o \
 src/hermes_cli/model_switch_helpers.o \
 src/hermes_cli/learning_graph_helpers.o \
 src/hermes_cli/learning_graph.o \
 src/hermes_cli/learning_graph_render_helpers.o \
 src/hermes_cli/learning_graph_render.o \
 src/hermes_cli/learning_mutations.o \
 src/hermes_cli/file_state_helpers.o \
 src/hermes_cli/fuzzy_match_helpers.o \
 src/hermes_cli/file_tools_helpers.o \
 src/hermes_cli/cua_backend_helpers.o \
 src/hermes_cli/delegate_tool_helpers.o \
 src/hermes_cli/profiles_helpers.o \
 src/tools/blueprints.o \
 src/tools/browser_cdp_tool.o \
 src/hermes_cli/browser_camofox.o \
 src/hermes_cli/discord_helpers.o \
 src/hermes_cli/pet_render.o \
 src/hermes_cli/whatsapp_common_helpers.o \
 src/hermes_cli/whatsapp_cloud_helpers.o \
 src/hermes_cli/clipboard_helpers.o \
 src/hermes_cli/doctor_helpers.o \
 src/hermes_cli/code_execution_helpers.o \
 src/hermes_cli/code_execution_helpers.o \
 \
 src/tools/clarify_gateway.o \
 \
 src/tools/computer_use/computer_use_backend.o src/tools/computer_use/tool.o src/tools/computer_use/permissions.o src/tools/computer_use/doctor.o src/tools/computer_use/backend.o \
 src/tools/computer_use/computer_use_vision_routing.o \
 src/tools/credential_files.o \
 src/tools/debug_helpers.o \
 src/tools/env_passthrough.o \
 \
 src/hermes_cli/environments_daytona.o \
 src/hermes_cli/environments_file_sync.o \
 src/hermes_cli/environments_managed_modal.o \
 src/hermes_cli/environments_modal_utils.o \
 src/hermes_cli/environments_singularity.o \
 src/hermes_cli/environments_ssh.o \
 src/tools/fal_common.o \
 src/tools/feishu_doc_tool.o \
 src/hermes_cli/managed_tool_gateway.o \
 src/hermes_cli/mcp_oauth_manager.o \
 src/hermes_cli/microsoft_graph_auth.o \
 \
 src/tools/mixture_of_agents_tool.o \
 src/tools/openrouter_client.o \
 src/tools/osv_check.o \
 \
 \
 src/tools/read_terminal_tool.o \
 src/tools/schema_sanitizer.o \
 src/tools/skill_ledger.o \
 \
 src/tools/skills_ast_audit.o \
 src/hermes_cli/skills_guard.o \
 src/tools/slash_confirm.o \
 src/tools/threat_patterns.o \
 src/tools/todo_tool.o \
 \
 \
 src/tools/tool_result_storage.o \
 src/tools/fuzzy_match.o \
 \
 \
 src/tools/video_generation_tool.o \
 src/tools/website_policy.o \
 src/hermes_cli/write_approval.o \
 \
 src/skills/skills_parser.o \
 src/hermes_cli/auth.o \
 src/hermes_cli/auth_store.o \
 src/hermes_cli/mcp_security.o \
 src/hermes_cli/security_advisories.o \
 src/hermes_cli/kanban_db.o \
 \
 src/hermes_cli/web_server.o src/hermes_cli/web_routers/profiles.o src/hermes_cli/web_routers/cron.o src/hermes_cli/dashboard_auth/token_auth.o src/hermes_cli/dashboard_auth/routes.o src/hermes_cli/dashboard_auth/prefix.o src/hermes_cli/dashboard_auth/native_flow.o src/hermes_cli/dashboard_auth/middleware.o src/hermes_cli/dashboard_auth/login_page.o src/hermes_cli/dashboard_auth/cookies.o src/hermes_cli/dashboard_auth/base.o \
 src/gateway/platforms/signal_na.o \
 src/hermes_cli/yuanbao_tools.o \
 src/hermes_cli/memory_setup.o \
 src/hermes_cli/toolset_validation.o \
 src/tools/memory_tool.o \
 src/tools/skills_hub.o src/tools/skills_hub_ports.o src/tools/skill_manager_tool_ports.o src/tools/skill_usage_ports.o src/tools/todo_tool_ports.o \
 src/tools/mcp_tool_ports.o \
 src/tools/send_message_tool.o \
 src/tools/tts_tool.o src/tools/tts_tool_ports.o src/tools/approval_ports.o src/tools/approval_helpers2.o src/tools/cua_backend_ports.o src/tools/voice_mode_ports.o src/tools/environments_base_ports.o src/tools/environments_modal_ports.o src/tools/environments_docker_ports.o src/tools/browser_supervisor_ports.o src/tools/tool_search_ports.o src/tools/moa_performance_ports.o src/tools/tts_streaming_ports.o src/tools/delegation_live_log_ports.o src/tools/memory_tool_ports.o src/tools/computer_use/computer_use_tool_ports.o src/tools/delegate_tool.o \
 src/tools/file_operations.o src/tools/file_operations_ports.o \
 src/tools/image_generation_tool.o \
 src/tools/environments.o \
 src/gateway/session.o \
 src/gateway/stream_consumer.o \
 src/hermes_cli/channel_directory.o \
 src/gateway/session_context.o \
 \
 \
 src/hermes_cli/extra.o \
 src/hermes_cli/config.o \
 src/hermes_cli/container_boot.o \
 src/hermes_cli/context_switch_guard.o \
 src/hermes_cli/dump.o src/hermes_cli/dump_ports.o src/hermes_cli/journey_ports.o src/hermes_cli/kanban_decompose_ports.o src/hermes_cli/pairing_ports.o \
 src/hermes_cli/gateway.o \
 src/hermes_cli/windows.o \
 src/hermes_cli/goals.o src/hermes_cli/goals_ports.o src/hermes_cli/profile_distribution_ports.o src/hermes_cli/projects_db_ports.o src/hermes_cli/pty_bridge_ports.o src/hermes_cli/win_pty_bridge_ports.o src/hermes_cli/service_manager_ports.o src/hermes_cli/active_sessions_ports.o \
 \
 src/hermes_cli/kanban_schema.o \
 src/hermes_cli/kanban_model.o \
 src/hermes_cli/kanban_tasks.o \
 src/hermes_cli/kanban_lifecycle.o \
 src/hermes_cli/kanban_notify.o \
 src/hermes_cli/kanban_stats.o \
 src/hermes_cli/kanban_runs.o \
 src/hermes_cli/kanban_query.o \
 src/hermes_cli/kanban_util.o \
 src/hermes_cli/kanban_boards.o \
 src/hermes_cli/kanban_decompose.o \
 src/hermes_cli/kanban_workers.o \
 \
 src/hermes_cli/memory_providers.o \
 src/hermes_cli/dashboard_auth_audit.o \
 src/hermes_cli/dashboard_auth_registry.o \
 src/hermes_cli/dashboard_auth_cookies.o \
 src/hermes_cli/kanban_diagnostics.o src/hermes_cli/kanban_diagnostics_ports.o src/hermes_cli/nous_account_ports.o src/hermes_cli/nous_portal_ports.o src/hermes_cli/uninstall_ports.o src/hermes_cli/proxy_cli_ports.o src/hermes_cli/windows_ssh_runtime_ports.o src/hermes_cli/hermes_state_helpers2.o \
 src/hermes_cli/moa_config.o \
 src/hermes_cli/azure_detect.o \
 src/hermes_cli/fallback_cmd.o \
 src/hermes_cli/session_recap.o \
 src/hermes_cli/middleware.o \
 src/tools/lazy_deps.o \
 src/hermes_cli/curses_ui.o \
 src/hermes_cli/security_audit.o \
 \
 src/hermes_cli/model_normalize.o \
 src/hermes_cli/provider_catalog.o \
 src/hermes_cli/model_switch.o \
 src/hermes_cli/nous_billing.o src/hermes_cli/billing_json_helpers.o \
 src/hermes_cli/runtime_provider.o src/hermes_cli/sqlite_util.o src/hermes_cli/projects_db.o src/hermes_cli/debug_cli.o src/hermes_cli/auth_helpers.o src/hermes_cli/kanban_format.o src/hermes_cli/prompt_stash.o \
 \
 \
 src/hermes_cli/web_server_auth.o \
 src/hermes_cli/web_server_schema_path.o \
 src/hermes_cli/web_git.o \
 src/hermes_cli/web_git_routes.o \
 src/hermes_cli/web_update.o \
 src/hermes_cli/update_cmd.o \
src/hermes_cli/update_cmd_more.o \
src/hermes_cli/update_cmd_gaps.o \
src/hermes_cli/gateway_gaps.o \
src/hermes_cli/auth_gaps.o \
src/hermes_cli/banner_gaps.o \
src/hermes_cli/profile_gaps.o \
 src/hermes_cli/agent_import.o \
 src/gateway/api_server.o \
 src/gateway/base.o \
 src/gateway/signal.o \
 src/gateway/signal_rate_limit.o \
 src/gateway/webhook.o \
 src/tools/browser_camofox.o src/tools/browser_camofox_state.o \
 src/tools/browser_supervisor.o \
 src/tools/computer_use/browser_route.o \
 src/tools/port_environments/base.o \
 src/agent/anthropic_adapter.o src/agent/anthropic_adapter_ports.o \
 src/agent/logger.o src/hermes_logging.o src/infrastructure_stubs.o \
 src/hermes_cli/plugin_manifest.o \
 src/hermes_cli/config_helpers.o \
 src/hermes_cli/command_registry.o \
 src/hermes_cli/shared_metrics_contract.o \
 src/hermes_cli/smc_event_extractors.o \
 src/gateway/relay/relay_shared_metrics.o src/gateway/relay/command_manifest.o \
 src/hermes_cli/heartbeat.o \
 src/hermes_cli/focus_view.o \
 src/hermes_cli/slash_exec.o \
 src/hermes_cli/kanban_swarm.o \
 src/hermes_cli/skills_config.o \
 \
 src/agent/secret_sources/registry_gaps3.o \
 src/agent/ssl_verify_gaps3.o \
 src/agent/terminal_env_registry_gaps3.o \
 src/agent/title_generator_gaps3.o \
 src/agent/transports/chat_completions_gaps3.o \
 src/agent/transports/types_gaps3.o \
 src/agent/usage_pricing_gaps3.o \
 src/agent/verification_evidence_gaps3.o \
 src/agent/vertex_adapter_gaps3.o \
 src/agent/web_search_registry_gaps3.o \
 src/cron/executions_gaps3.o \
 src/cron/notepad_gaps3.o \
 src/cron/scheduler_provider_gaps3.o \
 src/cron/suggestions_gaps3.o \
 src/gateway/channel_directory_gaps3.o \
 src/gateway/config_gaps3.o \
 src/gateway/control_socket_gaps3.o \
 src/gateway/delivery_ledger_gaps3.o \
 src/gateway/drain_control_gaps3.o \
 src/gateway/lifecycle_ledger_gaps3.o \
 src/gateway/pairing_gaps3.o \
 src/gateway/platform_registry_gaps3.o \
 src/gateway/platforms/api_server_room_grants_gaps3.o \
 src/gateway/platforms/weixin_gaps3.o \
 src/gateway/platforms/yuanbao_gaps3.o \
 src/gateway/profile_routing_gaps3.o \
 src/gateway/readiness_gaps3.o \
 src/gateway/relay/adapter_gaps3.o \
 src/gateway/relay/ws_transport_gaps3.o \
 src/gateway/run_gaps3.o \
 src/gateway/scale_to_zero_gaps3.o \
 src/gateway/session_gaps3.o \
 src/gateway/session_db_recovery_gaps3.o \
 src/gateway/shutdown_flush_gaps3.o \
 src/gateway/shutdown_watchdog_gaps3.o \
 src/gateway/wake_gaps3.o \
 src/hermes_cli/_parser_gaps3.o \
 src/hermes_cli/agent_plugins_gaps3.o \
 src/hermes_cli/approvals_test_gaps3.o \
 src/hermes_cli/azure_detect_gaps3.o \
 src/hermes_cli/banner_gaps3.o \
 src/hermes_cli/build_info_gaps3.o \
 src/hermes_cli/bundles_gaps3.o \
 src/hermes_cli/callbacks_gaps3.o \
 src/hermes_cli/checkpoints_gaps3.o \
 src/hermes_cli/cli_agent_setup_mixin_gaps3.o \
 src/hermes_cli/cli_output_gaps3.o \
 src/hermes_cli/clipboard_gaps3.o \
 src/hermes_cli/codex_runtime_switch_gaps3.o \
 src/hermes_cli/container_boot_gaps3.o \
 src/hermes_cli/curator_gaps3.o \
 src/hermes_cli/dashboard_auth/base_gaps3.o \
 src/hermes_cli/dashboard_auth/routes_gaps3.o \
 src/hermes_cli/dashboard_procs_gaps3.o \
 src/hermes_cli/dep_ensure_gaps3.o \
 src/hermes_cli/dingtalk_auth_gaps3.o \
 src/hermes_cli/fallback_cmd_gaps3.o \
 src/hermes_cli/gitlock_gaps3.o \
 src/hermes_cli/image_provenance_gaps3.o \
 src/hermes_cli/install_identity_gaps3.o \
 src/hermes_cli/kanban_gaps3.o \
 src/hermes_cli/kanban_db_gaps3.o \
 src/hermes_cli/kanban_swarm_gaps3.o \
 src/hermes_cli/local_runtime/capabilities_gaps3.o \
 src/hermes_cli/local_runtime/detect_gaps3.o \
 src/hermes_cli/managed_uv_gaps3.o \
 src/hermes_cli/mcp_picker_gaps3.o \
 src/hermes_cli/migrate_gaps3.o \
 src/hermes_cli/model_catalog_gaps3.o \
 src/hermes_cli/model_cost_guard_gaps3.o \
 src/hermes_cli/models_gaps3.o \
 src/hermes_cli/nous_account_gaps3.o \
 src/hermes_cli/nous_billing_gaps3.o \
 src/hermes_cli/nous_subscription_gaps3.o \
 src/hermes_cli/observability/shared_metrics_gaps3.o \
 src/hermes_cli/observability/shared_metrics_send_config_gaps3.o \
 src/hermes_cli/oneshot_gaps3.o \
 src/hermes_cli/partial_compress_gaps3.o \
 src/hermes_cli/platforms_gaps3.o \
 src/hermes_cli/plugin_capabilities_gaps3.o \
 src/hermes_cli/plugin_index_gaps3.o \
 src/hermes_cli/plugin_packs_gaps3.o \
 src/hermes_cli/portal_cli_gaps3.o \
 src/hermes_cli/process_identity_gaps3.o \
 src/hermes_cli/profile_describer_gaps3.o \
 src/hermes_cli/projects_db_gaps3.o \
 src/hermes_cli/providers_gaps3.o \
 src/hermes_cli/psutil_android_gaps3.o \
 src/hermes_cli/pty_bridge_gaps3.o \
 src/hermes_cli/relaunch_gaps3.o \
 src/hermes_cli/resource_limits_gaps3.o \
 src/hermes_cli/secrets_cli_gaps3.o \
 src/hermes_cli/session_lost_and_found_gaps3.o \
 src/hermes_cli/sessions_cmd_gaps3.o \
 src/hermes_cli/skin_engine_gaps3.o \
 src/hermes_cli/stdio_gaps3.o \
 src/hermes_cli/subcommands/dashboard_gaps3.o \
 src/hermes_cli/subcommands/peer_gaps3.o \
 src/hermes_cli/telegram_managed_bot_gaps3.o \
 src/hermes_cli/terminal_breadcrumbs_gaps3.o \
 src/hermes_cli/toolset_validation_gaps3.o \
 src/hermes_cli/uninstall_gaps3.o \
 src/hermes_cli/update_contract_gaps3.o \
 src/hermes_cli/update_inventory_gaps3.o \
 src/hermes_cli/voice_gaps3.o \
 src/hermes_cli/web_git_gaps3.o \
 src/hermes_cli/web_routers/git_gaps3.o \
 src/hermes_cli/web_routers/mcp_gaps3.o \
 src/hermes_cli/web_routers/sessions_gaps3.o \
 src/hermes_cli/web_routers/skills_gaps3.o \
 src/hermes_cli/webhook_gaps3.o \
 src/hermes_cli/win_pty_bridge_gaps3.o \
 src/hermes_cli/xai_retirement_gaps3.o \
 src/tools/binary_extensions_gaps3.o \
 src/tools/computer_use/doctor_gaps3.o \
 src/tools/credential_files_gaps3.o \
 src/tools/delegation_output_schema_gaps3.o \
 src/tools/desktop_ui_gaps3.o \
 src/tools/env_probe_gaps3.o \
 src/tools/environments/docker_gaps3.o \
 src/tools/file_state_gaps3.o \
 src/tools/fuzzy_match_gaps3.o \
 src/tools/image_source_gaps3.o \
 src/tools/interpreter_shutdown_gaps3.o \
 src/tools/interrupt_gaps3.o \
 src/tools/kanban_tools_gaps3.o \
 src/tools/mcp_death_supervisor_gaps3.o \
 src/tools/preview_tool_gaps3.o \
 src/tools/project_tools_gaps3.o \
 src/tools/react_to_message_tool_gaps3.o \
 src/tools/self_repo_guard_gaps3.o \
 src/tools/skill_manager_tool_gaps3.o \
 src/tools/skills_sync_gaps3.o \
 src/tools/tip_tool_gaps3.o \
 src/tools/todo_tool_gaps3.o \
 src/tools/tool_backend_helpers_gaps3.o \
 src/tools/tour_tool_gaps3.o \
 src/tools/transcription_tools_gaps3.o \
 src/tools/video_generation_tool_gaps3.o \
 src/tools/voice_client_config_gaps3.o \
 src/tools/web_tools_gaps3.o \
