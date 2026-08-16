# ── Object File Lists ──────────────────────────────────────────────
# All source file objects organized by subsystem
# Included by top-level Makefile



CLI_OBJ = src/cli/cli.o src/hermes_cli/partial_compress.o src/cli/config.o src/hermes_cli/config_env.o src/hermes_cli/config_platforms.o src/hermes_cli/config_profile.o src/hermes_cli/config_diff.o src/hermes_cli/config_io.o src/hermes_cli/config_schema.o src/hermes_cli/config_migrate.o src/cli/config_setup.o src/hermes_cli/config_schema.o src/cli/paths.o src/hermes_cli/cli_output.o src/cli/display_core.o src/cli/main.o src/cli/doctor.o src/cli/setup_wizard.o src/hermes_cli/cli_gaps.o src/hermes_cli/linux_desktop_entry.o src/hermes_cli/banner_pure.o src/hermes_cli/context_switch_guard.o src/hermes_cli/windows.o src/hermes_cli/nous_billing.o src/hermes_cli/nous_billing_ports.o src/hermes_cli/nous_subscription_ports.o src/hermes_cli/proxy_xai_ports.o src/hermes_cli/proxy_adapters_base_ports.o src/hermes_cli/console_engine_ports.o src/cli/port_voice.o src/hermes_cli/config_pure.o src/hermes_cli/status_helpers.o src/hermes_cli/logs.o src/cli/cli_cmd_system.o src/hermes_cli/cli_cmd_help.o src/cli/cli_cmd_session.o src/cli/cli_cmd_config.o src/hermes_cli/cli_cmd_misc.o src/cli/cli_cmd_gateway.o src/hermes_cli/cli_cmd_skills.o src/hermes_cli/cli_cmd_tools.o src/hermes_cli/cli_cmd_mcp.o src/hermes_cli/cli_cmd_kanban.o src/cli/cli_cmd_security.o src/cli/cli_cmd_memory.o src/hermes_cli/cli_cmd_display.o src/hermes_cli/cli_cmd_parity.o src/hermes_cli/model_cost_guard.o src/battery.o src/hermes_cli/profiles.o src/hermes_cli/web_server_status.o src/hermes_cli/web_server_managed_files.o src/hermes_cli/web_server_fs.o src/hermes_cli/web_server_events.o src/hermes_cli/web_server_console.o src/hermes_cli/web_server_themes.o src/hermes_cli/web_server_ws_auth.o src/hermes_cli/hermes_cli_ws_tickets.o src/hermes_cli/web_server_chat_argv.o src/hermes_cli/web_server_cron_dash.o src/hermes_cli/web_server_sessions_admin.o src/hermes_cli/web_server_session_detail.o src/hermes_cli/web_server_session_endpoints.o src/hermes_cli/web_server_prune.o src/hermes_cli/default_soul.o src/cli/port_agent_browser_provider_methods.o src/hermes_cli/gemini_cloudcode_adapter_methods.o src/cli/port_agent_think_scrubber.o src/agent/tts_provider.o src/hermes_cli/dashboard_auth_prefix.o src/hermes_cli/hooks.o src/hermes_cli/prompt_size.o src/cli/port_hermes_cli_voice.o src/cli/port_hermes_cli_write_approval_commands.o src/hermes_cli/display_diff.o \
    src/hermes_cli/web_server_paths.o \
    src/hermes_cli/msgraph_webhook_helpers.o \
    src/hermes_cli/pairing_helpers.o \
    src/hermes_cli/webhook_helpers.o \

# Auto-generated hermes_cli/ port objects (empty — placeholder)
HERMES_CLI_PORT_OBJ = \

HERMES_CLI_PORT_EXTRA_OBJ = \

PORT_OBJ = \
    src/hermes_cli/proxy_adapters_base.o \
    src/hermes_cli/proxy_adapters_xai.o \
    src/hermes_cli/proxy_adapters_nous_portal.o \
    src/hermes_cli/anthropic_adapter.o \
    src/hermes_cli/bedrock_adapter.o \
    src/cli/port_agent_codex_runtime.o \
    src/hermes_cli/context_compressor.o \
    src/cli/port_agent_context_references.o \
    src/cli/port_agent_copilot_acp_client.o \
    src/cli/port_agent_credits_tracker.o \
    src/hermes_cli/display.o src/agent/display_ports.o \
    src/hermes_cli/display_tool_preview.o \
    src/agent/error_classifier.o \
    src/agent/oneshot.o \
    src/agent/google_oauth.o \
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
    src/agent/web_search_provider.o \
    src/hermes_cli/cli.o \
    src/cli/port_gateway_authz_mixin.o \
    src/hermes_cli/channel_directory.o \
    src/cli/port_gateway_config.o \
    src/cli/port_gateway_delivery.o \
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
    src/cli/port_gateway_slash_access.o \
    src/cli/port_gateway_sticker_cache.o \
    src/hermes_cli/models.o \
    src/hermes_cli/model_catalog.o \
    src/hermes_cli/main_helpers.o \
    src/hermes_cli/main_gaps.o \
    src/hermes_cli/turn_summary_cli.o \
    src/hermes_cli/main_cmds.o \
    src/hermes_cli/approvals_suggest.o \
    src/hermes_cli/checkpoints_format.o \
    src/hermes_cli/backup.o \
    src/hermes_cli/pty_clamp_helpers.o \
    src/hermes_cli/gateway_platform.o \
    src/hermes_cli/kanban_helpers.o \
    src/cli/port_config_helpers.o \
    src/hermes_cli/fallback_config.o \
    src/hermes_cli/timeouts.o \
    src/hermes_cli/session_listing.o \
    src/hermes_cli/project_tree.o \
    src/hermes_cli/models_helpers.o \
    src/hermes_cli/models_net.o \
    src/hermes_cli/models_pure.o \
    src/hermes_cli/models_validate.o \
    src/hermes_cli/status_helpers.o \
    src/hermes_cli/shared_metrics.o \
    src/hermes_cli/managed_scope_helpers.o \
    src/hermes_cli/context_breakdown_helpers.o \
    src/hermes_cli/verification_stop_helpers.o \
    src/hermes_cli/blueprint_catalog_helpers.o \
    src/hermes_cli/vertex_adapter_helpers.o \
    src/hermes_cli/drain_control_helpers.o \
    src/hermes_cli/platforms_base_helpers.o \
    src/cli/port_auth_helpers.o \
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
                                src/cli/port_tools_checkpoint_manager.o \
        src/hermes_cli/clarify_gateway.o \
        src/cli/port_tools_clarify_tool.o \
        src/hermes_cli/computer_use_backend.o \
        src/hermes_cli/computer_use_vision_routing.o \
        src/hermes_cli/credential_files.o \
        src/tools/debug_helpers.o \
        src/tools/env_passthrough.o \
        src/cli/port_tools_env_probe.o \
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
        src/cli/port_tools_microsoft_graph_client.o \
        src/tools/mixture_of_agents_tool.o \
        src/tools/openrouter_client.o \
        src/tools/osv_check.o \
        src/cli/port_tools_patch_parser.o \
        src/cli/port_tools_path_security.o \
        src/tools/read_terminal_tool.o \
        src/hermes_cli/schema_sanitizer.o \
        src/cli/port_tools_session_search_tool.o \
        src/tools/skills_ast_audit.o \
        src/hermes_cli/skills_guard.o \
        src/tools/slash_confirm.o \
        src/hermes_cli/threat_patterns.o \
        src/tools/todo_tool.o \
        src/cli/port_tools_tool_backend_helpers.o \
        src/cli/port_tools_tool_output_limits.o \
        src/tools/tool_result_storage.o \
        src/hermes_cli/fuzzy_match.o \
        src/cli/port_tools_tool_search.o \
        src/cli/port_tools_url_safety.o \
        src/hermes_cli/video_generation_tool.o \
        src/hermes_cli/website_policy.o \
        src/hermes_cli/write_approval.o \
        src/cli/port_tools_xai_http.o \
        src/skills/skills_parser.o \
        src/hermes_cli/auth.o \
        src/hermes_cli/auth_store.o \
        src/hermes_cli/mcp_security.o \
        src/hermes_cli/security_advisories.o \
        src/cli/port_kanban_db_na.o \
        src/cli/port_main_na.o \
        src/cli/port_web_server_extra.o \
        src/gateway/platforms/signal_na.o \
        src/hermes_cli/yuanbao_tools.o \
        src/hermes_cli/memory_setup.o \
        src/hermes_cli/toolset_validation.o \
        src/tools/memory_tool.o \
        src/tools/skills_hub.o src/tools/skills_hub_ports.o src/tools/skills_hub_gaps.o src/tools/skill_manager_tool_ports.o src/tools/skill_usage_ports.o src/tools/todo_tool_ports.o \
        src/tools/port_mcp_tool.o src/tools/mcp_tool_ports.o \
        src/tools/send_message_tool.o \
        src/tools/tts_tool.o src/tools/tts_tool_ports.o src/tools/approval_ports.o src/tools/approval_helpers2.o src/tools/cua_backend_ports.o src/tools/voice_mode_ports.o src/tools/environments_base_ports.o src/tools/environments_modal_ports.o src/tools/environments_docker_ports.o src/tools/browser_supervisor_ports.o src/tools/tool_search_ports.o src/tools/moa_performance_ports.o src/tools/tts_streaming_ports.o src/tools/delegation_live_log_ports.o src/tools/memory_tool_ports.o src/tools/computer_use_tool_ports.o src/tools/delegate_tool_ports.o \
        src/tools/file_operations.o src/tools/file_operations_ports.o \
        src/tools/image_generation_tool.o \
        src/tools/environments.o \
        src/gateway/session.o \
        src/gateway/stream_consumer.o \
        src/hermes_cli/channel_directory.o \
        src/gateway/session_context.o \
        src/cli/port_agent_insights.o \
        src/cli/port_agent_usage_pricing.o \
        src/hermes_cli/extra.o \
        src/cli/port_config.o \
        src/hermes_cli/container_boot.o \
        src/hermes_cli/context_switch_guard.o \
        src/hermes_cli/dump.o src/hermes_cli/dump_ports.o src/hermes_cli/journey_ports.o src/hermes_cli/kanban_decompose_ports.o src/hermes_cli/pairing_ports.o \
        src/hermes_cli/gateway.o \
        src/hermes_cli/windows.o \
        src/hermes_cli/goals.o src/hermes_cli/goals_ports.o src/hermes_cli/profile_distribution_ports.o src/hermes_cli/projects_db_ports.o src/hermes_cli/pty_bridge_ports.o src/hermes_cli/win_pty_bridge_ports.o src/hermes_cli/service_manager_ports.o src/hermes_cli/active_sessions_ports.o \
        src/cli/port_kanban_db.o \
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
        src/cli/port_main.o \
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
        src/hermes_cli/lazy_deps.o \
        src/hermes_cli/curses_ui.o \
        src/hermes_cli/security_audit.o \
        src/cli/port_doctor.o \
        src/hermes_cli/model_normalize.o \
        src/hermes_cli/provider_catalog.o \
        src/hermes_cli/model_switch.o \
        src/hermes_cli/nous_billing.o src/hermes_cli/billing_json_helpers.o \
        src/hermes_cli/runtime_provider.o src/hermes_cli/sqlite_util.o src/hermes_cli/projects_db.o src/hermes_cli/debug_cli.o src/hermes_cli/auth_helpers.o src/hermes_cli/kanban_format.o src/hermes_cli/prompt_stash.o \
        src/cli/port_voice.o \
        src/cli/port_web_server.o \
        src/hermes_cli/web_server_auth.o \
        src/hermes_cli/web_server_schema_path.o \
        src/hermes_cli/web_git.o \
        src/hermes_cli/web_git_routes.o \
        src/hermes_cli/web_update.o \
        src/hermes_cli/update_cmd.o \
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
        src/ports/agent/anthropic_adapter.o src/agent/anthropic_adapter_ports.o \
        src/hermes_cli/plugin_manifest.o \
        src/cli/port_tools_config_helpers.o \
        src/hermes_cli/command_registry.o \
        src/hermes_cli/shared_metrics_contract.o \
        src/hermes_cli/smc_event_extractors.o \
        src/hermes_cli/relay_shared_metrics.o \
        src/hermes_cli/heartbeat.o \
        src/hermes_cli/focus_view.o \
        src/hermes_cli/slash_exec.o \
        src/hermes_cli/kanban_swarm.o \
        src/hermes_cli/skills_config.o \
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
    src/ports/tools/voice_mode.o src/tools/transcribe.o src/tools/transcribe_helpers.o lib/libtranscribe/transcribe.o \
    src/ports/tools/registry.o src/tools/tool_result.o src/gui_core.o \
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
    src/pet/pet_prompts.o \
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
    src/ports/tools/voice_mode.o src/tools/transcribe.o src/tools/transcribe_helpers.o src/ports/tools/registry.o src/tools/tool_result.o \
    lib/libtranscribe/transcribe.o \
    lib/libdb/sqlite3.o lib/libhttp/http.o lib/libjson/json.o lib/libbase64/base64.o lib/libcrypto/crypto.o
DESKTOP_GUI_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_ttf 2>/dev/null)
DESKTOP_GUI_LIBS := $(shell pkg-config --libs sdl2 SDL2_ttf 2>/dev/null) -lm -lssl -lcrypto -lz -lasound -lpthread

# Phase targets (each adds its objects)
DEPS_OBJ = src/hermes_error.o src/secrets.o src/hermes_tokenizer.o src/xai_retirement.o src/skills_hub.o src/mcp_serve.o src/api_server.o src/hermes_env_keys.o src/web_dashboard.o src/jiter_preload.o src/util_str.o lib/libwubuoffice/src/wubuzip/crc.o lib/libwubuoffice/src/wubuzip/zip.o lib/libwubuoffice/src/wubuzip/reader.o lib/libwubuoffice/src/wubuzip/bit.o lib/libwubuoffice/src/wubuzip/bitw.o lib/libwubuoffice/src/wubuzip/canon.o lib/libwubuoffice/src/wubuzip/huffman.o lib/libwubuoffice/src/wubuzip/fixed.o lib/libwubuoffice/src/wubuzip/fixedcode.o lib/libwubuoffice/src/wubuzip/limitcode.o lib/libwubuoffice/src/wubuzip/lz77.o lib/libwubuoffice/src/wubuzip/deflate.o lib/libwubuoffice/src/wubuzip/block.o lib/libwubuoffice/src/wubuzip/inflate.o lib/libwubuoffice/src/wubuoxml/package.o lib/libwubuoffice/src/wubuoxml/rels_path.o lib/libwubuoffice/src/wubuoxml/reader.o lib/libwubuoffice/src/wubuoxml/docx_text.o
AGENT_OBJ = src/agent/agent_loop.o src/ports/agent/agent_runtime_helpers.o src/agent/runtime_helpers_ports.o src/agent/agent_runtime_pure.o src/agent/agent_message_payload.o src/agent/prompt_caching_strip.o src/agent/prompt_caching_plan.o src/agent/chat_completion_helpers.o src/agent/credential_pool_engine.o src/agent/credential_pool_persistence.o src/agent/credential_pool_custom.o src/agent/credential_pool_sync.o src/agent/credential_pool_ports.o src/agent/credential_sources.o src/agent/llm_client.o src/agent/credential_persistence.o src/agent/logger.o src/agent/context.o src/agent/context_compressor_pure.o src/agent/context_engine.o src/ports/agent/coding_context.o src/agent/stream_diag.o src/agent/stream_diag_ports.o src/agent/title.o src/agent/provider.o src/agent/provider_openai.o src/agent/provider_openrouter.o src/agent/provider_deepseek.o src/agent/provider_xai.o src/agent/provider_anthropic.o src/agent/provider_google.o src/agent/provider_azure.o src/agent/provider_bedrock.o src/agent/process_bootstrap.o src/agent/provider_custom.o src/agent/provider_codex_responses.o src/agent/codex_app_server_client.o src/agent/transports/codex_app_server_session.o src/agent/codex_event_projector.o src/agent/codex_event_projector_ports.o src/ports/agent/transports/hermes_tools_mcp_server.o src/agent/hermes_tools_mcp_server_ports.o src/agent/fallback_routing.o src/agent/budget_tracker.o src/agent/provider_metadata.o src/agent/checkpoint.o src/agent/plugin_ext.o src/agent/redact.o src/agent/audit.o src/agent/sanitize.o src/agent/vault.o src/agent/think_scrubber.o src/agent/think_scrubber_helpers2.o src/ports/agent/retry_utils.o src/agent/retry_utils_ports.o src/agent/thread_scoped_output_ports.o src/agent/trajectory.o src/agent/trajectory_ports.o src/agent/portal_tags.o src/agent/portal_tags_ports.o src/ports/agent/markdown_tables.o src/agent/markdown_tables_ports.o src/agent/markdown_render.o src/agent/file_safety.o src/ports/agent/system_prompt.o src/agent/system_prompt_ports.o src/agent/skill_preprocessing.o src/agent/skill_preprocessing_ports.o src/ports/agent/tool_guardrails.o src/agent/tool_guardrails_ports.o src/agent/i18n.o src/agent/i18n_ports.o src/agent/subdir_hints.o src/agent/subdirectory_hints_ports.o src/agent/onboarding.o src/agent/image_routing.o src/agent/skill_bundles.o src/ports/agent/usage_pricing.o src/agent/lmstudio_reasoning.o src/agent/context_breakdown.o src/agent/manual_compression_feedback.o src/agent/prompt_caching.o src/agent/prompt_caching_ports.o src/ports/agent/context_references.o src/agent/context_references_ports.o src/agent/curator_backup_ports.o src/agent/plugin_llm_ports.o src/agent/context_engine_ports.o src/agent/file_safety_ports.o src/agent/conversation_loop_ports.o src/agent/credential_sources_ports.o src/agent/memory_manager_ports.o src/agent/video_gen_provider_ports.o src/agent/video_gen_registry_ports.o src/agent/codex_app_server_session_ports.o src/agent/azure_identity_adapter_ports.o src/agent/lsp_manager_ports.o src/agent/lsp_workspace_ports.o src/agent/lsp_reporter_ports.o src/agent/onepassword_ports.o src/agent/skill_bundles_ports.o src/agent/chat_completions_ports.o src/agent/skill_commands_ports.o src/agent/gemini_schema.o src/agent/moonshot_schema.o src/agent/moonshot_schema_ports.o src/ports/agent/auxiliary_client.o src/agent/auxiliary_client_helpers2.o src/agent/memory_provider_ports.o src/ports/agent/browser_provider.o src/agent/browser_provider_ports.o src/agent/browser_registry.o src/agent/browser_registry_ports.o src/agent/tool_error.o src/agent/hook_registry.o src/agent/shell_hooks.o src/agent/shell_hooks_ports.o src/agent/ssl_guard.o src/agent/ssl_guard_ports.o src/agent/web_search_registry_ports.o src/agent/web_search_provider_ports.o src/agent/nous_rate_guard.o src/agent/nous_rate_guard_ports.o src/agent/agent_gaps.o src/agent/todo_hydrate.o src/agent/file_mutation_verifier.o src/agent/api_error_summary.o src/agent/transcription_provider.o src/agent/transcription_provider_ports.o src/agent/transcription_registry_ports.o src/agent/verify_environment.o src/agent/verify_recipes.o src/agent/verification_evidence.o src/agent/curator.o src/agent/curator_ports.o src/ports/agent/plugin_llm.o src/ports/agent/tool_executor.o src/agent/tool_executor_ports.o src/agent/google_code_assist.o src/ports/agent/copilot_acp_client.o src/agent/copilot_acp_client_ports.o src/agent/copilot_acp_client_helpers2.o src/agent/azure_identity_adapter.o src/agent/agent_message_repair.o src/agent/agent_message_sanitize.o src/agent/proxy_utils.o src/ports/agent/credits_tracker.o src/agent/billing_pure.o src/ports/agent/turn_context.o src/agent/turn_retry_state.o src/agent/turn_retry_state.o src/agent/agent_init.o src/agent/init_ports.o src/agent/iteration_budget_ports.o src/agent/moa_loop_ports.o src/ports/agent/title_generator.o src/agent/title_generator_ports.o src/provider/token_exchange.o src/provider/google_oauth.o src/provider/copilot_oauth.o src/acp/server.o src/acp/edit_approval.o src/acp/events.o src/acp/permissions.o src/acp/resource.o src/tools/rate_limit.o src/tools/tool_result.o src/ports/agent/skill_commands.o src/ports/agent/memory_provider.o src/ports/agent/anthropic_adapter.o src/agent/async_utils.o src/ports/agent/background_review.o src/agent/background_review_ports.o src/agent/bedrock_adapter.o src/agent/bedrock_adapter_ports.o src/agent/codex_responses_adapter.o src/agent/codex_responses_adapter_ports.o src/agent/codex_app_server_ports.o src/ports/agent/codex_runtime.o src/ports/agent/context_compressor.o src/agent/context_compressor_micro.o src/agent/conversation_compression.o src/ports/agent/conversation_loop.o src/agent/turn_finalizer.o src/agent/gemini_cloudcode_adapter.o src/agent/gemini_native_adapter.o src/agent/gemini_native_adapter_ports.o src/agent/gemini_native_helpers2.o src/agent/redact_ports.o src/agent/image_routing_ports.o src/agent/transports_codex_ports.o src/agent/transports_codex_gaps.o src/agent/transports_anthropic_ports.o src/agent/secret_sources_cache_ports.o src/agent/transports_bedrock_ports.o src/agent/transports_base_ports.o src/agent/transports_init_ports.o src/agent/model_metadata_ports.o src/agent/usage_pricing_ports.o src/agent/conversation_compression_ports.o src/agent/chat_completion_helpers_ports.o src/agent/lsp_install_ports.o src/agent/credits_tracker_ports.o src/agent/transports_types_ports.o src/agent/image_gen_provider_ports.o src/agent/image_gen_registry_ports.o src/agent/onboarding_ports.o src/agent/message_sanitization_ports.o src/agent/secret_sources_command_ports.o src/agent/secret_sources_base_ports.o src/agent/secret_sources_registry_ports.o src/agent/lsp_cli_ports.o src/ports/agent/insights.o src/agent/insights_ports.o src/agent/message_sanitization.o src/agent/message_sanitization_pure.o src/ports/agent/model_metadata.o src/agent/models_dev.o src/agent/models_dev_ports.o src/ports/agent/prompt_builder.o src/agent/prompt_builder_ports.o src/agent/prompt_builder_guidance.o src/agent/runtime_cwd.o src/agent/runtime_cwd_ports.o src/agent/subdirectory_hints.o src/agent/antigravity_code_assist.o src/agent/port_agent_antigravity_oauth.o src/agent/port_agent_auxiliary_client.o src/agent/billing_view.o src/agent/port_agent_context_compressor.o src/agent/port_agent_conversation_loop.o src/agent/port_agent_display.o src/agent/port_agent_image_gen_provider.o src/agent/memory_manager.o src/agent/port_agent_memory_provider.o src/agent/message_content.o src/agent/message_content_ports.o src/agent/port_agent_prompt_builder.o src/agent/secret_scope.o src/agent/port_agent_skill_commands.o src/agent/port_agent_skill_utils.o src/agent/port_agent_system_prompt.o src/agent/port_agent_title_generator.o src/agent/port_agent_tool_executor.o src/agent/memory_manager_helpers.o src/agent/learn_prompt.o src/agent/estop.o src/agent/relay_runtime.o src/agent/turn_summary.o src/agent/monitoring/monitoring_emitter.o
TOOLS_OBJ = src/ports/tools/registry.o src/tools/terminal.o src/tools/terminal_tool.o src/tools/terminal_env_registry.o src/tools/file.o src/tools/sandbox.o src/tools/file_lint.o src/tools/web.o src/tools/skills.o src/tools/tool_init.o src/tools/patch.o src/tools/exec_code.o src/tools/clarify.o src/tools/memory.o src/tools/memory_storage.o src/tools/todo.o src/tools/process.o src/tools/process_registry.o src/tools/close_terminal_tool.o src/tools/send_message.o src/tools/send_message_target.o src/tools/cronjob.o src/tools/skill_mgmt.o src/tools/session_search.o src/tools/session_crud.o src/tools/tts.o src/tools/vision.o src/tools/tts_text_normalize.o \
    src/tools/vision_helpers.o \
src/tools/delegate.o src/tools/x_search.o src/tools/browser.o src/tools/browser_tool_env.o src/tools/browser_tool_platform.o src/tools/browser_tool_eval.o src/tools/browser_tool_install.o src/tools/browser_tool_path.o src/tools/browser_tool_cdp.o src/ports/tools/approval.o src/tools/port_approval.o src/tools/skills_sync_fs.o src/ports/tools/url_safety.o src/tools/url_safety_ssrf.o src/tools/web_base64_img.o src/tools/tirith.o src/ports/tools/voice_mode.o src/tools/voice_mode_pure.o src/tools/image_gen.o src/tools/image_gen_path.o src/tools/homeassistant.o src/tools/kanban_adapter.o src/tools/kanban_handlers.o src/tools/computer_use.o src/tools/result_storage.o src/tools/api_helpers.o src/tools/tool_config.o src/tools/discord.o src/ports/tools/mcp_tool.o src/tools/mcp_tool_pure.o src/tools/mcp_schema_cache.o \
src/tools/file_batch.o src/tools/file_watch.o src/tools/feishu_tools.o src/tools/feishu_comment_rules.o src/tools/file_merge.o src/tools/mixture_of_agents.o src/tools/moa_performance.o src/tools/online_research.o src/tools/video_gen.o src/tools/video_gen_registry.o src/tools/video_analyze.o src/tools/image_gen_registry.o src/tools/web_search_registry.o src/ports/tools/path_security.o src/ports/tools/xai_http.o src/tools/account_usage.o src/agent/account_usage_ports.o src/agent/billing_view_ports.o src/tools/ansi_strip.o src/tools/transcribe.o \ src/tools/skill_linter.o \
    src/tools/transcribe_helpers.o \
src/tools/wecom_crypto.o src/tools/yuanbao_tools.o src/tools/yuanbao_media.o src/tools/media_cache.o src/sandbox_escape.o src/ports/tools/env_probe.o src/tools/skills_guard.o src/tools/curator_backup.o src/tools/daytona.o src/tools/environment_gaps.o src/tools/image_gen_provider.o src/tools/tool_result_classification.o src/tools/tts_provider.o src/agent/tts_provider_ports.o src/tools/tts_registry.o src/agent/tts_registry_ports.o src/tools/video_gen_provider.o src/tools/binary_extensions.o src/tools/browser_camofox.o src/tools/managed_tool_gateway.o src/tools/microsoft_graph_auth.o src/ports/tools/microsoft_graph_client.o src/ports/tools/tool_backend_helpers.o src/tools/tool_output_limits.o src/ports/tools/tool_search.o src/ports/tools/checkpoint_manager.o src/tools/async_delegation.o src/tools/base.o src/tools/cron_prompt_sanitize.o src/tools/cronjob_tools.o src/tools/file_text_ops.o src/tools/file_ops_lint.o src/tools/file_fs_ops.o src/tools/file_pagination_ops.o src/tools/file_tools.o src/tools/file_tools_helpers2.o src/tools/code_execution_tool_ports.o src/tools/mcp_oauth_ports.o src/tools/checkpoint_manager_ports.o src/tools/read_extract.o src/tools/browser_redact.o src/tools/browser_supervisor_redact.o src/tools/skill_manager_val.o src/tools/cua_backend_helpers.o src/tools/cua_backend_helpers2.o src/tools/kanban_tools.o src/tools/kanban_tools_ports.o src/tools/transcription_tools_ports.o src/tools/env_managed_modal_ports.o src/tools/vision_analysis_native.o src/tools/browser_use_cli_gaps.o src/tools/terminal_tool_gaps.o src/tools/read_extract_gaps.o src/tools/mcp_tool_gaps.o src/tools/wake_word_gaps.o src/tools/tts_tool_gaps.o src/tools/approval_gaps.o src/gateway/server.o
GATEWAY_OBJ = src/gateway/server.o src/gateway/gw_pollers.o src/gateway/gw_notifier.o src/gateway/gw_format.o src/gateway/gw_thread.o src/gateway/gw_approval.o src/gateway/gw_logging.o src/gateway/gw_dispatch.o src/gateway/gw_session.o src/gateway/gw_setup.o src/gateway/gw_misc.o src/gateway/gateway_runtime.o src/ports/gateway/config.o src/gateway/telegram_filter.o src/ports/gateway/platforms/helpers.o src/gateway/run_pure.o src/gateway/shutdown_forensics.o src/gateway/sticker_cache.o src/gateway/mirror.o src/gateway/slash_access.o src/gateway/runtime_footer.o src/gateway/channel_directory.o src/gateway/gateway_gaps.o src/gateway/platforms/telegram.o src/gateway/platforms/telegram_network.o src/gateway/platforms/discord.o src/gateway/platforms/webhook.o src/gateway/platforms/slack.o src/gateway/platforms/matrix.o src/gateway/platforms/mattermost.o src/gateway/platforms/whatsapp.o src/gateway/platforms/email.o src/gateway/platforms/signal.o src/gateway/platforms/homeassistant.o src/gateway/platforms/sms.o src/gateway/platforms/feishu.o src/gateway/platforms/wecom.o src/gateway/platforms/wecom_callback.o src/gateway/platforms/dingtalk.o src/gateway/platforms/qqbot.o src/gateway/platforms/bluebubbles.o src/gateway/platforms/msgraph_webhook.o src/gateway/msgraph_webhook_ports.o src/gateway/platform_registry_ports.o src/gateway/platforms/msgraph_webhook_security.o src/gateway/platforms/weixin.o src/gateway/platforms/yuanbao.o src/gateway/platforms/base.o src/gateway/platforms/base_ext.o src/gateway/platforms/base_ext2.o src/gateway/platforms/base_adapter.o src/gateway/platforms/api_server_adapter.o src/gateway/platforms/api_server_adapter_handlers.o src/gateway/platforms/api_server_adapter_sessions.o src/gateway/platforms/api_server_adapter_chat.o src/gateway/platforms/api_server_adapter_responses.o src/gateway/platforms/api_server_adapter_runs.o src/gateway/platforms/api_server_adapter_cron.o src/gateway/platforms/feishu_comment_rules.o src/gateway/platforms/signal_rate_limit.o src/gateway/platforms/feishu_comment.o src/gateway/platforms/yuanbao_proto.o src/gateway/yuanbao_proto_ports.o src/gateway/platforms/yuanbao_media.o src/gateway/platforms/yuanbao_sticker.o src/gateway/gateway_lifecycle.o src/gateway/stream_events.o src/ports/gateway/delivery.o src/ports/gateway/display_config.o src/gateway/memory_monitor.o src/gateway/pairing.o src/ports/gateway/restart.o src/ports/gateway/run.o src/gateway/session.o src/gateway/session_context.o src/gateway/stream_consumer.o src/gateway/streaming_tts_consumer.o src/gateway/whatsapp_identity.o src/gateway/whatsapp_identity_ports.o src/gateway/message_timestamps.o src/gateway/platforms_api_server.o src/gateway/port_gateway_platforms_base.o src/gateway/port_platforms_base.o src/gateway/platforms_base_helpers2.o src/gateway/platforms_base_helpers3.o src/gateway/platforms_helpers_ports.o src/gateway/turn_lease_ports.o src/gateway/message_timestamps_ports.o src/gateway/qqbot_chunked_upload_ports.o src/gateway/signal_rate_limit_ports.o src/gateway/relay_adapter.o src/gateway/relay_auth.o src/gateway/relay_auth_helpers2.o src/gateway/relay_transport_ports.o src/gateway/relay_descriptor.o src/gateway/relay_transport.o src/gateway/relay_ws_transport.o src/gateway/rich_sent_store.o src/gateway/code_skew.o src/gateway/code_skew_ports.o src/gateway/port_gateway_authz_mixin.o src/gateway/restart_loop_guard.o src/gateway/dead_targets.o src/gateway/platforms/http_client_limits.o \
  src/gateway/session_state.o \
 src/gateway/port_gateway_run_gaps.o src/gateway/session_gaps.o src/gateway/platforms_base_media.o src/gateway/status.o
AGENT_PORT_NEW = src/agent/redact_helpers.o src/agent/tool_dispatch_helpers.o src/agent/verify_hooks.o src/agent/verify_runner.o src/agent/ssl_verify.o src/agent/port_agent_turn_context.o src/agent/billing_links.o src/agent/billing_usage.o src/agent/battery.o src/agent/aux_accounting.o src/agent/port_auxiliary_client.o src/agent/port_async_utils.o src/agent/hermes_state/hermes_state_open.o src/agent/hermes_state/hermes_state_lifecycle.o src/agent/hermes_state/hermes_state_messages.o src/agent/hermes_state/hermes_state_lineage.o src/agent/hermes_state/hermes_state_tokens.o src/agent/hermes_state/hermes_state_archive.o src/agent/hermes_state/hermes_state_misc.o src/agent/hermes_state/hermes_state_gaps.o src/agent/hermes_state/hermes_state_repair.o src/agent/hermes_state/hermes_state_locks.o src/agent/hermes_state/hermes_state_ports.o src/agent/iron_proxy.o src/agent/iron_proxy_ports.o src/agent/bitwarden_ports.o src/agent/secret_sources_helpers.o src/agent/lsp_servers.o src/agent/lsp_servers_ports.o src/agent/lsp_client_ports.o src/agent/lsp_eventlog.o src/agent/lsp_eventlog_ports.o src/agent/lsp_init_ports.o src/agent/conversation_compression_helpers.o src/hermes_cli/session_recovery/session_recovery_paths.o src/hermes_cli/session_recovery/session_recovery_preflight.o src/hermes_cli/session_recovery/session_recovery_copy.o src/hermes_cli/session_recovery/session_recovery_salvage.o src/hermes_cli/session_recovery/session_recovery_meta.o src/hermes_cli/session_recovery/session_recovery_orphans.o src/hermes_cli/session_recovery/session_recovery_verify.o src/hermes_cli/session_recovery/session_recovery_main.o src/agent/port_agent_background_review.o src/agent/port_antigravity_oauth.o src/agent/monitoring_health_export.o src/agent/subagent_lifecycle.o src/agent/outbound_webhooks.o src/agent/web_deps.o src/agent/port_gateway_health.o src/agent/port_cron_health.o
TOOLS_PORT_NEW = src/tools/file_operations_search.o src/tools/slash_confirm.o src/tools/interrupt.o src/hermes_cli/moa_pure_helpers.o src/hermes_cli/config_py_pure.o src/hermes_cli/config_py_io.o src/hermes_cli/config_migrations.o src/hermes_cli/update_lock.o src/hermes_cli/npm_engine.o src/hermes_cli/timefmt.o src/hermes_cli/sizefmt.o src/hermes_cli/personality.o src/hermes_cli/models_py_helpers.o src/tools/research_helpers.o src/hermes_cli/moa_config_pure.o src/hermes_cli/moa_slash.o src/hermes_cli/input_sanitize.o src/hermes_cli/utils_truthy.o src/hermes_cli/session_title.o src/hermes_cli/mem_trim.o src/hermes_cli/state_usage_db.o src/tools/toolsets.o src/hermes_cli/platform_tools.o src/tools/browser_tool.o src/tools/budget_config.o src/tools/port_clarify_tool.o src/tools/registry_cache.o src/tools/feishu_drive_tool.o src/tools/feishu_drive_tool_ports.o src/tools/feishu_doc_tool_ports.o src/tools/debug_helpers_ports.o src/tools/env_local_ports.o src/tools/terminal_tool_ports.o src/tools/terminal_hints.o src/tools/env_ssh_ports.o src/tools/mcp_oauth_manager_ports.o src/tools/microsoft_graph_client_ports.o src/tools/port_session_search_tool.o src/tools/write_approval.o src/tools/wake_word.o src/tools/flux3_video.o src/tools/flux3_video_tool.o src/tools/transcription_tools_pure.o src/tools/vercel_sandbox.o src/tools/vercel_sandbox_helpers.o src/tools/vercel_sandbox_env.o src/agent/hermes_state/hermes_state.o src/hermes_cli/startup_fast.o src/tools/self_repo_guard.o
GATEWAY_PORT_NEW = src/gateway/platforms/platforms_helpers_md.o src/hermes_cli/platforms_helpers.o src/gateway/platforms_helpers.o src/gateway/drain_control.o src/gateway/run_pure2.o src/gateway/agent_cache_pressure.o src/gateway/slash_command_guards.o src/gateway/run_deps.o src/gateway/slash_commands.o src/gateway/run_wrappers.o src/gateway/api_server_ports.o src/gateway/qqbot_wrappers.o src/gateway/yuanbao_wrappers.o src/gateway/yuanbao_ports.o src/gateway/weixin_wrappers.o src/gateway/weixin_ports.o src/gateway/signal_ports.o src/gateway/signal_helpers2.o src/gateway/api_server_ports.o src/gateway/session_helpers2.o src/gateway/relay_init_ports.o src/gateway/config_ports.o src/gateway/config_helpers2.o src/gateway/relay_ws_transport_ports.o src/gateway/session_ports.o src/gateway/session_context_ports.o src/gateway/status_ports.o src/gateway/qqbot_adapter_ports.o src/gateway/whatsapp_cloud_ports.o src/gateway/shutdown_watchdog_ports.o src/gateway/stream_dispatch_ports.o src/gateway/relay_adapter_ports.o src/gateway/relay_adapter_helpers.o src/gateway/relay_adapter_ops.o src/gateway/bluebubbles_ports.o src/gateway/systemd_notify_ports.o src/gateway/yuanbao_media_ports.o src/gateway/qqbot_keyboards_ports.o src/gateway/run_ports.o src/gateway/webhook_ports.o src/gateway/util_helpers.o src/gateway/status_wrappers.o src/gateway/bluebubbles_wrappers.o src/gateway/ports_wrappers.o src/gateway/hermes_constants_reasoning.o src/gateway/run_agent.o src/hermes_cli/provider_registry.o
AGENT_PORT_REGEN = src/agent/otlp_exporter.o src/agent/session_activity.o src/agent/relay_tools.o src/agent/monitoring_events.o src/agent/approvals_suggest_proposals.o src/hermes_cli/status_pure.o src/agent/monitoring_health_export_pure.o src/agent/backend_identity.o src/agent/reasoning_timeouts.o src/agent/replay_cleanup.o src/agent/port_agent_retry_utils.o src/agent/thinking_timeout_guidance.o src/agent/intent_ack.o src/agent/display_helpers.o src/agent/port_markdown_tables.o src/agent/auxiliary_client_helpers.o src/agent/auxiliary_client_runtime.o src/agent/relay_llm.o src/agent/port_agent_auxiliary_client_gaps.o src/agent/port_auxiliary_client_gaps.o src/agent/moa_trace_helpers.o src/agent/lsp_range_shift.o src/agent/transport_common.o src/agent/secret_common.o src/agent/lsp_protocol.o src/agent/lsp_client.o src/agent/lsp_manager.o src/agent/provider_profile.o src/agent/provider_profiles_builtin.o src/agent/provider_profile_apply.o src/agent/delegation_context.o src/agent/reactions.o src/hermes_cli/plugins_wrappers.o src/hermes_cli/plugins_cmd_wrappers.o src/hermes_cli/commands_mixin_wrappers.o src/hermes_cli/commands_wrappers.o src/hermes_cli/web_server_wrappers.o src/hermes_cli/main_wrappers.o src/hermes_cli/main_ports.o src/hermes_cli/utils_ports.o src/hermes_cli/kanban_db_ports.o src/hermes_cli/billing_mixin_ports.o src/hermes_cli/auth_helpers2.o src/hermes_cli/cli_ports.o src/hermes_cli/web_server_helpers2.o src/hermes_cli/pty_session_ports.o src/hermes_cli/plugins_ports.o src/hermes_cli/auth_wrappers.o src/hermes_cli/gateway_wrappers.o src/hermes_cli/windows_wrappers.o src/hermes_cli/setup_ports.o src/hermes_cli/kanban_db_wrappers.o src/hermes_cli/managed_uv_wrappers.o src/hermes_cli/windows_ssh_wrappers.o src/hermes_cli/model_setup_wrappers.o src/hermes_cli/nous_account_wrappers.o src/hermes_cli/session_export_wrappers.o src/hermes_cli/uninstall_wrappers.o src/hermes_cli/model_switch_wrappers.o src/hermes_cli/session_export_md_wrappers.o src/hermes_cli/nous_sub_wrappers.o src/agent/skill_utils_wrappers.o src/agent/context_compressor_wrappers.o src/agent/context_compressor_ports.o src/agent/verif_evidence_wrappers.o src/hermes_cli/ports_wrappers.o src/hermes_cli/other_ports_wrappers.o src/hermes_cli/helpers2.o src/hermes_cli/helpers3.o src/hermes_cli/config_ports.o src/agent/ports_wrappers.o src/agent/core_helpers.o src/hermes_cli/util_helpers.o src/hermes_cli/bang_shell_ports.o $(AGENT_PORT_NEW)
GATEWAY_PORT_REGEN = src/gateway/cgroup_cleanup.o src/gateway/port_display_config.o src/gateway/port_gateway_run.o src/gateway/gateway_runner.o src/gateway/run_helpers.o src/gateway/relay_media.o src/hermes_cli/scan_venv_blockers.o src/gateway/session_stall.o src/gateway/platforms/media_cache.o src/hermes_cli/mirror.o src/gateway/lifecycle_ledger.o src/gateway/sqlite_safe_read.o src/gateway/cua_doctor.o src/gateway/shutdown_flush_ports.o $(GATEWAY_PORT_NEW)
TOOLS_PORT_REGEN = src/tools/url_safety_helpers.o src/tools/port_patch_parser.o src/tools/working_diff.o src/hermes_cli/schema_sanitizer.o src/tools/slash_confirm.o src/hermes_cli/migrate.o src/tools/skills_sync.o src/tools/skills_sync_ports.o src/hermes_cli/pet_input.o src/tools/browser_tool_helpers.o src/tools/web_tools.o src/tools/skill_prereqs.o src/tools/skill_usage_wrappers.o src/tools/transcription_wrappers.o src/tools/vision_wrappers.o src/tools/file_tools_wrappers.o src/tools/cua_backend_wrappers.o src/tools/cua_backend_methods.o src/tools/env_local_wrappers.o src/tools/mcp_oauth_wrappers.o src/tools/async_delegation_wrappers.o src/tools/terminal_tool_wrappers.o src/tools/env_base_wrappers.o src/tools/env_docker_wrappers.o src/tools/skill_mgr_tool_wrappers.o src/tools/approval_wrappers.o src/tools/ports_wrappers.o src/tools/skills_tool.o src/tools/skills_tool_ports.o src/tools/file_state_ports.o src/tools/online_research_ports.o src/tools/util_helpers.o src/tools/skills_sync_client.o src/tools/computer_use_doctor_mcp.o $(TOOLS_PORT_NEW)
PHASE2_OBJ += src/hermes_cli/plugins_wrappers.o src/hermes_cli/plugins_cmd_wrappers.o src/hermes_cli/commands_mixin_wrappers.o src/hermes_cli/commands_wrappers.o src/hermes_cli/web_server_wrappers.o src/hermes_cli/main_wrappers.o src/hermes_cli/auth_wrappers.o src/hermes_cli/gateway_wrappers.o src/hermes_cli/windows_wrappers.o src/hermes_cli/kanban_db_wrappers.o src/hermes_cli/managed_uv_wrappers.o src/hermes_cli/windows_ssh_wrappers.o src/agent/moa_loop_wrappers.o src/agent/memory_manager_wrappers.o src/hermes_cli/model_setup_wrappers.o src/hermes_cli/nous_account_wrappers.o src/hermes_cli/session_export_wrappers.o src/hermes_cli/uninstall_wrappers.o src/hermes_cli/model_switch_wrappers.o src/hermes_cli/session_export_md_wrappers.o src/hermes_cli/nous_sub_wrappers.o src/agent/skill_utils_wrappers.o src/agent/context_compressor_wrappers.o src/agent/verif_evidence_wrappers.o src/hermes_cli/ports_wrappers.o src/hermes_cli/other_ports_wrappers.o src/agent/ports_wrappers.o $(AGENT_PORT_NEW) $(AGENT_PORT_REGEN)
PHASE3_OBJ += $(TOOLS_PORT_NEW) $(TOOLS_PORT_REGEN)
PHASE4_OBJ += $(GATEWAY_PORT_NEW) $(GATEWAY_PORT_REGEN)
CRON_OBJ = src/cron/scheduler.o src/cron/jobs.o src/cron/cron_extras.o src/cron/cron_sqlite.o src/cron/scheduler_provider_ports.o src/cron/scheduler_ports.o src/cron/cron_cli.o src/cron/scheduler_provider.o src/cron/scheduler_helpers.o src/cron/scheduler_delivery.o src/cron/scheduler_runtime.o src/cron/scheduler_runtime_impl.o src/cron/scheduler_toolsets.o src/cron/scheduler_script.o src/cron/scheduler_prompt.o src/cron/port_scheduler.o src/cron/cron_suggestions.o src/cron/suggestion_catalog.o src/cron/port_jobs.o src/cron/lifecycle_guard.o src/cron/port_cron_jobs.o src/cron/classify_items.o src/cron/notepad.o src/cron/monitor.o \

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
PHASE5_OBJ = $(PHASE4_OBJ) $(CRON_OBJ) $(AGENT_PORT_REGEN) $(TOOLS_PORT_REGEN) $(GATEWAY_PORT_REGEN) src/hermes_cli/turn_summary_cli.o src/hermes_cli/parity_gaps.o src/hermes_cli/pure_gaps.o src/tools/file_operations_gaps.o src/agent/markdown_tables_gaps.o src/hermes_cli/config_gaps.o src/hermes_cli/port_service_manager_gaps.o src/agent/context_breakdown_gaps.o src/agent/models_dev_gaps.o src/agent/insights_gaps.o src/agent/small_mods_gaps.o src/agent/hermes_state_gaps.o src/agent/multi_mod_gaps.o src/agent/truly_missing_gaps.o src/agent/final_missing_gaps.o
