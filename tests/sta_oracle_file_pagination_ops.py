from pathlib import Path
import load_dev_module as _ldm
import os
#!/usr/bin/env python3
"""Oracle for v554 file_pagination_ops extraction: C == LIVE Python.

Reads JSON lines from tests/t_port_file_pagination_ops.c and asserts C output
equals what LIVE tools/file_operations.py produces.

  gcc -O2 -g -I include -I src/tools -I src/agent -I lib/libjson \
      tests/t_port_file_pagination_ops.c src/tools/file_pagination_ops.o \
      lib/libjson/json.o -o /tmp/t_pg
  /tmp/t_pg | python3 tests/sta_oracle_file_pagination_ops.py
"""
import json, sys, os, re
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, REPO)
import tools.file_operations as fo
_ldm.rebind_used(fo, 'fo', ["tools.__init__", "tools.annotate_preview_tool", "tools.ansi_strip", "tools.approval", "tools.approval_context", "tools.approval_detection", "tools.approval_floors", "tools.approval_gateway_wait", "tools.approval_human_wait", "tools.approval_prompt", "tools.approval_smart", "tools.approval_yolo", "tools.arg_coercion", "tools.async_delegation", "tools.async_delegation_recovery_hints", "tools.async_delegation_worker", "tools.audio_container", "tools.binary_extensions", "tools.blueprints", "tools.bot_failure_reasons", "tools.bot_live_delivery", "tools.bot_mode_dm", "tools.bot_mode_probe", "tools.bot_relay", "tools.browser_cdp_tool", "tools.browser_dialog_tool", "tools.browser_harness_dirs", "tools.browser_launcher", "tools.browser_launcher_holders", "tools.browser_lightpanda", "tools.browser_session", "tools.browser_session_cdp", "tools.browser_session_cloud", "tools.browser_session_lifecycle", "tools.browser_session_real_profile", "tools.browser_session_sandbox", "tools.browser_supervisor", "tools.browser_supervisor_capture", "tools.browser_supervisor_dialogs", "tools.browser_supervisor_frames", "tools.browser_tool", "tools.browser_tool_install", "tools.browser_url_policy", "tools.browser_use_cli", "tools.browser_use_shot", "tools.browser_vault_context", "tools.browser_vault_tool", "tools.budget_config", "tools.checkpoint_maintenance", "tools.checkpoint_manager", "tools.checkpoint_manager_profile_rename", "tools.checkpoint_pruning", "tools.clarify_batch", "tools.clarify_gateway", "tools.clarify_tool", "tools.close_preview_tool", "tools.close_terminal_tool", "tools.code_execution_env", "tools.code_execution_rpc", "tools.code_execution_tool", "tools.code_kernel", "tools.code_kernel_remote", "tools.computer_use_tool", "tools.credential_files", "tools.cronjob_job_args", "tools.cronjob_prompt_scan", "tools.cronjob_tools", "tools.daemon_pool", "tools.debug_helpers", "tools.delegate_tool", "tools.delegate_tool_child_run", "tools.delegate_tool_config", "tools.delegate_tool_dispatch", "tools.delegate_tool_progress", "tools.delegate_tool_registry", "tools.delegate_tool_results", "tools.delegate_tool_routes", "tools.delegate_tool_tasks", "tools.delegate_tool_toolsets", "tools.delegate_tool_tuning", "tools.delegation_live_log", "tools.delegation_output_schema", "tools.delegation_redaction", "tools.desktop_ui", "tools.discord_tool", "tools.drive_preview_tool", "tools.env_passthrough", "tools.env_probe", "tools.escape_drift_autocorrect", "tools.fal_common", "tools.feishu_doc_tool", "tools.feishu_drive_tool", "tools.feishu_lark", "tools.file_operations_common", "tools.file_operations_lint", "tools.file_operations_native", "tools.file_operations_search", "tools.file_state", "tools.file_tools", "tools.file_tools_paths", "tools.file_tools_read_tracking", "tools.file_tools_write_guards", "tools.focus_pane_tool", "tools.fuzzy_match", "tools.hook_output_spill", "tools.human_input_hooks", "tools.image_generation_catalog", "tools.image_generation_managed", "tools.image_generation_tool", "tools.image_source", "tools.interpreter_shutdown", "tools.interrupt", "tools.kanban_tools", "tools.kanban_tools_schemas", "tools.kanban_toolset_context", "tools.lazy_deps", "tools.managed_gateway_auth", "tools.managed_gateway_identity", "tools.managed_tool_gateway", "tools.mcp_dashboard_oauth", "tools.mcp_liveness", "tools.mcp_oauth", "tools.mcp_oauth_device", "tools.mcp_oauth_manager", "tools.mcp_oauth_provider", "tools.mcp_schema_cache", "tools.mcp_tool", "tools.mcp_tool_agent", "tools.mcp_tool_common", "tools.mcp_tool_config", "tools.mcp_tool_content", "tools.mcp_tool_costs", "tools.mcp_tool_discovery", "tools.mcp_tool_errors", "tools.mcp_tool_events", "tools.mcp_tool_handlers", "tools.mcp_tool_health", "tools.mcp_tool_lifecycle", "tools.mcp_tool_loop", "tools.mcp_tool_node_abi", "tools.mcp_tool_registration", "tools.mcp_tool_sampling", "tools.mcp_tool_schema", "tools.mcp_tool_scope", "tools.mcp_tool_server_run", "tools.mcp_tool_transport", "tools.memory_tool", "tools.memory_tool_store", "tools.microsoft_graph_auth", "tools.microsoft_graph_client", "tools.neutts_synth", "tools.open_preview_tool", "tools.openrouter_client", "tools.osv_check", "tools.patch_parser", "tools.path_security", "tools.plugin_guard", "tools.plugin_guard_context", "tools.preview_tool", "tools.process_registry", "tools.process_registry_awaiting", "tools.process_registry_checkpoint", "tools.process_registry_env_log", "tools.process_registry_notifications", "tools.process_registry_output_log", "tools.process_registry_ownership", "tools.process_registry_results", "tools.process_registry_stdin", "tools.process_registry_termination", "tools.process_registry_wait_policy", "tools.process_registry_wsl", "tools.project_tools", "tools.pty_query_responder", "tools.react_to_message_tool", "tools.read_extract", "tools.read_preview_tool", "tools.read_terminal_tool", "tools.read_window_tool", "tools.registry", "tools.schema_sanitizer", "tools.self_repo_guard", "tools.send_message_senders", "tools.send_message_targets", "tools.send_message_tool", "tools.session_search_tool", "tools.set_theme_tool", "tools.setup_beat_tool", "tools.shell_heredoc", "tools.skill_ledger", "tools.skill_linter", "tools.skill_manager_batch", "tools.skill_manager_guards", "tools.skill_manager_tool", "tools.skill_provenance", "tools.skill_usage", "tools.skillevaluator_scan", "tools.skills_ast_audit", "tools.skills_guard", "tools.skills_hub", "tools.skills_hub_clawhub", "tools.skills_hub_github", "tools.skills_hub_install", "tools.skills_hub_models", "tools.skills_hub_official", "tools.skills_hub_search", "tools.skills_hub_skillssh", "tools.skills_hub_sources", "tools.skills_sync", "tools.skills_sync_bundled_ops", "tools.skills_sync_optional", "tools.skills_tool", "tools.skills_tool_dedup", "tools.skills_tool_plugin", "tools.skills_tool_setup", "tools.slash_confirm", "tools.spill_safety", "tools.stt_lease", "tools.subagent_worktree", "tools.terminal_hints", "tools.terminal_scope", "tools.terminal_tool", "tools.terminal_tool_backends", "tools.terminal_tool_background", "tools.terminal_tool_config", "tools.terminal_tool_guards", "tools.terminal_tool_lifecycle", "tools.terminal_tool_macos_open", "tools.terminal_tool_result", "tools.terminal_tool_sudo", "tools.thread_context", "tools.threat_patterns", "tools.tip_tool", "tools.todo_tool", "tools.tool_backend_helpers", "tools.tool_labels", "tools.tool_output_limits", "tools.tool_output_truncate", "tools.tool_result_storage", "tools.tool_search", "tools.tool_search_catalog", "tools.tool_search_validation", "tools.tour_presets", "tools.tour_tool", "tools.transcription_audio", "tools.transcription_chunking", "tools.transcription_cloud", "tools.transcription_command", "tools.transcription_common", "tools.transcription_local", "tools.transcription_streaming", "tools.transcription_tools", "tools.transcription_whisper_cpp", "tools.tts_command_provider", "tools.tts_elevenlabs_voices", "tools.tts_streaming", "tools.tts_text_normalize", "tools.tts_tool", "tools.tts_tool_delivery", "tools.tts_tool_lifecycle", "tools.tts_tool_local", "tools.tts_tool_openai", "tools.tts_tool_plugins", "tools.tts_tool_providers", "tools.tts_tool_speaker", "tools.url_safety", "tools.video_generation_tool", "tools.vision_tools", "tools.vision_tools_history_budget", "tools.vision_tools_image_prep", "tools.voice_client_config", "tools.voice_live", "tools.voice_mode", "tools.voice_mode_transcript", "tools.wake_word", "tools.wake_word_engines", "tools.web_result_cache", "tools.web_tools", "tools.web_tools_extract", "tools.web_tools_rescue", "tools.web_tools_truncate", "tools.website_policy", "tools.working_diff", "tools.write_approval", "tools.x_search_tool", "tools.xai_http", "tools.xai_video_tools", "tools.yuanbao_tools"], Path(__file__))

mismatches = 0
total = 0

def parse_offlim(out):
    """Parse C JSON '{\"offset\":N,\"limit\":N}' -> (offset, limit)."""
    obj = json.loads(out)
    return obj["offset"], obj["limit"]

for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    rec = json.loads(line)
    fn = rec["fn"]
    total += 1

    if fn == "read_pagination":
        offset, limit, default = (int(x) for x in rec["in"].split("|"))
        # Python: returns (offset, limit) tuple with faithful clamping
        exp_off, exp_lim = fo.normalize_read_pagination(offset, limit)
        got_off, got_lim = parse_offlim(rec["out"])
        if (got_off, got_lim) != (exp_off, exp_lim):
            mismatches += 1
            print(f"MISMATCH {fn} {rec['in']} C=({got_off},{got_lim}) PY=({exp_off},{exp_lim})")

    elif fn == "search_pagination":
        offset, limit, default = (int(x) for x in rec["in"].split("|"))
        exp_off, exp_lim = fo.normalize_search_pagination(offset, limit)
        got_off, got_lim = parse_offlim(rec["out"])
        if (got_off, got_lim) != (exp_off, exp_lim):
            mismatches += 1
            print(f"MISMATCH {fn} {rec['in']} C=({got_off},{got_lim}) PY=({exp_off},{exp_lim})")

    elif fn == "is_line_oriented_error":
        err = rec.get("in")
        exp = fo._is_line_oriented_newline_error(err) if err else False
        got = (rec["out"] == "true")
        if got != exp:
            mismatches += 1
            print(f"MISMATCH {fn} {err!r} C={got} PY={exp}")

    elif fn == "pattern_has_regex_newline":
        pat = rec["in"]
        exp = fo._pattern_has_regex_newline(pat) if pat else False
        got = (rec["out"] == "true")
        if got != exp:
            mismatches += 1
            print(f"MISMATCH {fn} {pat!r} C={got} PY={exp}")

    elif fn == "maybe_warn":
        # in = "tc=N|errkind|PATTERN"  ; out = serialized result JSON
        parts = rec["in"].split("|", 2)
        tc = int(parts[0].split("=")[1])
        errkind = parts[1]
        pat = parts[2]
        if errkind == "lineerr":
            err = "literal \"\\n\" is not allowed in --multiline mode: use -U"
        elif errkind == "othererr":
            err = "some other error"
        else:
            err = None
        # Build a Python SearchResult-like object (total_count, error, warning)
        class R:
            def __init__(s): s.total_count = tc; s.error = err; s.warning = None
        res = fo._maybe_warn_line_oriented_newline_pattern(R(), pat)
        exp_warn = res.warning is not None
        exp_err_cleared = (res.error is None)
        got = json.loads(rec["out"])
        got_warn = "warning" in got and got["warning"] is not None
        got_err_cleared = (got.get("error") is None)
        if (got_warn, got_err_cleared) != (exp_warn, exp_err_cleared):
            mismatches += 1
            print(f"MISMATCH {fn} {rec['in']} C=(warn={got_warn},errcleared={got_err_cleared}) "
                  f"PY=(warn={exp_warn},errcleared={exp_err_cleared})")

print(f"{total} cases, {mismatches} mismatches")
sys.exit(1 if mismatches else 0)