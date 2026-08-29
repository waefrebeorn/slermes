# Slermes REAL_GAP List — Function Level (live scanner)

> Generated 2026-08-29T05:12:59Z from `live_parity_scan.json` by `make parity-walkway`. **963 REAL_GAP across 250 modules** (of 16,714 total functions).

> This is the forward work plan. Each entry is a Python function not yet ported to C. Close by implementing real C + a single-line `/* PoP: fn @ module.py:fn */` annotation (see slermes-gap-closure skill). Never hand-edit — regenerate via the scanner.

### agent/agent_init.py (1 gaps)

- _normalize_run_budget_seconds (function)

### agent/agent_runtime_helpers.py (6 gaps)

- _raw_cache_ttl_from_config (function)
- configured_cache_ttl (function)
- _is_litellm_route (function)
- _has_litellm_token (function)
- _tool_call_id_variants (function)
- trailing_continue_intent (function)

### agent/anthropic_adapter.py (1 gaps)

- _accepts_thinking_disable (function)

### agent/auxiliary_client.py (10 gaps)

- _AuxProbeClientStub.__getattr__ (method)
- _task_prefers_fast_model (function)
- _is_official_codex_base_url (function)
- _apply_required_codex_headers (function)
- _is_dual_surface_anthropic_host (function)
- _translate_anthropic_response_format (function)
- _record_route_info (function)
- _is_structured_output_rejection (function)
- _without_structured_output_format (function)
- _schedule_async_close (function)

### agent/background_review.py (3 gaps)

- _BackgroundReviewRun.begin_request (method)
- _BackgroundReviewRun.cancel (method)
- _BackgroundReviewRun.mark_request_finished (method)

### agent/bedrock_adapter.py (4 gaps)

- resolve_bedrock_bearer_token (function)
- BedrockOpenAISigV4Auth.auth_flow (method)
- build_bedrock_openai_http_client (function)
- resolve_bedrock_runtime_region (function)

### agent/browser_registry.py (1 gaps)

- restore_registration (function)

### agent/chat_completion_helpers.py (11 gaps)

- _join_worker_for_relay_teardown (function)
- _parse_provider_sse_events (function)
- _provider_stream_error_from_json_decode_error (function)
- _iter_provider_stream_chunks (function)
- _provider_stream_error_from_text (function)
- _prompt_cache_scope_for_agent (function)
- _record_interrupted_provider_wait (function)
- _report_stale_nonstream_kill (function)
- _touch_stale_kill_activity (function)
- _resolve_direct_stale_timeout (function)
- _inline_nonstream_hard_timeout (function)

### agent/codex_runtime.py (5 gaps)

- _codex_request_failure_details (function)
- _log_codex_request_failure (function)
- _sanitize_consumer_codex_request (function)
- _is_plain_json_data (function)
- _bypass_sdk_request_transform (function)

### agent/coding_context.py (1 gaps)

- build_coding_workspace_block (function)

### agent/context_references.py (3 gaps)

- async ContextReferenceProvider.autocomplete (method)
- async ContextReferenceProvider.expand (method)
- register_context_reference_provider (function)

### agent/conversation_compression.py (2 gaps)

- _strip_marker_for_comparison (function)
- _pruned_skill_reload_notice (function)

### agent/conversation_loop.py (9 gaps)

- _review_input_budget_exhausted (function)
- _maybe_inject_run_budget_wrapup (function)
- _should_rearm_compression_budget (function)
- _is_interpreter_shutdown_error (function)
- _moa_client_consumes_prepared_request (function)
- _moa_reference_metrics_for_hook (function)
- _system_prompt_for_hooks (function)
- _billing_terminal_label (function)
- _billing_failure_result (function)

### agent/copilot_acp_client.py (1 gaps)

- _acp_supported (function)

### agent/credential_pool.py (2 gaps)

- resolve_runtime_pool_key (function)
- get_env_prefer_dotenv (function)

### agent/deadline.py (2 gaps)

- SuspectableBackend.mark_suspect (method)
- async SuspectableBackend.ensure_healthy (method)

### agent/delegation_context.py (3 gaps)

- non_dispatcher_owned_context (function)
- is_dispatcher_owned_worker_context (function)
- enter_non_dispatcher_owned_context (function)

### agent/error_classifier.py (5 gaps)

- ClassifiedError.billing_unverified (method)
- _billing_ambiguity_context (function)
- _is_server_injected_param_rejection (function)
- _has_usage_limit_transient_signal (function)
- _extract_response_headers (function)

### agent/file_safety.py (2 gaps)

- build_write_approval_paths (function)
- is_write_approval_required (function)

### agent/gemini_native_adapter.py (3 gaps)

- _gemini_major_version (function)
- gemini_requires_tool_call_ids (function)
- _looks_like_json_schema (function)

### agent/image_gen_registry.py (2 gaps)

- snapshot_registration (function)
- restore_registration (function)

### agent/image_routing.py (1 gaps)

- _resolve_inference_api_key (function)

### agent/interrupt_compat.py (1 gaps)

- _accepts_keyword (function)

### agent/lsp/client.py (1 gaps)

- LSPClient._connection_is_open (method)

### agent/memory_manager.py (3 gaps)

- memory_provider_tools_exposed (function)
- build_memory_context_block (function)
- MemoryManager.describe_recall (method)

### agent/memory_provider.py (3 gaps)

- MemoryProvider.unavailable_reason (method)
- MemoryProvider.prefetch (method)
- MemoryProvider.recall_status (method)

### agent/message_metadata.py (1 gaps)

- append_message (function)

### agent/message_sanitization.py (2 gaps)

- _expand_tool_id_variants (function)
- tool_call_id_variants (function)

### agent/moa_loop.py (2 gaps)

- MoAChatCompletions.last_reference_metrics (method)
- MoAClient.last_reference_metrics (method)

### agent/moa_trace.py (1 gaps)

- slot_metrics (function)

### agent/model_metadata.py (7 gaps)

- _extract_flat_context_length (function)
- _context_length_from_model_payload (function)
- _stale_pre_catalog_cache_entry (function)
- _model_name_suggests_minimax (function)
- _model_name_suggests_stale_32k_underreport (function)
- _bare_codex_slug (function)
- _verified_codex_ctx_for_slug (function)

### agent/monitoring/gateway_health.py (1 gaps)

- build_gateway_health_snapshot (function)

### agent/monitoring/gateway_health_export.py (1 gaps)

- _start_diagnostic_log_streamer (function)

### agent/native_compaction.py (2 gaps)

- _extract_item_text (function)
- _is_summary_item (function)

### agent/pet/generate/imagegen.py (1 gaps)

- _discover (function)

### agent/plugin_llm.py (2 gaps)

- _resolve_task_ownership (function)
- _check_task (function)

### agent/process_bootstrap.py (7 gaps)

- _interleave_addrinfos (function)
- _happy_eyeballs_create_connection (function)
- _HappyEyeballsSyncBackend._default_backend (method)
- _HappyEyeballsSyncBackend.connect_tcp (method)
- _HappyEyeballsSyncBackend.connect_unix_socket (method)
- _HappyEyeballsSyncBackend.sleep (method)
- _enable_happy_eyeballs (function)

### agent/prompt_builder.py (7 gaps)

- execution_guidance_text (function)
- hud_surface_note (function)
- _plugin_backend_is_remote (function)
- _plugin_backend_description (function)
- _windows_marketing_version (function)
- _build_skills_system_prompt_inner (function)
- _agents_md_directory_chain (function)

### agent/prompt_cache_boundary.py (3 gaps)

- register_stable_prefix (function)
- find_stable_prefix (function)
- clear_stable_prefixes (function)

### agent/prompt_caching.py (2 gaps)

- is_qwen_model (function)
- effective_cache_ttl (function)

### agent/redact.py (6 gaps)

- _mask_control_split_tokens (function)
- _has_top_level_alternation (function)
- _has_nested_unbounded_repeat (function)
- _rebuild_prefix_matcher (function)
- register_redaction_patterns (function)
- _reset_plugin_redaction_patterns (function)

### agent/relay_llm.py (5 gaps)

- _relay_protocol (function)
- _relay_operation_name (function)
- _relay_metadata (function)
- ManagedLlmStream._prime_completed_response (method)
- ManagedLlmStream._release_runtime_lease (method)

### agent/review_engine.py (3 gaps)

- _message_text (function)
- _load_review_credentials_cfg (function)
- start_review (function)

### agent/secret_sources/base.py (1 gaps)

- get_source_environment (function)

### agent/secret_sources/registry.py (1 gaps)

- list_plugin_sources (function)

### agent/skill_utils.py (2 gaps)

- _project_trusted_dirs_from_config (function)
- _candidate_project_skills_dirs (function)

### agent/subagent_lifecycle.py (1 gaps)

- SubagentLifecycleService._cleanup_locked (method)

### agent/system_prompt.py (8 gaps)

- _plugin_session_info (function)
- _frozen_plugin_prompt_sections (function)
- _restore_plugin_prompt_sections (function)
- restore_plugin_prompt_sections (function)
- _plugin_section_blocks (function)
- _agent_home (function)
- _agent_skills_dir (function)
- _profile_name_for_home (function)

### agent/terminal_env_provider.py (10 gaps)

- TerminalEnvironmentProvider.description (method)
- TerminalEnvironmentProvider.cache_path_base (method)
- TerminalEnvironmentProvider.strip_env_keys (method)
- TerminalEnvironmentProvider.env_description (method)
- TerminalEnvironmentProvider.check_requirements (method)
- TerminalEnvironmentProvider.probe (method)
- TerminalEnvironmentProvider.setup_instructions (method)
- TerminalEnvironmentProvider.post_setup (method)
- TerminalEnvironmentProvider.doctor_checks (method)
- TerminalEnvironmentProvider.create_environment (method)

### agent/terminal_env_registry.py (5 gaps)

- plugin_backend_names (function)
- provider_flag (function)
- plugin_strip_env_keys (function)
- snapshot_registration (function)
- restore_registration (function)

### agent/thread_scoped_output.py (1 gaps)

- _ThreadRoutingStream._target (method)

### agent/title_generator.py (2 gaps)

- apply_instant_title (function)
- _session_is_untitled (function)

### agent/tool_dispatch_helpers.py (3 gaps)

- _normalize_tool_call_id (function)
- _detect_upstream_elision (function)
- _maybe_append_elision_notice (function)

### agent/tool_executor.py (6 gaps)

- _record_persisted_path_for_stub (function)
- _authorization_gate_lock_timeout (function)
- _ConcurrentToolAuthorizationGate._human_wait_seconds (method)
- _run_tool_activity_heartbeat (function)
- _resolve_sequential_tool_timeout (function)
- _run_sequential_tool_execution_middleware (function)

### agent/tool_guardrails.py (5 gaps)

- is_stall_guard_repeatable (function)
- ToolCallGuardrailController.observe_identical_call (method)
- ToolCallGuardrailController.observe_call (method)
- ToolCallGuardrailController.record_persisted_result (method)
- ToolCallGuardrailController._build_result_reference_stub (method)

### agent/transcription_registry.py (2 gaps)

- snapshot_registration (function)
- restore_registration (function)

### agent/transports/codex.py (5 gaps)

- _is_opencode_responses_backend (function)
- _rename_reserved_tools_for_opencode (function)
- _is_azure_foundry_responses (function)
- _is_post_tool_replay (function)
- _native_compaction_active (function)

### agent/tts_registry.py (2 gaps)

- snapshot_registration (function)
- restore_registration (function)

### agent/turn_context.py (2 gaps)

- _maybe_title_session_at_turn_start (function)
- _review_fork_first_request_pending (function)

### agent/turn_finalizer.py (1 gaps)

- _record_kanban_budget_exhausted (function)

### agent/usage_pricing.py (2 gaps)

- _usage_get (function)
- _usage_count (function)

### agent/verification_evidence.py (4 gaps)

- _split_shell_segments (function)
- _exit_status_is_attributable (function)
- record_verify_run (function)
- _insert_evidence (function)

### agent/verification_stop.py (1 gaps)

- verify_on_stop_enabled (function)

### agent/vertex_adapter.py (8 gaps)

- _vertex_config (function)
- _resolve_region (function)
- _resolve_project_override (function)
- _resolve_credentials_path (function)
- get_vertex_credentials (function)
- get_vertex_config (function)
- has_vertex_credentials (function)
- has_explicit_vertex_config (function)

### agent/video_gen_registry.py (2 gaps)

- snapshot_registration (function)
- restore_registration (function)

### agent/web_search_provider.py (1 gaps)

- WebSearchProvider.is_keyless_available (method)

### agent/web_search_registry.py (4 gaps)

- snapshot_registration (function)
- restore_registration (function)
- _keyless_preference (function)
- _keyless_tier_enabled (function)

### cli.py (38 gaps)

- HermesCLI._install_resize_recovery (method)
- HermesCLI._on_reaction (method)
- HermesCLI._get_voice_status_fragments (method)
- HermesCLI._build_status_bar_text (method)
- HermesCLI._get_status_bar_fragments (method)
- HermesCLI._install_tool_callbacks (method)
- HermesCLI._show_browser_backend_notice (method)
- HermesCLI.finalize_preloaded_skills (method)
- HermesCLI._restore_session_yolo (method)
- HermesCLI._persist_model_switch_to_session (method)
- HermesCLI._restore_session_model (method)
- HermesCLI._check_termios_drift (method)
- HermesCLI._build_command_palette_entries (method)
- HermesCLI._open_command_palette (method)
- HermesCLI._command_palette_visible_entries (method)
- HermesCLI._handle_command_palette_selection (method)
- HermesCLI._snapshot_model_runtime (method)
- HermesCLI._restore_model_runtime_snapshot (method)
- HermesCLI._filter_model_picker_entries (method)
- HermesCLI._confirm_and_apply_cli_model_switch (method)
- HermesCLI._should_handle_background_command_inline (method)
- HermesCLI.handle_bang_shell (method)
- HermesCLI._start_heartbeat_watchdog (method)
- HermesCLI._voice_restart_recording_async (method)
- HermesCLI._voice_full_duplex_listener (method)
- HermesCLI._voice_submit_barge_utterance (method)
- HermesCLI._typed_voice_stop (method)
- HermesCLI._maybe_start_wake_word (method)
- HermesCLI._start_wake_word_listener (method)
- HermesCLI._stop_wake_word_listener (method)
- HermesCLI._on_wake_word (method)
- HermesCLI._start_wake_watchdog (method)
- HermesCLI._show_wake_word_status (method)
- HermesCLI._clarify_batch_set_active (method)
- HermesCLI._clarify_batch_lock (method)
- HermesCLI._clarify_batch_enter (method)
- HermesCLI._clarify_callback_batch (method)
- HermesCLI._get_approval_display_fragments (method)

### cron/blueprint_catalog.py (2 gaps)

- BlueprintSlot.__post_init__ (method)
- get_blueprint (function)

### cron/lifecycle_guard.py (1 gaps)

- _executable_name (function)

### cron/scheduler_provider.py (5 gaps)

- _backoff_wait_seconds (function)
- _note_tick_failure (function)
- CronScheduler.supports_force_fire (method)
- CronScheduler.claim_fire (method)
- CronScheduler.fire_claimed (method)

### gateway/agent_cache_pressure.py (1 gaps)

- _total_memory_bytes (function)

### gateway/drain_control.py (3 gaps)

- _marker_is_expired (function)
- drain_requested (function)
- drain_notification_suppressed (function)

### gateway/kanban_watchers.py (4 gaps)

- _kanban_dispatch_allowed (function)
- _run_in_fresh_context (function)
- async _to_thread_process_service (function)
- _wake_scope_id (function)

### gateway/media_policy.py (1 gaps)

- apply_media_policy_env (function)

### gateway/pairing.py (3 gaps)

- _default_pairing_dir (function)
- _iter_live_gateway_adapters (function)
- _secure_write (function)

### gateway/platform_registry.py (8 gaps)

- _plugin_scope_from_callable (function)
- _caller_plugin_scope (function)
- PlatformRegistry.current_scope_key (method)
- PlatformRegistry._scope_maps (method)
- PlatformRegistry.snapshot_registration (method)
- PlatformRegistry.restore_registration (method)
- PlatformRegistry.is_deferred_load_cancelled (method)
- PlatformRegistry.registered_names (method)

### gateway/platforms/base.py (7 gaps)

- _consume_detached_handler_exception (function)
- _translate_docker_container_media_path (function)
- BasePlatformAdapter.set_platform_event_handler (method)
- BasePlatformAdapter.set_owner_profile (method)
- BasePlatformAdapter._session_key_profile (method)
- async BasePlatformAdapter._bounded_history_media_paths_for_session (method)
- BasePlatformAdapter.toolsets_for_source (method)

### gateway/platforms/signal.py (4 gaps)

- _sig_secret (function)
- SignalAdapter._utf16_offsets (method)
- SignalAdapter._styles_for_chunk (method)
- SignalAdapter._split_signal_formatted_message (method)

### gateway/platforms/webhook.py (1 gaps)

- WebhookAdapter.toolsets_for_source (method)

### gateway/platforms/weixin.py (1 gaps)

- async WeixinAdapter._recycle_poll_session (method)

### gateway/platforms/yuanbao.py (3 gaps)

- _yb_secret (function)
- async MessageSender.send_text (method)
- async OutboundManager.send_text (method)

### gateway/readiness.py (1 gaps)

- _probe_session_store (function)

### gateway/restart.py (2 gaps)

- parse_cron_drain_timeout (function)
- resolve_cron_drain_budget (function)

### gateway/scale_to_zero.py (2 gaps)

- self_suspend_available (function)
- suspend_self (function)

### gateway/session.py (9 gaps)

- auto_continue_freshness_window (function)
- SessionStore._open_session_db_for_active_scope (method)
- SessionStore._db (method)
- SessionStore._db (method)
- SessionStore.close_all_db_handles (method)
- SessionStore._routing_scope (method)
- SessionStore._reconcile_recovered_routing_locked (method)
- SessionStore.lookup_by_session_key (method)
- SessionStore._get_transcript_drain_lock (method)

### gateway/session_db_recovery.py (4 gaps)

- _publish_health (function)
- RecoverableHandleCache.get (method)
- RecoverableHandleCache.close_all (method)
- RecoverableHandleCache.status_for (method)

### gateway/shutdown_flush.py (2 gaps)

- spool_dropped_transcript_message (function)
- drain_transcript_spool (function)

### gateway/slash_commands.py (7 gaps)

- _home_thread_from_source (function)
- async GatewaySlashCommandsMixin._handle_heartbeat_command (method)
- async GatewaySlashCommandsMixin._handle_refine_command (method)
- async GatewaySlashCommandsMixin._handle_review_command (method)
- async GatewaySlashCommandsMixin._get_loop_manager_for_event (method)
- async GatewaySlashCommandsMixin._handle_loop_command (method)
- async GatewaySlashCommandsMixin._handle_save_command (method)

### gateway/status.py (1 gaps)

- _get_code_identity_fields (function)

### gateway/stream_consumer.py (9 gaps)

- GatewayStreamConsumer._stream_is_message (method)
- GatewayStreamConsumer._append_accumulated (method)
- GatewayStreamConsumer._mark_skip_redundant_finalize (method)
- GatewayStreamConsumer._record_turn_final_payload (method)
- GatewayStreamConsumer.delivered_final_matches (method)
- GatewayStreamConsumer._fallback_flood_retry_delay (method)
- GatewayStreamConsumer._is_flood_error (method)
- async GatewayStreamConsumer._abandon_native_stream (method)
- GatewayStreamConsumer._raw_message_limit (method)

### hermes_cli/_early_recovery.py (8 gaps)

- restore_quarantined_shims (function)
- _pid_is_running (function)
- _marker_owner_is_live (function)
- _find_uv_binary (function)
- _base_interpreter_is_externally_managed (function)
- _claim_recovery_lock (function)
- _release_recovery_lock (function)
- _complete_pending_core_install (function)

### hermes_cli/_install_repair.py (8 gaps)

- _stdout_to_stderr (function)
- _venv_is_relocatable (function)
- _normalize_windows_path (function)
- _windows_user_path_entries (function)
- ensure_windows_bin_launchers (function)
- _read_user_path_raw (function)
- _write_user_path_raw (function)
- migrate_windows_bin_path (function)

### hermes_cli/_parser.py (2 gaps)

- top_level_value_flag_sets (function)
- build_top_level_parser (function)

### hermes_cli/_scan_venv_blockers.py (4 gaps)

- _classify_local_preview_args (function)
- _local_preview_metadata (function)
- _terminate_safe_preview (function)
- _terminate_safe_main (function)

### hermes_cli/_subprocess_compat.py (3 gaps)

- resolve_node_command (function)
- _legacy_kill_process_tree (function)
- bounded_probe_run (function)

### hermes_cli/active_sessions.py (4 gaps)

- _pid_alive (function)
- try_acquire_active_session (function)
- release_active_session (function)
- active_session_registry_snapshot (function)

### hermes_cli/agent_plugins.py (1 gaps)

- _discover_skills (function)

### hermes_cli/approvals_test.py (2 gaps)

- evaluate_command (function)
- approvals_test_command (function)

### hermes_cli/auth.py (8 gaps)

- _secret_matches_declared_prefix (function)
- _parse_retry_after_seconds (function)
- _scope_values (function)
- _spotify_scope_list (function)
- get_auth_status (function)
- _confirm_selection_guards (function)
- login_command (function)
- logout_command (function)

### hermes_cli/auth_commands.py (4 gaps)

- _get_custom_provider_entries (function)
- _configured_provider_entry (function)
- _migrate_legacy_custom_pool_key (function)
- _is_known_provider (function)

### hermes_cli/azure_detect.py (2 gaps)

- detect (function)
- lookup_context_length (function)

### hermes_cli/backup.py (9 gaps)

- _should_skip_backup_file (function)
- _validate_backup_zip (function)
- _detect_prefix (function)
- _default_new_file_mode (function)
- _extract_member_atomically (function)
- _sibling_profile_homes (function)
- create_pre_update_snapshots_all_profiles (function)
- restore_cron_jobs_all_profiles (function)
- _prune_pre_update_backups (function)

### hermes_cli/banner.py (10 gaps)

- cprint (function)
- _github_compare_behind (function)
- _upstream_main_sha (function)
- _check_via_rev (function)
- _check_via_local_git (function)
- get_git_banner_state (function)
- get_latest_release_tag (function)
- format_banner_version_label (function)
- prefetch_banner_data (function)
- build_welcome_banner (function)

### hermes_cli/browser_connect.py (2 gaps)

- get_chrome_debug_candidates (function)
- is_browser_debug_ready (function)

### hermes_cli/build_info.py (3 gaps)

- _resolve_git_head_sha (function)
- get_code_identity (function)
- get_build_sha (function)

### hermes_cli/callbacks.py (3 gaps)

- clarify_callback (function)
- prompt_for_secret (function)
- approval_callback (function)

### hermes_cli/checkpoints.py (2 gaps)

- cmd_prune (function)
- register_cli (function)

### hermes_cli/cli_agent_setup_mixin.py (3 gaps)

- CLIAgentSetupMixin._ensure_runtime_credentials (method)
- CLIAgentSetupMixin._resolve_turn_agent_config (method)
- CLIAgentSetupMixin._resume_history_limit_error (method)

### hermes_cli/cli_commands_mixin.py (9 gaps)

- CLICommandsMixin._handle_export_command (method)
- CLICommandsMixin._handle_import_command (method)
- CLICommandsMixin._handle_worktree_command (method)
- CLICommandsMixin._handle_heartbeat_command (method)
- CLICommandsMixin._handle_refine_command (method)
- CLICommandsMixin._handle_review_command (method)
- CLICommandsMixin._handle_loop_command (method)
- CLICommandsMixin._handle_wake_command (method)
- CLICommandsMixin._persist_wake_word_enabled (method)

### hermes_cli/cli_output.py (1 gaps)

- line_input (function)

### hermes_cli/clipboard.py (2 gaps)

- save_clipboard_image (function)
- has_clipboard_image (function)

### hermes_cli/codex_models.py (2 gaps)

- _add_context_variants (function)
- get_codex_model_ids (function)

### hermes_cli/codex_runtime_plugin_migration.py (1 gaps)

- migrate (function)

### hermes_cli/codex_runtime_switch.py (3 gaps)

- get_current_runtime (function)
- set_runtime (function)
- apply (function)

### hermes_cli/commands.py (2 gaps)

- _personalities_from_cli_config (function)
- SlashCommandCompleter.get_completions (method)

### hermes_cli/config.py (8 gaps)

- _canonical_api_mode (function)
- _prompt_text (function)
- resolve_ephemeral_system_prompt_from_config (function)
- load_env (function)
- _valid_cron_impact_job_id (function)
- _cron_impact_job_name (function)
- _unavailable_cron_model_impact (function)
- _looks_structured_value (function)

### hermes_cli/config_migrations.py (5 gaps)

- _migrate_to_34 (function)
- _migrate_to_35 (function)
- _migrate_to_36 (function)
- _migrate_to_37 (function)
- _migrate_to_38 (function)

### hermes_cli/container_boot.py (1 gaps)

- reconcile_profile_gateways (function)

### hermes_cli/copilot_auth.py (4 gaps)

- _invalidate_gh_cli_token_cache (function)
- _probe_gh_cli_token (function)
- copilot_device_code_login (function)
- get_copilot_api_token (function)

### hermes_cli/cron.py (1 gaps)

- _builtin_gateway_liveness (function)

### hermes_cli/curator.py (4 gaps)

- _cmd_ledger (function)
- _cmd_purge (function)
- _cmd_usage (function)
- register_cli (function)

### hermes_cli/dashboard_auth/cookies.py (2 gaps)

- _clear_cookie_variants (function)
- _pkce_attrs (function)

### hermes_cli/dashboard_auth/middleware.py (1 gaps)

- _client_ip (function)

### hermes_cli/dashboard_auth/token_auth.py (1 gaps)

- _client_ip (function)

### hermes_cli/dep_ensure.py (2 gaps)

- _has_npx_agent_browser (function)
- ensure_dependency (function)

### hermes_cli/dingtalk_auth.py (2 gaps)

- begin_registration (function)
- poll_registration (function)

### hermes_cli/doctor.py (5 gaps)

- _hermes_database_paths (function)
- _unreadable_reason (function)
- _report_database_journal_modes (function)
- _doctor_web_capability_rows (function)
- check_certificates (function)

### hermes_cli/env_loader.py (1 gaps)

- load_hermes_dotenv (function)

### hermes_cli/fallback_cmd.py (1 gaps)

- cmd_fallback (function)

### hermes_cli/foreign_sessions.py (8 gaps)

- _read_json_lines (function)
- list_claude_sessions (function)
- list_codex_sessions (function)
- _first_user_line (function)
- import_foreign_session (function)
- gather_foreign_sessions (function)
- pick_foreign_session (function)
- run_sessions_import (function)

### hermes_cli/gateway.py (22 gaps)

- _wait_for_pid_exit (function)
- _escalate_wedged_gateway (function)
- _systemd_restart_wait_timeout (function)
- _parse_launchd_pid_from_print_output (function)
- _launchd_print_service_pid (function)
- _launchd_service_registered (function)
- _locate_launchd_gateway_service (function)
- _reaper_candidate_is_supervisor_owned (function)
- _windows_scheduled_task_state (function)
- _windows_scheduled_task_running (function)
- _windows_scheduled_task_supervises (function)
- _windows_console_window_attached (function)
- _windows_gateway_breakaway_state (function)
- _path_exists_safe (function)
- _runtime_dir_is_ours (function)
- _probe_launchd_domain_for_label (function)
- _launchctl_label_supervising_process (function)
- _timestamped_stderr_gateway_command (function)
- _launchd_kickstart (function)
- _wait_for_launchd_service_pid (function)
- run_gateway (function)
- gateway_setup (function)

### hermes_cli/gateway_enroll.py (1 gaps)

- _warn_if_secondary_multiplex_profile (function)

### hermes_cli/gateway_windows.py (3 gaps)

- install (function)
- is_installed (function)
- restart (function)

### hermes_cli/goals.py (1 gaps)

- _goal_judge_timeout (function)

### hermes_cli/inventory.py (5 gaps)

- load_picker_context (function)
- _reasoning_catalog_reader (function)
- _apply_custom_aliases (function)
- _anthropic_oauth_credentials_present (function)
- _provider_is_keyless (function)

### hermes_cli/kanban_db.py (11 gaps)

- _kanban_observer_consumed (function)
- _fire_worker_spawned_hook (function)
- notify_task_updated (function)
- _fire_dispatch_tick_hook (function)
- _looks_like_tls_record_at (function)
- _schema_is_present (function)
- _cleanup_worktree_workspace (function)
- _system_memory_sample (function)
- _memory_pressure_level (function)
- _worker_terminal_timeout_env (function)
- purge_stale_done_notify_subs (function)

### hermes_cli/kanban_decompose.py (1 gaps)

- decompose_task (function)

### hermes_cli/kanban_diagnostics.py (1 gaps)

- compute_task_diagnostics (function)

### hermes_cli/kanban_specify.py (1 gaps)

- specify_task (function)

### hermes_cli/kanban_swarm.py (2 gaps)

- _activate_root_inline (function)
- _create_swarm_uncommitted (function)

### hermes_cli/linux_desktop_entry.py (1 gaps)

- _needs_interpreter (function)

### hermes_cli/managed_scope.py (2 gaps)

- _parse_env (function)
- _flatten_keys (function)

### hermes_cli/managed_uv.py (3 gaps)

- resolve_uv (function)
- ensure_uv (function)
- update_managed_uv (function)

### hermes_cli/mcp_catalog.py (4 gaps)

- list_catalog (function)
- installed_servers (function)
- install_entry (function)
- uninstall_entry (function)

### hermes_cli/mcp_config.py (3 gaps)

- cmd_mcp_add (function)
- cmd_mcp_list (function)
- cmd_mcp_configure (function)

### hermes_cli/migrate.py (2 gaps)

- cmd_migrate (function)
- cmd_migrate_xai (function)

### hermes_cli/model_catalog.py (2 gaps)

- get_catalog (function)
- seed_cache_from_checkout (function)

### hermes_cli/model_cost_guard.py (1 gaps)

- _can_trust_pricing_lookup (function)

### hermes_cli/model_switch.py (8 gaps)

- _entry_models_discovered (function)
- _fetch_picker_live_models (function)
- _ambiguous_alias_message (function)
- _scoped_key_env (function)
- _prefetch_provider_models_parallel (function)
- _collect_authed_provider_slugs (function)
- list_authenticated_providers (function)
- list_picker_providers (function)

### hermes_cli/models.py (22 gaps)

- _fetch_reasoning_caps_catalog (function)
- fetch_ollama_local_models (function)
- detect_provider_for_model (function)
- provider_label (function)
- _resolve_copilot_catalog_api_key (function)
- _merge_with_models_dev (function)
- update_provider_cache_entry (function)
- _resolve_anthropic_pool_catalog_credentials (function)
- copilot_default_headers (function)
- get_copilot_model_context (function)
- _lmstudio_server_root (function)
- _lmstudio_request_headers (function)
- _lmstudio_fetch_raw_models (function)
- probe_lmstudio_models (function)
- lmstudio_model_reasoning_options (function)
- _fetch_github_models (function)
- _copilot_catalog_ids (function)
- opencode_zen_free_runtime (function)
- github_model_reasoning_efforts (function)
- probe_api_models (function)
- fetch_api_models (function)
- cached_fetch_api_models (function)

### hermes_cli/nous_account.py (3 gaps)

- NousPortalAccountInfo.is_paid (method)
- NousPortalAccountInfo.is_free_tier (method)
- get_nous_portal_account_info (function)

### hermes_cli/nous_billing.py (1 gaps)

- _raise_for_error (function)

### hermes_cli/nous_subscription.py (2 gaps)

- _selected_provider (function)
- get_nous_subscription_features (function)

### hermes_cli/npm_engine.py (1 gaps)

- _provision_managed_npm (function)

### hermes_cli/oneshot.py (2 gaps)

- _normalize_skills (function)
- _build_preloaded_skills_prompt (function)

### hermes_cli/partial_compress.py (3 gaps)

- parse_partial_compress_args (function)
- split_history_for_partial_compress (function)
- rejoin_compressed_head_and_tail (function)

### hermes_cli/personality.py (1 gaps)

- persist_personality (function)

### hermes_cli/platforms.py (2 gaps)

- platform_label (function)
- get_all_platforms (function)

### hermes_cli/plugin_capabilities.py (2 gaps)

- _plugin_entry (function)
- _log_capability_decision (function)

### hermes_cli/plugin_dev.py (3 gaps)

- _doctor_runtime (function)
- _accepts_var_kwargs (function)
- doctor_plugin (function)

### hermes_cli/plugin_index.py (5 gaps)

- get_index_url (function)
- _load_seed_entries (function)
- _fetch_remote (function)
- load_index (function)
- _score_entry (function)

### hermes_cli/plugin_packs.py (9 gaps)

- PackPluginEntry.display (method)
- PackPluginEntry.install_identifier (method)
- parse_pack (function)
- load_pack (function)
- _seed_plugin_config (function)
- _source_to_repo_subdir (function)
- _sanitized_entry_config (function)
- export_pack (function)
- pack_command (function)

### hermes_cli/portal_cli.py (2 gaps)

- portal_command (function)
- add_parser (function)

### hermes_cli/profile_describer.py (2 gaps)

- describe_profile (function)
- list_describable_profiles (function)

### hermes_cli/profile_distribution.py (4 gaps)

- plan_install (function)
- install_distribution (function)
- update_distribution (function)
- describe_distribution (function)

### hermes_cli/profiles.py (9 gaps)

- profile_matches_home (function)
- list_profiles (function)
- create_profile (function)
- delete_profile (function)
- _make_profile_archive (function)
- _scrub_export_secrets (function)
- export_profile (function)
- import_profile (function)
- rename_profile (function)

### hermes_cli/projects_db.py (1 gaps)

- find_by_primary_path (function)

### hermes_cli/providers.py (3 gaps)

- get_label (function)
- is_aggregator (function)
- resolve_provider_full (function)

### hermes_cli/psutil_android.py (1 gaps)

- prepare_patched_psutil_sdist (function)

### hermes_cli/pt_input_extras.py (5 gaps)

- _lock_twins (function)
- _clear_vt100_prefix_cache (function)
- install_shift_enter_alias (function)
- install_ctrl_enter_alias (function)
- install_modify_other_keys_aliases (function)

### hermes_cli/pty_bridge.py (2 gaps)

- PtyBridge.pid (method)
- PtyBridge.is_alive (method)

### hermes_cli/relaunch.py (2 gaps)

- build_relaunch_argv (function)
- relaunch (function)

### hermes_cli/resource_limits.py (2 gaps)

- _configured_nofile_soft_limit (function)
- apply_nofile_soft_limit (function)

### hermes_cli/secret_prompt.py (1 gaps)

- masked_secret_prompt (function)

### hermes_cli/secrets_cli.py (5 gaps)

- _load_bw (function)
- __getattr__ (function)
- cmd_sync (function)
- cmd_disable (function)
- cmd_install (function)

### hermes_cli/security_audit.py (1 gaps)

- run_audit (function)

### hermes_cli/send_cmd.py (1 gaps)

- register_send_subparser (function)

### hermes_cli/service_manager.py (6 gaps)

- ServiceManager.restart (method)
- SystemdServiceManager.restart (method)
- LaunchdServiceManager.restart (method)
- WindowsServiceManager.install (method)
- WindowsServiceManager.restart (method)
- S6ServiceManager.restart (method)

### hermes_cli/session_recovery.py (2 gaps)

- _probe_populated_edge (function)
- _recover_via_lost_and_found (function)

### hermes_cli/sessions_cmd.py (1 gaps)

- _prune_never_active_keyed (function)

### hermes_cli/setup.py (29 gaps)

- _handle_setup_menu_navigation (function)
- _curses_prompt_choice (function)
- _prompt_container_resources (function)
- _prompt_vercel_sandbox_settings (function)
- _read_nearest_vercel_project (function)
- _install_neutts_deps (function)
- _install_kittentts_deps (function)
- _run_xai_oauth_login_from_setup (function)
- _setup_tts_provider (function)
- _setup_telegram_auto_result (function)
- _setup_telegram_auto (function)
- _prompt_telegram_bot_token (function)
- setup_telemetry (function)
- _get_section_config_summary (function)
- _skip_configured_section (function)
- _load_openclaw_migration_module (function)
- _print_migration_preview (function)
- _offer_openclaw_migration (function)
- _run_portal_one_shot (function)
- _setup_navigation_scope (function)
- _run_setup_steps (function)
- run_setup_action_with_navigation (function)
- _run_setup_wizard_impl (function)
- _run_first_time_quick_setup (function)
- _blank_slate_minimal_toolsets (function)
- _blank_slate_minimize_config (function)
- _run_blank_slate_setup (function)
- _blank_slate_walkthrough (function)
- _run_quick_setup (function)

### hermes_cli/skin_engine.py (4 gaps)

- list_skins (function)
- load_skin (function)
- get_active_skin (function)
- set_active_skin (function)

### hermes_cli/sqlite_safe_read.py (1 gaps)

- _tracking_factory (function)

### hermes_cli/status.py (1 gaps)

- redact_key (function)

### hermes_cli/stderr_timestamp.py (4 gaps)

- _write_timestamped_line (function)
- _copy_stderr_with_timestamps (function)
- _install_signal_forwarders (function)
- _parse_args (function)

### hermes_cli/stdio.py (1 gaps)

- configure_windows_stdio (function)

### hermes_cli/subcommands/pause.py (1 gaps)

- build_pause_parser (function)

### hermes_cli/subcommands/verify.py (1 gaps)

- build_verify_parser (function)

### hermes_cli/telegram_managed_bot.py (2 gaps)

- is_valid_telegram_bot_token (function)
- auto_setup_telegram_bot (function)

### hermes_cli/terminal_breadcrumbs.py (2 gaps)

- read_breadcrumb (function)
- resolve_breadcrumb_session (function)

### hermes_cli/tips.py (1 gaps)

- get_random_tip (function)

### hermes_cli/uninstall.py (1 gaps)

- remove_windows_bin_launchers (function)

### hermes_cli/update_cmd.py (34 gaps)

- _abort_zip_update_if_dirty_tree (function)
- _park_stashed_changes (function)
- _capture_active_lazy_features (function)
- _capture_active_tool_dependencies (function)
- _restore_active_tool_dependencies (function)
- _repair_node_deps_on_current_checkout (function)
- _wait_for_windows_update_gateway_exit (function)
- _detect_venv_python_processes (function)
- _dependency_sync_would_rewrite (function)
- _detect_self_loaded_native_modules (function)
- _abort_dependency_sync_if_self_locked (function)
- _defer_update_for_self_lock (function)
- _holder_value_flags (function)
- _hermes_holder_subcommand (function)
- _venv_launcher_ancestors (function)
- _leftover_pausable_gateway_pids (function)
- _ledger_reapable_backend_pids (function)
- _handoff_reapable_backend_pids (function)
- _looks_like_desktop_control_plane (function)
- _desktop_owns_gateway_lifecycle (function)
- _pause_windows_gateways_for_update (function)
- _cold_start_windows_gateway_after_update (function)
- _service_unit_supports_graceful_sigusr1_restart (function)
- _restart_macos_launchd_gateways (function)
- _surviving_gateway_pids_after_failed_restart (function)
- _warn_gateway_restart_phase_aborted (function)
- _refresh_bootstrap_cache_scripts (function)
- _resume_windows_gateways_after_update (function)
- _desktop_app_present (function)
- _rebuild_desktop_after_update (function)
- _restart_phase_failure_is_incomplete (function)
- _fleet_probe_expected_runtimes (function)
- _print_items (function)
- _service_restart_sec (function)

### hermes_cli/urllib_security.py (1 gaps)

- _resolved_https_context (function)

### hermes_cli/voice.py (3 gaps)

- start_recording (function)
- start_continuous (function)
- speak_text (function)

### hermes_cli/web_git.py (2 gaps)

- review_pr_list (function)
- _remote_of_ref (function)

### hermes_cli/web_routers/git.py (2 gaps)

- async gh_auth_status_route (function)
- async git_pr_list_route (function)

### hermes_cli/web_routers/mcp.py (1 gaps)

- async cancel_mcp_oauth_flow (function)

### hermes_cli/web_routers/profiles.py (5 gaps)

- _stat_fingerprint (function)
- _sidebar_db_fingerprint (function)
- _sidebar_profile_cache_get (function)
- _sidebar_profile_cache_put (function)
- _sidebar_singleflight_cache (function)

### hermes_cli/web_server.py (36 gaps)

- _eager_reconcile_own_session_db (function)
- should_require_dashboard_auth (function)
- _host_header_hostname (function)
- _fs_mime_type (function)
- _fs_looks_binary (function)
- _media_serve_roots (function)
- _decode_data_url (function)
- _managed_file_response (function)
- async stream_managed_file (function)
- async fs_download (function)
- _git_path (function)
- _profile_gateway_writer_identity (function)
- _owned_profile_platforms (function)
- _read_or_create_install_id (function)
- get_install_id (function)
- _is_profile_platform_status_key (function)
- _status_platform_key_allowed (function)
- _public_platform_entry (function)
- _merge_profile_gateway_platforms (function)
- async get_client_voice_config (function)
- _latest_update_receipt_summary (function)
- async get_update_receipt (function)
- _dashboard_code_skew_guard (function)
- _is_other_profile (function)
- _approval_mode_of (function)
- _broadcast_gateway_session_info (function)
- _notify_cron_provider_for_profile (function)
- _mutate_cron_for_profile (function)
- _profile_env_value (function)
- _gateway_fire_endpoint (function)
- async _forward_cron_fire_to_gateway (function)
- _gateway_intentionally_stopped (function)
- _plugin_terminal_backend_rows (function)
- _terminal_backend_rows (function)
- _gateway_ws_ticket_from_subprotocol (function)
- _port_bind_conflict (function)

### hermes_cli/win_pty_bridge.py (2 gaps)

- WinPtyBridge.pid (method)
- WinPtyBridge.is_alive (method)

### hermes_cli/xai_retirement.py (2 gaps)

- find_retired_xai_refs (function)
- apply_migration (function)

### tools/approval.py (13 gaps)

- _is_single_query_approval_context (function)
- _resolve_cli_approval_callback (function)
- _should_fall_through_to_cli_approval (function)
- _contains_shell_carrier (function)
- _mask_quoted_prose (function)
- _check_sudo_stdin_guard (function)
- _is_shell_token_spliced_gateway_lifecycle (function)
- _get_single_query_approval_mode (function)
- get_plugin_manager (function)
- _get_approval_transport_config (function)
- _present_with_selected_transport (function)
- _transport_denied_result (function)
- _await_coalesced_leader (function)

### tools/async_delegation.py (1 gaps)

- _capture_routing_origin (function)

### tools/audio_container.py (2 gaps)

- sniff_container (function)
- sniff_audio_ext (function)

### tools/binary_extensions.py (1 gaps)

- has_opaque_document_extension (function)

### tools/bot_relay.py (9 gaps)

- _normalize_roster_row (function)
- write_remote_roster (function)
- _envelope_ttl_seconds (function)
- _target_liveness (function)
- claim_pending_envelopes (function)
- _sweep_stale (function)
- cleanup_bot_relay_artifacts (function)
- waiter_command (function)
- _hermes_cli (function)

### tools/browser_use_cli.py (1 gaps)

- _floor_subprocess_path (function)

### tools/budget_config.py (1 gaps)

- _configured_mcp_result_size (function)

### tools/checkpoint_manager.py (8 gaps)

- _hash_file (function)
- _load_ledger (function)
- _save_ledger (function)
- _migrate_legacy_store (function)
- CheckpointManager.record_agent_write (method)
- CheckpointManager.safe_restore_plan (method)
- CheckpointManager.list_all_checkpoints (method)
- maybe_auto_prune_checkpoints (function)

### tools/clarify_gateway.py (4 gaps)

- _label_matches (function)
- _selection_attempt_tokens (function)
- _coerce_text_response_detailed (function)
- attempt_text_response_for_session (function)

### tools/code_execution_tool.py (6 gaps)

- _format_interrupted_output (function)
- _cache_probe_result (function)
- _probe_python (function)
- _python_environment_prefix (function)
- _uses_hermes_python_environment (function)
- _execute_code_handler (function)

### tools/computer_use/doctor.py (2 gaps)

- _drive_fallback_probes (function)
- _apply_display_count_guard (function)

### tools/cronjob_tools.py (7 gaps)

- _validate_bot_chat_deliver (function)
- _resolve_cron_context_deliver (function)
- _run_claimed_job (function)
- _latest_job_output_excerpt (function)
- _try_dispatch_background_run (function)
- _apply_continuity (function)
- _gateway_liveness_notice (function)

### tools/delegation_output_schema.py (2 gaps)

- coerce_output_schema (function)
- validate_output (function)

### tools/env_probe.py (1 gaps)

- _plugin_backend_is_remote (function)

### tools/environments/base.py (1 gaps)

- sanitize_task_id_for_path (function)

### tools/environments/local.py (7 gaps)

- _plugin_terminal_env_strip_keys (function)
- _same_path (function)
- _build_hermes_repo_root_aliases (function)
- _validated_runtime_venv (function)
- _get_hermes_site_packages (function)
- _strip_hermes_owned_pythonpath_and_runtime_markers (function)
- _strip_hermes_owned_pythonpath (function)

### tools/file_operations.py (5 gaps)

- ShellFileOperations._sample_file_bytes (method)
- ShellFileOperations._escape_native_tool_arg (method)
- ShellFileOperations._try_read_utf16 (method)
- ShellFileOperations._unicode_variant_match (method)
- ShellFileOperations._has_ancestor_tsconfig (method)

### tools/file_tools.py (6 gaps)

- _get_real_hermes_home (function)
- _protected_instruction_config (function)
- _request_protected_instruction_approval (function)
- _check_protected_instruction_write (function)
- _check_approval_required_write (function)
- _check_binary_document_write (function)

### tools/fuzzy_match.py (2 gaps)

- _backslash_runs (function)
- _detect_backslash_doubling (function)

### tools/image_generation_tool.py (9 gaps)

- _wait_fal_result (function)
- _resolve_fal_model (function)
- _upscale_image (function)
- _agent_visible_cache_path (function)
- check_fal_api_key (function)
- _normalize_krea_model (function)
- _confine_source_images (function)
- _handle_image_generate (function)
- _active_image_capabilities (function)

### tools/image_source.py (2 gaps)

- _ensure_container_env (function)
- async resolve_local_source_to_data_url (function)

### tools/interpreter_shutdown.py (1 gaps)

- interpreter_shutting_down (function)

### tools/interrupt.py (1 gaps)

- get_interrupt_reason (function)

### tools/kanban_tools.py (3 gaps)

- _is_dispatcher_owned_worker (function)
- _handle_request_review (function)
- _handle_request_changes (function)

### tools/lazy_deps.py (2 gaps)

- restore_features (function)
- _refresh_features (function)

### tools/react_to_message_tool.py (1 gaps)

- _react_to_message_with_db (function)

### tools/read_extract.py (2 gaps)

- _pdf_coverage_note_from_bytes (function)
- _sheet_rows (function)

### tools/self_repo_guard.py (2 gaps)

- guard_active (function)
- _scratch_dir_hint (function)

### tools/send_message_tool.py (2 gaps)

- prepare_send_message_platforms (function)
- resolve_send_target (function)

### tools/session_search_tool.py (4 gaps)

- _session_end_reason (function)
- _session_left_live_context (function)
- _is_fresh_reset_session (function)
- _session_search_impl (function)

### tools/skill_manager_tool.py (2 gaps)

- _find_skill (function)
- _attach_lint_findings (function)

### tools/skillevaluator_scan.py (1 gaps)

- tier1_advisory_enabled (function)

### tools/skills_hub.py (4 gaps)

- ClawHubSource._owner_from_payload (method)
- ClawHubSource._owner_matches (method)
- OptionalSkillSource._fetch_from_live_repo (method)
- OptionalSkillSource._list_remote_skill_dirs (method)

### tools/skills_sync.py (3 gaps)

- _hermes_home (function)
- _manifest_file (function)
- _essential_names (function)

### tools/skills_sync_client.py (6 gaps)

- resolve_sync_base_url (function)
- is_sync_eligible (function)
- list_synced_skill_names (function)
- _all_local_skill_names (function)
- build_commit (function)
- _build_root_tree (function)

### tools/skills_tool.py (1 gaps)

- _is_remote_env_backend (function)

### tools/terminal_hints.py (1 gaps)

- annotate_masked_success (function)

### tools/terminal_tool.py (9 gaps)

- _current_session_key (function)
- _in_delegated_child_context (function)
- _handle_sudo_failure (function)
- _maybe_reap_docker_orphans (function)
- _session_isolation_enabled (function)
- _plugin_env_flag (function)
- _is_container_backend (function)
- _get_plugin_env_provider (function)
- _interpret_signal_exit (function)

### tools/tool_result_storage.py (4 gaps)

- _prune_spillover_once (function)
- _is_host_side_env (function)
- _write_to_spillover (function)
- _sandbox_visible_spillover_path (function)

### tools/transcription_tools.py (4 gaps)

- _get_local_command_template (function)
- _command_stt_env_passthrough (function)
- _enforce_prompt_length_limit (function)
- _apply_pre_transcription_hook (function)

### tools/tts_tool.py (2 gaps)

- _concat_audio_files (function)
- _text_to_speech_single (function)

### tools/video_generation_tool.py (3 gaps)

- _missing_provider_error (function)
- _normalize_reference_images (function)
- _format_model_caveats (function)

### tools/vision_tools.py (3 gaps)

- async _stream_download_to_file (function)
- _crop_image_region (function)
- async _materialize_video_from_terminal_backend (function)

### tools/voice_client_config.py (4 gaps)

- _client_direct_enabled (function)
- _resolve_stt_client_config (function)
- _resolve_tts_client_config (function)
- resolve_client_voice_config (function)

### tools/voice_mode.py (1 gaps)

- is_tts_echo (function)

### tools/web_tools.py (6 gaps)

- _keyless_rescue_enabled (function)
- _rescue_eligible (function)
- _rescue_search (function)
- _policy_blocked_result (function)
- _rescue_extract (function)
- _provider_is_ready (function)

### utils.py (4 gaps)

- _is_contended_windows_replace_error (function)
- _rewrite_in_place (function)
- _copy_fallback (function)
- atomic_roundtrip_yaml_save (function)
