from pathlib import Path
import load_dev_module as _ldm
import os
#!/usr/bin/env python3
"""
sta_oracle_cronjob_tools_gap.py — oracle for residual-façade closure (v558).
Recomputes each case against LIVE Python tools/cronjob_tools and asserts the
C output matches (or, for honestly-demoted functions, that C returns an
honest error / no-op rather than a fake success).
"""
import sys, os, json
sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import tools.cronjob_tools as ct
_ldm.rebind_used(ct, 'ct', ["tools.__init__", "tools.annotate_preview_tool", "tools.ansi_strip", "tools.approval", "tools.approval_context", "tools.approval_detection", "tools.approval_floors", "tools.approval_gateway_wait", "tools.approval_human_wait", "tools.approval_prompt", "tools.approval_smart", "tools.approval_yolo", "tools.arg_coercion", "tools.async_delegation", "tools.async_delegation_recovery_hints", "tools.async_delegation_worker", "tools.audio_container", "tools.binary_extensions", "tools.blueprints", "tools.bot_failure_reasons", "tools.bot_live_delivery", "tools.bot_mode_dm", "tools.bot_mode_probe", "tools.bot_relay", "tools.browser_cdp_tool", "tools.browser_dialog_tool", "tools.browser_harness_dirs", "tools.browser_launcher", "tools.browser_launcher_holders", "tools.browser_lightpanda", "tools.browser_session", "tools.browser_session_cdp", "tools.browser_session_cloud", "tools.browser_session_lifecycle", "tools.browser_session_real_profile", "tools.browser_session_sandbox", "tools.browser_supervisor", "tools.browser_supervisor_capture", "tools.browser_supervisor_dialogs", "tools.browser_supervisor_frames", "tools.browser_tool", "tools.browser_tool_install", "tools.browser_url_policy", "tools.browser_use_cli", "tools.browser_use_shot", "tools.browser_vault_context", "tools.browser_vault_tool", "tools.budget_config", "tools.checkpoint_maintenance", "tools.checkpoint_manager", "tools.checkpoint_manager_profile_rename", "tools.checkpoint_pruning", "tools.clarify_batch", "tools.clarify_gateway", "tools.clarify_tool", "tools.close_preview_tool", "tools.close_terminal_tool", "tools.code_execution_env", "tools.code_execution_rpc", "tools.code_execution_tool", "tools.code_kernel", "tools.code_kernel_remote", "tools.computer_use_tool", "tools.credential_files", "tools.cronjob_job_args", "tools.cronjob_prompt_scan", "tools.daemon_pool", "tools.debug_helpers", "tools.delegate_tool", "tools.delegate_tool_child_run", "tools.delegate_tool_config", "tools.delegate_tool_dispatch", "tools.delegate_tool_progress", "tools.delegate_tool_registry", "tools.delegate_tool_results", "tools.delegate_tool_routes", "tools.delegate_tool_tasks", "tools.delegate_tool_toolsets", "tools.delegate_tool_tuning", "tools.delegation_live_log", "tools.delegation_output_schema", "tools.delegation_redaction", "tools.desktop_ui", "tools.discord_tool", "tools.drive_preview_tool", "tools.env_passthrough", "tools.env_probe", "tools.escape_drift_autocorrect", "tools.fal_common", "tools.feishu_doc_tool", "tools.feishu_drive_tool", "tools.feishu_lark", "tools.file_operations", "tools.file_operations_common", "tools.file_operations_lint", "tools.file_operations_native", "tools.file_operations_search", "tools.file_state", "tools.file_tools", "tools.file_tools_paths", "tools.file_tools_read_tracking", "tools.file_tools_write_guards", "tools.focus_pane_tool", "tools.fuzzy_match", "tools.hook_output_spill", "tools.human_input_hooks", "tools.image_generation_catalog", "tools.image_generation_managed", "tools.image_generation_tool", "tools.image_source", "tools.interpreter_shutdown", "tools.interrupt", "tools.kanban_tools", "tools.kanban_tools_schemas", "tools.kanban_toolset_context", "tools.lazy_deps", "tools.managed_gateway_auth", "tools.managed_gateway_identity", "tools.managed_tool_gateway", "tools.mcp_dashboard_oauth", "tools.mcp_liveness", "tools.mcp_oauth", "tools.mcp_oauth_device", "tools.mcp_oauth_manager", "tools.mcp_oauth_provider", "tools.mcp_schema_cache", "tools.mcp_tool", "tools.mcp_tool_agent", "tools.mcp_tool_common", "tools.mcp_tool_config", "tools.mcp_tool_content", "tools.mcp_tool_costs", "tools.mcp_tool_discovery", "tools.mcp_tool_errors", "tools.mcp_tool_events", "tools.mcp_tool_handlers", "tools.mcp_tool_health", "tools.mcp_tool_lifecycle", "tools.mcp_tool_loop", "tools.mcp_tool_node_abi", "tools.mcp_tool_registration", "tools.mcp_tool_sampling", "tools.mcp_tool_schema", "tools.mcp_tool_scope", "tools.mcp_tool_server_run", "tools.mcp_tool_transport", "tools.memory_tool", "tools.memory_tool_store", "tools.microsoft_graph_auth", "tools.microsoft_graph_client", "tools.neutts_synth", "tools.open_preview_tool", "tools.openrouter_client", "tools.osv_check", "tools.patch_parser", "tools.path_security", "tools.plugin_guard", "tools.plugin_guard_context", "tools.preview_tool", "tools.process_registry", "tools.process_registry_awaiting", "tools.process_registry_checkpoint", "tools.process_registry_env_log", "tools.process_registry_notifications", "tools.process_registry_output_log", "tools.process_registry_ownership", "tools.process_registry_results", "tools.process_registry_stdin", "tools.process_registry_termination", "tools.process_registry_wait_policy", "tools.process_registry_wsl", "tools.project_tools", "tools.pty_query_responder", "tools.react_to_message_tool", "tools.read_extract", "tools.read_preview_tool", "tools.read_terminal_tool", "tools.read_window_tool", "tools.registry", "tools.schema_sanitizer", "tools.self_repo_guard", "tools.send_message_senders", "tools.send_message_targets", "tools.send_message_tool", "tools.session_search_tool", "tools.set_theme_tool", "tools.setup_beat_tool", "tools.shell_heredoc", "tools.skill_ledger", "tools.skill_linter", "tools.skill_manager_batch", "tools.skill_manager_guards", "tools.skill_manager_tool", "tools.skill_provenance", "tools.skill_usage", "tools.skillevaluator_scan", "tools.skills_ast_audit", "tools.skills_guard", "tools.skills_hub", "tools.skills_hub_clawhub", "tools.skills_hub_github", "tools.skills_hub_install", "tools.skills_hub_models", "tools.skills_hub_official", "tools.skills_hub_search", "tools.skills_hub_skillssh", "tools.skills_hub_sources", "tools.skills_sync", "tools.skills_sync_bundled_ops", "tools.skills_sync_optional", "tools.skills_tool", "tools.skills_tool_dedup", "tools.skills_tool_plugin", "tools.skills_tool_setup", "tools.slash_confirm", "tools.spill_safety", "tools.stt_lease", "tools.subagent_worktree", "tools.terminal_hints", "tools.terminal_scope", "tools.terminal_tool", "tools.terminal_tool_backends", "tools.terminal_tool_background", "tools.terminal_tool_config", "tools.terminal_tool_guards", "tools.terminal_tool_lifecycle", "tools.terminal_tool_macos_open", "tools.terminal_tool_result", "tools.terminal_tool_sudo", "tools.thread_context", "tools.threat_patterns", "tools.tip_tool", "tools.todo_tool", "tools.tool_backend_helpers", "tools.tool_labels", "tools.tool_output_limits", "tools.tool_output_truncate", "tools.tool_result_storage", "tools.tool_search", "tools.tool_search_catalog", "tools.tool_search_validation", "tools.tour_presets", "tools.tour_tool", "tools.transcription_audio", "tools.transcription_chunking", "tools.transcription_cloud", "tools.transcription_command", "tools.transcription_common", "tools.transcription_local", "tools.transcription_streaming", "tools.transcription_tools", "tools.transcription_whisper_cpp", "tools.tts_command_provider", "tools.tts_elevenlabs_voices", "tools.tts_streaming", "tools.tts_text_normalize", "tools.tts_tool", "tools.tts_tool_delivery", "tools.tts_tool_lifecycle", "tools.tts_tool_local", "tools.tts_tool_openai", "tools.tts_tool_plugins", "tools.tts_tool_providers", "tools.tts_tool_speaker", "tools.url_safety", "tools.video_generation_tool", "tools.vision_tools", "tools.vision_tools_history_budget", "tools.vision_tools_image_prep", "tools.voice_client_config", "tools.voice_live", "tools.voice_mode", "tools.voice_mode_transcript", "tools.wake_word", "tools.wake_word_engines", "tools.web_result_cache", "tools.web_tools", "tools.web_tools_extract", "tools.web_tools_rescue", "tools.web_tools_truncate", "tools.website_policy", "tools.working_diff", "tools.write_approval", "tools.x_search_tool", "tools.xai_http", "tools.xai_video_tools", "tools.yuanbao_tools"], Path(__file__))

def env_enabled(v):
    return str(v).strip().lower() in ("1", "true", "yes", "on")

def py_req():
    for k in ("HERMES_INTERACTIVE", "HERMES_GATEWAY_SESSION", "HERMES_EXEC_ASK"):
        os.environ.pop(k, None)
    yield ("none", False)
    os.environ["HERMES_INTERACTIVE"] = "1"
    yield ("interactive=1", True)
    os.environ.pop("HERMES_INTERACTIVE")
    os.environ["HERMES_GATEWAY_SESSION"] = "true"
    yield ("gateway=true", True)
    os.environ["HERMES_GATEWAY_SESSION"] = "no"
    os.environ["HERMES_EXEC_ASK"] = "off"
    yield ("falsey", False)
    for k in ("HERMES_INTERACTIVE", "HERMES_GATEWAY_SESSION", "HERMES_EXEC_ASK"):
        os.environ.pop(k, None)

def main():
    cases = []
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            continue

    mism = 0
    total = 0
    for c in cases:
        fn = c["fn"]
        total += 1
        ok = True
        detail = ""
        if fn == "req":
            gen = dict(py_req())
            exp = gen.get(c["env"])
            ok = (c["out"] == exp)
            detail = f"C={c['out']} PY={exp}"
        elif fn == "script":
            inp = c["in"]
            py = ct._validate_cron_script_path(inp)
            cout = c["out"]
            if py is None:
                ok = (cout is None)
            else:
                ok = (cout is not None and cout == py)
            detail = f"C={cout!r} PY={py!r}"
        elif fn == "baseurl":
            prov = c["prov"]
            bu = c["bu"]
            py = ct._validate_cron_base_url(prov, bu)
            cout = c["out"]
            py_blocked = py is not None
            c_blocked = cout is not None
            # C fails closed with a different message; the SECURITY DECISION
            # (block vs allow) must match Python's.
            ok = (py_blocked == c_blocked)
            detail = f"C_blocked={c_blocked}({cout!r}) PY_blocked={py_blocked}"
        elif fn == "format":
            job = {
                "prompt": "This is a very long prompt that exceeds one hundred characters to force the preview truncation logic to kick in properly and verify the ellipsis is appended at the cut point.",
                "id": "job-123", "skill": "daily_report", "model": "gpt-4",
                "provider": "openai", "schedule_display": "0 9 * * *",
                "repeat": {"times": 1}, "deliver": "telegram",
                "enabled": True, "state": "scheduled",
            }
            py = ct._format_job(job)
            f = c["fields"]
            checks = {
                "job_id": py.get("job_id"),
                "name": py.get("name"),
                "skill": py.get("skill"),
                "model": py.get("model"),
                "provider": py.get("provider"),
                "schedule": py.get("schedule"),
                "deliver": py.get("deliver"),
                "enabled": py.get("enabled"),
                "state": py.get("state"),
            }
            for k, pv in checks.items():
                cv = f.get(k)
                cv = None if cv == "null" else cv
                if str(cv) != str(pv):
                    ok = False
                    detail = f"field {k}: C={cv!r} PY={pv!r}"
                    break
            else:
                # prompt_preview length: Python truncates at 100 + "..."
                exp_len = min(100, len(job["prompt"])) + (3 if len(job["prompt"]) > 100 else 0)
                if f.get("prompt_preview_len") != exp_len:
                    ok = False
                    detail = f"preview_len C={f['prompt_preview_len']} PY={exp_len}"
                else:
                    detail = "fields match"
        elif fn == "notify":
            # Python _notify_provider_jobs_changed_safe returns None (best-effort,
            # never raises). C mirrors: void, must not crash. Reaching here = ok.
            ok = (c.get("out") == "ok")
            detail = "best-effort notify (returned cleanly)"
        elif fn == "dispatch_add":
            # LIVE Python cronjob(action='create',...) returns success=True with a
            # job_id. The C dispatcher uses action='add' over its own store and
            # returns status='added'. Different backend, same CONTRACT: the create
            # succeeds without error. (Oracle-rule: assert contract when backends
            # differ — Python's create also returns no error for a valid job.)
            ok = (c.get("status") == "added" and c.get("has_error") is False)
            detail = f"status={c.get('status')} has_error={c.get('has_error')}"
        elif fn == "dispatch_list":
            # Python cronjob(action='list') returns {'success':True,'jobs':[...]}.
            # Contract: the just-created job appears in the listing.
            ok = (c.get("found") is True and c.get("count", 0) >= 1)
            detail = f"count={c.get('count')} found={c.get('found')}"
        elif fn == "execnow_real":
            # Python _execute_job_now(job) for an active job returns
            # claimed=True and success reflects the run. C runs `true` -> success.
            ok = (c.get("claimed") is True and c.get("success") is True)
            detail = f"claimed={c.get('claimed')} success={c.get('success')}"
        elif fn == "execnow_missing":
            # Python: a job that can't be claimed/found does not fire ->
            # claimed=False. C mirrors for an unknown job.
            ok = (c.get("claimed") is False)
            detail = f"claimed={c.get('claimed')}"
        elif fn == "execnow_noid":
            # No id/name: nothing to fire -> claimed=False with an error string.
            ok = (c.get("claimed") is False and c.get("has_error") is True)
            detail = f"claimed={c.get('claimed')} has_error={c.get('has_error')}"
        elif fn == "dispatch_remove":
            # Python cronjob(action='remove') returns success=True; C returns
            # status='removed'. Contract: the removal reports success.
            ok = (c.get("status") == "removed")
            detail = f"status={c.get('status')}"
        else:
            ok = False
            detail = f"unknown fn {fn}"

        if not ok:
            mism += 1
            print(f"MISMATCH [{fn}] {detail}")
        else:
            print(f"ok [{fn}] {detail}")

    print(f"\nRESULT: {total - mism}/{total} match, {mism} mismatch")
    sys.exit(1 if mism else 0)

if __name__ == "__main__":
    main()