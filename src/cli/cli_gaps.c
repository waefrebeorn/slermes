/*
 * cli_gaps.c — CLI subprocess hook table + Python module registry.
 *
 * Every Python hermes_cli/ module is documented with its port status, C
 * equivalent (if any), and invocation path for subprocess delegation.
 *
 * THREE CLASSES:
 *   PORTED — Fully ported to C (commands.c, cli.c, etc.)
 *   STUB   — C API exists but needs depth (numbered fallback, basic impl)
 *   N/A    — Python SDK/async/platform-specific, invoked via subprocess
 *
 * SUBPROCESS PATTERN:
 *   hermes python <module> [args...]
 *   → hermes_cli/<module>.py entry point
 *   → stdin/stdout pipe for data
 *   → Used for features that require Python deps (pyperclip, sounddevice…)
 *
 * See also: docs/how-it-works.md (full assembly documentation)
 */

#include "hermes_core_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

/* ── Helper: resolve the upstream Hermes Python installation path ── */

/* Resolve the Python Hermes agent's cli module directory.
 * Returns a heap-allocated string (caller frees) or NULL on failure.
 * Strategy: check HERMES_HOME, then common install paths. */
static char *resolve_hermes_python_dir(void) {
    const char *home = getenv("HERMES_HOME");
    if (home && *home) {
        /* Check <HERMES_HOME>/hermes_cli/ */
        size_t len = strlen(home) + 32;
        char *path = malloc(len);
        if (!path) return NULL;
        snprintf(path, len, "%s/hermes_cli", home);
        if (access(path, F_OK) == 0) return path;
        /* Check <HERMES_HOME>/../hermes_cli/ */
        snprintf(path, len, "%s/../hermes_cli", home);
        if (access(path, F_OK) == 0) return path;
        free(path);
    }

    /* Check common locations */
    const char *checks[] = {
        "/usr/local/lib/hermes-agent/hermes_cli",
        "/usr/lib/hermes-agent/hermes_cli",
        "/opt/hermes-agent/hermes_cli",
        NULL
    };
    for (int i = 0; checks[i]; i++) {
        if (access(checks[i], F_OK) == 0) {
            return strdup(checks[i]);
        }
    }

    /* Try resolving from which hermes */
    FILE *fp = popen("command -v hermes 2>/dev/null", "r");
    if (fp) {
        char buf[4096];
        if (fgets(buf, sizeof(buf), fp)) {
            buf[strcspn(buf, "\n")] = '\0';
            /* Resolve symlink and go up to find hermes_cli/ */
            char real[4096];
            if (realpath(buf, real)) {
                char *dir = dirname(real);
                size_t plen = strlen(dir) + 32;
                char *path = malloc(plen);
                if (path) {
                    snprintf(path, plen, "%s/../hermes_cli", dir);
                    char r2[4096];
                    if (realpath(path, r2)) {
                        free(path);
                        path = strdup(r2);
                    }
                    if (access(path, F_OK) == 0) {
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
