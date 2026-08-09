# ── Object File Lists ──────────────────────────────────────────────
# All source file objects organized by subsystem
# Included by top-level Makefile

CLI_OBJ = src/cli/cli.o src/cli/commands.o src/cli/config.o src/cli/config_env.o src/cli/config_platforms.o src/cli/config_profile.o src/cli/config_diff.o src/cli/config_io.o src/cli/config_schema.o src/cli/config_migrate.o src/cli/config_setup.o src/cli/config_schema.o src/cli/paths.o src/cli/display.o src/cli/display_core.o src/cli/main.o src/cli/doctor.o src/cli/setup_wizard.o src/cli/cli_gaps.o src/cli/port_context_switch_guard.o src/cli/port_gateway_windows.o src/cli/port_nous_billing.o src/cli/port_nous_billing_ports.o src/cli/port_nous_subscription_ports.o src/cli/port_proxy_xai_ports.o src/cli/port_proxy_adapters_base_ports.o src/cli/port_console_engine_ports.o src/cli/port_voice.o src/cli/port_config_pure.o src/cli/port_status_helpers.o src/cli/port_cli_logs.o src/cli/cli_cmd_system.o src/cli/cli_cmd_help.o src/cli/cli_cmd_session.o src/cli/cli_cmd_config.o src/cli/cli_cmd_misc.o src/cli/cli_cmd_gateway.o src/cli/cli_cmd_skills.o src/cli/cli_cmd_tools.o src/cli/cli_cmd_mcp.o src/cli/cli_cmd_kanban.o src/cli/cli_cmd_security.o src/cli/cli_cmd_memory.o src/cli/cli_cmd_display.o src/cli/cli_cmd_parity.o src/cli/port_model_cost_guard.o src/battery.o src/cli/port_cli_profiles.o src/cli/port_web_server_status.o src/cli/port_web_server_managed_files.o src/cli/port_web_server_fs.o src/cli/port_web_server_events.o src/cli/port_web_server_console.o src/cli/port_web_server_themes.o src/cli/port_web_server_ws_auth.o src/cli/hermes_cli_ws_tickets.o src/cli/port_web_server_chat_argv.o src/cli/port_web_server_cron_dash.o src/cli/port_web_server_sessions_admin.o src/cli/port_web_server_session_detail.o src/cli/port_web_server_session_endpoints.o src/cli/port_web_server_prune.o src/cli/port_default_soul.o src/cli/port_agent_browser_provider_methods.o src/cli/port_agent_gemini_cloudcode_adapter_methods.o src/cli/port_agent_think_scrubber.o src/cli/port_agent_tts_provider_methods.o src/cli/port_hermes_cli_dashboard_auth_prefix.o src/cli/port_hermes_cli_hooks.o src/cli/port_hermes_cli_prompt_size.o src/cli/port_hermes_cli_voice.o src/cli/port_hermes_cli_write_approval_commands.o src/cli/display_diff.o \
    src/cli/port_web_server_paths.o \
    src/cli/port_msgraph_webhook_helpers.o \
    src/cli/port_pairing_helpers.o \
    src/cli/port_webhook_helpers.o \

# Auto-generated hermes_cli/ port objects (empty — placeholder)
HERMES_CLI_PORT_OBJ = \

HERMES_CLI_PORT_EXTRA_OBJ = \

PORT_OBJ = \
    src/cli/port_hermes_cli_proxy_adapters_base.o \
    src/cli/port_hermes_cli_proxy_adapters_xai.o \
    src/cli/port_hermes_cli_proxy_adapters_nous_portal.o \
    src/cli/port_agent_anthropic_adapter.o \
    src/cli/port_agent_bedrock_adapter.o \
    src/cli/port_agent_codex_runtime.o \
    src/cli/port_agent_context_compressor.o \
    src/cli/port_agent_context_references.o \
    src/cli/port_agent_copilot_acp_client.o \
    src/cli/port_agent_credits_tracker.o \
    src/cli/port_agent_display.o src/agent/port_display_ports.o \
    src/cli/port_display_tool_preview.o \
    src/cli/port_agent_error_classifier.o \
    src/cli/port_agent_oneshot.o \
    src/cli/port_agent_google_oauth.o \
    src/cli/port_agent_image_gen_provider.o \
    src/cli/port_agent_insights.o \
    src/cli/port_agent_model_metadata.o \
    src/cli/port_agent_onboarding.o \
    src/cli/port_agent_plugin_llm.o \
    src/cli/port_agent_skill_utils.o \
    src/cli/port_agent_tool_executor.o \
    src/cli/port_agent_tool_guardrails.o \
    src/cli/port_agent_transcription_provider.o \
    src/cli/port_agent_usage_pricing.o \
    src/cli/port_agent_web_search_provider.o \
    src/cli/port_cli.o \
    src/cli/port_gateway_authz_mixin.o \
    src/cli/port_gateway_channel_directory.o \
    src/cli/port_gateway_config.o \
    src/cli/port_gateway_delivery.o \
    src/cli/port_gateway_hooks.o \
    src/cli/port_gateway_platform_registry.o \
    src/cli/port_gateway_platforms_feishu_meeting_invite.o \
    src/cli/port_gateway_platforms_helpers.o \
    src/cli/port_gateway_platforms_qqbot_crypto.o \
    src/cli/port_gateway_platforms_qqbot_keyboards.o \
    src/cli/port_gateway_platforms_qqbot_onboard.o \
    src/cli/port_gateway_platforms_qqbot_utils.o \
    src/cli/port_gateway_platforms_signal_rate_limit.o \
    src/cli/port_gateway_platforms_sms.o \
    src/cli/port_gateway_platforms_telegram_network.o \
    src/cli/port_gateway_platforms_wecom_callback.o \
    src/cli/port_gateway_platforms_wecom_crypto.o \
    src/cli/port_gateway_platforms_yuanbao_media.o \
    src/cli/port_gateway_platforms_yuanbao_sticker.o \
    src/cli/port_gateway_response_filters.o \
    src/cli/port_gateway_scale_to_zero.o \
    src/cli/port_gateway_signal_format.o \
    src/cli/port_gateway_whatsapp_identity.o \
    src/cli/port_gateway_shutdown_forensics.o \
    src/cli/port_gateway_slash_access.o \
    src/cli/port_gateway_sticker_cache.o \
    src/cli/port_hermes_cli_models.o \
    src/cli/model_catalog.o \
    src/cli/port_hermes_cli_main_helpers.o \
    src/cli/port_hermes_cli_main_gaps.o \
    src/cli/port_turn_summary_cli.o \
    src/cli/port_hermes_cli_main_cmds.o \
    src/cli/port_checkpoints_format.o \
    src/cli/port_hermes_cli_backup.o \
    src/cli/port_pty_clamp_helpers.o \
    src/cli/port_hermes_cli_gateway_platform.o \
    src/cli/port_hermes_cli_kanban_helpers.o \
    src/cli/port_config_helpers.o \
    src/cli/port_fallback_config.o \
    src/cli/port_timeouts.o \
    src/cli/port_session_listing.o \
    src/cli/port_project_tree.o \
    src/cli/port_models_helpers.o \
    src/cli/port_models_net.o \
    src/cli/port_models_pure.o \
    src/cli/port_models_validate.o \
    src/cli/port_status_helpers.o \
    src/cli/port_shared_metrics.o \
    src/cli/port_managed_scope_helpers.o \
    src/cli/port_context_breakdown_helpers.o \
    src/cli/port_verification_stop_helpers.o \
    src/cli/port_blueprint_catalog_helpers.o \
    src/cli/port_vertex_adapter_helpers.o \
    src/cli/port_drain_control_helpers.o \
    src/cli/port_platforms_base_helpers.o \
    src/cli/port_auth_helpers.o \
    src/cli/gateway_command_sanitize.o \
    src/cli/blueprint_cmd.o \
    src/cli/port_goals_data.o \
    src/cli/port_goals_manager.o \
    src/cli/port_goals_helpers.o \
    src/cli/port_provider_meta.o \
    src/cli/port_completion.o src/cli/port_completion_ports.o \
    src/cli/port_scale_to_zero_helpers.o \
    src/cli/port_cron_jobs_helpers.o \
    src/cli/port_profiles_helpers.o \
    src/cli/port_yuanbao_proto_helpers.o \
    src/cli/port_learning_graph_render_helpers.o \
    src/cli/port_lazy_deps_helpers.o \
    src/cli/port_model_switch_helpers.o \
    src/cli/port_learning_graph_helpers.o \
    src/cli/port_learning_graph.o \
    src/cli/port_learning_graph_render_helpers.o \
    src/cli/port_learning_graph_render.o \
    src/cli/port_learning_mutations.o \
    src/cli/port_file_state_helpers.o \
    src/cli/port_fuzzy_match_helpers.o \
    src/cli/port_file_tools_helpers.o \
    src/cli/port_cua_backend_helpers.o \
    src/cli/port_delegate_tool_helpers.o \
    src/cli/port_profiles_helpers.o \
    src/cli/port_tools_blueprints.o \
    src/cli/port_tools_browser_cdp_tool.o \
    src/cli/port_tools_browser_camofox.o \
    src/cli/port_tools_discord_helpers.o \
    src/cli/port_agent_pet_render.o \
    src/cli/port_whatsapp_common_helpers.o \
    src/cli/port_whatsapp_cloud_helpers.o \
    src/cli/port_clipboard_helpers.o \
    src/cli/port_doctor_helpers.o \
    src/cli/port_code_execution_helpers.o \
    src/cli/port_code_execution_helpers.o \
                                src/cli/port_tools_checkpoint_manager.o \
        src/cli/port_tools_clarify_gateway.o \
        src/cli/port_tools_clarify_tool.o \
        src/cli/port_tools_computer_use_backend.o \
        src/cli/port_tools_computer_use_vision_routing.o \
        src/cli/port_tools_credential_files.o \
        src/cli/port_tools_debug_helpers.o \
        src/cli/port_tools_env_passthrough.o \
        src/cli/port_tools_env_probe.o \
        src/cli/port_tools_environments_daytona.o \
        src/cli/port_tools_environments_file_sync.o \
        src/cli/port_tools_environments_managed_modal.o \
        src/cli/port_tools_environments_modal_utils.o \
        src/cli/port_tools_environments_singularity.o \
        src/cli/port_tools_environments_ssh.o \
        src/cli/port_tools_fal_common.o \
        src/cli/port_tools_feishu_drive_tool.o \
        src/cli/port_tools_managed_tool_gateway.o \
        src/cli/port_tools_mcp_oauth_manager.o \
        src/cli/port_tools_microsoft_graph_auth.o \
        src/cli/port_tools_microsoft_graph_client.o \
        src/cli/port_tools_mixture_of_agents_tool.o \
        src/cli/port_tools_openrouter_client.o \
        src/cli/port_tools_osv_check.o \
        src/cli/port_tools_patch_parser.o \
        src/cli/port_tools_path_security.o \
        src/cli/port_tools_read_terminal_tool.o \
        src/cli/port_tools_schema_sanitizer.o \
        src/cli/port_tools_session_search_tool.o \
        src/cli/port_tools_skills_ast_audit.o \
        src/cli/port_tools_skills_guard.o \
        src/tools/port_tools_slash_confirm.o \
        src/cli/port_tools_threat_patterns.o \
        src/cli/port_tools_todo_tool.o \
        src/cli/port_tools_tool_backend_helpers.o \
        src/cli/port_tools_tool_output_limits.o \
        src/cli/port_tools_tool_result_storage.o \
        src/cli/port_tools_fuzzy_match.o \
        src/cli/port_tools_tool_search.o \
        src/cli/port_tools_url_safety.o \
        src/cli/port_tools_video_generation_tool.o \
        src/cli/port_tools_website_policy.o \
        src/cli/port_tools_write_approval.o \
        src/cli/port_tools_xai_http.o \
        src/skills/skills_parser.o \
        src/cli/port_auth_na.o \
        src/cli/port_auth_store.o \
        src/cli/port_mcp_security.o \
        src/cli/port_hermes_cli_security_advisories.o \
        src/cli/port_kanban_db_na.o \
        src/cli/port_main_na.o \
        src/cli/port_web_server_extra.o \
        src/gateway/platforms/port_signal_na.o \
        src/cli/port_tools_yuanbao_tools.o \
        src/cli/port_hermes_cli_memory_setup.o \
        src/cli/port_hermes_cli_toolset_validation.o \
        src/tools/port_memory_tool.o \
        src/tools/port_skills_hub.o src/tools/port_skills_hub_ports.o src/tools/port_skill_manager_tool_ports.o src/tools/port_skill_usage_ports.o src/tools/port_todo_tool_ports.o \
        src/tools/port_mcp_tool.o src/tools/port_mcp_tool_ports.o \
        src/tools/port_send_message_tool.o \
        src/tools/port_tts_tool.o src/tools/port_tts_tool_ports.o src/tools/port_approval_ports.o src/tools/port_approval_helpers2.o src/tools/port_cua_backend_ports.o src/tools/port_voice_mode_ports.o src/tools/port_environments_base_ports.o src/tools/port_environments_modal_ports.o src/tools/port_environments_docker_ports.o src/tools/port_browser_supervisor_ports.o src/tools/port_tool_search_ports.o src/tools/port_moa_performance_ports.o src/tools/port_tts_streaming_ports.o src/tools/port_delegation_live_log_ports.o src/tools/port_memory_tool_ports.o src/tools/port_computer_use_tool_ports.o src/tools/port_delegate_tool_ports.o \
        src/tools/port_file_operations.o src/tools/port_file_operations_ports.o \
        src/tools/port_image_generation_tool.o \
        src/tools/environments.o \
        src/gateway/session.o \
        src/gateway/stream_consumer.o \
        src/cli/port_gateway_channel_directory.o \
        src/gateway/session_context.o \
        src/cli/port_agent_insights.o \
        src/cli/port_agent_usage_pricing.o \
        src/cli/port_cli_extra.o \
        src/cli/port_config.o \
        src/cli/port_container_boot.o \
        src/cli/port_context_switch_guard.o \
        src/cli/port_dump.o src/cli/port_dump_ports.o src/cli/port_journey_ports.o src/cli/port_kanban_decompose_ports.o src/cli/port_pairing_ports.o \
        src/cli/port_gateway.o \
        src/cli/port_gateway_windows.o \
        src/cli/port_goals.o src/cli/port_goals_ports.o src/cli/port_profile_distribution_ports.o src/cli/port_projects_db_ports.o src/cli/port_pty_bridge_ports.o src/cli/port_win_pty_bridge_ports.o src/cli/port_service_manager_ports.o src/cli/port_active_sessions_ports.o \
        src/cli/port_kanban_db.o \
        src/cli/kanban_schema.o \
        src/cli/kanban_model.o \
        src/cli/kanban_tasks.o \
        src/cli/kanban_lifecycle.o \
        src/cli/kanban_notify.o \
        src/cli/kanban_stats.o \
        src/cli/kanban_runs.o \
        src/cli/kanban_query.o \
        src/cli/kanban_util.o \
        src/cli/kanban_boards.o \
        src/cli/kanban_decompose.o \
        src/cli/kanban_workers.o \
        src/cli/port_main.o \
        src/cli/port_memory_providers.o \
        src/cli/port_dashboard_auth_audit.o \
        src/cli/port_dashboard_auth_registry.o \
        src/cli/port_dashboard_auth_cookies.o \
        src/cli/port_kanban_diagnostics.o src/cli/port_kanban_diagnostics_ports.o src/cli/port_nous_account_ports.o src/cli/port_nous_portal_ports.o src/cli/port_uninstall_ports.o src/cli/port_proxy_cli_ports.o src/cli/port_windows_ssh_runtime_ports.o src/cli/port_hermes_state_helpers2.o \
        src/cli/port_moa_config.o \
        src/cli/port_azure_detect.o \
        src/cli/port_fallback_cmd.o \
        src/cli/port_session_recap.o \
        src/cli/port_middleware.o \
        src/cli/port_lazy_deps.o \
        src/cli/port_curses_ui.o \
        src/cli/port_security_audit.o \
        src/cli/port_doctor.o \
        src/cli/port_model_normalize.o \
        src/cli/port_provider_catalog.o \
        src/cli/port_model_switch.o \
        src/cli/port_nous_billing.o src/cli/billing_json_helpers.o \
        src/cli/port_runtime_provider.o src/hermes_cli/sqlite_util.o src/hermes_cli/projects_db.o src/hermes_cli/debug_cli.o src/hermes_cli/auth_helpers.o src/hermes_cli/kanban_format.o src/hermes_cli/port_prompt_stash.o \
        src/cli/port_voice.o \
        src/cli/port_web_server.o \
        src/cli/port_web_server_auth.o \
        src/cli/port_web_server_schema_path.o \
        src/cli/port_web_git.o \
        src/cli/port_web_git_routes.o \
        src/cli/port_web_update.o \
        src/cli/port_hermes_cli_update_cmd.o \
        src/cli/port_hermes_cli_agent_import.o \
        src/gateway/port_api_server.o \
        src/gateway/port_base.o \
        src/gateway/port_signal.o \
        src/gateway/port_signal_rate_limit.o \
        src/gateway/port_webhook.o \
        src/tools/browser_camofox.o src/tools/browser_camofox_state.o \
        src/tools/port_browser_supervisor.o \
        src/tools/computer_use/port_browser_route.o \
        src/tools/port_environments/base.o \
        src/agent/anthropic_adapter.o src/agent/port_anthropic_adapter_ports.o \
        src/cli/port_plugin_manifest.o \
        src/cli/port_tools_config_helpers.o \
        src/cli/port_cli_command_registry.o \
        src/cli/port_hermes_cli_shared_metrics_contract.o \
        src/cli/port_hermes_cli_relay_shared_metrics.o \
        src/cli/port_hermes_cli_focus_view.o \
        src/cli/port_hermes_cli_slash_exec.o \
        src/cli/port_hermes_cli_kanban_swarm.o \
        src/cli/port_hermes_cli_skills_config.o \
        src/cli/port_anthropic_adapter.o \
        src/cli/port_tools_voice_mode.o \

# Desktop app parity objects (v465-v468)
# Desktop app parity objects (core — always needed for main binary)
DESKTOP_CORE_OBJ = \
    src/pty.o \
    src/terminal.o \
    src/gateway_client.o \
    src/clipboard.o \
    src/file_ops.o \
    src/slermes_home.o

# Desktop windowing objects (only for desktop target)
DESKTOP_WINDOW_OBJ = \
    src/window_compositor.o \
    src/window_stubs.o \
    src/chat_render.o \
    src/chat_composer.o \
    src/gateway_probe.o \
    src/desktop_app_common.o \
    src/desktop_sessions.o \
    src/desktop_models.o \
    src/desktop_profiles.o \
    src/desktop_settings.o \
    src/app_desktop.o

# Platform-specific window backends (only for desktop target)
ifeq ($(UNAME_S),Linux)
    DESKTOP_WINDOW_OBJ += src/window_wayland.o
    DESKTOP_WINDOW_OBJ += src/xdg-shell-protocol.o
else ifeq ($(UNAME_S),Darwin)
    DESKTOP_WINDOW_OBJ += src/window_macos.o
else
    # Windows (MinGW/Cygwin)
    DESKTOP_WINDOW_OBJ += src/window_win32.o
endif

# Combined for legacy targets
DESKTOP_OBJ = $(DESKTOP_CORE_OBJ) $(DESKTOP_WINDOW_OBJ)

# Desktop app (ncurses-based, PoP replacement)
DESKTOP_APP_OBJ = src/main_desktop.o src/app_desktop.o src/desktop_ui_layout.o src/desktop_ui_lifecycle.o src/desktop_ui_chrome.o src/desktop_ui_overlays.o src/desktop_input.o src/desktop_pty.o src/chat_render.o src/chat_composer.o \
    src/desktop_app_common.o src/desktop_sessions.o src/desktop_models.o src/desktop_profiles.o src/desktop_settings.o src/gateway_probe.o src/window_stubs.o \
    src/hermes_env_keys.o src/file_ops.o src/app_state.o src/session_db.o src/slermes_home.o \
    src/tools/voice_mode.o src/tools/transcribe.o src/tools/transcribe_helpers.o lib/libtranscribe/transcribe.o \
    src/tools/registry.o src/tools/tool_result.o src/gui_core.o \
    src/agent/logger.o src/pty.o src/clipboard.o
DESKTOP_LIBS_FILTER = lib/libdb/sqlite3.o lib/libtranscribe/transcribe.o

# Pet system parity objects (v509 — ripping Hermes pet system into C11)
PET_OBJ = \
    src/pet/pet_constants.o \
    src/pet/pet_state.o \
    src/pet/pet_manifest.o \
    src/pet/pet_store.o \
    src/pet/pet_render.o \
    src/pet/atlas.o \
    src/pet/pet_commands.o \
    src/pet/port_pet_prompts.o \
    lib/libpngdec/pngdec.o

# Custom GUI desktop (SDL2-based)
# Full SDL GUI application object set (mirrors DESKTOP_APP_OBJ but uses the
# SDL2 entry/event modules instead of the ncurses desktop_ui_* chain).
DESKTOP_GUI_OBJ := \
    src/gui_core.o src/desktop_gui.o src/slermes_home.o \
    src/app_state.o src/session_db.o src/sidebar.o src/chat_view.o src/titlebar.o \
    src/event_handling.o src/hud.o src/desktop_controller.o src/pet_ui.o \
    src/session_switcher.o \
    src/chat_render.o src/chat_composer.o \
    src/desktop_app_common.o src/desktop_sessions.o src/desktop_models.o \
    src/desktop_profiles.o src/desktop_settings.o src/gateway_probe.o src/window_stubs.o \
    src/hermes_env_keys.o src/file_ops.o \
    src/agent/logger.o src/pty.o src/clipboard.o \
    src/tools/voice_mode.o src/tools/transcribe.o src/tools/transcribe_helpers.o src/tools/registry.o src/tools/tool_result.o \
    lib/libtranscribe/transcribe.o \
    lib/libdb/sqlite3.o lib/libhttp/http.o lib/libjson/json.o lib/libbase64/base64.o lib/libcrypto/crypto.o
DESKTOP_GUI_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_ttf 2>/dev/null)
DESKTOP_GUI_LIBS := $(shell pkg-config --libs sdl2 SDL2_ttf 2>/dev/null) -lm -lssl -lcrypto -lz -lasound -lpthread

# Phase targets (each adds its objects)
DEPS_OBJ = src/hermes_error.o src/secrets.o src/hermes_tokenizer.o src/xai_retirement.o src/skills_hub.o src/mcp_serve.o src/api_server.o src/hermes_env_keys.o src/web_dashboard.o src/jiter_preload.o src/util_str.o lib/libwubuoffice/src/wubuzip/crc.o lib/libwubuoffice/src/wubuzip/zip.o lib/libwubuoffice/src/wubuzip/reader.o lib/libwubuoffice/src/wubuzip/bit.o lib/libwubuoffice/src/wubuzip/bitw.o lib/libwubuoffice/src/wubuzip/canon.o lib/libwubuoffice/src/wubuzip/huffman.o lib/libwubuoffice/src/wubuzip/fixed.o lib/libwubuoffice/src/wubuzip/fixedcode.o lib/libwubuoffice/src/wubuzip/limitcode.o lib/libwubuoffice/src/wubuzip/lz77.o lib/libwubuoffice/src/wubuzip/deflate.o lib/libwubuoffice/src/wubuzip/block.o lib/libwubuoffice/src/wubuzip/inflate.o lib/libwubuoffice/src/wubuoxml/package.o lib/libwubuoffice/src/wubuoxml/rels_path.o lib/libwubuoffice/src/wubuoxml/reader.o lib/libwubuoffice/src/wubuoxml/docx_text.o
AGENT_OBJ = src/agent/agent_loop.o src/agent/agent_runtime_helpers.o src/agent/port_agent_runtime_helpers_ports.o src/agent/chat_completion_helpers.o src/agent/credential_pool_engine.o src/agent/credential_pool_persistence.o src/agent/credential_pool_custom.o src/agent/credential_pool_sync.o src/agent/port_credential_pool_ports.o src/agent/credential_sources.o src/agent/llm_client.o src/agent/credential_persistence.o src/agent/logger.o src/agent/context.o src/agent/context_compressor_pure.o src/agent/context_engine.o src/agent/coding_context.o src/agent/stream_diag.o src/agent/port_stream_diag_ports.o src/agent/title.o src/agent/provider.o src/agent/provider_openai.o src/agent/provider_openrouter.o src/agent/provider_deepseek.o src/agent/provider_xai.o src/agent/provider_anthropic.o src/agent/provider_google.o src/agent/provider_azure.o src/agent/provider_bedrock.o src/agent/process_bootstrap.o src/agent/provider_custom.o src/agent/provider_codex_responses.o src/agent/codex_app_server_client.o src/agent/codex_app_server_session.o src/agent/codex_event_projector.o src/agent/port_codex_event_projector_ports.o src/agent/hermes_tools_mcp_server.o src/agent/port_hermes_tools_mcp_server_ports.o src/agent/fallback_routing.o src/agent/budget_tracker.o src/agent/provider_metadata.o src/agent/checkpoint.o src/agent/plugin_ext.o src/agent/redact.o src/agent/audit.o src/agent/sanitize.o src/agent/vault.o src/agent/think_scrubber.o src/agent/port_think_scrubber_helpers2.o src/agent/retry_utils.o src/agent/port_retry_utils_ports.o src/agent/port_thread_scoped_output_ports.o src/agent/trajectory.o src/agent/port_trajectory_ports.o src/agent/portal_tags.o src/agent/port_portal_tags_ports.o src/agent/markdown_tables.o src/agent/port_markdown_tables_ports.o src/agent/markdown_render.o src/agent/file_safety.o src/agent/system_prompt.o src/agent/port_system_prompt_ports.o src/agent/skill_preprocessing.o src/agent/port_skill_preprocessing_ports.o src/agent/tool_guardrails.o src/agent/port_tool_guardrails_ports.o src/agent/i18n.o src/agent/port_i18n_ports.o src/agent/subdir_hints.o src/agent/port_subdirectory_hints_ports.o src/agent/onboarding.o src/agent/image_routing.o src/agent/skill_bundles.o src/agent/usage_pricing.o src/agent/lmstudio_reasoning.o src/agent/context_breakdown.o src/agent/manual_compression_feedback.o src/agent/prompt_caching.o src/agent/port_prompt_caching_ports.o src/agent/context_references.o src/agent/port_context_references_ports.o src/agent/port_curator_backup_ports.o src/agent/port_plugin_llm_ports.o src/agent/port_context_engine_ports.o src/agent/port_file_safety_ports.o src/agent/port_conversation_loop_ports.o src/agent/port_credential_sources_ports.o src/agent/port_memory_manager_ports.o src/agent/port_video_gen_provider_ports.o src/agent/port_video_gen_registry_ports.o src/agent/port_codex_app_server_session_ports.o src/agent/port_azure_identity_adapter_ports.o src/agent/port_lsp_manager_ports.o src/agent/port_lsp_workspace_ports.o src/agent/port_lsp_reporter_ports.o src/agent/port_onepassword_ports.o src/agent/port_skill_bundles_ports.o src/agent/port_chat_completions_ports.o src/agent/port_skill_commands_ports.o src/agent/gemini_schema.o src/agent/moonshot_schema.o src/agent/port_moonshot_schema_ports.o src/agent/auxiliary_client.o src/agent/port_auxiliary_client_helpers2.o src/agent/port_memory_provider_ports.o src/agent/browser_provider.o src/agent/port_browser_provider_ports.o src/agent/browser_registry.o src/agent/port_browser_registry_ports.o src/agent/tool_error.o src/agent/hook_registry.o src/agent/shell_hooks.o src/agent/port_shell_hooks_ports.o src/agent/ssl_guard.o src/agent/port_ssl_guard_ports.o src/agent/port_web_search_registry_ports.o src/agent/port_web_search_provider_ports.o src/agent/nous_rate_guard.o src/agent/port_nous_rate_guard_ports.o src/agent/agent_gaps.o src/agent/todo_hydrate.o src/agent/file_mutation_verifier.o src/agent/api_error_summary.o src/agent/transcription_provider.o src/agent/port_transcription_provider_ports.o src/agent/port_transcription_registry_ports.o src/agent/verification_evidence.o src/agent/curator.o src/agent/port_curator_ports.o src/agent/plugin_llm.o src/agent/tool_executor.o src/agent/port_tool_executor_ports.o src/agent/google_code_assist.o src/agent/copilot_acp_client.o src/agent/port_copilot_acp_client_ports.o src/agent/port_copilot_acp_client_helpers2.o src/agent/azure_identity_adapter.o src/agent/agent_message_repair.o src/agent/agent_message_sanitize.o src/agent/proxy_utils.o src/agent/credits_tracker.o src/agent/billing_pure.o src/agent/turn_context.o src/agent/turn_retry_state.o src/agent/turn_retry_state.o src/agent/agent_init.o src/agent/port_agent_init_ports.o src/agent/port_iteration_budget_ports.o src/agent/port_moa_loop_ports.o src/agent/title_generator.o src/agent/port_title_generator_ports.o src/provider/token_exchange.o src/provider/google_oauth.o src/provider/copilot_oauth.o src/acp/server.o src/acp/edit_approval.o src/acp/events.o src/acp/permissions.o src/acp/resource.o src/tools/rate_limit.o src/tools/tool_result.o src/agent/skill_commands.o src/agent/memory_provider.o src/agent/anthropic_adapter.o src/agent/async_utils.o src/agent/background_review.o src/agent/port_background_review_ports.o src/agent/bedrock_adapter.o src/agent/port_bedrock_adapter_ports.o src/agent/codex_responses_adapter.o src/agent/port_codex_responses_adapter_ports.o src/agent/port_codex_app_server_ports.o src/agent/codex_runtime.o src/agent/context_compressor.o src/agent/port_context_compressor_micro.o src/agent/conversation_compression.o src/agent/conversation_loop.o src/agent/turn_finalizer.o src/agent/gemini_cloudcode_adapter.o src/agent/gemini_native_adapter.o src/agent/port_gemini_native_adapter_ports.o src/agent/port_gemini_native_helpers2.o src/agent/port_redact_ports.o src/agent/port_image_routing_ports.o src/agent/port_transports_codex_ports.o src/agent/port_transports_anthropic_ports.o src/agent/port_secret_sources_cache_ports.o src/agent/port_transports_bedrock_ports.o src/agent/port_transports_base_ports.o src/agent/port_transports_init_ports.o src/agent/port_model_metadata_ports.o src/agent/port_usage_pricing_ports.o src/agent/port_conversation_compression_ports.o src/agent/port_chat_completion_helpers_ports.o src/agent/port_lsp_install_ports.o src/agent/port_credits_tracker_ports.o src/agent/port_transports_types_ports.o src/agent/port_image_gen_provider_ports.o src/agent/port_image_gen_registry_ports.o src/agent/port_onboarding_ports.o src/agent/port_message_sanitization_ports.o src/agent/port_secret_sources_command_ports.o src/agent/port_secret_sources_base_ports.o src/agent/port_secret_sources_registry_ports.o src/agent/port_lsp_cli_ports.o src/agent/insights.o src/agent/port_insights_ports.o src/agent/message_sanitization.o src/agent/message_sanitization_pure.o src/agent/model_metadata.o src/agent/models_dev.o src/agent/port_models_dev_ports.o src/agent/prompt_builder.o src/agent/port_prompt_builder_ports.o src/agent/prompt_builder_guidance.o src/agent/runtime_cwd.o src/agent/port_runtime_cwd_ports.o src/agent/subdirectory_hints.o src/agent/port_agent_antigravity_code_assist.o src/agent/port_agent_antigravity_oauth.o src/agent/port_agent_auxiliary_client.o src/agent/port_agent_billing_view.o src/agent/port_agent_context_compressor.o src/agent/port_agent_conversation_loop.o src/agent/port_agent_display.o src/agent/port_agent_image_gen_provider.o src/agent/port_agent_memory_manager.o src/agent/port_agent_memory_provider.o src/agent/port_agent_message_content.o src/agent/port_message_content_ports.o src/agent/port_agent_prompt_builder.o src/agent/port_agent_secret_scope.o src/agent/port_agent_skill_commands.o src/agent/port_agent_skill_utils.o src/agent/port_agent_system_prompt.o src/agent/port_agent_title_generator.o src/agent/port_agent_tool_executor.o src/agent/port_memory_manager_helpers.o src/agent/port_agent_learn_prompt.o src/agent/port_agent_relay_runtime.o src/agent/port_turn_summary.o src/agent/monitoring/port_monitoring_emitter.o
TOOLS_OBJ = src/tools/registry.o src/tools/terminal.o src/tools/terminal_tool.o src/tools/terminal_env_registry.o src/tools/file.o src/tools/sandbox.o src/tools/file_lint.o src/tools/web.o src/tools/skills.o src/tools/tool_init.o src/tools/patch.o src/tools/exec_code.o src/tools/clarify.o src/tools/memory.o src/tools/memory_storage.o src/tools/todo.o src/tools/process.o src/tools/process_registry.o src/tools/port_close_terminal_tool.o src/tools/send_message.o src/tools/send_message_target.o src/tools/cronjob.o src/tools/skill_mgmt.o src/tools/session_search.o src/tools/session_crud.o src/tools/tts.o src/tools/vision.o src/tools/port_tools_tts_text_normalize.o \
    src/tools/port_vision_helpers.o \
src/tools/delegate.o src/tools/x_search.o src/tools/browser.o src/tools/browser_tool_env.o src/tools/browser_tool_platform.o src/tools/browser_tool_eval.o src/tools/browser_tool_install.o src/tools/browser_tool_path.o src/tools/browser_tool_cdp.o src/tools/approval.o src/tools/port_approval.o src/tools/skills_sync_fs.o src/tools/url_safety.o src/tools/port_tools_url_safety_ssrf.o src/tools/web_base64_img.o src/tools/tirith.o src/tools/voice_mode.o src/tools/voice_mode_pure.o src/tools/image_gen.o src/tools/image_gen_path.o src/tools/homeassistant.o src/tools/kanban_adapter.o src/tools/kanban_handlers.o src/tools/computer_use.o src/tools/result_storage.o src/tools/api_helpers.o src/tools/tool_config.o src/tools/discord.o src/tools/mcp_tool.o \
src/tools/file_batch.o src/tools/file_watch.o src/tools/feishu_tools.o src/tools/feishu_comment_rules.o src/tools/file_merge.o src/tools/mixture_of_agents.o src/tools/moa_performance.o src/tools/online_research.o src/tools/video_gen.o src/tools/video_gen_registry.o src/tools/video_analyze.o src/tools/image_gen_registry.o src/tools/web_search_registry.o src/tools/path_security.o src/tools/xai_http.o src/tools/account_usage.o src/agent/port_account_usage_ports.o src/agent/port_billing_view_ports.o src/tools/ansi_strip.o src/tools/transcribe.o \
    src/tools/transcribe_helpers.o \
src/tools/wecom_crypto.o src/tools/yuanbao_tools.o src/tools/yuanbao_media.o src/tools/media_cache.o src/sandbox_escape.o src/tools/env_probe.o src/tools/skills_guard.o src/tools/curator_backup.o src/tools/daytona.o src/tools/environment_gaps.o src/tools/image_gen_provider.o src/tools/tool_result_classification.o src/tools/tts_provider.o src/agent/port_tts_provider_ports.o src/tools/tts_registry.o src/agent/port_tts_registry_ports.o src/tools/video_gen_provider.o src/tools/binary_extensions.o src/tools/browser_camofox.o src/tools/managed_tool_gateway.o src/tools/microsoft_graph_auth.o src/tools/microsoft_graph_client.o src/tools/tool_backend_helpers.o src/tools/tool_output_limits.o src/tools/tool_search.o src/tools/checkpoint_manager.o src/tools/port_tools_async_delegation.o src/tools/port_base.o src/tools/cron_prompt_sanitize.o src/tools/port_cronjob_tools.o src/tools/file_text_ops.o src/tools/file_ops_lint.o src/tools/file_fs_ops.o src/tools/file_pagination_ops.o src/tools/port_file_tools_helpers.o src/tools/port_file_tools_helpers2.o src/tools/port_code_execution_tool_ports.o src/tools/port_mcp_oauth_ports.o src/tools/port_checkpoint_manager_ports.o src/tools/port_tools_read_extract.o src/tools/browser_redact.o src/tools/browser_supervisor_redact.o src/tools/skill_manager_val.o src/tools/port_cua_backend_helpers.o src/tools/port_cua_backend_helpers2.o src/tools/port_kanban_tools.o src/tools/port_kanban_tools_ports.o src/tools/port_transcription_tools_ports.o src/tools/port_env_managed_modal_ports.o src/gateway/server.o
GATEWAY_OBJ = src/gateway/server.o src/gateway/gw_pollers.o src/gateway/gw_notifier.o src/gateway/gw_format.o src/gateway/gw_thread.o src/gateway/gw_approval.o src/gateway/gw_logging.o src/gateway/gw_dispatch.o src/gateway/gw_session.o src/gateway/gw_setup.o src/gateway/gw_misc.o src/gateway/gateway_runtime.o src/gateway/config.o src/gateway/telegram_filter.o src/gateway/helpers.o src/gateway/run_pure.o src/gateway/shutdown_forensics.o src/gateway/sticker_cache.o src/gateway/mirror.o src/gateway/slash_access.o src/gateway/runtime_footer.o src/gateway/channel_directory.o src/gateway/gateway_gaps.o src/gateway/platforms/telegram.o src/gateway/platforms/telegram_network.o src/gateway/platforms/discord.o src/gateway/platforms/webhook.o src/gateway/platforms/slack.o src/gateway/platforms/matrix.o src/gateway/platforms/mattermost.o src/gateway/platforms/whatsapp.o src/gateway/platforms/email.o src/gateway/platforms/signal.o src/gateway/platforms/homeassistant.o src/gateway/platforms/sms.o src/gateway/platforms/feishu.o src/gateway/platforms/wecom.o src/gateway/platforms/wecom_callback.o src/gateway/platforms/dingtalk.o src/gateway/platforms/qqbot.o src/gateway/platforms/bluebubbles.o src/gateway/platforms/msgraph_webhook.o src/gateway/port_msgraph_webhook_ports.o src/gateway/port_platform_registry_ports.o src/gateway/platforms/msgraph_webhook_security.o src/gateway/platforms/weixin.o src/gateway/platforms/yuanbao.o src/gateway/platforms/base.o src/gateway/platforms/base_ext.o src/gateway/platforms/base_ext2.o src/gateway/platforms/base_adapter.o src/gateway/platforms/api_server_adapter.o src/gateway/platforms/api_server_adapter_handlers.o src/gateway/platforms/api_server_adapter_sessions.o src/gateway/platforms/api_server_adapter_chat.o src/gateway/platforms/api_server_adapter_responses.o src/gateway/platforms/api_server_adapter_runs.o src/gateway/platforms/api_server_adapter_cron.o src/gateway/platforms/feishu_comment_rules.o src/gateway/platforms/signal_rate_limit.o src/gateway/platforms/feishu_comment.o src/gateway/platforms/yuanbao_proto.o src/gateway/port_yuanbao_proto_ports.o src/gateway/platforms/yuanbao_media.o src/gateway/platforms/yuanbao_sticker.o src/gateway/gateway_lifecycle.o src/gateway/stream_events.o src/gateway/delivery.o src/gateway/display_config.o src/gateway/memory_monitor.o src/gateway/pairing.o src/gateway/restart.o src/gateway/run.o src/gateway/session.o src/gateway/session_context.o src/gateway/stream_consumer.o src/gateway/port_streaming_tts_consumer.o src/gateway/whatsapp_identity.o src/gateway/port_whatsapp_identity_ports.o src/gateway/port_gateway_message_timestamps.o src/gateway/port_gateway_platforms_api_server.o src/gateway/port_gateway_platforms_base.o src/gateway/port_platforms_base.o src/gateway/port_platforms_base_helpers2.o src/gateway/port_platforms_base_helpers3.o src/gateway/port_platforms_helpers_ports.o src/gateway/port_turn_lease_ports.o src/gateway/port_message_timestamps_ports.o src/gateway/port_qqbot_chunked_upload_ports.o src/gateway/port_signal_rate_limit_ports.o src/gateway/port_gateway_relay_adapter.o src/gateway/port_gateway_relay_auth.o src/gateway/port_relay_auth_helpers2.o src/gateway/port_relay_transport_ports.o src/gateway/port_gateway_relay_descriptor.o src/gateway/port_gateway_relay_transport.o src/gateway/port_gateway_relay_ws_transport.o src/gateway/port_gateway_rich_sent_store.o src/gateway/port_gateway_code_skew.o src/gateway/port_code_skew_ports.o src/gateway/port_gateway_authz_mixin.o src/gateway/restart_loop_guard.o src/gateway/dead_targets.o src/gateway/platforms/http_client_limits.o \
  src/gateway/port_gateway_session_state.o \
 src/gateway/port_gateway_platforms_base_media.o src/gateway/status.o
AGENT_PORT_NEW = src/agent/port_agent_redact_helpers.o src/agent/port_agent_tool_dispatch_helpers.o src/agent/port_agent_verify_hooks.o src/agent/port_agent_ssl_verify.o src/agent/port_agent_turn_context.o src/agent/port_billing_links.o src/agent/port_billing_usage.o src/agent/port_battery.o src/agent/port_aux_accounting.o src/agent/port_auxiliary_client.o src/agent/port_async_utils.o src/agent/hermes_state/hermes_state_open.o src/agent/hermes_state/hermes_state_lifecycle.o src/agent/hermes_state/hermes_state_messages.o src/agent/hermes_state/hermes_state_lineage.o src/agent/hermes_state/hermes_state_tokens.o src/agent/hermes_state/hermes_state_archive.o src/agent/hermes_state/hermes_state_misc.o src/agent/hermes_state/hermes_state_gaps.o src/agent/hermes_state/hermes_state_repair.o src/agent/hermes_state/hermes_state_locks.o src/agent/hermes_state/port_hermes_state_ports.o src/agent/port_iron_proxy.o src/agent/port_iron_proxy_ports.o src/agent/port_bitwarden_ports.o src/agent/port_secret_sources_helpers.o src/agent/port_lsp_servers.o src/agent/port_lsp_servers_ports.o src/agent/port_lsp_client_ports.o src/agent/port_lsp_eventlog.o src/agent/port_lsp_eventlog_ports.o src/agent/port_lsp_init_ports.o src/agent/conversation_compression_helpers.o src/cli/session_recovery/session_recovery_paths.o src/cli/session_recovery/session_recovery_preflight.o src/cli/session_recovery/session_recovery_copy.o src/cli/session_recovery/session_recovery_salvage.o src/cli/session_recovery/session_recovery_meta.o src/cli/session_recovery/session_recovery_orphans.o src/cli/session_recovery/session_recovery_verify.o src/cli/session_recovery/session_recovery_main.o src/agent/port_agent_coding_context.o src/agent/port_agent_background_review.o src/agent/port_antigravity_oauth.o src/agent/port_agent_monitoring_health_export.o src/agent/port_subagent_lifecycle.o src/agent/port_outbound_webhooks.o src/agent/port_web_deps.o src/agent/port_gateway_health.o
TOOLS_PORT_NEW = src/tools/port_file_operations_search.o src/tools/port_tools_slash_confirm.o src/tools/port_interrupt.o src/cli/port_moa_pure_helpers.o src/cli/port_config_py_pure.o src/cli/port_config_py_io.o src/cli/port_config_migrations.o src/cli/port_hermes_cli_update_lock.o src/cli/port_hermes_cli_npm_engine.o src/cli/port_hermes_cli_timefmt.o src/cli/port_models_py_helpers.o src/tools/port_research_helpers.o src/cli/port_moa_config_pure.o src/cli/port_moa_slash.o src/cli/port_input_sanitize.o src/cli/port_utils_truthy.o src/cli/port_session_title.o src/cli/port_state_usage_db.o src/tools/port_toolsets.o src/cli/port_platform_tools.o src/tools/port_browser_tool.o src/tools/port_budget_config.o src/tools/port_clarify_tool.o src/tools/port_feishu_drive_tool.o src/tools/port_feishu_drive_tool_ports.o src/tools/port_feishu_doc_tool_ports.o src/tools/port_debug_helpers_ports.o src/tools/port_env_local_ports.o src/tools/port_terminal_tool_ports.o src/tools/port_tools_terminal_hints.o src/tools/port_env_ssh_ports.o src/tools/port_mcp_oauth_manager_ports.o src/tools/port_microsoft_graph_client_ports.o src/tools/port_session_search_tool.o src/tools/port_write_approval.o src/tools/port_tools_wake_word.o src/tools/port_tools_flux3_video.o src/tools/flux3_video_tool.o src/tools/transcription_tools_pure.o src/tools/port_tools_vercel_sandbox.o src/tools/port_tools_vercel_sandbox_helpers.o src/tools/port_tools_vercel_sandbox_env.o src/agent/hermes_state/port_hermes_state.o src/cli/port_startup_fast.o
GATEWAY_PORT_NEW = src/gateway/platforms/port_platforms_helpers_md.o src/cli/port_gateway_platforms_helpers.o src/gateway/port_gateway_platforms_helpers.o src/gateway/port_gateway_drain_control.o src/gateway/run_pure2.o src/gateway/port_slash_command_guards.o src/gateway/port_gateway_run_deps.o src/gateway/port_slash_commands.o src/gateway/port_gateway_run_wrappers.o src/gateway/port_api_server_wrappers.o src/gateway/port_api_server_ports.o src/gateway/port_qqbot_wrappers.o src/gateway/port_yuanbao_wrappers.o src/gateway/port_yuanbao_ports.o src/gateway/port_weixin_wrappers.o src/gateway/port_weixin_ports.o src/gateway/port_signal_ports.o src/gateway/port_signal_helpers2.o src/gateway/port_api_server_ports.o src/gateway/port_session_helpers2.o src/gateway/port_relay_init_ports.o src/gateway/port_gateway_config_ports.o src/gateway/port_gateway_config_helpers2.o src/gateway/port_relay_ws_transport_ports.o src/gateway/port_gateway_session_ports.o src/gateway/port_session_context_ports.o src/gateway/port_gateway_status_ports.o src/gateway/port_qqbot_adapter_ports.o src/gateway/port_whatsapp_cloud_ports.o src/gateway/port_shutdown_watchdog_ports.o src/gateway/port_stream_dispatch_ports.o src/gateway/port_relay_adapter_ports.o src/gateway/port_relay_adapter_helpers.o src/gateway/port_relay_adapter_ops.o src/gateway/port_bluebubbles_ports.o src/gateway/port_systemd_notify_ports.o src/gateway/port_yuanbao_media_ports.o src/gateway/port_qqbot_keyboards_ports.o src/gateway/port_gateway_run_ports.o src/gateway/port_webhook_ports.o src/gateway/port_gateway_util_helpers.o src/gateway/port_status_wrappers.o src/gateway/port_bluebubbles_wrappers.o src/gateway/port_gateway_ports_wrappers.o src/gateway/port_hermes_constants_reasoning.o src/gateway/port_gateway_run_agent.o src/cli/port_provider_registry.o
AGENT_PORT_REGEN = src/agent/port_otlp_exporter.o src/agent/port_approvals_suggest_proposals.o src/cli/port_cli_status_pure.o src/agent/port_agent_monitoring_health_export_pure.o src/agent/port_context_compressor_pure.o src/agent/port_backend_identity.o src/agent/port_agent_reasoning_timeouts.o src/agent/port_agent_replay_cleanup.o src/agent/port_agent_retry_utils.o src/agent/port_agent_thinking_timeout_guidance.o src/agent/port_agent_intent_ack.o src/agent/port_agent_display_helpers.o src/agent/port_markdown_tables.o src/agent/port_agent_auxiliary_client_helpers.o src/agent/port_agent_auxiliary_client_runtime.o src/agent/port_agent_relay_llm.o src/agent/port_agent_auxiliary_client_gaps.o src/agent/port_moa_trace_helpers.o src/agent/port_lsp_range_shift.o src/agent/transport_common.o src/agent/secret_common.o src/agent/lsp_protocol.o src/agent/lsp_client.o src/agent/lsp_manager.o src/agent/provider_profile.o src/agent/provider_profiles_builtin.o src/agent/provider_profile_apply.o src/agent/port_agent_delegation_context.o src/agent/port_agent_reactions.o src/cli/port_plugins_wrappers.o src/cli/port_plugins_cmd_wrappers.o src/cli/port_cli_commands_mixin_wrappers.o src/cli/port_commands_wrappers.o src/cli/port_web_server_wrappers.o src/cli/port_main_wrappers.o src/cli/port_main_ports.o src/cli/port_utils_ports.o src/cli/port_kanban_db_ports.o src/cli/port_cli_billing_mixin_ports.o src/cli/port_auth_helpers2.o src/cli/port_gateway_cli_ports.o src/cli/port_web_server_helpers2.o src/cli/port_pty_session_ports.o src/cli/port_plugins_ports.o src/cli/port_auth_wrappers.o src/cli/port_tools_config_wrappers.o src/cli/port_cli_gateway_wrappers.o src/cli/port_console_engine_wrappers.o src/cli/port_gateway_windows_wrappers.o src/cli/port_kanban_wrappers.o src/cli/port_setup_wrappers.o src/cli/port_setup_ports.o src/cli/port_kanban_db_wrappers.o src/cli/port_managed_uv_wrappers.o src/cli/port_windows_ssh_wrappers.o src/cli/port_model_setup_wrappers.o src/cli/port_nous_account_wrappers.o src/cli/port_session_export_wrappers.o src/cli/port_uninstall_wrappers.o src/cli/port_model_switch_wrappers.o src/cli/port_session_export_md_wrappers.o src/cli/port_nous_sub_wrappers.o src/cli/port_runtime_provider_wrappers.o src/cli/port_cli_wrappers.o src/agent/port_skill_utils_wrappers.o src/agent/port_context_compressor_wrappers.o src/agent/port_context_compressor_ports.o src/agent/port_verif_evidence_wrappers.o src/cli/port_cli_ports_wrappers.o src/cli/port_other_ports_wrappers.o src/cli/port_cli_helpers2.o src/cli/port_cli_helpers3.o src/cli/port_config_ports.o src/agent/port_agent_ports_wrappers.o src/agent/port_agent_core_helpers.o src/cli/port_hermes_cli_util_helpers.o $(AGENT_PORT_NEW)
GATEWAY_PORT_REGEN = src/gateway/port_gateway_cgroup_cleanup.o src/gateway/port_display_config.o src/gateway/port_gateway_run.o src/gateway/gateway_runner.o src/gateway/port_gateway_run_helpers.o src/cli/port_gateway_mirror.o $(GATEWAY_PORT_NEW)
TOOLS_PORT_REGEN = src/tools/port_url_safety_helpers.o src/tools/port_patch_parser.o src/cli/port_tools_schema_sanitizer.o src/tools/port_tools_slash_confirm.o src/cli/port_hermes_cli_migrate.o src/tools/port_skills_sync.o src/tools/port_skills_sync_ports.o src/cli/port_cli_pet_input.o src/tools/port_browser_tool_helpers.o src/tools/port_web_tools.o src/tools/skill_prereqs.o src/tools/port_skill_usage_wrappers.o src/tools/port_transcription_wrappers.o src/tools/port_vision_wrappers.o src/tools/port_file_tools_wrappers.o src/tools/port_cua_backend_wrappers.o src/tools/port_env_local_wrappers.o src/tools/port_mcp_oauth_wrappers.o src/tools/port_async_delegation_wrappers.o src/tools/port_terminal_tool_wrappers.o src/tools/port_env_base_wrappers.o src/tools/port_env_docker_wrappers.o src/tools/port_skill_mgr_tool_wrappers.o src/tools/port_memory_tool_wrappers.o src/tools/port_approval_wrappers.o src/tools/port_tools_ports_wrappers.o src/tools/port_skills_tool.o src/tools/port_skills_tool_ports.o src/tools/port_file_state_ports.o src/tools/port_online_research_ports.o src/tools/port_tools_util_helpers.o src/tools/port_skills_sync_client.o $(TOOLS_PORT_NEW)
PHASE2_OBJ += src/cli/port_plugins_wrappers.o src/cli/port_plugins_cmd_wrappers.o src/cli/port_cli_commands_mixin_wrappers.o src/cli/port_commands_wrappers.o src/cli/port_web_server_wrappers.o src/cli/port_main_wrappers.o src/cli/port_auth_wrappers.o src/cli/port_tools_config_wrappers.o src/cli/port_cli_gateway_wrappers.o src/cli/port_console_engine_wrappers.o src/cli/port_gateway_windows_wrappers.o src/cli/port_kanban_wrappers.o src/cli/port_setup_wrappers.o src/cli/port_kanban_db_wrappers.o src/cli/port_managed_uv_wrappers.o src/cli/port_windows_ssh_wrappers.o src/agent/port_moa_loop_wrappers.o src/agent/port_memory_manager_wrappers.o src/cli/port_model_setup_wrappers.o src/cli/port_nous_account_wrappers.o src/cli/port_session_export_wrappers.o src/cli/port_uninstall_wrappers.o src/cli/port_model_switch_wrappers.o src/cli/port_session_export_md_wrappers.o src/cli/port_nous_sub_wrappers.o src/cli/port_runtime_provider_wrappers.o src/cli/port_cli_wrappers.o src/agent/port_skill_utils_wrappers.o src/agent/port_context_compressor_wrappers.o src/agent/port_verif_evidence_wrappers.o src/cli/port_cli_ports_wrappers.o src/cli/port_other_ports_wrappers.o src/agent/port_agent_ports_wrappers.o $(AGENT_PORT_NEW) $(AGENT_PORT_REGEN)
PHASE3_OBJ += $(TOOLS_PORT_NEW) $(TOOLS_PORT_REGEN)
PHASE4_OBJ += $(GATEWAY_PORT_NEW) $(GATEWAY_PORT_REGEN)
CRON_OBJ = src/cron/scheduler.o src/cron/jobs.o src/cron/cron_extras.o src/cron/cron_sqlite.o src/cron/port_scheduler_provider_ports.o src/cron/port_cron_scheduler_ports.o src/cron/cron_cli.o src/cron/port_cron_scheduler_provider.o src/cron/port_cron_scheduler_helpers.o src/cron/port_cron_scheduler_delivery.o src/cron/port_cron_scheduler_runtime.o src/cron/port_cron_scheduler_runtime_impl.o src/cron/port_cron_scheduler_toolsets.o src/cron/port_cron_scheduler_script.o src/cron/port_cron_scheduler_prompt.o src/cron/port_scheduler.o src/cron/cron_suggestions.o src/cron/suggestion_catalog.o src/cron/port_jobs.o src/cron/port_lifecycle_guard.o src/cron/port_cron_jobs.o src/cron/port_classify_items.o

# NOTE: the PHASE*_OBJ += lines above feed the standalone phase<N> targets only.
# The `slermes` link line builds PHASE5_OBJ FRESH from the subsystem *_OBJ lists,
# so port objects MUST be appended into those base lists to actually be linked.
# These appends wire the PoP port objects into the binary (root-cause fix for
# orphaned port_*.o that compiled but never linked).

# Progressively larger builds
PHASE1_OBJ = $(DEPS_OBJ) $(CLI_PORT_NEW2)
PHASE2_OBJ = $(PHASE1_OBJ) src/main.o $(AGENT_OBJ) $(CLI_OBJ)
PHASE3_OBJ = $(PHASE2_OBJ) $(TOOLS_OBJ)
PHASE4_OBJ = $(PHASE3_OBJ) $(GATEWAY_OBJ)
PHASE5_OBJ = $(PHASE4_OBJ) $(CRON_OBJ) $(AGENT_PORT_REGEN) $(TOOLS_PORT_REGEN) $(GATEWAY_PORT_REGEN) src/cli/port_turn_summary_cli.o src/cli/port_cli_parity_gaps.o
