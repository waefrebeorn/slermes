# ── Object File Lists ──────────────────────────────────────────────
# build/objects.mk -- cupboards and jars.
#
# Each CUPBOARD_* variable holds one subsystem's object files (the jars).
# Add a new object to its subsystem's cupboard, never to a global list.
# CLI_OBJ and PORT_OBJ are concatenations of their cupboards.
#
# Regenerate:  python scripts/organize_objects.py
# Verify only: python scripts/organize_objects.py --check
# Included by the top-level Makefile.


# ── CLI_OBJ ──
CUPBOARD_ORPHANS = src/port_missing_impl.o src/acp/acp_protocol.o \
    src/acp/events.o \
    src/acp/permissions.o \
    src/acp/resource.o \
    src/acp/server.o \
    src/acp_adapter/acp_adapter_permissions.o \
    src/acp_adapter/gateway_session_meta.o \
    src/agent/account_usage_ports.o \
    src/agent/agent_init_flow.o \
    src/agent/agent_init_gaps3.o \
    src/agent/anthropic_adapter_gaps3.o \
    src/agent/anthropic_thinking_replay.o \
    src/agent/antigravity_code_assist.o \
    src/agent/antigravity_oauth.o \
    src/agent/background_review.o \
    src/agent/bedrock_adapter.o \
    src/agent/billing_view.o \
    src/agent/chat_completion_helpers_ports.o \
    src/agent/chat_completion_helpers_tool_calls.o \
    src/agent/chat_completion_nonstream.o \
    src/agent/checkpoints/__init__.o \
    src/agent/checkpoints/checkpoint_store.o \
    src/agent/checkpoints/maintenance.o \
    src/agent/context_compressor.o \
    src/agent/context_compressor_sampling.o \
    src/agent/context_compressor_tail_flow2.o \
    src/agent/context_references_ports.o \
    src/agent/conversation_compression.o \
    src/agent/conversation_compression_archive.o \
    src/agent/conversation_compression_checkpoint.o \
    src/agent/conversation_compression_feasibility.o \
    src/agent/conversation_compression_ports.o \
    src/agent/conversation_compression_store.o \
    src/agent/conversation_compression_telemetry.o \
    src/agent/copilot_acp_client_gaps3.o \
    src/agent/credential_persistence_gaps3.o \
    src/agent/credential_remove.o \
    src/agent/curator_backup_gaps3.o \
    src/agent/curator_backup_ports.o \
    src/agent/deadline_gaps3.o \
    src/agent/display.o \
    src/agent/error_classifier_gaps3.o \
    src/agent/errors.o \
    src/agent/estop_gaps3.o \
    src/agent/fallback_cooldown_flow.o \
    src/agent/fallback_routing.o \
    src/agent/gemini_cloudcode_adapter.o \
    src/agent/gemini_native_adapter.o \
    src/agent/gemini_native_adapter_gaps.o \
    src/agent/gemini_native_adapter_ports.o \
    src/agent/google_code_assist.o \
    src/agent/i18n.o \
    src/agent/image_routing_gaps3.o \
    src/agent/insights_fmt.o \
    src/agent/kanban_turn_recovery_flow2.o \
    src/agent/llm_stream.o \
    src/agent/lmstudio_reasoning.o \
    src/agent/lmstudio_reasoning_py.o \
    src/agent/lsp/__init__.o \
    src/agent/lsp/cli.o \
    src/agent/lsp/deferred.o \
    src/agent/lsp/eventlog.o \
    src/agent/lsp/install.o \
    src/agent/lsp/lsp_reporter_ports.o \
    src/agent/lsp/lsp_workspace_ports.o \
    src/agent/lsp/manager_gaps3.o \
    src/agent/lsp/workspace.o \
    src/agent/manual_compression_feedback.o \
    src/agent/markdown_tables.o \
    src/agent/markdown_tables_ports.o \
    src/agent/memory_manager.o \
    src/agent/memory_provider_ports.o \
    src/agent/message_content.o \
    src/agent/message_content_ports.o \
    src/agent/message_sanitization_gaps.o \
    src/agent/moa_loop_gaps3.o \
    src/agent/models_dev_info.o \
    src/agent/monitoring/__init__.o \
    src/agent/monitoring/events.o \
    src/agent/monitoring/gateway_health.o \
    src/agent/monitoring/gateway_health_export.o \
    src/agent/periodic_scheduler_gaps3.o \
    src/agent/pet/__init__.o \
    src/agent/pet/generate/__init__.o \
    src/agent/pet/generate/atlas.o \
    src/agent/pet/store.o \
    src/agent/plugin_llm_gaps3.o \
    src/agent/port_agent_oneshot.o \
    src/agent/port_turn_summary.o \
    src/agent/portal_tags_ports.o \
    src/agent/process_bootstrap.o \
    src/agent/prompt_cache_lifecycle/__init__.o \
    src/agent/prompt_cache_lifecycle/edge.o \
    src/agent/prompt_cache_lifecycle/tier.o \
    src/agent/prompt_cache_lifecycle/warmer.o \
    src/agent/prompt_caching_ports.o \
    src/agent/provider_bedrock_sign.o \
    src/agent/provider_endpoint_detect.o \
    src/agent/provider_model_caps.o \
    src/agent/proxy_sources/__init__.o \
    src/agent/reasoning_effort.o \
    src/agent/redact_match.o \
    src/agent/runtime_session_lifecycle.o \
    src/agent/secret_sources/__init__.o \
    src/agent/secret_sources/base.o \
    src/agent/session_activity.o \
    src/agent/skill_commands_ports.o \
    src/agent/skill_preprocessing.o \
    src/agent/skill_utils.o \
    src/agent/subdirectory_hints.o \
    src/agent/terminal_env_provider.o \
    src/agent/test_omarchy_usage_main.o \
    src/agent/threat_patterns.o \
    src/agent/tool_dispatch_flow2.o \
    src/agent/tool_dispatch_helpers.o \
    src/agent/tool_error.o \
    src/agent/tool_executor_ports.o \
    src/agent/transcription_provider.o \
    src/agent/transcription_registry_ports.o \
    src/agent/transports/__init__.o \
    src/agent/transports/base.o \
    src/agent/transports/bedrock_transport_flow.o \
    src/agent/transports/codex_app_server_ports.o \
    src/agent/transports/codex_app_server_session.o \
    src/agent/transports/codex_app_server_session_ports.o \
    src/agent/turn_context.o \
    src/agent/turn_context_gaps.o \
    src/agent/usage_pricing_flow.o \
    src/agent/vault_backends/__init__.o \
    src/agent/vault_backends/base.o \
    src/agent/vault_backends/browser.o \
    src/agent/vault_backends/unlock.o \
    src/agent/verification_evidence.o \
    src/agent/verify/__init__.o \
    src/agent/verify/recipes.o \
    src/agent/verify/verify_runner.o \
    src/agent/web_search_provider_ports.o \
    src/api_server_http.o \
    src/api_server_readonly.o \
    src/api_server_sse.o \
    src/app_desktop.o \
    src/app_state.o \
    src/chat_composer.o \
    src/chat_view.o \
    src/cli/cli_gaps.o \
    src/cli/commands_flow.o \
    src/cli/config_setup_wizard.o \
    src/cli/display_diff.o \
    src/cli/gateway_windows_flow3.o \
    src/cli/kanban_workers_db.o \
    src/cli/port_outbound_webhooks.o \
    src/cli/port_run_adapters_flow.o \
    src/cli/port_web_git_flow.o \
    src/cli/session_search_tool.o \
    src/cli/setup_wizard_select.o \
    src/cli/setup_wizard_ui.o \
    src/cli/tui_config_editor.o \
    src/cli/tui_fullscreen.o \
    src/cli/tui_image_viewer.o \
    src/cli/tui_input.o \
    src/cli/tui_model_picker.o \
    src/cli/tui_plugin_hub.o \
    src/cli/tui_session_browser.o \
    src/cli/tui_slash_cmds.o \
    src/cli/tui_stream.o \
    src/cli/tui_todo_panel.o \
    src/cli/tui_views.o \
    src/cli/tui_win.o \
    src/cli/turn_exit_flow.o \
    src/cli/web_server_flow2.o \
    src/cron/blueprint_catalog.o \
    src/cron/cron_jobs_axes.o \
    src/cron/cron_jobs_fields.o \
    src/cron/cron_jobs_paths.o \
    src/cron/cron_jobs_sched.o \
    src/cron/cron_jobs_time2.o \
    src/cron/cron_scripts.o \
    src/cron/port_cron_jobs.o \
    src/cron/port_cron_jobs_helpers.o \
    src/cron/scripts/__init__.o \
    src/desktop_app.o \
    src/desktop_controller.o \
    src/tools/registry_init_all.o \
    src/tools/desktop_tools.o \
    src/desktop_gui.o \
    src/desktop_input.o \
    src/desktop_pty.o \
    src/desktop_ui_chrome.o \
    src/desktop_ui_layout.o \
    src/desktop_ui_lifecycle.o \
    src/desktop_ui_overlays.o \
    src/event_handling.o \
    src/gateway/agent_cache_pressure.o \
    src/gateway/browser_control_broker.o \
    src/gateway/channel_directory.o \
    src/gateway/code_skew.o \
    src/gateway/config_ports.o \
    src/gateway/control_socket.o \
    src/gateway/delivery.o \
    src/gateway/gateway_gaps.o \
    src/gateway/gateway_status_io.o \
    src/gateway/gateway_status_proc.o \
    src/gateway/hosted_room_work_records.o \
    src/gateway/hosted_room_work_storage.o \
    src/gateway/message_timestamps.o \
    src/gateway/outbound/__init__.o \
    src/gateway/outbound/senders.o \
    src/gateway/pairing.o \
    src/gateway/platforms/__init__.o \
    src/gateway/platforms/api_server_adapter_chat.o \
    src/gateway/platforms/api_server_adapter_cron.o \
    src/gateway/platforms/api_server_adapter_gaps.o \
    src/gateway/platforms/api_server_adapter_responses.o \
    src/gateway/platforms/api_server_adapter_runs.o \
    src/gateway/platforms/api_server_adapter_sessions.o \
    src/gateway/platforms/api_server_authority_runs.o \
    src/gateway/platforms/api_server_bot_chat.o \
    src/gateway/platforms/api_server_flow2.o \
    src/gateway/platforms/api_server_memory_sessions.o \
    src/gateway/platforms/api_server_ports.o \
    src/gateway/platforms/api_server_profile_prefix.o \
    src/gateway/platforms/api_server_response_admissions.o \
    src/gateway/platforms/base_exec_approval.o \
    src/gateway/platforms/base_log_safety.o \
    src/gateway/platforms/discord_adapter_flow2.o \
    src/gateway/platforms/docker_media_paths.o \
    src/gateway/platforms/feishu_comment.o \
    src/gateway/platforms/feishu_comment_rules.o \
    src/gateway/platforms/irc_adapter_flow2.o \
    src/gateway/platforms/platforms_helpers.o \
    src/gateway/platforms/shared_ingress.o \
    src/gateway/platforms/signal_adapter_flow2.o \
    src/gateway/platforms/signal_rate_limit.o \
    src/gateway/platforms/telegram_network.o \
    src/gateway/platforms/webhook_http.o \
    src/gateway/platforms/webhook_ingress.o \
    src/gateway/platforms/webhook_mcp_events.o \
    src/gateway/platforms/yuanbao_media.o \
    src/gateway/platforms/yuanbao_sticker.o \
    src/gateway/platforms_api_server.o \
    src/gateway/port_gateway_run.o \
    src/gateway/relay/__init__.o \
    src/gateway/relay/adapter_discord_interaction.o \
    src/gateway/relay/link.o \
    src/gateway/relay/transport.o \
    src/gateway/relay_adapter_flow2.o \
    src/gateway/run_inbound_transcription.o \
    src/gateway/run_shutdown_session_end.o \
    src/gateway/run_turn_runner_clarify_delivery.o \
    src/gateway/run_wrappers.o \
    src/gateway/session_authority_flow2.o \
    src/gateway/session_gaps.o \
    src/gateway/session_hosted_attachments.o \
    src/gateway/session_hosted_member_homes.o \
    src/gateway/session_hosted_transport.o \
    src/gateway/session_ingress_context.o \
    src/gateway/session_kanban_flow3.o \
    src/gateway/session_local_inventory.o \
    src/gateway/session_local_migration.o \
    src/gateway/session_mutation_compress.o \
    src/gateway/session_pending_controls.o \
    src/gateway/session_policy_credentials.o \
    src/gateway/session_policy_flow3.o \
    src/gateway/session_producer_lifecycle.o \
    src/gateway/session_queued_controls.o \
    src/gateway/session_runtime_workers.o \
    src/gateway/session_server_requests.o \
    src/gateway/session_setting_commands.o \
    src/gateway/session_settlement_recovery.o \
    src/gateway/session_transcript_flow.o \
    src/gateway/session_viewer_automation.o \
    src/gateway/session_worker_construct.o \
    src/gateway/shutdown_forensics.o \
    src/gateway/status_phrases.o \
    src/gateway/stream_consumer_ceiling.o \
    src/gateway/stream_dispatch_flow2.o \
    src/gateway/stream_events.o \
    src/gateway/tui_methods_misc_flow.o \
    src/gateway/tui_methods_plugins_flow.o \
    src/gateway/whatsapp_identity.o \
    src/gateway_client.o \
    src/hermes_agent/compat/__init__.o \
    src/hermes_cli/api_server.o \
    src/hermes_cli/auth_oauth_pkce_plugin.o \
    src/hermes_cli/auth_plugin_providers.o \
    src/hermes_cli/cli_commands_session_tools.o \
    src/hermes_cli/cli_process_notifications.o \
    src/hermes_cli/commands.o \
    src/hermes_cli/config_credential_values.o \
    src/hermes_cli/config_migrations.o \
    src/hermes_cli/config_migrations_browser.o \
    src/hermes_cli/config_migrations_gaps.o \
    src/hermes_cli/dashboard_auth/__init__.o \
    src/hermes_cli/dashboard_auth/body_limit.o \
    src/hermes_cli/dashboard_auth/native_http.o \
    src/hermes_cli/dashboard_auth/registry.o \
    src/hermes_cli/dashboard_auth/request_utils.o \
    src/hermes_cli/desktop_plugin_halves.o \
    src/hermes_cli/desktop_plugin_install.o \
    src/hermes_cli/desktop_release_holders.o \
    src/hermes_cli/gateway_chat_commands.o \
    src/hermes_cli/gateway_connections_normalize.o \
    src/hermes_cli/gateway_connections_oauth.o \
    src/hermes_cli/gateway_connections_reach.o \
    src/hermes_cli/gateway_connections_ssh.o \
    src/hermes_cli/gateway_connections_ssh_remote.o \
    src/hermes_cli/gateway_connections_ssh_transport.o \
    src/hermes_cli/gateway_gaps2.o \
    src/hermes_cli/gateway_migrate_backends.o \
    src/hermes_cli/gateway_migrate_guards.o \
    src/hermes_cli/gateway_multiplex_mode.o \
    src/hermes_cli/gateway_multiplex_s6.o \
    src/hermes_cli/gateway_owner_restart.o \
    src/hermes_cli/gateway_plugin_commands.o \
    src/hermes_cli/gateway_profile_lifecycle.o \
    src/hermes_cli/gateway_runtime_discovery.o \
    src/hermes_cli/gateway_runtime_replace.o \
    src/hermes_cli/gateway_runtime_service.o \
    src/hermes_cli/gateway_runtime_service_identity.o \
    src/hermes_cli/gateway_service_owner.o \
    src/hermes_cli/gateway_setup_wizard.o \
    src/hermes_cli/gateway_tools_notify.o \
    src/hermes_cli/gateway_wrappers.o \
    src/hermes_cli/input_sanitize.o \
    src/hermes_cli/kanban_owner_recovery.o \
    src/hermes_cli/kanban_worker_client.o \
    src/hermes_cli/local_runtime/__init__.o \
    src/hermes_cli/local_runtime/recovery.o \
    src/hermes_cli/main_desktop_apparmor.o \
    src/hermes_cli/main_ports.o \
    src/hermes_cli/main_wrappers.o \
    src/hermes_cli/managed_uv_wrappers.o \
    src/hermes_cli/memory_provider_migration.o \
    src/hermes_cli/mirror.o \
    src/hermes_cli/moa_pure_helpers.o \
    src/hermes_cli/model_setup_flows_common.o \
    src/hermes_cli/model_setup_flows_custom.o \
    src/hermes_cli/model_setup_flows_local.o \
    src/hermes_cli/models_py_helpers.o \
    src/hermes_cli/models_reasoning_caps.o \
    src/hermes_cli/observability/__init__.o \
    src/hermes_cli/observability/observability_relay_runtime.o \
    src/hermes_cli/observability/port_shared_metrics_contract.o \
    src/hermes_cli/observability/relay_shared_metrics_gaps.o \
    src/hermes_cli/observability/shared_metrics_busy.o \
    src/hermes_cli/observability/shared_metrics_catalog.o \
    src/hermes_cli/observability/shared_metrics_connectors.o \
    src/hermes_cli/observability/shared_metrics_consent.o \
    src/hermes_cli/observability/shared_metrics_contract.o \
    src/hermes_cli/observability/shared_metrics_desktop.o \
    src/hermes_cli/observability/shared_metrics_disabled.o \
    src/hermes_cli/observability/shared_metrics_efficiency.o \
    src/hermes_cli/observability/shared_metrics_engagement.o \
    src/hermes_cli/observability/shared_metrics_events.o \
    src/hermes_cli/observability/shared_metrics_fields.o \
    src/hermes_cli/observability/shared_metrics_gateway.o \
    src/hermes_cli/observability/shared_metrics_harness.o \
    src/hermes_cli/observability/shared_metrics_install.o \
    src/hermes_cli/observability/shared_metrics_install_run.o \
    src/hermes_cli/observability/shared_metrics_loop.o \
    src/hermes_cli/observability/shared_metrics_model.o \
    src/hermes_cli/observability/shared_metrics_process.o \
    src/hermes_cli/observability/shared_metrics_setup.o \
    src/hermes_cli/observability/shared_metrics_signals.o \
    src/hermes_cli/observability/shared_metrics_snapshot.o \
    src/hermes_cli/observability/shared_metrics_startup.o \
    src/hermes_cli/observability/shared_metrics_update.o \
    src/hermes_cli/plugin_catalog_presence.o \
    src/hermes_cli/plugin_host_profiles.o \
    src/hermes_cli/plugin_install_phase.o \
    src/hermes_cli/plugin_provider_requests.o \
    src/hermes_cli/plugin_validate_desktop.o \
    src/hermes_cli/plugin_validate_private_imports.o \
    src/hermes_cli/plugins_capability_ctx.o \
    src/hermes_cli/plugins_cmd_gaps.o \
    src/hermes_cli/port_auth_helpers.o \
    src/hermes_cli/port_gateway.o \
    src/hermes_cli/port_hermes_cli_write_approval_commands.o \
    src/hermes_cli/port_mcp_security.o \
    src/hermes_cli/port_models_pure.o \
    src/hermes_cli/port_nous_account.o \
    src/hermes_cli/port_service_manager.o \
    src/hermes_cli/profile_provision_gateway.o \
    src/hermes_cli/profiles_gap.o \
    src/hermes_cli/proxy/__init__.o \
    src/hermes_cli/proxy/adapters/__init__.o \
    src/hermes_cli/proxy/adapters/base.o \
    src/hermes_cli/pt_input_extras_gaps.o \
    src/hermes_cli/relay_plugin_migrate.o \
    src/hermes_cli/subcommands/__init__.o \
    src/hermes_cli/terminal_notify_gaps.o \
    src/hermes_cli/tools_config_post_setup.o \
    src/hermes_cli/tui_entry.o \
    src/hermes_cli/tui_eventpub.o \
    src/hermes_cli/tui_json_rpc.o \
    src/hermes_cli/tui_layout.o \
    src/hermes_cli/tui_render.o \
    src/hermes_cli/tui_slash_worker.o \
    src/hermes_cli/tui_websocket.o \
    src/hermes_cli/update_cmd_drain_report.o \
    src/hermes_cli/update_cmd_force_stop.o \
    src/hermes_cli/update_downgrade_guard.o \
    src/hermes_cli/update_host_obligation.o \
    src/hermes_cli/update_serve_obligations.o \
    src/hermes_cli/web_git_branch.o \
    src/hermes_cli/web_routers/__init__.o \
    src/hermes_cli/web_routers/_common.o \
    src/hermes_cli/web_routers/actions.o \
    src/hermes_cli/web_routers/analytics.o \
    src/hermes_cli/web_routers/chat_ws_errors.o \
    src/hermes_cli/web_routers/dashboard_ui.o \
    src/hermes_cli/web_routers/delegation.o \
    src/hermes_cli/web_routers/desktop_plugins.o \
    src/hermes_cli/web_routers/gateway_connections.o \
    src/hermes_cli/web_routers/gateway_connections_reach.o \
    src/hermes_cli/web_routers/models.o \
    src/hermes_cli/web_routers/peer.o \
    src/hermes_cli/web_routers/transfer.o \
    src/hermes_cli/web_routers/webapp.o \
    src/hermes_cli/web_routers/webapp_access.o \
    src/hermes_cli/web_routers/webapp_relay_proxy.o \
    src/hermes_cli/web_server_file_response.o \
    src/hermes_cli/web_server_messaging.o \
    src/hermes_cli/web_server_plugin_profiles.o \
    src/hermes_cli/web_server_profiles.o \
    src/hermes_cli/web_server_wrappers.o \
    src/hermes_cli/web_session_sockets.o \
    src/hermes_cli/ws_revive1.o \
    src/hermes_cli/ws_revive2.o \
    src/hermes_cli/ws_revive3.o \
    src/hermes_cli/ws_revive4.o \
    src/hermes_platform/host/__init__.o \
    src/hermes_platform/host/facts.o \
    src/hermes_platform/host/summary.o \
    src/hermes_platform/resolver/__init__.o \
    src/hermes_platform/resolver/app.o \
    src/hermes_platform/resolver/base.o \
    src/hermes_platform/resolver/known_dirs.o \
    src/jiter_preload.o \
    src/lua/slv_ui.o \
    src/lv/slv_arena.o \
    src/lv/slv_eval.o \
    src/lv/slv_host.o \
    src/lv/slv_lex.o \
    src/lv/slv_parse.o \
    src/lv/slv_stdlib.o \
    src/lv/slv_string.o \
    src/lv/slv_value.o \
    src/main.o \
    src/main_desktop.o \
    src/nix/tests/agent-references.o \
    src/nix/tests/desktop-backend.o \
    src/persistence/authority/__init__.o \
    src/persistence/authority/bindings.o \
    src/persistence/authority/coordination.o \
    src/persistence/conversation/__init__.o \
    src/persistence/conversation/lineage.o \
    src/persistence/conversation/messages.o \
    src/persistence/conversation/sessions.o \
    src/persistence/conversation/usage.o \
    src/persistence/core/__init__.o \
    src/persistence/core/backend.o \
    src/persistence/core/capabilities.o \
    src/persistence/core/fences.o \
    src/persistence/core/identity.o \
    src/persistence/gateway/__init__.o \
    src/persistence/housekeeping/__init__.o \
    src/persistence/housekeeping/state_meta.o \
    src/persistence/sqlite/__init__.o \
    src/persistence/sqlite/_registry.o \
    src/persistence/sqlite/_schema_ddl.o \
    src/persistence/sqlite/_state/__init__.o \
    src/persistence/sqlite/_state/common.o \
    src/persistence/sqlite/_state/guard.o \
    src/persistence/sqlite/_state/keys.o \
    src/persistence/sqlite/_state/lockguard.o \
    src/persistence/sqlite/admin/__init__.o \
    src/persistence/sqlite/admin/_base.o \
    src/persistence/sqlite/admin/store.o \
    src/persistence/sqlite/authority/_admission_media.o \
    src/persistence/sqlite/authority/_admission_owner.o \
    src/persistence/sqlite/authority/_admission_store.o \
    src/persistence/sqlite/authority/_binding_history.o \
    src/persistence/sqlite/authority/_local.o \
    src/persistence/sqlite/authority/_runtime.o \
    src/persistence/sqlite/authority/_runtime_ends.o \
    src/persistence/sqlite/authority/_runtime_guards.o \
    src/persistence/sqlite/authority/_runtime_picks.o \
    src/persistence/sqlite/conversation/__init__.o \
    src/persistence/sqlite/conversation/_store/__init__.o \
    src/persistence/sqlite/conversation/_store/core.o \
    src/persistence/sqlite/conversation/_store/identity.o \
    src/persistence/sqlite/conversation/_store/legacy_refusal.o \
    src/persistence/sqlite/conversation/_store/local.o \
    src/persistence/sqlite/conversation/_store/portability.o \
    src/persistence/sqlite/conversation/_store/retirement.o \
    src/persistence/sqlite/conversation/_store/rules.o \
    src/persistence/sqlite/conversation/_store/sessions.o \
    src/persistence/sqlite/conversation/_store/sql_common.o \
    src/persistence/sqlite/conversation/_store/stored_reads.o \
    src/persistence/sqlite/conversation/_store/terminal.o \
    src/persistence/sqlite/conversation/_store/titles.o \
    src/persistence/sqlite/conversation/_store/transcript_repair.o \
    src/persistence/sqlite/conversation/_store/usage.o \
    src/persistence/sqlite/conversation/lineage.o \
    src/persistence/sqlite/gateway/_hosted_store.o \
    src/persistence/sqlite/gateway/hosted_rooms.o \
    src/persistence/sqlite/housekeeping/state_meta.o \
    src/persistence/sqlite/workers/_compression.o \
    src/persistence/sqlite/workers/_gateway.o \
    src/persistence/sqlite/workers/_lifecycle.o \
    src/persistence/sqlite/workers/_runtime_workers.o \
    src/persistence/workers/__init__.o \
    src/persistence/workers/compression.o \
    src/persistence/workers/compression_reads.o \
    src/persistence/workers/executions.o \
    src/persistence/workers/sessions.o \
    src/pet/atlas.o \
    src/pet_ui.o \
    src/plugins/browser/browser_use/__init__.o \
    src/plugins/browser/browserbase/__init__.o \
    src/plugins/browser/firecrawl/__init__.o \
    src/plugins/computer_use/__init__.o \
    src/plugins/computer_use/cua/__init__.o \
    src/plugins/config_backends/__init__.o \
    src/plugins/config_backends/remote/__init__.o \
    src/plugins/config_backends/remote/diff.o \
    src/plugins/context_engine/__init__.o \
    src/plugins/cron_providers/__init__.o \
    src/plugins/cron_providers/chronos/__init__.o \
    src/plugins/dashboard_auth/_shared.o \
    src/plugins/dashboard_auth/basic/__init__.o \
    src/plugins/dashboard_auth/drain/__init__.o \
    src/plugins/dashboard_auth/nous/__init__.o \
    src/plugins/dashboard_auth/self_hosted/__init__.o \
    src/plugins/dashboard_auth/session_token/__init__.o \
    src/plugins/delegation/default/__init__.o \
    src/plugins/disk-cleanup/__init__.o \
    src/plugins/disk-cleanup/disk_cleanup.o \
    src/plugins/google_meet/__init__.o \
    src/plugins/google_meet/cli.o \
    src/plugins/google_meet/meet_bot.o \
    src/plugins/google_meet/node/__init__.o \
    src/plugins/google_meet/node/client.o \
    src/plugins/google_meet/process_manager.o \
    src/plugins/google_meet/realtime/__init__.o \
    src/plugins/google_meet/tools.o \
    src/plugins/hermes-achievements/dashboard/plugin_api.o \
    src/plugins/hermes-achievements/tests/test_achievement_engine.o \
    src/plugins/image_gen/_common.o \
    src/plugins/image_gen/deepinfra/__init__.o \
    src/plugins/image_gen/fal/__init__.o \
    src/plugins/image_gen/krea/__init__.o \
    src/plugins/image_gen/meta-ai/__init__.o \
    src/plugins/image_gen/openai-codex/__init__.o \
    src/plugins/image_gen/openai/__init__.o \
    src/plugins/image_gen/openrouter/__init__.o \
    src/plugins/image_gen/xai/__init__.o \
    src/plugins/memory/__init__.o \
    src/plugins/memory/byterover/__init__.o \
    src/plugins/memory/config_schema.o \
    src/plugins/memory/retaindb/__init__.o \
    src/plugins/model-providers/ai-gateway/__init__.o \
    src/plugins/model-providers/alibaba-coding-plan/__init__.o \
    src/plugins/model-providers/alibaba/__init__.o \
    src/plugins/model-providers/anthropic/__init__.o \
    src/plugins/model-providers/arcee/__init__.o \
    src/plugins/model-providers/azure-foundry/__init__.o \
    src/plugins/model-providers/bedrock/__init__.o \
    src/plugins/model-providers/custom/__init__.o \
    src/plugins/model-providers/deepinfra/__init__.o \
    src/plugins/model-providers/fireworks/__init__.o \
    src/plugins/model-providers/gemini/__init__.o \
    src/plugins/model-providers/gmi/__init__.o \
    src/plugins/model-providers/huggingface/__init__.o \
    src/plugins/model-providers/kilocode/__init__.o \
    src/plugins/model-providers/kimi-coding/__init__.o \
    src/plugins/model-providers/lmstudio/__init__.o \
    src/plugins/model-providers/meta-ai/__init__.o \
    src/plugins/model-providers/minimax/__init__.o \
    src/plugins/model-providers/nous/__init__.o \
    src/plugins/model-providers/novita/__init__.o \
    src/plugins/model-providers/nvidia/__init__.o \
    src/plugins/model-providers/ollama-cloud/__init__.o \
    src/plugins/model-providers/openai-codex/__init__.o \
    src/plugins/model-providers/opencode-zen/__init__.o \
    src/plugins/model-providers/openrouter/__init__.o \
    src/plugins/model-providers/qwen-oauth/__init__.o \
    src/plugins/model-providers/router/__init__.o \
    src/plugins/model-providers/solstice/__init__.o \
    src/plugins/model-providers/solstice/transport.o \
    src/plugins/model-providers/stepfun/__init__.o \
    src/plugins/model-providers/tencent-tokenhub/__init__.o \
    src/plugins/model-providers/upstage/__init__.o \
    src/plugins/model-providers/vertex/__init__.o \
    src/plugins/model-providers/xai/__init__.o \
    src/plugins/model-providers/xiaomi/__init__.o \
    src/plugins/model-providers/zai/__init__.o \
    src/plugins/platforms/a2a/__init__.o \
    src/plugins/platforms/a2a/protocol.o \
    src/plugins/platforms/a2a/security.o \
    src/plugins/platforms/a2a/tools.o \
    src/plugins/platforms/bluebubbles/__init__.o \
    src/plugins/platforms/bluebubbles/adapter.o \
    src/plugins/platforms/buzz/__init__.o \
    src/plugins/platforms/buzz/nostr_auth.o \
    src/plugins/platforms/buzz/send_targets.o \
    src/plugins/platforms/dingtalk/__init__.o \
    src/plugins/platforms/dingtalk/inbound.o \
    src/plugins/platforms/discord/__init__.o \
    src/plugins/platforms/discord/adapter_reactions.o \
    src/plugins/platforms/discord/adapter_slash_auth.o \
    src/plugins/platforms/discord/adapter_voice_info.o \
    src/plugins/platforms/discord/onboarding.o \
    src/plugins/platforms/discord/send_targets.o \
    src/plugins/platforms/email/__init__.o \
    src/plugins/platforms/feishu/__init__.o \
    src/plugins/platforms/feishu/adapter.o \
    src/plugins/platforms/feishu/adapter_synthetic_events.o \
    src/plugins/platforms/feishu/feishu_comment_rules.o \
    src/plugins/platforms/feishu/feishu_meeting_invite.o \
    src/plugins/platforms/feishu/send_targets.o \
    src/plugins/platforms/google_chat/__init__.o \
    src/plugins/platforms/google_chat/cards.o \
    src/plugins/platforms/google_chat/oauth.o \
    src/plugins/platforms/irc/__init__.o \
    src/plugins/platforms/line/__init__.o \
    src/plugins/platforms/line/adapter.o \
    src/plugins/platforms/matrix/__init__.o \
    src/plugins/platforms/matrix/adapter_crypto.o \
    src/plugins/platforms/mattermost/__init__.o \
    src/plugins/platforms/ntfy/__init__.o \
    src/plugins/platforms/ntfy/adapter.o \
    src/plugins/platforms/photon/__init__.o \
    src/plugins/platforms/photon/auth.o \
    src/plugins/platforms/photon/cli.o \
    src/plugins/platforms/qqbot/__init__.o \
    src/plugins/platforms/qqbot/keyboards.o \
    src/plugins/platforms/qqbot/plugin_llm.o \
    src/plugins/platforms/qqbot/send.o \
    src/plugins/platforms/raft/__init__.o \
    src/plugins/platforms/raft/adapter.o \
    src/plugins/platforms/signal/__init__.o \
    src/plugins/platforms/simplex/__init__.o \
    src/plugins/platforms/slack/__init__.o \
    src/plugins/platforms/slack/adapter_forwarded.o \
    src/plugins/platforms/slack/adapter_slash_lane.o \
    src/plugins/platforms/slack/adapter_socket_lifecycle.o \
    src/plugins/platforms/slack/block_kit.o \
    src/plugins/platforms/sms/__init__.o \
    src/plugins/platforms/sms/adapter.o \
    src/plugins/platforms/teams/__init__.o \
    src/plugins/platforms/telegram/__init__.o \
    src/plugins/platforms/telegram/rich_markdown.o \
    src/plugins/platforms/telegram/rich_parts.o \
    src/plugins/platforms/telegram/telegram_album.o \
    src/plugins/platforms/telegram/telegram_chunking.o \
    src/plugins/platforms/telegram/telegram_entities.o \
    src/plugins/platforms/telegram/telegram_flood_window.o \
    src/plugins/platforms/telegram/telegram_media_fetch.o \
    src/plugins/platforms/telegram/telegram_media_group.o \
    src/plugins/platforms/telegram/telegram_polling_probe.o \
    src/plugins/platforms/telegram/telegram_replacement.o \
    src/plugins/platforms/telegram/telegram_send_errors.o \
    src/plugins/platforms/telegram/telegram_status_indicator.o \
    src/plugins/platforms/telegram/telegram_text_photo_pairing.o \
    src/plugins/platforms/telegram/telegram_topic_routing.o \
    src/plugins/platforms/wecom/__init__.o \
    src/plugins/platforms/wecom/adapter.o \
    src/plugins/platforms/wecom/callback_adapter.o \
    src/plugins/platforms/wecom/media.o \
    src/plugins/platforms/wecom/send_queue.o \
    src/plugins/platforms/wecom/streaming.o \
    src/plugins/platforms/weixin/__init__.o \
    src/plugins/platforms/weixin/adapter.o \
    src/plugins/platforms/weixin/util_helpers.o \
    src/plugins/platforms/weixin/weixin_crypto.o \
    src/plugins/platforms/weixin/weixin_http.o \
    src/plugins/platforms/weixin/weixin_markdown.o \
    src/plugins/platforms/whatsapp/__init__.o \
    src/plugins/platforms/whatsapp_cloud/__init__.o \
    src/plugins/platforms/whatsapp_cloud/adapter.o \
    src/plugins/platforms/whatsapp_cloud/checkpoint_fs.o \
    src/plugins/platforms/whatsapp_cloud/setup_wizard.o \
    src/plugins/platforms/yuanbao/base.o \
    src/plugins/platforms/yuanbao/web_git_util.o \
    src/plugins/platforms/yuanbao/yuanbao_proto_decode.o \
    src/plugins/platforms/yuanbao/yuanbao_proto_wire.o \
    src/plugins/plugin_achievements.o \
    src/plugins/plugin_achievements_api.o \
    src/plugins/plugin_browser.o \
    src/plugins/plugin_context_engine.o \
    src/plugins/plugin_dashboard_auth.o \
    src/plugins/plugin_disk_cleanup.o \
    src/plugins/plugin_file_memory.o \
    src/plugins/plugin_image_gen.o \
    src/plugins/plugin_kanban.o \
    src/plugins/plugin_model_providers.o \
    src/plugins/plugin_observability.o \
    src/plugins/plugin_platforms.o \
    src/plugins/plugin_security_guidance.o \
    src/plugins/plugin_skills.o \
    src/plugins/plugin_spotify.o \
    src/plugins/plugin_storage.o \
    src/plugins/plugin_teams_pipeline.o \
    src/plugins/plugin_video_gen.o \
    src/plugins/plugin_web.o \
    src/plugins/security-guidance/__init__.o \
    src/plugins/teams_pipeline/__init__.o \
    src/plugins/teams_pipeline/cli.o \
    src/plugins/teams_pipeline/meetings.o \
    src/plugins/teams_pipeline/microsoft_graph_client.o \
    src/plugins/video_gen/deepinfra/__init__.o \
    src/plugins/video_gen/fal/__init__.o \
    src/plugins/video_gen/openrouter/__init__.o \
    src/plugins/video_gen/xai/__init__.o \
    src/plugins/web/__init__.o \
    src/plugins/web/_common.o \
    src/plugins/web/brave_free/__init__.o \
    src/plugins/web/ddgs/__init__.o \
    src/plugins/web/exa/__init__.o \
    src/plugins/web/firecrawl/__init__.o \
    src/plugins/web/firecrawl/provider.o \
    src/plugins/web/keenable/__init__.o \
    src/plugins/web/keyless_mcp.o \
    src/plugins/web/openai_native/__init__.o \
    src/plugins/web/parallel/__init__.o \
    src/plugins/web/perplexity/__init__.o \
    src/plugins/web/searxng/__init__.o \
    src/plugins/web/searxng/provider.o \
    src/plugins/web/tavily/__init__.o \
    src/plugins/web/xai/__init__.o \
    src/ports/agent/transports/hermes_tools_mcp_server.o \
    src/provider/copilot_oauth.o \
    src/session_rules/hosted_attachments.o \
    src/session_rules/onboarding_first_tasks.o \
    src/session_switcher.o \
    src/sidebar.o \
    src/skills_hub_catalog.o \
    src/slermes_home.o \
    src/terminal.o \
    src/titlebar.o \
    src/tools/annotate_preview_tool.o \
    src/tools/ansi_strip.o \
    src/tools/api_helpers.o \
    src/tools/base.o \
    src/tools/bot_desktop/__init__.o \
    src/tools/bot_desktop/browser.o \
    src/tools/bot_desktop/install.o \
    src/tools/bot_desktop/lease.o \
    src/tools/bot_desktop/sandbox_host.o \
    src/tools/bot_mode_dm_coordination.o \
    src/tools/browser_camofox.o \
    src/tools/browser_dom.o \
    src/tools/browser_launcher_holders.o \
    src/tools/browser_real_profile_logins.o \
    src/tools/browser_session_lifecycle.o \
    src/tools/browser_session_real_profile.o \
    src/tools/browser_supervisor_dialogs.o \
    src/tools/browser_tool.o \
    src/tools/budget_config.o \
    src/tools/code_kernel_remote_gaps.o \
    src/tools/computer_use.o \
    src/tools/computer_use/__init__.o \
    src/tools/computer_use/cua_backend_driver.o \
    src/tools/computer_use/cua_backend_helpers2.o \
    src/tools/computer_use/cua_backend_parse.o \
    src/tools/computer_use_resp.o \
    src/tools/computer_use_tool.o \
    src/tools/connectors/__init__.o \
    src/tools/connectors/catalog.o \
    src/tools/connectors/gateway/__init__.o \
    src/tools/connectors/gateway/client.o \
    src/tools/connectors/gateway/config.o \
    src/tools/connectors/gateway/files.o \
    src/tools/connectors/gateway/merge.o \
    src/tools/connectors/managed.o \
    src/tools/connectors/mcp_launch.o \
    src/tools/connectors/mcp_oauth.o \
    src/tools/connectors/portal/__init__.o \
    src/tools/connectors/portal/errors.o \
    src/tools/connectors/portal/wire.o \
    src/tools/connectors/run.o \
    src/tools/connectors/targets.o \
    src/tools/cron_prompt_match.o \
    src/tools/cronjob.o \
    src/tools/delegate_child_run_flow2.o \
    src/tools/delegate_tool_auto_resume.o \
    src/tools/discord.o \
    src/tools/discord_http.o \
    src/tools/discord_ops.o \
    src/tools/environments/__init__.o \
    src/tools/environments/base_output.o \
    src/tools/environments/base_processes.o \
    src/tools/environments/base_session_env.o \
    src/tools/environments/docker_egress.o \
    src/tools/environments/docker_ownership.o \
    src/tools/environments/env_local_ports.o \
    src/tools/environments/env_managed_modal_ports.o \
    src/tools/environments/file_sync.o \
    src/tools/environments/sandbox_scope.o \
    src/tools/environments/streams.o \
    src/tools/exec_code.o \
    src/tools/feishu_tools.o \
    src/tools/file_batch.o \
    src/tools/file_batch_fs.o \
    src/tools/file_batch_rename.o \
    src/tools/file_merge.o \
    src/tools/file_pagination_ops.o \
    src/tools/file_text_ops.o \
    src/tools/file_tools_read_tracking.o \
    src/tools/file_watch.o \
    src/tools/homeassistant.o \
    src/tools/image_gen_path.o \
    src/tools/image_gen_provider.o \
    src/tools/kanban_adapter.o \
    src/tools/kanban_handlers.o \
    src/tools/managed_gateway_identity.o \
    src/tools/managed_tool_gateway.o \
    src/tools/microsoft_graph_auth.o \
    src/tools/port_lazy_deps.o \
    src/tools/port_tools_path_security.o \
    src/tools/process_registry_checkpoint.o \
    src/tools/process_registry_env_log.o \
    src/tools/process_registry_gaps.o \
    src/tools/process_registry_ownership.o \
    src/tools/process_registry_termination.o \
    src/tools/process_registry_wait_policy.o \
    src/tools/rate_limit.o \
    src/tools/result_storage.o \
    src/tools/send_message_target.o \
    src/tools/skill_manager_val.o \
    src/tools/skill_mgmt.o \
    src/tools/skill_mgr_tool_wrappers.o \
    src/tools/skill_usage.o \
    src/tools/skills_fs.o \
    src/tools/skills_guard.o \
    src/tools/skills_sync.o \
    src/tools/skills_sync_client.o \
    src/tools/skills_sync_fs.o \
    src/tools/skills_tool_flow.o \
    src/tools/terminal_cmd.o \
    src/tools/terminal_tool.o \
    src/tools/tool_coerce.o \
    src/tools/tool_init.o \
    src/tools/tts_provider.o \
    src/tools/tts_registry.o \
    src/tools/video_analyze.o \
    src/tools/video_gen_provider.o \
    src/tools/vision_tools_history_budget.o \
    src/tools/web_search.o \
    src/tools/write_approval.o \
    src/tools/x_search.o \
    src/tools/xai_http.o \
    src/tools/yuanbao_tools.o \
    src/tui_gateway/contracts/__init__.o \
    src/tui_gateway/contracts/base.o \
    src/tui_gateway/contracts/billing_delegation_pets.o \
    src/tui_gateway/contracts/canonical.o \
    src/tui_gateway/contracts/canonical_projections.o \
    src/tui_gateway/contracts/canonical_sessions.o \
    src/tui_gateway/contracts/canonical_stored_reads.o \
    src/tui_gateway/contracts/common.o \
    src/tui_gateway/contracts/connectors.o \
    src/tui_gateway/contracts/connectors_operation.o \
    src/tui_gateway/contracts/display.o \
    src/tui_gateway/contracts/groups_bot_relay.o \
    src/tui_gateway/contracts/i18n.o \
    src/tui_gateway/contracts/liveness.o \
    src/tui_gateway/contracts/machine.o \
    src/tui_gateway/contracts/profiles_vault_complete_foreign_subagents.o \
    src/tui_gateway/contracts/projects_pets.o \
    src/tui_gateway/contracts/prompt_voice.o \
    src/tui_gateway/contracts/registry.o \
    src/tui_gateway/contracts/server_requests.o \
    src/tui_gateway/contracts/tools_commands.o \
    src/tui_gateway/hosted_room_lease_keeper.o \
    src/tui_gateway/hosted_room_member_activity.o \
    src/tui_gateway/hosted_room_server_rpc.o \
    src/tui_gateway/launch_profile_policy.o \
    src/tui_gateway/mcp_oauth_sessions.o \
    src/tui_gateway/methods_connectors.o \
    src/tui_gateway/methods_connectors_account.o \
    src/tui_gateway/methods_display_watch.o \
    src/tui_gateway/methods_groups_forward.o \
    src/tui_gateway/methods_onboarding.o \
    src/tui_gateway/methods_session_branch.o \
    src/tui_gateway/methods_session_control.o \
    src/tui_gateway/methods_shared_metrics.o \
    src/tui_gateway/prompt_attachments.o \
    src/tui_gateway/session_compression.o \
    src/tui_gateway/session_transports.o \
    src/tui_gateway/ws_legacy_fallback.o \
    src/ui-tui/scripts/clipboard_image.o \
    src/ui-tui/scripts/gateway_bootstrap.o \
    src/ui-tui/scripts/native_canonical_probe.o \
    src/ui-tui/scripts/native_stop_probe.o \
    src/util_str.o \
    src/web_app.o \
    src/web_dashboard_http.o \
    src/web_server.o \
    src/window_compositor.o \
    src/window_stubs.o \
    src/window_wayland.o \
    src/window_win32.o \
    src/xdg-shell-protocol.o

CLI_OBJ = $(CUPBOARD_ORPHANS) $(CUPBOARD_HERMES_CLI) $(CUPBOARD_CLI) $(CUPBOARD_AGENT) \
    $(CUPBOARD_GATEWAY) $(CUPBOARD_TOOLS) $(CUPBOARD_CRON) $(CUPBOARD_MISC) \
    $(PORT_OBJ)

# ── PORT_OBJ ──
PORT_OBJ = $(CUPBOARD_PORT_HERMES_CLI) $(CUPBOARD_PORT_CLI) \
    $(CUPBOARD_PORT_AGENT) $(CUPBOARD_PORT_WIRED) $(CUPBOARD_PORT_GATEWAY) \
    $(CUPBOARD_PORT_TOOLS) $(CUPBOARD_PORT_CRON) $(CUPBOARD_PORT_PLUGINS) \
    $(CUPBOARD_PORT_MISC) $(PORTS_LINKED_OBJ) $(CUPBOARD_UNBUILT_MODS)

# ── Object File Lists ──────────────────────────────────────────────
# build/objects.mk -- cupboards and jars.
#
# Each CUPBOARD_* variable holds one subsystem's object files (the jars).
# Add a new object to its subsystem's cupboard, never to a global list.
# CLI_OBJ and PORT_OBJ are concatenations of their cupboards.
#
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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
# Regenerate:  python scripts/organize_objects.py
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

CUPBOARD_AGENT = src/agent/vault_local_store.o src/agent/vault_file.o src/agent/fernet.o src/agent/onboarding_flow_apply.o src/agent/onboarding_task_catalog.o src/agent/onboarding_task_catalog_data.o src/agent/onboarding_reminders.o src/agent/turn_summary.o src/agent/redact_helpers.o \
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
CUPBOARD_PORT_HERMES_CLI = src/hermes_cli/browser_connect_gaps.o src/hermes_cli/web_server_gaps.o src/hermes_cli/setup_gaps.o src/hermes_cli/web_routers/local_models_gaps.o src/hermes_cli/update_cmd_gaps.o src/hermes_cli/proxy/adapters/proxy_adapters_base.o src/hermes_cli/proxy/adapters/proxy_adapters_xai.o src/hermes_cli/proxy/adapters/proxy_adapters_nous_portal.o src/hermes_cli/anthropic_adapter.o src/hermes_cli/bedrock_adapter.o src/hermes_cli/web_server_simple.o src/hermes_cli/context_compressor.o src/hermes_cli/display.o src/hermes_cli/display_tool_preview.o src/hermes_cli/oneshot.o src/hermes_cli/cli.o src/hermes_cli/channel_directory.o src/plugins/platforms/feishu/platforms_feishu_meeting_invite.o src/hermes_cli/platforms_helpers.o src/plugins/platforms/qqbot/platforms_qqbot_crypto.o src/plugins/platforms/qqbot/platforms_qqbot_keyboards.o src/plugins/platforms/qqbot/platforms_qqbot_onboard.o src/plugins/platforms/qqbot/platforms_qqbot_utils.o src/plugins/platforms/signal/platforms_signal_rate_limit.o src/plugins/platforms/sms/platforms_sms.o src/hermes_cli/platforms_telegram_network.o src/plugins/platforms/whatsapp_cloud/platforms_wecom_callback.o src/plugins/platforms/wecom/platforms_wecom_crypto.o src/plugins/platforms/yuanbao/platforms_yuanbao_media.o src/plugins/platforms/yuanbao/platforms_yuanbao_sticker.o src/hermes_cli/scale_to_zero.o src/hermes_cli/signal_format.o src/hermes_cli/whatsapp_identity.o src/hermes_cli/shutdown_forensics.o src/hermes_cli/models.o src/hermes_cli/model_catalog.o src/hermes_cli/main_helpers.o src/hermes_cli/turn_summary_cli.o src/hermes_cli/main_cmds.o src/hermes_cli/approvals_suggest.o src/hermes_cli/checkpoints_format.o src/hermes_cli/backup.o src/hermes_cli/pty_clamp_helpers.o src/hermes_cli/gateway_platform.o src/hermes_cli/kanban_helpers.o src/hermes_cli/kanban_cmd_gaps.o src/hermes_cli/fallback_config.o src/hermes_cli/timeouts.o src/hermes_cli/session_listing.o src/hermes_cli/project_tree.o src/hermes_cli/models_helpers.o src/hermes_cli/models_net.o src/hermes_cli/models_pure_impl.o src/hermes_cli/models_pure.o src/hermes_cli/models_validate.o src/hermes_cli/models_gaps.o src/hermes_cli/shared_metrics.o src/hermes_cli/managed_scope_helpers.o src/agent/context_breakdown_helpers.o src/hermes_cli/verification_stop_helpers.o src/cron/blueprint_catalog_helpers.o src/agent/vertex_adapter_helpers.o src/hermes_cli/drain_control_helpers.o src/gateway/platforms/platforms_base_helpers.o src/hermes_cli/gateway_command_sanitize.o src/hermes_cli/blueprint_cmd.o src/hermes_cli/goals_data.o src/hermes_cli/goals_manager.o src/hermes_cli/goals_helpers.o src/hermes_cli/provider_meta.o src/hermes_cli/completion.o src/hermes_cli/completion_ports.o src/gateway/scale_to_zero_helpers.o src/cron/jobs_helpers.o src/hermes_cli/profiles_helpers.o src/plugins/platforms/yuanbao/yuanbao_proto_helpers.o src/agent/learning_graph_render_helpers.o src/tools/lazy_deps_helpers.o src/hermes_cli/model_switch_helpers.o src/hermes_cli/learning_graph_helpers.o src/agent/learning_graph.o src/agent/learning_graph_render.o src/agent/learning_mutations.o src/tools/file_state_helpers.o src/tools/fuzzy_match_helpers.o src/tools/file_tools_helpers.o src/tools/computer_use/cua_backend_helpers.o src/tools/delegate_tool_helpers.o src/hermes_cli/browser_camofox.o src/hermes_cli/discord_helpers.o src/pet/pet_render.o src/plugins/platforms/qqbot/whatsapp_common_helpers.o src/plugins/platforms/qqbot/whatsapp_cloud_helpers.o src/hermes_cli/clipboard_helpers.o src/hermes_cli/doctor_helpers.o src/tools/code_execution_helpers.o src/tools/environments/environments_daytona.o src/hermes_cli/environments_file_sync.o src/tools/environments/environments_managed_modal.o src/tools/environments/environments_modal_utils.o src/tools/environments/environments_singularity.o src/tools/environments/environments_ssh.o src/hermes_cli/managed_tool_gateway.o src/hermes_cli/mcp_oauth_manager.o src/plugins/teams_pipeline/microsoft_graph_auth.o src/hermes_cli/skills_guard.o src/hermes_cli/write_approval.o src/hermes_cli/auth.o src/hermes_cli/auth_store.o src/hermes_cli/mcp_security.o src/hermes_cli/security_advisories.o src/hermes_cli/kanban_db.o src/hermes_cli/web_server.o src/hermes_cli/profiles_warn_port.o src/hermes_cli/web_routers/profiles.o src/hermes_cli/web_routers/cron.o src/hermes_cli/dashboard_auth/token_auth.o src/hermes_cli/dashboard_auth/routes.o src/hermes_cli/dashboard_auth/prefix.o src/hermes_cli/dashboard_auth/native_flow.o src/hermes_cli/dashboard_auth/middleware.o src/hermes_cli/dashboard_auth/login_page.o src/hermes_cli/dashboard_auth/cookies.o src/hermes_cli/dashboard_auth/base.o src/hermes_cli/yuanbao_tools.o src/hermes_cli/memory_setup.o src/hermes_cli/toolset_validation.o src/hermes_cli/extra.o src/hermes_cli/config.o src/hermes_cli/container_boot.o src/hermes_cli/dump.o src/hermes_cli/dump_ports.o src/hermes_cli/journey_ports.o src/hermes_cli/kanban_decompose_ports.o src/hermes_cli/pairing_ports.o src/hermes_cli/gateway.o src/hermes_cli/goals.o src/hermes_cli/goals_ports.o src/hermes_cli/profile_distribution_ports.o src/hermes_cli/projects_db_ports.o src/hermes_cli/pty_bridge_ports.o src/hermes_cli/win_pty_bridge_ports.o src/hermes_cli/service_manager_ports.o src/hermes_cli/active_sessions_ports.o src/hermes_cli/kanban_schema.o src/hermes_cli/kanban_model.o src/hermes_cli/kanban_tasks.o src/hermes_cli/kanban_lifecycle.o src/hermes_cli/kanban_notify.o src/hermes_cli/kanban_stats.o src/hermes_cli/kanban_runs.o src/hermes_cli/kanban_query.o src/hermes_cli/kanban_util.o src/hermes_cli/kanban_boards.o src/hermes_cli/kanban_decompose.o src/hermes_cli/kanban_workers.o src/hermes_cli/web_routers/memory_providers.o src/hermes_cli/dashboard_auth/dashboard_auth_audit.o src/hermes_cli/dashboard_auth/dashboard_auth_registry.o src/hermes_cli/dashboard_auth/dashboard_auth_cookies.o src/hermes_cli/kanban_diagnostics.o src/hermes_cli/kanban_diagnostics_ports.o src/hermes_cli/nous_account_ports.o src/hermes_cli/proxy/adapters/nous_portal_ports.o src/hermes_cli/uninstall_ports.o src/hermes_cli/proxy_cli_ports.o src/hermes_cli/windows_ssh_runtime_ports.o src/hermes_cli/hermes_state_helpers2.o src/hermes_cli/bang_shell_ports.o src/hermes_cli/commands_mixin_wrappers.o src/hermes_cli/config_ports.o src/hermes_cli/pet_input.o src/hermes_cli/config_py_io.o src/hermes_cli/util_helpers.o src/hermes_cli/pty_session_ports.o src/hermes_cli/startup_fast.o src/hermes_cli/auth_wrappers.o src/hermes_cli/plugins_cmd_wrappers.o src/hermes_cli/setup_ports.o src/hermes_cli/plugins_wrappers.o src/hermes_cli/tools_config_cua.o src/hermes_cli/tools_config_install.o src/hermes_cli/plugins_core.o src/hermes_cli/tools_config_providers.o src/hermes_cli/tools_config_toolsets.o src/hermes_cli/platform_tools.o src/hermes_cli/session_title.o src/hermes_cli/personality.o src/hermes_cli/helpers2.o src/hermes_cli/config_py_pure.o src/utils/utils_truthy.o src/hermes_cli/session_export_wrappers.o src/hermes_cli/moa_config.o src/hermes_cli/azure_detect.o src/hermes_cli/fallback_cmd.o src/hermes_cli/session_recap.o src/hermes_cli/middleware.o src/hermes_cli/curses_ui.o src/hermes_cli/security_audit.o src/hermes_cli/model_normalize.o src/hermes_cli/provider_catalog.o src/hermes_cli/model_switch.o src/hermes_cli/billing_json_helpers.o src/hermes_cli/runtime_provider.o src/hermes_cli/sqlite_util.o src/hermes_cli/projects_db.o src/hermes_cli/debug_cli.o src/hermes_cli/auth_helpers.o src/hermes_cli/kanban_format.o src/hermes_cli/prompt_stash.o src/hermes_cli/web_server_auth.o src/hermes_cli/web_server_schema_path.o src/plugins/platforms/yuanbao/web_git.o src/hermes_cli/web_routers/web_git_routes.o src/hermes_cli/web_update.o src/hermes_cli/update_cmd.o src/hermes_cli/update_cmd_more.o src/hermes_cli/gateway_gaps.o src/hermes_cli/auth_gaps.o src/hermes_cli/banner_gaps.o src/hermes_cli/profile_gaps.o src/hermes_cli/agent_import.o src/hermes_cli/plugin_manifest.o src/hermes_cli/config_helpers.o src/hermes_cli/command_registry.o src/hermes_cli/shared_metrics_contract.o src/hermes_cli/port_update_pause_record.o src/hermes_cli/port_left_core_migration.o src/hermes_cli/port_model_switch.o src/hermes_cli/observability/smc_event_extractors.o src/hermes_cli/heartbeat.o src/hermes_cli/focus_view.o src/hermes_cli/slash_exec.o src/hermes_cli/kanban_swarm.o src/hermes_cli/skills_config.o src/hermes_cli/_parser_gaps3.o src/hermes_cli/agent_plugins_gaps3.o src/hermes_cli/approvals_test_gaps3.o src/hermes_cli/azure_detect_gaps3.o src/hermes_cli/banner_gaps3.o src/hermes_cli/build_info_gaps3.o src/hermes_cli/bundles_gaps3.o src/hermes_cli/callbacks_gaps3.o src/hermes_cli/checkpoints_gaps3.o src/hermes_cli/cli_agent_setup_mixin_gaps3.o src/hermes_cli/cli_output_gaps3.o src/hermes_cli/clipboard_gaps3.o src/hermes_cli/codex_runtime_switch_gaps3.o src/hermes_cli/container_boot_gaps3.o src/hermes_cli/curator_gaps3.o src/hermes_cli/dashboard_auth/base_gaps3.o src/hermes_cli/dashboard_auth/routes_gaps3.o src/hermes_cli/dashboard_procs_gaps3.o src/hermes_cli/dep_ensure_gaps3.o src/hermes_cli/dingtalk_auth_gaps3.o src/hermes_cli/fallback_cmd_gaps3.o src/hermes_cli/gitlock_gaps3.o src/hermes_cli/image_provenance_gaps3.o src/hermes_cli/install_identity_gaps3.o src/hermes_cli/kanban_gaps3.o src/hermes_cli/kanban_db_gaps3.o src/hermes_cli/kanban_swarm_gaps3.o src/hermes_cli/local_runtime/capabilities_gaps3.o src/hermes_cli/local_runtime/detect_gaps3.o src/hermes_cli/managed_uv_gaps3.o src/hermes_cli/mcp_picker_gaps3.o src/hermes_cli/migrate_gaps3.o src/hermes_cli/model_catalog_gaps3.o src/hermes_cli/model_cost_guard_gaps3.o src/hermes_cli/models_gaps3.o src/hermes_cli/nous_account_gaps3.o src/hermes_cli/nous_billing_gaps3.o src/hermes_cli/nous_subscription_gaps3.o src/hermes_cli/observability/shared_metrics_gaps3.o src/hermes_cli/observability/shared_metrics_send_config_gaps3.o src/hermes_cli/oneshot_gaps3.o src/hermes_cli/partial_compress_gaps3.o src/hermes_cli/platforms_gaps3.o src/hermes_cli/plugin_capabilities_gaps3.o src/hermes_cli/plugin_index_gaps3.o src/hermes_cli/plugin_packs_gaps3.o src/hermes_cli/portal_cli_gaps3.o src/hermes_cli/process_identity_gaps3.o src/hermes_cli/profile_describer_gaps3.o src/hermes_cli/projects_db_gaps3.o src/hermes_cli/providers_gaps3.o src/hermes_cli/psutil_android_gaps3.o src/hermes_cli/pty_bridge_gaps3.o src/hermes_cli/relaunch_gaps3.o src/hermes_cli/resource_limits_gaps3.o src/hermes_cli/secrets_cli_gaps3.o src/hermes_cli/session_lost_and_found_gaps3.o src/hermes_cli/sessions_cmd_gaps3.o src/hermes_cli/skin_engine_gaps3.o src/hermes_cli/stdio_gaps3.o src/hermes_cli/subcommands/dashboard_gaps3.o src/hermes_cli/subcommands/peer_gaps3.o src/hermes_cli/telegram_managed_bot_gaps3.o src/hermes_cli/terminal_breadcrumbs_gaps3.o src/hermes_cli/toolset_validation_gaps3.o src/hermes_cli/uninstall_gaps3.o src/hermes_cli/update_contract_gaps3.o src/hermes_cli/update_inventory_gaps3.o src/hermes_cli/voice_gaps3.o src/hermes_cli/web_git_gaps3.o src/hermes_cli/web_routers/git_gaps3.o src/hermes_cli/web_routers/mcp_gaps3.o src/hermes_cli/web_routers/sessions_gaps3.o src/hermes_cli/web_routers/skills_gaps3.o src/hermes_cli/webhook_gaps3.o src/hermes_cli/win_pty_bridge_gaps3.o src/hermes_cli/xai_retirement_gaps3.o src/hermes_cli/port_config.o src/hermes_cli/port_config_nested.o src/hermes_cli/port_auth.o src/hermes_cli/provider_registry.o

CUPBOARD_PORT_CLI = src/cli/hermes_home_defaults.o src/cli/hermes_home_override.o src/cli/hermes_time_ports.o \
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

CUPBOARD_PORT_AGENT = src/cli/port_tui_dispatch_flow.o src/cli/port_gateway_agent_flow.o src/cli/port_gateway_windows_flow.o src/cli/gateway_windows_pure.o src/gateway/relay/relay_adapter_flow.o src/hermes_cli/observability/relay_shared_metrics_flow.o src/gateway/session_policy_flow.o src/gateway/run_startup_flow.o src/gateway/run_shutdown_flow.o src/cli/port_agent_runtime_helpers_flow.o src/cli/port_update_lock_flow.o src/cli/port_stream_consumer_flow.o src/tools/terminal_tool_lifecycle_flow.o src/cli/port_gw_service_lifecycle.o src/hermes_cli/config_load_save_flow.o src/hermes_cli/port_kanban_db_flow.o src/agent/port_credential_pool_flow.o src/gateway/platforms/base_flow.o src/agent/auxiliary_client_flow.o src/gateway/platforms/api_server_flow.o src/agent/port_conversation_compression_flow.o src/cron/port_scheduler_flow.o src/agent/port_chat_completion_helpers.o src/agent/context_compressor_flow.o src/gateway/run_flow.o src/agent/chat_completion_helpers_flow.o src/agent/verify/verify_recipes.o src/gateway/slash_commands.o src/agent/image_gen_provider_ports.o src/agent/image_gen_registry_ports.o src/agent/proxy_sources/iron_proxy_ports.o src/agent/redact_ports.o src/agent/skill_bundles_ports.o src/agent/verify/verify_environment.o src/hermes_cli/timefmt.o src/tools/patch.o src/agent/credential_pool_custom.o src/agent/anthropic_adapter.o src/agent/aux_accounting.o src/agent/background_review_ports.o src/agent/battery.o src/agent/bedrock_adapter_ports.o src/agent/billing_view_ports.o src/agent/chat_completions_ports.o src/agent/copilot_acp_client_ports.o src/agent/credential_pool_ports.o src/agent/credential_sources_ports.o src/gateway/platforms/media_cache.o src/hermes_cli/state_usage_db.o src/tools/media_cache.o src/tools/skills_hub.o src/agent/pet/pet_render.o src/agent/account_usage.o src/tools/threat_patterns.o src/agent/auxiliary_client_gaps.o src/agent/display_ports.o src/agent/error_classifier.o src/agent/google_oauth.o src/agent/web_search_provider.o src/persistence/sqlite/_state/hermes_state.o src/persistence/sqlite/_state/hermes_state_archive.o src/persistence/sqlite/_state/hermes_state_common.o src/persistence/sqlite/_state/hermes_state_db_identity.o src/persistence/sqlite/_state/hermes_state_fd_budget.o src/persistence/sqlite/_state/hermes_state_gaps.o src/persistence/sqlite/_state/hermes_state_journal.o src/persistence/sqlite/_state/hermes_state_lifecycle.o src/persistence/sqlite/_state/hermes_state_lineage.o src/persistence/sqlite/_state/hermes_state_locks.o src/persistence/sqlite/_state/hermes_state_messages.o src/persistence/sqlite/_state/hermes_state_misc.o src/persistence/sqlite/_state/hermes_state_msgs.o src/persistence/sqlite/_state/hermes_state_open.o src/persistence/sqlite/_state/hermes_state_ops.o src/persistence/sqlite/_state/hermes_state_portability.o src/persistence/sqlite/_state/hermes_state_ports.o src/persistence/sqlite/_state/hermes_state_repair.o src/persistence/sqlite/_state/hermes_state_schema.o src/persistence/sqlite/_state/hermes_state_search.o src/persistence/sqlite/_state/hermes_state_session.o src/persistence/sqlite/_state/hermes_state_session2.o src/persistence/sqlite/_state/hermes_state_tokens.o src/persistence/sqlite/_state/port_hermes_state_keys.o src/agent/skill_commands.o src/agent/context_compressor_pure.o src/agent/prompt_builder.o src/agent/context.o src/agent/provider_metadata.o src/agent/context_compressor_ports.o src/agent/agent_runtime_pure.o src/plugins/platforms/bluebubbles/redact.o src/agent/auxiliary_client_helpers.o src/agent/models_dev.o src/agent/agent_message_sanitize.o src/agent/vault.o src/agent/learn_prompt.o src/agent/file_safety.o src/agent/context_breakdown.o src/agent/estop.o src/agent/curator.o src/agent/plugin_ext.o src/agent/subagent_lifecycle.o src/agent/image_gen_provider.o src/agent/video_gen_provider_ports.o src/agent/browser_registry.o src/agent/budget_tracker.o src/agent/file_mutation_verifier.o src/agent/turn_finalizer.o src/agent/nous_rate_guard.o src/agent/hook_registry.o src/agent/image_routing.o src/agent/deadline.o src/agent/agent_message_repair.o src/agent/trajectory.o src/agent/skill_bundles.o src/agent/subdir_hints.o src/agent/system_prompt.o src/agent/todo_hydrate.o src/agent/conversation_loop.o src/agent/agent_loop.o src/agent/display_helpers.o src/agent/audit.o src/agent/checkpoint.o src/agent/tool_guardrails.o src/agent/markdown_render.o src/agent/insights.o src/agent/onboarding.o src/agent/outbound_webhooks.o src/plugins/platforms/qqbot/shell_hooks.o src/agent/usage_pricing.o src/agent/provider_profile_apply.o src/agent/codex_responses_adapter.o src/agent/api_error_summary.o src/agent/transports/transport_common.o src/agent/portal_tags.o src/agent/provider_profile.o src/agent/provider_anthropic.o src/agent/provider_azure.o src/agent/provider_bedrock.o src/agent/provider_codex_responses.o src/agent/provider_custom.o src/agent/provider_deepseek.o src/agent/provider_google.o src/agent/provider_openai.o src/agent/provider_openrouter.o src/agent/provider_xai.o src/agent/prompt_caching.o src/agent/credential_pool_engine.o src/agent/provider_profiles_builtin.o src/agent/sanitize.o src/agent/auxiliary_client.o src/agent/chat_completion_helpers.o src/agent/llm_client.o src/agent/provider.o src/agent/message_sanitization.o src/agent/message_metadata.o src/agent/anthropic_adapter_ports.o src/agent/logger.o src/agent/port_conversation_compression.o src/agent/port_model_metadata.o src/agent/chat_completion_helpers_pure.o src/agent/reasoning_timeouts.o src/agent/secret_sources/registry_gaps3.o src/agent/ssl_verify_gaps3.o src/agent/terminal_env_registry_gaps3.o src/agent/title_generator_gaps3.o src/agent/transports/chat_completions_gaps3.o src/agent/transports/types_gaps3.o src/agent/usage_pricing_gaps3.o src/agent/verification_evidence_gaps3.o src/agent/vertex_adapter_gaps3.o src/agent/web_search_registry_gaps3.o src/agent/port_error_surface.o src/agent/port_image_eviction_policy.o src/agent/port_compaction_events.o src/agent/port_markdown_tables.o src/agent/port_turn_liveness.o src/agent/azure_identity_adapter_ports.o src/agent/browser_provider_ports.o src/agent/browser_registry_ports.o src/agent/transports/codex_event_projector_ports.o src/agent/codex_responses_adapter_ports.o src/agent/context_engine_ports.o src/agent/credits_tracker_ports.o src/agent/file_safety_ports.o src/agent/transports/hermes_tools_mcp_server_ports.o src/agent/insights_ports.o src/agent/iteration_budget_ports.o src/agent/port_auxiliary_client.o src/agent/port_display.o src/agent/port_agent_secret_scope.o src/agent/credits_tracker.o

CUPBOARD_PORT_WIRED = src/gateway/platforms/platforms_helpers_flow.o src/agent/bedrock_adapter_flow.o src/gateway/run_busy_flow.o src/cli/port_ms_providers_flow.o src/agent/redact_flow.o src/gateway/platforms/openai_routes_flow.o src/cli/port_model_switch_flow.o src/cli/port_main_desktop_flow.o src/cli/port_gateway_migrate_flow.o src/tools/mcp_oauth_flow.o src/gateway/hosted_rooms_flow.o src/gateway/platforms/api_server_runs_flow.o src/tools/environments/local_env_flow.o src/gateway/run_adapters_flow.o src/agent/moa_loop_flow.o src/cron/cron_jobs_flow.o src/cron/cron_jobs_rewrite.o src/cli/port_main_cli_flow.o src/tools/browser_launcher_flow.o src/hermes_cli/config_migration_flow.o src/gateway/gateway_inbound_flow.o src/tools/transcribe.o src/gui_core.o src/gateway/platforms_base.o src/tools/skill_provenance.o src/gateway/platforms/api_server_session_json.o src/cli/port_gateway_status_flow.o src/cli/port_models_catalog_flow.o src/tools/process_registry_flow.o src/hermes_cli/kanban_db_ports.o src/tools/terminal_hints.o src/agent/secret_sources/secret_common.o src/hermes_cli/tools_config_reconfig.o src/tools/sandbox.o src/tools/vision_analysis_native.o src/desktop_app_common.o src/agent/codex_runtime_gaps.o src/agent/compaction_display.o src/agent/context_compressor_gaps.o src/agent/context_compressor_micro.o src/agent/context_compressor_wrappers.o src/agent/context_engine.o src/agent/context_references.o src/agent/context_references_gaps3.o src/agent/conversation_compression_gaps.o src/agent/conversation_compression_gaps2.o src/agent/conversation_compression_helpers.o src/agent/conversation_loop_gaps.o src/agent/conversation_loop_py_parity.o src/agent/copilot_acp_client_helpers2.o src/agent/core_helpers.o src/agent/credential_pool_gaps.o src/agent/credential_pool_sync.o src/agent/credential_sources.o src/agent/curator_ports.o src/agent/delegation_context.o src/agent/delegation_context_gaps3.o src/agent/empty_response_guard.o src/agent/error_surface.o src/agent/fast_mode_gaps3.o src/agent/gemini_native_helpers2.o src/agent/gemini_schema.o src/agent/i18n_languages.o src/agent/i18n_ports.o src/agent/image_gen_provider_py_final.o src/agent/image_routing_ports.o src/agent/init_ports.o src/agent/intent_ack.o src/agent/lsp/lsp_cli_ports.o src/agent/lsp/lsp_client.o src/agent/lsp/lsp_client_gaps.o src/agent/lsp/lsp_client_ports.o src/agent/lsp/lsp_eventlog.o src/agent/lsp/lsp_eventlog_ports.o src/agent/lsp/lsp_init_ports.o src/agent/lsp/lsp_install_ports.o src/agent/lsp/lsp_manager.o src/agent/lsp/lsp_manager_ports.o src/agent/lsp/lsp_protocol.o src/agent/lsp/lsp_range_shift.o src/agent/lsp/lsp_servers.o src/agent/models_dev_ports.o src/agent/monitoring/cron_health.o src/agent/monitoring/monitoring_events.o src/agent/monitoring/monitoring_policy.o src/agent/monitoring/policy.o src/agent/monitoring/redaction.o src/agent/moonshot_schema.o src/agent/moonshot_schema_ports.o src/agent/native_compaction.o src/agent/native_compaction_gaps.o src/agent/nous_rate_guard_ports.o src/agent/onboarding_ports.o src/agent/opencode_affinity_gaps.o src/agent/pet/generate/imagegen.o src/agent/pet/generate/orchestrate.o src/agent/pet/render_gaps3.o src/agent/plan_prompt_gaps3.o src/agent/plugin_llm_ports.o src/agent/plugin_stream_hooks.o src/agent/portal_tags_gaps.o src/agent/process_bootstrap_gaps.o src/agent/prompt_builder_gaps.o src/agent/prompt_builder_guidance.o src/agent/prompt_builder_ports.o src/agent/prompt_cache_boundary.o src/agent/prompt_cache_scope_gaps.o src/agent/prompt_caching_gaps.o src/agent/prompt_caching_plan.o src/agent/prompt_caching_strip.o src/agent/provider_projection_gaps3.o src/agent/proxy_sources/iron_proxy.o src/agent/reactions.o src/agent/reasoning_summaries.o src/agent/redact_gaps.o src/agent/redact_gaps3.o src/agent/relay_llm_gaps.o src/agent/relay_tools.o src/agent/retry_utils_ports.o src/agent/review_engine.o src/agent/review_engine_gaps3.o src/agent/review_idle_queue_gaps.o src/agent/runtime_cwd.o src/agent/runtime_cwd_gaps3.o src/agent/runtime_cwd_ports.o src/agent/runtime_helpers_ports.o src/agent/secret_sources/bitwarden_ports.o src/agent/secret_sources/onepassword_ports.o src/agent/secret_sources/secret_sources_base_ports.o src/agent/secret_sources/secret_sources_cache_ports.o src/agent/secret_sources/secret_sources_command_ports.o src/agent/secret_sources/secret_sources_registry_ports.o src/agent/secret_sources_helpers.o src/agent/shell_hooks_ports.o src/agent/side_question_gaps.o src/agent/skill_preprocessing_ports.o src/agent/skill_utils_gaps.o src/agent/skill_utils_wrappers.o src/cli_gaps.o src/cron/cron_locking.o src/cron/delivery_queue_gaps.o src/cron/executions.o src/cron/incidents_gaps.o src/cron/jobs_gaps.o src/cron/lifecycle_guard_gaps.o src/cron/monitor.o src/cron/port_jobs_pure.o src/cron/scheduler_preflight.o src/cron/scheduler_provider.o src/cron/scheduler_provider_ports.o src/cron/scheduler_toolsets.o src/cron/scripts/classify_items.o src/desktop_settings.o src/gateway/authz_mixin_gaps.o src/gateway/browser_control_artifacts.o src/gateway/turn_context.o src/gateway/turn_lease_ports.o src/gateway/whatsapp_identity_ports.o src/hermes_cli/_early_recovery_gaps.o src/hermes_cli/_install_repair_gaps.o src/hermes_cli/_scan_venv_blockers_gaps.o src/hermes_cli/_subprocess_compat_gaps.o src/hermes_cli/active_sessions_gaps.o src/hermes_cli/archive_safe_gaps.o src/hermes_cli/auth_commands_gaps.o src/hermes_cli/backup_gaps.o src/hermes_cli/billing_mixin_ports.o src/hermes_cli/cli_commands_mixin_gaps.o src/hermes_cli/cli_ports.o src/hermes_cli/clipboard.o src/hermes_cli/commands_gaps.o src/hermes_cli/commands_wrappers.o src/hermes_cli/config_gaps.o src/hermes_cli/copilot_auth_gaps.o src/hermes_cli/cron_gaps.o src/hermes_cli/dashboard_auth/cookies_gaps.o src/hermes_cli/dashboard_auth/dashboard_auth_public_paths.o src/hermes_cli/doctor_gaps.o src/hermes_cli/foreign_sessions_gaps.o src/hermes_cli/gateway_windows_gaps.o src/hermes_cli/helpers3.o src/hermes_cli/install_repair.o src/hermes_cli/inventory_gaps.o src/hermes_cli/journey_gaps.o src/hermes_cli/kanban_db_wrappers.o src/hermes_cli/kanban_transfer_gaps.o src/hermes_cli/linux_desktop_entry_gaps.o src/hermes_cli/local_runtime/binaries_gaps.o src/hermes_cli/local_runtime/bootstrap_gaps.o src/hermes_cli/local_runtime/catalog_gaps.o src/hermes_cli/local_runtime/context_policy_gaps.o src/hermes_cli/local_runtime/endpoint_gaps.o src/hermes_cli/local_runtime/estimator_gaps.o src/hermes_cli/local_runtime/gguf_gaps.o src/hermes_cli/local_runtime/growth_gaps.o src/hermes_cli/local_runtime/hardware_gaps.o src/hermes_cli/local_runtime/hf_browse_gaps.o src/hermes_cli/local_runtime/load_progress_gaps.o src/hermes_cli/local_runtime/presets_gaps.o src/hermes_cli/local_runtime/supervisor_gaps.o src/hermes_cli/macos_tcc_anchor_gaps.o src/hermes_cli/main_gaps.o src/hermes_cli/mcp_catalog_gaps.o src/hermes_cli/mcp_config_gaps.o src/hermes_cli/mcp_startup_gaps.o src/hermes_cli/mem_trim.o src/hermes_cli/migrate.o src/hermes_cli/moa_config_pure.o src/hermes_cli/moa_slash.o src/hermes_cli/model_setup_wrappers.o src/hermes_cli/model_switch_gaps.o src/hermes_cli/model_switch_wrappers.o src/hermes_cli/nous_account_wrappers.o src/hermes_cli/nous_auth_keepalive_gaps.o src/hermes_cli/nous_sub_wrappers.o src/hermes_cli/session_recovery_preflight.o src/hermes_cli/uninstall_wrappers.o src/hermes_cli/update_abort_recovery_gaps.o src/hermes_cli/update_lock.o src/hermes_cli/update_restart_recovery_gaps.o src/hermes_cli/web_routers/profiles_gaps.o src/hermes_cli/web_server_helpers2.o src/hermes_cli/windows_ssh_wrappers.o src/hermes_cli/windows_wrappers.o src/persistence/sqlite/_state/port_state_errors.o src/persistence/sqlite/_state/port_state_guard.o src/plugins/cron_providers/chronos/chronos_verify.o src/plugins/platforms/bluebubbles/bluebubbles_ports.o src/plugins/platforms/bluebubbles/bluebubbles_wrappers.o src/plugins/platforms/email/codex_app_server_client.o src/plugins/platforms/qqbot/api_server_adapter_handlers.o src/plugins/platforms/qqbot/chunked_upload.o src/plugins/platforms/qqbot/qqbot_adapter_ports.o src/plugins/platforms/qqbot/qqbot_constants.o src/plugins/platforms/qqbot/qqbot_keyboards_ports.o src/plugins/platforms/qqbot/qqbot_wrappers.o src/plugins/platforms/signal/signal_gaps.o src/plugins/platforms/signal/signal_helpers2.o src/plugins/platforms/signal/signal_ports.o src/plugins/platforms/signal/signal_rate_limit_ports.o src/plugins/platforms/wecom/wecom_callback.o src/plugins/platforms/weixin/vision_wrappers.o src/plugins/platforms/weixin/weixin_ports.o src/plugins/platforms/weixin/weixin_wrappers.o src/plugins/platforms/whatsapp_cloud/whatsapp_cloud.o src/plugins/platforms/yuanbao/api_server_adapter.o src/plugins/platforms/yuanbao/yuanbao_media.o src/plugins/platforms/yuanbao/yuanbao_media_ports.o src/plugins/platforms/yuanbao/yuanbao_ports.o src/plugins/platforms/yuanbao/yuanbao_proto_buf.o src/plugins/platforms/yuanbao/yuanbao_proto_encode.o src/plugins/platforms/yuanbao/yuanbao_wrappers.o src/tools/computer_use/computer_use_x11.o src/tools/computer_use/cua_backend_gaps.o src/tools/computer_use/cua_backend_methods.o src/tools/computer_use/cua_doctor.o src/tools/computer_use_windows.o src/tools/cronjob_tools_gaps.o src/tools/cua_backend_wrappers.o src/tools/debug_helpers_ports.o src/tools/delegate_tool_gaps.o src/tools/drive_preview_tool.o src/tools/env_local_wrappers.o src/tools/env_probe.o src/tools/environments/env_base_wrappers.o src/tools/environments/env_docker_wrappers.o src/tools/environments/env_ssh_ports.o src/tools/environments/local_gaps.o src/tools/environments/managed_modal.o src/tools/environments/modal.o src/tools/environments/vercel_sandbox_env.o src/tools/environments/vercel_sandbox_helpers.o src/tools/feishu_comment_rules.o src/tools/feishu_doc_tool_ports.o src/tools/feishu_drive_tool.o src/tools/feishu_drive_tool_ports.o src/tools/file.o src/tools/file_operations_search.o src/tools/file_state_ports.o src/tools/file_tools.o src/tools/file_tools_gaps.o src/tools/file_tools_helpers2.o src/tools/file_tools_wrappers.o src/tools/image_generation_tool_gaps.o src/tools/kanban_tools.o src/tools/kanban_tools_ports.o src/tools/lazy_deps_gaps.o src/tools/mcp_oauth_manager_ports.o src/tools/mcp_oauth_ports.o src/tools/mcp_oauth_wrappers.o src/tools/mcp_schema_cache.o src/tools/mcp_tool_gaps.o src/tools/moa_performance.o src/tools/online_research_ports.o src/tools/plugin_guard.o src/tools/tool_search_gaps.o src/tools/tour_tool.o src/tools/transcribe_helpers.o src/tools/transcription_tools_ports.o src/tools/transcription_wrappers.o src/tools/tts_tool_gaps.o src/tools/vercel_sandbox.o src/tools/vision.o src/tools/vision_analysis.o src/tools/vision_delegate.o src/tools/vision_helpers.o src/tools/vision_tools_gaps.o src/tools/web.o src/tools/web_base64_img.o src/tools/web_result_cache_gaps.o src/tools/web_tools.o src/tools/wecom_crypto.o src/trajectory_compressor.o src/tui_gateway/tui_gateway_compute_host.o src/utils/utils.o src/utils/utils_gaps.o src/hermes_cli/session_recovery_paths.o src/hermes_cli/session_recovery/session_recovery_verify.o src/hermes_cli/session_recovery_main.o src/hermes_cli/session_recovery_salvage.o src/acp_adapter/acp_adapter_entry.o src/acp_adapter/acp_adapter_server.o src/acp_adapter/acp_adapter_session.o src/acp_adapter/acp_adapter_tools.o src/acp_adapter/modes.o src/hermes_cli/config_turn_limit.o src/hermes_cli/url_utils.o src/cron/jobs_tristate.o src/cron/constants.o src/gateway/run_common.o src/tools/tool_output_truncate.o src/agent/chat_completion_headers.o src/agent/model_metadata_llamacpp.o src/agent/redact_approval_display.o src/agent/redact_command_segments.o src/agent/runtime_self_protection.o src/agent/terminal_approval_batch.o src/agent/turn_context_compaction.o src/cron/jobs_oneshot_diagnostics.o src/cron/scheduler_delivery_media.o src/cron/scheduler_worker_failure.o src/gateway/inbound_reply_context.o src/gateway/plugin_command_choice.o src/gateway/plugin_keyless_inject.o src/gateway/run_adapters_recovery.o src/gateway/run_heartbeat_restore.o src/gateway/run_housekeeping_disk.o src/gateway/run_profile_reconcile.o src/gateway/run_restored_delivery.o src/gateway/run_turn_followup_ack.o src/gateway/session_acp_lifecycle.o src/gateway/session_busy_controls.o src/gateway/session_clarify_batch.o src/gateway/session_deferred_pick.o src/gateway/session_history_mount.o src/gateway/session_ingress_burst.o src/gateway/session_ingress_media.o src/gateway/session_picker_fields.o src/gateway/session_secret_prompt.o src/gateway/slash_commands_status.o src/gateway/stream_consumer_think.o src/gateway/warning_notifications.o src/hermes_cli/auth_codex_browser.o src/hermes_cli/backend_retirement.o src/hermes_cli/browser_credential.o src/hermes_cli/cli_session_switch.o src/hermes_cli/cli_terminal_input.o src/hermes_cli/cli_terminal_mixin.o src/hermes_cli/commands_platforms.o src/hermes_cli/config_env_routing.o src/hermes_cli/config_read_errors.o src/hermes_cli/config_route_guard.o src/hermes_cli/config_secret_refs.o src/hermes_cli/config_set_scalars.o src/hermes_cli/config_worker_save.o src/hermes_cli/desktop_build_lock.o src/hermes_cli/gateway_automation.o src/hermes_cli/gateway_connection.o src/hermes_cli/gateway_replay_gap.o src/hermes_cli/kanban_db_dispatch.o src/hermes_cli/model_picker_usage.o src/hermes_cli/plugins_activation.o src/hermes_cli/plugins_cmd_remove.o src/hermes_cli/plugins_cmd_toggle.o src/hermes_cli/plugins_cmd_update.o src/hermes_cli/plugins_idle_tasks.o src/hermes_cli/plugins_load_guard.o src/hermes_cli/plugins_provenance.o src/hermes_cli/plugins_registrars.o src/hermes_cli/profile_model_seed.o src/hermes_cli/quiet_single_query.o src/hermes_cli/setup_agent_import.o src/hermes_cli/update_cmd_windows.o src/hermes_cli/update_fleet_scope.o src/hermes_cli/update_windows_acl.o src/hermes_cli/web_server_gateway.o src/hermes_cli/web_server_surface.o src/hermes_cli/webapp_page_policy.o src/hermes_cli/worktree_retention.o src/session_rules/onboarding_flow.o src/tools/async_delegation_worker.o src/tools/browser_session_sandbox.o src/tools/delegate_tool_child_run.o src/agent/context_default_record.o src/agent/credential_pool_errors.o src/agent/credential_pool_plugin.o src/agent/persisted_turn_receipt.o src/agent/vault_backend_registry.o src/agent/vault_login_classifier.o src/cron/scheduler_delivery_room.o src/gateway/parked_route_refusal.o src/gateway/run_active_work_verb.o src/gateway/run_inbound_consumer.o src/gateway/run_plugin_injection.o src/gateway/run_startup_liveness.o src/gateway/run_turn_persistence.o src/gateway/run_turn_setup_guard.o src/gateway/session_context_refs.o src/gateway/session_live_listing.o src/gateway/session_local_editor.o src/gateway/session_stored_reads.o src/gateway/session_terminal_cwd.o src/gateway/session_vault_prompt.o src/gateway/session_viewer_close.o src/gateway/session_worker_child.o src/gateway/slash_commands_goals.o src/gateway/slash_commands_login.o src/gateway/slash_commands_model.o src/gateway/status_inline_source.o src/hermes_cli/auth_model_picker.o src/hermes_cli/auth_oauth_grants.o src/hermes_cli/cli_inline_images.o src/hermes_cli/cli_modern_layout.o src/hermes_cli/cli_session_mixin.o src/hermes_cli/gateway_chat_view.o src/hermes_cli/gateway_mutations.o src/hermes_cli/kanban_db_connect.o src/hermes_cli/kanban_db_context.o src/hermes_cli/kanban_worker_env.o src/hermes_cli/kanban_worker_log.o src/hermes_cli/model_setup_flows.o src/hermes_cli/plugin_host_child.o src/hermes_cli/plugins_admission.o src/hermes_cli/plugins_aux_tasks.o src/hermes_cli/plugins_discovery.o src/hermes_cli/plugins_injection.o src/hermes_cli/profile_provision.o src/hermes_cli/runtime_readiness.o src/hermes_cli/source_completion.o src/hermes_cli/termination_guard.o src/hermes_cli/update_cmd_commit.o src/hermes_cli/update_cmd_config.o src/hermes_cli/update_completion.o src/hermes_cli/web_host_terminal.o src/hermes_cli/web_server_config.o src/hermes_cli/web_server_memory.o src/hermes_cli/worktree_registry.o src/tools/approval_parser_budget.o src/tools/delegate_tool_continue.o src/tools/delegate_tool_dispatch.o src/tools/delegate_tool_progress.o src/tools/delegate_tool_registry.o src/tools/delegate_tool_sessions.o src/tools/delegate_tool_toolsets.o src/tools/file_operations_native.o src/tools/process_registry_stdin.o src/tools/terminal_tool_backends.o src/tools/tool_search_validation.o src/tools/transcription_chunking.o src/tui_gateway/methods_complete.o src/tui_gateway/methods_profiles.o src/tui_gateway/methods_projects.o src/tui_gateway/session_registry.o src/agent/auxiliary_async_rebuild.o src/agent/auxiliary_managed_local.o src/hermes_cli/anon_sign_in_cli.o src/hermes_cli/auth_device_flow.o src/hermes_cli/backup_integrity.o src/hermes_cli/backup_keep_host.o src/hermes_cli/cli_process_dock.o src/hermes_cli/cli_session_dock.o src/hermes_cli/cli_single_query.o src/hermes_cli/cli_stream_mixin.o src/hermes_cli/config_effective.o src/hermes_cli/config_providers.o src/hermes_cli/gateway_rollback.o src/hermes_cli/git_commit_graph.o src/hermes_cli/home_data_layout.o src/hermes_cli/kanban_db_boards.o src/hermes_cli/kanban_db_notify.o src/hermes_cli/main_desktop_tcc.o src/hermes_cli/main_resume_args.o src/hermes_cli/model_assignment.o src/hermes_cli/old_updater_deps.o src/hermes_cli/old_updater_main.o src/hermes_cli/plugin_host_wire.o src/hermes_cli/plugin_isolation.o src/hermes_cli/plugins_dispatch.o src/hermes_cli/plugins_manifest.o src/hermes_cli/plugins_settings.o src/hermes_cli/profile_channels.o src/hermes_cli/profile_identity.o src/hermes_cli/release_channels.o src/hermes_cli/session_worktree.o src/hermes_cli/sessions_discard.o src/hermes_cli/ssh_workspace_fs.o src/hermes_cli/tools_config_mcp.o src/hermes_cli/update_cmd_check.o src/hermes_cli/update_cmd_fleet.o src/hermes_cli/update_cmd_maint.o src/hermes_cli/update_cmd_stash.o src/hermes_cli/update_proxy_env.o src/hermes_cli/web_build_limits.o src/hermes_cli/web_server_files.o src/hermes_cli/web_server_oauth.o src/hermes_cli/web_upload_limit.o src/hermes_cli/worktree_archive.o src/hermes_platform/declaration.o src/session_rules/route_entries.o src/tools/approval_gateway_wait.o src/tools/approval_notify_lease.o src/tools/approval_shell_tokens.o src/tools/browser_session_cloud.o src/tools/browser_vault_context.o src/tools/browser_vault_preview.o src/tools/delegate_tool_results.o src/tools/mcp_tool_registration.o src/tools/transcription_command.o src/tools/tts_elevenlabs_voices.o src/tools/voice_mode_transcript.o src/tui_gateway/_stdin_recovery.o src/tui_gateway/agent_callbacks.o src/tui_gateway/host_supervisor.o src/tui_gateway/mcp_rpc_helpers.o src/tui_gateway/methods_browser.o src/tui_gateway/methods_display.o src/tui_gateway/methods_machine.o src/tui_gateway/methods_session.o src/tui_gateway/server_requests.o src/tui_gateway/session_history.o src/tui_gateway/session_workdir.o src/tui_gateway/stream_segments.o src/acp_adapter/gateway_commands.o src/agent/auxiliary_call_outcome.o src/agent/auxiliary_error_policy.o src/agent/auxiliary_plugin_oauth.o src/agent/compression_automation.o src/hermes_cli/profiles_remove.o src/hermes_cli/setup_migration.o src/hermes_cli/setup_platforms.o src/hermes_cli/source_releases.o src/hermes_cli/webapp_launcher.o src/hermes_cli/webapp_previews.o src/session_rules/api_payloads.o src/session_rules/hosted_rooms.o src/session_rules/hosted_tasks.o src/session_rules/legacy_local.o src/session_rules/local_create.o src/tools/approval_applescript.o src/tools/approval_git_options.o src/tools/browser_harness_dirs.o src/tools/delegate_tool_config.o src/tools/delegate_tool_routes.o src/tools/delegate_tool_tuning.o src/tools/delegation_redaction.o src/tools/file_operations_lint.o src/tools/kanban_tools_schemas.o src/tools/managed_gateway_auth.o src/tools/mcp_elicitation_form.o src/tools/skill_manager_guards.o src/tools/skills_sync_optional.o src/tools/terminal_tool_config.o src/tools/terminal_tool_guards.o src/tools/terminal_tool_result.o src/tools/transcription_common.o src/tools/tts_command_provider.o src/tui_gateway/change_watcher.o src/tui_gateway/methods_config.o src/tui_gateway/methods_groups.o src/tui_gateway/methods_images.o src/tui_gateway/methods_prompt.o src/tui_gateway/session_reaper.o src/tui_gateway/synthetic_turn.o src/agent/auxiliary_task_config.o src/agent/auxiliary_unavailable.o src/agent/browser_page_registry.o src/agent/chat_completion_local.o src/agent/credential_pool_admin.o src/agent/image_eviction_policy.o src/agent/inline_tool_executors.o src/agent/local_prompt_progress.o src/agent/primary_auth_fallback.o src/agent/redact_secret_sources.o src/agent/runtime_session_child.o src/agent/runtime_session_store.o src/agent/side_question_request.o src/agent/timeout_error_summary.o src/agent/turn_request_assembly.o src/agent/turn_scripted_prelude.o src/cron/scheduler_failure_copy.o src/cron/scheduler_gateway_gate.o src/gateway/hosted_room_context.o src/gateway/hosted_rooms_common.o src/gateway/local_viewer_events.o src/gateway/platform_activation.o src/gateway/restart_watcher_env.o src/gateway/session_active_list.o src/gateway/session_authorities.o src/gateway/session_bot_mailbox.o src/gateway/session_bot_outcome.o src/gateway/session_cold_review.o src/gateway/session_connections.o src/gateway/session_group_peers.o src/gateway/session_llm_oneshot.o src/gateway/session_local_title.o src/gateway/session_persistence.o src/gateway/session_sudo_prompt.o src/gateway/status_scoped_locks.o src/hermes_cli/_update_takeover.o src/tools/transcription_audio.o src/tools/transcription_cloud.o src/tools/transcription_local.o src/tui_gateway/agent_factory.o src/tui_gateway/methods_slash.o src/tui_gateway/methods_tools.o src/tui_gateway/methods_vault.o src/tui_gateway/methods_voice.o src/tui_gateway/owner_forward.o src/tui_gateway/plugin_inject.o src/tui_gateway/store_handles.o src/tui_gateway/tool_progress.o src/tui_gateway/user_messages.o src/acp_adapter/gateway_server.o src/agent/auxiliary_openai_url.o src/agent/browser_page_backend.o src/agent/context_file_sources.o src/agent/memory_manager_tools.o src/agent/model_metadata_cache.o src/agent/provider_route_hooks.o src/agent/redact_browser_input.o src/agent/redact_control_split.o src/agent/redact_pattern_shape.o src/agent/sdk_transform_bypass.o src/agent/tool_history_as_text.o src/agent/turn_proactive_prune.o src/agent/turn_response_intake.o src/agent/turn_tool_validation.o src/cron/scheduler_bookkeeping.o src/gateway/cron_store_notices.o src/gateway/hosted_room_router.o src/gateway/run_config_loaders.o src/gateway/session_automation.o src/gateway/session_bot_cancel.o src/gateway/session_dm_threads.o src/gateway/session_info_reads.o src/gateway/session_onboarding.o src/gateway/session_plugin_rpc.o src/gateway/session_prompt_pin.o src/gateway/session_retirement.o src/gateway/session_tui_config.o src/gateway/status_pid_records.o src/hermes_cli/_secrets_common.o src/hermes_cli/auth_error_copy.o src/hermes_cli/auth_openrouter.o src/hermes_cli/browser_runtime.o src/hermes_cli/cli_config_load.o src/hermes_cli/cli_enter_burst.o src/hermes_cli/cli_loops_mixin.o src/hermes_cli/cli_modal_mixin.o src/hermes_cli/cli_voice_mixin.o src/hermes_cli/config_language.o src/hermes_cli/debug_redaction.o src/hermes_cli/default_plugins.o src/hermes_cli/desktop_console.o src/hermes_cli/doctor_platform.o src/hermes_cli/free_tier_offer.o src/hermes_cli/gateway_launchd.o src/hermes_cli/gateway_migrate.o src/hermes_cli/gateway_runtime.o src/hermes_cli/git_credentials.o src/hermes_cli/kanban_db_graph.o src/hermes_cli/kanban_workflow.o src/hermes_cli/main_agent_cmds.o src/hermes_cli/main_tui_launch.o src/hermes_cli/plugin_messages.o src/hermes_cli/plugin_validate.o src/hermes_cli/plugins_cadence.o src/hermes_cli/plugins_cmd_git.o src/hermes_cli/plugins_content.o src/hermes_cli/plugins_context.o src/hermes_cli/plugins_updates.o src/gateway/run_turn_progress.o src/gateway/runtime_ownership.o src/gateway/session_ancillary.o src/gateway/session_api_media.o src/gateway/session_discovery.o src/gateway/session_execution.o src/gateway/session_lifecycle.o src/gateway/session_local_mcp.o src/gateway/session_mutations.o src/gateway/session_reactions.o src/gateway/session_readiness.o src/gateway/session_tui_reads.o src/gateway/stuck_loop_counts.o src/gateway/systemd_stop_mark.o src/gateway/turn_thread_reset.o src/hermes_cli/anon_challenge.o src/hermes_cli/auth_constants.o src/hermes_cli/backup_restore.o src/hermes_cli/backup_staging.o src/hermes_cli/boot_bootstrap.o src/hermes_cli/cli_cron_lines.o src/hermes_cli/cli_info_lines.o src/hermes_cli/cli_info_mixin.o src/hermes_cli/cli_init_mixin.o src/hermes_cli/config_backend.o src/hermes_cli/config_backups.o src/hermes_cli/connectors_cmd.o src/hermes_cli/fallback_check.o src/hermes_cli/gateway_client.o src/hermes_cli/isolated_scope.o src/hermes_cli/main_container.o src/hermes_cli/main_dashboard.o src/hermes_cli/main_dep_hints.o src/hermes_cli/main_web_build.o src/hermes_cli/models_pricing.o src/hermes_cli/onboarding_due.o src/hermes_cli/onboarding_run.o src/hermes_cli/plugin_catalog.o src/hermes_cli/plugin_choices.o src/hermes_cli/plugin_session.o src/hermes_cli/plugins_ledger.o src/hermes_cli/plugins_loader.o src/hermes_cli/profiles_cache.o src/hermes_cli/sessions_adopt.o src/hermes_cli/setup_terminal.o src/hermes_cli/staged_uploads.o src/hermes_cli/status_bar_git.o src/hermes_cli/ui_preferences.o src/hermes_cli/update_channel.o src/hermes_cli/update_cmd_git.o src/hermes_cli/update_cmd_zip.o src/hermes_cli/update_custody.o src/hermes_cli/update_handoff.o src/hermes_cli/web_server_app.o src/hermes_cli/web_server_mcp.o src/hermes_cli/webapp_sign_in.o src/hermes_cli/worktree_sweep.o src/session_rules/session_ids.o src/tools/approval_fetch_exec.o src/tools/approval_grep_spans.o src/tools/approval_human_wait.o src/tools/approval_provenance.o src/tools/bot_failure_reasons.o src/tools/bot_mode_dm_outcome.o src/tools/browser_dialog_tool.o src/tools/browser_session_cdp.o src/tools/delegate_tool_tasks.o src/tools/mcp_tool_server_run.o src/tools/skill_manager_batch.o src/tools/skills_hub_official.o src/tools/skills_hub_skillssh.o src/tools/tool_search_catalog.o src/hermes_cli/stale_modules.o src/hermes_cli/status_report.o src/hermes_cli/update_finish.o src/hermes_cli/webapp_access.o src/hermes_cli/welcome_trial.o src/hermes_cli/worktree_exit.o src/hermes_platform/__init__.o src/session_rules/automation.o src/tools/approval_env_split.o src/tools/approval_home_fold.o src/tools/browser_url_policy.o src/tools/browser_vault_tool.o src/tools/code_execution_env.o src/tools/code_execution_rpc.o src/tools/hosted_tool_policy.o src/tools/mcp_oauth_provider.o src/tools/mcp_tool_discovery.o src/tools/mcp_tool_lifecycle.o src/tools/mcp_tool_transport.o src/tools/skill_manager_scan.o src/tools/skills_hub_clawhub.o src/tools/skills_hub_install.o src/tools/skills_hub_sources.o src/tools/skills_tool_plugin.o src/tools/terminal_tool_sudo.o src/tools/tts_tool_lifecycle.o src/tools/tts_tool_providers.o src/tools/web_tools_truncate.o src/tui_gateway/billing_view.o src/tui_gateway/compute_host.o src/tui_gateway/event_replay.o src/tui_gateway/methods_i18n.o src/tui_gateway/model_switch.o src/tui_gateway/project_tree.o src/tui_gateway/rpc_dispatch.o src/tui_gateway/slash_worker.o src/acp_adapter/edit_approval.o src/acp_adapter/gateway_media.o src/acp_adapter/gateway_reply.o src/acp_adapter/model_catalog.o src/agent/account_usage_cache.o src/agent/agent_init_fallback.o src/agent/anthropic_endpoints.o src/agent/codex_service_tiers.o src/agent/delegation_backends.o src/agent/delegation_registry.o src/agent/memory_caller_scope.o src/agent/model_metadata_http.o src/agent/plugin_session_fork.o src/agent/prepared_compaction.o src/agent/prompt_cache_routes.o src/agent/reasoning_promotion.o src/agent/redact_partial_keys.o src/agent/redact_phone_policy.o src/agent/redact_vault_values.o src/agent/session_persistence.o src/agent/side_question_lease.o src/agent/stream_max_lifetime.o src/agent/tool_progress_relay.o src/agent/turn_empty_response.o src/agent/turn_final_response.o src/agent/turn_iteration_prep.o src/agent/turn_preflight_gate.o src/agent/turn_recovery_codex.o src/agent/turn_response_check.o src/agent/vision_message_prep.o src/cron/scheduler_worker_env.o src/gateway/restart_when_idle.o src/gateway/run_inbound_hooks.o src/gateway/run_inbound_media.o src/gateway/run_plugin_rewire.o src/gateway/run_status_filter.o src/tools/bot_mode_dm_chain.o src/tools/human_input_hooks.o src/tools/mcp_tool_handlers.o src/tools/mcp_tool_node_abi.o src/tools/mcp_tool_sampling.o src/tools/memory_tool_store.o src/tools/skills_hub_github.o src/tools/skills_hub_models.o src/tools/skills_tool_dedup.o src/tools/skills_tool_setup.o src/tools/tts_tool_delivery.o src/tools/wake_word_engines.o src/tools/web_tools_extract.o src/tui_gateway/checkpoints.o src/tui_gateway/prompt_turn.o src/tui_gateway/slash_fuzzy.o src/tui_gateway/turn_marker.o src/agent/agent_init_session.o src/agent/compression_facade.o src/agent/compression_marker.o src/agent/gemini_long_decode.o src/agent/history_commentary.o src/agent/onboarding_session.o src/agent/plan_snapshot_sync.o src/agent/rate_limit_credits.o src/agent/reasoning_carriers.o src/agent/replayed_reasoning.o src/agent/safe_worker_policy.o src/agent/session_client_env.o src/agent/skills_index_delta.o src/cron/scheduler_authority.o src/cron/scheduler_ownership.o src/gateway/hosted_room_lead.o src/gateway/local_desktop_ui.o src/gateway/run_profile_home.o src/gateway/run_runtime_init.o src/gateway/run_turn_hygiene.o src/gateway/run_turn_prepare.o src/gateway/session_api_turn.o src/gateway/session_cli_turn.o src/gateway/session_commands.o src/gateway/session_detached.o src/gateway/session_envelope.o src/gateway/session_identity.o src/gateway/session_moa_turn.o src/gateway/session_recovery.o src/gateway/session_tui_side.o src/hermes_cli/_startup_fast.o src/hermes_cli/auth_zai_kimi.o src/hermes_cli/backup_atomic.o src/hermes_cli/backup_sqlite.o src/hermes_cli/cli_tui_mixin.o src/hermes_cli/clipboard_cmd.o src/hermes_cli/debug_summary.o src/hermes_cli/doctor_config.o src/hermes_cli/doctor_report.o src/hermes_cli/gateway_agent.o src/hermes_cli/gateway_queue.o src/hermes_cli/gateway_tools.o src/hermes_cli/git_pack_tidy.o src/hermes_cli/guest_handoff.o src/hermes_cli/install_state.o src/hermes_cli/kanban_output.o src/hermes_cli/kanban_parser.o src/hermes_cli/macos_signing.o src/hermes_cli/models_detect.o src/hermes_cli/plugin_events.o src/hermes_cli/plugins_state.o src/hermes_cli/prune_advisor.o src/hermes_cli/runtime_paths.o src/hermes_cli/runtime_state.o src/hermes_cli/setup_summary.o src/tools/approval_context.o src/tools/browser_use_shot.o src/tools/file_tools_paths.o src/tools/mcp_oauth_device.o src/tools/mcp_tool_content.o src/tools/tts_tool_plugins.o src/tools/tts_tool_speaker.o src/tools/web_tools_rescue.o src/tui_gateway/loop_noise.o src/tui_gateway/method_ctx.o src/acp_adapter/elicitation.o src/acp_adapter/permissions.o src/agent/activity_tracking.o src/agent/agent_init_ollama.o src/agent/api_request_hooks.o src/agent/compaction_events.o src/agent/custom_entry_pool.o src/agent/interrupt_control.o src/agent/onboarding_record.o src/agent/oneshot_footprint.o src/agent/plugin_live_notes.o src/agent/process_transport.o src/agent/provider_registry.o src/agent/redact_spaced_cfg.o src/agent/redact_structured.o src/agent/tool_call_batches.o src/agent/turn_facade_lease.o src/agent/turn_failure_copy.o src/agent/verification_stop.o src/cron/execution_identity.o src/cron/scheduler_liveness.o src/gateway/active_lease_io.o src/gateway/control_resolve.o src/gateway/host_rendezvous.o src/gateway/plugin_messages.o src/gateway/run_agent_cache.o src/gateway/run_auto_resume.o src/gateway/run_inbound_moa.o src/gateway/run_turn_runner.o src/gateway/session_display.o src/gateway/session_handoff.o src/gateway/session_ingress.o src/gateway/session_listing.o src/gateway/session_results.o src/gateway/session_surface.o src/gateway/session_tui_ops.o src/gateway/status_lock_dir.o src/gateway/webapp_listener.o src/hermes_cli/_old_updater.o src/hermes_cli/anon_sign_in.o src/hermes_cli/auth_minimax.o src/hermes_cli/chat_catalog.o src/hermes_cli/cli_shutdown.o src/hermes_cli/cli_snapshot.o src/hermes_cli/cloud_agents.o src/hermes_cli/data_cleanup.o src/hermes_cli/doctor_state.o src/hermes_cli/doctor_tools.o src/hermes_cli/gateway_chat.o src/hermes_cli/gateway_side.o src/hermes_cli/goal_command.o src/hermes_cli/main_desktop.o src/hermes_cli/models_local.o src/hermes_cli/plugin_oauth.o src/hermes_cli/source_build.o src/hermes_cli/source_check.o src/hermes_cli/source_stamp.o src/hermes_cli/update_stage.o src/hermes_cli/version_info.o src/hermes_cli/worktree_ops.o src/providers/plugin_reload.o src/tools/bot_live_delivery.o src/gateway/config_loader.o src/gateway/host_topology.o src/gateway/run_bootstrap.o src/gateway/run_idle_exit.o src/gateway/session_local.o src/gateway/session_prune.o src/gateway/session_shell.o src/gateway/session_usage.o src/gateway/status_served.o src/hermes_agent/__init__.o src/hermes_cli/_launchers.o src/hermes_cli/auth_codex.o src/hermes_cli/cli_render.o src/hermes_cli/github_api.o src/hermes_cli/kanban_ops.o src/hermes_cli/plugin_rpc.o src/plugins/plugin_loader.o src/pm/environments_adopt.o src/session_rules/imports.o src/session_rules/results.o src/tools/approval_ansi_c.o src/tools/approval_floors.o src/tools/approval_prompt.o src/tools/browser_session.o src/tools/mcp_tool_common.o src/tools/mcp_tool_config.o src/tools/mcp_tool_errors.o src/tools/mcp_tool_events.o src/tools/mcp_tool_schema.o src/tools/setup_beat_tool.o src/tools/tts_tool_openai.o src/tui_gateway/git_probe.o src/tui_gateway/pet_sheet.o src/tui_gateway/transport.o src/acp_adapter/provenance.o src/agent/auxiliary_health.o src/agent/client_lifecycle.o src/agent/image_token_cost.o src/agent/micro_compaction.o src/agent/reasoning_params.o src/agent/relay_compaction.o src/agent/turn_api_request.o src/agent/turn_loop_errors.o src/agent/voice_turn_route.o src/cron/blueprint_plugins.o src/cron/unreachable_retry.o src/gateway/approval_actor.o src/gateway/handoff_target.o src/gateway/owner_identity.o src/gateway/restart_notice.o src/gateway/run_background.o src/gateway/run_idle_gates.o src/gateway/session_client.o src/gateway/session_config.o src/gateway/session_events.o src/gateway/session_images.o src/gateway/session_refine.o src/gateway/session_worker.o src/hermes_cli/bundled_app.o src/hermes_cli/chat_launch.o src/hermes_cli/config_home.o src/hermes_cli/config_show.o src/hermes_cli/cpu_profile.o src/hermes_cli/doctor_live.o src/hermes_cli/plugin_host.o src/hermes_cli/post_update.o src/hermes_cli/profile_cmd.o src/hermes_cli/setup_quick.o src/hermes_cli/status_auth.o src/hermes_cli/stream_json.o src/pm/plugin_declarations.o src/session_rules/__init__.o src/gateway/restart_ack.o src/gateway/run_runtime.o src/gateway/session_a2a.o src/gateway/session_api.o src/gateway/session_bot.o src/hermes_cli/__init__.o src/hermes_cli/auth_xai.o src/hermes_cli/fs_utils.o src/hermes_cli/web_deps.o src/pm/build_operations.o src/pm/pnpm_release_age.o src/session_rules/media.o src/tools/approval_wmic.o src/tools/approval_yolo.o src/tools/mcp_tool_loop.o src/acp_adapter/__init__.o src/acp_adapter/__main__.o src/acp_adapter/commands.o src/agent/managed_worker.o src/agent/memory_context.o src/agent/prompt_refresh.o src/agent/provider_media.o src/agent/redact_connstr.o src/agent/redact_discord.o src/agent/redact_headers.o src/agent/review_signals.o src/agent/route_verdicts.o src/agent/session_source.o src/agent/surface_switch.o src/agent/terminal_image.o src/agent/turn_api_error.o src/agent/turn_preflight.o src/agent/vault_keychain.o src/cron/bot_chat_legacy.o src/cron/jobs_error_text.o src/cron/jobs_invariants.o src/gateway/platform_env.o src/gateway/run_watchers.o src/gateway/session_cron.o src/gateway/session_open.o src/hermes_cli/anon_auth.o src/hermes_cli/auth_nous.o src/hermes_cli/auth_qwen.o src/hermes_cli/setup_tts.o src/hermes_cli/turn_exit.o src/hermes_cli/venv_sync.o src/persistence/__init__.o src/plugins/plugin_utils.o src/pm/security_packages.o src/pm/worker_operations.o src/tools/approval_smart.o src/tools/arg_validation.o src/tools/mcp_tool_agent.o src/tools/mcp_tool_costs.o src/tools/mcp_tool_scope.o src/tools/set_theme_tool.o src/tools/tts_tool_local.o src/tui_gateway/__init__.o src/agent/auxiliary_hooks.o src/agent/fallback_notice.o src/agent/interrupt_scope.o src/agent/moa_alternation.o src/agent/onboarding_text.o src/agent/stream_delivery.o src/agent/turn_explainers.o src/agent/turn_stop_gates.o src/agent/turn_tool_round.o src/agent/turn_truncation.o src/cron/delivery_outcome.o src/cron/scheduler_thread.o src/cron/skill_blueprints.o src/cron/worker_bootstrap.o src/agent/acp_client.o src/agent/legacy_cli.o src/agent/turn_usage.o src/cron/missed_runs.o src/cron/occurrences.o src/cron/prompt_scan.o src/gateway/__init__.o src/gateway/run_turn.o src/hermes_cli/vault.o src/plugins/__init__.o src/pm/plugin_inputs.o src/pm/plugins_state.o src/pm/runtime_stage.o src/tools/skill_lock.o src/tools/voice_live.o src/acp_adapter/entry.o src/acp_adapter/tools.o src/agent/context_pin.o src/agent/i18n_layers.o src/agent/sleep_clock.o src/agent/turn_author.o src/agent/turn_facade.o src/agent/vault_store.o src/cron/cancellation.o src/cron/store_health.o src/gateway/run_goals.o src/gateway/run_voice.o src/hermes_cli/webapp.o src/pm/carried_extras.o src/pm/download_state.o src/pm/install_states.o src/pm/uv_cache_prune.o src/tools/feishu_lark.o src/tui_gateway/entry.o src/acp_adapter/events.o src/acp_adapter/server.o src/agent/cache_bounds.o src/agent/image_replay.o src/agent/proxy_bypass.o src/agent/redact_zhipu.o src/agent/secret_scope.o src/agent/served_model.o src/agent/usage_anchor.o src/agent/usage_gauges.o src/cron/dispatch_hold.o src/docker/build_agent.o src/gateway/run_topics.o src/hermes_cli/steward.o src/pm/build_compilers.o src/pm/plugin_eviction.o src/pm/uv_cache_health.o src/providers/__init__.o src/tools/discord_tool.o src/tools/mcp_liveness.o src/tools/tour_presets.o src/tui_gateway/render.o src/tui_gateway/server.o src/acp_adapter/catalog.o src/acp_adapter/content.o src/acp_adapter/session.o src/agent/idle_schedule.o src/agent/provider_base.o src/agent/redact_db_url.o src/agent/route_binding.o src/agent/runtime_scope.o src/agent/turn_api_call.o src/agent/turn_overflow.o src/agent/turn_recovery.o src/cron/job_definition.o src/gateway/host_attach.o src/gateway/media_fetch.o src/tools/neutts_synth.o src/hermes_yaml.o src/hermes_scope.o src/hermes_dir_acl.o src/session_rescue.o src/lineage_adoption.o src/storage_user_copy.o src/persistence_assembly.o src/hermes_state_registry.o src/toolset_distributions.o src/hermes_startup_watchdog.o src/hermes_state_loop_guard.o src/hermes_constants_scratch.o src/hermes_constants_scratch_stuck.o src/hermes_constants_scratch_detach.o src/pm/_uv.o src/pm/cli.o src/pm/lock.o src/pm/paths.o src/pm/shell.o src/pm/store.o src/pm/client.o src/pm/extras.o src/pm/launch.o src/pm/update.o src/pm/worker.o src/utils/env.o src/pm/install.o src/pm/network.o src/pm/package.o src/pm/receipt.o src/pm/runtime.o src/pm/testenv.o src/utils/urls.o src/pm/__init__.o src/pm/defaults.o src/pm/features.o src/pm/packages.o src/pm/progress.o src/pm/recovery.o src/pm/registry.o src/utils/files.o src/pm/build_env.o src/pm/libatomic.o src/pm/pip_check.o src/pm/workspace.o src/cron/__init__.o src/cron/job_args.o src/pm/downloader.o src/pm/filesystem.o src/pm/operations.o src/agent/__init__.o src/pm/_venv_entry.o src/pm/environment.o src/pm/publication.o src/pm/termux_libs.o src/pm/user_facing.o src/providers/base.o src/tools/__init__.o src/tui_gateway/ws.o src/utils/__init__.o src/agent/bot_chain.o src/agent/nous_wire.o src/agent/relay_cwd.o src/cron/extra_fire.o src/cron/quota_hold.o src/gateway/run_api.o src/pm/_marker_eval.o src/pm/environments.o src/pm/index_config.o src/pm/native_build.o src/tools/stt_lease.o src/acp_adapter/auth.o src/agent/auxiliary_key_rotation.o src/agent/auxiliary_local_runtime.o src/agent/auxiliary_wire.o src/agent/context_compressor_prompt_rules.o src/agent/conversation_compression_scaffolding.o src/agent/model_metadata_memo_bounds.o src/gateway/session_complete_path.o src/gateway/session_info_route.o src/gateway/session_local_close.o src/gateway/session_clarify_lock.o src/gateway/control_dispatch.o src/gateway/config_env.o src/gateway/bridged_env.o src/gateway/bot_loop_guard.o src/hermes_cli/config_check_diagnostics.o src/hermes_cli/models_cache_lock.o src/hermes_cli/models_profile_cache.o src/hermes_cli/plugin_command_dispatch.o src/hermes_cli/update_cmd_common.o src/hermes_cli/web_server_session_token.o src/cron/jobs_time.o src/cron/scheduler_tick.o src/tools/approval_detection.o src/tools/file_operations_common.o src/tools/image_generation_catalog.o src/tools/skills_hub_search.o src/plugins/platforms/telegram/telegram_draft_budget.o src/plugins/platforms/slack/adapter_slash_payload.o src/tui_gateway/connector_refusals.o src/tui_gateway/theme_bridge.o src/hermes_cli/cli_commands_worktree.o src/hermes_cli/cli_undo_hint.o src/agent/plan_limits.o src/agent/status_output.o src/tools/tool_labels.o src/tools/arg_coercion.o src/agent/agent_init_params.o src/agent/chat_completion_stream_monitor.o src/agent/context_compressor_prellm.o src/agent/delegation_provider.o src/agent/search_policy.o src/agent/vault_backends/bitwarden.o src/gateway/platforms/access_policy_mixin.o src/gateway/runtime_bootstrap.o src/gateway/session_hosted_controls.o src/gateway/session_hosted_rpc.o src/gateway/stream_consumer_transport.o src/gateway/turn_executor.o src/hermes_cli/cli_backtrack.o src/hermes_cli/dashboard_auth/public_paths.o src/hermes_cli/proxy/adapters/xai.o src/plugins/browser/_common.o src/plugins/browser/browserbase/provider.o src/plugins/browser/firecrawl/provider.o src/plugins/cron_providers/chronos/_nas_client.o src/plugins/model-providers/commandcode/__init__.o src/plugins/model-providers/deepseek/__init__.o src/plugins/model-providers/nebius-token-factory/__init__.o src/plugins/platforms/buzz/buzz_cursors.o src/plugins/platforms/discord/adapter_live_gates.o src/plugins/platforms/discord/adapter_thread_titles.o src/plugins/platforms/discord/dashboard_card.o src/plugins/platforms/discord/recovery.o src/plugins/platforms/feishu/adapter_choice_cards.o src/plugins/platforms/feishu/dashboard_card.o src/plugins/platforms/matrix/dashboard_card.o src/plugins/platforms/slack/dashboard_card.o src/plugins/platforms/telegram/dashboard_card.o src/plugins/web/brave_free/provider.o src/tools/approval_config_set.o src/tools/approval_hermes_store.o src/tools/approval_variation_selectors.o src/tools/bot_desktop/rfb_filter.o src/tools/computer_use/schema.o src/tools/environments/base_wait.o src/tools/mcp_tool_health.o src/tools/pty_query_responder.o src/tui_gateway/event_publisher.o src/plugins/platforms/matrix/send_targets.o src/plugins/platforms/slack/send_targets.o src/gateway/webhook_targets.o src/hermes_cli/cli_unknown_command.o src/persistence/sqlite/_dashboard_procs.o src/storage_limits.o src/agent/lazy_forward.o src/agent/model_metadata_memo_keys.o src/agent/redact_hermes_home.o src/agent/onboarding_facts.o src/pm/artifact_mirror.o src/hermes_cli/main_query_file.o src/cron/jobs_normalize.o src/cron/jobs_model_pin.o src/cron/scheduler_diagnostics.o src/hermes_cli/config_secret_display.o src/hermes_cli/sessions_owner.o src/hermes_cli/cli_footer_split.o src/gateway/session_cron_commands.o src/gateway/session_worker_spawn.o src/gateway/session_local_route.o src/gateway/session_operator.o src/gateway/session_command_sink.o src/gateway/platforms/api_server_response_identity.o src/tools/kanban_toolset_context.o src/tools/connectors/data_use.o src/tools/process_registry_wsl.o src/tools/approval_quoted_newlines.o src/tools/approval_pipe_launchers.o src/session_rules/activity.o src/persistence/core/health.o src/persistence/workers/receipts.o src/persistence/sqlite/conversation/_store/write_guards.o src/plugins/platforms/slack/adapter_config_readers.o src/plugins/google_meet/_jsonfile.o src/agent/tool_executor_deadline.o src/gateway/relay/descriptor.o src/gateway/session_contract.o src/hermes_agent/compat/moves.o src/hermes_agent/compat/required_names.o src/hermes_cli/config_defaults_voice.o src/hermes_cli/gateway_connections_ssh_programs.o src/persistence/conversation/search.o src/persistence/core/admin.o src/persistence/core/artifacts.o src/persistence/housekeeping/maintenance.o src/plugins/model-providers/actual/__init__.o src/plugins/platforms/bluebubbles/gateway_env.o src/plugins/platforms/dingtalk/gateway_env.o src/plugins/platforms/discord/adapter_hint.o src/plugins/platforms/discord/gateway_env.o src/plugins/platforms/email/gateway_env.o src/plugins/platforms/feishu/adapter_hint.o src/plugins/platforms/feishu/gateway_env.o src/plugins/platforms/matrix/adapter_hint.o src/plugins/platforms/mattermost/gateway_env.o src/plugins/platforms/qqbot/constants.o src/plugins/platforms/signal/gateway_env.o src/plugins/platforms/slack/adapter_hint.o src/plugins/platforms/telegram/adapter_hint.o src/plugins/platforms/wecom/gateway_env.o src/plugins/platforms/weixin/gateway_env.o src/plugins/platforms/whatsapp_cloud/gateway_env.o src/plugins/teams_pipeline/graph_client.o src/tools/browser_supervisor_frames.o src/tools/connectors/operation.o src/hermes_cli/toolset_scope.o src/gateway/platforms/base_media_flags.o src/hermes_cli/config_key_notices.o src/gateway/platforms/base_image_sniff.o src/tools/process_registry_wait.o src/hermes_cli/models_process_fingerprint.o src/tui_gateway/_env.o src/hermes_cli/config_issues.o src/gateway/bot_peer_notices.o src/persistence/sqlite/conversation/_store/local_migration.o src/agent/bedrock_service_tiers.o src/hermes_cli/shared_profile_warning.o src/cron/env_settings.o src/gateway/session_tool_events.o src/hermes_cli/colors.o src/session_rules/surfaces.o src/tools/clarify_batch.o src/hermes_cli/plugin_compat.o src/gateway/runtime_contract.o src/hermes_cli/models_fast_regions.o src/hermes_cli/cli_panel.o src/agent/auxiliary_profile_gates.o src/agent/credential_pool_timestamps.o src/hermes_cli/profiles_config.o src/tools/environments/path_utils.o src/gateway/session_skin.o src/hermes_cli/subcommands/bundles.o src/hermes_cli/subcommands/checkpoints.o src/hermes_cli/subcommands/completion.o src/hermes_cli/subcommands/moa.o src/tui_gateway/connector_payload.o src/hermes_cli/cli_tui_wrap.o src/gateway/hosted_room_task_input.o src/persistence/core/canonical.o src/persistence/core/keys.o src/gateway/session_bot_state.o src/agent/acp_openai_bridge_gaps.o src/agent/agent_gaps.o src/agent/agent_init.o src/agent/agent_message_payload.o src/agent/agent_runtime_helpers.o src/agent/agent_runtime_helpers_gaps.o src/agent/anthropic_credentials_gaps.o src/agent/async_utils.o src/agent/auxiliary_client_helpers2.o src/agent/auxiliary_client_runtime.o src/agent/azure_identity_adapter.o src/agent/background_review_gaps3.o src/agent/bedrock_adapter_gaps.o src/agent/browser_provider.o src/agent/chat_completion_helpers_gaps.o src/agent/checkpoints/checkpoint_manager_gaps.o src/agent/checkpoints/checkpoint_manager_ports.o src/agent/codex_event_projector.o src/agent/codex_headers_gaps.o src/agent/codex_responses_adapter_gaps.o src/agent/codex_runtime.o src/agent/conversation_loop_ports.o src/agent/lsp/lsp_servers_ports.o src/agent/memory_manager_gaps.o src/agent/memory_manager_helpers.o src/agent/memory_manager_ports.o src/agent/memory_manager_wrappers.o src/agent/memory_provider.o src/agent/memory_provider_gaps3.o src/agent/message_sanitization_ports.o src/agent/moa_loop_ports.o src/agent/moa_loop_wrappers.o src/agent/moa_trace_helpers.o src/agent/model_metadata_gaps.o src/agent/model_metadata_ports.o src/agent/ssl_guard.o src/agent/ssl_guard_ports.o src/agent/ssl_verify.o src/agent/stream_diag.o src/agent/stream_diag_ports.o src/agent/subdirectory_hints_ports.o src/agent/system_prompt_gaps.o src/agent/system_prompt_ports.o src/agent/think_scrubber.o src/agent/think_scrubber_helpers2.o src/agent/thinking_timeout_guidance.o src/agent/thread_scoped_output_ports.o src/agent/title.o src/agent/title_generator_ports.o src/agent/tool_dispatch_helpers_gaps.o src/agent/tool_executor_gaps.o src/agent/tool_guardrails_gaps.o src/agent/tool_guardrails_ports.o src/agent/trajectory_ports.o src/agent/transcript_repair_gaps.o src/agent/transcription_provider_ports.o src/agent/transports/codex_gaps.o src/agent/transports/transports_anthropic_ports.o src/agent/transports/transports_base_ports.o src/agent/transports/transports_bedrock_ports.o src/agent/transports/transports_codex_ports.o src/agent/transports/transports_init_ports.o src/agent/transports/transports_types_ports.o src/agent/tts_provider_ports.o src/agent/tts_registry_ports.o src/agent/turn_finalizer_gaps.o src/agent/turn_liveness_gaps.o src/agent/usage_pricing_ports.o src/agent/verif_evidence_wrappers.o src/agent/verify_hooks.o src/agent/video_gen_registry_ports.o src/agent/web_deps.o src/agent/web_search_registry_ports.o src/cli/port_agent_runtime_helpers_flow.o src/cli/port_gateway_agent_flow.o src/cli/port_gateway_windows_flow.o src/cli/port_tui_dispatch_flow.o src/cli/port_update_lock_flow.o src/gateway/cgroup_cleanup.o src/gateway/code_skew_ports.o src/gateway/config_helpers2.o src/gateway/cwd_placeholder.o src/gateway/delivery_ledger.o src/gateway/drain_control.o src/gateway/gateway_escape.o src/gateway/gateway_status_paths.o src/gateway/hosted_room_execution_policy_gaps.o src/gateway/hosted_room_links_gaps.o src/gateway/hosted_room_peer_gaps.o src/gateway/hosted_room_policy_checkpoint_gaps.o src/gateway/hosted_room_replicas_gaps.o src/gateway/hosted_rooms_gaps2.o src/gateway/hosted_rooms_gaps3.o src/gateway/kanban_watchers.o src/gateway/kanban_watchers_gaps.o src/gateway/media_policy.o src/gateway/media_repair_gaps.o src/gateway/memory_monitor.o src/gateway/message_timestamps_ports.o src/gateway/outbound/send_message_tool_gaps.o src/gateway/pairing_util.o src/gateway/platforms/api_server_gaps.o src/gateway/platforms/api_server_run_idempotency_gaps.o src/gateway/platforms/api_server_runs_gaps.o src/gateway/platforms/base_gaps.o src/gateway/platforms/msgraph_webhook_ports.o src/gateway/platforms/platforms_base_helpers2.o src/gateway/platforms/platforms_base_helpers3.o src/gateway/platforms/platforms_base_media.o src/gateway/platforms/platforms_helpers_ports.o src/gateway/platforms/webhook_filters.o src/gateway/platforms/webhook_ports.o src/gateway/profile_routing.o src/gateway/readiness.o src/gateway/relay/adapter.o src/gateway/relay/relay_adapter.o src/gateway/relay/relay_adapter_flow.o src/gateway/relay/relay_adapter_helpers.o src/gateway/relay/relay_adapter_ops.o src/gateway/relay/relay_adapter_ports.o src/gateway/relay/relay_auth.o src/gateway/relay/relay_auth_helpers2.o src/gateway/relay/relay_descriptor.o src/gateway/relay/relay_init_ports.o src/gateway/relay/relay_media.o src/gateway/relay/relay_transport.o src/gateway/relay/relay_transport_ports.o src/gateway/relay/relay_ws_transport.o src/gateway/relay/relay_ws_transport_ports.o src/gateway/relay/relay_ws_transport_py_parity.o src/gateway/relay/ws_transport.o src/gateway/restart.o src/gateway/restart_gaps.o src/gateway/rich_sent_store.o src/gateway/run_helpers.o src/gateway/run_ports.o src/gateway/session_context_ports.o src/gateway/session_helpers2.o src/gateway/session_ports.o src/gateway/session_stall.o src/gateway/shutdown_flush_ports.o src/gateway/shutdown_watchdog.o src/gateway/shutdown_watchdog_ports.o src/gateway/slash_command_guards.o src/gateway/slash_commands_gaps.o src/gateway/status_gaps.o src/gateway/status_ports.o src/gateway/status_wrappers.o src/gateway/sticker_cache.o src/gateway/stream_consumer_gaps.o src/gateway/stream_dispatch_ports.o src/gateway/streaming_tts_consumer.o src/gateway/systemd_notify_ports.o src/hermes_cli/npm_engine.o src/hermes_cli/observability/relay_shared_metrics_flow.o src/hermes_cli/observability/shared_metrics_sender_gaps.o src/hermes_cli/plugin_dev_gaps.o src/hermes_cli/plugins_ports.o src/hermes_cli/profiles_gaps.o src/hermes_cli/proxy/sse_done_gaps.o src/hermes_cli/runtime_provider_gaps.o src/hermes_cli/scan_venv_blockers.o src/hermes_cli/session_export_md_wrappers.o src/hermes_cli/session_recovery_copy.o src/hermes_cli/session_recovery_gaps.o src/hermes_cli/session_recovery_meta.o src/hermes_cli/session_recovery_orphans.o src/hermes_cli/sizefmt.o src/hermes_cli/sqlite_safe_read.o src/hermes_cli/subcommands/peer.o src/hermes_cli/tools_config_gaps.o src/hermes_cli/tools_config_more.o src/plugins/teams_pipeline/microsoft_graph_client_ports.o src/tools/approval_gaps.o src/tools/approval_wrappers.o src/tools/audio_container.o src/tools/binary_extensions.o src/tools/bot_relay_gaps.o src/tools/browser_lightpanda_gaps.o src/tools/browser_supervisor_redact.o src/tools/browser_tool_gaps.o src/tools/browser_use_cli_gaps.o src/tools/clarify_gateway_gaps.o src/tools/clarify_tool.o src/tools/close_preview_tool.o src/tools/close_terminal_tool.o src/tools/code_execution_tool.o src/tools/code_execution_tool_gaps.o src/tools/code_kernel_gaps.o src/tools/computer_use/computer_use_backend.o src/tools/computer_use/computer_use_doctor_mcp.o src/tools/computer_use/computer_use_noop.o src/tools/computer_use/computer_use_schema.o src/tools/computer_use/computer_use_wayland.o src/tools/process.o src/tools/read_extract_gaps.o src/tools/read_preview_tool.o src/tools/read_window_tool.o src/tools/registry_cache.o src/tools/research_helpers.o src/tools/self_repo_guard.o src/tools/session_search_tool.o src/tools/session_search_tool_gaps.o src/tools/shell_heredoc.o src/tools/skill_ledger_gaps.o src/tools/skill_prereqs.o src/tools/skill_usage_wrappers.o src/tools/skillevaluator_scan.o src/tools/skills_guard_gaps.o src/tools/skills_hub_gaps.o src/tools/skills_sync_ports.o src/tools/skills_tool_ports.o src/tools/subagent_worktree.o src/tools/terminal_scope_gaps.o src/tools/terminal_tool_gaps.o src/tools/terminal_tool_wrappers.o src/tools/todo.o src/tools/tool_backend_helpers.o src/agent/context_compressor_flow2.o src/agent/conversation_loop_flow.o src/agent/learning_graph_flow.o src/agent/prompt_cache_clock_flow.o src/agent/tool_executor_flow.o src/cli/cli_py_flow.o src/cli/cli_tui_mixin_flow.o src/cli/config_backend_flow.o src/cli/config_flow2.o src/cli/gateway_agent_cli_flow.o src/cli/gateway_cli_flow.o src/cli/gateway_windows_flow2.o src/cli/left_core_migration_flow.o src/cli/main_cli_flow2.o src/cli/models_flow2.o src/cli/update_lock_flow2.o src/cli/web_git_flow.o src/gateway/gateway_run_flow2.o src/gateway/gateway_session_flow.o src/gateway/platforms/buzz_adapter_flow.o src/gateway/platforms/discord_adapter_flow.o src/gateway/platforms/feishu_adapter_flow.o src/gateway/platforms/google_chat_adapter_flow.o src/gateway/platforms/matrix_adapter_flow.o src/gateway/platforms/photon_adapter_flow.o src/gateway/platforms/slack_adapter_flow.o src/gateway/platforms/telegram_adapter_flow.o src/gateway/platforms/whatsapp_adapter_flow.o src/gateway/run_inbound_flow.o src/gateway/run_notifications_flow.o src/gateway/run_shutdown_flow2.o src/gateway/session_authority_flow.o src/gateway/session_controls_flow.o src/gateway/tui_methods_flow.o src/gateway/tui_methods_tools_flow.o src/gateway/tui_server_flow.o src/gateway/turn_progress_flow.o src/hermes_cli/kanban_dashboard_flow.o src/hermes_cli/pm_packages_flow.o src/persistence/conversation_messages_flow.o src/persistence/conversation_sessions_flow.o src/persistence/sqlite_sessions_flow.o src/tools/browser_launcher_flow2.o src/tools/environments/docker_env_flow.o src/tools/kanban_tools_flow.o src/tools/terminal_tool_flow2.o src/cli/port_gw_session_store_flow.o src/cli/port_runtime_provider_flow.o src/cli/port_sessions_cmd_flow.o src/agent/display_flow.o src/gateway/hosted_room_driver_flow2.o src/cli/setup_flow2.o src/agent/agent_runtime_helpers_flow2.o src/cli/relay_shared_metrics_flow2.o src/gateway/platforms/platforms_base_flow2.o src/agent/conversation_compression_flow2.o src/cli/profiles_flow.o src/gateway/scale_to_zero_flow.o src/tools/code_execution_flow.o src/agent/transports/codex_transport_flow.o src/tools/vision_analysis_flow.o src/agent/auxiliary_client_flow2.o src/gateway/hosted_room_driver_flow.o src/agent/transports/anthropic_transport_flow.o src/tools/file_operations_flow.o src/agent/prompt_caching_flow.o src/cli/auth_flow.o src/cli/gateway_migrate_flow2.o src/tools/web_tools_flow.o src/tools/browser_tools_flow.o src/gateway/session_policy_flow2.o src/agent/moa_loop_flow2.o src/cli/curator_flow.o src/tools/skills_guard_flow.o src/gateway/platforms/openai_routes_flow2.o src/agent/anthropic_convert_flow2.o src/gateway/run_notifications_flow2.o src/cli/config_migrations_flow2.o src/agent/context_references_flow.o src/tools/sandbox_flow.o src/gateway/status_phrases_flow.o src/tools/terminal_hints_flow.o src/cli/model_metadata_flow.o src/gateway/run_startup_flow2.o src/cli/skill_usage_flow.o src/cli/update_cmd_flow.o src/agent/transports/openai_transport_flow.o src/tools/transcription_flow.o src/cli/models_detect_flow.o src/tools/lsp_tools_flow.o src/agent/tool_dispatch_flow.o src/cli/plugins_cmd_flow.o src/cli/web_server_flow.o src/cli/kanban_adapter_flow.o src/gateway/platforms/platform_registry_flow.o src/cli/relay_metrics_flow.o src/tools/mcp_oauth_flow2.o src/agent/scheduler_flow2.o src/cli/main_desktop_flow2.o src/gateway/tui_methods_sessions_flow.o src/gateway/platforms/signal_adapter_flow.o src/agent/transports/gemini_transport_flow.o src/gateway/relay_protocol_flow.o src/gateway/tui_methods_kanban_flow.o src/gateway/platforms/slack_adapter_flow2.o src/persistence/conversation_store_flow.o src/gateway/platforms/feishu_adapter_flow2.o src/cli/kanban_db_flow2.o src/gateway/run_inbound_flow3.o src/gateway/platforms/telegram_adapter_flow2.o src/gateway/tui_methods_config_flow.o src/persistence/sqlite_messages_flow.o src/gateway/platforms/irc_adapter_flow.o src/agent/tool_result_processing_flow.o src/cli/gateway_inbound_flow2.o src/gateway/tui_methods_settings_flow.o src/gateway/session_kanban_flow2.o src/persistence/store_rules_flow.o src/agent/error_classifier_flow.o src/cli/console_engine_flow.o src/cli/goals_flow.o src/cli/projects_db_flow.o src/gateway/platforms/buzz_adapter_flow2.o src/gateway/platforms/photon_adapter_flow2.o src/gateway/tui_methods_audit_flow.o src/agent/account_usage_flow2.o src/tools/image_tools_flow.o src/cli/doctor_flow.o src/agent/usage_tracking_flow.o src/cli/shared_metrics_flow.o src/agent/conversation_loop_flow2.o src/persistence/store_identity_flow.o src/gateway/tui_methods_goals_flow.o src/cli/main_cli_flow3.o src/gateway/gateway_run_flow3.o src/cli/kanban_cli_flow.o src/cli/config_backend_flow2.o src/gateway/tui_methods_projects_flow.o src/agent/transports/chat_completions_flow2.o src/gateway/platforms/google_chat_adapter_flow2.o src/cli/route_identity_flow.o src/gateway/platforms/matrix_adapter_flow2.o src/gateway/platforms/whatsapp_adapter_flow2.o src/cli/gateway_agent_flow2.o src/cli/cli_tui_mixin_flow2.o src/gateway/platforms/platforms_base_flow3.o src/cli/config_env_flow2.o src/gateway/session_context_flow2.o src/cli/observability_metrics_flow.o src/cli/config_defaults_flow.o src/agent/context_budget_flow.o src/cli/plugins_cmd_flow2.o src/gateway/tui_methods_cron_flow.o src/agent/moa_loop_flow3.o src/gateway/session_controls_flow2.o src/agent/transports/transport_types_flow2.o src/cli/gateway_cli_flow2.o src/cli/relay_metrics_flow2.o src/gateway/run_notifications_flow3.o src/cli/kanban_db_dispatch_flow2.o src/gateway/session_finite_flow2.o src/cli/process_identity_flow2.o src/agent/transports/bedrock_transport_flow2.o src/gateway/scale_to_zero_flow2.o src/cli/update_cmd_flow2.o src/cli/cli_output_flow.o src/cli/gateway_enroll_flow.o src/cli/config_migration_flow2.o src/gateway/tui_methods_status_flow.o src/agent/i18n_flow2.o src/agent/account_usage_nous_flow2.o src/tools/delegate_progress_flow2.o src/tools/delegate_prefix_gate_flow2.o src/tools/process_registry_notifications_flow2.o src/persistence/store_rules_flow2.o src/cli/gateway_multiplex_flow2.o src/agent/transports/anthropic_transport_flow2.o src/agent/anthropic_adapter_flow2.o src/gateway/platforms/telegram_adapter_flow3.o src/cli/doctor_flow2.o src/tools/file_tools_flow2.o src/gateway/run_busy_flow2.o src/cli/session_recovery_flow2.o src/tools/terminal_tool_flow3.o src/gateway/tui_methods_voice_flow.o src/gateway/session_policy_flow4.o src/cron/cron_jobs_flow2.o src/cli/learning_graph_flow2.o src/agent/context_references_flow2.o src/gateway/tui_methods_tools_flow2.o src/plugins/kanban/dashboard/kanban_plugin_api_flow2.o src/cli/config_flow2b.o src/agent/model_metadata_flow2.o src/session_rules/turns_flow2.o src/agent/redact_flow2.o src/tools/fuzzy_match_flow2.o src/cli/skills_hub_flow2.o src/cli/web_files_flow2.o src/tools/async_delegation_flow2.o src/gateway/platforms/slack_adapter_flow3.o src/gateway/platforms/feishu_adapter_flow3.o src/cli/gw_conn_normalize_flow2.o src/gateway/tui_server_flow2.o src/cli/gw_conn_ssh_flow2.o src/cli/tools_config_flow2.o src/cron/lifecycle_guard_flow2.o src/acp_adapter/acp_tools_flow2.o src/agent/display_flow2.o src/agent/models_dev_flow2.o src/persistence/store_messages_flow2.o src/cli/auth_flow2.o src/cli/plugins_install_scan_flow2.o src/gateway/platforms/photon_auth_flow2.o src/cron/scheduler_delivery_flow2.o src/agent/prompt_cache_clock_flow2.o src/agent/error_classifier_flow2.o src/agent/pet/generate/pet_atlas_flow2.o src/cli/web_profiles_flow2.o src/cli/gw_conn_ssh_local_flow2.o src/agent/nous_rate_guard_flow2.o src/cron/cron_incidents_flow2.o src/tools/session_search_gaps_flow2.o src/cli/commands_flow3.o src/agent/chat_completion_helpers_flow2.o src/cli/plugins_install_flow2.o src/cli/backup_flow2.o src/cli/doctor_tools_flow2.o src/agent/verif_evidence_flow2.o src/cli/gateway_flow3.o src/agent/auxiliary_client_flow3.o src/cli/gw_connections_flow2.o src/gateway/tui_methods_session_flow2.o src/session_rules/turns_flow3.o src/agent/context_compressor_flow3.o src/gateway/relay/relay_descriptor_flow3.o src/agent/tool_guardrails_flow3.o src/gateway/relay/relay_auth_flow3.o src/cli/relay_metrics_flow3.o src/agent/monitoring/monitoring_events_flow3.o src/gateway/platforms/slack_adapter_flow4.o src/gateway/platforms/feishu_adapter_flow4.o src/session_rules/turns_flow4.o src/tools/terminal_tool_flow4.o src/gateway/session_transcript_rows_flow2.o src/gateway/session_transcript_subagent_flow2.o src/gateway/tui_methods_voice_flow2.o src/agent/auxiliary_client_flow4.o src/cli/gateway_flow4.o src/agent/chat_completion_helpers_flow3.o src/session_rules/turns_flow5.o src/agent/context_compressor_flow4.o src/cli/backup_flow3.o src/cli/web_profiles_flow3.o src/gateway/relay/relay_descriptor_flow4.o src/agent/memory_manager_flow2.o src/agent/context_references_flow3.o src/agent/session_search_flow3.o src/cli/backup_flow4.o src/cli/cron_flow4.o src/gateway/web_server_flow5.o src/tools/memory_tool_flow4.o src/cli/runtime_provider_flow2.o src/agent/memory_manager_flow3.o src/gateway/tui_hosted_room_service_flow.o src/persistence/executions_worker_flow.o src/gateway/platforms/feishu_adapter_flow5.o src/acp_adapter/acp_gateway_server_flow.o src/gateway/platforms/api_server_runs_flow2.o src/gateway/tui_server_flow3.o src/gateway/platforms/telegram_adapter_flow6.o src/gateway/platforms/discord_adapter_flow5.o src/gateway/tui_hosted_room_service_flow2.o src/persistence/executions_worker_flow2.o src/acp_adapter/acp_gateway_server_flow2.o src/gateway/platforms/matrix_adapter_flow3.o src/gateway/platforms/telegram_adapter_flow7.o src/gateway/platforms/discord_adapter_flow6.o src/gateway/platforms/slack_adapter_flow5.o src/gateway/platforms/matrix_adapter_flow4.o src/gateway/platforms/telegram_adapter_flow8.o src/gateway/platforms/discord_adapter_flow7.o src/gateway/platforms/slack_adapter_flow6.o src/gateway/platforms/matrix_adapter_flow5.o src/gateway/platforms/feishu_adapter_flow6.o src/agent/context_compressor_flow5.o src/gateway/platforms/google_chat_flow2.o src/gateway/gateway_run_flow4.o src/agent/context_compressor_flow6.o src/gateway/platforms/feishu_adapter_flow7.o src/gateway/platforms/matrix_adapter_flow6.o src/agent/context_compressor_flow7.o src/gateway/platforms/feishu_adapter_flow8.o src/gateway/platforms/matrix_adapter_flow7.o src/agent/context_compressor_flow8.o src/gateway/platforms/telegram_adapter_flow9.o src/gateway/platforms/discord_adapter_flow8.o src/gateway/platforms/slack_adapter_flow7.o src/gateway/platforms/feishu_adapter_flow9.o src/gateway/platforms/telegram_adapter_flow10.o src/gateway/platforms/discord_adapter_flow9.o src/gateway/platforms/slack_adapter_flow8.o src/gateway/platforms/feishu_adapter_flow10.o src/gateway/platforms/telegram_adapter_flow11.o src/gateway/platforms/discord_adapter_flow10.o src/gateway/platforms/slack_adapter_flow9.o src/gateway/platforms/feishu_adapter_flow11.o src/gateway/platforms/telegram_adapter_flow12.o src/gateway/platforms/discord_adapter_flow11.o src/gateway/platforms/slack_adapter_flow10.o src/gateway/platforms/feishu_adapter_flow12.o src/gateway/platforms/telegram_adapter_flow13.o src/gateway/platforms/discord_adapter_flow12.o src/gateway/platforms/slack_adapter_flow11.o src/gateway/platforms/feishu_adapter_flow13.o src/gateway/platforms/slack_adapter_flow12.o src/gateway/platforms/telegram_adapter_flow14.o src/gateway/platforms/discord_adapter_flow13.o src/gateway/platforms/feishu_adapter_flow14.o src/gateway/platforms/telegram_adapter_flow15.o src/gateway/platforms/discord_adapter_flow14.o src/gateway/platforms/slack_adapter_flow13.o src/gateway/platforms/feishu_adapter_flow15.o src/gateway/platforms/telegram_adapter_flow16.o src/gateway/platforms/discord_adapter_flow15.o src/gateway/platforms/slack_adapter_flow14.o src/gateway/platforms/matrix_adapter_flow8.o src/chat_render.o src/desktop_models.o src/desktop_profiles.o src/desktop_sessions.o src/gateway_probe.o src/session_db.o src/slermes_init_shim.o src/tools/delegate_tool_resumable.o src/tools/file_tools_write_guards.o src/tools/process_registry_access.o src/tools/skills_sync_bundled_ops.o src/tools/transcription_streaming.o src/tools/vision_tools_image_prep.o src/tui_gateway/command_discovery.o src/tui_gateway/methods_bot_relay.o src/tui_gateway/methods_free_tier.o src/tui_gateway/methods_subagents.o src/tui_gateway/notification_park.o src/tui_gateway/session_lifecycle.o src/agent/auxiliary_codex_response.o src/agent/context_compressor_prune.o src/agent/credential_auth_recovery.o src/agent/primary_runtime_snapshot.o src/agent/redact_credential_stores.o src/cron/scheduler_delivery_origin.o src/cron/scheduler_detached_worker.o src/gateway/kanban_watchers_common.o src/gateway/runtime_downgrade_exit.o src/gateway/session_local_adoption.o src/gateway/session_local_recovery.o src/gateway/session_managed_worker.o src/gateway/session_mutation_model.o src/gateway/session_policy_refresh.o src/gateway/session_route_identity.o src/gateway/session_route_recovery.o src/gateway/slash_commands_session.o src/gateway/stream_consumer_fences.o src/gateway/stream_consumer_redact.o src/hermes_cli/_early_recovery_zip.o src/hermes_cli/backup_cron_prompts.o src/hermes_cli/cli_browser_gateway.o src/hermes_cli/cli_chat_error_copy.o src/hermes_cli/cli_chat_turn_mixin.o src/hermes_cli/commands_completion.o src/hermes_cli/config_defaults_env.o src/hermes_cli/config_provider_env.o src/hermes_cli/config_runtime_keys.o src/hermes_cli/config_value_checks.o src/hermes_cli/desktop_build_reuse.o src/hermes_cli/doctor_connectivity.o src/hermes_cli/free_tier_bootstrap.o src/hermes_cli/gateway_bundled_env.o src/hermes_cli/gateway_connections.o src/hermes_cli/gateway_host_record.o src/hermes_cli/gateway_restart_env.o src/hermes_cli/gateway_runtime_cli.o src/hermes_cli/kanban_db_workspace.o src/hermes_cli/kanban_resume_guard.o src/hermes_cli/left_core_migration.o src/hermes_cli/main_install_repair.o src/hermes_cli/main_platform_setup.o src/hermes_cli/main_provider_setup.o src/hermes_cli/plugins_auto_update.o src/hermes_cli/plugins_cmd_catalog.o src/hermes_cli/plugins_cmd_inspect.o src/hermes_cli/plugins_cmd_install.o src/hermes_cli/plugins_cmd_listing.o src/hermes_cli/plugins_session_env.o src/hermes_cli/plugins_transaction.o src/hermes_cli/sessions_cmd_browse.o src/hermes_cli/tui_worktree_resume.o src/hermes_cli/update_launch_files.o src/hermes_cli/update_pause_record.o src/hermes_cli/update_serve_resume.o src/hermes_cli/web_read_coalescing.o src/hermes_cli/web_server_boundary.o src/gateway/session_group_controls.o src/gateway/session_hosted_members.o src/gateway/session_hosted_service.o src/persistence/authority/subagents.o src/plugins/security-guidance/patterns.o src/persistence/workers/delegations.o src/tools/connectors/gateway/wire.o src/plugins/platforms/discord/adapter_exec_approval_view.o src/plugins/platforms/teams/summary_writer.o src/persistence/workers/compaction.o src/agent/vault_backends/onepassword.o src/plugins/model-providers/gemini/client.o src/persistence/sqlite/conversation/usage.o src/gateway/platforms/session_model.o src/hermes_cli/local_runtime/processes.o src/persistence/sqlite/gateway/hosted_room_peers.o src/persistence/sqlite/admin/profile_repair.o src/persistence/sqlite/admin/_doctor.o src/gateway/kanban_watchers_dispatcher.o src/plugins/web/xai/provider.o src/persistence/sqlite/housekeeping/profile_identity.o src/persistence/sqlite/admin/_schema_history.o src/hermes_cli/model_setup_flows_bedrock.o src/gateway/api_server_turn_boundary.o src/gateway/platforms/api_server_response_store.o src/gateway/platforms/event.o src/gateway/run_notifications_restart.o src/plugins/google_meet/realtime/openai_client.o src/gateway/run_shutdown_home_notice.o src/hermes_cli/cli_status_bar_mixin.o src/persistence/sqlite/admin/_maintenance.o src/persistence/authority/admissions.o src/persistence/core/errors.o src/gateway/relay/media.o src/persistence/sqlite/authority/admissions.o src/agent/context_compressor_window.o src/hermes_cli/cli_commands_handoff.o src/cron/scheduler_delivery_standalone.o src/plugins/platforms/matrix/voice_mention.o src/agent/vault_backends/plugin.o src/persistence/gateway/routing.o src/persistence/sqlite/gateway/_delivery_ledger.o src/tools/environments/ssh.o src/persistence/sqlite/_backend.o src/plugins/platforms/telegram/telegram_media_flood.o src/persistence/sqlite/authority/runtime.o src/hermes_cli/plugin_isolation_audit.o src/persistence/sqlite/_state/errors.o src/gateway/stream_consumer_fallback.o src/plugins/platforms/discord/adapter_media.o src/gateway/run_notifications_ownership.o src/persistence/sqlite/conversation/usage_analytics.o src/tools/computer_use/cua_backend_input.o src/plugins/platforms/telegram/telegram_held_inbound.o src/plugins/platforms/telegram/telegram_vault_code.o src/agent/context_compressor_telemetry.o src/agent/runtime_session_compression.o src/persistence/sqlite/authority/bindings.o src/tools/connectors/tool.o src/tools/browser_supervisor_capture.o src/plugins/platforms/qqbot/adapter.o src/gateway/run_notifications_update.o src/plugins/platforms/slack/adapter_vault_code.o src/plugins/teams_pipeline/graph_auth.o src/gateway/platforms/base_chunking.o src/persistence/housekeeping/profile_identity.o src/agent/checkpoints/pruning.o src/persistence/sqlite/workers/executions.o src/hermes_cli/subcommands/sessions.o src/persistence/sqlite/conversation/_store/prune_rewrite.o src/gateway/platforms/whatsapp_common.o src/persistence/sqlite/gateway/hosted_room_replicas.o src/tools/computer_use/cua_backend_daemon.o src/agent/conversation_compression_manual.o src/plugins/platforms/discord/voice_mixer.o src/tui_gateway/hosted_room_peer_transport.o src/hermes_cli/shared_metrics_contract_tables.o src/hermes_cli/observability/shared_metrics_subscriber.o src/gateway/platforms/webhook_coalesce.o src/agent/credential_pool_selection.o src/plugins/platforms/discord/adapter_vault_code.o src/agent/conversation_compression_watermark.o src/hermes_cli/gateway_restart_watcher.o src/hermes_cli/config_model_section.o src/agent/context_compressor_durable.o src/agent/context_compressor_summary.o src/plugins/platforms/telegram/telegram_overflow_edit.o src/plugins/google_meet/audio_bridge.o src/persistence/sqlite/conversation/_store/worker_lifecycle.o src/plugins/platforms/wecom/wecom_crypto.o src/plugins/web/keenable/provider.o src/agent/chat_completion_helpers_relay.o src/persistence/authority/vocabulary.o src/persistence/workers/vocabulary.o src/hermes_cli/subcommands/worktree.o src/persistence/sqlite/conversation/_errors.o src/hermes_cli/cli_auto_maintenance.o src/persistence/sqlite/authority/_local_migration.o src/gateway/run_delivery_queue_watch.o src/tools/process_registry_handoff.o src/plugins/platforms/slack/adapter_image_batch.o src/plugins/platforms/google_chat/setup_files.o src/gateway/session_vault_code_card.o src/plugins/google_meet/node/server.o src/tui_gateway/hosted_room_peer_deadline.o src/agent/context_compressor_typed_input.o src/plugins/teams_pipeline/store.o src/hermes_cli/gateway_runtime_progress.o src/gateway/run_turn_runner_approval_text.o src/persistence/sqlite/workers/_executions.o src/gateway/windows_proactor_accept.o src/plugins/platforms/telegram/telegram_send_path_recovery.o src/hermes_cli/windows_appinstaller_update.o src/persistence/sqlite/conversation/search.o src/plugins/web/openai_native/provider.o src/agent/prompt_cache_lifecycle/economics.o src/hermes_cli/update_cmd_stale_survivors.o src/tools/connectors/dispatch.o src/hermes_platform/host/gpu_adapters.o src/gateway/stream_consumer_directives.o src/plugins/web/exa/provider.o src/persistence/sqlite/conversation/_store/api_content.o src/tools/connectors/contract.o src/gateway/run_turn_bubble_cleanup.o src/gateway/platforms/tcp_site.o src/tui_gateway/slash_worker_client.o src/agent/pet/generate/prompts.o src/plugins/teams_pipeline/models.o src/agent/auxiliary_stream_watchdog.o src/tools/code_execution_lifecycle.o src/tools/terminal_tool_macos_open.o src/agent/secret_sources/command.o src/hermes_cli/gateway_supervised_restart.o src/gateway/hosted_rooms_legacy_import.o src/tui_gateway/methods_session_seed.o src/plugins/platforms/signal/standalone.o src/persistence/sqlite/gateway/_hosted_rooms_legacy_import.o src/tui_gateway/hosted_room_router_call.o src/persistence/sqlite/authority/_binding_api.o src/agent/credential_pool_model_cooldowns.o src/agent/conversation_compression_codex.o src/hermes_cli/gateway_runtime_attach.o src/plugins/browser/browser_use/provider.o src/hermes_cli/shared_session_attach.o src/plugins/platforms/telegram/telegram_flood_cooldown.o src/plugins/platforms/matrix/adapter_html.o src/hermes_cli/gateway_setup_service.o src/hermes_cli/sandbox_image_switch.o src/plugins/platforms/telegram/telegram_pool.o src/agent/auxiliary_client_registry.o src/plugins/platforms/qqbot/register.o src/hermes_cli/runtime_provider_missing_key.o src/gateway/outbound/targets.o src/hermes_cli/auth_declared_providers.o src/hermes_cli/gateway_launchd_runtime.o src/tools/approval_windows_hardline.o src/tools/skills_guard_credentials.o src/persistence/sqlite/conversation/_store/todo.o src/hermes_cli/subcommands/computer_use_screen.o src/gateway/platforms/base_delivery_ledger.o src/tui_gateway/methods_tools_plugin_servers.o src/tools/connectors/gateway/errors.o src/agent/monitoring/emitter.o src/tools/connectors/search.o src/hermes_cli/dashboard_auth/refresh_singleflight.o src/gateway/platforms/api_server_room_dispatch.o src/gateway/hosted_room_authority_history.o src/gateway/run_turn_runner_approval_settle.o src/tui_gateway/hosted_room_prepare_stall.o src/agent/checkpoints/manager_profile_rename.o src/hermes_cli/desktop_update_verify.o src/agent/turn_recovery_autorecover.o src/gateway/run_inbound_unauthorized.o src/gateway/platforms/api_server_turn_boundary.o src/hermes_cli/observability/shared_metrics_drift.o src/plugins/platforms/telegram/telegram_voice_mention.o src/hermes_cli/kanban_db_default_assignee.o src/hermes_cli/web_server_owner_exit.o src/hermes_cli/web_routers/status_public.o src/persistence/sqlite/conversation/_store/telegram.o src/hermes_cli/subcommands/codex_runtime.o src/tui_gateway/methods_session_model_guard.o src/plugins/platforms/discord/adapter_opus.o src/hermes_cli/web_server_idle_proof.o src/hermes_cli/gateway_chat_listing.o src/hermes_cli/update_cmd_fleet_gatewayless.o src/hermes_cli/web_whatsapp_session.o src/plugins/platforms/telegram/telegram_typing_gate.o src/persistence/core/context.o src/plugins/platforms/matrix/media_download.o src/gateway/platforms/base_streaming_tts.o src/persistence/sqlite/conversation/_store/compaction_display.o src/tui_gateway/methods_plugin_catalog.o src/plugins/platforms/telegram/telegram_media_budget.o src/gateway/platforms/api_server_request_options.o src/hermes_cli/web_server_static_assets.o src/hermes_cli/main_profile_recovery.o src/plugins/platforms/discord/adapter_authorization.o src/hermes_cli/config_section_guard.o src/gateway/run_turn_runner_approval_transport.o src/persistence/sqlite/conversation/_store/mutation_transcript.o src/hermes_cli/subcommands/browser.o src/persistence/sqlite/services.o src/gateway/relay/adapter_inbound_media.o src/persistence/sqlite/conversation/_store/session_list.o src/plugins/google_meet/node/registry.o src/hermes_cli/subcommands/verify.o src/tools/process_registry_env_scope.o src/hermes_cli/update_cmd_fleet_checkout.o src/tools/environments/fd_wait.o src/hermes_cli/sessions_cmd_journal_mode.o src/persistence/sqlite/housekeeping/_retirement.o src/tools/bot_desktop/thumbnail.o src/hermes_cli/subcommands/connectors.o src/plugins/platforms/qqbot/gateway_env.o src/plugins/platforms/telegram/telegram_status.o src/hermes_cli/subcommands/secrets.o src/hermes_cli/web_routers/uploads.o src/plugins/platforms/slack/adapter_reactions_context.o src/agent/agent_runtime_helpers_placeholders.o src/hermes_cli/subcommands/fallback.o src/plugins/teams_pipeline/runtime.o src/gateway/platforms/webhook_delivery.o src/tools/process_registry_results.o src/gateway/relay/prompt_resolvers.o src/hermes_cli/dashboard_auth/audit.o src/plugins/platforms/telegram/telegram_split.o src/hermes_agent/compat/finder.o src/hermes_cli/sessions_cmd_repair_profiles.o src/hermes_cli/plugin_validate_locales.o src/persistence/core/conventions.o src/gateway/session_worker_delegation.o src/persistence/sqlite/_state/lockowners.o src/tools/connectors/portal/tools_cache.o src/hermes_cli/gateway_connections_watch.o src/gateway/session_worker_reservation.o src/plugins/platforms/discord/adapter_component_auth.o src/hermes_platform/resolver/availability.o src/plugins/platforms/signal/setup.o src/plugins/config_backends/remote/destination.o src/persistence/sqlite/conversation/_store/local_lineage.o src/plugins/memory/query_rewrite.o src/hermes_agent/compat/paths.o src/hermes_cli/update_cmd_validation.o src/agent/pet/state.o src/persistence/sqlite/conversation/_store/model_config.o src/hermes_cli/gateway_command_errors.o src/gateway/platforms/_http_client_limits.o src/persistence/sqlite/conversation/_store/media.o src/hermes_cli/gateway_chat_startup.o src/hermes_cli/web_routers/ui_preferences.o src/tui_gateway/free_tier_task_done.o src/gateway/platforms/base_post_delivery.o src/plugins/platforms/telegram/telegram_live_gates.o src/hermes_cli/subcommands/clipboard.o src/plugins/platforms/discord/ffmpeg_utils.o src/plugins/platforms/matrix/session_proxy.o src/hermes_cli/plugin_host_http_auth.o src/plugins/platforms/qqbot/crypto.o src/hermes_cli/web_asset_mime_types.o src/plugins/platforms/feishu/feishu_admission_diagnostics.o src/plugins/platforms/telegram/telegram_resolved_prompt.o src/persistence/sqlite/conversation/_store/gateway.o src/plugins/platforms/matrix/gateway_env.o src/plugins/platforms/slack/gateway_env.o src/hermes_cli/subcommands/webapp.o src/plugins/platforms/whatsapp/gateway_env.o src/plugins/platforms/telegram/telegram_dice.o src/plugins/platforms/telegram/gateway_env.o src/agent/vault_backends/local.o src/hermes_cli/subcommands/egress.o src/agent/checkpoints/refs.o src/hermes_cli/subcommands/curator.o src/hermes_cli/subcommands/journey.o src/plugins/platforms/telegram/telegram_intake.o src/gateway/platforms/api_server_config_guard.o src/plugins/platforms/sms/gateway_env.o src/hermes_cli/subcommands/pets.o src/hermes_cli/subcommands/vault.o src/hermes_cli/config_defaults_auxiliary.o src/persistence/sqlite/_holder.o src/persistence/sqlite/authority/coordination.o src/agent/lsp/client.o src/tools/computer_use/cua_backend_session.o src/tools/computer_use/cua_backend_capture.o src/hermes_cli/cli_subagent_monitor.o src/persistence/sqlite/gateway/_routes.o src/tools/terminal_tool_sudo_askpass.o src/hermes_cli/runtime_provider_backends.o src/persistence/sqlite/authority/_mutation.o src/plugins/platforms/telegram/update_admission.o src/tools/terminal_tool_background.o src/agent/lsp/edit_check.o src/gateway/hosted_room_attachments.o src/persistence/sqlite/_state/readpool.o src/persistence/gateway/hosted_rooms.o src/hermes_cli/profiles_initialization.o src/agent/pet/manifest.o src/hermes_platform/resolver/core.o src/hermes_cli/gateway_windows_legacy.o src/plugins/platforms/telegram/inline_picker.o src/hermes_cli/subcommands/migrate.o src/gateway/session_unattended_unknown.o src/hermes_cli/web_server_skew_exit.o src/tools/image_generation_managed.o src/persistence/core/aio.o src/hermes_cli/observability/shared_metrics_prepared_compaction.o src/agent/chat_completion_wait_notice.o src/gateway/platforms/base_session_command.o src/plugins/platforms/photon/sidecar_paths.o src/tools/connectors/turn.o src/persistence/sqlite/gateway/_hosted_rooms_common.o src/gateway/relay/auth.o src/hermes_cli/web_routers/shared_metrics.o src/tools/connectors/gateway/names.o src/gateway/platforms/base_kanban_media.o src/tui_gateway/methods_session_foreign.o src/persistence/sqlite/conversation/_store/prompt_refresh.o src/hermes_cli/dashboard_auth/listener_client.o src/agent/verify/environment.o src/hermes_cli/session_reset_retirement.o src/tools/connectors/portal/policy.o src/plugins/platforms/qqbot/utils.o src/gateway/session_retry_admission.o src/gateway/hosted_room_work_lineage.o src/hermes_cli/kanban_pr_acceptance_store.o src/plugins/platforms/telegram/telegram_dm_topic_send.o src/plugins/web/ddgs/_search_worker.o src/tools/environments/sudo_password_note.o src/gateway/platforms/base_auto_tts.o src/plugins/platforms/discord/standalone_rest.o src/hermes_cli/subcommands/pause.o src/hermes_platform/host/products.o src/tools/connectors/display.o src/plugins/platforms/telegram/telegram_ids.o src/hermes_cli/gateway_status_notes.o src/gateway/platforms/send_targets.o src/hermes_cli/auth_external_process.o src/plugins/platforms/telegram/telegram_group_observe.o src/tui_gateway/methods_tools_plugin_commands.o src/hermes_platform/host/runtime.o src/hermes_cli/web_routers/messaging_relay.o src/plugins/config_backends/remote/paths.o src/plugins/google_meet/node/cli.o src/gateway/session_native_recovery_routes.o src/tools/connectors/live.o src/plugins/google_meet/node/protocol.o src/plugins/config_backends/remote/client.o src/persistence/sqlite/_state/health.o src/tools/environments/remote_common.o src/plugins/platforms/telegram/telegram_picker_labels.o src/hermes_cli/dashboard_auth/ws_tickets.o src/agent/pet/constants.o src/agent/anthropic_thinking_policy.o src/hermes_cli/foreign_sessions_browser.o src/gateway/relay/http_diagnostics.o src/persistence/sqlite/authority/_runtime_retirement.o src/tools/bot_desktop/placement.o src/persistence/sqlite/_state/write.o src/plugins/platforms/whatsapp/bridge_ownership.o src/agent/prompt_cache_lifecycle/replay.o src/plugins/platforms/telegram/telegram_outbound.o src/plugins/platforms/qqbot/onboard.o src/plugins/platforms/telegram/telegram_context.o src/persistence/sqlite/gateway/_local_lineage.o src/persistence/sqlite/conversation/_store/messages_sql.o src/plugins/config_backends/remote/values.o src/plugins/web/tavily/provider.o src/agent/model_metadata_token_memo.o src/hermes_cli/webapp_relay_target.o src/tools/connectors/account.o src/plugins/platforms/discord/adapter_config_readers.o src/gateway/relay/admitted_authors.o src/agent/agent_runtime_helpers_dead_connections.o src/gateway/platforms/api_server_cron_jobs.o src/gateway/platforms/api_server_fire_startup.o src/gateway/platforms/api_server_store.o src/tools/async_delegation_recovery_hints.o src/plugins/model-providers/solstice/auth.o src/agent/auxiliary_custom_credentials.o src/agent/auxiliary_fallback_recovery.o src/agent/auxiliary_rate_limit_backoff.o src/hermes_cli/browser_preview_signins.o src/plugins/platforms/telegram/callback_answer.o src/hermes_cli/web_routers/chat_workspaces.o src/agent/codex_runtime_history_seed.o src/agent/conversation_compression_deadline.o src/hermes_platform/host/darwin_pci.o src/tools/delegate_tool_prefix_gate.o src/persistence/sqlite/conversation/_store/detached_results.o src/hermes_cli/local_runtime/devices.o src/hermes_cli/gateway_cli_commands.o src/hermes_cli/gateway_connections_probe.o src/hermes_cli/gateway_runtime_multiplex.o src/hermes_cli/gateway_setting_commands.o src/tools/bot_desktop/idle_watch.o src/gateway/relay/inbound_admission.o src/hermes_cli/kanban_notify_thread.o src/agent/notification_presentation.o src/hermes_cli/dashboard_auth/peer_pass.o src/hermes_cli/profiles_export_scrub.o src/hermes_cli/profiles_service_cleanup.o src/plugins/web/parallel/provider.o src/agent/lsp/range_shift.o src/plugins/platforms/signal/register.o src/agent/lsp/reporter.o src/gateway/run_heartbeat_acceptance.o src/cron/scheduler_worktree_maintenance.o src/gateway/session_ingress_live_turn.o src/gateway/session_managed_worker_log.o src/gateway/session_mutation_policy.o src/gateway/slash_commands_branch_thread.o src/agent/lsp/sync_diagnostics.o src/plugins/platforms/telegram/telegram_clarify_binding.o src/plugins/platforms/telegram/telegram_ingress.o src/plugins/platforms/telegram/telegram_live_location.o src/plugins/platforms/telegram/telegram_media_retry.o src/plugins/platforms/telegram/telegram_topic_transfer.o src/hermes_cli/tool_availability_notices.o src/tools/transcription_whisper_cpp.o src/hermes_cli/update_cmd_pause_gate.o src/hermes_cli/update_owning_install.o src/hermes_cli/subcommands/usage.o src/plugins/cron_providers/chronos/verify.o src/hermes_cli/web_host_terminal_ssh.o src/hermes_cli/web_server_remote_media.o src/hermes_cli/windows_store_update.o src/agent/agent_init_compression_notice.o src/agent/agent_init_memory_provider.o src/agent/auxiliary_reasoning_floor.o src/agent/auxiliary_structured_output.o src/tools/connectors/catalog_tool.o src/hermes_cli/cli_backtrack_browse.o src/tools/connectors/portal/client.o src/hermes_cli/subcommands/computer_use.o src/hermes_cli/config_version_stamp.o src/persistence/sqlite/workers/_context.o src/agent/conversation_compression_image_shrink.o src/agent/conversation_compression_reply_anchor.o src/plugins/config_backends/remote/credentials.o src/hermes_cli/data_cleanup_holders.o src/hermes_cli/web_routers/display.o src/tools/escape_drift_autocorrect.o src/hermes_cli/gateway_lifecycle_guards.o src/hermes_cli/gateway_multiplex_served.o src/hermes_cli/gateway_runtime_tickets.o src/hermes_cli/gateway_runtime_start.o src/persistence/sqlite/gateway/_hosted_rooms_schema.o src/gateway/hosted_rooms_gateway_import.o src/hermes_cli/kanban_pr_acceptance.o src/tools/environments/local_pythonpath.o src/tui_gateway/methods_complete_helpers.o src/tui_gateway/methods_session_interrupt.o src/hermes_cli/model_setup_flows_azure.o src/persistence/sqlite/authority/_mutation_publish.o src/hermes_cli/plugins_activation_live.o src/hermes_cli/plugins_cmd_update_all.o src/hermes_cli/plugins_cmd_capabilities.o src/tools/process_registry_output_log.o src/tools/process_registry_awaiting.o src/hermes_cli/profile_memory_config.o src/tui_gateway/profile_roster_cache.o src/agent/lsp/protocol.o src/plugins/web/ddgs/provider.o src/persistence/sqlite/workers/_receipt.o src/tools/bot_desktop/resources.o src/persistence/sqlite/conversation/_store/rewind.o src/plugins/platforms/telegram/rich_code_regions.o src/agent/verify/runner.o src/gateway/run_turn_final_reconcile.o src/gateway/session_completion_notes.o src/gateway/session_control_actions.o src/gateway/session_messaging_picks.o src/plugins/platforms/signal/signal_format.o src/plugins/teams_pipeline/subscriptions.o src/plugins/platforms/telegram/telegram_addressing.o src/plugins/platforms/telegram/telegram_audio_meta.o src/plugins/platforms/telegram/telegram_batching.o src/plugins/platforms/telegram/telegram_clarify_multi.o src/plugins/platforms/telegram/telegram_latex.o src/persistence/sqlite/conversation/_store/timeline.o src/hermes_cli/update_cmd_fleet_unit_budget.o src/hermes_cli/web_server_file_tickets.o src/hermes_cli/web_server_idle_exit.o src/tools/connectors/gateway/bridge.o src/agent/secret_sources/_cache.o src/plugins/web/perplexity/provider.o src/plugins/platforms/buzz/adapter.o src/plugins/platforms/buzz/adapter.o src/plugins/platforms/buzz/adapter.o src/plugins/platforms/buzz/adapter.o src/plugins/platforms/discord/adapter.o src/plugins/platforms/discord/adapter.o src/plugins/platforms/discord/adapter.o src/plugins/platforms/discord/adapter.o src/plugins/platforms/discord/adapter.o src/plugins/platforms/discord/adapter.o src/plugins/platforms/discord/adapter.o src/plugins/platforms/discord/adapter.o src/plugins/platforms/discord/adapter.o src/plugins/platforms/discord/adapter.o src/plugins/platforms/discord/adapter.o src/plugins/platforms/discord/adapter.o src/plugins/platforms/discord/adapter.o src/plugins/platforms/discord/adapter.o src/plugins/platforms/whatsapp/adapter.o src/plugins/platforms/whatsapp/adapter.o src/plugins/platforms/whatsapp/adapter.o src/plugins/platforms/whatsapp/adapter.o src/plugins/platforms/slack/adapter.o src/plugins/platforms/slack/adapter.o src/plugins/platforms/slack/adapter.o src/plugins/platforms/slack/adapter.o src/plugins/platforms/slack/adapter.o src/plugins/platforms/slack/adapter.o src/plugins/platforms/slack/adapter.o src/plugins/platforms/slack/adapter.o src/plugins/platforms/photon/adapter.o src/plugins/platforms/photon/adapter.o src/plugins/platforms/photon/adapter.o src/plugins/platforms/photon/adapter.o src/plugins/platforms/photon/adapter.o src/plugins/platforms/google_chat/adapter.o src/plugins/platforms/google_chat/adapter.o src/plugins/platforms/google_chat/adapter.o src/plugins/platforms/google_chat/adapter.o src/plugins/platforms/telegram/adapter.o src/plugins/platforms/telegram/adapter.o src/plugins/platforms/telegram/adapter.o src/plugins/platforms/telegram/adapter.o src/plugins/platforms/telegram/adapter.o src/plugins/platforms/telegram/adapter.o src/plugins/platforms/telegram/adapter.o src/plugins/platforms/telegram/adapter.o src/plugins/platforms/telegram/adapter.o src/plugins/platforms/telegram/adapter.o src/plugins/platforms/telegram/adapter.o src/plugins/platforms/telegram/adapter.o src/plugins/platforms/telegram/adapter.o src/plugins/platforms/matrix/adapter.o src/plugins/platforms/matrix/adapter.o src/plugins/platforms/matrix/adapter.o src/plugins/platforms/matrix/adapter.o src/plugins/platforms/matrix/adapter.o src/plugins/platforms/matrix/adapter.o src/gateway/platforms/api_server_openai_routes.o src/gateway/platforms/api_server_openai_routes.o src/gateway/platforms/api_server_openai_routes.o src/persistence/sqlite/conversation/_store/messages.o src/persistence/sqlite/conversation/_store/messages.o src/persistence/sqlite/conversation/_store/messages.o src/persistence/sqlite/conversation/_store/messages.o src/plugins/kanban/dashboard/plugin_api.o src/plugins/kanban/dashboard/plugin_api.o src/plugins/kanban/dashboard/plugin_api.o src/plugins/kanban/dashboard/plugin_api.o src/persistence/sqlite/_schema.o src/persistence/sqlite/_schema.o src/persistence/sqlite/_schema.o src/plugins/platforms/a2a/adapter.o src/plugins/platforms/a2a/adapter.o src/plugins/platforms/a2a/adapter.o src/plugins/platforms/teams/adapter.o src/plugins/platforms/teams/adapter.o src/plugins/platforms/dingtalk/adapter.o src/plugins/platforms/dingtalk/adapter.o src/plugins/platforms/signal/adapter.o src/plugins/platforms/signal/adapter.o src/plugins/platforms/signal/adapter.o src/plugins/config_backends/remote/backend.o src/plugins/config_backends/remote/backend.o src/plugins/config_backends/remote/backend.o src/persistence/sqlite/_state/dbfile.o src/persistence/sqlite/_state/dbfile.o src/persistence/sqlite/gateway/hosted_room_driver.o src/persistence/sqlite/gateway/hosted_room_driver.o src/persistence/sqlite/gateway/hosted_room_driver.o src/hermes_cli/web_routers/oauth.o src/hermes_cli/web_routers/oauth.o src/hermes_cli/plugins_install_scan_context.o src/hermes_cli/plugins_install_scan_context.o src/tui_gateway/session_notifications.o src/tui_gateway/session_notifications.o src/persistence/sqlite/conversation/sessions.o src/persistence/sqlite/conversation/sessions.o src/persistence/sqlite/conversation/sessions.o src/persistence/sqlite/conversation/sessions.o src/hermes_cli/web_routers/status.o src/hermes_cli/web_routers/status.o src/plugins/platforms/email/adapter.o src/plugins/platforms/email/adapter.o src/plugins/platforms/email/adapter.o src/hermes_cli/cli_model_switch_mixin.o src/hermes_cli/cli_model_switch_mixin.o src/hermes_cli/web_routers/config_env.o src/hermes_cli/web_routers/config_env.o src/hermes_cli/web_routers/files.o src/hermes_cli/web_routers/files.o src/hermes_cli/web_routers/files.o src/tui_gateway/hosted_room_driver.o src/tui_gateway/hosted_room_driver.o src/tui_gateway/hosted_room_driver.o src/tui_gateway/hosted_room_service.o src/tui_gateway/hosted_room_service.o src/tui_gateway/hosted_room_service.o src/agent/checkpoints/manager.o src/agent/checkpoints/manager.o src/agent/checkpoints/manager.o src/hermes_cli/web_routers/messaging.o src/hermes_cli/web_routers/messaging.o src/hermes_cli/web_routers/messaging.o src/hermes_cli/model_switch_providers.o src/hermes_cli/model_switch_providers.o src/hermes_cli/model_switch_providers.o src/persistence/sqlite/admin/_recovery.o src/persistence/sqlite/admin/_recovery.o src/persistence/sqlite/_state/repair.o src/persistence/sqlite/_state/repair.o src/persistence/sqlite/_state/repair.o src/hermes_cli/web_server_dashboard.o src/hermes_cli/web_server_dashboard.o src/plugins/platforms/mattermost/adapter.o src/plugins/platforms/mattermost/adapter.o src/tools/environments/base.o src/tools/environments/base.o src/plugins/platforms/feishu/feishu_comment.o src/plugins/platforms/feishu/feishu_comment.o src/persistence/sqlite/gateway/hosted_room_attachments.o src/persistence/sqlite/gateway/hosted_room_attachments.o src/gateway/kanban_watchers_notifier.o src/gateway/kanban_watchers_notifier.o src/persistence/sqlite/admin/_lost_and_found.o src/persistence/sqlite/admin/_lost_and_found.o src/persistence/sqlite/conversation/messages.o src/persistence/sqlite/conversation/messages.o src/persistence/sqlite/conversation/messages.o src/hermes_cli/models_catalog_static.o src/tools/bot_desktop/runtime.o src/tools/bot_desktop/runtime.o src/hermes_cli/runtime_provider_custom.o src/hermes_cli/runtime_provider_custom.o src/persistence/sqlite/conversation/_store/search.o src/persistence/sqlite/conversation/_store/search.o src/hermes_cli/update_cmd_posix_pause.o src/hermes_cli/update_cmd_posix_pause.o src/plugins/platforms/simplex/adapter.o src/plugins/platforms/simplex/adapter.o src/agent/anthropic_message_convert.o src/agent/anthropic_message_convert.o src/persistence/sqlite/conversation/_store/compression.o src/persistence/sqlite/conversation/_store/compression.o src/hermes_cli/gateway_session_store.o src/hermes_cli/gateway_session_store.o src/hermes_cli/gateway_session_store.o src/tui_gateway/hosted_room_peer_http.o src/tui_gateway/hosted_room_peer_http.o src/tools/environments/local_env_policy.o src/tools/connectors/mcp.o src/tools/connectors/mcp.o src/hermes_cli/web_routers/ops.o src/hermes_cli/web_routers/ops.o src/tui_gateway/session_auto_continue.o src/gateway/platforms/_shared.o src/gateway/platforms/_shared.o src/plugins/platforms/telegram/telegram_markdown.o src/plugins/platforms/telegram/telegram_markdown.o src/hermes_cli/web_routers/audio.o src/hermes_cli/web_routers/audio.o src/persistence/sqlite/admin/_backup.o src/persistence/sqlite/admin/_backup.o src/hermes_cli/web_routers/chat_ws.o src/hermes_cli/web_routers/chat_ws.o src/hermes_cli/cli_tui_runtime_mixin.o src/tui_gateway/contracts/events.o src/persistence/sqlite/gateway/hosted_room_policy.o src/persistence/sqlite/gateway/hosted_room_policy.o src/tui_gateway/methods_config_set.o src/tui_gateway/methods_config_set.o src/hermes_cli/plugin_validate_core_override.o src/hermes_cli/plugin_validate_core_override.o src/persistence/sqlite/gateway/routing.o src/persistence/sqlite/gateway/routing.o src/persistence/sqlite/authority/subagents.o src/persistence/sqlite/authority/subagents.o src/hermes_cli/web_routers/tools.o src/hermes_cli/web_routers/tools.o src/tui_gateway/contracts/tools_mcp_plugins.o src/plugins/platforms/irc/adapter.o src/plugins/platforms/irc/adapter.o src/persistence/sqlite/housekeeping/maintenance.o src/persistence/sqlite/housekeeping/maintenance.o src/plugins/teams_pipeline/pipeline.o src/plugins/teams_pipeline/pipeline.o src/persistence/authority/runtime.o src/persistence/authority/runtime.o src/tools/environments/sandbox_conformance.o src/tools/environments/sandbox_conformance.o src/agent/lsp/servers.o src/plugins/platforms/telegram/standalone.o src/plugins/platforms/telegram/telegram_network.o src/plugins/platforms/telegram/telegram_network.o src/hermes_cli/update_cmd_fleet_verify.o src/tui_gateway/contracts/config_free_tier_control.o src/plugins/model-providers/gemini/interactions.o src/plugins/model-providers/gemini/interactions.o src/persistence/sqlite/housekeeping/_portability.o src/tui_gateway/compute_host_bridge.o src/persistence/sqlite/conversation/_store/coverage.o src/gateway/relay/egress.o src/persistence/sqlite/_state/holders.o src/persistence/sqlite/gateway/_hosted_work.o src/hermes_cli/plugins_install_scan.o src/persistence/sqlite/admin/_reconcile.o src/persistence/sqlite/admin/_reconcile.o src/gateway/runtime_bootstrap_windows.o src/tui_gateway/contracts/sessions.o src/hermes_cli/sessions_repair_profiles.o src/hermes_cli/sessions_repair_profiles.o src/hermes_cli/web_host_terminal_sessions.o src/hermes_cli/web_host_terminal_sessions.o src/hermes_cli/web_server_lifecycle.o src/agent/checkpoints/undo_hint.o

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
 src/tools/website_policy.o src/tools/memory_tool.o src/tools/memory_available_port.o src/skills_hub.o \
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
 src/hermes_logging.o src/mcp_serve.o src/hermes_posix_compat.o src/infrastructure_stubs.o src/pm/port_pm_environments.o


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
