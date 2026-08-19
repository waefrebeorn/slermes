# Slermes REAL_GAP List — Function Level (live scanner)

> Generated 2026-08-19T00:36:53Z from `live_parity_scan.json` by `make parity-walkway`. **1,208 REAL_GAP across 173 modules** (of 13,450 total functions).

> This is the forward work plan. Each entry is a Python function not yet ported to C. Close by implementing real C + a single-line `/* PoP: fn @ module.py:fn */` annotation (see slermes-gap-closure skill). Never hand-edit — regenerate via the scanner.

### agent/auxiliary_client.py (1 gaps)

- _AuxProbeClientStub.__getattr__ (method)

### agent/chat_completion_helpers.py (3 gaps)

- _report_stale_nonstream_kill (function)
- _touch_stale_kill_activity (function)
- _resolve_direct_stale_timeout (function)

### agent/coding_context.py (2 gaps)

- _parse_status (function)
- build_coding_workspace_block (function)

### agent/conversation_loop.py (2 gaps)

- _restore_user_after_reference_handoff (function)
- _should_skip_model_call_for_reference_handoff (function)

### agent/credential_pool.py (1 gaps)

- get_env_prefer_dotenv (function)

### agent/delegation_context.py (4 gaps)

- non_dispatcher_owned_context (function)
- is_dispatcher_owned_worker_context (function)
- enter_non_dispatcher_owned_context (function)
- exit_non_dispatcher_owned_context (function)

### agent/image_gen_provider.py (1 gaps)

- ImageGenProvider.capabilities (method)

### agent/memory_manager.py (1 gaps)

- build_memory_context_block (function)

### agent/memory_provider.py (1 gaps)

- MemoryProvider.prefetch (method)

### agent/model_metadata.py (1 gaps)

- _fetch_codex_oauth_context_lengths (function)

### agent/monitoring/gateway_health.py (1 gaps)

- build_gateway_health_snapshot (function)

### agent/monitoring/gateway_health_export.py (3 gaps)

- _require_metrics_sdk (function)
- _start_metric_provider (function)
- _start_diagnostic_log_streamer (function)

### agent/native_compaction.py (1 gaps)

- native_compaction_context_management (function)

### agent/pet/generate/imagegen.py (1 gaps)

- _discover (function)

### agent/prompt_builder.py (2 gaps)

- hud_surface_note (function)
- _agents_md_directory_chain (function)

### agent/prompt_cache_boundary.py (3 gaps)

- register_stable_prefix (function)
- find_stable_prefix (function)
- clear_stable_prefixes (function)

### agent/reasoning_summaries.py (1 gaps)

- separate_glued_reasoning_blocks (function)

### agent/redact.py (1 gaps)

- _mask_control_split_tokens (function)

### agent/relay_runtime.py (3 gaps)

- managed_callback_guard.__enter__ (method)
- managed_callback_guard.__exit__ (method)
- relay_instrumentation_enabled (function)

### agent/secret_sources/base.py (1 gaps)

- get_source_environment (function)

### agent/subagent_lifecycle.py (1 gaps)

- SubagentLifecycleService._cleanup_locked (method)

### agent/thread_scoped_output.py (2 gaps)

- _ThreadRoutingStream._target (method)
- _ensure_installed (function)

### agent/title_generator.py (2 gaps)

- apply_instant_title (function)
- _session_is_untitled (function)

### agent/tool_executor.py (2 gaps)

- _authorization_gate_lock_timeout (function)
- _ConcurrentToolAuthorizationGate._human_wait_seconds (method)

### agent/turn_context.py (1 gaps)

- _maybe_title_session_at_turn_start (function)

### agent/verification_evidence.py (2 gaps)

- record_verify_run (function)
- _insert_evidence (function)

### cli.py (232 gaps)

- CanonicalUsage (function)
- _reverse_alias_for_display (function)
- _assistant_copy_text (function)
- _load_prefill_messages (function)
- _resolve_prefill_messages_file (function)
- _parse_reasoning_config (function)
- load_cli_config (function)
- _AsyncHttpxDelNeuter.find_spec (method)
- AIAgent (function)
- get_tool_definitions (function)
- get_toolset_for_tool (function)
- get_all_toolsets (function)
- get_toolset_info (function)
- validate_toolset (function)
- _sync_process_session_id (function)
- get_job (function)
- _cleanup_all_terminals (function)
- set_sudo_password_callback (function)
- set_approval_callback (function)
- set_secret_capture_callback (function)
- _cleanup_all_browsers (function)
- _mark_tui_input_modes_active (function)
- _prepare_deferred_agent_startup (function)
- _arm_exit_watchdog (function)
- _arm_exit_watchdog_on_shutdown_signal (function)
- _run_cleanup (function)
- _should_emit_cleanup_session_finalize (function)
- _notify_session_finalize (function)
- _emit_interrupted_session_end (function)
- _notify_single_query_session_finalize (function)
- _finalize_single_query (function)
- _reset_terminal_input_modes_on_exit (function)
- _git_repo_root (function)
- _setup_worktree (function)
- _worktree_has_unpushed_commits (function)
- _worktree_merge_cache_path (function)
- _worktree_commits_all_merged_upstream (function)
- _cleanup_worktree (function)
- _run_state_db_auto_maintenance (function)
- _run_checkpoint_auto_maintenance (function)
- _prune_stale_worktrees (function)
- _prune_orphaned_branches (function)
- _query_osc11_background (function)
- _detect_light_mode (function)
- _maybe_remap_for_light_mode (function)
- _install_skin_light_mode_hook (function)
- _SkinAwareAnsi.__str__ (method)
- _SkinAwareAnsi.__add__ (method)
- _SkinAwareAnsi.__radd__ (method)
- _post_stream_transform_output (function)
- _select_classic_cli_pt_output (function)
- _get_plugin_cmd_handler_names (function)
- _parse_skills_argument (function)
- save_config_value (function)
- _normalize_moa_model (function)
- _VoiceInputMessage.__str__ (method)
- HermesCLI._claim_active_session (method)
- HermesCLI._release_active_session (method)
- HermesCLI._invalidate (method)
- HermesCLI._paint_now (method)
- HermesCLI._force_full_redraw (method)
- HermesCLI._clear_prompt_toolkit_screen (method)
- HermesCLI._recover_after_resize (method)
- HermesCLI._schedule_resize_recovery (method)
- HermesCLI._status_bar_context_style (method)
- HermesCLI._battery_status_style (method)
- HermesCLI._compression_count_style (method)
- HermesCLI._build_context_bar (method)
- HermesCLI._format_prompt_elapsed (method)
- HermesCLI._format_idle_since (method)
- HermesCLI._get_status_bar_snapshot (method)
- HermesCLI._get_status_bar_session_title (method)
- HermesCLI._status_bar_display_width (method)
- HermesCLI._trim_status_bar_text (method)
- HermesCLI._right_align_status_title (method)
- HermesCLI._right_align_status_title_fragments (method)
- HermesCLI._get_tui_terminal_width (method)
- HermesCLI._use_minimal_tui_chrome (method)
- HermesCLI._scrollback_box_width (method)
- HermesCLI._tui_input_rule_height (method)
- HermesCLI._spinner_widget_height (method)
- HermesCLI._render_spinner_text (method)
- HermesCLI._on_reaction (method)
- HermesCLI._voice_record_key_label (method)
- HermesCLI.set_voice_record_key_cache (method)
- HermesCLI._get_voice_status_fragments (method)
- HermesCLI._build_status_bar_text (method)
- HermesCLI._get_status_bar_fragments (method)
- HermesCLI._normalize_model_for_provider (method)
- HermesCLI._on_thinking (method)
- HermesCLI._on_notice (method)
- HermesCLI._flush_credit_notices (method)
- HermesCLI._on_notice_clear (method)
- HermesCLI._current_reasoning_callback (method)
- HermesCLI._emit_reasoning_preview (method)
- HermesCLI._flush_reasoning_preview (method)
- HermesCLI._format_submitted_user_message_preview (method)
- HermesCLI._expand_paste_references (method)
- HermesCLI._print_user_message_preview (method)
- HermesCLI._stream_reasoning_delta (method)
- HermesCLI._close_reasoning_box (method)
- HermesCLI._stream_delta (method)
- HermesCLI._emit_stream_text (method)
- HermesCLI._flush_stream (method)
- HermesCLI._reset_stream_state (method)
- HermesCLI._slow_command_status (method)
- HermesCLI._command_spinner_frame (method)
- HermesCLI._busy_command (method)
- HermesCLI._open_external_editor (method)
- HermesCLI._inline_pastes (method)
- HermesCLI._install_tool_callbacks (method)
- HermesCLI._ensure_tirith_security (method)
- HermesCLI._show_security_advisories (method)
- HermesCLI._show_browser_backend_notice (method)
- HermesCLI.finalize_preloaded_skills (method)
- HermesCLI.show_banner (method)
- HermesCLI._restore_session_cwd (method)
- HermesCLI._render_resume_history_panel_lines (method)
- HermesCLI._try_attach_clipboard_image (method)
- HermesCLI._resolve_checkpoint_ref (method)
- HermesCLI._write_osc52_clipboard (method)
- HermesCLI._recover_terminal_input_modes (method)
- HermesCLI._preprocess_images_with_vision (method)
- HermesCLI._show_tool_availability_warnings (method)
- HermesCLI._show_status (method)
- HermesCLI._show_session_status (method)
- HermesCLI._fast_command_available (method)
- HermesCLI._command_available (method)
- HermesCLI.show_help (method)
- HermesCLI.show_tools (method)
- HermesCLI.show_toolsets (method)
- HermesCLI._list_recent_sessions (method)
- HermesCLI._show_recent_sessions (method)
- HermesCLI.show_history (method)
- HermesCLI._discard_session_if_empty (method)
- HermesCLI._launch_session_boundary_memory_flush (method)
- HermesCLI.new_session (method)
- HermesCLI._consume_pending_resume_selection (method)
- HermesCLI.save_conversation (method)
- HermesCLI.retry_last (method)
- HermesCLI.undo_last (method)
- HermesCLI._undo_content_to_text (method)
- HermesCLI._prefill_input_buffer (method)
- HermesCLI._run_curses_picker (method)
- HermesCLI._prompt_text_input (method)
- HermesCLI._prompt_text_input_modal (method)
- HermesCLI._submit_slash_confirm_response (method)
- HermesCLI._normalize_slash_confirm_choice (method)
- HermesCLI._get_slash_confirm_display_fragments (method)
- HermesCLI._open_model_picker (method)
- HermesCLI._confirm_expensive_model_switch (method)
- HermesCLI._confirm_and_apply_model_switch_result (method)
- HermesCLI._close_model_picker (method)
- HermesCLI._snapshot_model_runtime (method)
- HermesCLI._restore_model_runtime_snapshot (method)
- HermesCLI._compute_model_picker_viewport (method)
- HermesCLI._clear_persisted_context_for_model_switch (method)
- HermesCLI._apply_model_switch_result (method)
- HermesCLI._handle_model_picker_selection (method)
- HermesCLI._handle_model_switch (method)
- HermesCLI._handle_codex_runtime (method)
- HermesCLI._should_handle_model_command_inline (method)
- HermesCLI._should_handle_steer_command_inline (method)
- HermesCLI._output_console (method)
- HermesCLI._console_print (method)
- HermesCLI._resolve_personality_prompt (method)
- HermesCLI._show_gateway_status (method)
- HermesCLI.process_command (method)
- HermesCLI._try_launch_chrome_debug (method)
- HermesCLI._get_goal_manager (method)
- HermesCLI._get_heartbeat_manager (method)
- HermesCLI._start_heartbeat_watchdog (method)
- HermesCLI._owns_process_notification (method)
- HermesCLI._drain_process_notifications (method)
- HermesCLI._maybe_continue_goal_after_turn (method)
- HermesCLI._toggle_verbose (method)
- HermesCLI._transfer_session_yolo (method)
- HermesCLI._is_session_yolo_active (method)
- HermesCLI._toggle_yolo (method)
- HermesCLI._persist_session_yolo (method)
- HermesCLI._on_reasoning (method)
- HermesCLI._manual_compress (method)
- HermesCLI._handle_usage_command (method)
- HermesCLI._usage_reset (method)
- HermesCLI._show_usage (method)
- HermesCLI._show_insights (method)
- HermesCLI._check_config_mcp_changes (method)
- HermesCLI._split_destructive_skip (method)
- HermesCLI._confirm_destructive_slash (method)
- HermesCLI._confirm_and_reload_mcp (method)
- HermesCLI._on_tool_gen_start (method)
- HermesCLI._on_tool_progress (method)
- HermesCLI._on_tool_start (method)
- HermesCLI._on_tool_complete (method)
- HermesCLI._voice_start_recording (method)
- HermesCLI._voice_stt_model (method)
- HermesCLI._voice_restart_recording_async (method)
- HermesCLI._voice_stop_and_transcribe (method)
- HermesCLI._voice_speak_response_async (method)
- HermesCLI._voice_speak_response (method)
- HermesCLI._voice_submit_barge_utterance (method)
- HermesCLI._voice_beeps_enabled (method)
- HermesCLI._enable_voice_mode (method)
- HermesCLI._disable_voice_mode (method)
- HermesCLI._toggle_voice_tts (method)
- HermesCLI._show_voice_status (method)
- HermesCLI._persist_prompt_summary (method)
- HermesCLI._clarify_callback (method)
- HermesCLI._sudo_password_callback (method)
- HermesCLI._approval_callback (method)
- HermesCLI._approval_choices (method)
- HermesCLI._computer_use_approval_callback (method)
- HermesCLI._handle_approval_selection (method)
- HermesCLI._get_approval_display_fragments (method)
- HermesCLI._secret_capture_callback (method)
- HermesCLI._capture_modal_input_snapshot (method)
- HermesCLI._restore_modal_input_snapshot (method)
- HermesCLI._submit_secret_response (method)
- HermesCLI._cancel_secret_capture (method)
- HermesCLI._clear_secret_input_buffer (method)
- HermesCLI._clear_terminal_on_exit (method)
- HermesCLI._print_exit_summary (method)
- HermesCLI._get_tui_prompt_symbols (method)
- HermesCLI._audio_level_bar (method)
- HermesCLI._get_tui_prompt_fragments (method)
- HermesCLI._get_tui_prompt_text (method)
- HermesCLI._build_tui_style_dict (method)
- HermesCLI._apply_tui_skin_style (method)
- HermesCLI._get_extra_tui_widgets (method)
- HermesCLI._register_extra_tui_keybindings (method)
- HermesCLI._build_tui_layout_children (method)
- _run_kanban_goal_loop_q (function)

### cron/blueprint_catalog.py (2 gaps)

- BlueprintSlot.__post_init__ (method)
- get_blueprint (function)

### cron/jobs.py (15 gaps)

- _fire_job_lock (function)
- fire_claim_fence (function)
- _job_is_stale_error_recurring (function)
- _schedule_cadence_seconds (function)
- _record_persisted_error_recovery (function)
- get_persisted_error_recovery_stats (function)
- _set_alert_flag (function)
- mark_drift_alerted (function)
- clear_drift_alerted (function)
- note_fire_forward_failure (function)
- _mark_job_run_locked (function)
- clear_run_claim (function)
- _claim_job_for_fire_locked (function)
- heartbeat_fire_claim (function)
- _heartbeat_fire_claim_locked (function)

### cron/lifecycle_guard.py (4 gaps)

- _is_cloud_placeholder_path (function)
- _mask_data_sink_arguments (function)
- _lifecycle_command_scan_with_data_exemption (function)
- _direct_lifecycle_scan (function)

### cron/monitor.py (1 gaps)

- _snapshot_path (function)

### cron/scheduler.py (37 gaps)

- _close_late_session_db_result (function)
- _fallback_chain_phrase (function)
- _failure_streak_nudge (function)
- _CancelEventLike.is_set (method)
- _CombinedCancelEvent.is_set (method)
- try_register_running_job (function)
- release_running_job (function)
- _inflight_min_allowance_minutes (function)
- _cron_interval_minutes (function)
- _job_interval_minutes (function)
- get_inflight_guard_stats (function)
- _record_forced_release (function)
- sweep_stale_inflight (function)
- _write_usage_audit (function)
- _is_lock_contention_errno (function)
- _is_fd_exhaustion_text (function)
- _is_fd_exhaustion (function)
- _reclaim_fds_best_effort (function)
- _get_config_home_channel (function)
- _env_home_target_chat_id (function)
- _relay_fronted_delivery_platforms (function)
- _get_media_send_timeout (function)
- _terminate_cron_script_process (function)
- _drain_script_pipes (function)
- _windows_cron_bootstrap_argv (function)
- _is_transient_provider_resolve_error (function)
- _cron_preflight_enabled (function)
- _preflight_check_provider_key (function)
- _preflight_check_skills (function)
- _preflight_job_config (function)
- _cron_cleanup_timeout_seconds (function)
- _run_cron_cleanup_with_timeout (function)
- _BoundedCronSessionDB.__getattr__ (method)
- _run_with_fire_claim_heartbeat (function)
- _run_one_job_body (function)
- CronSchedulerRegistrationError.user_message (method)
- create_job_with_scheduler_registration (function)

### cron/scheduler_provider.py (11 gaps)

- _backoff_wait_seconds (function)
- _note_tick_failure (function)
- CronScheduler.supports_force_fire (method)
- CronScheduler.claim_fire (method)
- CronScheduler.fire_claimed (method)
- provider_supports_force_fire (function)
- provider_supports_split_fire (function)
- provider_supports_fire_cancel (function)
- _misfire_grace_minutes (function)
- fire_overdue_jobs (function)
- scheduler_for_profile_mode (function)

### gateway/agent_cache_pressure.py (1 gaps)

- _total_memory_bytes (function)

### gateway/kanban_watchers.py (1 gaps)

- _kanban_dispatch_allowed (function)

### gateway/lifecycle_ledger.py (1 gaps)

- _process_hermes_home (function)

### gateway/pairing.py (2 gaps)

- _iter_live_gateway_adapters (function)
- _secure_write (function)

### gateway/platforms/api_server.py (80 gaps)

- _approval_event_choices (function)
- ThreadSafeAsyncQueue.put_threadsafe (method)
- _clean_request_string (function)
- _request_reasoning_config (function)
- _request_service_tier (function)
- _apply_runtime_agent_overrides (function)
- _resolve_request_runtime_agent_kwargs (function)
- _request_agent_overrides (function)
- _message_text_prefix (function)
- _is_compressed_summary_message (function)
- _auto_truncate_response_history (function)
- _multimodal_validation_error (function)
- _session_chat_user_message (function)
- check_api_server_requirements (function)
- ResponseStore._tighten_file_permissions (method)
- ResponseStore.__len__ (method)
- async cors_middleware (function)
- _resolve_media_to_data_urls (function)
- _redact_api_error_text (function)
- _openai_error (function)
- _admit_api_agent_request (function)
- _release_pending_api_work (function)
- _reserve_pending_api_work (function)
- async body_limit_middleware (function)
- async security_headers_middleware (function)
- APIServerAdapter.active_agent_work_count (method)
- APIServerAdapter.interrupt_active_runs (method)
- APIServerAdapter._gateway_is_draining (method)
- APIServerAdapter._draining_response (method)
- APIServerAdapter._activate_admitted_request (method)
- APIServerAdapter._readiness_work_counts (method)
- APIServerAdapter._parse_cors_origins (method)
- APIServerAdapter._resolve_model_name (method)
- APIServerAdapter._cors_headers_for_origin (method)
- APIServerAdapter._clean_log_value (method)
- APIServerAdapter._request_audit_context (method)
- APIServerAdapter._request_audit_log_suffix (method)
- APIServerAdapter._cron_origin_from_request (method)
- APIServerAdapter._normalize_callback_platform (method)
- APIServerAdapter._get_platform_callback_adapter (method)
- async APIServerAdapter._handle_platform_event_callback (method)
- APIServerAdapter._make_profile_prefix_middleware (method)
- APIServerAdapter._http_route_table (method)
- APIServerAdapter._open_and_cache_session_db (method)
- APIServerAdapter._ensure_session_db (method)
- async APIServerAdapter._ensure_session_db_async (method)
- APIServerAdapter._parse_model_routes (method)
- APIServerAdapter._resolve_route (method)
- APIServerAdapter._clean_runtime_id (method)
- APIServerAdapter._split_provider_prefixed_model (method)
- APIServerAdapter._runtime_options_from_model_options (method)
- APIServerAdapter._session_runtime_request_from_body (method)
- APIServerAdapter._runtime_lock_error (method)
- APIServerAdapter._persist_session_runtime_lock (method)
- APIServerAdapter._parse_session_model_config (method)
- APIServerAdapter._runtime_request_from_persisted_session_lock (method)
- APIServerAdapter._effective_session_runtime_request (method)
- APIServerAdapter._sanitize_runtime_metadata (method)
- APIServerAdapter._normalize_session_source (method)
- APIServerAdapter._session_model_override_for (method)
- APIServerAdapter._request_route_conflict_error (method)
- APIServerAdapter._session_response (method)
- APIServerAdapter._message_response (method)
- async APIServerAdapter._read_json_body (method)
- async APIServerAdapter._get_existing_session_or_404 (method)
- async APIServerAdapter._conversation_history_for_session (method)
- async APIServerAdapter._handle_session_model_lock (method)
- async APIServerAdapter._write_sse_chat_completion (method)
- async APIServerAdapter._write_sse_responses (method)
- APIServerAdapter._check_jobs_available (method)
- APIServerAdapter._check_job_id (method)
- APIServerAdapter._build_response_conversation_history (method)
- APIServerAdapter._response_messages_turn_start_index (method)
- APIServerAdapter._turn_transcript_messages (method)
- APIServerAdapter._extract_output_items (method)
- APIServerAdapter._set_run_status (method)
- APIServerAdapter._make_run_event_callback (method)
- async APIServerAdapter._sweep_orphaned_runs (method)
- APIServerAdapter._sweep_orphaned_runs_once (method)
- APIServerAdapter._api_key_passes_startup_guard (method)

### gateway/platforms/base.py (3 gaps)

- _consume_detached_handler_exception (function)
- _translate_docker_container_media_path (function)
- async BasePlatformAdapter._bounded_history_media_paths_for_session (method)

### gateway/platforms/yuanbao.py (2 gaps)

- async MessageSender.send_text (method)
- async OutboundManager.send_text (method)

### gateway/relay/adapter.py (4 gaps)

- RelayAdapter.prime_routing_cache (method)
- async RelayAdapter.wait_for_auto_thread_info (method)
- RelayAdapter._note_prompt_resolved (method)
- async RelayAdapter._notify_prompt_expired (method)

### gateway/relay/ws_transport.py (1 gaps)

- _disconnect_drain_grace_s (function)

### gateway/run.py (3 gaps)

- async GatewayRunner._clear_restart_failure_count (method)
- async GatewayRunner._handle_message (method)
- GatewayRunner._check_slash_access (method)

### gateway/session.py (3 gaps)

- auto_continue_freshness_window (function)
- SessionStore._routing_scope (method)
- SessionStore._ensure_loaded_locked (method)

### gateway/shutdown_flush.py (2 gaps)

- spool_dropped_transcript_message (function)
- drain_transcript_spool (function)

### gateway/slash_commands.py (2 gaps)

- async GatewaySlashCommandsMixin._handle_heartbeat_command (method)
- async GatewaySlashCommandsMixin._handle_refine_command (method)

### gateway/stream_consumer.py (2 gaps)

- GatewayStreamConsumer._append_accumulated (method)
- GatewayStreamConsumer._mark_skip_redundant_finalize (method)

### hermes_cli/_parser.py (1 gaps)

- build_top_level_parser (function)

### hermes_cli/_subprocess_compat.py (1 gaps)

- resolve_node_command (function)

### hermes_cli/active_sessions.py (4 gaps)

- _pid_alive (function)
- try_acquire_active_session (function)
- release_active_session (function)
- active_session_registry_snapshot (function)

### hermes_cli/agent_import.py (14 gaps)

- AgentImporter.load_target_config (method)
- AgentImporter._run_claude_code (method)
- AgentImporter._run_codex (method)
- AgentImporter._load_claude_settings (method)
- AgentImporter._claude_mcp_servers (method)
- AgentImporter._load_codex_config (method)
- AgentImporter.import_context_file (method)
- AgentImporter.import_memories_dir (method)
- AgentImporter._merge_memory_entries (method)
- AgentImporter.import_permission_denylist (method)
- AgentImporter.import_mcp_servers (method)
- AgentImporter.import_skills (method)
- import_agent_command (function)
- print_import_report (function)

### hermes_cli/agent_plugins.py (1 gaps)

- _discover_skills (function)

### hermes_cli/approvals_test.py (2 gaps)

- evaluate_command (function)
- approvals_test_command (function)

### hermes_cli/auth.py (8 gaps)

- _LazyHttpx.__getattr__ (method)
- _LazyHttpx.__setattr__ (method)
- _LazyHttpx.__delattr__ (method)
- _scope_values (function)
- _spotify_scope_list (function)
- get_auth_status (function)
- login_command (function)
- logout_command (function)

### hermes_cli/backup.py (2 gaps)

- _validate_backup_zip (function)
- _prune_pre_update_backups (function)

### hermes_cli/banner.py (20 gaps)

- cprint (function)
- get_available_skills (function)
- _check_via_rev (function)
- _check_via_local_git (function)
- check_for_updates (function)
- _resolve_repo_dir (function)
- _git_short_hash (function)
- get_git_banner_state (function)
- _compute_git_banner_state (function)
- get_latest_release_tag (function)
- format_banner_version_label (function)
- prefetch_update_check (function)
- prefetch_banner_data (function)
- get_update_result (function)
- _defer_update_notice (function)
- banner_snapshot_fingerprint (function)
- load_banner_snapshot (function)
- save_banner_snapshot (function)
- compute_toolset_availability (function)
- build_welcome_banner (function)

### hermes_cli/blueprint_cmd.py (10 gaps)

- _resolve_origin (function)
- _parse_kv (function)
- match_blueprint (function)
- _humanize_schedule (function)
- build_blueprint_seed (function)
- _fmt_catalog (function)
- _fmt_candidates (function)
- _fmt_no_match (function)
- _manage_hint (function)
- handle_blueprint_command (function)

### hermes_cli/browser_connect.py (3 gaps)

- get_chrome_debug_candidates (function)
- is_browser_debug_ready (function)
- try_launch_chrome_debug (function)

### hermes_cli/callbacks.py (3 gaps)

- clarify_callback (function)
- prompt_for_secret (function)
- approval_callback (function)

### hermes_cli/checkpoints.py (3 gaps)

- cmd_list (function)
- cmd_prune (function)
- register_cli (function)

### hermes_cli/cli_agent_setup_mixin.py (3 gaps)

- CLIAgentSetupMixin._ensure_runtime_credentials (method)
- CLIAgentSetupMixin._resolve_turn_agent_config (method)
- CLIAgentSetupMixin._resume_history_limit_error (method)

### hermes_cli/cli_commands_mixin.py (4 gaps)

- CLICommandsMixin._handle_export_command (method)
- CLICommandsMixin._handle_import_command (method)
- CLICommandsMixin._handle_heartbeat_command (method)
- CLICommandsMixin._handle_refine_command (method)

### hermes_cli/clipboard.py (2 gaps)

- save_clipboard_image (function)
- has_clipboard_image (function)

### hermes_cli/commands.py (1 gaps)

- SlashCommandCompleter.get_completions (method)

### hermes_cli/config.py (4 gaps)

- _prompt_text (function)
- render_personality_prompt (function)
- resolve_ephemeral_system_prompt_from_config (function)
- load_env (function)

### hermes_cli/config_migrations.py (1 gaps)

- _migrate_to_34 (function)

### hermes_cli/console_engine.py (52 gaps)

- _capture_output (function)
- _is_status_footer_rule (function)
- _strip_console_status_footer (function)
- _table_summary (function)
- _split_line (function)
- _contains_shell_syntax (function)
- _format_sessions (function)
- _format_job (function)
- _parser_root (function)
- _subparser_actions (function)
- _choice_help (function)
- _clean_summary (function)
- _summaries_from_parser (function)
- _noop_console_command (function)
- _extracted_summaries (function)
- _registered_summaries (function)
- _builder_summaries (function)
- _adder_summaries (function)
- _invoke_namespace (function)
- _set_attrs (function)
- _dispatch_extracted_subcommand (function)
- _dispatch_registered_subcommand (function)
- _dispatch_builder_subcommand (function)
- _dispatch_adder_subcommand (function)
- _extracted_handler (function)
- _registered_handler (function)
- _builder_handler (function)
- _adder_handler (function)
- _register_command_family (function)
- HermesConsoleEngine.help_text (method)
- HermesConsoleEngine._register_defaults (method)
- HermesConsoleEngine._register_broad_cli_surface (method)
- HermesConsoleEngine._execute_builtin (method)
- HermesConsoleEngine._rejection_for (method)
- HermesConsoleEngine._help_result (method)
- HermesConsoleEngine._cap_output (method)
- _expect_no_args (function)
- _apply_confirmed_defaults (function)
- _status (function)
- _sessions_stats (function)
- _config_show (function)
- _sessions_export (function)
- _sessions_rename (function)
- _sessions_optimize (function)
- _sessions_repair (function)
- _profile_status (function)
- _cron_list (function)
- _cron_status (function)
- _cron_pause (function)
- _cron_resume (function)
- _cron_run (function)
- run_console_repl (function)

### hermes_cli/container_boot.py (1 gaps)

- reconcile_profile_gateways (function)

### hermes_cli/copilot_auth.py (2 gaps)

- copilot_device_code_login (function)
- get_copilot_api_token (function)

### hermes_cli/cron.py (5 gaps)

- cron_list (function)
- cron_tick (function)
- cron_status (function)
- cron_create (function)
- cron_edit (function)

### hermes_cli/curator.py (2 gaps)

- _cmd_usage (function)
- register_cli (function)

### hermes_cli/curses_ui.py (3 gaps)

- curses_checklist (function)
- curses_radiolist (function)
- curses_single_select (function)

### hermes_cli/dashboard_auth/routes.py (1 gaps)

- _client_ip (function)

### hermes_cli/dashboard_procs.py (3 gaps)

- _process_ppid (function)
- _lock_owned_serve_pids (function)
- _reap_orphaned_desktop_local_serves (function)

### hermes_cli/debug.py (4 gaps)

- collect_debug_report (function)
- build_debug_share (function)
- run_debug_share (function)
- run_debug_delete (function)

### hermes_cli/dep_ensure.py (1 gaps)

- ensure_dependency (function)

### hermes_cli/doctor.py (8 gaps)

- _hermes_database_paths (function)
- _report_database_journal_modes (function)
- check_ok (function)
- check_warn (function)
- check_fail (function)
- check_info (function)
- check_certificates (function)
- run_doctor (function)

### hermes_cli/doctor_live.py (15 gaps)

- _http_get (function)
- _browser_available (function)
- _launch_browser_probe (function)
- _probe_mcp_server (function)
- _classify_http (function)
- _probe_firecrawl (function)
- _probe_fal (function)
- _probe_browser (function)
- _audio_provider_probe (function)
- _probe_tts (function)
- _probe_stt (function)
- _report (function)
- _run_one (function)
- run_live_checks (function)
- maybe_run_live_checks (function)

### hermes_cli/env_loader.py (1 gaps)

- load_hermes_dotenv (function)

### hermes_cli/fallback_cmd.py (5 gaps)

- cmd_fallback_list (function)
- cmd_fallback_add (function)
- cmd_fallback_remove (function)
- cmd_fallback_clear (function)
- cmd_fallback (function)

### hermes_cli/gateway.py (8 gaps)

- _wait_for_pid_exit (function)
- find_gateway_pids (function)
- kill_gateway_processes (function)
- stop_profile_gateway (function)
- _launchctl_label_supervising_process (function)
- run_gateway (function)
- gateway_setup (function)
- gateway_command (function)

### hermes_cli/gateway_windows.py (3 gaps)

- install (function)
- is_installed (function)
- restart (function)

### hermes_cli/goals.py (32 gaps)

- workspace_fingerprint (function)
- run_gate (function)
- load_goal (function)
- save_goal (function)
- clear_goal (function)
- _goal_judge_max_tokens (function)
- _parse_judge_response (function)
- _render_background_block (function)
- judge_goal (function)
- _extract_json_object (function)
- GoalManager.is_active (method)
- GoalManager.has_goal (method)
- GoalManager.status_line (method)
- GoalManager.set (method)
- GoalManager.set_contract (method)
- GoalManager.mark_done (method)
- GoalManager.add_subgoal (method)
- GoalManager.remove_subgoal (method)
- GoalManager.clear_subgoals (method)
- GoalManager.render_subgoals (method)
- GoalManager.add_gate (method)
- GoalManager.remove_gate (method)
- GoalManager.clear_gates (method)
- GoalManager.render_gates (method)
- GoalManager._check_gates (method)
- GoalManager.wait_on (method)
- GoalManager.wait_on_session (method)
- GoalManager.wait_for_seconds (method)
- GoalManager.stop_waiting (method)
- GoalManager.is_waiting (method)
- GoalManager.next_continuation_prompt (method)
- GoalManager.render_contract (method)

### hermes_cli/inventory.py (1 gaps)

- load_picker_context (function)

### hermes_cli/kanban.py (56 gaps)

- _check_dispatcher_presence (function)
- build_parser (function)
- _is_delegated_child_cli_mutation (function)
- _dispatch_boards (function)
- _board_task_counts (function)
- _cmd_boards_list (function)
- _cmd_boards_create (function)
- _cmd_boards_rm (function)
- _cmd_boards_switch (function)
- _cmd_boards_show (function)
- _cmd_boards_rename (function)
- _cmd_boards_set_default_workdir (function)
- _cmd_init (function)
- _cmd_heartbeat (function)
- _cmd_assignees (function)
- _cmd_create (function)
- _cmd_swarm (function)
- _cmd_show (function)
- _cmd_assign (function)
- _cmd_set_model (function)
- _cmd_reclaim (function)
- _cmd_reassign (function)
- _cmd_diagnostics (function)
- _cmd_link (function)
- _cmd_unlink (function)
- _cmd_claim (function)
- _cmd_comment (function)
- _cmd_attach (function)
- _cmd_attachments (function)
- _cmd_attach_rm (function)
- _goal_mode_handoff_rejection (function)
- _cmd_complete (function)
- _cmd_edit (function)
- _cmd_block (function)
- _cmd_schedule (function)
- _cmd_unblock (function)
- _cmd_request_review (function)
- _cmd_request_changes (function)
- _cmd_reopen_review (function)
- _cmd_promote (function)
- _cmd_archive (function)
- _cmd_tail (function)
- _cmd_dispatch (function)
- _cmd_daemon (function)
- _cmd_watch (function)
- _cmd_notify_subscribe (function)
- _cmd_notify_list (function)
- _cmd_notify_unsubscribe (function)
- _cmd_log (function)
- _cmd_runs (function)
- _cmd_context (function)
- _cmd_specify (function)
- _cmd_decompose (function)
- _cmd_gc (function)
- _cmd_repair (function)
- run_slash (function)

### hermes_cli/kanban_db.py (18 gaps)

- _looks_like_tls_record_at (function)
- task_graph_contexts (function)
- task_graph_context (function)
- _resume_status_from_events (function)
- _parents_satisfied (function)
- _retry_status_for_run (function)
- goal_run_status (function)
- redact_review_value (function)
- request_review (function)
- request_changes (function)
- _reclaim_dangling_run (function)
- _landing_status_after_parents (function)
- reopen_review_task (function)
- invalidate_descendants_for_parent_reopen (function)
- reconcile_orphaned_running (function)
- check_respawn_guard (function)
- review_dispatch_enabled (function)
- _worker_terminal_timeout_env (function)

### hermes_cli/kanban_diagnostics.py (1 gaps)

- compute_task_diagnostics (function)

### hermes_cli/kanban_swarm.py (2 gaps)

- _activate_root_inline (function)
- _create_swarm_uncommitted (function)

### hermes_cli/main.py (6 gaps)

- cmd_verify (function)
- _register_linux_desktop_entry (function)
- _update_marker_path (function)
- _coalesce_session_name_args (function)
- _try_fast_chat_launch (function)
- _advertise_agent_env (function)

### hermes_cli/managed_uv.py (4 gaps)

- resolve_uv (function)
- ensure_uv (function)
- _uv_self_update_is_fresh (function)
- update_managed_uv (function)

### hermes_cli/mcp_catalog.py (5 gaps)

- list_catalog (function)
- installed_servers (function)
- is_installed (function)
- install_entry (function)
- uninstall_entry (function)

### hermes_cli/mcp_config.py (4 gaps)

- _info (function)
- cmd_mcp_add (function)
- cmd_mcp_list (function)
- cmd_mcp_configure (function)

### hermes_cli/memory_setup.py (2 gaps)

- _print_cancelled_setup (function)
- _clear_interactive_transition (function)

### hermes_cli/model_catalog.py (2 gaps)

- get_catalog (function)
- seed_cache_from_checkout (function)

### hermes_cli/model_normalize.py (2 gaps)

- detect_vendor (function)
- normalize_model_for_provider (function)

### hermes_cli/model_switch.py (4 gaps)

- _ambiguous_alias_message (function)
- _scoped_key_env (function)
- list_authenticated_providers (function)
- list_picker_providers (function)

### hermes_cli/models.py (10 gaps)

- is_nous_free_tier (function)
- get_default_model_for_provider (function)
- model_ids (function)
- _cached_catalog (function)
- _cache_catalog (function)
- list_available_providers (function)
- detect_provider_for_model (function)
- _find_openrouter_slug (function)
- provider_label (function)
- cached_fetch_api_models (function)

### hermes_cli/nous_account.py (3 gaps)

- NousPortalAccountInfo.is_paid (method)
- NousPortalAccountInfo.is_free_tier (method)
- get_nous_portal_account_info (function)

### hermes_cli/nous_billing.py (1 gaps)

- _raise_for_error (function)

### hermes_cli/nous_subscription.py (1 gaps)

- get_nous_subscription_features (function)

### hermes_cli/npm_engine.py (1 gaps)

- _provision_managed_npm (function)

### hermes_cli/observability/shared_metrics.py (5 gaps)

- SharedMetricsStore.record_client_active (method)
- SharedMetricsStore._validate_counter (method)
- SharedMetricsStore._record_counter_in_transaction (method)
- SharedMetricsStore._create_counter_aggregates_table (method)
- SharedMetricsStore._migrate_v1_counter_aggregates (method)

### hermes_cli/personality.py (1 gaps)

- persist_personality (function)

### hermes_cli/plugins.py (17 gaps)

- PluginManager._collect_directory_manifests (method)
- PluginManager.has_enabled_portable_mcp (method)
- PluginManager._load_portable_plugin (method)
- PluginManager.list_plugins (method)
- PluginManager.list_plugin_skill_metadata (method)
- PluginManager.get_portable_mcp_servers (method)
- PluginManager.has_portable_mcp_servers (method)
- has_enabled_agent_plugin_mcp (function)
- discover_plugins (function)
- start_background_plugin_discovery (function)
- _join_background_discovery (function)
- _plugin_toolset_keys_cache_path (function)
- _persist_plugin_toolset_keys (function)
- _read_plugin_keys_cache (function)
- get_plugin_toolset_keys_nowait (function)
- get_portable_mcp_server_names_nowait (function)
- resolve_pre_tool_block (function)

### hermes_cli/plugins_cmd.py (8 gaps)

- cmd_install (function)
- cmd_update (function)
- cmd_remove (function)
- cmd_enable (function)
- cmd_disable (function)
- _bundled_default_on (function)
- cmd_list (function)
- cmd_toggle (function)

### hermes_cli/portal_cli.py (2 gaps)

- portal_command (function)
- add_parser (function)

### hermes_cli/profile_distribution.py (4 gaps)

- plan_install (function)
- install_distribution (function)
- update_distribution (function)
- describe_distribution (function)

### hermes_cli/profiles.py (8 gaps)

- list_profiles (function)
- create_profile (function)
- delete_profile (function)
- _make_profile_archive (function)
- _scrub_export_secrets (function)
- export_profile (function)
- import_profile (function)
- rename_profile (function)

### hermes_cli/providers.py (3 gaps)

- get_label (function)
- is_aggregator (function)
- resolve_provider_full (function)

### hermes_cli/pt_input_extras.py (2 gaps)

- install_shift_enter_alias (function)
- install_ctrl_enter_alias (function)

### hermes_cli/pty_bridge.py (2 gaps)

- PtyBridge.pid (method)
- PtyBridge.is_alive (method)

### hermes_cli/resource_limits.py (2 gaps)

- _configured_nofile_soft_limit (function)
- apply_nofile_soft_limit (function)

### hermes_cli/runtime_provider.py (21 gaps)

- _config_base_url_trustworthy_for_bare_custom (function)
- _fallback_api_mode (function)
- _resolve_plain_custom_api_mode (function)
- _auto_detect_local_model (function)
- _copilot_runtime_api_mode (function)
- _nous_inference_base_url_override (function)
- _maybe_apply_codex_app_server_runtime (function)
- _resolve_runtime_from_pool_entry (function)
- resolve_requested_provider (function)
- _try_resolve_from_custom_pool (function)
- _lift_max_output_tokens (function)
- _lift_extra_headers (function)
- _get_named_custom_provider (function)
- has_named_custom_provider (function)
- _custom_provider_request_overrides (function)
- _resolve_named_custom_runtime (function)
- _resolve_openrouter_runtime (function)
- _resolve_azure_foundry_runtime (function)
- _resolve_explicit_runtime (function)
- resolve_runtime_provider (function)
- format_runtime_provider_error (function)

### hermes_cli/secrets_cli.py (3 gaps)

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

### hermes_cli/session_recovery.py (1 gaps)

- _append_skipped_range (function)

### hermes_cli/setup.py (32 gaps)

- _get_credential_pool_strategies (function)
- _set_credential_pool_strategy (function)
- _set_reasoning_effort (function)
- _curses_prompt_choice (function)
- is_noninteractive (function)
- _prompt_container_resources (function)
- _prompt_vercel_sandbox_settings (function)
- _read_nearest_vercel_project (function)
- setup_model_provider (function)
- _install_neutts_deps (function)
- _install_kittentts_deps (function)
- _run_xai_oauth_login_from_setup (function)
- _setup_tts_provider (function)
- _is_valid_telegram_bot_token (function)
- _setup_telegram_auto_result (function)
- _profile_name_from_hermes_home (function)
- _setup_telegram_auto (function)
- _prompt_telegram_bot_token (function)
- setup_tools (function)
- setup_telemetry (function)
- _get_section_config_summary (function)
- _skip_configured_section (function)
- _load_openclaw_migration_module (function)
- _print_migration_preview (function)
- _offer_openclaw_migration (function)
- _run_portal_one_shot (function)
- _run_first_time_quick_setup (function)
- _blank_slate_minimal_toolsets (function)
- _blank_slate_minimize_config (function)
- _run_blank_slate_setup (function)
- _blank_slate_walkthrough (function)
- _run_quick_setup (function)

### hermes_cli/skills_hub.py (21 gaps)

- do_search (function)
- do_browse (function)
- do_install (function)
- do_inspect (function)
- browse_skills (function)
- inspect_skill (function)
- do_list (function)
- do_check (function)
- do_update (function)
- do_audit (function)
- do_uninstall (function)
- do_reset (function)
- do_opt_out (function)
- do_opt_in (function)
- do_repair_official (function)
- do_tap (function)
- do_publish (function)
- do_snapshot_export (function)
- do_snapshot_import (function)
- skills_command (function)
- handle_skills_slash (function)

### hermes_cli/sqlite_safe_read.py (3 gaps)

- has_live_connection (function)
- _tracking_factory (function)
- offline_file_access (function)

### hermes_cli/status.py (2 gaps)

- check_mark (function)
- redact_key (function)

### hermes_cli/subcommands/pause.py (1 gaps)

- build_pause_parser (function)

### hermes_cli/subcommands/verify.py (1 gaps)

- build_verify_parser (function)

### hermes_cli/tips.py (1 gaps)

- get_random_tip (function)

### hermes_cli/tools_config.py (72 gaps)

- _post_setup_no_window_flags (function)
- _homeassistant_credentials_present (function)
- _get_effective_configurable_toolsets (function)
- _get_plugin_toolset_keys (function)
- _checklist_toolset_keys (function)
- _cua_driver_cmd (function)
- _resolved_cua_driver_cmd (function)
- _cua_driver_env (function)
- _pip_install (function)
- _cua_install_target_writable (function)
- install_cua_driver (function)
- _cua_install_home (function)
- _cua_install_lock_dir (function)
- _cua_windows_install_lock_file (function)
- _clear_stale_windows_cua_install_lock (function)
- _clear_stale_cua_install_lock (function)
- _ps_single_quote (function)
- _cua_driver_autostart_registered_windows (function)
- _repair_cua_driver_autostart_windows (function)
- _run_cua_driver_installer (function)
- _run_post_setup (function)
- valid_post_setup_keys (function)
- run_post_setup_command (function)
- _enable_recently_shipped_toolsets (function)
- _toolset_has_keys (function)
- _estimate_tool_tokens (function)
- _prompt_toolset_checklist (function)
- _configure_toolset (function)
- _plugin_image_gen_providers (function)
- _plugin_video_gen_providers (function)
- _plugin_web_search_providers (function)
- web_provider_capabilities (function)
- _plugin_browser_providers (function)
- _plugin_tts_providers (function)
- _visible_providers (function)
- _hidden_nous_gateway_message (function)
- _post_setup_already_installed (function)
- _module_installed (function)
- _agent_browser_installed (function)
- _camofox_installed (function)
- _cloud_agent_browser_installed (function)
- provider_readiness_status (function)
- _toolset_needs_configuration_prompt (function)
- _configure_tool_category (function)
- _is_provider_active (function)
- _detect_active_provider_index (function)
- _fal_model_catalog (function)
- _configure_imagegen_model (function)
- _plugin_image_gen_catalog (function)
- _configure_imagegen_model_for_plugin (function)
- _configure_xai_imagine_storage (function)
- _select_plugin_image_gen_provider (function)
- _plugin_video_gen_catalog (function)
- _configure_videogen_model_for_plugin (function)
- _configure_stt_model (function)
- _select_plugin_video_gen_provider (function)
- _write_provider_config (function)
- apply_provider_selection (function)
- _configure_provider (function)
- _configure_vision_backend (function)
- _configure_vision_provider_model (function)
- _configure_simple_requirements (function)
- _reconfigure_tool (function)
- _toolset_enabled_for_reconfigure (function)
- _configure_tool_category_for_reconfig (function)
- _reconfigure_provider (function)
- _reconfigure_simple_requirements (function)
- _configure_mcp_tools_interactive (function)
- _apply_toolset_change (function)
- _apply_mcp_change (function)
- _print_tools_list (function)
- tools_disable_enable_command (function)

### hermes_cli/update_cmd.py (16 gaps)

- _reload_config_modules (function)
- _run_config_check_fresh (function)
- _run_migrate_config_fresh (function)
- _web_build_toolchain_ready (function)
- _wait_for_windows_update_gateway_exit (function)
- _detect_venv_python_processes (function)
- _venv_launcher_ancestors (function)
- _leftover_pausable_gateway_pids (function)
- _orphaned_desktop_backend_pids (function)
- _stop_process_trees (function)
- _pause_windows_gateways_for_update (function)
- _cold_start_windows_gateway_after_update (function)
- _refresh_bootstrap_cache_scripts (function)
- _resume_windows_gateways_after_update (function)
- _print_items (function)
- _service_restart_sec (function)

### hermes_cli/verify_cmd.py (4 gaps)

- run_verify_command (function)
- _merge_project_facts_commands (function)
- _record_evidence (function)
- _print_human_report (function)

### hermes_cli/voice.py (3 gaps)

- start_recording (function)
- start_continuous (function)
- speak_text (function)

### hermes_cli/web_git.py (1 gaps)

- review_pr_list (function)

### hermes_cli/web_routers/git.py (1 gaps)

- async git_pr_list_route (function)

### hermes_cli/web_routers/profiles.py (6 gaps)

- _warn_profile_read_error (function)
- get_profiles_projects_tree (function)
- post_profiles_sessions_pull_requests (function)
- async export_profile_endpoint (function)
- async import_profile_endpoint (function)
- async get_profile_desktop_overlay (function)

### hermes_cli/web_server.py (14 gaps)

- _fs_mime_type (function)
- _fs_looks_binary (function)
- _media_serve_roots (function)
- _decode_data_url (function)
- _git_path (function)
- _get_portal_status_sync (function)
- _terminate_desktop_managed_gateway (function)
- _get_env_vars_sync (function)
- _session_db_read_probe_statements (function)
- _open_session_db_at_path (function)
- _raise_if_cron_registration_error (function)
- _is_serve_orphaned (function)
- _start_parent_death_watchdog (function)
- _demo (function)

### hermes_cli/win_pty_bridge.py (2 gaps)

- WinPtyBridge.pid (method)
- WinPtyBridge.is_alive (method)

### hermes_cli/xai_retirement.py (2 gaps)

- find_retired_xai_refs (function)
- apply_migration (function)

### hermes_state.py (1 gaps)

- SessionDB.find_orphaned_gateway_sessions (method)

### tools/approval.py (1 gaps)

- _check_sudo_stdin_guard (function)

### tools/async_delegation.py (1 gaps)

- _capture_routing_origin (function)

### tools/browser_tool.py (2 gaps)

- _annotate_lightpanda_fallback (function)
- evaluate_url_safety (function)

### tools/checkpoint_manager.py (2 gaps)

- _migrate_legacy_store (function)
- maybe_auto_prune_checkpoints (function)

### tools/computer_use/backend.py (1 gaps)

- ComputerUseBackend.wait (method)

### tools/computer_use/cua_backend.py (5 gaps)

- _EmbeddedCuaDaemon.stop (method)
- _AsyncBridge.stop (method)
- _CuaDriverSession.stop (method)
- CuaDriverBackend.stop (method)
- CuaDriverBackend.drag (method)

### tools/computer_use/doctor.py (2 gaps)

- _drive_fallback_probes (function)
- _compose_fallback_report (function)

### tools/cronjob_tools.py (3 gaps)

- _run_claimed_job (function)
- _latest_job_output_excerpt (function)
- _try_dispatch_background_run (function)

### tools/delegate_tool.py (4 gaps)

- _close_subagent_steering (function)
- steer_subagent (function)
- _capture_gateway_steer_authority (function)
- _validate_batch_tasks (function)

### tools/delegation_output_schema.py (2 gaps)

- coerce_output_schema (function)
- validate_output (function)

### tools/file_operations.py (2 gaps)

- ShellFileOperations._sample_file_bytes (method)
- ShellFileOperations._unicode_variant_match (method)

### tools/file_tools.py (4 gaps)

- _get_real_hermes_home (function)
- _protected_instruction_config (function)
- _request_protected_instruction_approval (function)
- _check_protected_instruction_write (function)

### tools/image_generation_tool.py (8 gaps)

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

### tools/kanban_tools.py (4 gaps)

- _is_dispatcher_owned_worker (function)
- _goal_mode_handoff_rejection (function)
- _handle_request_review (function)
- _handle_request_changes (function)

### tools/mcp_tool.py (2 gaps)

- _get_mcp_stderr_log (function)
- __getattr__ (function)

### tools/memory_tool.py (21 gaps)

- get_memory_dir (function)
- _scan_memory_content (function)
- _drift_error (function)
- _read_failed_error (function)
- MemoryStore.reset_consolidation_failures (method)
- MemoryStore.load_from_disk (method)
- MemoryStore._sanitize_entries_for_snapshot (method)
- MemoryStore._file_lock (method)
- MemoryStore._reload_target (method)
- MemoryStore._set_entries (method)
- MemoryStore._batch_error (method)
- MemoryStore.format_for_system_prompt (method)
- MemoryStore._previews (method)
- MemoryStore._read_raw_checked (method)
- MemoryStore._parse_entries (method)
- MemoryStore._read_entries_checked (method)
- load_on_disk_store (function)
- _apply_write_gate (function)
- _apply_batch_write_gate (function)
- memory_tool (function)
- apply_memory_pending (function)

### tools/process_registry.py (6 gaps)

- _worker_memory_max_bytes (function)
- _systemd_run_user_scope_available (function)
- _is_supervised_gateway_process (function)
- _stop_systemd_unit (function)
- ProcessRegistry.kill_process (method)
- _handle_process (function)

### tools/read_extract.py (2 gaps)

- _pdf_coverage_note_from_bytes (function)
- _sheet_rows (function)

### tools/read_preview_tool.py (1 gaps)

- read_preview_tool (function)

### tools/read_window_tool.py (1 gaps)

- read_window_below_tool (function)

### tools/registry.py (1 gaps)

- check_fn_cache_scope (function)

### tools/skill_manager_tool.py (2 gaps)

- _find_skill (function)
- _attach_lint_findings (function)

### tools/skill_usage.py (4 gaps)

- telemetry_provenance (function)
- _emit_skill_lifecycle (function)
- record_created (function)
- record_installed (function)

### tools/skills_hub.py (3 gaps)

- github_provider_for (function)
- OptionalSkillSource._fetch_from_live_repo (method)
- OptionalSkillSource._list_remote_skill_dirs (method)

### tools/skills_sync_client.py (16 gaps)

- build_sync_manifest_bytes (function)
- parse_sync_manifest (function)
- wire_address (function)
- canonical_json_bytes (function)
- _decode_jwt_payload_unverified (function)
- resolve_identity (function)
- dev_gate_open (function)
- resolve_sync_base_url (function)
- sync_feature_enabled (function)
- sync_org_auto_propose (function)
- sync_default_opt_in (function)
- is_sync_eligible (function)
- list_synced_skill_names (function)
- _all_local_skill_names (function)
- build_commit (function)
- _build_root_tree (function)

### tools/skills_tool.py (1 gaps)

- _plugin_skill_linked_files (function)

### tools/terminal_hints.py (2 gaps)

- _hint_gh_unknown_json_field (function)
- annotate_failure (function)

### tools/terminal_tool.py (3 gaps)

- _handle_sudo_failure (function)
- _maybe_reap_docker_orphans (function)
- _docker_session_isolation_enabled (function)

### tools/tirith_security.py (6 gaps)

- _verify_checksum (function)
- _extract_tirith_binary (function)
- _install_tirith (function)
- _resolve_tirith_path (function)
- _background_install (function)
- ensure_installed (function)

### tools/transcription_tools.py (3 gaps)

- _get_local_command_template (function)
- _command_stt_env_passthrough (function)
- _confidence_thresholds (function)

### tools/tts_tool.py (3 gaps)

- _read_tts_response_bytes (function)
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

### tools/voice_mode.py (1 gaps)

- is_tts_echo (function)

### tools/wake_word.py (1 gaps)

- _get (function)

### utils.py (1 gaps)

- atomic_roundtrip_yaml_save (function)
