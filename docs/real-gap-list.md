# Slermes REAL_GAP List — Function Level (live scanner)

> Generated 2026-08-07T01:41:25Z from `live_parity_scan.json` by `make parity-walkway`. **1,346 REAL_GAP across 211 modules** (of 14,045 total functions).

> This is the forward work plan. Each entry is a Python function not yet ported to C. Close by implementing real C + a single-line `/* PoP: fn @ module.py:fn */` annotation (see slermes-gap-closure skill). Never hand-edit — regenerate via the scanner.

### agent/agent_runtime_helpers.py (9 gaps)

- _direct_native_anthropic_tool_cache_capability (function)
- cache_ttl_means_disabled (function)
- prompt_caching_disabled_from_config (function)
- blank_cache_policy_stub (function)
- plan_cache_sections_for_destination (function)
- _msg_has_payload (function)
- repair_empty_non_final_messages (function)
- _iter_httpx_pool_objects (function)
- _connection_candidates (function)

### agent/anthropic_adapter.py (4 gaps)

- _getenv (function)
- _is_nous_portal_endpoint (function)
- _fix_blank_text_blocks_in_list (function)
- _scrub_blank_text_blocks (function)

### agent/auxiliary_client.py (31 gaps)

- _aux_interrupt_cancel_requested (function)
- _capture_aux_cancel_check (function)
- _captured_aux_cancel_requested (function)
- _AuxiliaryCancellationDecision.begin_timeout_cleanup (method)
- _run_protected_sync_provider_call (function)
- _scoped_key_env (function)
- _is_free_model (function)
- _aux_openrouter_settings (function)
- _warn_paid_lane_once (function)
- _relay_auxiliary_call (function)
- _relay_auxiliary_call_async (function)
- _set_relay_auxiliary_route (function)
- _relay_auxiliary_metadata (function)
- _relay_sync_completion (function)
- async _relay_async_completion (function)
- _relay_sync_stream (function)
- _fallback_chain_entry (function)
- _fallback_provider_from_label (function)
- _complete_fallback_destination (function)
- _fallback_destination_from_entry (function)
- _fallback_destination (function)
- _replan_synchronous_cache_sections (function)
- _get_task_max_concurrency (function)
- _acquire_sync_aux_semaphore (function)
- _acquire_async_aux_semaphore (function)
- _reset_aux_semaphores (function)
- _complete_relay_auxiliary_call (function)
- _fail_relay_auxiliary_call (function)
- _release_sync_semaphore_after_stream (function)
- _call_llm_impl (function)
- async _async_call_llm_impl (function)

### agent/billing_view.py (1 gaps)

- _parse_payment_method (function)

### agent/chat_completion_helpers.py (3 gaps)

- _context_thread_target (function)
- _merge_nous_portal_messages_extra_body (function)
- _estimate_chunk_bytes (function)

### agent/coding_context.py (2 gaps)

- is_coding_context (function)
- project_facts_for (function)

### agent/context_breakdown.py (6 gaps)

- _bytes_to_tokens (function)
- compute_context_details (function)
- render_context_grid (function)
- render_context_category_lines (function)
- render_context_details_lines (function)
- render_context_breakdown_lines (function)

### agent/context_compressor.py (28 gaps)

- _template_visible_role (function)
- _reasoning_details_text_chars (function)
- ContextCompressor._emit_init_summary_once (method)
- ContextCompressor._resolve_context_length (method)
- ContextCompressor.context_length (method)
- ContextCompressor.context_length (method)
- ContextCompressor.threshold_tokens (method)
- ContextCompressor.threshold_tokens (method)
- ContextCompressor.tail_token_budget (method)
- ContextCompressor.tail_token_budget (method)
- ContextCompressor.max_summary_tokens (method)
- ContextCompressor.max_summary_tokens (method)
- ContextCompressor.record_timeout_failure (method)
- ContextCompressor._resolve_compact_cursor (method)
- ContextCompressor._find_one_exchange (method)
- ContextCompressor._serialize_one_exchange (method)
- ContextCompressor._build_micro_summary_prompt (method)
- ContextCompressor._micro_summarize_one (method)
- ContextCompressor._needs_defrag (method)
- ContextCompressor._defrag_rolling_summary (method)
- ContextCompressor._micro_compact (method)
- ContextCompressor._rolling_summary_from_marker (method)
- ContextCompressor._cursor_after_splice (method)
- ContextCompressor._emit_micro_compaction_telemetry (method)
- ContextCompressor._sync_micro_compact_to_db (method)
- ContextCompressor._splice_micro_compact_result (method)
- ContextCompressor._render_micro_marker_content (method)
- ContextCompressor._merge_adjacent_user_turns (method)

### agent/conversation_compression.py (18 gaps)

- _snapshot_compressor_attempt_state (function)
- _restore_compressor_attempt_state (function)
- _capture_authoritative_cooldown_under_lease (function)
- CompressionCommitFence.commit_in_flight (method)
- CompressionCommitFence.is_cancelled (method)
- CompressionCommitFence.revoke_commit_admission (method)
- CompressionCommitFence.begin_lock_setup (method)
- CompressionCommitFence.finish_lock_setup (method)
- CompressionCommitFence.register_cancelled_lock_release (method)
- CompressionCommitFence.clear_cancelled_lock_release (method)
- CompressionCommitFence.release_cancelled_compression_lock (method)
- _try_admit_compression_job (function)
- _release_compression_admission (function)
- _get_compress_timeout_executor (function)
- resolve_context_compression_timeouts (function)
- run_compress_context_with_progress_timeout (function)
- _CompressionActivityHeartbeat._fence_cancelled (method)
- _CompressionActivityHeartbeat._should_suppress (method)

### agent/conversation_loop.py (8 gaps)

- _is_copilot_provider (function)
- _is_stale_copilot_credential_error (function)
- _canonicalize_tool_call_arguments (function)
- _canonicalize_api_tool_calls (function)
- _rewrite_system_content_blocks (function)
- _ensure_cached_system_prompt_static (function)
- _peel_moa_guidance (function)
- _redecorate_prompt_cache_for_provider (function)

### agent/credential_pool.py (4 gaps)

- CredentialPool.next_available_at (method)
- CredentialPool._select_under_lock (method)
- CredentialPool._refresh_pending_entries (method)
- CredentialPool._acquire_lease_under_lock (method)

### agent/credits_tracker.py (1 gaps)

- new_credits_latch (function)

### agent/curator_backup.py (1 gaps)

- _unstage (function)

### agent/display.py (1 gaps)

- prepare_tool_preview (function)

### agent/gemini_native_adapter.py (1 gaps)

- is_standard_key_auth_error (function)

### agent/insights.py (1 gaps)

- InsightsEngine.get_usage_breakdown (method)

### agent/interrupt_compat.py (1 gaps)

- request_hard_interrupt (function)

### agent/lsp/eventlog.py (1 gaps)

- log_reaped (function)

### agent/lsp/manager.py (4 gaps)

- async LSPService._start_idle_reaper (method)
- LSPService._touch (method)
- async LSPService._idle_reaper_loop (method)
- async LSPService._reap_idle_once (method)

### agent/memory_provider.py (1 gaps)

- is_trivial_prompt (function)

### agent/message_sanitization.py (9 gaps)

- deterministic_call_id (function)
- coalesce_tool_call_id (function)
- uniquify_tool_call_ids (function)
- _family_rule (function)
- matches_reasoning_echo_family (function)
- reasoning_echo_family (function)
- needs_reasoning_echo (function)
- apply_reasoning_content_policy (function)
- reapply_reasoning_echo (function)

### agent/moa_loop.py (2 gaps)

- _completed_response_as_stream_chunk (function)
- peel_reference_guidance (function)

### agent/model_metadata.py (14 gaps)

- _ensure_requests (function)
- __getattr__ (function)
- _endpoint_host_key (function)
- _note_endpoint_blackholed (function)
- _endpoint_blackholed (function)
- _is_connect_timeout (function)
- _local_probe_disk_cache_path (function)
- _load_local_probe_disk_cache (function)
- _local_probe_disk_get (function)
- _local_probe_disk_put (function)
- _warn_context_length_fallback (function)
- _msg_fingerprint (function)
- _estimate_message_tokens_cached (function)
- _wire_message_shadow (function)

### agent/models_dev.py (6 gaps)

- _fetch_models_dev_from_network (function)
- _mark_stale_cache_grace (function)
- _commit_registry (function)
- _note_refresh_failure (function)
- _background_refresh_models_dev (function)
- _start_background_refresh_models_dev (function)

### agent/monitoring/cron_health.py (9 gaps)

- _now (function)
- _job_key (function)
- classify_cron_error (function)
- _parse_time (function)
- _duration_ms (function)
- project_execution_event (function)
- emit_execution_state (function)
- _is_overdue (function)
- build_cron_health_snapshot (function)

### agent/monitoring/events.py (4 gaps)

- _now_ns (function)
- GatewayHealthEvent.to_dict (method)
- GatewayDiagnosticEvent.to_dict (method)
- CronExecutionEvent.to_dict (method)

### agent/monitoring/gateway_health.py (2 gaps)

- GatewayDiagnosticLogHandler.__init__ (method)
- GatewayDiagnosticLogHandler.emit (method)

### agent/monitoring/gateway_health_export.py (25 gaps)

- _redact_string (function)
- _safe_resource_attributes (function)
- _runtime_resource_attributes (function)
- GatewayHealthExportRuntime.shutdown (method)
- _require_metrics_sdk (function)
- _resolve_headers (function)
- _version (function)
- _profile (function)
- _install_id (function)
- _supervision_mode (function)
- _read_gateway_snapshot (function)
- _read_cron_snapshot (function)
- _read_background_work_count (function)
- _read_background_delegations_count (function)
- _read_runtime_snapshot (function)
- _emit_snapshot_events (function)
- _start_metric_provider (function)
- GatewayDiagnosticLogStreamer.__init__ (method)
- GatewayDiagnosticLogStreamer.__call__ (method)
- GatewayDiagnosticLogStreamer.shutdown (method)
- _start_diagnostic_log_streamer (function)
- _start_snapshot_thread (function)
- _attach_log_handler (function)
- _gateway_health_event (function)
- start_gateway_health_export (function)

### agent/monitoring/otlp_exporter.py (14 gaps)

- _require_sdk (function)
- _resolve_headers (function)
- _otlp_config (function)
- build_exporter (function)
- _resource_attributes (function)
- _make_provider (function)
- _span_attrs (function)
- export_batch (function)
- OTLPStreamer.__init__ (method)
- OTLPStreamer.__call__ (method)
- OTLPStreamer.shutdown (method)
- is_available (function)
- is_enabled (function)
- start_streaming (function)

### agent/monitoring/policy.py (1 gaps)

- ensure_install_id (function)

### agent/monitoring/redaction.py (2 gaps)

- _secret_redact (function)
- redact_for_export (function)

### agent/outbound_webhooks.py (1 gaps)

- _NoRedirectHandler.redirect_request (method)

### agent/prompt_caching.py (6 gaps)

- PromptCachePlan.marker_count (method)
- strip_anthropic_cache_control (function)
- strip_anthropic_tool_cache_control (function)
- _count_cache_markers (function)
- _completed_transaction_endpoint_indexes (function)
- build_prompt_cache_plan (function)

### agent/relay_llm.py (17 gaps)

- async execute_async (function)
- execute_current (function)
- async execute_current_async (function)
- _has_running_event_loop (function)
- stream (function)
- ManagedLlmStream.__init__ (method)
- ManagedLlmStream.__iter__ (method)
- ManagedLlmStream.__next__ (method)
- ManagedLlmStream.close (method)
- ManagedLlmStream._preserve_pending_provider_chunks (method)
- ManagedLlmStream._close (method)
- ManagedLlmStream.__del__ (method)
- AnthropicStreamAccumulator.__init__ (method)
- AnthropicStreamAccumulator.observe (method)
- AnthropicStreamAccumulator.finalize (method)
- AnthropicStreamAccumulator.response (method)
- _run_awaitable (function)

### agent/relay_tools.py (4 gaps)

- execute (function)
- _jsonable (function)
- _json_equal (function)
- _run_awaitable (function)

### agent/retry_utils.py (1 gaps)

- parse_retry_after_seconds (function)

### agent/secret_scope.py (1 gaps)

- _strip_inline_comment (function)

### agent/secret_sources/base.py (3 gaps)

- set_source_environment (function)
- reset_source_environment (function)
- get_source_environment (function)

### agent/session_activity.py (4 gaps)

- bound_activity_description (function)
- normalize_activity_provenance (function)
- reset_session_activity_persist_window (function)
- build_activity_snapshot (function)

### agent/skill_utils.py (3 gaps)

- read_active_org_id (function)
- is_org_mirror_path (function)
- org_id_of_path (function)

### agent/subagent_lifecycle.py (1 gaps)

- SubagentLifecycleService._run (method)

### agent/subdirectory_hints.py (2 gaps)

- SubdirectoryHintTracker._seed_working_dir_digest (method)
- SubdirectoryHintTracker._is_excluded (method)

### agent/system_prompt.py (1 gaps)

- reconstruct_static_prefix (function)

### agent/tool_dispatch_helpers.py (1 gaps)

- _extract_parallel_scope_paths (function)

### agent/tool_executor.py (8 gaps)

- _image_generate_parallel_limit (function)
- _max_workers_for_tool_batch (function)
- _ConcurrentToolAuthorizationGate.__init__ (method)
- _ConcurrentToolAuthorizationGate.run (method)
- _ConcurrentToolAuthorizationGate.excluded_seconds (method)
- _managed_values (function)
- _begin_tool_execution (function)
- _append_cancelled_tool_results (function)

### agent/tool_guardrails.py (3 gaps)

- ToolCallGuardrailController._check_loop_cap (method)
- _non_negative_int (function)
- _subagent_spawn_count (function)

### agent/transports/chat_completions.py (3 gaps)

- _static_prompt_instructions (function)
- _add_prompt_cache_key (function)
- _is_openai_api_base_url (function)

### cli.py (25 gaps)

- _worktree_merge_cache_path (function)
- _load_worktree_merge_cache (function)
- _save_worktree_merge_cache (function)
- HermesCLI._spinner_token_flow (method)
- HermesCLI._turn_summary_is_active (method)
- HermesCLI._turn_summary_begin (method)
- HermesCLI._turn_summary_record (method)
- HermesCLI._turn_summary_emit (method)
- HermesCLI._status_bar_goal_segment (method)
- HermesCLI._fmt_stash_age (method)
- HermesCLI._render_stash_panel (method)
- HermesCLI._restore_session_yolo (method)
- HermesCLI._should_handle_background_command_inline (method)
- HermesCLI.handle_bang_shell (method)
- HermesCLI._persist_session_yolo (method)
- HermesCLI._show_context_breakdown (method)
- HermesCLI._voice_stt_provider (method)
- HermesCLI._voice_full_duplex_listener (method)
- HermesCLI._typed_voice_stop (method)
- HermesCLI._maybe_start_wake_word (method)
- HermesCLI._start_wake_word_listener (method)
- HermesCLI._stop_wake_word_listener (method)
- HermesCLI._on_wake_word (method)
- HermesCLI._start_wake_watchdog (method)
- HermesCLI._show_wake_word_status (method)

### cron/executions.py (1 gaps)

- _emit_execution_state (function)

### cron/jobs.py (8 gaps)

- _ensure_croniter (function)
- _atomic_write_counter (function)
- record_catch_up_occurrence (function)
- get_catch_up_occurrence_count (function)
- _write_wedged_oneshot_diagnostic (function)
- advance_next_runs (function)
- _completed_oneshot_retention_days (function)
- _sweep_completed_oneshots (function)

### cron/lifecycle_guard.py (10 gaps)

- _iter_command_segments (function)
- _command_token_index (function)
- contains_launchctl_submit_command (function)
- _resolve_terminal_script_path (function)
- _iter_referenced_shell_scripts (function)
- _iter_shell_command_payloads (function)
- _resolve_script_directory (function)
- _read_referenced_script (function)
- _contains_unsafe_gateway_action (function)
- contains_gateway_lifecycle_command_or_referenced_script (function)

### gateway/authz_mixin.py (1 gaps)

- _platform_gate_env (function)

### gateway/channel_directory.py (1 gaps)

- _normalize_adapter_channels (function)

### gateway/kanban_watchers.py (2 gaps)

- GatewayKanbanWatchersMixin._owns_kanban_dispatcher_lock (method)
- GatewayKanbanWatchersMixin._release_kanban_dispatcher_lock (method)

### gateway/lifecycle_ledger.py (11 gaps)

- _process_hermes_home (function)
- get_lifecycle_sentinel_path (function)
- sample_memory (function)
- _read_json (function)
- _write_sentinel (function)
- _append_exit_diag (function)
- _pid_alive_with_start_time (function)
- detect_unclean_exit (function)
- record_startup (function)
- mark_exited (function)
- read_prior_exit_label (function)

### gateway/pairing.py (10 gaps)

- _platform_uses_whatsapp_identity (function)
- _read_allowlist_env (function)
- _iter_live_gateway_adapters (function)
- _adapter_platform_name (function)
- _purge_allowlist_entries (function)
- _sync_live_adapter_allowlist_remove (function)
- PairingStore._finish_approval (method)
- PairingStore.looks_like_request_id (method)
- PairingStore.approve_request (method)
- PairingStore._reset_failed_attempts (method)

### gateway/platforms/api_server.py (5 gaps)

- _get_scoped_secret (function)
- _reap_disconnected_agent_processes (function)
- _publish_turn_process_ownership (function)
- _clear_turn_process_ownership (function)
- APIServerAdapter._expected_api_key (method)

### gateway/platforms/base.py (20 gaps)

- streaming_tts_turn_key (function)
- streaming_tts_should_skip_whole_file (function)
- _cleanup_cache_dir (function)
- _sniff_audio_ext (function)
- _match_extensionless_path (function)
- BasePlatformAdapter.max_message_length_for_chat (method)
- BasePlatformAdapter.message_len_fn_for_chat (method)
- BasePlatformAdapter.format_tool_preview (method)
- BasePlatformAdapter._history_media_paths_for_session (method)
- BasePlatformAdapter._ea_escape (method)
- BasePlatformAdapter._format_exec_approval (method)
- BasePlatformAdapter.supports_streaming_tts (method)
- async BasePlatformAdapter.begin_streaming_tts (method)
- async BasePlatformAdapter.write_streaming_tts (method)
- async BasePlatformAdapter.finish_streaming_tts (method)
- async BasePlatformAdapter.abort_streaming_tts (method)
- BasePlatformAdapter._streaming_tts_turn_key (method)
- BasePlatformAdapter._mark_streaming_tts_completed_turn (method)
- BasePlatformAdapter._streaming_tts_turn_completed (method)
- async BasePlatformAdapter._notify_media_delivery_failure (method)

### gateway/platforms/bluebubbles.py (1 gaps)

- _get_scoped_secret (function)

### gateway/platforms/helpers.py (14 gaps)

- compile_mention_patterns (function)
- text_has_unclosed_fence (function)
- text_ends_with_table_row (function)
- is_fence_atom (function)
- is_table_atom (function)
- split_at_paragraph_boundary (function)
- split_markdown_atoms (function)
- infer_block_separator (function)
- merge_streaming_fences (function)
- balance_fences_across_chunks (function)
- greedy_pack_blocks (function)
- split_text_fence_aware (function)
- _chunk_markdown_paragraphs (function)
- _chunk_newline_preferred (function)

### gateway/platforms/media_cache.py (4 gaps)

- _normalize_mime (function)
- ext_for_mime (function)
- mime_for_ext (function)
- cache_media_bytes (function)

### gateway/platforms/webhook.py (2 gaps)

- _is_webhook_silence_response (function)
- WebhookAdapter._route_allows_profile (method)

### gateway/platforms/whatsapp_common.py (2 gaps)

- _get_wsecret (function)
- WhatsAppBehaviorMixin._live_dm_allow_from (method)

### gateway/relay/__init__.py (1 gaps)

- relay_display_name (function)

### gateway/relay/adapter.py (36 gaps)

- RelayAdapter.supports_status_text (method)
- RelayAdapter._descriptor_for_chat (method)
- RelayAdapter.max_message_length_for_chat (method)
- RelayAdapter.message_len_fn_for_chat (method)
- async RelayAdapter.connect (method)
- RelayAdapter._relay_slack_extra (method)
- RelayAdapter._coerce_flag (method)
- RelayAdapter._effective_reply_in_thread (method)
- RelayAdapter._dm_top_level_threads_as_sessions (method)
- RelayAdapter._stamp_slack_session_thread (method)
- async RelayAdapter._localize_inbound_media (method)
- RelayAdapter._decode_prompt_token (method)
- RelayAdapter.auto_thread_info_for_chat (method)
- RelayAdapter._resolve_reply_to_for_send (method)
- RelayAdapter._apply_slack_thread_anchor (method)
- RelayAdapter._with_status_thread_anchor (method)
- RelayAdapter._get_media_client (method)
- async RelayAdapter._send_media (method)
- async RelayAdapter.send_image (method)
- async RelayAdapter.send_image_file (method)
- async RelayAdapter.send_voice (method)
- async RelayAdapter.send_video (method)
- async RelayAdapter.send_document (method)
- RelayAdapter._mint_prompt (method)
- RelayAdapter._pop_prompt (method)
- async RelayAdapter._send_prompt (method)
- async RelayAdapter.send_exec_approval (method)
- async RelayAdapter.send_slash_confirm (method)
- async RelayAdapter.send_clarify (method)
- async RelayAdapter._consume_prompt_response (method)
- RelayAdapter._prompt_reply_metadata (method)
- async RelayAdapter._react (method)
- async RelayAdapter.on_processing_start (method)
- async RelayAdapter.on_processing_complete (method)
- async RelayAdapter.create_handoff_thread (method)
- async RelayAdapter.rename_thread (method)

### gateway/relay/command_manifest.py (2 gaps)

- _opt (function)
- build_relay_command_manifest (function)

### gateway/relay/descriptor.py (1 gaps)

- CapabilityDescriptor.supports_op (method)

### gateway/relay/media.py (7 gaps)

- media_base_url (function)
- RelayMediaClient.__init__ (method)
- RelayMediaClient.enabled (method)
- RelayMediaClient._bearer (method)
- RelayMediaClient.is_relay_media_url (method)
- async RelayMediaClient.upload (method)
- async RelayMediaClient.download (method)

### gateway/relay/ws_transport.py (1 gaps)

- WebSocketRelayTransport.descriptor_for_platform (method)

### gateway/response_filters.py (1 gaps)

- is_autonomous_silence_response (function)

### gateway/restart.py (3 gaps)

- is_container_restart_context (function)
- parse_restart_after_turn_timeout (function)
- resolve_restart_exit_wait_budget (function)

### gateway/session.py (2 gaps)

- SessionStore._next_routing_generation_locked (method)
- SessionStore._save_entry (method)

### gateway/session_context.py (2 gaps)

- scoped_current_session_id (function)
- session_is_messaging_surface (function)

### gateway/session_stall.py (4 gaps)

- should_emit_session_stall_notification (function)
- should_clear_session_stall_notification (function)
- format_session_stall_notification (function)
- resolve_session_idle_seconds_from_activity (function)

### gateway/shutdown_flush.py (7 gaps)

- _get_flush_dir (function)
- _fsync_directory (function)
- _write_payload (function)
- flush_pending_to_file (function)
- _serialise_value (function)
- recover_pending_to_db (function)
- flush_agent_history_to_file (function)

### gateway/slash_commands.py (9 gaps)

- _clean_str (function)
- _int_value (function)
- async GatewaySlashCommandsMixin._handle_context_command (method)
- async GatewaySlashCommandsMixin._handle_diff_command (method)
- async GatewaySlashCommandsMixin._gateway_session_diff (method)
- GatewaySlashCommandsMixin._fenced_truncated_diff (method)
- async GatewaySlashCommandsMixin._handle_approvals_command (method)
- async GatewaySlashCommandsMixin._handle_compress_command_inner (method)
- GatewaySlashCommandsMixin._context_breakdown_block (method)

### gateway/stream_consumer.py (2 gaps)

- GatewayStreamConsumer._record_turn_final_payload (method)
- GatewayStreamConsumer.delivered_final_matches (method)

### gateway/streaming_tts_consumer.py (21 gaps)

- StreamingTTSConsumer.__init__ (method)
- StreamingTTSConsumer.active (method)
- StreamingTTSConsumer.completed (method)
- StreamingTTSConsumer.partial (method)
- StreamingTTSConsumer.started (method)
- StreamingTTSConsumer.audible (method)
- StreamingTTSConsumer.dropped (method)
- StreamingTTSConsumer.suppress_whole_file (method)
- StreamingTTSConsumer.done (method)
- StreamingTTSConsumer.on_delta (method)
- StreamingTTSConsumer.finish (method)
- StreamingTTSConsumer._enqueue_done (method)
- StreamingTTSConsumer.start (method)
- async StreamingTTSConsumer._run (method)
- async StreamingTTSConsumer._synthesise_and_write (method)
- async StreamingTTSConsumer._iter_stream_chunks (method)
- StreamingTTSConsumer._next_stream_chunk (method)
- StreamingTTSConsumer._strip_markdown_for_tts (method)
- async StreamingTTSConsumer._safe_abort (method)
- StreamingTTSConsumer.abort (method)
- async StreamingTTSConsumer.wait_complete (method)

### hermes_cli/_early_recovery.py (1 gaps)

- _pytest_owns_live_checkout (function)

### hermes_cli/_scan_venv_blockers.py (6 gaps)

- _probe_fail_json (function)
- _emit_probe_fail (function)
- _find_flag (function)
- _redact_sensitive_cmdline (function)
- _is_pausable_gateway (function)
- main (function)

### hermes_cli/_startup_fast.py (3 gaps)

- ensure_project_root_on_path (function)
- print_fast_version_info (function)
- try_fast_version (function)

### hermes_cli/_subprocess_compat.py (1 gaps)

- noninteractive_git_env (function)

### hermes_cli/active_sessions.py (3 gaps)

- format_age (function)
- summarize_holders (function)
- release_orphaned_leases (function)

### hermes_cli/agent_import.py (25 gaps)

- load_yaml_file (function)
- dump_yaml_file (function)
- parse_existing_memory_entries (function)
- backup_memory_file (function)
- default_source_dir (function)
- detect_agents (function)
- AgentImporter.__init__ (method)
- AgentImporter.record (method)
- AgentImporter.load_target_config (method)
- AgentImporter.build_report (method)
- AgentImporter.run (method)
- AgentImporter._run_claude_code (method)
- AgentImporter._run_codex (method)
- AgentImporter._load_claude_settings (method)
- AgentImporter._claude_mcp_servers (method)
- AgentImporter._load_codex_config (method)
- AgentImporter.import_context_file (method)
- AgentImporter.import_memories_dir (method)
- AgentImporter._merge_memory_entries (method)
- AgentImporter.import_permission_allowlist (method)
- AgentImporter.import_permission_denylist (method)
- AgentImporter.import_mcp_servers (method)
- AgentImporter.import_skills (method)
- import_agent_command (function)
- print_import_report (function)

### hermes_cli/approval_mode.py (2 gaps)

- _effective_mode (function)
- run_approval_mode_command (function)

### hermes_cli/approvals_suggest.py (15 gaps)

- Proposal.add_example (method)
- default_db_path (function)
- _connect_readonly (function)
- _iter_terminal_calls (function)
- _blocked_tool_call_ids (function)
- scan_approval_history (function)
- is_unsafe_class (function)
- _unsafe_root_binary (function)
- derive_glob (function)
- build_proposals (function)
- parse_apply_indices (function)
- apply_proposals (function)
- _render_text (function)
- suggest_command (function)
- approvals_command (function)

### hermes_cli/auth.py (5 gaps)

- _probe_single_zai_endpoint (function)
- _nous_device_auth_timeout_message (function)
- get_nous_auth_status_local (function)
- _minimax_response_error_text (function)
- _minimax_post_form (function)

### hermes_cli/backup.py (6 gaps)

- _backup_operation_lock (function)
- _atomic_output_path (function)
- is_zeroed_sqlite_file (function)
- _run_backup_locked (function)
- _create_quick_snapshot_locked (function)
- _write_full_zip_backup_locked (function)

### hermes_cli/bang_shell.py (7 gaps)

- is_bang_command (function)
- parse_bang_command (function)
- bang_shell_enabled (function)
- resolve_bang_cwd (function)
- check_bang_approval (function)
- _bang_env (function)
- run_bang_command (function)

### hermes_cli/cli_agent_setup_mixin.py (2 gaps)

- CLIAgentSetupMixin._runtime_credentials_ready (method)
- CLIAgentSetupMixin._offer_first_run_setup (method)

### hermes_cli/cli_commands_mixin.py (12 gaps)

- CLICommandsMixin._handle_diff_command (method)
- CLICommandsMixin._print_session_diff (method)
- CLICommandsMixin._print_diff_text (method)
- CLICommandsMixin._handle_init_command (method)
- CLICommandsMixin._handle_focus_command (method)
- CLICommandsMixin._set_tool_progress_mode (method)
- CLICommandsMixin._note_focus_hidden_line (method)
- CLICommandsMixin._emit_focus_recovery_line (method)
- CLICommandsMixin._handle_approvals_command (method)
- CLICommandsMixin._handle_indicator_command (method)
- CLICommandsMixin._handle_wake_command (method)
- CLICommandsMixin._persist_wake_word_enabled (method)

### hermes_cli/clipboard.py (4 gaps)

- _powershell_write_script (function)
- _write_clipboard_commands (function)
- is_remote_shell_session (function)
- write_clipboard_text (function)

### hermes_cli/commands.py (1 gaps)

- is_interrupt_then_dispatch (function)

### hermes_cli/config.py (8 gaps)

- _raw_config_has_explicit_version (function)
- read_user_config_raw (function)
- read_raw_config_readonly (function)
- _cron_model_drift_axis_for_config_key (function)
- cron_model_drift_guard_enabled (function)
- _cron_fleet_default_covers_axis (function)
- _load_cron_jobs_for_config_warning (function)
- warn_unpinned_cron_jobs_after_model_config_change (function)

### hermes_cli/config_migrations.py (16 gaps)

- support_floor_message (function)
- _cfg (function)
- _migrate_to_12 (function)
- _migrate_to_13 (function)
- _migrate_to_14 (function)
- _migrate_to_15 (function)
- _migrate_to_16 (function)
- _migrate_to_17 (function)
- _migrate_to_21 (function)
- _migrate_to_23 (function)
- _migrate_to_25 (function)
- _migrate_to_29 (function)
- _migrate_to_31 (function)
- _migrate_to_32 (function)
- _migrate_to_33 (function)
- run_migrations (function)

### hermes_cli/container_boot.py (1 gaps)

- _read_prior_exit_label (function)

### hermes_cli/copilot_auth.py (5 gaps)

- _read_jwt_store (function)
- evict_cached_exchanged_token (function)
- _jwt_disk_path (function)
- _load_jwt_from_disk (function)
- _save_jwt_to_disk (function)

### hermes_cli/curator.py (3 gaps)

- _print_unmanaged_summary (function)
- _cmd_list_unmanaged (function)
- _cmd_adopt (function)

### hermes_cli/dashboard_procs.py (4 gaps)

- _m (function)
- _scan_dashboard_processes (function)
- _kill_stale_dashboard_processes (function)
- _detect_concurrent_hermes_instances (function)

### hermes_cli/doctor.py (1 gaps)

- _sqlite_upgrade_hint (function)

### hermes_cli/env_loader.py (7 gaps)

- _known_hermes_env_keys (function)
- _env_keys_defined_in_dotenv (function)
- _clear_known_keys_missing_from_dotenv (function)
- hydrate_profile_secret_sources (function)
- _hydrate_profile_secret_sources (function)
- _reapply_terminal_config_bridge (function)
- _process_hermes_home (function)

### hermes_cli/gateway.py (3 gaps)

- _append_node_dir_for_service (function)
- _get_restart_after_turn_timeout (function)
- _get_restart_exit_wait_budget (function)

### hermes_cli/init_command.py (2 gaps)

- build_init_prompt (function)
- build_init_prompt_for_cwd (function)

### hermes_cli/inventory.py (3 gaps)

- build_aux_picker_rows (function)
- format_aux_picker_entries (function)
- _apply_featured (function)

### hermes_cli/kanban_db.py (9 gaps)

- normalize_reasoning_effort (function)
- _inherit_notify_subs (function)
- set_reasoning_effort (function)
- list_comments_after (function)
- _retag_legacy_worker_sessions (function)
- _encode_notify_delivery_metadata (function)
- _decode_notify_delivery_metadata (function)
- _notify_profile_filter (function)
- count_notify_subs (function)

### hermes_cli/lifecycle.py (3 gaps)

- invoke_hook (function)
- has_hook (function)
- finalize_session (function)

### hermes_cli/main.py (32 gaps)

- _project_root_str_fast (function)
- _ensure_project_root_on_path_fast (function)
- _is_global_fast_version_argv (function)
- _is_container_startup_environment_fast (function)
- _active_profile_may_override_home_fast (function)
- _container_mode_may_be_active_fast (function)
- _try_ultrafast_version (function)
- _resolve_workspace_key (function)
- cmd_sync (function)
- cmd_approvals (function)
- _record_bytecode_fingerprint (function)
- _sweep_stale_bytecode_if_checkout_changed (function)
- _run_npm_watching_for_engine_failure (function)
- _missing_web_build_tool (function)
- _windows_native_machine_from_iswow64 (function)
- _windows_user_runnable_pe_machines (function)
- _windows_native_machine (function)
- _desktop_macos_bundle_id (function)
- _desktop_macos_local_signing_identity (function)
- _desktop_macos_has_valid_real_signature (function)
- _desktop_macos_local_codesign (function)
- _parse_dashboard_runtime (function)
- _dashboard_probe_host (function)
- _get_systemd_service_for_pid (function)
- _extract_scope_from_cgroup (function)
- _get_pid_cgroup_path (function)
- _try_restart_systemd_service (function)
- _dashboard_cmdline_for_pid (function)
- _respawn_dashboard_processes (function)
- _pytest_owns_live_checkout (function)
- _resolve_deferred_platform_cli_command (function)
- cmd_monitoring (function)

### hermes_cli/managed_uv.py (9 gaps)

- _uv_self_update_is_fresh (function)
- _touch_uv_self_update_stamp (function)
- _reload_hermes_constants (function)
- _list_available_patches (function)
- _attempt_install_generation (function)
- _uv_version_string (function)
- _refresh_managed_uv_catalog (function)
- _default_live_venv (function)
- _sweep_stale_runtime_backups (function)

### hermes_cli/mcp_startup.py (1 gaps)

- ensure_mcp_discovery_before_agent_build (function)

### hermes_cli/mem_trim.py (9 gaps)

- _config_settings (function)
- _cooldown_seconds (function)
- _log_every_n (function)
- _nonnegative_float (function)
- _read_proc_status (function)
- collect_memory_snapshot (function)
- _should_log_trim (function)
- _probe_glibc_malloc_trim (function)
- trim_memory (function)

### hermes_cli/memory_setup.py (1 gaps)

- _provider_pip_dependencies (function)

### hermes_cli/model_catalog.py (1 gaps)

- _spawn_catalog_swr_refresh (function)

### hermes_cli/model_search.py (2 gaps)

- model_alias_canonical (function)
- model_search_text (function)

### hermes_cli/model_setup_flows.py (2 gaps)

- _existing_api_key_for_model_flow (function)
- _model_flow_ai_gateway (function)

### hermes_cli/model_switch.py (7 gaps)

- ModelSwitchRequest.model_input (method)
- ModelSwitchRequest.flags (method)
- ModelSwitchRequest.error_messages (method)
- parse_model_switch_args (function)
- _effective_model_candidate (function)
- resolve_effective_model (function)
- async resolve_display_context_length_async (function)

### hermes_cli/models.py (9 gaps)

- _ai_gateway_model_is_free (function)
- fetch_ai_gateway_models (function)
- ai_gateway_model_ids (function)
- fetch_ai_gateway_pricing (function)
- _provider_catalog_names (function)
- _model_dedup_key (function)
- _openai_discovery_base_url (function)
- _spawn_swr_refresh (function)
- _fetch_ai_gateway_models (function)

### hermes_cli/npm_engine.py (12 gaps)

- is_ebadengine (function)
- _iter_required_blocks (function)
- required_npm_range (function)
- actual_npm_version (function)
- _repo_npm_range (function)
- managed_npm_prefix (function)
- _upgrade_env (function)
- upgrade_managed_npm (function)
- _probe_version (function)
- _print_manual_fix (function)
- _provision_managed_npm (function)
- maybe_repair_npm_engine (function)

### hermes_cli/observability/__init__.py (3 gaps)

- observe_lifecycle (function)
- handles_hook (function)
- _safe_observe (function)

### hermes_cli/observability/relay_shared_metrics.py (32 gaps)

- _Runtime.__init__ (method)
- _Runtime.ensure_session (method)
- _Runtime._run_in_session (method)
- _Runtime.start_task (method)
- _Runtime._run_in_task (method)
- _Runtime.start_model_call (method)
- _Runtime.record_tool_call (method)
- _Runtime.end_model_call (method)
- _Runtime.end_pending_model_calls (method)
- _Runtime.finish_task (method)
- _Runtime.close_session (method)
- _Runtime.shutdown (method)
- _Runtime.deactivate (method)
- _Runtime._session (method)
- _Runtime._task_key (method)
- _Runtime._task_session (method)
- _Runtime._turn_key (method)
- _Runtime._remember_turn (method)
- _Runtime._finish_model_call (method)
- _Runtime._end_pending_model_calls (method)
- _Runtime._finish_task (method)
- _Runtime._export (method)
- _Runtime._event_metadata (method)
- _Runtime._safe (method)
- handles_hook (function)
- observe_lifecycle (function)
- prepare_session_start (function)
- _prepare_core_session (function)
- start_task_run (function)
- finish_task_run (function)
- _get_runtime (function)
- _reset_for_tests (function)

### hermes_cli/observability/shared_metrics.py (22 gaps)

- _utc_now (function)
- _isoformat (function)
- SharedMetricsStore.__init__ (method)
- SharedMetricsStore.record_model_call (method)
- SharedMetricsStore.record_counter (method)
- SharedMetricsStore.create_and_export_package (method)
- SharedMetricsStore.create_and_export_package_if_due (method)
- SharedMetricsStore._export_and_prune (method)
- SharedMetricsStore.counter_snapshot (method)
- SharedMetricsStore._connection (method)
- SharedMetricsStore._ensure_private_directory (method)
- SharedMetricsStore._ensure_private_file (method)
- SharedMetricsStore._ensure_schema (method)
- SharedMetricsStore._ensure_schema_in_transaction (method)
- SharedMetricsStore._install_id (method)
- SharedMetricsStore._pending_period_count (method)
- SharedMetricsStore._create_pending_packages_if_due (method)
- SharedMetricsStore._create_package (method)
- SharedMetricsStore._create_package_in_transaction (method)
- SharedMetricsStore._package_metric (method)
- SharedMetricsStore._export_pending_packages (method)
- SharedMetricsStore._prune_expired_history (method)

### hermes_cli/observability/shared_metrics_contract.py (5 gaps)

- model_call_dimensions (function)
- task_counter (function)
- _provider_metadata (function)
- _known_provider_ids (function)
- _model_locality (function)

### hermes_cli/observability/shared_metrics_subscriber.py (3 gaps)

- SharedMetricsSubscriber.__init__ (method)
- SharedMetricsSubscriber.deactivate (method)
- SharedMetricsSubscriber.__call__ (method)

### hermes_cli/plugins.py (1 gaps)

- PluginContext.subagent_lifecycle (method)

### hermes_cli/plugins_cmd.py (1 gaps)

- _clear_plugin_bytecode (function)

### hermes_cli/prompt_size.py (4 gaps)

- _tool_name (function)
- _skill_md_paths_by_name (function)
- _compute_skills_breakdown (function)
- _compute_toolsets_breakdown (function)

### hermes_cli/providers.py (3 gaps)

- is_official_openai_host (function)
- nous_api_mode (function)
- custom_provider_aliases (function)

### hermes_cli/pt_input_extras.py (1 gaps)

- install_cmd_backspace_alias (function)

### hermes_cli/route_identity.py (1 gaps)

- async should_clear_context_pin_async (function)

### hermes_cli/runtime_provider.py (1 gaps)

- _fallback_api_mode (function)

### hermes_cli/security_audit.py (1 gaps)

- _discover_components (function)

### hermes_cli/sessions_cmd.py (7 gaps)

- _m (function)
- get_hermes_home (function)
- _relative_time (function)
- _session_browse_picker (function)
- _size_delta_label (function)
- _confirm_prompt (function)
- cmd_sessions (function)

### hermes_cli/setup.py (3 gaps)

- _prompt_vercel_sandbox_settings (function)
- _read_nearest_vercel_project (function)
- setup_telemetry (function)

### hermes_cli/setup_hidden_env.py (1 gaps)

- is_setup_hidden_env (function)

### hermes_cli/slash_exec.py (9 gaps)

- _exec_version (function)
- _exec_egress (function)
- _exec_profile (function)
- _exec_bundles (function)
- _exec_help (function)
- _exec_commands (function)
- resolve_executor (function)
- run_execute (function)
- execute_command (function)

### hermes_cli/sqlite_safe_read.py (13 gaps)

- _key (function)
- _canonical_db_path (function)
- track_connection (function)
- untrack_connection (function)
- has_live_connection (function)
- _TrackingMixin.close (method)
- _tracking_factory (function)
- connect_tracked (function)
- _retrofit_tracking (function)
- page_count_bytes (function)
- file_length_matches_header (function)
- read_header_bytes_preopen (function)
- offline_file_access (function)

### hermes_cli/status.py (1 gaps)

- _format_relative_ts (function)

### hermes_cli/subcommands/approvals.py (1 gaps)

- build_approvals_parser (function)

### hermes_cli/subcommands/import_agent.py (1 gaps)

- build_import_agent_parser (function)

### hermes_cli/subcommands/monitoring.py (1 gaps)

- build_monitoring_parser (function)

### hermes_cli/subcommands/sync.py (1 gaps)

- build_sync_parser (function)

### hermes_cli/timefmt.py (1 gaps)

- relative_time (function)

### hermes_cli/tools_config.py (9 gaps)

- _homeassistant_credentials_present (function)
- _cua_install_home (function)
- _cua_windows_install_lock_file (function)
- _clear_stale_windows_cua_install_lock (function)
- _ps_single_quote (function)
- _cua_driver_autostart_registered_windows (function)
- _repair_cua_driver_autostart_windows (function)
- _enable_recently_shipped_toolsets (function)
- _configure_stt_model (function)

### hermes_cli/update_lock.py (11 gaps)

- update_marker_path (function)
- _pid_alive (function)
- _handoff_pid (function)
- _is_ancestor_pid (function)
- read_live_update (function)
- describe_holder (function)
- UpdateLock.__init__ (method)
- UpdateLock.acquire (method)
- UpdateLock.release (method)
- UpdateLock.__enter__ (method)
- UpdateLock.__exit__ (method)

### hermes_cli/vercel_auth.py (2 gaps)

- _present (function)
- describe_vercel_auth (function)

### hermes_cli/voice.py (3 gaps)

- set_voice_busy_probe (function)
- _voice_activity_held (function)
- _speak_text_streaming (function)

### hermes_cli/web_models.py (1 gaps)

- _MoaReferenceControls._validate_reference_timeout (method)

### hermes_cli/web_routers/cron.py (13 gaps)

- async list_cron_jobs (function)
- async get_cron_job (function)
- async list_cron_job_runs (function)
- async create_cron_job (function)
- async get_cron_delivery_targets (function)
- async update_cron_job (function)
- async pause_cron_job (function)
- async resume_cron_job (function)
- async trigger_cron_job (function)
- async delete_cron_job (function)
- async cron_fire_webhook (function)
- async list_cron_blueprints (function)
- async instantiate_blueprint (function)

### hermes_cli/web_routers/git.py (19 gaps)

- async git_status_route (function)
- async git_worktrees_route (function)
- async git_branches_route (function)
- async git_base_branches_route (function)
- async git_review_list_route (function)
- async git_review_diff_route (function)
- async git_file_diff_route (function)
- async git_commit_context_route (function)
- async git_rev_parse_route (function)
- async git_ship_info_route (function)
- async git_stage_route (function)
- async git_unstage_route (function)
- async git_revert_route (function)
- async git_commit_route (function)
- async git_push_route (function)
- async git_create_pr_route (function)
- async git_worktree_add_route (function)
- async git_worktree_remove_route (function)
- async git_branch_switch_route (function)

### hermes_cli/web_routers/mcp.py (11 gaps)

- async list_mcp_servers (function)
- async add_mcp_server (function)
- async replace_mcp_servers (function)
- async remove_mcp_server (function)
- async test_mcp_server (function)
- async auth_mcp_server (function)
- async mcp_oauth_flow_status (function)
- async mcp_oauth_callback (function)
- async set_mcp_server_enabled (function)
- async list_mcp_catalog (function)
- async install_mcp_catalog_entry (function)

### hermes_cli/web_routers/profiles.py (15 gaps)

- get_profiles_sessions (function)
- get_profiles_sessions_sidebar (function)
- async list_profiles_endpoint (function)
- async create_profile_endpoint (function)
- async get_active_profile_endpoint (function)
- async set_active_profile_endpoint (function)
- async get_profile_setup_command (function)
- async open_profile_terminal_endpoint (function)
- async rename_profile_endpoint (function)
- async delete_profile_endpoint (function)
- async get_profile_soul (function)
- async update_profile_soul (function)
- async update_profile_description_endpoint (function)
- async update_profile_model_endpoint (function)
- async describe_profile_auto_endpoint (function)

### hermes_cli/web_routers/sessions.py (14 gaps)

- get_sessions (function)
- async search_sessions (function)
- async bulk_delete_sessions_endpoint (function)
- async import_sessions_endpoint (function)
- async count_empty_sessions_endpoint (function)
- async delete_empty_sessions_endpoint (function)
- async get_session_stats (function)
- async get_session_detail (function)
- async get_session_latest_descendant (function)
- async get_session_messages (function)
- async delete_session_endpoint (function)
- async rename_session_endpoint (function)
- async export_session_endpoint (function)
- async prune_sessions_endpoint (function)

### hermes_cli/web_routers/skills.py (12 gaps)

- async install_skill_hub (function)
- async uninstall_skill_hub (function)
- async update_skills_hub (function)
- async list_skills_hub_sources (function)
- async search_skills_hub (function)
- async preview_skill_hub (function)
- async scan_skill_hub (function)
- async get_skills (function)
- async toggle_skill (function)
- async get_skill_content (function)
- async create_skill (function)
- async update_skill_content (function)

### hermes_cli/web_routers/tools.py (12 gaps)

- async get_toolsets (function)
- async toggle_toolset (function)
- async get_toolset_config (function)
- async get_toolset_models (function)
- async select_toolset_model (function)
- async select_toolset_provider (function)
- async save_toolset_env (function)
- async run_toolset_post_setup (function)
- async get_terminal_backends (function)
- async select_terminal_backend (function)
- async get_computer_use_status (function)
- async grant_computer_use_permissions (function)

### hermes_cli/web_server.py (7 gaps)

- _resolve_session_token (function)
- _timezone_options (function)
- _topology_cache_get (function)
- _collect_profile_gateway_topology_cached (function)
- _load_configured_gateway_platforms (function)
- _invalidate_plugins_hub_cache (function)
- _schedule_check_fn_probe (function)

### hermes_state.py (33 gaps)

- _default_db_path (function)
- apply_database_pragmas (function)
- SessionDB._store_system_prompt (method)
- SessionDB._delete_unreferenced_system_prompts (method)
- SessionDB._session_row_dict (method)
- SessionDB._get_read_conn (method)
- SessionDB._read_ctx (method)
- SessionDB._sleep_before_write_retry (method)
- SessionDB.get_compression_failure_cooldown_row (method)
- SessionDB.restore_compression_failure_cooldown_row (method)
- SessionDB.touch_session_activity (method)
- SessionDB.clear_session_activity_labels (method)
- SessionDB.get_session_activity (method)
- SessionDB.set_session_yolo (method)
- SessionDB.session_yolo_enabled (method)
- SessionDB.queue_token_counts (method)
- SessionDB.flush_token_counts (method)
- SessionDB._token_writer_loop (method)
- SessionDB._apply_token_batch (method)
- SessionDB._coalesce_token_deltas (method)
- SessionDB._stop_token_writer (method)
- SessionDB._drain_token_queue_at_exit (method)
- SessionDB._check_transcript_write_guards (method)
- SessionDB.append_messages_batch (method)
- SessionDB.set_message_reaction (method)
- SessionDB.get_message_reactions (method)
- SessionDB.take_unseen_reactions (method)
- SessionDB.latest_message_row_id (method)
- SessionDB.latest_user_message_row_id (method)
- SessionDB.get_message_role (method)
- SessionDB.session_count_ge (method)
- SessionDB.session_count_by_source (method)
- SessionDB.retag_kanban_worker_sessions (method)

### tools/approval.py (10 gaps)

- _is_cron_approval_context (function)
- _save_blocked_payload (function)
- _mask_quoted_newlines (function)
- _get_denial_breaker_threshold (function)
- _record_denial (function)
- _reset_denials (function)
- _denial_breaker_addendum (function)
- _release_permission_mode_dependents (function)
- is_approval_bypass_active_for_session (function)
- _get_smart_policy (function)

### tools/async_delegation.py (11 gaps)

- active_for_session (function)
- active_task_count (function)
- _matches_session_selectors (function)
- has_live_for_session (function)
- _begin_finalization (function)
- _finish_finalization (function)
- _push_batch_completion_event (function)
- _ensure_stale_monitor (function)
- _stale_monitor_loop (function)
- _finalize_stalled (function)
- _children_activity_from_token (function)

### tools/audio_container.py (2 gaps)

- sniff_container (function)
- sniff_audio_ext (function)

### tools/browser_supervisor.py (5 gaps)

- PendingDialog.to_dict (method)
- DialogRecord.to_dict (method)
- FrameInfo.to_dict (method)
- SupervisorSnapshot.to_dict (method)
- async CDPSupervisor._run (method)

### tools/browser_tool.py (6 gaps)

- __getattr__ (function)
- _lazy_call_llm (function)
- _get_cdp_override_raw (function)
- _resolve_allow_private_urls (function)
- _session_expiry_timestamp (function)
- _session_has_expired (function)

### tools/checkpoint_manager.py (1 gaps)

- CheckpointManager.session_diff (method)

### tools/clarify_gateway.py (1 gaps)

- _coerce_multi_select_text (function)

### tools/clarify_tool.py (2 gaps)

- _invoke_callback (function)
- _parse_multi_select_response (function)

### tools/code_execution_tool.py (1 gaps)

- _sandbox_failure_hint (function)

### tools/computer_use/backend.py (4 gaps)

- ComputerUseBackend._typed_browser_unavailable (method)
- ComputerUseBackend.typed_browser_state (method)
- ComputerUseBackend.typed_browser_prepare (method)
- ComputerUseBackend.typed_browser_action (method)

### tools/computer_use/cua_backend.py (15 gaps)

- _wsl_windows_path_to_posix (function)
- _EmbeddedCuaDaemon.child_env (method)
- _EmbeddedCuaDaemon._drain_stderr (method)
- _EmbeddedCuaDaemon.proxy_invocation (method)
- _CuaDriverSession.supports_input_property (method)
- _CuaDriverSession._logical_error_text (method)
- _CuaDriverSession._is_ended_session_result (method)
- _CuaDriverSession._revive_declared_session_once (method)
- _windows_from_tool_result (function)
- _apps_from_windows (function)
- CuaDriverBackend._browser_route (method)
- CuaDriverBackend._run_input_action (method)
- CuaDriverBackend.typed_browser_state (method)
- CuaDriverBackend.typed_browser_prepare (method)
- CuaDriverBackend.typed_browser_action (method)

### tools/computer_use/doctor.py (14 gaps)

- _is_valid_health_report (function)
- _read_cli_version (function)
- _normalize_version_token (function)
- _build_identity (function)
- _extract_health_report_from_result (function)
- _open_mcp (function)
- _mcp_rpc (function)
- _close_mcp (function)
- _cli_driver_version (function)
- _cli_doctor_snippet (function)
- _drive_fallback_probes (function)
- _platform_name (function)
- _compose_fallback_report (function)
- _drive_health_report_or_fallback (function)

### tools/computer_use/tool.py (5 gaps)

- _cua_permission_mode (function)
- release_computer_use_session (function)
- _shutdown_backend_atexit (function)
- _classify_action_result (function)
- _action_payload (function)

### tools/delegate_tool.py (8 gaps)

- _sanitize_tool_target (function)
- _summarize_tool_arguments (function)
- _sanitize_tool_input_summary (function)
- _subagent_stop_tool_call_history (function)
- _build_child_preserving_parent_tools (function)
- _parent_finalization_lock (function)
- _finalize_child_results (function)
- _run_child_lifecycle (function)

### tools/env_passthrough.py (1 gaps)

- resolve_passthrough_value (function)

### tools/environments/base.py (7 gaps)

- _BoundedOutputCollector._maybe_spill (method)
- _BoundedOutputCollector.close_spill (method)
- get_activity_callback (function)
- _export_dump_excluding_session_vars (function)
- BaseEnvironment._additional_profile_scoped_passthrough_names (method)
- BaseEnvironment._snapshot_excluded_passthrough_names (method)
- BaseEnvironment._finalize_wait_result (method)

### tools/environments/docker.py (6 gaps)

- _extra_args_set_shm_size (function)
- DockerEnvironment._additional_profile_scoped_passthrough_names (method)
- DockerEnvironment._build_passthrough_env (method)
- DockerEnvironment._resolve_passthrough_env (method)
- DockerEnvironment._build_runtime_env_args_with_unsets (method)
- DockerEnvironment._build_runtime_env_args (method)

### tools/environments/file_sync.py (2 gaps)

- FileSyncManager._sync_transaction (method)
- FileSyncManager._sync_back_transaction (method)

### tools/environments/local.py (2 gaps)

- build_subprocess_env (function)
- _managed_runtime_path_entries (function)

### tools/environments/vercel_sandbox.py (28 gaps)

- _ensure_vercel_sdk (function)
- _retry_vercel_call (function)
- _snapshot_store_path (function)
- _load_snapshots (function)
- _save_snapshots (function)
- _get_snapshot_id (function)
- _store_snapshot (function)
- _delete_snapshot (function)
- _sandbox_status_type (function)
- _terminal_sandbox_states (function)
- VercelSandboxEnvironment.__init__ (method)
- VercelSandboxEnvironment._build_create_params (method)
- VercelSandboxEnvironment._create_sandbox (method)
- VercelSandboxEnvironment._configure_attached_sandbox (method)
- VercelSandboxEnvironment._detect_workspace_root (method)
- VercelSandboxEnvironment._detect_remote_home (method)
- VercelSandboxEnvironment._wait_for_running (method)
- VercelSandboxEnvironment._close_sandbox_client (method)
- VercelSandboxEnvironment._stop_sandbox (method)
- VercelSandboxEnvironment._snapshot_sandbox (method)
- VercelSandboxEnvironment._ensure_sandbox_ready (method)
- VercelSandboxEnvironment._vercel_upload (method)
- VercelSandboxEnvironment._vercel_bulk_upload (method)
- VercelSandboxEnvironment._vercel_delete (method)
- VercelSandboxEnvironment._vercel_bulk_download (method)
- VercelSandboxEnvironment._before_execute (method)
- VercelSandboxEnvironment._run_bash (method)
- VercelSandboxEnvironment.cleanup (method)

### tools/file_operations.py (2 gaps)

- ShellFileOperations._try_multi_path_search (method)
- ShellFileOperations._zero_match_probe (method)

### tools/file_tools.py (4 gaps)

- _file_ops_uses_host_paths (function)
- _rewrite_v4a_patch_paths_for_host (function)
- _check_not_found_cache (function)
- _record_not_found (function)

### tools/flux3_video_tool.py (24 gaps)

- _endpoints (function)
- async _call_gateway (function)
- async _wait_between_looks (function)
- _warm_nous_token (function)
- async _prepare_media (function)
- async _deliver_media (function)
- async _save_if_ready (function)
- async _download_video (function)
- _delivers_as_an_attachment (function)
- _default_directory (function)
- _resolve_destination (function)
- _free_path (function)
- async _submit (function)
- async _handle_text_to_video (function)
- async _handle_image_to_video (function)
- async _handle_keyframes_to_video (function)
- async _handle_video_continuation (function)
- _still_generating (function)
- async _poll_until_done (function)
- async _handle_get_result (function)
- async _handle_prompting_guide (function)
- _has_nous_credential (function)
- check_bfl_requirements (function)
- _shared_submit_properties (function)

### tools/fuzzy_match.py (3 gaps)

- is_already_applied (function)
- _format_match_locations (function)
- _visualize_whitespace (function)

### tools/image_source.py (1 gaps)

- _detect_video_mime (function)

### tools/kanban_tools.py (1 gaps)

- inject_new_comments_from_env (function)

### tools/lazy_deps.py (1 gaps)

- install_specs (function)

### tools/managed_tool_gateway.py (8 gaps)

- _read_user_token_override (function)
- managed_vendor_base_path (function)
- managed_vendor_upload_path (function)
- managed_vendor_endpoints (function)
- is_managed_nous_gateway_url (function)
- managed_gateway_auth_headers (function)
- _describe_media_upload_refusal (function)
- build_managed_media_uploader (function)

### tools/mcp_oauth.py (4 gaps)

- _ensure_sdk_loaded (function)
- _is_figma_remote_mcp (function)
- apply_oauth_provider_defaults (function)
- humanize_oauth_registration_error (function)

### tools/mcp_schema_cache.py (10 gaps)

- _cache_path (function)
- config_fingerprint (function)
- _load_all (function)
- _save_all (function)
- get_cached_entry (function)
- has_cached_entry (function)
- write_cache_entry (function)
- clear_cache_entry (function)
- tools_from_cache_entry (function)
- utility_tools_from_cache_entry (function)

### tools/mcp_tool.py (10 gaps)

- _LockCookie.release (method)
- _acquire_lock_on_fh (function)
- _try_acquire_mcp_discovery_lock (function)
- _warn_hidden_whitespace (function)
- _resolve_server_lazy (function)
- _ensure_lazy_server_connected (function)
- matches_name_filter (function)
- _register_from_cache_sync (function)
- async _drain_mcp_loop_tasks (function)
- async _drain_and_stop_mcp_loop (function)

### tools/osv_check.py (2 gaps)

- _cache_get (function)
- _cache_put (function)

### tools/process_registry.py (2 gaps)

- ProcessRegistry.snapshot_running_ids (method)
- ProcessRegistry.kill_started_since (method)

### tools/react_to_message_tool.py (3 gaps)

- _open_session_db (function)
- react_to_message_tool (function)
- check_react_requirements (function)

### tools/registry.py (6 gaps)

- _discovery_cache_path (function)
- _load_discovery_cache (function)
- _save_discovery_cache (function)
- _prune_check_fn_caches (function)
- check_fn_cache_scope (function)
- get_cached_check_fn_result (function)

### tools/schema_sanitizer.py (3 gaps)

- sanitize_property_key (function)
- _rename_property_keys (function)
- unrename_tool_args (function)

### tools/session_search_tool.py (2 gaps)

- _get_message_storage_state (function)
- _session_link (function)

### tools/skill_manager_tool.py (3 gaps)

- _maybe_auto_propose_org_edit (function)
- _org_mirror_write_guard (function)
- _maybe_debounced_sync_push (function)

### tools/skill_usage.py (7 gaps)

- is_curator_managed (function)
- list_unmanaged_skill_names (function)
- unmanaged_report (function)
- adopt_skill (function)
- set_sync (function)
- is_sync_enabled (function)
- curated_report (function)

### tools/skills_hub.py (3 gaps)

- ClawHubSource._fetch_owner_handle (method)
- ClawHubSource.enrich_owners (method)
- _category_skill_dirs (function)

### tools/skills_sync.py (5 gaps)

- _index_installed_skill_dirs_by_name (function)
- _find_installed_skill_dir_by_name (function)
- _read_hub_install_paths (function)
- _index_active_skills (function)
- _recover_renamed_skill (function)

### tools/skills_tool.py (4 gaps)

- _skill_view_fingerprint (function)
- _record_skill_view (function)
- _check_skill_view_dedup (function)
- reset_skill_view_dedup (function)

### tools/terminal_hints.py (8 gaps)

- _hint_gh_unknown_json_field (function)
- _hint_command_not_found (function)
- _hint_module_not_found (function)
- _hint_merge_conflict (function)
- _hint_already_exists (function)
- _hint_gh_rate_limit (function)
- _hint_permission_denied (function)
- annotate_failure (function)

### tools/terminal_tool.py (3 gaps)

- _is_supported_vercel_runtime (function)
- _check_vercel_sandbox_requirements (function)
- _is_safe_workdir_char (function)

### tools/tool_backend_helpers.py (2 gaps)

- _scoped_credential (function)
- resolve_provider_secret (function)

### tools/tool_search.py (7 gaps)

- listing_token_budget (function)
- _short_desc (function)
- _listing_group_label (function)
- build_catalog_listing (function)
- build_catalog_listing_with_form (function)
- _available_source_summary (function)
- validate_deferred_call_args (function)

### tools/transcription_tools.py (19 gaps)

- _resolve_provider_key (function)
- _resolve_stt_language (function)
- _transcode_audio_for_stt (function)
- _is_local_stt_provider (function)
- _command_stt_env_passthrough (function)
- _unregistered_stt_provider_error (function)
- _validate_audio_file_size (function)
- _validate_audio_source_file (function)
- _prepare_audio_for_transcription (function)
- _sysctl_value (function)
- _should_force_faster_whisper_cpu (function)
- build_local_transcribe_kwargs (function)
- _confidence_thresholds (function)
- _is_hallucinated_segment (function)
- _join_confident_segments (function)
- _convert_caf_to_wav (function)
- _transcribe_prepared_audio (function)
- _is_local_or_private_url (function)
- transcribe_audio_local_fallback (function)

### tools/tts_streaming.py (7 gaps)

- _resolve_key (function)
- _try_instantiate (function)
- _openai_config_api_key (function)
- _capped (function)
- XAIStreamer._collect_async (method)
- async XAIStreamer._drain_async (method)
- async XAIStreamer._async_frames (method)

### tools/tts_text_normalize.py (1 gaps)

- _normalize_temperature_ranges (function)

### tools/tts_tool.py (16 gaps)

- _resolve_provider_key (function)
- _elevenlabs_environment_kwargs (function)
- _response_has_explicit_stream (function)
- _close_response (function)
- _read_tts_response_bytes (function)
- _read_tts_response_json (function)
- _write_tts_response_to_file (function)
- _resolve_minimax_tts_runtime (function)
- _command_provider_env_passthrough (function)
- _ffmpeg_transcode_to_opus (function)
- _sniff_audio_container (function)
- _repair_ogg_container (function)
- _tts_cache_get_or_load (function)
- _SyncSentencePipeline.__init__ (method)
- _SyncSentencePipeline._synthesize_to_tmp (method)
- _SyncSentencePipeline._drain (method)

### tools/url_safety.py (1 gaps)

- _resolve_allow_private_urls (function)

### tools/vision_tools.py (1 gaps)

- _load_auxiliary_client (function)

### tools/voice_mode.py (25 gaps)

- _import_numpy (function)
- _sounddevice_output_allowed (function)
- _play_int16_via_tempfile (function)
- _default_input_samplerate (function)
- _get_beep_volume (function)
- _is_nan (function)
- mark_audio_output_active (function)
- is_audio_output_active (function)
- thinking_sound_enabled (function)
- _synth_thinking_blip (function)
- _thinking_sound_loop (function)
- start_thinking_sound (function)
- stop_thinking_sound (function)
- AudioRecorder._max_duration_reached (method)
- _load_voice_stop_phrases (function)
- is_voice_stop_phrase (function)
- voice_stop_hint (function)
- _is_wsl (function)
- _is_wsl2_env (function)
- _wsl_powershell_tts_available (function)
- _play_audio_file_impl (function)
- _voice_debug_enabled (function)
- _vad_log (function)
- full_duplex_listen (function)
- _check_plugin_stt_provider (function)

### tools/working_diff.py (4 gaps)

- _run (function)
- _untracked_files (function)
- _untracked_diff (function)
- collect_working_diff (function)

### tools/xai_http.py (1 gaps)

- hermes_xai_default_headers (function)

### utils.py (1 gaps)

- atomic_write_text (function)
