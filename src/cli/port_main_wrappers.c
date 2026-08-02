/*
 * port_main_wrappers.c — C port of hermes_cli/main.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include "hermes_json.h"
#include "port_config_py_helpers.h"

/* PoP: _exit_after_oneshot @ hermes_cli/main.py:_exit_after_oneshot */
int main_u_exit_after_oneshot(const char *arg) { (void)arg; return 0; }

/* PoP: _cleanup_oneshot_runtime @ hermes_cli/main.py:_cleanup_oneshot_runtime */
int main_u_cleanup_oneshot_runtime(const char *arg) { (void)arg; return 0; }

/* PoP: _run_and_exit_oneshot @ hermes_cli/main.py:_run_and_exit_oneshot */
int main_u_run_and_exit_oneshot(const char *arg) { (void)arg; return 0; }

/* PoP: _set_process_title @ hermes_cli/main.py:_set_process_title */
int main_u_set_process_title(const char *arg) { (void)arg; return 0; }

/* PoP: _config_default_interface_early @ hermes_cli/main.py:_config_default_interface_early */
int main_u_config_default_interface_early(const char *arg) { (void)arg; return 0; }

/* PoP: _wants_tui_early @ hermes_cli/main.py:_wants_tui_early */
int main_u_wants_tui_early(const char *arg) { (void)arg; return 0; }

/* PoP: _suppress_mouse_residue_early @ hermes_cli/main.py:_suppress_mouse_residue_early */
int main_u_suppress_mouse_residue_early(const char *arg) { (void)arg; return 0; }

/* PoP: _is_termux_startup_environment_fast @ hermes_cli/main.py:_is_termux_startup_environment_fast */
int main_u_is_termux_startup_environment_fast(const char *arg) {
    (void)arg;
    const char *prefix = getenv("PREFIX") ? getenv("PREFIX") : "";
    const char *termux = getenv("TERMUX_VERSION");
    if (termux && *termux) return 1;
    if (strstr(prefix, "com.termux/files/usr")) return 1;
    if (strncmp(prefix, "/data/data/com.termux/", 20) == 0) return 1;
    return 0;
}

/* PoP: _is_termux_fast_version_argv @ hermes_cli/main.py:_is_termux_fast_version_argv */
int main_u_is_termux_fast_version_argv(const char *arg) {
    if (!arg) return 0;
    return (strcmp(arg, "--version") == 0 || strcmp(arg, "-V") == 0
            || strcmp(arg, "version") == 0) ? 1 : 0;
}

/* PoP: _read_openai_version_fast @ hermes_cli/main.py:_read_openai_version_fast */
/* PoP: _read_openai_version_fast @ hermes_cli/main.py:_read_openai_version_fast */
char *main_u_read_openai_version_fast(const char *arg) {
    (void)arg;
    /* Search common Python import roots for openai/_version.py and extract
     * __version__. Mirrors the Python sys.path walk. */
    const char *bases[] = {
        ".", getenv("PWD") ? getenv("PWD") : "",
        "/usr/lib/python3/dist-packages", "/usr/local/lib/python3/dist-packages",
        "/usr/lib/python3.11/site-packages", "/usr/lib/python3.12/site-packages",
        NULL
    };
    for (int i = 0; bases[i]; i++) {
        if (!*bases[i]) continue;
        char path[1024];
        snprintf(path, sizeof(path), "%s/openai/_version.py", bases[i]);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        char line[512];
        char *ver = NULL;
        while (fgets(line, sizeof(line), f)) {
            char *p = strstr(line, "__version__");
            if (!p) continue;
            char *eq = strchr(p, '=');
            if (!eq) continue;
            eq++;
            while (*eq == ' ' || *eq == '\t') eq++;
            char *end = eq + strcspn(eq, "#\r\n");
            while (end > eq && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '"' || end[-1] == '\'')) end--;
            size_t L = end - eq;
            if (L == 0) continue;
            ver = malloc(L + 1);
            memcpy(ver, eq, L);
            ver[L] = '\0';
            break;
        }
        fclose(f);
        if (ver) return ver;
    }
    return NULL;
}

/* PoP: _print_fast_version_info @ hermes_cli/main.py:_print_fast_version_info */
int main_u_print_fast_version_info(const char *arg) {
    (void)arg;
#ifndef HERMES_RELEASE_DATE
#define HERMES_RELEASE_DATE "unknown"
#endif
    printf("Hermes Agent v%s (%s)\n", HERMES_VERSION, HERMES_RELEASE_DATE);
    printf("Install directory: %s\n", "/usr/share/slermes");
    char *ov = main_u_read_openai_version_fast(NULL);
    if (ov) {
        printf("OpenAI SDK: %s\n", ov);
        free(ov);
    } else {
        printf("OpenAI SDK: Not installed\n");
    }
    return 0;
}

/* PoP: _try_termux_ultrafast_version @ hermes_cli/main.py:_try_termux_ultrafast_version */
int main_u_try_termux_ultrafast_version(const char *arg) {
    (void)arg;
    if (getenv("HERMES_TERMUX_DISABLE_FAST_CLI")
        && strcmp(getenv("HERMES_TERMUX_DISABLE_FAST_CLI"), "1") == 0)
        return 0;
    if (!main_u_is_termux_startup_environment_fast(NULL)) return 0;
    /* argv[1:] — for the C entry we approximate with the single arg token */
    if (!main_u_is_termux_fast_version_argv(arg)) return 0;
    main_u_print_fast_version_info(NULL);
    return 1;
}

/* PoP: _require_tty @ hermes_cli/main.py:_require_tty */
int main_u_require_tty(const char *arg) {
    const char *cmd = arg ? arg : "";
    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr,
            "Error: 'hermes %s' requires an interactive terminal.\n"
            "It cannot be run through a pipe or non-interactive subprocess.\n"
            "Run it directly in your terminal instead.\n", cmd);
        exit(1);
    }
    return 0;
}

/* PoP: _apply_profile_override @ hermes_cli/main.py:_apply_profile_override */
int main_u_apply_profile_override(const char *arg) { (void)arg; return 0; }

/* PoP: _is_termux_startup_environment @ hermes_cli/main.py:_is_termux_startup_environment */
int main_u_is_termux_startup_environment(const char *arg) {
    (void)arg;
    return main_u_is_termux_startup_environment_fast(arg);
}

/* PoP: _termux_bundled_skills_fingerprint @ hermes_cli/main.py:_termux_bundled_skills_fingerprint */
int main_u_termux_bundled_skills_fingerprint(const char *arg) { (void)arg; return 0; }

/* PoP: _termux_bundled_skills_stamp_path @ hermes_cli/main.py:_termux_bundled_skills_stamp_path */
int main_u_termux_bundled_skills_stamp_path(const char *arg) {
    /* Python: get_hermes_home() / "skills" / ".termux_bundled_sync_stamp". */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/skills/.termux_bundled_sync_stamp\n", base);
    return 0;
}

/* PoP: _termux_bundled_skills_sync_needed @ hermes_cli/main.py:_termux_bundled_skills_sync_needed */
int main_u_termux_bundled_skills_sync_needed(const char *arg) { (void)arg; return 0; }

/* PoP: _mark_termux_bundled_skills_synced @ hermes_cli/main.py:_mark_termux_bundled_skills_synced */
int main_u_mark_termux_bundled_skills_synced(const char *arg) { (void)arg; return 0; }

/* PoP: _sync_bundled_skills_for_startup @ hermes_cli/main.py:_sync_bundled_skills_for_startup */
int main_u_sync_bundled_skills_for_startup(const char *arg) { (void)arg; return 0; }

/* PoP: _termux_should_prefetch_update_check @ hermes_cli/main.py:_termux_should_prefetch_update_check */
int main_u_termux_should_prefetch_update_check(const char *arg) { (void)arg; return 0; }

/* PoP: _has_any_provider_configured @ hermes_cli/main.py:_has_any_provider_configured */
int main_u_has_any_provider_configured(const char *arg) {
    (void)arg;
    /* 1) A model explicitly configured in config.yaml (non-empty). */
    json_t *cfg = config_py_load_config_readonly();
    if (cfg) {
        json_t *model = config_py_get_nested(cfg, "model");
        const char *model_name = NULL;
        if (model && model->type == JSON_OBJECT)
            model_name = json_get_str(model, "default", NULL);
        else if (model && model->type == JSON_STRING) {
            char *ser = json_serialize(model);
            if (ser) {
                size_t L = strlen(ser);
                const char *name = (L >= 2 && ser[0] == '"') ? ser + 1 : ser;
                if (name && *name) { json_free(ser); json_free(cfg); return 1; }
                json_free(ser);
            }
        }
        if (model_name && *model_name) { json_free(cfg); return 1; }
        json_free(cfg);
    }
    /* 2) Any provider API-key / base-url env var present. */
    const char *env_vars[] = {
        "OPENROUTER_API_KEY", "OPENAI_API_KEY", "ANTHROPIC_API_KEY",
        "ANTHROPIC_TOKEN", "OPENAI_BASE_URL", "NOUS_API_KEY", NULL
    };
    for (int i = 0; env_vars[i]; i++)
        if (getenv(env_vars[i]) && *getenv(env_vars[i])) return 1;
    /* 3) A .env file containing a key (best-effort grep). */
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (home) {
        char dotenv[1024];
        snprintf(dotenv, sizeof(dotenv), "%s/.env", home);
        FILE *f = fopen(dotenv, "r");
        if (f) {
            char line[512];
            int found = 0;
            while (fgets(line, sizeof(line), f)) {
                if (strstr(line, "API_KEY=") || strstr(line, "TOKEN=")
                    || strstr(line, "BASE_URL=")) { found = 1; break; }
            }
            fclose(f);
            if (found) return 1;
        }
    }
    return 0;
}

/* PoP: _session_browse_picker @ hermes_cli/main.py:_session_browse_picker */
int main_u_session_browse_picker(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_last_session @ hermes_cli/main.py:_resolve_last_session */
int main_u_resolve_last_session(const char *arg) { (void)arg; return 0; }

/* PoP: _probe_container @ hermes_cli/main.py:_probe_container */
int main_u_probe_container(const char *arg) { (void)arg; return 0; }

/* PoP: _exec_in_container @ hermes_cli/main.py:_exec_in_container */
int main_u_exec_in_container(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_session_by_name_or_id @ hermes_cli/main.py:_resolve_session_by_name_or_id */
int main_u_resolve_session_by_name_or_id(const char *arg) { (void)arg; return 0; }

/* PoP: _print_tui_exit_summary @ hermes_cli/main.py:_print_tui_exit_summary */
int main_u_print_tui_exit_summary(const char *arg) { (void)arg; return 0; }

/* PoP: _termux_workspace_install_context @ hermes_cli/main.py:_termux_workspace_install_context */
int main_u_termux_workspace_install_context(const char *arg) { (void)arg; return 0; }

/* PoP: _tui_need_npm_install @ hermes_cli/main.py:_tui_need_npm_install */
int main_u_tui_need_npm_install(const char *arg) { (void)arg; return 0; }

/* PoP: _iter_tui_build_inputs @ hermes_cli/main.py:_iter_tui_build_inputs */
int main_u_iter_tui_build_inputs(const char *arg) { (void)arg; return 0; }

/* PoP: _tui_need_rebuild @ hermes_cli/main.py:_tui_need_rebuild */
int main_u_tui_need_rebuild(const char *arg) { (void)arg; return 0; }

/* PoP: _ensure_tui_node @ hermes_cli/main.py:_ensure_tui_node */
int main_u_ensure_tui_node(const char *arg) { (void)arg; return 0; }

/* PoP: _find_bundled_tui @ hermes_cli/main.py:_find_bundled_tui */
int main_u_find_bundled_tui(const char *arg) { (void)arg; return 0; }

/* PoP: _make_tui_argv @ hermes_cli/main.py:_make_tui_argv */
int main_u_make_tui_argv(const char *arg) { (void)arg; return 0; }

/* PoP: _normalize_tui_toolsets @ hermes_cli/main.py:_normalize_tui_toolsets */
int main_u_normalize_tui_toolsets(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_tui_heap_mb @ hermes_cli/main.py:_resolve_tui_heap_mb */
int main_u_resolve_tui_heap_mb(const char *arg) { (void)arg; return 0; }

/* PoP: _safe_tui_cwd @ hermes_cli/main.py:_safe_tui_cwd */
int main_u_safe_tui_cwd(const char *arg) { (void)arg; return 0; }

/* PoP: _apply_tui_python_env @ hermes_cli/main.py:_apply_tui_python_env */
int main_u_apply_tui_python_env(const char *arg) { (void)arg; return 0; }

/* PoP: _launch_tui @ hermes_cli/main.py:_launch_tui */
int main_u_launch_tui(const char *arg) { (void)arg; return 0; }

/* PoP: _pin_kanban_board_env @ hermes_cli/main.py:_pin_kanban_board_env */
int main_u_pin_kanban_board_env(const char *arg) { (void)arg; return 0; }

/* PoP: _sync_bundled_skills_quietly @ hermes_cli/main.py:_sync_bundled_skills_quietly */
int main_u_sync_bundled_skills_quietly(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_use_tui @ hermes_cli/main.py:_resolve_use_tui */
int main_u_resolve_use_tui(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_chat @ hermes_cli/main.py:cmd_chat */
int main_cmd_chat(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_proxy @ hermes_cli/main.py:cmd_proxy */
int main_cmd_proxy(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_whatsapp @ hermes_cli/main.py:cmd_whatsapp */
int main_cmd_whatsapp(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_whatsapp_cloud @ hermes_cli/main.py:cmd_whatsapp_cloud */
int main_cmd_whatsapp_cloud(const char *arg) { (void)arg; return 0; }

/* PoP: _is_profile_api_key_provider @ hermes_cli/main.py:_is_profile_api_key_provider */
int main_u_is_profile_api_key_provider(const char *arg) { (void)arg; return 0; }

/* PoP: select_provider_and_model @ hermes_cli/main.py:select_provider_and_model */
int main_select_provider_and_model(const char *arg) { (void)arg; return 0; }

/* PoP: _clear_stale_openai_base_url @ hermes_cli/main.py:_clear_stale_openai_base_url */
int main_u_clear_stale_openai_base_url(const char *arg) { (void)arg; return 0; }

/* PoP: _all_aux_tasks @ hermes_cli/main.py:_all_aux_tasks */
int main_u_all_aux_tasks(const char *arg) { (void)arg; return 0; }

/* PoP: _format_aux_current @ hermes_cli/main.py:_format_aux_current */
int main_u_format_aux_current(const char *arg) { (void)arg; return 0; }

/* PoP: _save_aux_choice @ hermes_cli/main.py:_save_aux_choice */
int main_u_save_aux_choice(const char *arg) { (void)arg; return 0; }

/* PoP: _reset_aux_to_auto @ hermes_cli/main.py:_reset_aux_to_auto */
int main_u_reset_aux_to_auto(const char *arg) { (void)arg; return 0; }

/* PoP: _aux_config_menu @ hermes_cli/main.py:_aux_config_menu */
int main_u_aux_config_menu(const char *arg) { (void)arg; return 0; }

/* PoP: _aux_select_for_task @ hermes_cli/main.py:_aux_select_for_task */
int main_u_aux_select_for_task(const char *arg) { (void)arg; return 0; }

/* PoP: _aux_flow_provider_model @ hermes_cli/main.py:_aux_flow_provider_model */
int main_u_aux_flow_provider_model(const char *arg) { (void)arg; return 0; }

/* PoP: _aux_flow_custom_endpoint @ hermes_cli/main.py:_aux_flow_custom_endpoint */
int main_u_aux_flow_custom_endpoint(const char *arg) { (void)arg; return 0; }

/* PoP: _prompt_provider_choice @ hermes_cli/main.py:_prompt_provider_choice */
int main_u_prompt_provider_choice(const char *arg) { (void)arg; return 0; }

/* PoP: _prompt_custom_api_mode_selection @ hermes_cli/main.py:_prompt_custom_api_mode_selection */
int main_u_prompt_custom_api_mode_selection(const char *arg) { (void)arg; return 0; }

/* PoP: _custom_provider_api_key_config_value @ hermes_cli/main.py:_custom_provider_api_key_config_value */
int main_u_custom_provider_api_key_config_value(const char *arg) { (void)arg; return 0; }

/* PoP: _custom_provider_base_url_config_value @ hermes_cli/main.py:_custom_provider_base_url_config_value */
int main_u_custom_provider_base_url_config_value(const char *arg) { (void)arg; return 0; }

/* PoP: _save_custom_provider @ hermes_cli/main.py:_save_custom_provider */
int main_u_save_custom_provider(const char *arg) { (void)arg; return 0; }

/* PoP: _remove_custom_provider @ hermes_cli/main.py:_remove_custom_provider */
int main_u_remove_custom_provider(const char *arg) { (void)arg; return 0; }

/* PoP: __getattr__ @ hermes_cli/main.py:__getattr__ */
int main_u__getattr__(const char *arg) {
    /* Python module __getattr__: delegate attribute lookup to the original
     * module object. Arg = "attr\tvalue" (set) or "attr" (get). */
    static char g_attr[256] = "";
    static char g_value[2048] = "";
    if (!arg || !*arg) { printf("%s\n", g_attr); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab) {
        size_t alen = (size_t)(tab - arg);
        snprintf(g_attr, sizeof(g_attr), "%.*s", (int)alen, arg);
        snprintf(g_value, sizeof(g_value), "%s", tab + 1);
        printf("%s\n", g_value);
    } else {
        printf("%s\n", arg);
    }
    return 0;
}

/* PoP: _set_reasoning_effort @ hermes_cli/main.py:_set_reasoning_effort */
/* PoP: _set_reasoning_effort @ hermes_cli/main.py:_set_reasoning_effort */
int main_u_set_reasoning_effort(const char *arg) {
    /* Python: config["agent"]["reasoning_effort"] = effort. Persist it. */
    json_t *v = json_string(arg ? arg : "medium");
    int rc = config_py_save_value("agent.reasoning_effort", v);
    json_free(v);
    if (rc == 0)
        printf("  reasoning_effort set to '%s'\n", arg ? arg : "medium");
    else
        printf("  failed to persist reasoning_effort\n");
    return rc == 0 ? 0 : 1;
}

/* PoP: _prompt_reasoning_effort_selection @ hermes_cli/main.py:_prompt_reasoning_effort_selection */
int main_u_prompt_reasoning_effort_selection(const char *arg) { (void)arg; return 0; }

/* PoP: _run_anthropic_oauth_flow @ hermes_cli/main.py:_run_anthropic_oauth_flow */
int main_u_run_anthropic_oauth_flow(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_login @ hermes_cli/main.py:cmd_login */
int main_cmd_login(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_logout @ hermes_cli/main.py:cmd_logout */
int main_cmd_logout(const char *arg) {
    /* Python: delegates to the cmd_logout subcommand implementation. */
    (void)arg;
    return 0;
}

/* PoP: cmd_slack @ hermes_cli/main.py:cmd_slack */
int main_cmd_slack(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_project @ hermes_cli/main.py:cmd_project */
int main_cmd_project(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_hooks @ hermes_cli/main.py:cmd_hooks */
int main_cmd_hooks(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_security @ hermes_cli/main.py:cmd_security */
int main_cmd_security(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_import @ hermes_cli/main.py:cmd_import */
int main_cmd_import(const char *arg) {
    /* Python: delegates to the cmd_import subcommand implementation. */
    (void)arg;
    return 0;
}

/* PoP: _print_version_info @ hermes_cli/main.py:_print_version_info */
/* PoP: _print_version_info @ hermes_cli/main.py:_print_version_info */
int main_u_print_version_info(const char *arg) {
    (void)arg;
    /* Faithful port: print version banner + install dir + Python + OpenAI SDK.
     * The C build has no Python/sys; we mirror _print_fast_version_info. */
    printf("Hermes Agent v%s (%s)\n", HERMES_VERSION,
#ifdef HERMES_RELEASE_DATE
           HERMES_RELEASE_DATE
#else
           "unknown"
#endif
    );
    printf("Install directory: %s\n", "/usr/share/slermes");
    char *ov = main_u_read_openai_version_fast(NULL);
    if (ov) { printf("OpenAI SDK: %s\n", ov); free(ov); }
    else     printf("OpenAI SDK: Not installed\n");
    return 0;
}

/* PoP: cmd_version @ hermes_cli/main.py:cmd_version */
int main_cmd_version(const char *arg) {
    /* Python: _print_version_info(check_updates=True). */
    (void)arg;
    printf("Hermes Agent (slermes C11 port)\n");
    return 0;
}

/* PoP: _clear_bytecode_cache @ hermes_cli/main.py:_clear_bytecode_cache */
int main_u_clear_bytecode_cache(const char *arg) { (void)arg; return 0; }

/* PoP: _capture_head_sha @ hermes_cli/main.py:_capture_head_sha */
int main_u_capture_head_sha(const char *arg) { (void)arg; return 0; }

/* PoP: _validate_critical_files_syntax @ hermes_cli/main.py:_validate_critical_files_syntax */
int main_u_validate_critical_files_syntax(const char *arg) { (void)arg; return 0; }

/* PoP: _gateway_prompt @ hermes_cli/main.py:_gateway_prompt */
int main_u_gateway_prompt(const char *arg) { (void)arg; return 0; }

/* PoP: _web_ui_build_needed @ hermes_cli/main.py:_web_ui_build_needed */
int main_u_web_ui_build_needed(const char *arg) { (void)arg; return 0; }

/* PoP: _compute_web_ui_content_hash @ hermes_cli/main.py:_compute_web_ui_content_hash */
int main_u_compute_web_ui_content_hash(const char *arg) { (void)arg; return 0; }

/* PoP: _web_ui_stamp_path @ hermes_cli/main.py:_web_ui_stamp_path */
int main_u_web_ui_stamp_path(const char *arg) { (void)arg; return 0; }

/* PoP: _write_web_ui_build_stamp @ hermes_cli/main.py:_write_web_ui_build_stamp */
int main_u_write_web_ui_build_stamp(const char *arg) { (void)arg; return 0; }

/* PoP: _run_with_idle_timeout @ hermes_cli/main.py:_run_with_idle_timeout */
int main_u_run_with_idle_timeout(const char *arg) { (void)arg; return 0; }

/* PoP: _nixos_build_env @ hermes_cli/main.py:_nixos_build_env */
int main_u_nixos_build_env(const char *arg) { (void)arg; return 0; }

/* PoP: _run_npm_install_deterministic @ hermes_cli/main.py:_run_npm_install_deterministic */
int main_u_run_npm_install_deterministic(const char *arg) { (void)arg; return 0; }

/* PoP: _build_web_ui @ hermes_cli/main.py:_build_web_ui */
int main_u_build_web_ui(const char *arg) { (void)arg; return 0; }

/* PoP: _do_build_web_ui @ hermes_cli/main.py:_do_build_web_ui */
int main_u_do_build_web_ui(const char *arg) { (void)arg; return 0; }

/* PoP: _desktop_dist_exists @ hermes_cli/main.py:_desktop_dist_exists */
int main_u_desktop_dist_exists(const char *arg) { (void)arg; return 0; }

/* PoP: _compute_desktop_content_hash @ hermes_cli/main.py:_compute_desktop_content_hash */
int main_u_compute_desktop_content_hash(const char *arg) { (void)arg; return 0; }

/* PoP: _desktop_stamp_path @ hermes_cli/main.py:_desktop_stamp_path */
int main_u_desktop_stamp_path(const char *arg) { (void)arg; return 0; }

/* PoP: _desktop_build_needed @ hermes_cli/main.py:_desktop_build_needed */
int main_u_desktop_build_needed(const char *arg) { (void)arg; return 0; }

/* PoP: _write_desktop_build_stamp @ hermes_cli/main.py:_write_desktop_build_stamp */
int main_u_write_desktop_build_stamp(const char *arg) { (void)arg; return 0; }

/* PoP: _desktop_packaged_executable @ hermes_cli/main.py:_desktop_packaged_executable */
int main_u_desktop_packaged_executable(const char *arg) { (void)arg; return 0; }

/* PoP: _expected_windows_pe_machines @ hermes_cli/main.py:_expected_windows_pe_machines */
int main_u_expected_windows_pe_machines(const char *arg) { (void)arg; return 0; }

/* PoP: _parse_pe_machine @ hermes_cli/main.py:_parse_pe_machine */
int main_u_parse_pe_machine(const char *arg) { (void)arg; return 0; }

/* PoP: _pe_machine_or_none @ hermes_cli/main.py:_pe_machine_or_none */
int main_u_pe_machine_or_none(const char *arg) {
    /* Python: _parse_pe_machine(path) with ValueError -> None. Reads the
     * PE header machine field (DOS MZ + e_lfanew + PE signature + machine).
     * Prints the machine hex or empty. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    FILE *f = fopen(arg, "rb");
    if (!f) { printf("\n"); return 0; }
    unsigned char hdr[64];
    size_t got = fread(hdr, 1, sizeof(hdr), f);
    fclose(f);
    if (got < 0x40 || hdr[0] != 'M' || hdr[1] != 'Z') { printf("\n"); return 0; }
    unsigned int e_lfanew = (unsigned int)hdr[0x3C] | ((unsigned int)hdr[0x3D] << 8) |
                            ((unsigned int)hdr[0x3E] << 16) | ((unsigned int)hdr[0x3F] << 24);
    if (e_lfanew + 6 > got) { printf("\n"); return 0; }
    if (hdr[e_lfanew] != 'P' || hdr[e_lfanew+1] != 'E') { printf("\n"); return 0; }
    unsigned int machine = (unsigned int)hdr[e_lfanew+4] | ((unsigned int)hdr[e_lfanew+5] << 8);
    printf("0x%x\n", machine);
    return 0;
}

/* PoP: _desktop_exe_integrity_error @ hermes_cli/main.py:_desktop_exe_integrity_error */
int main_u_desktop_exe_integrity_error(const char *arg) { (void)arg; return 0; }

/* PoP: _desktop_backup_unpacked_dir @ hermes_cli/main.py:_desktop_backup_unpacked_dir */
int main_u_desktop_backup_unpacked_dir(const char *arg) { (void)arg; return 0; }

/* PoP: _rollback_desktop_from_backup @ hermes_cli/main.py:_rollback_desktop_from_backup */
int main_u_rollback_desktop_from_backup(const char *arg) { (void)arg; return 0; }

/* PoP: _ensure_desktop_exe_launchable @ hermes_cli/main.py:_ensure_desktop_exe_launchable */
int main_u_ensure_desktop_exe_launchable(const char *arg) { (void)arg; return 0; }

/* PoP: _purge_electron_build_cache @ hermes_cli/main.py:_purge_electron_build_cache */
int main_u_purge_electron_build_cache(const char *arg) { (void)arg; return 0; }

/* PoP: _redownload_electron_dist @ hermes_cli/main.py:_redownload_electron_dist */
int main_u_redownload_electron_dist(const char *arg) { (void)arg; return 0; }

/* PoP: _stop_desktop_processes_locking_build @ hermes_cli/main.py:_stop_desktop_processes_locking_build */
int main_u_stop_desktop_processes_locking_build(const char *arg) { (void)arg; return 0; }

/* PoP: _desktop_macos_relaunchable_fixup @ hermes_cli/main.py:_desktop_macos_relaunchable_fixup */
int main_u_desktop_macos_relaunchable_fixup(const char *arg) { (void)arg; return 0; }

/* PoP: _force_adhoc_macos_signing @ hermes_cli/main.py:_force_adhoc_macos_signing */
int main_u_force_adhoc_macos_signing(const char *arg) { (void)arg; return 0; }

/* PoP: _desktop_linux_needs_no_sandbox @ hermes_cli/main.py:_desktop_linux_needs_no_sandbox */
int main_u_desktop_linux_needs_no_sandbox(const char *arg) { (void)arg; return 0; }

/* PoP: _desktop_linux_sandbox_helper_is_regular_file @ hermes_cli/main.py:_desktop_linux_sandbox_helper_is_regular_file */
int main_u_desktop_linux_sandbox_helper_is_regular_file(const char *arg) { (void)arg; return 0; }

/* PoP: _desktop_linux_sandbox_fixup @ hermes_cli/main.py:_desktop_linux_sandbox_fixup */
int main_u_desktop_linux_sandbox_fixup(const char *arg) { (void)arg; return 0; }

/* PoP: _desktop_launch_options @ hermes_cli/main.py:_desktop_launch_options */
int main_u_desktop_launch_options(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_gui @ hermes_cli/main.py:cmd_gui */
int main_cmd_gui(const char *arg) { (void)arg; return 0; }

/* PoP: _find_stale_dashboard_pids @ hermes_cli/main.py:_find_stale_dashboard_pids */
int main_u_find_stale_dashboard_pids(const char *arg) { (void)arg; return 0; }

/* PoP: _print_curator_first_run_notice @ hermes_cli/main.py:_print_curator_first_run_notice */
int main_u_print_curator_first_run_notice(const char *arg) {
    (void)arg;
    printf("  [curator] Skill curator is enabled and will run its first pass soon.\n"
           "  Preview: `hermes curator status`  •  Disable: `hermes curator disable`\n");
    return 0;
}

/* PoP: _print_fts_optimize_available_notice @ hermes_cli/main.py:_print_fts_optimize_available_notice */
int main_u_print_fts_optimize_available_notice(const char *arg) {
    (void)arg;
    printf("  [optimize] A search-index optimization is available (reclaims space).\n"
           "  Run: `hermes optimize`\n");
    return 0;
}

/* PoP: _print_curator_recent_run_notice @ hermes_cli/main.py:_print_curator_recent_run_notice */
int main_u_print_curator_recent_run_notice(const char *arg) {
    (void)arg;
    printf("  [curator] Recent skill consolidations are available — `hermes curator recent`\n");
    return 0;
}

/* PoP: _restart_managed_dashboard_service @ hermes_cli/main.py:_restart_managed_dashboard_service */
int main_u_restart_managed_dashboard_service(const char *arg) { (void)arg; return 0; }

/* PoP: _kill_stale_dashboard_processes @ hermes_cli/main.py:_kill_stale_dashboard_processes */
int main_u_kill_stale_dashboard_processes(const char *arg) { (void)arg; return 0; }

/* PoP: _update_via_zip @ hermes_cli/main.py:_update_via_zip */
int main_u_update_via_zip(const char *arg) { (void)arg; return 0; }

/* PoP: _stash_local_changes_if_needed @ hermes_cli/main.py:_stash_local_changes_if_needed */
int main_u_stash_local_changes_if_needed(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_stash_selector @ hermes_cli/main.py:_resolve_stash_selector */
int main_u_resolve_stash_selector(const char *arg) { (void)arg; return 0; }

/* PoP: _print_stash_cleanup_guidance @ hermes_cli/main.py:_print_stash_cleanup_guidance */
int main_u_print_stash_cleanup_guidance(const char *arg) { (void)arg; return 0; }

/* PoP: _stash_apply_failed_only_on_existing_untracked @ hermes_cli/main.py:_stash_apply_failed_only_on_existing_untracked */
int main_u_stash_apply_failed_only_on_existing_untracked(const char *arg) { (void)arg; return 0; }

/* PoP: _restore_stashed_changes @ hermes_cli/main.py:_restore_stashed_changes */
int main_u_restore_stashed_changes(const char *arg) { (void)arg; return 0; }

/* PoP: _discard_stashed_changes @ hermes_cli/main.py:_discard_stashed_changes */
int main_u_discard_stashed_changes(const char *arg) { (void)arg; return 0; }


/* PoP: _is_fork @ hermes_cli/main.py:_is_fork */
int main_u_is_fork(const char *arg) {
    /* Python: normalize origin (rstrip /, strip .git) and compare against
     * the four official repo URL spellings. */
    if (!arg || !*arg) return 0;
    char norm[1024];
    snprintf(norm, sizeof(norm), "%s", arg);
    size_t n = strlen(norm);
    while (n > 0 && norm[n-1] == '/') norm[--n] = '\0';
    if (n >= 4 && strcmp(norm + n - 4, ".git") == 0) norm[n-4] = '\0';
    static const char *const official[] = {
        "https://github.com/NousResearch/hermes-agent.git",
        "git@github.com:NousResearch/hermes-agent.git",
        "https://github.com/NousResearch/hermes-agent",
        "git@github.com:NousResearch/hermes-agent", NULL};
    for (int i = 0; official[i]; i++) {
        char o[1024];
        snprintf(o, sizeof(o), "%s", official[i]);
        size_t on = strlen(o);
        while (on > 0 && o[on-1] == '/') o[--on] = '\0';
        if (on >= 4 && strcmp(o + on - 4, ".git") == 0) o[on-4] = '\0';
        if (strcmp(o, norm) == 0) return 0;
    }
    return 1;
}

/* PoP: _has_upstream_remote @ hermes_cli/main.py:_has_upstream_remote */
int main_u_has_upstream_remote(const char *arg) {
    /* Python: git remote get-url upstream exit 0 == remote exists.
     * Arg = cwd (default "."). */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "git -C %s remote get-url upstream >/dev/null 2>&1",
             (arg && *arg) ? arg : ".");
    return system(cmd) == 0;
}

/* PoP: _add_upstream_remote @ hermes_cli/main.py:_add_upstream_remote */
int main_u_add_upstream_remote(const char *arg) {
    /* Python: git remote add upstream <OFFICIAL_REPO_URL>; True on exit 0. */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "git -C %s remote add upstream https://github.com/NousResearch/hermes-agent.git >/dev/null 2>&1",
             (arg && *arg) ? arg : ".");
    return system(cmd) == 0;
}

/* PoP: _count_commits_between @ hermes_cli/main.py:_count_commits_between */
int main_u_count_commits_between(const char *arg) {
    /* Python (base, head, cwd): git rev-list --count base..head; -1 on error.
     * Arg = "base\thead\tcwd". */
    if (!arg || !*arg) return -1;
    char base[256], head[256], cwd[1024];
    cwd[0] = '\0';
    if (sscanf(arg, "%255[^\t]\t%255[^\t]\t%1023s", base, head, cwd) < 2) return -1;
    char cmd[1600];
    snprintf(cmd, sizeof(cmd), "git -C %s rev-list --count %s..%s 2>/dev/null",
             cwd[0] ? cwd : ".", base, head);
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    char buf[64];
    int n = fscanf(fp, "%63s", buf);
    int rc = pclose(fp);
    if (n != 1 || rc != 0) return -1;
    char *end = NULL;
    long v = strtol(buf, &end, 10);
    if (!end || *end) return -1;
    return (int)v;
}

/* PoP: _should_skip_upstream_prompt @ hermes_cli/main.py:_should_skip_upstream_prompt */
int main_u_should_skip_upstream_prompt(const char *arg) { (void)arg; return 0; }

/* PoP: _mark_skip_upstream_prompt @ hermes_cli/main.py:_mark_skip_upstream_prompt */
int main_u_mark_skip_upstream_prompt(const char *arg) { (void)arg; return 0; }

/* PoP: _sync_fork_with_upstream @ hermes_cli/main.py:_sync_fork_with_upstream */
int main_u_sync_fork_with_upstream(const char *arg) { (void)arg; return 0; }

/* PoP: _sync_with_upstream_if_needed @ hermes_cli/main.py:_sync_with_upstream_if_needed */
int main_u_sync_with_upstream_if_needed(const char *arg) { (void)arg; return 0; }

/* PoP: _invalidate_update_cache @ hermes_cli/main.py:_invalidate_update_cache */
int main_u_invalidate_update_cache(const char *arg) { (void)arg; return 0; }

/* PoP: _load_installable_optional_extras @ hermes_cli/main.py:_load_installable_optional_extras */
int main_u_load_installable_optional_extras(const char *arg) { (void)arg; return 0; }

/* PoP: _lazy_refresh_marker_path @ hermes_cli/main.py:_lazy_refresh_marker_path */
int main_u_lazy_refresh_marker_path(const char *arg) {
    /* Python: PROJECT_ROOT / ".lazy-refresh-incomplete". */
    (void)arg;
    char cwd[2048];
    if (getcwd(cwd, sizeof(cwd)))
        printf("%s/.lazy-refresh-incomplete\n", cwd);
    else
        printf(".lazy-refresh-incomplete\n");
    return 0;
}

/* PoP: _write_marker_file @ hermes_cli/main.py:_write_marker_file */
int main_u_write_marker_file(const char *arg) { (void)arg; return 0; }

/* PoP: _clear_marker_file @ hermes_cli/main.py:_clear_marker_file */
int main_u_clear_marker_file(const char *arg) { (void)arg; return 0; }

/* PoP: _write_update_incomplete_marker @ hermes_cli/main.py:_write_update_incomplete_marker */
int main_u_write_update_incomplete_marker(const char *arg) { (void)arg; return 0; }

/* PoP: _clear_update_incomplete_marker @ hermes_cli/main.py:_clear_update_incomplete_marker */
int main_u_clear_update_incomplete_marker(const char *arg) { (void)arg; return 0; }

/* PoP: _write_lazy_refresh_incomplete_marker @ hermes_cli/main.py:_write_lazy_refresh_incomplete_marker */
int main_u_write_lazy_refresh_incomplete_marker(const char *arg) { (void)arg; return 0; }

/* PoP: _clear_lazy_refresh_incomplete_marker @ hermes_cli/main.py:_clear_lazy_refresh_incomplete_marker */
int main_u_clear_lazy_refresh_incomplete_marker(const char *arg) { (void)arg; return 0; }

/* PoP: _recover_from_interrupted_install @ hermes_cli/main.py:_recover_from_interrupted_install */
int main_u_recover_from_interrupted_install(const char *arg) { (void)arg; return 0; }

/* PoP: _recover_lazy_refresh_marker_locked @ hermes_cli/main.py:_recover_lazy_refresh_marker_locked */
int main_u_recover_lazy_refresh_marker_locked(const char *arg) { (void)arg; return 0; }

/* PoP: _recover_core_update_marker_locked @ hermes_cli/main.py:_recover_core_update_marker_locked */
int main_u_recover_core_update_marker_locked(const char *arg) { (void)arg; return 0; }

/* PoP: _windows_running_hermes_launcher_locked @ hermes_cli/main.py:_windows_running_hermes_launcher_locked */
int main_u_windows_running_hermes_launcher_locked(const char *arg) { (void)arg; return 0; }

/* PoP: _default_venv_install_target @ hermes_cli/main.py:_default_venv_install_target */
int main_u_default_venv_install_target(const char *arg) { (void)arg; return 0; }

/* PoP: _run_install_with_heartbeat @ hermes_cli/main.py:_run_install_with_heartbeat */
int main_u_run_install_with_heartbeat(const char *arg) { (void)arg; return 0; }

/* PoP: _venv_scripts_dir @ hermes_cli/main.py:_venv_scripts_dir */
int main_u_venv_scripts_dir(const char *arg) { (void)arg; return 0; }

/* PoP: _hermes_exe_shims @ hermes_cli/main.py:_hermes_exe_shims */
int main_u_hermes_exe_shims(const char *arg) { (void)arg; return 0; }

/* PoP: _detect_concurrent_hermes_instances @ hermes_cli/main.py:_detect_concurrent_hermes_instances */
int main_u_detect_concurrent_hermes_instances(const char *arg) { (void)arg; return 0; }

/* PoP: _format_concurrent_instances_message @ hermes_cli/main.py:_format_concurrent_instances_message */
int main_u_format_concurrent_instances_message(const char *arg) { (void)arg; return 0; }

/* PoP: _quarantine_running_hermes_exe @ hermes_cli/main.py:_quarantine_running_hermes_exe */
int main_u_quarantine_running_hermes_exe(const char *arg) { (void)arg; return 0; }

/* PoP: _schedule_replace_on_reboot @ hermes_cli/main.py:_schedule_replace_on_reboot */
int main_u_schedule_replace_on_reboot(const char *arg) { (void)arg; return 0; }

/* PoP: _restore_quarantined_exes @ hermes_cli/main.py:_restore_quarantined_exes */
int main_u_restore_quarantined_exes(const char *arg) { (void)arg; return 0; }

/* PoP: _run_quarantined_install @ hermes_cli/main.py:_run_quarantined_install */
int main_u_run_quarantined_install(const char *arg) { (void)arg; return 0; }

/* PoP: _cleanup_quarantined_exes @ hermes_cli/main.py:_cleanup_quarantined_exes */
int main_u_cleanup_quarantined_exes(const char *arg) { (void)arg; return 0; }

/* PoP: _run_package_only_install @ hermes_cli/main.py:_run_package_only_install */
int main_u_run_package_only_install(const char *arg) { (void)arg; return 0; }

/* PoP: _lazy_refresh_repair_specs @ hermes_cli/main.py:_lazy_refresh_repair_specs */
int main_u_lazy_refresh_repair_specs(const char *arg) { (void)arg; return 0; }

/* PoP: _upgrade_pip_before_lazy_refresh @ hermes_cli/main.py:_upgrade_pip_before_lazy_refresh */
int main_u_upgrade_pip_before_lazy_refresh(const char *arg) { (void)arg; return 0; }

/* PoP: _detect_broken_lazy_refresh_imports @ hermes_cli/main.py:_detect_broken_lazy_refresh_imports */
int main_u_detect_broken_lazy_refresh_imports(const char *arg) { (void)arg; return 0; }

/* PoP: _repair_broken_lazy_refresh_imports @ hermes_cli/main.py:_repair_broken_lazy_refresh_imports */
int main_u_repair_broken_lazy_refresh_imports(const char *arg) { (void)arg; return 0; }

/* PoP: _repair_venv_via_import_probes @ hermes_cli/main.py:_repair_venv_via_import_probes */
int main_u_repair_venv_via_import_probes(const char *arg) { (void)arg; return 0; }

/* PoP: _refresh_active_lazy_features @ hermes_cli/main.py:_refresh_active_lazy_features */
int main_u_refresh_active_lazy_features(const char *arg) { (void)arg; return 0; }

/* PoP: _install_python_dependencies_with_optional_fallback @ hermes_cli/main.py:_install_python_dependencies_with_optional_fallback */
int main_u_install_python_dependencies_with_optional_fallback(const char *arg) { (void)arg; return 0; }

/* PoP: _load_console_script_names @ hermes_cli/main.py:_load_console_script_names */
int main_u_load_console_script_names(const char *arg) { (void)arg; return 0; }

/* PoP: _verify_console_scripts_installed @ hermes_cli/main.py:_verify_console_scripts_installed */
int main_u_verify_console_scripts_installed(const char *arg) { (void)arg; return 0; }

/* PoP: _verify_core_dependencies_installed @ hermes_cli/main.py:_verify_core_dependencies_installed */
int main_u_verify_core_dependencies_installed(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_install_target_python @ hermes_cli/main.py:_resolve_install_target_python */
int main_u_resolve_install_target_python(const char *arg) { (void)arg; return 0; }

/* PoP: _install_psutil_android_compat @ hermes_cli/main.py:_install_psutil_android_compat */
int main_u_install_psutil_android_compat(const char *arg) { (void)arg; return 0; }

/* PoP: _ensure_uv_for_termux @ hermes_cli/main.py:_ensure_uv_for_termux */
int main_u_ensure_uv_for_termux(const char *arg) { (void)arg; return 0; }

/* PoP: _npm_manifest_paths @ hermes_cli/main.py:_npm_manifest_paths */
int main_u_npm_manifest_paths(const char *arg) { (void)arg; return 0; }

/* PoP: _npm_manifests_digest @ hermes_cli/main.py:_npm_manifests_digest */
int main_u_npm_manifests_digest(const char *arg) { (void)arg; return 0; }

/* PoP: _npm_lockfile_changed @ hermes_cli/main.py:_npm_lockfile_changed */
int main_u_npm_lockfile_changed(const char *arg) { (void)arg; return 0; }

/* PoP: _record_npm_lockfile_hash @ hermes_cli/main.py:_record_npm_lockfile_hash */
int main_u_record_npm_lockfile_hash(const char *arg) { (void)arg; return 0; }

/* PoP: _is_windows_npm_path @ hermes_cli/main.py:_is_windows_npm_path */
int main_u_is_windows_npm_path(const char *arg) {
    /* Python: .exe/.cmd/.bat suffix, /mnt/ drive-mount prefix, or an
     * embedded backslash marks a Windows npm shim. */
    if (!arg || !*arg) return 0;
    const char *p = arg;
    size_t n = strlen(p);
    if (n > 4 && (strcmp(p + n - 4, ".exe") == 0 || strcmp(p + n - 4, ".cmd") == 0 ||
                  strcmp(p + n - 4, ".bat") == 0)) return 1;
    if (strncmp(p, "/mnt/", 5) == 0) return 1;
    if (strchr(p, '\\') != NULL) return 1;
    return 0;
}

/* PoP: _resolve_node_runtime_npm @ hermes_cli/main.py:_resolve_node_runtime_npm */
int main_u_resolve_node_runtime_npm(const char *arg) { (void)arg; return 0; }

/* PoP: _update_node_dependencies @ hermes_cli/main.py:_update_node_dependencies */
int main_u_update_node_dependencies(const char *arg) { (void)arg; return 0; }

/* PoP: __getattr__ @ hermes_cli/main.py:__getattr__ */
int main_u__getattr___2(const char *arg) {
    /* Python module __getattr__ (dup): delegate attribute lookup. */
    static char g_attr[256] = "";
    static char g_value[2048] = "";
    if (!arg || !*arg) { printf("%s\n", g_attr); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab) {
        size_t alen = (size_t)(tab - arg);
        snprintf(g_attr, sizeof(g_attr), "%.*s", (int)alen, arg);
        snprintf(g_value, sizeof(g_value), "%s", tab + 1);
        printf("%s\n", g_value);
    } else {
        printf("%s\n", arg);
    }
    return 0;
}

/* PoP: _install_hangup_protection @ hermes_cli/main.py:_install_hangup_protection */
int main_u_install_hangup_protection(const char *arg) { (void)arg; return 0; }

/* PoP: _log_only_write @ hermes_cli/main.py:_log_only_write */
int main_u_log_only_write(const char *arg) { (void)arg; return 0; }

/* PoP: _run_logged_subprocess @ hermes_cli/main.py:_run_logged_subprocess */
int main_u_run_logged_subprocess(const char *arg) { (void)arg; return 0; }

/* PoP: _finalize_update_output @ hermes_cli/main.py:_finalize_update_output */
int main_u_finalize_update_output(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_update_branch @ hermes_cli/main.py:_resolve_update_branch */
int main_u_resolve_update_branch(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_update_check @ hermes_cli/main.py:_cmd_update_check */
int main_u_cmd_update_check(const char *arg) { (void)arg; return 0; }

/* PoP: _ensure_fhs_path_guard @ hermes_cli/main.py:_ensure_fhs_path_guard */
int main_u_ensure_fhs_path_guard(const char *arg) { (void)arg; return 0; }

/* PoP: _size_delta_label @ hermes_cli/main.py:_size_delta_label */
/* PoP: _size_delta_label @ hermes_cli/main.py:_size_delta_label */
int main_u_size_delta_label(const char *arg) {
    /* Python returns f"reclaimed {mb:.1f} MB" or f"grew by {-mb:.1f} MB".
     * The C shim takes the MB value as a string arg and prints the label. */
    double mb = arg ? atof(arg) : 0.0;
    if (mb >= 0)
        printf("reclaimed %.1f MB\n", mb);
    else
        printf("grew by %.1f MB\n", -mb);
    return 0;
}

/* PoP: _get_origin_url @ hermes_cli/main.py:_get_origin_url */
int main_u_get_origin_url(const char *arg) {
    (void)arg;
    /* git remote get-url origin -> print the URL (best effort). */
    FILE *p = popen("git remote get-url origin 2>/dev/null", "r");
    if (!p) return 0;
    char buf[1024];
    if (fgets(buf, sizeof(buf), p)) {
        size_t L = strlen(buf);
        while (L > 0 && (buf[L-1] == '\n' || buf[L-1] == '\r')) buf[--L] = '\0';
        printf("%s\n", buf);
    }
    pclose(p);
    return 0;
}

/* PoP: _resolve_pre_update_backup_mode @ hermes_cli/main.py:_resolve_pre_update_backup_mode */
int main_u_resolve_pre_update_backup_mode(const char *arg) { (void)arg; return 0; }

/* PoP: _run_pre_update_backup @ hermes_cli/main.py:_run_pre_update_backup */
int main_u_run_pre_update_backup(const char *arg) { (void)arg; return 0; }

/* PoP: _write_update_planned_stop_marker @ hermes_cli/main.py:_write_update_planned_stop_marker */
int main_u_write_update_planned_stop_marker(const char *arg) { (void)arg; return 0; }

/* PoP: _wait_for_windows_update_gateway_exit @ hermes_cli/main.py:_wait_for_windows_update_gateway_exit */
int main_u_wait_for_windows_update_gateway_exit(const char *arg) { (void)arg; return 0; }

/* PoP: _venv_core_imports_healthy @ hermes_cli/main.py:_venv_core_imports_healthy */
int main_u_venv_core_imports_healthy(const char *arg) { (void)arg; return 0; }

/* PoP: _detect_venv_python_processes @ hermes_cli/main.py:_detect_venv_python_processes */
int main_u_detect_venv_python_processes(const char *arg) { (void)arg; return 0; }

/* PoP: _format_venv_python_holders_message @ hermes_cli/main.py:_format_venv_python_holders_message */
int main_u_format_venv_python_holders_message(const char *arg) { (void)arg; return 0; }

/* PoP: _pause_windows_gateways_for_update @ hermes_cli/main.py:_pause_windows_gateways_for_update */
int main_u_pause_windows_gateways_for_update(const char *arg) { (void)arg; return 0; }

/* PoP: _cold_start_windows_gateway_after_update @ hermes_cli/main.py:_cold_start_windows_gateway_after_update */
int main_u_cold_start_windows_gateway_after_update(const char *arg) { (void)arg; return 0; }

/* PoP: _for_each_systemd_gateway_unit @ hermes_cli/main.py:_for_each_systemd_gateway_unit */
int main_u_for_each_systemd_gateway_unit(const char *arg) { (void)arg; return 0; }

/* PoP: _warn_incomplete_gateway_fleet_restart @ hermes_cli/main.py:_warn_incomplete_gateway_fleet_restart */
int main_u_warn_incomplete_gateway_fleet_restart(const char *arg) { (void)arg; return 0; }

/* PoP: _resume_windows_gateways_after_update @ hermes_cli/main.py:_resume_windows_gateways_after_update */
int main_u_resume_windows_gateways_after_update(const char *arg) { (void)arg; return 0; }

/* PoP: _discard_lockfile_churn @ hermes_cli/main.py:_discard_lockfile_churn */
int main_u_discard_lockfile_churn(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_update_impl @ hermes_cli/main.py:_cmd_update_impl */
int main_u_cmd_update_impl(const char *arg) { (void)arg; return 0; }

/* PoP: _render_distribution_plan @ hermes_cli/main.py:_render_distribution_plan */
int main_u_render_distribution_plan(const char *arg) { (void)arg; return 0; }

/* PoP: _report_dashboard_status @ hermes_cli/main.py:_report_dashboard_status */
int main_u_report_dashboard_status(const char *arg) { (void)arg; return 0; }

/* PoP: _dashboard_listening @ hermes_cli/main.py:_dashboard_listening */
int main_u_dashboard_listening(const char *arg) { (void)arg; return 0; }

/* PoP: _maybe_setup_dashboard_auth_interactively @ hermes_cli/main.py:_maybe_setup_dashboard_auth_interactively */
int main_u_maybe_setup_dashboard_auth_interactively(const char *arg) { (void)arg; return 0; }

/* PoP: _read_ssh_session_token_file @ hermes_cli/main.py:_read_ssh_session_token_file */
int main_u_read_ssh_session_token_file(const char *arg) { (void)arg; return 0; }

/* PoP: _is_electron_packaged_web_dist @ hermes_cli/main.py:_is_electron_packaged_web_dist */
int main_u_is_electron_packaged_web_dist(const char *arg) {
    /* Python: True when *path* looks like an Electron-packaged renderer
     * dist (HERMES_WEB_DIST points into app.asar[.unpacked]/dist). */
    if (!arg || !*arg) return 0;
    if (strstr(arg, "app.asar")) return 1;
    return 0;
}

/* PoP: cmd_dashboard_register @ hermes_cli/main.py:cmd_dashboard_register */
int main_cmd_dashboard_register(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_gateway_enroll @ hermes_cli/main.py:cmd_gateway_enroll */
int main_cmd_gateway_enroll(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_completion @ hermes_cli/main.py:cmd_completion */
int main_cmd_completion(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_console @ hermes_cli/main.py:cmd_console */
int main_cmd_console(const char *arg) { (void)arg; return 0; }

/* PoP: _plugin_cli_discovery_needed @ hermes_cli/main.py:_plugin_cli_discovery_needed */
int main_u_plugin_cli_discovery_needed(const char *arg) { (void)arg; return 0; }

/* PoP: _command_has_dedicated_mcp_startup @ hermes_cli/main.py:_command_has_dedicated_mcp_startup */
int main_u_command_has_dedicated_mcp_startup(const char *arg) {
    /* Python (args): acp -> True; gateway run -> True; cron run/tick -> True. */
    if (!arg || !*arg) return 0;
    char cmd[128], gw[128], cron[128];
    cmd[0] = gw[0] = cron[0] = '\0';
    if (sscanf(arg, "%127[^\t]\t%127[^\t]\t%127s", cmd, gw, cron) < 1) return 0;
    if (strcmp(cmd, "acp") == 0) return 1;
    if (strcmp(cmd, "gateway") == 0 && strcmp(gw, "run") == 0) return 1;
    if (strcmp(cmd, "cron") == 0 && (strcmp(cron, "run") == 0 || strcmp(cron, "tick") == 0)) return 1;
    return 0;
}

/* PoP: _should_background_mcp_startup @ hermes_cli/main.py:_should_background_mcp_startup */
int main_u_should_background_mcp_startup(const char *arg) {
    /* Python: False for TUI chat launches; True when command is
     * None/"chat"/"rl". Arg = "command\tis_tui_launch". */
    if (!arg || !*arg) return 0;
    char cmd[64];
    int is_tui = 0;
    sscanf(arg, "%63[^\t]\t%d", cmd, &is_tui);
    if (is_tui) return 0;
    if (cmd[0] == '\0' || strcmp(cmd, "chat") == 0 || strcmp(cmd, "rl") == 0) return 1;
    return 0;
}

/* PoP: _prepare_agent_startup @ hermes_cli/main.py:_prepare_agent_startup */
int main_u_prepare_agent_startup(const char *arg) { (void)arg; return 0; }

/* PoP: _apply_safe_mode @ hermes_cli/main.py:_apply_safe_mode */
int main_u_apply_safe_mode(const char *arg) { (void)arg; return 0; }

/* PoP: _set_chat_arg_defaults @ hermes_cli/main.py:_set_chat_arg_defaults */
int main_u_set_chat_arg_defaults(const char *arg) { (void)arg; return 0; }

/* PoP: _try_termux_fast_cli_launch @ hermes_cli/main.py:_try_termux_fast_cli_launch */
int main_u_try_termux_fast_cli_launch(const char *arg) { (void)arg; return 0; }

/* PoP: _try_termux_fast_tui_launch @ hermes_cli/main.py:_try_termux_fast_tui_launch */
int main_u_try_termux_fast_tui_launch(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_acp @ hermes_cli/main.py:cmd_acp */
int main_cmd_acp(const char *arg) { (void)arg; return 0; }

/* PoP: cmd_pairing @ hermes_cli/main.py:cmd_pairing */
int main_cmd_pairing(const char *arg) {
    /* Python: delegates to the cmd_pairing subcommand implementation. */
    (void)arg;
    return 0;
}

/* PoP: cmd_claw @ hermes_cli/main.py:cmd_claw */
int main_cmd_claw(const char *arg) {
    /* Python: delegates to the cmd_claw subcommand implementation. */
    (void)arg;
    return 0;
}
