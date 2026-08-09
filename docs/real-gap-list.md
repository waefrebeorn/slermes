# Slermes REAL_GAP List — Function Level (live scanner)

> Generated 2026-08-09T06:16:13Z from `live_parity_scan.json` by `make parity-walkway`. **742 REAL_GAP across 156 modules** (of 14,045 total functions).

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

### agent/auxiliary_client.py (2 gaps)

- _call_llm_impl (function)
- async _async_call_llm_impl (function)

### agent/chat_completion_helpers.py (3 gaps)

- _context_thread_target (function)
- _merge_nous_portal_messages_extra_body (function)
- _estimate_chunk_bytes (function)

### agent/coding_context.py (2 gaps)

- is_coding_context (function)
- project_facts_for (function)

### agent/context_breakdown.py (1 gaps)

- compute_context_details (function)

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

### agent/display.py (1 gaps)

- prepare_tool_preview (function)

### agent/insights.py (1 gaps)

- InsightsEngine.get_usage_breakdown (method)

### agent/lsp/manager.py (4 gaps)

- async LSPService._start_idle_reaper (method)
- LSPService._touch (method)
- async LSPService._idle_reaper_loop (method)
- async LSPService._reap_idle_once (method)

### agent/memory_provider.py (1 gaps)

- is_trivial_prompt (function)

### agent/moa_loop.py (2 gaps)

- _completed_response_as_stream_chunk (function)
- peel_reference_guidance (function)

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

### agent/monitoring/otlp_exporter.py (10 gaps)

- _require_sdk (function)
- build_exporter (function)
- _resource_attributes (function)
- _make_provider (function)
- export_batch (function)
- OTLPStreamer.__init__ (method)
- OTLPStreamer.__call__ (method)
- OTLPStreamer.shutdown (method)
- is_available (function)
- start_streaming (function)

### agent/outbound_webhooks.py (1 gaps)

- _NoRedirectHandler.redirect_request (method)

### agent/prompt_caching.py (6 gaps)

- PromptCachePlan.marker_count (method)
- strip_anthropic_cache_control (function)
- strip_anthropic_tool_cache_control (function)
- _count_cache_markers (function)
- _completed_transaction_endpoint_indexes (function)
- build_prompt_cache_plan (function)

### agent/proxy_sources/iron_proxy.py (1 gaps)

- _pid_alive (function)

### agent/relay_tools.py (4 gaps)

- execute (function)
- _jsonable (function)
- _json_equal (function)
- _run_awaitable (function)

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

### gateway/platforms/bluebubbles.py (1 gaps)

- _get_scoped_secret (function)

### gateway/platforms/helpers.py (1 gaps)

- compile_mention_patterns (function)

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

### gateway/session_context.py (1 gaps)

- scoped_current_session_id (function)

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

### gateway/streaming_tts_consumer.py (2 gaps)

- async StreamingTTSConsumer._iter_stream_chunks (method)
- StreamingTTSConsumer._next_stream_chunk (method)

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

### hermes_cli/active_sessions.py (3 gaps)

- format_age (function)
- summarize_holders (function)
- release_orphaned_leases (function)

### hermes_cli/approvals_suggest.py (9 gaps)

- default_db_path (function)
- _connect_readonly (function)
- _iter_terminal_calls (function)
- _blocked_tool_call_ids (function)
- scan_approval_history (function)
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

### hermes_cli/config.py (8 gaps)

- _raw_config_has_explicit_version (function)
- read_user_config_raw (function)
- read_raw_config_readonly (function)
- _cron_model_drift_axis_for_config_key (function)
- cron_model_drift_guard_enabled (function)
- _cron_fleet_default_covers_axis (function)
- _load_cron_jobs_for_config_warning (function)
- warn_unpinned_cron_jobs_after_model_config_change (function)

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

### hermes_cli/observability/__init__.py (3 gaps)

- observe_lifecycle (function)
- handles_hook (function)
- _safe_observe (function)

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

### hermes_cli/voice.py (3 gaps)

- set_voice_busy_probe (function)
- _voice_activity_held (function)
- _speak_text_streaming (function)

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

### tools/computer_use/cua_backend.py (14 gaps)

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

### tools/environments/base.py (8 gaps)

- _BoundedOutputCollector._maybe_spill (method)
- _BoundedOutputCollector.close_spill (method)
- get_activity_callback (function)
- _export_dump_excluding_session_vars (function)
- BaseEnvironment._additional_profile_scoped_passthrough_names (method)
- BaseEnvironment._snapshot_excluded_passthrough_names (method)
- BaseEnvironment._finalize_wait_result (method)
- BaseEnvironment._kill_process (method)

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

### tools/environments/local.py (3 gaps)

- build_subprocess_env (function)
- _managed_runtime_path_entries (function)
- LocalEnvironment._kill_process (method)

### tools/file_operations.py (2 gaps)

- ShellFileOperations._try_multi_path_search (method)
- ShellFileOperations._zero_match_probe (method)

### tools/file_tools.py (4 gaps)

- _file_ops_uses_host_paths (function)
- _rewrite_v4a_patch_paths_for_host (function)
- _check_not_found_cache (function)
- _record_not_found (function)

### tools/fuzzy_match.py (3 gaps)

- is_already_applied (function)
- _format_match_locations (function)
- _visualize_whitespace (function)

### tools/image_source.py (2 gaps)

- _get_active_env (function)
- _detect_video_mime (function)

### tools/kanban_tools.py (1 gaps)

- inject_new_comments_from_env (function)

### tools/lazy_deps.py (1 gaps)

- install_specs (function)

### tools/managed_tool_gateway.py (2 gaps)

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

### tools/process_registry.py (32 gaps)

- ProcessRegistry._emit_output (method)
- ProcessRegistry._global_watch_admit (method)
- ProcessRegistry._is_host_pid_alive (method)
- ProcessRegistry._safe_host_start_time (method)
- ProcessRegistry._host_pid_is_ours (method)
- ProcessRegistry._refresh_detached_session (method)
- ProcessRegistry._proc_alive (method)
- ProcessRegistry._daemon_term_grace_seconds (method)
- ProcessRegistry._terminate_host_pid (method)
- ProcessRegistry._env_temp_dir (method)
- ProcessRegistry.spawn_via_env (method)
- ProcessRegistry._reader_loop (method)
- ProcessRegistry._env_poller_loop (method)
- ProcessRegistry._pty_reader_loop (method)
- ProcessRegistry.is_session_waiting (method)
- ProcessRegistry._drain_should_skip (method)
- ProcessRegistry.read_log (method)
- ProcessRegistry.kill_process (method)
- ProcessRegistry.write_stdin (method)
- ProcessRegistry.submit_stdin (method)
- ProcessRegistry.close_stdin (method)
- ProcessRegistry.list_sessions (method)
- ProcessRegistry.has_active_processes (method)
- ProcessRegistry.has_any_active (method)
- ProcessRegistry.snapshot_running_ids (method)
- ProcessRegistry.kill_started_since (method)
- ProcessRegistry._prune_if_needed (method)
- _format_age (function)
- _format_async_delegation (function)
- format_process_notification (function)
- _redact_process_result (function)
- _handle_process (function)

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

### tools/tts_streaming.py (7 gaps)

- _resolve_key (function)
- register (function)
- _openai_config_api_key (function)
- _capped (function)
- XAIStreamer._collect_async (method)
- async XAIStreamer._drain_async (method)
- async XAIStreamer._async_frames (method)

### tools/vision_tools.py (1 gaps)

- _load_auxiliary_client (function)

### tools/voice_mode.py (10 gaps)

- _import_numpy (function)
- _play_int16_via_tempfile (function)
- _default_input_samplerate (function)
- _synth_thinking_blip (function)
- _thinking_sound_loop (function)
- start_thinking_sound (function)
- stop_thinking_sound (function)
- _play_audio_file_impl (function)
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
