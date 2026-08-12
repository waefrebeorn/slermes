# Slermes REAL_GAP List — Function Level (live scanner)

> Generated 2026-08-12T04:29:50Z from `live_parity_scan.json` by `make parity-walkway`. **671 REAL_GAP across 124 modules** (of 13,359 total functions).

> This is the forward work plan. Each entry is a Python function not yet ported to C. Close by implementing real C + a single-line `/* PoP: fn @ module.py:fn */` annotation (see slermes-gap-closure skill). Never hand-edit — regenerate via the scanner.

### agent/auxiliary_client.py (9 gaps)

- _AuxProbeClientStub.__getattr__ (method)
- _aux_probe_active (function)
- aux_probe_mode (function)
- _model_recency_key (function)
- _fast_model_from_catalog (function)
- _resolve_auto_route (function)
- _tag_effective_provider (function)
- _effective_provider_for_client (function)
- _record_relay_auxiliary_response_model (function)

### agent/chat_completion_helpers.py (3 gaps)

- _report_stale_nonstream_kill (function)
- _touch_stale_kill_activity (function)
- _resolve_direct_stale_timeout (function)

### agent/context_compressor.py (9 gaps)

- _prune_stale_reasoning_replay (function)
- _is_clarify_non_response_sentinel (function)
- _last_assistant_index (function)
- ContextCompressor._load_proactive_prune_rearm_tokens (method)
- ContextCompressor._clear_durable_proactive_prune_rearm (method)
- ContextCompressor.note_request_rough_estimate (method)
- _handoff_carries_live_user_content (function)
- reference_handoff_would_drive_next_model_call (function)
- is_user_originated_turn (function)

### agent/context_references.py (1 gaps)

- _agent_visible_path (function)

### agent/conversation_loop.py (4 gaps)

- _restore_user_after_reference_handoff (function)
- _should_skip_model_call_for_reference_handoff (function)
- _join_truncated_parts (function)
- _clone_message_for_send (function)

### agent/credential_pool.py (1 gaps)

- get_env_prefer_dotenv (function)

### agent/delegation_context.py (4 gaps)

- non_dispatcher_owned_context (function)
- is_dispatcher_owned_worker_context (function)
- enter_non_dispatcher_owned_context (function)
- exit_non_dispatcher_owned_context (function)

### agent/display.py (1 gaps)

- _browser_exec_step_label (function)

### agent/error_classifier.py (1 gaps)

- _model_id_missing_known_prefix (function)

### agent/estop.py (9 gaps)

- _hermes_home (function)
- sentinel_path (function)
- is_engaged (function)
- engage (function)
- disengage (function)
- get_state (function)
- paused_reply (function)
- check_paused (function)
- _reset_log_state_for_tests (function)

### agent/model_metadata.py (1 gaps)

- _list_providers (function)

### agent/native_compaction.py (6 gaps)

- is_native_compaction_model (function)
- is_direct_openai_route (function)
- resolve_compact_threshold (function)
- native_compaction_context_management (function)
- is_native_compaction_rejection (function)
- merge_interim_reasoning_items (function)

### agent/prompt_builder.py (2 gaps)

- hud_surface_note (function)
- _agents_md_directory_chain (function)

### agent/prompt_cache_boundary.py (3 gaps)

- register_stable_prefix (function)
- find_stable_prefix (function)
- clear_stable_prefixes (function)

### agent/reasoning_summaries.py (1 gaps)

- separate_glued_reasoning_blocks (function)

### agent/redact.py (2 gaps)

- _mask_control_split_tokens (function)
- _command_reads_env_file (function)

### agent/relay_llm.py (1 gaps)

- _response_model_name (function)

### agent/relay_runtime.py (3 gaps)

- managed_callback_guard.__enter__ (method)
- managed_callback_guard.__exit__ (method)
- relay_instrumentation_enabled (function)

### agent/shell_hooks.py (2 gaps)

- _fail_closed_block (function)
- _evaluate_result (function)

### agent/skill_commands.py (1 gaps)

- append_user_instruction (function)

### agent/title_generator.py (8 gaps)

- strip_control_wrappers (function)
- is_titleable_user_message (function)
- derive_title (function)
- _extract_title_text (function)
- _clean_title (function)
- apply_instant_title (function)
- _is_real_user_turn (function)
- _session_is_untitled (function)

### agent/tool_executor.py (2 gaps)

- _authorization_gate_lock_timeout (function)
- _ConcurrentToolAuthorizationGate._human_wait_seconds (method)

### agent/transports/codex.py (3 gaps)

- _cache_scope_from_session_id (function)
- _xai_prefers_native_web_search (function)
- _rename_client_web_search_for_xai (function)

### agent/turn_context.py (2 gaps)

- _maybe_title_session_at_turn_start (function)
- compression_made_progress (function)

### agent/verification_evidence.py (2 gaps)

- record_verify_run (function)
- _insert_evidence (function)

### agent/verification_stop.py (1 gaps)

- _workspace_has_runnable_recipe (function)

### agent/verify/environment.py (4 gaps)

- manifest_path (function)
- load_manifest (function)
- save_manifest (function)
- load_or_detect (function)

### agent/verify/recipes.py (17 gaps)

- Recipe.to_dict (method)
- Recipe.from_dict (method)
- _read_text (function)
- _read_package_json (function)
- detect_package_manager (function)
- _infer_port_from_command (function)
- _dedupe (function)
- _script_runner (function)
- _detect_node_recipe (function)
- _detect_python_recipe (function)
- _detect_go_recipe (function)
- _detect_rust_recipe (function)
- _detect_java_recipe (function)
- _parse_make_targets (function)
- _detect_make_recipe (function)
- _detect_compose_recipe (function)
- detect_recipe (function)

### agent/verify/runner.py (11 gaps)

- PhaseResult.ok (method)
- PhaseResult.to_dict (method)
- ReadinessResult.to_dict (method)
- VerifyResult.ok (method)
- VerifyResult.to_dict (method)
- _tail (function)
- _run_phase_command (function)
- _poll_readiness (function)
- _terminate_process_group (function)
- _run_start_phase (function)
- run_verify (function)

### cli.py (8 gaps)

- _post_stream_transform_output (function)
- HermesCLI._get_status_bar_session_title (method)
- HermesCLI._right_align_status_title (method)
- HermesCLI._right_align_status_title_fragments (method)
- HermesCLI._show_browser_backend_notice (method)
- HermesCLI.finalize_preloaded_skills (method)
- HermesCLI._get_heartbeat_manager (method)
- HermesCLI._start_heartbeat_watchdog (method)

### cron/jobs.py (13 gaps)

- _has_pause_marker (function)
- is_job_runnable (function)
- effective_job_state (function)
- _parse_jobs_file (function)
- _peek_jobs_unlocked (function)
- _jobs_file_stamp (function)
- _record_load_stamp (function)
- _merge_unexpected_disk_jobs (function)
- _validate_job_mode_invariants (function)
- _set_preflight_alerted (function)
- mark_preflight_alerted (function)
- clear_preflight_alerted (function)
- _canonical_skill_ref (function)

### cron/lifecycle_guard.py (5 gaps)

- _mask_data_sink_arguments (function)
- _lifecycle_command_scan_with_data_exemption (function)
- _direct_lifecycle_scan (function)
- _expand_candidate_path (function)
- _sanitize_remote_script_text (function)

### cron/monitor.py (10 gaps)

- hash_monitor_output (function)
- build_monitor_diff (function)
- _snapshot_path (function)
- _read_last_output (function)
- _write_last_output (function)
- _fetch_monitor_url (function)
- _run_monitor_source (function)
- job_has_monitor (function)
- check_monitor (function)
- _persist_monitor_state (function)

### cron/notepad.py (10 gaps)

- _connect (function)
- _initialize_schema (function)
- _transaction (function)
- _validate (function)
- set_note (function)
- get_note (function)
- delete_note (function)
- list_notes (function)
- clear_notepad (function)
- render_notepad_section (function)

### cron/scheduler.py (15 gaps)

- try_register_running_job (function)
- release_running_job (function)
- _cron_inactivity_seconds (function)
- _cwd_lock_timeout_seconds (function)
- _usage_audit_path (function)
- _utcnow_iso_ms (function)
- _write_usage_audit (function)
- _cron_preflight_enabled (function)
- _preflight_check_provider_key (function)
- _preflight_check_delivery (function)
- _preflight_check_skills (function)
- _preflight_job_config (function)
- CronSchedulerRegistrationError.user_message (method)
- CronSchedulerRegistrationError.to_dict (method)
- create_job_with_scheduler_registration (function)

### cron/scheduler_provider.py (1 gaps)

- CronScheduler.register_job (method)

### gateway/config.py (1 gaps)

- _normalize_multiplex_profile_allowlist (function)

### gateway/kanban_watchers.py (1 gaps)

- _kanban_dispatch_allowed (function)

### gateway/platforms/api_server.py (3 gaps)

- ThreadSafeAsyncQueue.put_threadsafe (method)
- _sse_frame (function)
- APIServerAdapter.interrupt_active_runs (method)

### gateway/platforms/base.py (7 gaps)

- _consume_detached_handler_exception (function)
- _parse_docker_volume_mounts (function)
- _default_docker_workspace_host_root (function)
- _docker_persistent_home_host_root (function)
- _cache_dir_container_mounts (function)
- _translate_docker_container_media_path (function)
- async BasePlatformAdapter._bounded_history_media_paths_for_session (method)

### gateway/relay/adapter.py (5 gaps)

- RelayAdapter.prime_routing_cache (method)
- async RelayAdapter.wait_for_auto_thread_info (method)
- RelayAdapter._minted_here (method)
- RelayAdapter._note_prompt_resolved (method)
- async RelayAdapter._notify_prompt_expired (method)

### gateway/relay/ws_transport.py (2 gaps)

- _disconnect_drain_grace_s (function)
- _env_disconnect_budget_s (function)

### gateway/restart_loop_guard.py (2 gaps)

- _chain_gap (function)
- _chain_ending_at (function)

### gateway/run.py (26 gaps)

- _hygiene_cooldown_for_failure (function)
- _reset_hygiene_failure_streak (function)
- hygiene_compaction_recovered (function)
- _strip_response_attachments_for_direct_send (function)
- TurnRunner._attach_session_title_callback (method)
- GatewayRunner._queue_retryable_fatal_platform (method)
- GatewayRunner._interrupt_api_server_runs (method)
- GatewayRunner._busy_modes_from_config (method)
- GatewayRunner._snapshot_profile_busy_modes (method)
- GatewayRunner._busy_profile_name_for_source (method)
- GatewayRunner._effective_busy_input_mode (method)
- GatewayRunner._effective_busy_text_mode (method)
- async GatewayRunner._consume_clean_shutdown_marker (method)
- async GatewayRunner._recover_unclean_sessions (method)
- GatewayRunner._make_profile_busy_session_handler (method)
- async GatewayRunner._handle_pause_command (method)
- async GatewayRunner._mark_durable_active_turn (method)
- async GatewayRunner._clear_durable_active_turn (method)
- async GatewayRunner._get_heartbeat_manager_for_event (method)
- GatewayRunner._register_heartbeat_watch (method)
- GatewayRunner._unregister_heartbeat_watch (method)
- GatewayRunner._start_heartbeat_poller (method)
- async GatewayRunner._deliver_queued_first_response (method)
- async GatewayRunner._await_relay_auto_thread_info (method)
- GatewayRunner._sweep_agent_cache_under_pressure (method)
- GatewayRunner._release_pressure_batch (method)

### gateway/session.py (5 gaps)

- SessionStore.mark_turn_active (method)
- SessionStore.clear_turn_active (method)
- SessionStore.recover_interrupted_turns (method)
- SessionStore.discard_active_turn_markers (method)
- SessionStore._drain_spooled_drops (method)

### gateway/shutdown_flush.py (2 gaps)

- spool_dropped_transcript_message (function)
- drain_transcript_spool (function)

### gateway/slash_commands.py (2 gaps)

- async GatewaySlashCommandsMixin._handle_heartbeat_command (method)
- async GatewaySlashCommandsMixin._handle_refine_command (method)

### gateway/stream_consumer.py (2 gaps)

- GatewayStreamConsumer._append_accumulated (method)
- GatewayStreamConsumer._mark_skip_redundant_finalize (method)

### hermes_cli/agent_plugins.py (15 gaps)

- _inside (function)
- _read_json_object (function)
- _validate_manifest (function)
- _valid_skill_frontmatter (function)
- _discover_skills (function)
- _expand (function)
- _resolve_scoped_path (function)
- _validate_headers (function)
- _validate_remote_url (function)
- _translate_remote (function)
- _translate_stdio (function)
- _discover_mcp (function)
- load_agent_plugin (function)
- read_agent_plugin_manifest (function)
- has_enabled_agent_plugin_mcp (function)

### hermes_cli/approvals_test.py (3 gaps)

- evaluate_command (function)
- _render_text (function)
- approvals_test_command (function)

### hermes_cli/auth.py (6 gaps)

- _LazyHttpx._resolve (method)
- _LazyHttpx.__getattr__ (method)
- _LazyHttpx.__setattr__ (method)
- _LazyHttpx.__delattr__ (method)
- is_actual_local_base_url (function)
- normalize_actual_base_url (function)

### hermes_cli/banner.py (9 gaps)

- _compute_git_banner_state (function)
- prefetch_banner_data (function)
- _format_update_notice (function)
- _defer_update_notice (function)
- _banner_snapshot_path (function)
- banner_snapshot_fingerprint (function)
- load_banner_snapshot (function)
- save_banner_snapshot (function)
- compute_toolset_availability (function)

### hermes_cli/cli_agent_setup_mixin.py (1 gaps)

- CLIAgentSetupMixin._resume_history_limit_error (method)

### hermes_cli/cli_commands_mixin.py (4 gaps)

- CLICommandsMixin._handle_export_command (method)
- CLICommandsMixin._handle_import_command (method)
- CLICommandsMixin._handle_heartbeat_command (method)
- CLICommandsMixin._handle_refine_command (method)

### hermes_cli/config.py (4 gaps)

- _prompt_text (function)
- render_personality_prompt (function)
- resolve_ephemeral_system_prompt_from_config (function)
- _is_ssh_remote_tilde_cwd (function)

### hermes_cli/config_migrations.py (1 gaps)

- _migrate_to_34 (function)

### hermes_cli/cron.py (1 gaps)

- cron_notepad (function)

### hermes_cli/dashboard_procs.py (7 gaps)

- _is_desktop_local_serve_cmdline (function)
- _process_ppid (function)
- _exclude_pids_from_env (function)
- _hermes_home_dir (function)
- _valid_lockfile_payload (function)
- _lock_owned_serve_pids (function)
- _reap_orphaned_desktop_local_serves (function)

### hermes_cli/debug.py (1 gaps)

- _missing_log_note (function)

### hermes_cli/doctor.py (5 gaps)

- _hermes_database_paths (function)
- _read_journal_mode (function)
- _format_db_size (function)
- _report_database_journal_modes (function)
- _render_state_db_stats (function)

### hermes_cli/doctor_live.py (16 gaps)

- _load_config (function)
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

### hermes_cli/gateway.py (2 gaps)

- _wait_for_pid_exit (function)
- _launchctl_label_supervising_process (function)

### hermes_cli/goals.py (7 gaps)

- workspace_fingerprint (function)
- run_gate (function)
- GoalManager.add_gate (method)
- GoalManager.remove_gate (method)
- GoalManager.clear_gates (method)
- GoalManager.render_gates (method)
- GoalManager._check_gates (method)

### hermes_cli/heartbeat.py (21 gaps)

- parse_interval (function)
- format_interval (function)
- HeartbeatState.to_json (method)
- HeartbeatState.from_json (method)
- HeartbeatState.is_due (method)
- HeartbeatState.render_prompt (method)
- _meta_key (function)
- _get_session_db (function)
- load_heartbeat (function)
- save_heartbeat (function)
- HeartbeatManager.__init__ (method)
- HeartbeatManager.state (method)
- HeartbeatManager.has_heartbeat (method)
- HeartbeatManager.is_active (method)
- HeartbeatManager.status_line (method)
- HeartbeatManager.set (method)
- HeartbeatManager.pause (method)
- HeartbeatManager.resume (method)
- HeartbeatManager.clear (method)
- HeartbeatManager.due_prompt (method)
- migrate_heartbeat_to_session (function)

### hermes_cli/hooks.py (1 gaps)

- _print_parsed (function)

### hermes_cli/kanban.py (4 gaps)

- _goal_mode_handoff_rejection (function)
- _cmd_request_review (function)
- _cmd_request_changes (function)
- _cmd_reopen_review (function)

### hermes_cli/kanban_db.py (15 gaps)

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
- review_dispatch_enabled (function)

### hermes_cli/kanban_diagnostics.py (1 gaps)

- _rule_review_dependency_deadlock (function)

### hermes_cli/kanban_swarm.py (2 gaps)

- _activate_root_inline (function)
- _create_swarm_uncommitted (function)

### hermes_cli/linux_desktop_entry.py (10 gaps)

- is_supported (function)
- _xdg_data_home (function)
- desktop_entry_path (function)
- icon_path (function)
- resolve_exec_command (function)
- _quote_exec_arg (function)
- render_desktop_entry (function)
- refresh_desktop_databases (function)
- _run_quiet (function)
- install_desktop_entry (function)

### hermes_cli/main.py (4 gaps)

- cmd_verify (function)
- _register_linux_desktop_entry (function)
- _try_fast_chat_launch (function)
- _advertise_agent_env (function)

### hermes_cli/model_normalize.py (2 gaps)

- _repair_prefix_from_catalogue (function)
- suggest_prefixed_model_id (function)

### hermes_cli/model_switch.py (4 gaps)

- _models_config_is_allowlist (function)
- AmbiguousAliasError.__init__ (method)
- _ambiguous_alias_message (function)
- _scoped_key_env (function)

### hermes_cli/models.py (5 gaps)

- _cached_catalog (function)
- _cache_catalog (function)
- _custom_endpoint_fingerprint (function)
- _cache_entry_valid (function)
- cached_fetch_api_models (function)

### hermes_cli/observability/relay_shared_metrics.py (16 gaps)

- _Runtime.record_client_active (method)
- _Runtime._emit_client_active (method)
- _Runtime.record_model_call_error (method)
- _Runtime.start_tool_call (method)
- _Runtime.record_approval (method)
- _Runtime.record_skill_lifecycle (method)
- _Runtime._tool_call_identity (method)
- _Runtime._tool_call_identities_are_compatible (method)
- _Runtime._event_matches_task_turn (method)
- _Runtime._approval_task (method)
- _Runtime._open_tool_call (method)
- _Runtime._finish_tool_call (method)
- _Runtime._end_pending_tool_calls (method)
- _Runtime._new_model_call_key (method)
- _Runtime._existing_model_call_key (method)
- _with_runtime_toolset (function)

### hermes_cli/observability/shared_metrics.py (6 gaps)

- SharedMetricsStore.record_client_active (method)
- SharedMetricsStore._validate_counter (method)
- SharedMetricsStore._record_counter_in_transaction (method)
- SharedMetricsStore._create_counter_aggregates_table (method)
- SharedMetricsStore._migrate_v1_counter_aggregates (method)
- SharedMetricsStore._parse_state_timestamp (method)

### hermes_cli/observability/shared_metrics_contract.py (22 gaps)

- client_os_family (function)
- client_architecture (function)
- client_install_method (function)
- client_resource (function)
- client_resource_is_valid (function)
- _event_metadata_is_valid (function)
- client_active_counter (function)
- _auxiliary_model_call_dimensions (function)
- tool_call_dimensions (function)
- tool_approval_counter (function)
- skill_counter (function)
- skill_lifecycle_fields (function)
- skill_load_fields (function)
- skill_provenance (function)
- tool_category (function)
- tool_outcome (function)
- tool_approval_outcome (function)
- tool_terminal_fields (function)
- tool_latency_bucket (function)
- tool_retry_bucket (function)
- _non_negative_number (function)
- _metric_identifier (function)

### hermes_cli/personality.py (10 gaps)

- _get (function)
- prompt_text (function)
- render_personality_prompt (function)
- describe_personality (function)
- normalize_personality_name (function)
- available_personalities (function)
- resolve_personality (function)
- active_personality_name (function)
- resolve_ephemeral_system_prompt (function)
- persist_personality (function)

### hermes_cli/plugins.py (16 gaps)

- _portable_skill_namespace (function)
- _display_author (function)
- PluginManager._collect_directory_manifests (method)
- PluginManager.has_enabled_portable_mcp (method)
- PluginManager._load_portable_plugin (method)
- PluginManager.list_plugin_skill_metadata (method)
- PluginManager.get_portable_mcp_servers (method)
- PluginManager.has_portable_mcp_servers (method)
- has_enabled_agent_plugin_mcp (function)
- start_background_plugin_discovery (function)
- _join_background_discovery (function)
- _plugin_toolset_keys_cache_path (function)
- _persist_plugin_toolset_keys (function)
- _read_plugin_keys_cache (function)
- get_plugin_toolset_keys_nowait (function)
- get_portable_mcp_server_names_nowait (function)

### hermes_cli/plugins_cmd.py (2 gaps)

- _is_portable_plugin_dir (function)
- _bundled_default_on (function)

### hermes_cli/profiles.py (3 gaps)

- _make_profile_archive (function)
- _should_redact_export_file (function)
- _scrub_export_secrets (function)

### hermes_cli/resource_limits.py (3 gaps)

- _configured_nofile_soft_limit (function)
- configured_nofile_soft_limit (function)
- apply_nofile_soft_limit (function)

### hermes_cli/sizefmt.py (1 gaps)

- format_bytes (function)

### hermes_cli/status.py (1 gaps)

- _estop_status_line (function)

### hermes_cli/subcommands/pause.py (3 gaps)

- cmd_pause (function)
- cmd_resume (function)
- build_pause_parser (function)

### hermes_cli/subcommands/verify.py (1 gaps)

- build_verify_parser (function)

### hermes_cli/update_cmd.py (7 gaps)

- _reload_config_modules (function)
- _run_config_check_fresh (function)
- _run_migrate_config_fresh (function)
- _print_update_completion (function)
- _orphaned_desktop_backend_pids (function)
- _stop_process_trees (function)
- _refresh_bootstrap_cache_scripts (function)

### hermes_cli/verify_cmd.py (4 gaps)

- run_verify_command (function)
- _merge_project_facts_commands (function)
- _record_evidence (function)
- _print_human_report (function)

### hermes_cli/web_git.py (3 gaps)

- _pr_query (function)
- _pr_payload (function)
- review_pr_list (function)

### hermes_cli/web_routers/git.py (1 gaps)

- async git_pr_list_route (function)

### hermes_cli/web_routers/profiles.py (9 gaps)

- _warn_profile_read_error (function)
- _merge_by_id (function)
- _merge_profile_tree (function)
- get_profiles_projects_tree (function)
- _pr_url_from_tool_output (function)
- post_profiles_sessions_pull_requests (function)
- async export_profile_endpoint (function)
- async import_profile_endpoint (function)
- async get_profile_desktop_overlay (function)

### hermes_cli/web_server.py (9 gaps)

- _get_portal_status_sync (function)
- _terminate_desktop_managed_gateway (function)
- _get_env_vars_sync (function)
- _session_db_read_probe_statements (function)
- _open_session_db_at_path (function)
- _raise_if_cron_registration_error (function)
- _is_serve_orphaned (function)
- _start_parent_death_watchdog (function)
- _demo (function)

### tools/approval.py (6 gaps)

- human_wait_ceiling (function)
- _clamped_window_seconds (function)
- _human_wait_state (function)
- human_wait_window (function)
- human_wait_seconds (function)
- _prompt_dangerous_approval_inner (function)

### tools/async_delegation.py (1 gaps)

- _capture_routing_origin (function)

### tools/browser_tool.py (1 gaps)

- evaluate_url_safety (function)

### tools/browser_use_cli.py (18 gaps)

- _blocked_url_in_code (function)
- _base_subprocess_env (function)
- _read_browser_cfg (function)
- get_browser_backend (function)
- is_legacy_browser_use_cloud_config (function)
- is_browser_use_cli_mode (function)
- default_downgrade_notice (function)
- _managed_bin_dir (function)
- _find_cli (function)
- install_cli (function)
- _workspace_dir (function)
- _find_screenshot (function)
- _native_screenshot_result (function)
- _resolve_backend_cdp (function)
- browser_exec (function)
- _description_header (function)
- _cli_skill_text (function)
- _dynamic_schema_overrides (function)

### tools/cronjob_tools.py (3 gaps)

- _run_claimed_job (function)
- _latest_job_output_excerpt (function)
- _try_dispatch_background_run (function)

### tools/delegate_tool.py (4 gaps)

- _close_subagent_steering (function)
- steer_subagent (function)
- _capture_gateway_steer_authority (function)
- _validate_batch_tasks (function)

### tools/delegation_output_schema.py (5 gaps)

- coerce_output_schema (function)
- append_output_contract (function)
- extract_json_candidate (function)
- validate_output (function)
- build_retry_message (function)

### tools/file_operations.py (9 gaps)

- identify_binary_bytes (function)
- describe_binary_file (function)
- FileOperations.read_file_bytes (method)
- ShellFileOperations._sample_file_bytes (method)
- ShellFileOperations._is_likely_binary_bytes (method)
- ShellFileOperations._size_probe_cmd (method)
- ShellFileOperations._not_regular_error (method)
- ShellFileOperations._unicode_variant_match (method)
- ShellFileOperations.read_file_bytes (method)

### tools/file_tools.py (6 gaps)

- _get_real_hermes_home (function)
- _protected_instruction_config (function)
- _protected_instruction_reason (function)
- _request_protected_instruction_approval (function)
- _check_protected_instruction_write (function)
- _special_file_kind (function)

### tools/image_generation_tool.py (1 gaps)

- _confine_source_images (function)

### tools/image_source.py (2 gaps)

- _ensure_container_env (function)
- async resolve_local_source_to_data_url (function)

### tools/kanban_tools.py (4 gaps)

- _is_dispatcher_owned_worker (function)
- _goal_mode_handoff_rejection (function)
- _handle_request_review (function)
- _handle_request_changes (function)

### tools/mcp_tool.py (11 gaps)

- __getattr__ (function)
- _ensure_mcp_sdk (function)
- _workspace_folder (function)
- _context_var_value (function)
- _resolve_identity_header (function)
- _apply_identity_header (function)
- _make_redirect_header_stripper (function)
- _normalize_server_trust (function)
- _annotation_read_only_hint (function)
- _record_tool_trust_metadata (function)
- _trust_gate_check (function)

### tools/patch_parser.py (1 gaps)

- _write_file_accepts_pre_content (function)

### tools/process_registry.py (5 gaps)

- _worker_memory_max_bytes (function)
- _systemd_run_user_scope_available (function)
- _is_supervised_gateway_process (function)
- _build_systemd_scope_argv (function)
- _stop_systemd_unit (function)

### tools/read_extract.py (16 gaps)

- _anydoc (function)
- extract_document_bytes (function)
- _extract_anydoc (function)
- _pdf_page_texts (function)
- _pdf_page_char_counts (function)
- _page_ranges (function)
- _group_ranges (function)
- _gap_map (function)
- _pdf_coverage_note (function)
- _extract_anydoc_bytes (function)
- _pdf_coverage_note_from_bytes (function)
- _human_size (function)
- _base64_bytes (function)
- _clean_stream_text (function)
- _notebook_output_text (function)
- _notebook_outputs (function)

### tools/read_preview_tool.py (1 gaps)

- read_preview_tool (function)

### tools/read_window_tool.py (1 gaps)

- read_window_below_tool (function)

### tools/registry.py (2 gaps)

- _bound_error_text (function)
- _bound_json_error_result (function)

### tools/schema_sanitizer.py (2 gaps)

- _const_branch_type (function)
- collapse_const_unions (function)

### tools/skill_linter.py (17 gaps)

- LintFinding.format (method)
- _check_name_matches_dir (function)
- _check_name_format (function)
- _check_description (function)
- _check_metadata_block (function)
- _check_shell_utilities (function)
- _check_sections (function)
- _check_reference_links (function)
- _check_platforms_gating (function)
- _check_forbidden_files (function)
- _check_platform_list_valid (function)
- _strip_code_blocks (function)
- lint_content (function)
- lint_skill (function)
- format_findings (function)
- has_errors (function)
- _main (function)

### tools/skill_manager_tool.py (1 gaps)

- _attach_lint_findings (function)

### tools/skill_usage.py (5 gaps)

- _non_negative_int (function)
- telemetry_provenance (function)
- _emit_skill_lifecycle (function)
- record_created (function)
- record_installed (function)

### tools/skills_hub.py (2 gaps)

- OptionalSkillSource._fetch_from_live_repo (method)
- OptionalSkillSource._list_remote_skill_dirs (method)

### tools/skills_tool.py (1 gaps)

- _plugin_skill_linked_files (function)

### tools/terminal_tool.py (10 gaps)

- _redact_terminal_error_text (function)
- register_container_alias (function)
- _resolve_container_alias (function)
- _docker_session_isolation_enabled (function)
- _has_isolation_overrides (function)
- _resolve_task_host_cwd (function)
- _ssh_config_from_config (function)
- _container_config_from_config (function)
- ensure_task_env (function)
- _evict_environment_for_task (function)

### tools/transcription_tools.py (11 gaps)

- _safe_find_spec (function)
- _run_ffmpeg_stt_encode (function)
- _get_idle_unload_seconds (function)
- _unload_local_model (function)
- _start_idle_unload_watcher (function)
- _touch_transcription_time (function)
- _find_ffprobe_binary (function)
- _probe_audio_duration (function)
- _cloud_trim_settings (function)
- _trim_silence_for_cloud_stt (function)
- _dispatch_stt_provider (function)

### tools/tts_tool.py (7 gaps)

- AudioDeliveryProfile.target_file_bytes (method)
- _resolve_audio_delivery_profile (function)
- _split_oversized_sentence (function)
- _pack_audio_files_for_delivery (function)
- _concat_audio_files (function)
- _build_audio_delivery_files (function)
- _text_to_speech_single (function)

### tools/vision_tools.py (6 gaps)

- async _stream_download_to_file (function)
- _crop_image_region (function)
- _build_scale_note (function)
- _terminal_backend_is_local (function)
- _is_path_like_video_source (function)
- async _materialize_video_from_terminal_backend (function)

### tools/voice_mode.py (2 gaps)

- _normalize_for_echo_compare (function)
- is_tts_echo (function)

### tools/wake_word.py (7 gaps)

- resolve_capture_mode (function)
- _local_input_device_ready (function)
- _capture_sample_rate (function)
- _resample_audio_frame (function)
- WakeWordDetector.feed (method)
- feed_audio (function)
- detector_frame_info (function)

### utils.py (1 gaps)

- atomic_roundtrip_yaml_save (function)
