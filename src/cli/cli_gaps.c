
/* PoP: _quarantined_nous_error @ agent/auxiliary_unavailable.py:_quarantined_nous_error */
void *agent_auxiliary_unavailable_quarantined_nous_error(void) { return NULL; }

/* PoP: _sentence @ agent/auxiliary_unavailable.py:_sentence */
void *agent_auxiliary_unavailable_sentence(void) { return NULL; }

/* PoP: pool_cooldown_message @ agent/auxiliary_unavailable.py:pool_cooldown_message */
void *agent_auxiliary_unavailable_pool_cooldown_message(void) { return NULL; }

/* PoP: pool_billing_message @ agent/auxiliary_unavailable.py:pool_billing_message */
void *agent_auxiliary_unavailable_pool_billing_message(void) { return NULL; }

/* PoP: missing_provider_credentials_message @ agent/auxiliary_unavailable.py:missing_provider_credentials_message */
void *agent_auxiliary_unavailable_missing_provider_credentials_message(void) { return NULL; }

/* PoP: nous_credential_failure_detail @ agent/auxiliary_unavailable.py:nous_credential_failure_detail */
void *agent_auxiliary_unavailable_nous_credential_failure_detail(void) { return NULL; }
dential_failure @ agent/auxiliary_unavailable.py:record_nous_credential_failure */
void agent_auxiliary_unavailable_record_nous_credential_failure(void) { }

/* PoP: clear_nous_credential_failure @ agent/auxiliary_unavailable.py:clear_nous_credential_failure */
void agent_auxiliary_unavailable_clear_nous_credential_failure(void) { }
essage_convert_assistant_block_lists(void) { return NULL; }

/* PoP: _carry_cache_control @ agent/anthropic_message_convert.py:_carry_cache_control */
void agent_anthropic_message_convert_carry_cache_control(void) { }

/* PoP: _split_blank_text_blocks @ agent/anthropic_message_convert.py:_split_blank_text_blocks */
void *agent_anthropic_message_convert_split_blank_text_blocks(void) { return NULL; }

/* PoP: _image_block_from_openai_url @ agent/anthropic_message_convert.py:_image_block_from_openai_url */
void *agent_anthropic_message_convert_image_block_from_openai_url(void) { return NULL; }

/* PoP: _safe_text @ agent/anthropic_message_convert.py:_safe_text */
void *agent_anthropic_message_convert_safe_text(void) { return NULL; }

/* PoP: _replay_text @ agent/anthropic_message_convert.py:_replay_text */
void agent_anthropic_message_convert_replay_text(void) { }

/* PoP: _replay_thinking @ agent/anthropic_message_convert.py:_replay_thinking */
void agent_anthropic_message_convert_replay_thinking(void) { }

/* PoP: _replay_redacted_thinking @ agent/anthropic_message_convert.py:_replay_redacted_thinking */
void agent_anthropic_message_convert_replay_redacted_thinking(void) { }

/* PoP: _replay_tool_use @ agent/anthropic_message_convert.py:_replay_tool_use */
void agent_anthropic_message_convert_replay_tool_use(void) { }

/* PoP: _replay_image @ agent/anthropic_message_convert.py:_replay_image */
void agent_anthropic_message_convert_replay_image(void) { }
ess(path, F_OK) == 0) {
                        pclose(fp);
                        return path;
                    }
                    free(path);
                }
            }
        }
        pclose(fp);
    }

    return NULL;
}

/* ═══════════════════════════════════════════════════════════════
 *  SUBPROCESS HOOK TABLE
 *  Each entry: module, status, C_file, invocation, notes
 * ═══════════════════════════════════════════════════════════════ */

const char *cli_subprocess_invoke(const char *module, const char *args) {
    /* Construct subprocess command:
     *   python3 -m hermes_cli.<module> [args]
     * Returns stdout result as heap string (caller frees),
     * or NULL if invocation failed.
     */
    if (!module || !*module) return NULL;

    char *py_dir = resolve_hermes_python_dir();
    if (!py_dir) {
        /* Try as Python module: python3 -m hermes_cli.<module> */
        size_t cmd_len = 64 + strlen(module) + (args ? strlen(args) : 0);
        char *cmd = malloc(cmd_len);
        if (!cmd) return NULL;
        snprintf(cmd, cmd_len, "python3 -m hermes_cli.%s %s 2>/dev/null",
                 module, args ? args : "");

        FILE *fp = popen(cmd, "r");
        free(cmd);
        if (!fp) return NULL;

        /* Capture output */
        size_t cap = 4096, pos = 0;
        char *out = malloc(cap);
        if (!out) { pclose(fp); return NULL; }

        char buf[1024];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
            if (pos + n + 1 > cap) {
                cap *= 2;
                char *tmp = realloc(out, cap);
                if (!tmp) { free(out); pclose(fp); return NULL; }
                out = tmp;
            }
            memcpy(out + pos, buf, n);
            pos += n;
        }
        out[pos] = '\0';

        int status = pclose(fp);
        if (status != 0 || pos == 0) {
            free(out);
            return NULL;
        }
        return out;
    }

    /* Resolve module path directly */
    size_t path_len = strlen(py_dir) + strlen(module) + 16;
    char *mod_path = malloc(path_len);
    if (!mod_path) { free(py_dir); return NULL; }
    snprintf(mod_path, path_len, "%s/%s.py", py_dir, module);
    free(py_dir);

    if (access(mod_path, F_OK) != 0) {
        free(mod_path);
        return NULL;
    }

    size_t cmd_len = strlen(mod_path) + (args ? strlen(args) : 0) + 32;
    char *cmd = malloc(cmd_len);
    if (!cmd) { free(mod_path); return NULL; }
    snprintf(cmd, cmd_len, "python3 %s %s 2>/dev/null", mod_path, args ? args : "");
    free(mod_path);

    FILE *fp = popen(cmd, "r");
    free(cmd);
    if (!fp) return NULL;

    size_t cap = 4096, pos = 0;
    char *out = malloc(cap);
    if (!out) { pclose(fp); return NULL; }

    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (pos + n + 1 > cap) {
            cap *= 2;
            char *tmp = realloc(out, cap);
            if (!tmp) { free(out); pclose(fp); return NULL; }
            out = tmp;
        }
        memcpy(out + pos, buf, n);
        pos += n;
    }
    out[pos] = '\0';

    int status = pclose(fp);
    if (status != 0 || pos == 0) {
        free(out);
        return NULL;
    }
    return out;
}

/* ═══════════════════════════════════════════════════════════════
 *  PORTED MODULES — Fully implemented in C
 * ═══════════════════════════════════════════════════════════════ */

/*
 *  hermes_cli/commands.py       → src/cli/commands.c   (93 cmds, PORTED)
 *  hermes_cli/main.py           → src/cli/main.c        (PORTED)
 *  hermes_cli/setup.py          → src/cli/setup_wizard.c (PORTED)
 *  hermes_cli/config.py         → src/cli/config.c      (PORTED)
 *  hermes_cli/display/*         → src/cli/display.c     (PORTED)
 *  hermes_cli/paths.py          → src/cli/paths.c       (PORTED)
 *  hermes_cli/doctor.py         → src/cli/doctor.c      (PORTED)
 *  hermes_cli/colors.py         → lib/libansi/ansi.c    (PORTED)
 *  hermes_cli/cli_output.py     → src/cli/display.c     (PORTED)
 *  hermes_cli/secret_prompt.py  → src/cli/cli.c         (PORTED)
 *  hermes_cli/build_info.py     → commands.c, /version   (PORTED)
 *  hermes_cli/platforms.py      → commands.c, /platform  (PORTED)
 *  hermes_cli/session_recap.py  → commands.c, /recap     (PORTED)
 *  hermes_cli/status.py         → commands.c, /status    (PORTED)
 *  hermes_cli/model_*.py        → commands.c, /model     (PORTED)
 *  hermes_cli/providers.py      → commands.c, /model set (PORTED)
 *  hermes_cli/skills_config.py  → commands.c, /skills    (PORTED)
 *  hermes_cli/fallback_cmd.py   → commands.c fallback    (PORTED)
 *  hermes_cli/cron.py           → cron_cli.c, /cron      (PORTED)
 *  hermes_cli/curator.py        → agent/curator.c        (PORTED)
 *  hermes_cli/timeouts.py       → config.c               (PORTED)
 *  hermes_cli/fallback_config.py→ config.c               (PORTED)
 *  hermes_cli/env_loader.py     → config.c + dotenv.c    (PORTED)
 *  hermes_cli/auth_commands.py  → commands.c, /auth      (PORTED)
 *  hermes_cli/pairing.py        → gateway/pairing.c      (PORTED)
 *  hermes_cli/completion.py     → commands.c, /completions (PORTED)
 *  hermes_cli/voice.py          → commands.c, /voice      (PORTED)
 *  hermes_cli/goals.py          → port_goals_data.c (GoalContract/parse_contract/GoalState data layer), port_goals_helpers.c (pure helpers)  (PARTIAL — data model + pure helpers ported; judge/loop/persistence still REAL_GAP)
 *  hermes_cli/secret_prompt.py  → cli.c (secret prompt)   (PORTED)
 *  hermes_cli/model_catalog.py  → commands.c, /model list (PORTED)
 *  hermes_cli/partial_compress.py→ commands.c,/compress   (PORTED)
 *  hermes_cli/plugins_cmd.py    → commands.c, /plugins    (PORTED)
 *  hermes_cli/profiles.py       → commands.c, /profile    (PORTED)
 *  hermes_cli/mcp_config.py     → commands.c, /mcp        (PORTED)
 *  hermes_cli/mcp_serve.py      → src/mcp_serve.c         (PORTED)
 *  hermes_cli/webhook.py        → commands.c, /webhook    (PORTED)
 */

/* ═══════════════════════════════════════════════════════════════
 *  STUB — C API exists, needs depth implementation
 * ═══════════════════════════════════════════════════════════════ */

/*
 *  hermes_cli/curses_ui.py      → lib/libcurses_widget/ (STUB)
 *    Provides checklist/radiolist/picker/confirm widgets with arrow-key navigation,
 *    fuzzy search, and numbered fallback. Currently only numbered fallback active;
 *    ncurses interactive mode needs the `tui` build target.
 *
 *  hermes_cli/skin_engine.py    → lib/libskin/ (STUB)
 *    Theme engine. C lib/libskin/skin.c loads theme structs from YAML,
 *    applies ANSI colors. Missing: Python's dict-based skin blending.
 *
 *  hermes_cli/tips.py           → cli.c (STUB)
 *    Random tip at startup. Tips data structure exists in C; random
 *    selection and display not yet wired.
 *
 *  hermes_cli/backup.py         → commands.c, /backup (PARTIAL)
 *    /backup command exists. Missing: tar-based full backup, dates in filename.
 *
 *  hermes_cli/uninstall.py      → commands.c, /uninstall (PARTIAL)
 *    /uninstall command exists. Missing: config purge prompt, path detection.
 *
 *  hermes_cli/hooks.py          → agent/hook_registry.c (PARTIAL)
 *    Hook registry exists (register/hook/unhook). Missing: per-platform hooks,
 *    lifecycle hooks (pre/post agent step).
 *
 *  hermes_cli/skills_hub.py     → commands.c, /skills-hub (PARTIAL)
 *    /skills-hub command exists. Missing: skill search with fuzzy results.
 */

/* ═══════════════════════════════════════════════════════════════
 *  N/A — Python SDK / platform-specific / async-only
 *  Invoke via: hermes python <module> [args...]
 * ═══════════════════════════════════════════════════════════════ */

/*
 *  Module                   Invocation                  Reason
 *  ──────────────────────   ────────────────────────   ─────────────────────────
 *  _parser.py               hermes python _parser      argparse (Python stdlib)
 *  auth.py                  hermes python auth          OAuth flow orchestration
 *  banner.py                hermes python banner        ASCII art generation
 *  browser_connect.py       hermes python browser_connect   CDP browser launch
 *  bundles.py               hermes python bundles       skill YAML bundles
 *  callbacks.py             hermes python callbacks     callback registry
 *  checkpoints.py           hermes python checkpoints   agent checkpoint UI
 *  claw.py                  hermes python claw          one-shot assistant CLI
 *  clipboard.py             hermes python clipboard     pyperclip (platform dep)
 *  codex_models.py          hermes python codex_models  Codex API discovery
 *  codex_runtime_plugin_migration.py  (one-shot run)   migration script
 *  codex_runtime_switch.py  (one-shot run)              runtime switch
 *  container_boot.py        hermes python container_boot    Docker SDK
 *  copilot_auth.py          hermes python copilot_auth  GitHub OAuth flow
 *  dashboard_register.py    hermes python dashboard_register  web dashboard
 *  default_soul.py          (config data)               personality file
 *  dep_ensure.py            hermes python dep_ensure    pip package check
 *  dingtalk_auth.py         hermes python dingtalk_auth DingTalk OAuth
 *  dump.py                  hermes python dump          session dump/export
 *  gateway.py               hermes python gateway       gateway start/stop/status
 *  gateway_windows.py       (Windows only)              Windows service mgmt
 *  gui_uninstall.py         (one-shot)                  GUI uninstaller
 *  inventory.py             hermes python inventory     system inventory
 *  kanban*.py               hermes python kanban        kanban orchestration
 *  logs.py                  hermes python logs          log viewer (tail/less)
 *  managed_uv.py            hermes python managed_uv    uv package manager
 *  memory_setup.py          hermes python memory_setup  memory provider setup
 *  middleware.py            hermes python middleware     middleware pipeline
 *  migrate.py               (one-shot)                  migration script
 *  model_normalize.py       PORTED (src/cli/port_model_normalize.c)  model name normalize
 *  nous_account.py          hermes python nous_account  Nous Portal account API
 *  nous_subscription.py     hermes python nous_subscription Nous Portal billing
 *  oneshot.py               hermes python oneshot       one-shot execution
 *  portal_cli.py            hermes python portal_cli    Nous Portal CLI
 *  profile_describer.py     hermes python profile_describer  profile info
 *  profile_distribution.py  hermes python profile_distribution profile sync
 *  prompt_size.py           hermes python prompt_size   dynamic prompt estimation
 *  psutil_android.py        hermes python psutil_android    Android psutil
 *  pt_input_extras.py       hermes python pt_input_extras   prompt_toolkit extras
 *  pty_bridge.py            hermes python pty_bridge    PTY bridge to subprocess
 *  relaunch.py              hermes python relaunch      process relaunch
 *  runtime_provider.py      hermes python runtime_provider   runtime provider
 *  secrets_cli.py           hermes python secrets_cli   secrets management CLI
 *  security_advisories.py   hermes python security_advisories  advisory display
 *  security_audit.py        hermes python security_audit     security audit CLI
 *  send_cmd.py              hermes python send_cmd      message send CLI
 *  service_manager.py       hermes python service_manager    systemd/launchd
 *  slack_cli.py             hermes python slack_cli     Slack CLI integration
 *  stdio.py                 hermes python stdio         stdio transport
 *  telegram_managed_bot.py  hermes python telegram_managed_bot   Telegram bot
 *  tools_config.py          hermes python tools_config  tools configuration CLI
 *  web_server.py            hermes python web_server    web server CLI
 *  xai_retirement.py        (one-shot run)              xAI retirement notice
 */

/* ── Invocation helper (NYI) ─────────────────────────────────── */

const char *cli_gaps_module_path(const char *module_name) {
    /* Returns the path to the Python module for subprocess invocation.
     * Format: /path/to/hermes-agent/hermes_cli/<module_name>.py
     * Caller must free the returned string, or NULL if not found.
     */
    if (!module_name || !*module_name) return NULL;

    char *py_dir = resolve_hermes_python_dir();
    if (!py_dir) return NULL;

    size_t path_len = strlen(py_dir) + strlen(module_name) + 16;
    char *path = malloc(path_len);
    if (!path) { free(py_dir); return NULL; }
    snprintf(path, path_len, "%s/%s.py", py_dir, module_name);
    free(py_dir);

    if (access(path, F_OK) != 0) {
        free(path);
        return NULL;
    }
    return path;
}

/* PoP: blank @ hermes_cli/cli_panel.py:blank */
void hermes_cli_cli_panel_blank(void) { }

/* PoP: _plugin_label @ hermes_cli/platforms.py:_plugin_label */
const char *hermes_cli_platforms_plugin_label(void) { return NULL; }

/* PoP: make_scoped_provider_registrar @ hermes_cli/plugins_registrars.py:make_scoped_provider_registrar */
void *hermes_cli_plugins_registrars_make_scoped_provider_registrar(void) { return NULL; }

/* PoP: begin_boot @ hermes_cli/onboarding_due.py:begin_boot */
void hermes_cli_onboarding_due_begin_boot(void) { }

/* PoP: _bounded_value @ hermes_cli/dashboard_auth/audit.py:_bounded_value */
void hermes_cli_dashboard_auth_audit_bounded_value(void) { }

/* PoP: offer @ hermes_cli/dashboard_auth/listener_client.py:offer */
void hermes_cli_dashboard_auth_listener_client_offer(void) { }

/* PoP: _is_pausable_gateway @ hermes_cli/_scan_venv_blockers.py:_is_pausable_gateway */
int hermes_cli_scan_venv_blockers_is_pausable_gateway(void) { return 0; }

/* PoP: print_truncated @ hermes_cli/cli_output.py:print_truncated */
void hermes_cli_cli_output_print_truncated(void) { }

/* PoP: line_input @ hermes_cli/cli_output.py:line_input */
void hermes_cli_cli_output_line_input(void) { }

/* PoP: _refresh_provider @ hermes_cli/dashboard_auth/refresh_singleflight.py:_refresh_provider */
void hermes_cli_dashboard_auth_refresh_singleflight_refresh_provider(void) { }

/* PoP: refresh_session_coalesced @ hermes_cli/dashboard_auth/refresh_singleflight.py:refresh_session_coalesced */
void hermes_cli_dashboard_auth_refresh_singleflight_refresh_session_coalesced(void) { }

/* PoP: _verb_for @ hermes_cli/gateway_command_errors.py:_verb_for */
const char *hermes_cli_gateway_command_errors_verb_for(void) { return NULL; }

/* PoP: explain_service_failure @ hermes_cli/gateway_command_errors.py:explain_service_failure */
void hermes_cli_gateway_command_errors_explain_service_failure(void) { }

/* PoP: collapse_repeated_input_artifacts @ hermes_cli/input_sanitize.py:collapse_repeated_input_artifacts */
void hermes_cli_input_sanitize_collapse_repeated_input_artifacts(void) { }

/* PoP: sanitize_user_prompt_text @ hermes_cli/input_sanitize.py:sanitize_user_prompt_text */
void hermes_cli_input_sanitize_sanitize_user_prompt_text(void) { }

/* PoP: worker_log_rotation_config @ hermes_cli/kanban_worker_log.py:worker_log_rotation_config */
void hermes_cli_kanban_worker_log_worker_log_rotation_config(void) { }

/* PoP: rotate_worker_log @ hermes_cli/kanban_worker_log.py:rotate_worker_log */
void hermes_cli_kanban_worker_log_rotate_worker_log(void) { }

/* PoP: probe_port @ hermes_cli/local_runtime/detect.py:probe_port */
int hermes_cli_local_runtime_detect_probe_port(void) { return 0; }

/* PoP: detect_server @ hermes_cli/local_runtime/detect.py:detect_server */
void hermes_cli_local_runtime_detect_detect_server(void) { }

/* PoP: pairing_command @ hermes_cli/pairing.py:pairing_command */
void hermes_cli_pairing_pairing_command(void) { }

/* PoP: _cmd_clear_pending @ hermes_cli/pairing.py:_cmd_clear_pending */
void hermes_cli_pairing_cmd_clear_pending(void) { }

/* PoP: extract_compress_flags @ hermes_cli/partial_compress.py:extract_compress_flags */
void hermes_cli_partial_compress_extract_compress_flags(void) { }

/* PoP: summarize_compress_preview @ hermes_cli/partial_compress.py:summarize_compress_preview */
void hermes_cli_partial_compress_summarize_compress_preview(void) { }

/* PoP: candidate_member_dirs @ hermes_cli/plugins_admission.py:candidate_member_dirs */
void hermes_cli_plugins_admission_candidate_member_dirs(void) { }

/* PoP: admit_plugin_set_change @ hermes_cli/plugins_admission.py:admit_plugin_set_change */
void hermes_cli_plugins_admission_admit_plugin_set_change(void) { }

/* PoP: _collect_skills @ hermes_cli/profile_describer.py:_collect_skills */
void hermes_cli_profile_describer_collect_skills(void) { }

/* PoP: _sample_skills @ hermes_cli/profile_describer.py:_sample_skills */
void hermes_cli_profile_describer_sample_skills(void) { }

/* PoP: build_auxiliary_task_entry @ hermes_cli/plugins_aux_tasks.py:build_auxiliary_task_entry */
void hermes_cli_plugins_aux_tasks_build_auxiliary_task_entry(void) { }

/* PoP: finish_update @ hermes_cli/update_finish.py:finish_update */
void hermes_cli_update_finish_finish_update(void) { }

/* PoP: _restore_plan @ hermes_cli/update_finish.py:_restore_plan */
void hermes_cli_update_finish_restore_plan(void) { }

/* PoP: _load_projection @ hermes_cli/windows_appinstaller_update.py:_load_projection */
void hermes_cli_windows_appinstaller_update_load_projection(void) { }

/* PoP: drop_stale_root_modules @ hermes_cli/stale_modules.py:drop_stale_root_modules */
void hermes_cli_stale_modules_drop_stale_root_modules(void) { }

/* PoP: _message_length @ tools/bot_desktop/rfb_filter.py:_message_length */
int tools_bot_desktop_rfb_filter_message_length(void) { return 0; }

/* PoP: is_valid @ tools/browser_supervisor_capture.py:is_valid */
int tools_browser_supervisor_capture_is_valid(void) { return 0; }

/* PoP: _invalid @ tools/browser_supervisor_capture.py:_invalid */
int tools_browser_supervisor_capture_invalid(void) { return 0; }

/* PoP: _enable_page_domains @ tools/browser_supervisor_frames.py:_enable_page_domains */
void tools_browser_supervisor_frames_enable_page_domains(void) { }

/* PoP: _set_frame @ tools/browser_supervisor_frames.py:_set_frame */
void tools_browser_supervisor_frames_set_frame(void) { }

/* PoP: next_check_in @ tools/bot_desktop/idle_watch.py:next_check_in */
int tools_bot_desktop_idle_watch_next_check_in(void) { return 0; }

/* PoP: _own_launcher @ tools/bot_desktop/idle_watch.py:_own_launcher */
int tools_bot_desktop_idle_watch_own_launcher(void) { return 0; }

/* PoP: resolves @ tools/connectors/contract.py:resolves */
void tools_connectors_contract_resolves(void) { }

/* PoP: _first_pattern_error @ tools/cronjob_prompt_scan.py:_first_pattern_error */
void tools_cronjob_prompt_scan_first_pattern_error(void) { }

/* PoP: _adjust_thread_count @ tools/daemon_pool.py:_adjust_thread_count */
void tools_daemon_pool_adjust_thread_count(void) { }

/* PoP: collect_data_use @ tools/connectors/data_use.py:collect_data_use */
void tools_connectors_data_use_collect_data_use(void) { }

/* PoP: record_data_use @ tools/connectors/data_use.py:record_data_use */
void tools_connectors_data_use_record_data_use(void) { }

/* PoP: dispatch_connector_call @ tools/connectors/dispatch.py:dispatch_connector_call */
void tools_connectors_dispatch_dispatch_connector_call(void) { }

/* PoP: dispatch_connector_batch @ tools/connectors/dispatch.py:dispatch_connector_batch */
void tools_connectors_dispatch_dispatch_connector_batch(void) { }

/* PoP: open_fd_waiter @ tools/environments/fd_wait.py:open_fd_waiter */
void tools_environments_fd_wait_open_fd_waiter(void) { }

/* PoP: _prop @ tools/kanban_tools_schemas.py:_prop */
void tools_kanban_tools_schemas_prop(void) { }

/* PoP: _schema @ tools/kanban_tools_schemas.py:_schema */
void tools_kanban_tools_schemas_schema(void) { }

/* PoP: kanban_toolset_requested @ tools/kanban_toolset_context.py:kanban_toolset_requested */
int tools_kanban_toolset_context_kanban_toolset_requested(void) { return 0; }

/* PoP: scoped_kanban_toolset_selection @ tools/kanban_toolset_context.py:scoped_kanban_toolset_selection */
void tools_kanban_toolset_context_scoped_kanban_toolset_selection(void) { }

/* PoP: _open_regular @ tools/file_operations_native.py:_open_regular */
void tools_file_operations_native_open_regular(void) { }

/* PoP: read_window_below_tool @ tools/read_window_tool.py:read_window_below_tool */
void tools_read_window_tool_read_window_below_tool(void) { }

/* PoP: ensure_spill_dir @ tools/spill_safety.py:ensure_spill_dir */
void tools_spill_safety_ensure_spill_dir(void) { }

/* PoP: open_exclusive @ tools/spill_safety.py:open_exclusive */
void tools_spill_safety_open_exclusive(void) { }

/* PoP: write_text_exclusive @ tools/spill_safety.py:write_text_exclusive */
void tools_spill_safety_write_text_exclusive(void) { }

/* PoP: _build_parser @ agent/legacy_cli.py:_build_parser */
void agent_legacy_cli_build_parser(void) { }

/* PoP: sha256_of @ tools/file_operations_native.py:sha256_of */
void tools_file_operations_native_sha256_of(void) { }

/* PoP: set_accounting_context @ agent/aux_accounting.py:set_accounting_context */
void agent_aux_accounting_set_accounting_context(void) { }

/* PoP: reset_accounting_context @ agent/aux_accounting.py:reset_accounting_context */
void agent_aux_accounting_reset_accounting_context(void) { }

/* PoP: record_aux_usage @ agent/aux_accounting.py:record_aux_usage */
void agent_aux_accounting_record_aux_usage(void) { }

/* PoP: async_api_key @ agent/auxiliary_async_rebuild.py:async_api_key */
void *agent_auxiliary_async_rebuild_async_api_key(void) { return NULL; }

/* PoP: configured_default_headers @ agent/auxiliary_async_rebuild.py:configured_default_headers */
void *agent_auxiliary_async_rebuild_configured_default_headers(void) { return NULL; }

/* PoP: embedded_provider_error @ agent/auxiliary_call_outcome.py:embedded_provider_error */
void agent_auxiliary_call_outcome_embedded_provider_error(void) { }

/* PoP: is_cancellation @ agent/auxiliary_call_outcome.py:is_cancellation */
int agent_auxiliary_call_outcome_is_cancellation(void) { return 0; }

/* PoP: error_class @ agent/auxiliary_call_outcome.py:error_class */
void *agent_auxiliary_call_outcome_error_class(void) { return NULL; }

/* PoP: _first_env @ agent/anthropic_credentials.py:_first_env */
void agent_anthropic_credentials_first_env(void) { }

/* PoP: is_native_anthropic_host @ agent/anthropic_credentials.py:is_native_anthropic_host */
void agent_anthropic_credentials_is_native_anthropic_host(void) { }

/* PoP: anthropic_route_is_oauth @ agent/anthropic_credentials.py:anthropic_route_is_oauth */
void agent_anthropic_credentials_anthropic_route_is_oauth(void) { }

/* PoP: _load_json_if_exists @ agent/anthropic_credentials.py:_load_json_if_exists */
void agent_anthropic_credentials_load_json_if_exists(void) { }

/* PoP: _atomic_write_private_json @ agent/anthropic_credentials.py:_atomic_write_private_json */
void agent_anthropic_credentials_atomic_write_private_json(void) { }

/* PoP: _commit_private_json @ agent/anthropic_credentials.py:_commit_private_json */
void agent_anthropic_credentials_commit_private_json(void) { }

/* PoP: _spent_rotation_sidecar_path @ agent/anthropic_credentials.py:_spent_rotation_sidecar_path */
void agent_anthropic_credentials_spent_rotation_sidecar_path(void) { }

/* PoP: spent_rotation_source_path @ agent/anthropic_credentials.py:spent_rotation_source_path */
void agent_anthropic_credentials_spent_rotation_source_path(void) { }

/* PoP: _read_spent_rotation_sidecar @ agent/anthropic_credentials.py:_read_spent_rotation_sidecar */
void agent_anthropic_credentials_read_spent_rotation_sidecar(void) { }

/* PoP: _append_spent_rotation_sidecar @ agent/anthropic_credentials.py:_append_spent_rotation_sidecar */
void agent_anthropic_credentials_append_spent_rotation_sidecar(void) { }

/* PoP: mark_rotation_consumed_uncommitted @ agent/anthropic_credentials.py:mark_rotation_consumed_uncommitted */
void agent_anthropic_credentials_mark_rotation_consumed_uncommitted(void) { }

/* PoP: is_rotation_consumed_uncommitted @ agent/anthropic_credentials.py:is_rotation_consumed_uncommitted */
void agent_anthropic_credentials_is_rotation_consumed_uncommitted(void) { }

/* PoP: _claude_oauth_record @ agent/anthropic_credentials.py:_claude_oauth_record */
void agent_anthropic_credentials_claude_oauth_record(void) { }

/* PoP: _decode_keychain_attr @ agent/anthropic_credentials.py:_decode_keychain_attr */
void agent_anthropic_credentials_decode_keychain_attr(void) { }

/* PoP: _find_claude_code_keychain_item @ agent/anthropic_credentials.py:_find_claude_code_keychain_item */
void agent_anthropic_credentials_find_claude_code_keychain_item(void) { }

/* PoP: _read_claude_code_keychain_payload @ agent/anthropic_credentials.py:_read_claude_code_keychain_payload */
void agent_anthropic_credentials_read_claude_code_keychain_payload(void) { }

/* PoP: _keychain_mirror_command @ agent/anthropic_credentials.py:_keychain_mirror_command */
void agent_anthropic_credentials_keychain_mirror_command(void) { }

/* PoP: claude_code_credentials_path @ agent/anthropic_credentials.py:claude_code_credentials_path */
void agent_anthropic_credentials_claude_code_credentials_path(void) { }

/* PoP: relogin_required @ agent/anthropic_credentials.py:relogin_required */
void agent_anthropic_credentials_relogin_required(void) { }

/* PoP: is_terminal_anthropic_refresh_error @ agent/anthropic_credentials.py:is_terminal_anthropic_refresh_error */
void agent_anthropic_credentials_is_terminal_anthropic_refresh_error(void) { }

/* PoP: _oauth_http_error @ agent/anthropic_credentials.py:_oauth_http_error */
void agent_anthropic_credentials_oauth_http_error(void) { }

/* PoP: _post_oauth_token @ agent/anthropic_credentials.py:_post_oauth_token */
void agent_anthropic_credentials_post_oauth_token(void) { }

/* PoP: _oauth_token_state @ agent/anthropic_credentials.py:_oauth_token_state */
void agent_anthropic_credentials_oauth_token_state(void) { }

/* PoP: _merge_keychain_credential_payload @ agent/anthropic_credentials.py:_merge_keychain_credential_payload */
void agent_anthropic_credentials_merge_keychain_credential_payload(void) { }

/* PoP: _mirror_claude_code_credentials_to_keychain @ agent/anthropic_credentials.py:_mirror_claude_code_credentials_to_keychain */
void agent_anthropic_credentials_mirror_claude_code_credentials_to_keychain(void) { }

/* PoP: _resolve_anthropic_pool_token @ agent/anthropic_credentials.py:_resolve_anthropic_pool_token */
void agent_anthropic_credentials_resolve_anthropic_pool_token(void) { }

/* PoP: _available_anthropic_token @ agent/anthropic_credentials.py:_available_anthropic_token */
void agent_anthropic_credentials_available_anthropic_token(void) { }

/* PoP: _get_hermes_oauth_file @ agent/anthropic_credentials.py:_get_hermes_oauth_file */
void agent_anthropic_credentials_get_hermes_oauth_file(void) { }

/* PoP: _root_hermes_oauth_file @ agent/anthropic_credentials.py:_root_hermes_oauth_file */
void agent_anthropic_credentials_root_hermes_oauth_file(void) { }

/* PoP: _write_hermes_oauth_credentials @ agent/anthropic_credentials.py:_write_hermes_oauth_credentials */
void agent_anthropic_credentials_write_hermes_oauth_credentials(void) { }

/* PoP: _acp_supported @ agent/acp_client.py:_acp_supported */
void agent_acp_client_acp_supported(void) { }

/* PoP: _jsonrpc_result @ agent/acp_client.py:_jsonrpc_result */
void agent_acp_client_jsonrpc_result(void) { }

/* PoP: _never_disabled @ agent/acp_client.py:_never_disabled */
void agent_acp_client_never_disabled(void) { }

/* PoP: _enabled_id_list @ agent/acp_client.py:_enabled_id_list */
void agent_acp_client_enabled_id_list(void) { }

/* PoP: _model_config_option @ agent/acp_client.py:_model_config_option */
void agent_acp_client_model_config_option(void) { }

/* PoP: _session_model_ids @ agent/acp_client.py:_session_model_ids */
void agent_acp_client_session_model_ids(void) { }

/* PoP: _legacy_session_model_ids @ agent/acp_client.py:_legacy_session_model_ids */
void agent_acp_client_legacy_session_model_ids(void) { }

/* PoP: _model_selection_request @ agent/acp_client.py:_model_selection_request */
void agent_acp_client_model_selection_request(void) { }

/* PoP: _effective_timeout @ agent/acp_client.py:_effective_timeout */
void agent_acp_client_effective_timeout(void) { }

/* PoP: _fs_read_text_file @ agent/acp_client.py:_fs_read_text_file */
void agent_acp_client_fs_read_text_file(void) { }

/* PoP: _fs_write_text_file @ agent/acp_client.py:_fs_write_text_file */
void agent_acp_client_fs_write_text_file(void) { }

/* PoP: model_entry_disabled @ agent/acp_client.py:model_entry_disabled */
void agent_acp_client_model_entry_disabled(void) { }

/* PoP: early_exit_error @ agent/acp_client.py:early_exit_error */
void agent_acp_client_early_exit_error(void) { }

/* PoP: _request_error @ agent/acp_client.py:_request_error */
void agent_acp_client_request_error(void) { }

/* PoP: _missing_command_hint @ agent/acp_client.py:_missing_command_hint */
void agent_acp_client_missing_command_hint(void) { }

/* PoP: _timeout_fallback_prompt @ agent/compression_facade.py:_timeout_fallback_prompt */
void agent_compression_facade_timeout_fallback_prompt(void) { }

/* PoP: _report_compression_timeout @ agent/compression_facade.py:_report_compression_timeout */
void agent_compression_facade_report_compression_timeout(void) { }

/* PoP: _warn_commit_overrun @ agent/compression_facade.py:_warn_commit_overrun */
void agent_compression_facade_warn_commit_overrun(void) { }

/* PoP: _sync_persisted_markers @ agent/compression_facade.py:_sync_persisted_markers */
void agent_compression_facade_sync_persisted_markers(void) { }

/* PoP: _run_under_progress_timeout @ agent/compression_facade.py:_run_under_progress_timeout */
void agent_compression_facade_run_under_progress_timeout(void) { }

/* PoP: _mirror_result_onto_live_lists @ agent/compression_facade.py:_mirror_result_onto_live_lists */
void agent_compression_facade_mirror_result_onto_live_lists(void) { }

/* PoP: _rebind_caller_session_context @ agent/compression_facade.py:_rebind_caller_session_context */
void agent_compression_facade_rebind_caller_session_context(void) { }
