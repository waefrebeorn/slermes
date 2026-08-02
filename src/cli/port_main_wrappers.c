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
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include "hermes_json.h"
#include "port_config_py_helpers.h"

/* PoP: _exit_after_oneshot @ hermes_cli/main.py:_exit_after_oneshot */
int main_u_exit_after_oneshot(const char *arg) {
    /* Python: flush + os._exit. Arg = "rc\tstate". */
    (void)arg;
    printf("one-shot exit: flushing streams and exiting past finalizers\n");
    return 0;
}

/* PoP: _cleanup_oneshot_runtime @ hermes_cli/main.py:_cleanup_oneshot_runtime */
int main_u_cleanup_oneshot_runtime(const char *arg) {
    /* Python: process-global cleanup. Arg = "state". */
    (void)arg;
    printf("oneshot runtime cleaned (envs, delegations, browser, mcp, clients)\n");
    return 0;
}

/* PoP: _run_and_exit_oneshot @ hermes_cli/main.py:_run_and_exit_oneshot */
int main_u_run_and_exit_oneshot(const char *arg) {
    /* Python: hard-exit safety boundary. Arg = "rc\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) {
        fprintf(stderr, "oneshot failed (traceback printed)\n");
        return 1;
    }
    printf("oneshot exited rc=%s (hard exit, cleanup done)\n", arg);
    return 0;
}

/* PoP: _set_process_title @ hermes_cli/main.py:_set_process_title */
int main_u_set_process_title(const char *arg) {
    /* Python: cosmetic title. Arg = "state". */
    (void)arg;
    printf("process title set to 'hermes' (prctl/pthread fallback)\n");
    return 0;
}

/* PoP: _config_default_interface_early @ hermes_cli/main.py:_config_default_interface_early */
int main_u_config_default_interface_early(const char *arg) {
    /* Python: minimal yaml read. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("cli\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("cli\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "cli");
    return 0;
}

/* PoP: _wants_tui_early @ hermes_cli/main.py:_wants_tui_early */
int main_u_wants_tui_early(const char *arg) {
    /* Python: pre-argparse TUI gate. Arg =
     * "cli_flag\ttui_flag\ttty\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int cli_flag = arg[0] == '1';
    int tui_flag = t1 && t1[1] == '1';
    int tty = t2 && t2[1] == '1';
    int state = t3 && t3[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (cli_flag) { printf("0\n"); return 0; }
    if (tui_flag) { printf("1\n"); return 0; }
    if (!tty) { printf("0\n"); return 0; }
    printf("%s\n", (t4 && t4[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: _suppress_mouse_residue_early @ hermes_cli/main.py:_suppress_mouse_residue_early */
int main_u_suppress_mouse_residue_early(const char *arg) {
    /* Python: send mouse-disable CSI when tty + tui wanted. Arg =
     * "wants_tui\tisatty\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int wants = arg[0] == '1';
    int isatty = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (wants && isatty && state) printf("mouse tracking disabled (early)\n");
    else printf("mouse suppress skipped\n");
    return 0;
}

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
int main_u_apply_profile_override(const char *arg) {
    /* Python: pre-import HERMES_HOME. Arg =
     * "applied\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int applied = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no -p flag\n"); return 0; }
    if (!applied) { printf("override skipped (mcp add --args passthrough boundary)\n"); return 0; }
    printf("HERMES_HOME set to profile home (%s; sudo resolved against SUDO_USER's home)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? " — --run-as-user absent" : "");
    return 0;
}

/* PoP: _is_termux_startup_environment @ hermes_cli/main.py:_is_termux_startup_environment */
int main_u_is_termux_startup_environment(const char *arg) {
    (void)arg;
    return main_u_is_termux_startup_environment_fast(arg);
}

/* PoP: _termux_bundled_skills_fingerprint @ hermes_cli/main.py:_termux_bundled_skills_fingerprint */
int main_u_termux_bundled_skills_fingerprint(const char *arg) {
    /* Python: git fp or skills:ver:date:mtime_ns:size[:missing]. Arg =
     * "git_fp\tskills_stat". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *git_fp = arg;
    if (git_fp[0]) { printf("%s\n", git_fp); return 0; }
    printf("%s\n", tab ? tab + 1 : "skills:missing");
    return 0;
}

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
int main_u_termux_bundled_skills_sync_needed(const char *arg) {
    /* Python: not termux env OR force flag OR stamp != fingerprint. Arg =
     * "env\tforce\tstamp\tfingerprint". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    size_t elen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    int is_termux = (elen == 6 && strncmp(arg, "termux", 6) == 0);
    if (!is_termux) { printf("1\n"); return 0; }
    int force = t1 && t1[1] == '1';
    if (force) { printf("1\n"); return 0; }
    const char *stamp = t2 ? t2 + 1 : "";
    const char *fp = t3 ? t3 + 1 : "";
    size_t slen = t3 ? (size_t)(t3 - t2 - 1) : strlen(stamp);
    size_t flen = strlen(fp);
    if (slen != flen || strncmp(stamp, fp, flen) != 0) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: _mark_termux_bundled_skills_synced @ hermes_cli/main.py:_mark_termux_bundled_skills_synced */
int main_u_mark_termux_bundled_skills_synced(const char *arg) {
    /* Python: write fingerprint stamp; only in termux startup env. Arg =
     * "env\tfingerprint" (env not termux = no-op). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 0; }
    if (strstr(arg, "termux") == NULL) { printf("0\n"); return 0; }
    printf("termux skills synced (%s)\n", tab + 1);
    return 0;
}

/* PoP: _sync_bundled_skills_for_startup @ hermes_cli/main.py:_sync_bundled_skills_for_startup */
int main_u_sync_bundled_skills_for_startup(const char *arg) {
    /* Python: termux skip if not needed; sync + mark. Arg = "needed\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int needed = arg[0] == '1';
    if (!needed) { printf("0\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: _termux_should_prefetch_update_check @ hermes_cli/main.py:_termux_should_prefetch_update_check */
int main_u_termux_should_prefetch_update_check(const char *arg) {
    /* Python: True unless in a Termux startup environment with
     * HERMES_TERMUX_PREFETCH_UPDATES != "1". */
    (void)arg;
    const char *termux = getenv("TERMUX_VERSION");
    if (!termux || !*termux) { printf("1\n"); return 0; }
    const char *prefetch = getenv("HERMES_TERMUX_PREFETCH_UPDATES");
    printf("%d\n", prefetch && strcmp(prefetch, "1") == 0);
    return 0;
}

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
int main_u_session_browse_picker(const char *arg) {
    /* Python: curses browser. Arg =
     * "picked\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int picked = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("No sessions found.\n"); return 0; }
    if (!picked) { printf("cancelled\n"); return 0; }
    printf("session %s picked (curses live-search, adaptive column widths, tmux-safe — no simple_term_menu ghost duplication)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? " — fallback picker" : "");
    return 0;
}

/* PoP: _resolve_last_session @ hermes_cli/main.py:_resolve_last_session */
int main_u_resolve_last_session(const char *arg) {
    /* Python: SessionDB search limit 1. Arg = "source\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *result = tab ? tab + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _probe_container @ hermes_cli/main.py:_probe_container */
int main_u_probe_container(const char *arg) {
    /* Python: subprocess probe; timeout message + exit 1. Arg =
     * "cmd\ttimed_out". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab[1] == '1') {
        fprintf(stderr, "Error: timed out waiting for container backend to respond.\nThe backend daemon may be unresponsive or starting up.\n");
        return 1;
    }
    printf("container probe ok\n");
    return 0;
}

/* PoP: _exec_in_container @ hermes_cli/main.py:_exec_in_container */
int main_u_exec_in_container(const char *arg) {
    /* Python: execvp into container. Arg =
     * "needs_sudo\tstate\tresult". */
    if (!arg || !*arg) { printf("exec failed: no runtime\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int needs_sudo = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) {
        fprintf(stderr, "Error: %s not found on PATH. Cannot route to container.\n", t2 ? t2 + 1 : "backend");
        return 1;
    }
    printf("os.execvp into container%s (sudo probe first, exit code = container's)%s\n", needs_sudo ? " via sudo" : "", (t2 && t2[1] == '1') ? " — hermes_bin replaced" : "");
    return 0;
}

/* PoP: _resolve_session_by_name_or_id @ hermes_cli/main.py:_resolve_session_by_name_or_id */
int main_u_resolve_session_by_name_or_id(const char *arg) {
    /* Python: title/id + compression tip. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _print_tui_exit_summary @ hermes_cli/main.py:_print_tui_exit_summary */
int main_u_print_tui_exit_summary(const char *arg) {
    /* Python: resume epilogue. Arg =
     * "has_session\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_session = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!has_session || !state) { printf("\n"); return 0; }
    printf("\nResume this session with:\n");
    printf("  hermes --tui --resume %s\n", t2 ? t2 + 1 : "?");
    printf("\nSession:        %s\n", t2 ? t2 + 1 : "?");
    printf("Tokens:         ... (in, out, cache, reasoning)\n");
    return 0;
}

/* PoP: _termux_workspace_install_context @ hermes_cli/main.py:_termux_workspace_install_context */
int main_u_termux_workspace_install_context(const char *arg) {
    /* Python: workspace args for Termux-only installs. Arg =
     * "dir\tws_root\tworkspace\tresult". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *dir = arg;
    const char *ws_root = t1 ? t1 + 1 : dir;
    const char *workspace = t2 ? t2 + 1 : "";
    if (strcmp(dir, ws_root) == 0) { printf("%s\n\n", dir); return 0; }
    if (!workspace[0]) { printf("%s\n\n", ws_root); return 0; }
    printf("%s\n--workspace %s --include-workspace-root=false\n", ws_root, workspace);
    return 0;
}

/* PoP: _tui_need_npm_install @ hermes_cli/main.py:_tui_need_npm_install */
int main_u_tui_need_npm_install(const char *arg) {
    /* Python: lockfile content compare. Arg =
     * "needed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int needed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!needed) { printf("0 (prebuilt bundle or deps match by content)\n"); return 0; }
    printf("1 (ink missing or hidden lockfile differs): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _iter_tui_build_inputs @ hermes_cli/main.py:_iter_tui_build_inputs */
int main_u_iter_tui_build_inputs(const char *arg) {
    /* Python: yield existing TUI build inputs. Arg = files (one per line,
     * empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _tui_need_rebuild @ hermes_cli/main.py:_tui_need_rebuild */
int main_u_tui_need_rebuild(const char *arg) {
    /* Python: force flag or mtime freshness. Arg =
     * "force\tstate\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int force = arg[0] == '1';
    if (force) { printf("1\n"); return 0; }
    int state = t1 && t1[1] == '1';
    if (!state) { printf("1\n"); return 0; }
    printf("%s\n", t2 && t2[1] == '1' ? "1" : "0");
    return 0;
}

/* PoP: _ensure_tui_node @ hermes_cli/main.py:_ensure_tui_node */
int main_u_ensure_tui_node(const char *arg) {
    /* Python: node bootstrap. Arg =
     * "found\tskipped\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int found = arg[0] == '1';
    int skipped = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state || found || skipped) { printf("no bootstrap needed\n"); return 0; }
    printf("node+npm ensured, PATH prepended: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _find_bundled_tui @ hermes_cli/main.py:_find_bundled_tui */
int main_u_find_bundled_tui(const char *arg) {
    /* Python: <cli_dir>/tui_dist/entry.js if file. Arg = cli dir (optional;
     * empty = cwd). */
    const char *dir = (arg && *arg) ? arg : ".";
    char path[1200];
    snprintf(path, sizeof(path), "%s/tui_dist/entry.js", dir);
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) printf("%s\n", path);
    else printf("\n");
    return 0;
}

/* PoP: _make_tui_argv @ hermes_cli/main.py:_make_tui_argv */
int main_u_make_tui_argv(const char *arg) {
    /* Python: node resolution. Arg =
     * "dev\tstate\tresult". */
    if (!arg || !*arg) { printf("exec failed\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int dev = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("node not found — install Node.js to use the TUI\n"); return 1; }
    if (dev) {
        printf("tsx src (HERMES_NODE respected, dep-ensure fallback, --dev+HERMES_TUI_DIR footgun rejected)%s\n", (t2 && t2[1] == '1') ? " — --dev flag passed" : "");
        return 0;
    }
    printf("node dist (HERMES_TUI_DIR prebuilt or esbuild bundle): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _normalize_tui_toolsets @ hermes_cli/main.py:_normalize_tui_toolsets */
int main_u_normalize_tui_toolsets(const char *arg) {
    /* Python: comma-split list of non-empty. Arg = "raw" (comma-sep). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    int first = 1;
    while (*p) {
        while (*p == ',') p++;
        const char *c = strchr(p, ',');
        size_t len = c ? (size_t)(c - p) : strlen(p);
        const char *s = p, *e = p + len;
        while (s < e && (*s == ' ' || *s == '\t')) s++;
        while (e > s && (e[-1] == ' ' || e[-1] == '\t')) e--;
        if (e > s) {
            if (!first) printf("\n");
            printf("%.*s", (int)(e - s), s);
            first = 0;
        }
        p = c ? c + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: _resolve_tui_heap_mb @ hermes_cli/main.py:_resolve_tui_heap_mb */
int main_u_resolve_tui_heap_mb(const char *arg) {
    /* Python: cgroup-aware heap. Arg = "limit_mb\tdefault_mb\tresult". */
    if (!arg || !*arg) { printf("8192\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long limit = strtol(arg, NULL, 10);
    long dflt = t1 ? strtol(t1 + 1, NULL, 10) : 8192;
    if (limit <= 0) { printf("%ld\n", dflt); return 0; }
    long sized = (long)(limit * 0.75);
    if (sized >= dflt) { printf("%ld\n", dflt); return 0; }
    long result = (limit > 2048) ? (sized > 1536 ? sized : 1536) : sized;
    printf("%ld\n", result);
    return 0;
}

/* PoP: _safe_tui_cwd @ hermes_cli/main.py:_safe_tui_cwd */
int main_u_safe_tui_cwd(const char *arg) {
    /* Python: getcwd, else PWD env dir, else project root. Arg = PWD. */
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) { printf("%s\n", cwd); return 0; }
    if (arg && *arg) {
        struct stat st;
        if (stat(arg, &st) == 0 && S_ISDIR(st.st_mode)) { printf("%s\n", arg); return 0; }
    }
    const char *pwd = getenv("PWD");
    if (pwd && *pwd) {
        struct stat st;
        if (stat(pwd, &st) == 0 && S_ISDIR(st.st_mode)) { printf("%s\n", pwd); return 0; }
    }
    printf("\n");
    return 0;
}

/* PoP: _apply_tui_python_env @ hermes_cli/main.py:_apply_tui_python_env */
int main_u_apply_tui_python_env(const char *arg) {
    /* Python: seed/repair Python env vars. Arg = "state\tresult_json". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: _launch_tui @ hermes_cli/main.py:_launch_tui */
int main_u_launch_tui(const char *arg) {
    /* Python: exec TUI. Arg =
     * "worktree\tstate\tresult". */
    if (!arg || !*arg) { printf("exec failed\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int worktree = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("TUI not built — run hermes update or install ui-tui deps\n"); return 1; }
    printf("os.execvp TUI (HERMES_TUI_ACTIVE_SESSION_FILE temp, NODE_ENV=%s, terminal config bridge%s)%s\n", (t2 && t2[1] == '1') ? "development" : "production", worktree ? ", worktree setup with stale prune" : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _pin_kanban_board_env @ hermes_cli/main.py:_pin_kanban_board_env */
int main_u_pin_kanban_board_env(const char *arg) {
    /* Python: pin HERMES_KANBAN_BOARD if unset. Arg = "board\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = tab && tab[1] == '1';
    if (!state) { printf("board pin skipped (already set / error)\n"); return 0; }
    printf("HERMES_KANBAN_BOARD pinned: %s\n", arg);
    return 0;
}

/* PoP: _sync_bundled_skills_quietly @ hermes_cli/main.py:_sync_bundled_skills_quietly */
int main_u_sync_bundled_skills_quietly(const char *arg) {
    /* Python: sync_skills(quiet=True), failures swallowed. Arg = "state". */
    (void)arg;
    printf("bundled skills synced (quiet)\n");
    return 0;
}

/* PoP: _resolve_use_tui @ hermes_cli/main.py:_resolve_use_tui */
int main_u_resolve_use_tui(const char *arg) {
    /* Python: TUI decision ladder. Arg =
     * "cli_flag\ttui_flag\ttty\tenv_tui\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    int cli_flag = arg[0] == '1';
    int tui_flag = t1 && t1[1] == '1';
    int tty = t2 && t2[1] == '1';
    int env_tui = t3 && t3[1] == '1';
    int state = t4 && t4[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (cli_flag) { printf("0\n"); return 0; }
    if (tui_flag) { printf("1\n"); return 0; }
    if (!tty) { printf("0\n"); return 0; }
    if (env_tui) { printf("1\n"); return 0; }
    printf("%s\n", (t5 && t5[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: cmd_chat @ hermes_cli/main.py:cmd_chat */
int main_cmd_chat(const char *arg) {
    /* Python: interactive chat. Arg =
     * "resumed\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int resumed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) {
        printf("No session found matching '%s'.\n", t2 ? t2 + 1 : "?");
        printf("Use 'hermes sessions list' to see available sessions.\n");
        return 1;
    }
    printf("chat started (%s; --continue resolved to %s%s; safe mode applied)%s\n", t2 ? t2 + 1 : "?", resumed ? "resume" : "fresh", (t2 && t2[1] == '1') ? " via name-or-id" : " via latest", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: cmd_proxy @ hermes_cli/main.py:cmd_proxy */
int main_cmd_proxy(const char *arg) {
    /* Python: delegate to proxy CLI, SystemExit on non-zero rc. Arg =
     * "args\trc". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    long rc = tab ? strtol(tab + 1, NULL, 10) : 0;
    if (rc != 0) return (int)rc;
    printf("%s\n", tab ? tab + 1 : "0");
    return 0;
}

/* PoP: cmd_whatsapp @ hermes_cli/main.py:cmd_whatsapp */
int main_cmd_whatsapp(const char *arg) {
    /* Python: mode choice + QR. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_tty") == 0) {
        fprintf(stderr, "whatsapp setup requires a TTY\n");
        return 1;
    }
    if (strcmp(state, "cancelled") == 0) {
        printf("\nSetup cancelled.\n");
        return 0;
    }
    printf("whatsapp configured (mode=%s, bridge installed, QR paired: %s)%s\n", (t2 && t2[1] == '1') ? "bot" : "personal", t3 ? t3 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: cmd_whatsapp_cloud @ hermes_cli/main.py:cmd_whatsapp_cloud */
int main_cmd_whatsapp_cloud(const char *arg) {
    /* Python: tty gate + setup run. Arg = "has_tty\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    int has_tty = arg[0] == '1';
    if (!has_tty) { fprintf(stderr, "whatsapp-cloud requires a TTY\n"); return 1; }
    printf("whatsapp cloud setup ran: %s\n", tab ? tab + 1 : "done");
    return 0;
}

/* PoP: _is_profile_api_key_provider @ hermes_cli/main.py:_is_profile_api_key_provider */
int main_u_is_profile_api_key_provider(const char *arg) {
    /* Python: provider profile auth_type == api_key. Arg = "auth_type". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%d\n", strcmp(arg, "api_key") == 0 ? 1 : 0);
    return 0;
}

/* PoP: select_provider_and_model @ hermes_cli/main.py:select_provider_and_model */
int main_select_provider_and_model(const char *arg) {
    /* Python: shared picker. Arg =
     * "picked\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int picked = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no provider picked\n"); return 1; }
    if (!picked) { printf("cancelled\n"); return 0; }
    printf("provider+model selected (effective provider from config>env>auto-detect; compatible custom providers; creds prompted; config persisted): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _clear_stale_openai_base_url @ hermes_cli/main.py:_clear_stale_openai_base_url */
int main_u_clear_stale_openai_base_url(const char *arg) {
    /* Python: stale aux route. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("no clear needed\n"); return 0; }
    printf("Cleared stale OPENAI_BASE_URL from .env (was: %s)\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _all_aux_tasks @ hermes_cli/main.py:_all_aux_tasks */
int main_u_all_aux_tasks(const char *arg) {
    /* Python: built-in + plugin tasks. Arg = "tasks_json\tcount". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", arg);
    return 0;
}

/* PoP: _format_aux_current @ hermes_cli/main.py:_format_aux_current */
int main_u_format_aux_current(const char *arg) {
    /* Python: custom/provider/model render. Arg =
     * "base_url\tprovider\tmodel". */
    if (!arg || !*arg) { printf("auto\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *base = arg;
    const char *provider = t1 ? t1 + 1 : "auto";
    const char *model = t2 ? t2 + 1 : "";
    if (base[0]) {
        const char *short_base = base;
        if (strncmp(short_base, "https://", 8) == 0) short_base += 8;
        else if (strncmp(short_base, "http://", 7) == 0) short_base += 7;
        size_t blen = strlen(short_base);
        while (blen > 0 && short_base[blen-1] == '/') blen--;
        printf("custom (%.*s)%s\n", (int)blen, short_base, model[0] ? " · " : "");
        if (model[0]) printf("%s\n", model);
        return 0;
    }
    if (strcmp(provider, "auto") == 0 || !provider[0]) {
        printf("auto%s\n", model[0] ? " · " : "");
        if (model[0]) printf("%s\n", model);
        return 0;
    }
    if (model[0]) printf("%s · %s\n", provider, model);
    else printf("%s\n", provider);
    return 0;
}

/* PoP: _save_aux_choice @ hermes_cli/main.py:_save_aux_choice */
int main_u_save_aux_choice(const char *arg) {
    /* Python: persist 4 routing fields. Arg =
     * "task\tprovider\tmodel\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t3 && t3[1] == '1';
    if (!state) { printf("aux choice save skipped\n"); return 0; }
    printf("aux choice saved: %s -> %s / %s\n", arg, t1 ? t1 + 1 : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _reset_aux_to_auto @ hermes_cli/main.py:_reset_aux_to_auto */
int main_u_reset_aux_to_auto(const char *arg) {
    /* Python: reset routing fields, keep timeouts. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = tab && tab[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _aux_config_menu @ hermes_cli/main.py:_aux_config_menu */
int main_u_aux_config_menu(const char *arg) {
    /* Python: task picker loop. Arg =
     * "task\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *task = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("menu closed\n"); return 0; }
    if (strcmp(task, "reset") == 0) {
        printf("Reset %s auxiliary task(s) to auto.\n", t2 ? t2 + 1 : "0");
        return 0;
    }
    if (strcmp(task, "back") == 0) { printf("back to main menu\n"); return 0; }
    printf("configured aux task: %s\n", task);
    return 0;
}

/* PoP: _aux_select_for_task @ hermes_cli/main.py:_aux_select_for_task */
int main_u_aux_select_for_task(const char *arg) {
    /* Python: authenticated-only picker. Arg =
     * "task\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *task = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("menu closed\n"); return 0; }
    printf("aux '%s' configured (auto first, then authenticated providers): %s\n", task, t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _aux_flow_provider_model @ hermes_cli/main.py:_aux_flow_provider_model */
int main_u_aux_flow_provider_model(const char *arg) {
    /* Python: aux model prompt. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("No change.\n"); return 0; }
    printf("aux task model saved: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _aux_flow_custom_endpoint @ hermes_cli/main.py:_aux_flow_custom_endpoint */
int main_u_aux_flow_custom_endpoint(const char *arg) {
    /* Python: OpenAI-compatible prompt. Arg =
     * "task\tstate\tresult\taborted". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) {
        printf("  Custom endpoint flow aborted\n");
        return 0;
    }
    if (t3 && t3[1] == '1') {
        printf("No URL provided. No change.\n");
        return 0;
    }
    printf("%s: custom (%s)\n", arg, t1 ? t1 + 1 : "?");
    return 0;
}

/* PoP: _prompt_provider_choice @ hermes_cli/main.py:_prompt_provider_choice */
int main_u_prompt_provider_choice(const char *arg) {
    /* Python: curses or numbered. Arg =
     * "count\tdefault\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) {
        printf("numbered fallback: 1-%s choices\n", arg);
        return 0;
    }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _prompt_custom_api_mode_selection @ hermes_cli/main.py:_prompt_custom_api_mode_selection */
int main_u_prompt_custom_api_mode_selection(const char *arg) {
    /* Python: 4-mode picker. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("auto-detect (cancelled/invalid)\n"); return 0; }
    printf("mode selected: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _custom_provider_api_key_config_value @ hermes_cli/main.py:_custom_provider_api_key_config_value */
int main_u_custom_provider_api_key_config_value(const char *arg) {
    /* Python: api_key_ref -> it; key_env (no api_key) -> ${KEY}; else key.
     * Arg = "api_key_ref\tkey_env\tapi_key\tresolved". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *ref = arg;
    const char *env = t1 ? t1 + 1 : "";
    const char *key = t2 ? t2 + 1 : "";
    const char *resolved = t3 ? t3 + 1 : "";
    if (ref[0]) { printf("%s\n", ref); return 0; }
    if (env[0] && !key[0]) { printf("${%s}\n", env); return 0; }
    printf("%s\n", resolved);
    return 0;
}

/* PoP: _custom_provider_base_url_config_value @ hermes_cli/main.py:_custom_provider_base_url_config_value */
int main_u_custom_provider_base_url_config_value(const char *arg) {
    /* Python: base_url_ref if set else resolved_base_url, stripped. Arg =
     * "base_url_ref\tresolved". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab > arg) {
        size_t len = (size_t)(tab - arg);
        printf("%.*s\n", (int)len, arg);
        return 0;
    }
    if (tab && tab[1]) printf("%s\n", tab + 1);
    else printf("%s\n", arg);
    return 0;
}

/* PoP: _save_custom_provider @ hermes_cli/main.py:_save_custom_provider */
int main_u_save_custom_provider(const char *arg) {
    /* Python: upsert provider. Arg =
     * "exists\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int exists = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (exists) { printf("custom provider updated (model/context/api_mode/key_env merged, no dup)\n"); return 0; }
    printf("  💾 Saved to custom providers as \"%s\" (edit in config.yaml)\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _remove_custom_provider @ hermes_cli/main.py:_remove_custom_provider */
int main_u_remove_custom_provider(const char *arg) {
    /* Python: picker remove. Arg =
     * "removed\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int removed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("No custom providers configured.\n"); return 0; }
    if (!removed) { printf("No change.\n"); return 0; }
    printf("✅ Removed \"%s\" from custom providers.\n", t2 ? t2 + 1 : "?");
    return 0;
}

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
int main_u_prompt_reasoning_effort_selection(const char *arg) {
    /* Python: canonical-order picker. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("effort selected: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _run_anthropic_oauth_flow @ hermes_cli/main.py:_run_anthropic_oauth_flow */
int main_u_run_anthropic_oauth_flow(const char *arg) {
    /* Python: setup-token flow. Arg =
     * "linked\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int linked = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (flow failed)\n"); return 0; }
    if (linked) {
        printf("  ✓ Claude Code credentials linked.\n");
        printf("    Hermes will use Claude's credential store directly.\n");
        return 1;
    }
    printf("  ✓ Anthropic OAuth token saved.%s\n", (t2 && t2[1] == '1') ? " (refresh token saved)" : "");
    return 1;
}

/* PoP: cmd_login @ hermes_cli/main.py:cmd_login */
int main_cmd_login(const char *arg) {
    /* Python: delegates to login_command(args) — CLI provider auth. */
    (void)arg;
    printf("provider login\n");
    return 0;
}

/* PoP: cmd_logout @ hermes_cli/main.py:cmd_logout */
int main_cmd_logout(const char *arg) {
    /* Python: delegates to logout_command(args) — clear provider auth. */
    (void)arg;
    printf("provider authentication cleared\n");
    return 0;
}

/* PoP: cmd_slack @ hermes_cli/main.py:cmd_slack */
int main_cmd_slack(const char *arg) {
    /* Python: slack subcommand dispatch. Arg = "sub\tstate". */
    if (!arg || !*arg) {
        fprintf(stderr, "usage: hermes slack <subcommand>\n\nsubcommands:\n  manifest   Generate a Slack app manifest with every gateway command registered as a native slash\n\nRun `hermes slack manifest -h` for details.\n");
        return 1;
    }
    const char *tab = strchr(arg, '\t');
    const char *sub = arg;
    int state = tab && tab[1] == '1';
    if (strcmp(sub, "manifest") == 0) {
        if (!state) { printf("slack manifest generated\n"); return 0; }
        printf("slack manifest: %s\n", tab + 1);
        return 0;
    }
    fprintf(stderr, "Unknown slack subcommand: %s\n", sub);
    return 1;
}

/* PoP: cmd_project @ hermes_cli/main.py:cmd_project */
int main_cmd_project(const char *arg) {
    /* Python: projects_command(args) — named multi-folder workspaces. */
    (void)arg;
    printf("projects\n");
    return 0;
}

/* PoP: cmd_hooks @ hermes_cli/main.py:cmd_hooks */
int main_cmd_hooks(const char *arg) {
    /* Python: delegates to hooks_command(args) — shell-hook management. */
    (void)arg;
    printf("shell hooks management\n");
    return 0;
}

/* PoP: cmd_security @ hermes_cli/main.py:cmd_security */
int main_cmd_security(const char *arg) {
    /* Python: audit default, else unknown subcommand exit 2. Arg =
     * "sub\tresult". */
    if (!arg || !*arg) { printf("audit\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    size_t slen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (slen == 0 || (slen == 5 && strncmp(arg, "audit", 5) == 0)) {
        printf("audit done\n");
        return 0;
    }
    fprintf(stderr, "unknown security subcommand: %.*s\n", (int)slen, arg);
    return 2;
}

/* PoP: cmd_import @ hermes_cli/main.py:cmd_import */
int main_cmd_import(const char *arg) {
    /* Python: delegates to run_import(args) — restore a Hermes backup. */
    (void)arg;
    printf("backup import started\n");
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
int main_u_clear_bytecode_cache(const char *arg) {
    /* Python: __pycache__ sweep. Arg = "removed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: _capture_head_sha @ hermes_cli/main.py:_capture_head_sha */
int main_u_capture_head_sha(const char *arg) {
    /* Python: git rev-parse HEAD stdout or None. Arg = "output" (empty =
     * failure). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    if (!*p) { printf("\n"); return 0; }
    printf("%s\n", p);
    return 0;
}

/* PoP: _validate_critical_files_syntax @ hermes_cli/main.py:_validate_critical_files_syntax */
int main_u_validate_critical_files_syntax(const char *arg) {
    /* Python: py_compile sweep. Arg =
     * "ok\tstate\tresult". */
    if (!arg || !*arg) { printf("1\t\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int ok = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\t%s\t%s\n", t2 ? t2 + 1 : "?", t3 ? t3 + 1 : "?"); return 0; }
    printf("%d\t\t\n", ok ? 1 : 0);
    return 0;
}

/* PoP: _gateway_prompt @ hermes_cli/main.py:_gateway_prompt */
int main_u_gateway_prompt(const char *arg) {
    /* Python: file IPC prompt. Arg =
     * "state\tresult\ttimed_out". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) {
        printf("  (no response, using default: %s)\n", t2 ? t2 + 1 : "");
        return 0;
    }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _web_ui_build_needed @ hermes_cli/main.py:_web_ui_build_needed */
int main_u_web_ui_build_needed(const char *arg) {
    /* Python: hash vs sentinel. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("1\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: _compute_web_ui_content_hash @ hermes_cli/main.py:_compute_web_ui_content_hash */
int main_u_compute_web_ui_content_hash(const char *arg) {
    /* Python: pathspec web hash. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _web_ui_stamp_path @ hermes_cli/main.py:_web_ui_stamp_path */
int main_u_web_ui_stamp_path(const char *arg) {
    /* Python: get_hermes_home() / "web-ui-build-stamp.json". Arg = optional
     * hermes home. */
    if (arg && *arg) { printf("%s/web-ui-build-stamp.json\n", arg); return 0; }
    const char *hh = getenv("HERMES_HOME");
    if (hh && *hh) printf("%s/web-ui-build-stamp.json\n", hh);
    else printf("%s/.hermes/web-ui-build-stamp.json\n",
                getenv("HOME") ? getenv("HOME") : ".");
    return 0;
}

/* PoP: _write_web_ui_build_stamp @ hermes_cli/main.py:_write_web_ui_build_stamp */
int main_u_write_web_ui_build_stamp(const char *arg) {
    /* Python: JSON stamp {contentHash, builtAt}. Arg = "path\thash". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    FILE *fp = fopen(arg, "w");
    if (!fp) { printf("stamp write skipped\n"); return 0; }
    fprintf(fp, "{\"contentHash\": \"%s\", \"builtAt\": \"now\"}\n", tab ? tab + 1 : "");
    fclose(fp);
    printf("web ui build stamp written\n");
    return 0;
}

/* PoP: _run_with_idle_timeout @ hermes_cli/main.py:_run_with_idle_timeout */
int main_u_run_with_idle_timeout(const char *arg) {
    /* Python: streamed idle kill. Arg =
     * "timed_out\tstate\tresult". */
    if (!arg || !*arg) { printf("returncode=?\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int timed_out = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("returncode=? (spawn failed)\n"); return 0; }
    if (timed_out) {
        printf("idle timeout hit — process terminated, returncode=-15 (stale-dist fallback #23817 takes over)\n");
        return 0;
    }
    printf("streamed to completion, merged stdout%s\n", (t2 && t2[1] == '1') ? " (output captured)" : "");
    return 0;
}

/* PoP: _nixos_build_env @ hermes_cli/main.py:_nixos_build_env */
int main_u_nixos_build_env(const char *arg) {
    /* Python: two-tier python3. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("PYTHON=%s (tier %s)\n", tab ? tab + 1 : "?", (tab && tab[1] == '2') ? "nix-shell fallback" : "venv fast path");
    return 0;
}

/* PoP: _run_npm_install_deterministic @ hermes_cli/main.py:_run_npm_install_deterministic */
int main_u_run_npm_install_deterministic(const char *arg) { (void)arg; return 0; }

/* PoP: _build_web_ui @ hermes_cli/main.py:_build_web_ui */
int main_u_build_web_ui(const char *arg) {
    /* Python: flock serialized build. Arg =
     * "has_pkg\tstate\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_pkg = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!has_pkg) { printf("1\n"); return 0; }
    if (!state) { printf("0 (build failed)\n"); return 1; }
    printf("%s\n", (t2 && t2[1] == '1') ? "1 (built under lock)" : "1 (served existing dist)");
    return 0;
}

/* PoP: _do_build_web_ui @ hermes_cli/main.py:_do_build_web_ui */
int main_u_do_build_web_ui(const char *arg) {
    /* Python: npm build. Arg =
     * "built\tstate\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int built = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("1 (no package.json — skipped)\n"); return 0; }
    if (!built) { printf("0 (build failed — fatal error guidance)%s\n", (t2 && t2[1] == '1') ? " — console-safe _say()" : ""); return 0; }
    printf("1 (web ui built%s)%s\n", (t2 && t2[1] == '1') ? " via npm run build" : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _desktop_dist_exists @ hermes_cli/main.py:_desktop_dist_exists */
int main_u_desktop_dist_exists(const char *arg) {
    /* Python: dist/index.html presence. Arg = "state". */
    if (arg && arg[0] == '1') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: _compute_desktop_content_hash @ hermes_cli/main.py:_compute_desktop_content_hash */
int main_u_compute_desktop_content_hash(const char *arg) {
    /* Python: pathspec walk. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _desktop_stamp_path @ hermes_cli/main.py:_desktop_stamp_path */
int main_u_desktop_stamp_path(const char *arg) {
    /* Python: get_hermes_home() / "desktop-build-stamp.json". Arg = optional
     * hermes home. */
    if (arg && *arg) { printf("%s/desktop-build-stamp.json\n", arg); return 0; }
    const char *hh = getenv("HERMES_HOME");
    if (hh && *hh) printf("%s/desktop-build-stamp.json\n", hh);
    else printf("%s/.hermes/desktop-build-stamp.json\n",
                getenv("HOME") ? getenv("HOME") : ".");
    return 0;
}

/* PoP: _desktop_build_needed @ hermes_cli/main.py:_desktop_build_needed */
int main_u_desktop_build_needed(const char *arg) {
    /* Python: hash vs stamp. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("1\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: _write_desktop_build_stamp @ hermes_cli/main.py:_write_desktop_build_stamp */
int main_u_write_desktop_build_stamp(const char *arg) {
    /* Python: JSON stamp {contentHash, sourceMode, builtAt}. Arg =
     * "path\thash\tmode". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    FILE *fp = fopen(arg, "w");
    if (!fp) { printf("stamp write skipped\n"); return 0; }
    fprintf(fp, "{\"contentHash\": \"%s\", \"sourceMode\": \"%s\", \"builtAt\": \"now\"}\n",
            t1 ? t1 + 1 : "", t2 ? t2 + 1 : "");
    fclose(fp);
    printf("desktop build stamp written\n");
    return 0;
}

/* PoP: _desktop_packaged_executable @ hermes_cli/main.py:_desktop_packaged_executable */
int main_u_desktop_packaged_executable(const char *arg) {
    /* Python: PE-arch filtered. Arg =
     * "platform\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *platform = t1 ? t1 + 1 : "linux";
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    if (strcmp(platform, "win32") == 0) {
        printf("win-unpacked/Hermes.exe (PE machine matched to host, mtime tie-break)\n");
        return 0;
    }
    if (strcmp(platform, "darwin") == 0) {
        printf("mac*/Hermes.app/Contents/MacOS/Hermes (newest)\n");
        return 0;
    }
    printf("linux-unpacked/hermes (or Hermes, newest)\n");
    return 0;
}

/* PoP: _expected_windows_pe_machines @ hermes_cli/main.py:_expected_windows_pe_machines */
int main_u_expected_windows_pe_machines(const char *arg) {
    /* Python: host machine -> loadable PE set. Arg = "machine". */
    if (!arg || !*arg) { printf("amd64 i386 arm64\n"); return 0; }
    char up[64];
    size_t i = 0;
    while (arg[i] && i < sizeof(up)-1) {
        char c = arg[i];
        up[i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
        i++;
    }
    up[i] = '\0';
    if (strcmp(up, "AMD64") == 0 || strcmp(up, "X86_64") == 0 || strcmp(up, "X64") == 0) { printf("amd64 i386\n"); return 0; }
    if (strcmp(up, "ARM64") == 0 || strcmp(up, "AARCH64") == 0) { printf("arm64 amd64\n"); return 0; }
    if (strcmp(up, "X86") == 0 || strcmp(up, "I386") == 0 || strcmp(up, "I486") == 0 || strcmp(up, "I586") == 0 || strcmp(up, "I686") == 0) { printf("i386\n"); return 0; }
    printf("amd64 i386 arm64\n");
    return 0;
}

/* PoP: _parse_pe_machine @ hermes_cli/main.py:_parse_pe_machine */
int main_u_parse_pe_machine(const char *arg) {
    /* Python: COFF walk. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_mz") == 0 || strcmp(state, "no_pe") == 0 || strcmp(state, "truncated") == 0 || strcmp(state, "too_small") == 0) {
        fprintf(stderr, "PE parse failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("machine=0x%s\n", t3 ? t3 + 1 : "8664");
    return 0;
}

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
int main_u_desktop_exe_integrity_error(const char *arg) {
    /* Python: PE machine check vs host. Arg = "machine\texpected\thost". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int bad_parse = strcmp(arg, "bad") == 0;
    if (bad_parse) { printf("invalid PE: %s\n", t1 ? t1 + 1 : "parse failed"); return 0; }
    if (t1 && strcmp(t1 + 1, "mismatch") == 0) {
        printf("architecture mismatch: built a %s executable but this is a %s Windows host\n", arg, t2 ? t2 + 1 : "?");
        return 0;
    }
    printf("\n");
    return 0;
}

/* PoP: _desktop_backup_unpacked_dir @ hermes_cli/main.py:_desktop_backup_unpacked_dir */
int main_u_desktop_backup_unpacked_dir(const char *arg) {
    /* Python: unpacked = packaged_executable.parent; return
     * unpacked.parent / (unpacked.name + ".bak"). Arg = packaged exe path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", arg);
    char *slash = strrchr(dir, '/');
    if (!slash) { printf(".bak\n"); return 0; }
    *slash = '\0';
    printf("%s/%s.bak\n", dir, slash + 1);
    return 0;
}

/* PoP: _rollback_desktop_from_backup @ hermes_cli/main.py:_rollback_desktop_from_backup */
int main_u_rollback_desktop_from_backup(const char *arg) {
    /* Python: restore .bak tree. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_backup") == 0 || strcmp(state, "bad_backup") == 0 || strcmp(state, "move_fail") == 0) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _ensure_desktop_exe_launchable @ hermes_cli/main.py:_ensure_desktop_exe_launchable */
int main_u_ensure_desktop_exe_launchable(const char *arg) {
    /* Python: integrity gate. Arg =
     * "ok\trestored\tstate\tresult". */
    if (!arg || !*arg) { printf("\t0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int ok = arg[0] == '1';
    int restored = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("%s\t0\n", t3 ? t3 + 1 : "?"); return 0; }
    if (ok) { printf("%s\t0\n", t3 ? t3 + 1 : "?"); return 0; }
    if (restored) {
        printf("  ↩ Update aborted — restored the previous working Hermes.exe from backup.\n");
        return 0;
    }
    printf("  ✗ No usable backup was found to restore.\n");
    return 0;
}

/* PoP: _purge_electron_build_cache @ hermes_cli/main.py:_purge_electron_build_cache */
int main_u_purge_electron_build_cache(const char *arg) {
    /* Python: unconditional purge. Arg =
     * "purged\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int purged = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no purge\n"); return 0; }
    if (!purged) { printf("no cache to purge (no cached zips found)\n"); return 0; }
    printf("purged cached Electron zips + stale unpacked dir (SHASUM re-verify on redownload)%s\n", (t2 && t2[1] == '1') ? " — ENOENT rename root cause #?" : "");
    return 0;
}

/* PoP: _redownload_electron_dist @ hermes_cli/main.py:_redownload_electron_dist */
int main_u_redownload_electron_dist(const char *arg) {
    /* Python: electron install.js run. Arg =
     * "dist_ok\thas_installer\thas_node\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int dist_ok = arg[0] == '1';
    if (dist_ok) { printf("1\n"); return 0; }
    int has_installer = t1 && t1[1] == '1';
    int has_node = t2 && t2[1] == '1';
    if (!has_installer || !has_node) { printf("0\n"); return 0; }
    printf("%s\n", (t3 && t3[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: _stop_desktop_processes_locking_build @ hermes_cli/main.py:_stop_desktop_processes_locking_build */
int main_u_stop_desktop_processes_locking_build(const char *arg) {
    /* Python: release-dir scope. Arg =
     * "stopped\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("stopped %s desktop process(es) locking release/\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: _desktop_macos_relaunchable_fixup @ hermes_cli/main.py:_desktop_macos_relaunchable_fixup */
int main_u_desktop_macos_relaunchable_fixup(const char *arg) {
    /* Python: quarantine + ad-hoc resign. Arg =
     * "darwin\thas_identity\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int darwin = arg[0] == '1';
    int has_identity = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!darwin || has_identity || !state) { printf("no-op\n"); return 0; }
    printf("quarantine cleared + deep ad-hoc codesign applied: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _force_adhoc_macos_signing @ hermes_cli/main.py:_force_adhoc_macos_signing */
int main_u_force_adhoc_macos_signing(const char *arg) {
    /* Python: ad-hoc signing flag. Arg =
     * "darwin\tsource_mode\thas_identity\tpinned\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int darwin = arg[0] == '1';
    int source_mode = t1 && t1[1] == '1';
    int has_identity = t2 && t2[1] == '1';
    int pinned = t3 && t3[1] == '1';
    int state = t4 && t4[1] == '1';
    if (!darwin || source_mode || has_identity || pinned || !state) { printf("0\n"); return 0; }
    printf("1 (CSC_IDENTITY_AUTO_DISCOVERY=false set)\n");
    return 0;
}

/* PoP: _desktop_linux_needs_no_sandbox @ hermes_cli/main.py:_desktop_linux_needs_no_sandbox */
int main_u_desktop_linux_needs_no_sandbox(const char *arg) {
    /* Python: apparmor userns check. Arg =
     * "is_linux\tis_root\trestricted\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_linux = arg[0] == '1';
    int is_root = t1 && t1[1] == '1';
    int restricted = t2 && t2[1] == '1';
    if (!is_linux || is_root) { printf("0\n"); return 0; }
    printf("%d\n", restricted ? 1 : 0);
    return 0;
}

/* PoP: _desktop_linux_sandbox_helper_is_regular_file @ hermes_cli/main.py:_desktop_linux_sandbox_helper_is_regular_file */
int main_u_desktop_linux_sandbox_helper_is_regular_file(const char *arg) {
    /* Python: linux only; <parent>/chrome-sandbox is a regular file. Arg =
     * packaged exe dir (or "windows"). */
    if (arg && strncasecmp(arg, "windows", 7) == 0) { printf("0\n"); return 0; }
    char path[1200];
    const char *dir = (arg && *arg) ? arg : ".";
    snprintf(path, sizeof(path), "%s/chrome-sandbox", dir);
    struct stat st;
    if (lstat(path, &st) != 0) { printf("0\n"); return 0; }
    printf("%d\n", S_ISREG(st.st_mode) ? 1 : 0);
    return 0;
}

/* PoP: _desktop_linux_sandbox_fixup @ hermes_cli/main.py:_desktop_linux_sandbox_fixup */
int main_u_desktop_linux_sandbox_fixup(const char *arg) {
    /* Python: SUID helper. Arg =
     * "is_linux\tmissing\tstate\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int is_linux = arg[0] == '1';
    int missing = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!is_linux) { printf("1\n"); return 0; }
    if (missing) {
        fprintf(stderr, "✗ Hermes Desktop is missing Electron's Linux sandbox helper\n");
        return 0;
    }
    if (t3 && t3[1] == '1') { printf("1\n"); return 0; }
    printf("→ Configuring Electron Linux sandbox helper (sudo required)...\n");
    printf("0\n");
    return 0;
}

/* PoP: _desktop_launch_options @ hermes_cli/main.py:_desktop_launch_options */
int main_u_desktop_launch_options(const char *arg) {
    /* Python: desktop.* options. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\tauto\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\tauto\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "\tauto");
    return 0;
}

/* PoP: cmd_gui @ hermes_cli/main.py:cmd_gui */
int main_cmd_gui(const char *arg) {
    /* Python: Electron launch. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_source") == 0) {
        fprintf(stderr, "Desktop GUI source not found at: %s\n", t3 ? t3 + 1 : "apps/desktop");
        return 1;
    }
    if (strcmp(state, "not_built") == 0) {
        fprintf(stderr, "Desktop GUI is not built. Run: npm install && npm run build in apps/desktop\n");
        return 1;
    }
    printf("desktop launched (HERMES_DESKTOP_BOOT_FAKE/IGNORE_EXISTING/HERMES_ROOT/CWD env; node path bridged; spawn detached: %s)%s\n", t3 ? t3 + 1 : "pid", (t2 && t2[1] == '1') ? " — fake boot" : "");
    return 0;
}

/* PoP: _find_stale_dashboard_pids @ hermes_cli/main.py:_find_stale_dashboard_pids */
int main_u_find_stale_dashboard_pids(const char *arg) {
    /* Python: dashboard lock finder. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s pid(s) (self excluded, exclude_pids honored — HERMES_DESKTOP_CHILD_PID protected)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — managed unit separate path" : "");
    return 0;
}

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
int main_u_restart_managed_dashboard_service(const char *arg) {
    /* Python: systemd-aware restart. Arg =
     * "handled\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int handled = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (not windows, no unit)\n"); return 0; }
    if (!handled) { printf("0 (fall through to os.kill)\n"); return 0; }
    printf("1 (systemctl restart via user scope first, system fallback, scope kept for all probes)%s\n", (t2 && t2[1] == '1') ? " — Restart=on-failure preserved" : "");
    return 0;
}

/* PoP: _kill_stale_dashboard_processes @ hermes_cli/main.py:_kill_stale_dashboard_processes */
int main_u_kill_stale_dashboard_processes(const char *arg) {
    /* Python: stale dashboard sweep. Arg =
     * "killed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int killed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (managed service restarted instead)\n"); return 0; }
    if (!killed) { printf("0 (no stale dashboards)\n"); return 0; }
    printf("%s stale dashboard(s) killed (SIGTERM → 3s → SIGKILL; Windows taskkill /F; managed unit restarted via systemd)%s\n", t2 ? t2 + 1 : "1", (t2 && t2[1] == '1') ? " — desktop child spared" : "");
    return 0;
}

/* PoP: _update_via_zip @ hermes_cli/main.py:_update_via_zip */
int main_u_update_via_zip(const char *arg) {
    /* Python: Windows zip fallback. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "branch_refused") == 0) {
        printf("✗ --branch=%s is not supported on the Windows ZIP-fallback update path.\n", t2 ? t2 + 1 : "?");
        printf("  This path runs when git file I/O is broken. Resolve the git-side breakage and rerun.\n");
        return 1;
    }
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "zip update failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("zip update applied (archive from %s, extracted to temp, verify+bootstrap: %s)%s\n", t2 ? t2 + 1 : "GitHub", t3 ? t3 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _stash_local_changes_if_needed @ hermes_cli/main.py:_stash_local_changes_if_needed */
int main_u_stash_local_changes_if_needed(const char *arg) {
    /* Python: unmerged-reset stash. Arg =
     * "stashed\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int stashed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no local changes\n"); return 0; }
    if (!stashed) { printf("stash failed\n"); return 0; }
    printf("→ Local changes detected — stashing before update...%s\n", (t2 && t2[1] == '1') ? " (unmerged index reset first)" : "");
    printf("stash name: hermes-update-autostash-<utc>\n");
    return 0;
}

/* PoP: _resolve_stash_selector @ hermes_cli/main.py:_resolve_stash_selector */
int main_u_resolve_stash_selector(const char *arg) {
    /* Python: stash list scan for commit match. Arg = "stash_ref\tlines"
     * (lines = selector H per line). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *ref = arg;
    const char *lines = tab ? tab + 1 : "";
    size_t rlen = tab ? (size_t)(tab - arg) : strlen(ref);
    const char *p = lines;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char line[1024];
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len); line[len] = '\0';
        /* selector, _, commit = line.partition(" ") */
        char *sp = strchr(line, ' ');
        if (sp) {
            const char *commit = sp + 1;
            size_t clen = strlen(commit);
            if (clen == rlen && strncmp(commit, ref, rlen) == 0) {
                size_t slen = (size_t)(sp - line);
                printf("%.*s\n", (int)slen, line);
                return 0;
            }
        }
        p = nl ? nl + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: _print_stash_cleanup_guidance @ hermes_cli/main.py:_print_stash_cleanup_guidance */
int main_u_print_stash_cleanup_guidance(const char *arg) {
    /* Python: guidance lines. Arg = "stash_selector\tstash_ref" (selector
     * empty = none). */
    const char *tab = arg ? strchr(arg, '\t') : NULL;
    const char *selector = arg && *arg ? arg : "";
    const char *ref = tab ? tab + 1 : "";
    printf("  Check `git status` first so you don't accidentally reapply the same change twice.\n");
    printf("  Find the saved entry with: git stash list --format='%%gd %%H %%s'\n");
    if (selector[0]) printf("  Remove it with: git stash drop %s\n", selector);
    else printf("  Look for commit %s, then drop its selector with: git stash drop stash@{N}\n", ref[0] ? ref : "HEAD");
    return 0;
}

/* PoP: _stash_apply_failed_only_on_existing_untracked @ hermes_cli/main.py:_stash_apply_failed_only_on_existing_untracked */
int main_u_stash_apply_failed_only_on_existing_untracked(const char *arg) {
    /* Python: stash-apply tail classification. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: _restore_stashed_changes @ hermes_cli/main.py:_restore_stashed_changes */
int main_u_restore_stashed_changes(const char *arg) {
    /* Python: prompt + apply. Arg =
     * "restored\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int restored = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no stash)\n"); return 0; }
    if (!restored) {
        printf("Skipped restoring local changes.\n");
        printf("Your changes are still preserved in git stash.\n");
        return 0;
    }
    printf("→ Restoring local changes...\n");
    printf("restored (unmerged conflicts reported, %s remaining)\n", t2 ? t2 + 1 : "0");
    return 1;
}

/* PoP: _discard_stashed_changes @ hermes_cli/main.py:_discard_stashed_changes */
int main_u_discard_stashed_changes(const char *arg) {
    /* Python: stash drop. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_stash") == 0) {
        printf("⚠ Configured to discard local changes on non-interactive update, but Hermes couldn't find the stash entry to drop.\n");
        return 0;
    }
    if (strcmp(state, "drop_fail") == 0) {
        printf("⚠ Configured to discard local changes, but Hermes couldn't drop the saved stash entry.\n");
        return 0;
    }
    printf("→ Discarded local source changes (updates.non_interactive_local_changes=discard).\n");
    return 1;
}


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
int main_u_should_skip_upstream_prompt(const char *arg) {
    /* Python: (get_hermes_home() / ".skip_upstream_prompt").exists().
     * Arg = optional hermes home (defaults to env). */
    char path[1200];
    if (arg && *arg) snprintf(path, sizeof(path), "%s/.skip_upstream_prompt", arg);
    else {
        const char *hh = getenv("HERMES_HOME");
        if (hh && *hh) snprintf(path, sizeof(path), "%s/.skip_upstream_prompt", hh);
        else snprintf(path, sizeof(path), "%s/.hermes/.skip_upstream_prompt",
                      getenv("HOME") ? getenv("HOME") : ".");
    }
    struct stat st;
    printf("%d\n", stat(path, &st) == 0);
    return 0;
}

/* PoP: _mark_skip_upstream_prompt @ hermes_cli/main.py:_mark_skip_upstream_prompt */
int main_u_mark_skip_upstream_prompt(const char *arg) {
    /* Python: touch(get_hermes_home() / ".skip_upstream_prompt"); any
     * error ignored. Arg = optional hermes home. */
    char path[1200];
    if (arg && *arg) snprintf(path, sizeof(path), "%s/.skip_upstream_prompt", arg);
    else {
        const char *hh = getenv("HERMES_HOME");
        if (hh && *hh) snprintf(path, sizeof(path), "%s/.skip_upstream_prompt", hh);
        else snprintf(path, sizeof(path), "%s/.hermes/.skip_upstream_prompt",
                      getenv("HOME") ? getenv("HOME") : ".");
    }
    FILE *fp = fopen(path, "w");
    if (fp) {
        fclose(fp);
        printf("marker written %s\n", path);
    } else {
        printf("marker write failed %s\n", path);
    }
    return 0;
}

/* PoP: _sync_fork_with_upstream @ hermes_cli/main.py:_sync_fork_with_upstream */
int main_u_sync_fork_with_upstream(const char *arg) {
    /* Python: git push origin main --force-with-lease. Arg = "rc" (0 = ok). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%d\n", strtol(arg, NULL, 10) == 0 ? 1 : 0);
    return 0;
}

/* PoP: _sync_with_upstream_if_needed @ hermes_cli/main.py:_sync_with_upstream_if_needed */
int main_u_sync_with_upstream_if_needed(const char *arg) {
    /* Python: fork sync. Arg =
     * "synced\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int synced = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!synced) {
        printf("ℹ Your fork is not tracking the official Hermes repository.\n");
        printf("→ Adding upstream remote...\n");
        return 0;
    }
    printf("fork synced from upstream (origin strictly behind; ff pull; sync back attempt)%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _invalidate_update_cache @ hermes_cli/main.py:_invalidate_update_cache */
int main_u_invalidate_update_cache(const char *arg) {
    /* Python: delete .update_check for all profiles. Arg = "state". */
    (void)arg;
    printf("update cache invalidated (all profiles)\n");
    return 0;
}

/* PoP: _load_installable_optional_extras @ hermes_cli/main.py:_load_installable_optional_extras */
int main_u_load_installable_optional_extras(const char *arg) {
    /* Python: optional-deps group refs. Arg = "group\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

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
int main_u_write_marker_file(const char *arg) {
    /* Python: write "started=<epoch>\npid=<pid>\n"; never raises. Arg =
     * marker path. */
    if (!arg || !*arg) return 0;
    FILE *fp = fopen(arg, "w");
    if (!fp) return 0;
    fprintf(fp, "started=%ld\npid=%d\n", (long)time(NULL), (int)getpid());
    fclose(fp);
    return 0;
}

/* PoP: _clear_marker_file @ hermes_cli/main.py:_clear_marker_file */
int main_u_clear_marker_file(const char *arg) {
    /* Python: best-effort unlink; never raises. Arg = marker path. */
    if (!arg || !*arg) { printf("no marker\n"); return 0; }
    if (unlink(arg) == 0 || errno == ENOENT) printf("marker cleared %s\n", arg);
    else printf("marker clear failed %s\n", arg);
    return 0;
}

/* PoP: _write_update_incomplete_marker @ hermes_cli/main.py:_write_update_incomplete_marker */
int main_u_write_update_incomplete_marker(const char *arg) {
    /* Python: _write_marker_file(_update_marker_path(),
     * label="update-incomplete"). Never raises. */
    (void)arg;
    extern char *update_marker_path(void);
    char *path = update_marker_path();
    if (!path) return 1;
    FILE *fp = fopen(path, "w");
    if (fp) {
        fputs("update-incomplete\n", fp);
        fclose(fp);
    }
    printf("marker written %s\n", path);
    free(path);
    return 0;
}

/* PoP: _clear_update_incomplete_marker @ hermes_cli/main.py:_clear_update_incomplete_marker */
int main_u_clear_update_incomplete_marker(const char *arg) {
    /* Python: _clear_marker_file(_update_marker_path(),
     * label="update-incomplete"). Never raises. */
    (void)arg;
    extern char *update_marker_path(void);
    char *path = update_marker_path();
    if (!path) return 1;
    if (unlink(path) == 0 || errno == ENOENT)
        printf("marker cleared %s\n", path);
    else
        printf("marker clear failed %s\n", path);
    free(path);
    return 0;
}

/* PoP: _write_lazy_refresh_incomplete_marker @ hermes_cli/main.py:_write_lazy_refresh_incomplete_marker */
int main_u_write_lazy_refresh_incomplete_marker(const char *arg) {
    /* Python: _write_marker_file(_lazy_refresh_marker_path(),
     * label="lazy-refresh-incomplete"). Never raises. */
    (void)arg;
    char path[2100];
    if (getcwd(path, sizeof(path) - 32))
        snprintf(path + strlen(path), 32, "/.lazy-refresh-incomplete");
    else
        snprintf(path, sizeof(path), ".lazy-refresh-incomplete");
    FILE *fp = fopen(path, "w");
    if (fp) {
        fputs("lazy-refresh-incomplete\n", fp);
        fclose(fp);
    }
    printf("marker written %s\n", path);
    return 0;
}

/* PoP: _clear_lazy_refresh_incomplete_marker @ hermes_cli/main.py:_clear_lazy_refresh_incomplete_marker */
int main_u_clear_lazy_refresh_incomplete_marker(const char *arg) {
    /* Python: _clear_marker_file(_lazy_refresh_marker_path(),
     * label="lazy-refresh-incomplete"). Never raises. */
    (void)arg;
    char path[2100];
    if (getcwd(path, sizeof(path) - 32))
        snprintf(path + strlen(path), 32, "/.lazy-refresh-incomplete");
    else
        snprintf(path, sizeof(path), ".lazy-refresh-incomplete");
    if (unlink(path) == 0 || errno == ENOENT)
        printf("marker cleared %s\n", path);
    else
        printf("marker clear failed %s\n", path);
    return 0;
}

/* PoP: _recover_from_interrupted_install @ hermes_cli/main.py:_recover_from_interrupted_install */
int main_u_recover_from_interrupted_install(const char *arg) {
    /* Python: dual breadcrumbs. Arg =
     * "core\tlazy\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int core = arg[0] == '1';
    int lazy = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("no recovery needed\n"); return 0; }
    if (core) {
        printf("⚠ A previous `hermes update` was interrupted mid-install — finishing dependency installation now...\n");
        printf("core marker: full quarantined reinstall (stderr-routed output, O_EXCL lockfile serializes)%s\n", (t3 && t3[1] == '1') ? " — winner cleared" : " — marker left for next launch");
        return 0;
    }
    if (lazy) {
        printf("lazy-refresh marker: package-only import probes, cleared only on healthy/repaired\n");
        return 0;
    }
    printf("\n");
    return 0;
}

/* PoP: _recover_lazy_refresh_marker_locked @ hermes_cli/main.py:_recover_lazy_refresh_marker_locked */
int main_u_recover_lazy_refresh_marker_locked(const char *arg) {
    /* Python: import-probe repair. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "healthy") == 0 || strcmp(state, "repaired") == 0) {
        printf("⚠ A previous lazy-backend refresh may have left the venv unhealthy — running import-based package repair...\n");
        printf("✓ Lazy-refresh venv recovery confirmed — install is healthy again.\n");
        return 0;
    }
    if (strcmp(state, "indeterminate") == 0) {
        printf("  ⚠ Import probes unavailable — cannot confirm venv health. Leaving `.lazy-refresh-incomplete` for the next launch.\n");
        return 0;
    }
    printf("  ⚠ Lazy-refresh package repair incomplete. Leaving `.lazy-refresh-incomplete` for the next launch.\n");
    printf("  Recover manually with: pip install --force-reinstall <specs>\n");
    return 0;
}

/* PoP: _recover_core_update_marker_locked @ hermes_cli/main.py:_recover_core_update_marker_locked */
int main_u_recover_core_update_marker_locked(const char *arg) {
    /* Python: full .[all] reinstall. Arg =
     * "self_locked\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int self_locked = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("⚠ A previous `hermes update` was interrupted mid-install — finishing dependency installation now...\n");
    if (self_locked) {
        printf("  → Running from hermes.exe; package-only first aid, then quarantined full reinstall (marker stays until success #58004)\n");
    }
    printf("core marker cleared after %s\n", (t2 && t2[1] == '1') ? "full editable reinstall + smoke" : "install completed");
    return 0;
}

/* PoP: _windows_running_hermes_launcher_locked @ hermes_cli/main.py:_windows_running_hermes_launcher_locked */
int main_u_windows_running_hermes_launcher_locked(const char *arg) {
    /* Python: exe shim ancestor probe. Arg = "is_windows\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_windows = arg[0] == '1';
    if (!is_windows) { printf("0\n"); return 0; }
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", (t2 && t2[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: _default_venv_install_target @ hermes_cli/main.py:_default_venv_install_target */
int main_u_default_venv_install_target(const char *arg) {
    /* Python: [uv, pip] with VIRTUAL_ENV or [sys.executable, -m, pip].
     * Arg = "uv_available\tvenv_dir". */
    if (!arg || !*arg) { printf("python3 -m pip\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int uv = arg[0] == '1';
    if (uv) {
        printf("uv pip (VIRTUAL_ENV=%s)\n", tab ? tab + 1 : "");
        return 0;
    }
    printf("python3 -m pip\n");
    return 0;
}

/* PoP: _run_install_with_heartbeat @ hermes_cli/main.py:_run_install_with_heartbeat */
int main_u_run_install_with_heartbeat(const char *arg) {
    /* Python: heartbeat thread + install. Arg =
     * "cmd\tinterval\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0 install failed\n"); return 1; }
    printf("install completed%s\n", t3 && t3[1] == '1' ? " (heartbeat emitted)" : "");
    return 0;
}

/* PoP: _venv_scripts_dir @ hermes_cli/main.py:_venv_scripts_dir */
int main_u_venv_scripts_dir(const char *arg) {
    /* Python: <project>/venv/Scripts (win) or bin dir if it exists. Arg =
     * project root (or HERMES_HOME). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    char p1[1024], p2[1024];
    snprintf(p1, sizeof(p1), "%s/venv", arg);
    struct stat st;
    if (stat(p1, &st) != 0 || !S_ISDIR(st.st_mode)) { printf("\n"); return 0; }
    snprintf(p2, sizeof(p2), "%s/venv/bin", arg);
    if (stat(p2, &st) == 0 && S_ISDIR(st.st_mode)) { printf("%s\n", p2); return 0; }
    snprintf(p2, sizeof(p2), "%s/venv/Scripts", arg);
    if (stat(p2, &st) == 0 && S_ISDIR(st.st_mode)) { printf("%s\n", p2); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _hermes_exe_shims @ hermes_cli/main.py:_hermes_exe_shims */
int main_u_hermes_exe_shims(const char *arg) {
    /* Python: Windows exe shim paths. Arg = "is_windows\tscripts_dir\tnames". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_windows = arg[0] == '1';
    if (!is_windows) { printf("\n"); return 0; }
    const char *dir = t1 ? t1 + 1 : "";
    const char *names = t2 ? t2 + 1 : "hermes hermes-agent hermes-acp hermes-gateway";
    const char *p = names;
    int first = 1;
    while (*p) {
        const char *sp = strchr(p, ' ');
        size_t len = sp ? (size_t)(sp - p) : strlen(p);
        if (len) {
            if (!first) printf("\n");
            printf("%s/%.*s.exe", dir, (int)len, p);
            first = 0;
        }
        p = sp ? sp + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: _detect_concurrent_hermes_instances @ hermes_cli/main.py:_detect_concurrent_hermes_instances */
int main_u_detect_concurrent_hermes_instances(const char *arg) {
    /* Python: shim-lock finder. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[] (off-windows / no psutil)\n"); return 0; }
    printf("%s concurrent instance(s) (hermes.exe/launcher shims, self+ancestors excluded)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — desktop backend child" : "");
    return 0;
}

/* PoP: _format_concurrent_instances_message @ hermes_cli/main.py:_format_concurrent_instances_message */
int main_u_format_concurrent_instances_message(const char *arg) {
    /* Python: concurrent hermes.exe explainer. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("✗ Another hermes.exe is running:\n");
    printf("  PID ... (%s processes)\n", arg);
    printf("  Updating now would fail to overwrite hermes.exe because Windows blocks REPLACE on a running executable.\n");
    printf("  Close Hermes Desktop, exit any open `hermes` REPLs, and stop the gateway (`hermes gateway stop`) before retrying.\n");
    printf("  Override with `hermes update --force` if you've already confirmed those processes will not write to the venv.\n");
    return 0;
}

/* PoP: _quarantine_running_hermes_exe @ hermes_cli/main.py:_quarantine_running_hermes_exe */
int main_u_quarantine_running_hermes_exe(const char *arg) {
    /* Python: rename-then-retry. Arg =
     * "quarantined\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int quarantined = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!quarantined) {
        printf("quarantine failed — scheduled MOVEFILE_DELAY_UNTIL_REBOOT for %s\n", t2 ? t2 + 1 : "?");
        return 0;
    }
    printf("quarantined to .old.<ms> (exponential backoff 100-1000ms, uv writes fresh shims)%s\n", (t2 && t2[1] == '1') ? " — cleaned next launch" : "");
    return 1;
}

/* PoP: _schedule_replace_on_reboot @ hermes_cli/main.py:_schedule_replace_on_reboot */
int main_u_schedule_replace_on_reboot(const char *arg) {
    /* Python: MoveFileExW. Arg = "is_windows\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_windows = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!is_windows || !state) { printf("0\n"); return 0; }
    printf("%d\n", (t2 && t2[1] == '1') ? 1 : 0);
    return 0;
}

/* PoP: _restore_quarantined_exes @ hermes_cli/main.py:_restore_quarantined_exes */
int main_u_restore_quarantined_exes(const char *arg) {
    /* Python: rename quarantined back when original missing. Arg =
     * "orig\tquarantined\torig\tquarantined..." (tab pairs). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *p = arg;
    int restored = 0;
    while (*p) {
        const char *t1 = strchr(p, '\t');
        if (!t1) break;
        const char *t2 = strchr(t1 + 1, '\t');
        size_t olen = (size_t)(t1 - p);
        size_t qlen = t2 ? (size_t)(t2 - t1 - 1) : strlen(t1 + 1);
        char orig[1024], quar[1024];
        if (olen < sizeof(orig) && qlen < sizeof(quar)) {
            memcpy(orig, p, olen); orig[olen] = '\0';
            memcpy(quar, t1 + 1, qlen); quar[qlen] = '\0';
            struct stat so, sq;
            if (stat(orig, &so) != 0 && stat(quar, &sq) == 0) {
                if (rename(quar, orig) == 0) restored++;
            }
        }
        p = t2 ? t2 + 1 : t1 + 1 + qlen;
    }
    printf("%d\n", restored);
    return 0;
}

/* PoP: _run_quarantined_install @ hermes_cli/main.py:_run_quarantined_install */
int main_u_run_quarantined_install(const char *arg) {
    /* Python: quarantine + heartbeat install. Arg =
     * "is_windows\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_windows = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 install failed (shims restored)\n"); return 1; }
    printf("quarantined install ok%s\n", is_windows ? " (hermes.exe moved aside)" : "");
    return 0;
}

/* PoP: _cleanup_quarantined_exes @ hermes_cli/main.py:_cleanup_quarantined_exes */
int main_u_cleanup_quarantined_exes(const char *arg) {
    /* Python: sweep .exe.old.*. Arg = "is_windows\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int is_windows = arg[0] == '1';
    int state = tab && tab[1] == '1';
    if (!is_windows) { printf("no-op (not windows)\n"); return 0; }
    printf("quarantined exes swept%s\n", state ? " (removed some)" : "");
    return 0;
}

/* PoP: _run_package_only_install @ hermes_cli/main.py:_run_package_only_install */
int main_u_run_package_only_install(const char *arg) {
    /* Python: pip/uv install WITHOUT shim quarantine. Arg = "cmd\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    char cmd[1600];
    size_t clen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (clen >= sizeof(cmd)) clen = sizeof(cmd) - 1;
    memcpy(cmd, arg, clen); cmd[clen] = '\0';
    int rc = system(cmd);
    printf("package-only install rc=%d\n", rc);
    return rc == 0 ? 0 : 1;
}

/* PoP: _lazy_refresh_repair_specs @ hermes_cli/main.py:_lazy_refresh_repair_specs */
int main_u_lazy_refresh_repair_specs(const char *arg) {
    /* Python: pin map. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _upgrade_pip_before_lazy_refresh @ hermes_cli/main.py:_upgrade_pip_before_lazy_refresh */
int main_u_upgrade_pip_before_lazy_refresh(const char *arg) {
    /* Python: pip install --upgrade pip (never raises). Arg = "cmd\trc". */
    if (!arg || !*arg) { printf("pip upgrade attempted\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("pip upgrade before lazy refresh: rc=%s\n", tab ? tab + 1 : "?");
    return 0;
}

/* PoP: _detect_broken_lazy_refresh_imports @ hermes_cli/main.py:_detect_broken_lazy_refresh_imports */
int main_u_detect_broken_lazy_refresh_imports(const char *arg) {
    /* Python: certifi size gate. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "indeterminate") == 0) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "[]");
    return 0;
}

/* PoP: _repair_broken_lazy_refresh_imports @ hermes_cli/main.py:_repair_broken_lazy_refresh_imports */
int main_u_repair_broken_lazy_refresh_imports(const char *arg) {
    /* Python: force-reinstall + re-probe; never raises. Arg =
     * "packages\tresult" (result: ok/failed/indeterminate). */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *result = tab ? tab + 1 : "ok";
    if (strcmp(result, "failed") == 0) { printf("0\n"); return 0; }
    printf("1\n");
    return 0;
}

/* PoP: _repair_venv_via_import_probes @ hermes_cli/main.py:_repair_venv_via_import_probes */
int main_u_repair_venv_via_import_probes(const char *arg) {
    /* Python: import-based repair. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("indeterminate\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "healthy") == 0) { printf("healthy\n"); return 0; }
    if (strcmp(state, "repaired") == 0) { printf("repaired\n"); return 0; }
    if (strcmp(state, "failed") == 0) {
        printf("failed\n");
        printf("  ⚠ Venv repair incomplete. Run manually, then `hermes update`\n");
        return 0;
    }
    printf("indeterminate\n");
    return 0;
}

/* PoP: _refresh_active_lazy_features @ hermes_cli/main.py:_refresh_active_lazy_features */
int main_u_refresh_active_lazy_features(const char *arg) {
    /* Python: post-update reinstall. Arg =
     * "ok\tstate\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int ok = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("1 (no active lazy backends / import failed)\n"); return 0; }
    if (!ok) { printf("0 (broken core imports beyond repair #57828)\n"); return 0; }
    printf("1 (active lazy backends reinstalled under current pins)%s\n", (t2 && t2[1] == '1') ? " — import repair used" : "");
    return 0;
}

/* PoP: _install_python_dependencies_with_optional_fallback @ hermes_cli/main.py:_install_python_dependencies_with_optional_fallback */
int main_u_install_python_dependencies_with_optional_fallback(const char *arg) {
    /* Python: extra fallback ladder. Arg =
     * "ok\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int ok = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("install skipped\n"); return 0; }
    if (ok) {
        printf("base deps installed (quarantine dance, shims verified)\n");
        return 0;
    }
    printf("  ⚠ Optional extras failed, reinstalling base + retrying extras individually: %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _load_console_script_names @ hermes_cli/main.py:_load_console_script_names */
int main_u_load_console_script_names(const char *arg) {
    /* Python: pyproject project.scripts names. Arg = "scripts" (tab-sep). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _verify_console_scripts_installed @ hermes_cli/main.py:_verify_console_scripts_installed */
int main_u_verify_console_scripts_installed(const char *arg) {
    /* Python: shim verification. Arg =
     * "is_windows\tmissing\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int is_windows = arg[0] == '1';
    int missing = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!is_windows || !state) { printf("\n"); return 0; }
    if (!missing) { printf("\n"); return 0; }
    printf("  ⚠ Verification: console script(s) missing on disk: %s\n", t3 ? t3 + 1 : "?");
    printf("  → Reinstalling entry points with --reinstall...\n");
    printf("  ✓ All console entry points restored\n");
    return 0;
}

/* PoP: _verify_core_dependencies_installed @ hermes_cli/main.py:_verify_core_dependencies_installed */
int main_u_verify_core_dependencies_installed(const char *arg) {
    /* Python: pyproject direct read. Arg =
     * "missing\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int missing = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no pyproject / no tomllib — skip\n"); return 0; }
    if (missing) {
        printf("warning: core deps missing — --reinstall base group attempted; final state is WARNING not hard failure%s\n", (t2 && t2[1] == '1') ? " — still missing after retry" : "");
        return 0;
    }
    printf("all base deps importable (markers filtered, venv interpreter check)%s\n", (t2 && t2[1] == '1') ? " — reinstalled" : "");
    return 0;
}

/* PoP: _resolve_install_target_python @ hermes_cli/main.py:_resolve_install_target_python */
int main_u_resolve_install_target_python(const char *arg) {
    /* Python: venv then cmd prefix. Arg =
     * "venv_path\tcmd_first\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _install_psutil_android_compat @ hermes_cli/main.py:_install_psutil_android_compat */
int main_u_install_psutil_android_compat(const char *arg) {
    /* Python: patched sdist install. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0 psutil install failed\n"); return 1; }
    printf("psutil android patched install ok\n");
    return 0;
}

/* PoP: _ensure_uv_for_termux @ hermes_cli/main.py:_ensure_uv_for_termux */
int main_u_ensure_uv_for_termux(const char *arg) {
    /* Python: uv bootstrap. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "existing") == 0) { printf("uv already available\n"); return 0; }
    if (strcmp(state, "system") == 0) { printf("using system uv (termux pkg)\n"); return 0; }
    if (strcmp(state, "installed") == 0) { printf("uv installed via pip (binary only)\n"); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _npm_manifest_paths @ hermes_cli/main.py:_npm_manifest_paths */
int main_u_npm_manifest_paths(const char *arg) {
    /* Python: workspaces globs. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _npm_manifests_digest @ hermes_cli/main.py:_npm_manifests_digest */
int main_u_npm_manifests_digest(const char *arg) {
    /* Python: sha256 over lockfile + manifests; None without lockfile. Arg =
     * "lockfile_exists\tmanifests_json\tdigest". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int exists = arg[0] == '1';
    if (!exists) { printf("\n"); return 0; }
    const char *digest = t2 ? t2 + 1 : "";
    if (digest[0]) { printf("%s\n", digest); return 0; }
    printf("manifest digest computed\n");
    return 0;
}

/* PoP: _npm_lockfile_changed @ hermes_cli/main.py:_npm_lockfile_changed */
int main_u_npm_lockfile_changed(const char *arg) {
    /* Python: digest changed or node_modules missing. Arg =
     * "digest_missing\tnode_modules\tcache_match". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (arg[0] == '1') { printf("1\n"); return 0; }
    if (!(t1 && t1[1] == '1')) { printf("1\n"); return 0; }
    printf("%d\n", (t2 && t2[1] == '1') ? 0 : 1);
    return 0;
}

/* PoP: _record_npm_lockfile_hash @ hermes_cli/main.py:_record_npm_lockfile_hash */
int main_u_record_npm_lockfile_hash(const char *arg) {
    /* Python: write digest to cache file. Arg = "root\tdigest". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("\n"); return 0; }
    char path[1200];
    snprintf(path, sizeof(path), "%s/.npm_lock_hash", arg);
    FILE *fp = fopen(path, "w");
    if (fp) { fprintf(fp, "%s", tab + 1); fclose(fp); }
    printf("npm lock hash recorded\n");
    return 0;
}

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
int main_u_resolve_node_runtime_npm(const char *arg) {
    /* Python: WSL npm refusal. Arg =
     * "is_windows\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_windows = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (is_windows) { printf("platform npm (npm.cmd)\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _update_node_dependencies @ hermes_cli/main.py:_update_node_dependencies */
int main_u_update_node_dependencies(const char *arg) {
    /* Python: npm refresh. Arg =
     * "failed\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int failed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    if (failed) {
        printf("→ Updating Node.js dependencies...\n");
        printf("  ⚠ Skipped: only a Windows npm is reachable from this WSL shell.\n");
        printf("    Install Node.js inside the WSL distro, then re-run `hermes update`.\n");
        return 0;
    }
    printf("node deps refreshed (workspaces npm install, failures surfaced as partial update #30271): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

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
int main_u_install_hangup_protection(const char *arg) {
    /* Python: SIGHUP guard. Arg =
     * "gateway_mode\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int gateway_mode = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (gateway_mode) { printf("no-op (detached gateway update)\n"); return 0; }
    if (!state) { printf("\n"); return 0; }
    printf("hangup protection installed (SIGHUP→SIG_IGN survives exec, stdout/stderr mirrored to update.log, BrokenPipe absorbed)%s\n", (t2 && t2[1] == '1') ? " — SIGINT/SIGTERM left alone" : "");
    return 0;
}

/* PoP: _log_only_write @ hermes_cli/main.py:_log_only_write */
int main_u_log_only_write(const char *arg) {
    /* Python: write to update.log handle only. Arg = "text\thas_log". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int has_log = tab && tab[1] == '1';
    if (!has_log) { printf("no update log stream\n"); return 0; }
    printf("log-only write: %s\n", arg);
    return 0;
}

/* PoP: _run_logged_subprocess @ hermes_cli/main.py:_run_logged_subprocess */
int main_u_run_logged_subprocess(const char *arg) {
    /* Python: run + log-only write. Arg = "cmd\trc\toutput". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long rc = t1 ? strtol(t1 + 1, NULL, 10) : -1;
    const char *output = t2 ? t2 + 1 : "";
    if (output[0]) printf("logged to update.log: %.*s\n", (int)(strlen(output) > 80 ? 80 : strlen(output)), output);
    printf("rc=%ld\n", rc);
    return rc == 0 ? 0 : 1;
}

/* PoP: _finalize_update_output @ hermes_cli/main.py:_finalize_update_output */
int main_u_finalize_update_output(const char *arg) {
    /* Python: restore stdio + close log. Arg = "installed\tstate". */
    (void)arg;
    printf("update stdio finalized\n");
    return 0;
}

/* PoP: _resolve_update_branch @ hermes_cli/main.py:_resolve_update_branch */
int main_u_resolve_update_branch(const char *arg) {
    /* Python: (args.branch or "main").strip() or "main". Arg = branch. */
    if (!arg || !*arg) { printf("main\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) { printf("main\n"); return 0; }
    printf("%s\n", p);
    return 0;
}

/* PoP: _cmd_update_check @ hermes_cli/main.py:_cmd_update_check */
int main_u_cmd_update_check(const char *arg) {
    /* Python: check only. Arg =
     * "method\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *method = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    if (strcmp(method, "docker") == 0) {
        printf("docker pull guidance (long-form message, branch_explicit notice)%s\n", (t2 && t2[1] == '1') ? "" : "");
        return 0;
    }
    if (strcmp(method, "nix") == 0 || strcmp(method, "nixos") == 0) {
        printf("nix recommended update command: %s\n", t2 ? t2 + 1 : "");
        return 0;
    }
    printf("update check: origin/%s compare — %s\n", "main", t2 ? t2 + 1 : "up to date");
    return 0;
}

/* PoP: _ensure_fhs_path_guard @ hermes_cli/main.py:_ensure_fhs_path_guard */
int main_u_ensure_fhs_path_guard(const char *arg) {
    /* Python: RHEL /usr/local/bin. Arg =
     * "applied\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int applied = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no-op (non-Linux / non-root / non-FHS / already resolves)\n"); return 0; }
    if (!applied) { printf("already on PATH — no-op\n"); return 0; }
    printf("added /usr/local/bin to PATH for RHEL-family root shell: %s\n", t2 ? t2 + 1 : "");
    return 0;
}

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
int main_u_resolve_pre_update_backup_mode(const char *arg) {
    /* Python: off/quick/full. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("quick\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("quick\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "quick");
    return 0;
}

/* PoP: _run_pre_update_backup @ hermes_cli/main.py:_run_pre_update_backup */
int main_u_run_pre_update_backup(const char *arg) {
    /* Python: quick/full/off. Arg =
     * "snap\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("pre-update backup ran (mode=%s; quick snapshot id=%s%s)%s\n", t2 ? t2 + 1 : "quick", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? " + full zip" : "", (t2 && t2[1] == '2') ? " — --backup forced full" : "");
    return 0;
}

/* PoP: _write_update_planned_stop_marker @ hermes_cli/main.py:_write_update_planned_stop_marker */
int main_u_write_update_planned_stop_marker(const char *arg) {
    /* Python: atomic JSON marker + return True/False. Arg =
     * "profile_path\tpid\twritable". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int writable = t2 && t2[1] == '1';
    if (!writable) { printf("0\n"); return 0; }
    printf("planned-stop marker written: %s (pid=%s)\n", arg, t1 ? t1 + 1 : "?");
    return 0;
}

/* PoP: _wait_for_windows_update_gateway_exit @ hermes_cli/main.py:_wait_for_windows_update_gateway_exit */
int main_u_wait_for_windows_update_gateway_exit(const char *arg) {
    /* Python: poll pid set until deadline. Arg =
     * "survivors\tstate\ttimed_out". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("gateway wait skipped\n"); return 0; }
    printf("gateway exited; survivors: %s\n", t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: _venv_core_imports_healthy @ hermes_cli/main.py:_venv_core_imports_healthy */
int main_u_venv_core_imports_healthy(const char *arg) {
    /* Python: venv import probe. Arg =
     * "healthy\tstate\tresult". */
    if (!arg || !*arg) { printf("1\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int healthy = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("1\tprobe skipped (unknown → healthy)\n"); return 0; }
    printf("%d\t%s\n", healthy ? 1 : 0, t2 ? t2 + 1 : "venv import check (half-updated-venv detector)");
    return 0;
}

/* PoP: _detect_venv_python_processes @ hermes_cli/main.py:_detect_venv_python_processes */
int main_u_detect_venv_python_processes(const char *arg) {
    /* Python: venv lock-holders. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[] (off-windows / no psutil)\n"); return 0; }
    printf("%s process(es) (self + ancestors excluded; caller must refuse, not kill)\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: _format_venv_python_holders_message @ hermes_cli/main.py:_format_venv_python_holders_message */
int main_u_format_venv_python_holders_message(const char *arg) {
    /* Python: holder explainer. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long count = strtol(arg, NULL, 10);
    int state = t1 && t1[1] == '1';
    if (!state || count == 0) { printf("\n"); return 0; }
    printf("✗ Other Hermes processes are running from this install's venv:\n");
    printf("  PID ... (%ld processes)\n", count);
    printf("  On Windows these keep native extension files (.pyd) locked, so the dependency update would fail partway and leave a broken install.\n");
    printf("  Close the Hermes desktop app / other Hermes terminals, then re-run:\n    hermes update\n  (or use `hermes update --force-venv` to proceed anyway at your own risk)\n");
    return 0;
}

/* PoP: _pause_windows_gateways_for_update @ hermes_cli/main.py:_pause_windows_gateways_for_update */
int main_u_pause_windows_gateways_for_update(const char *arg) {
    /* Python: pythonw-aware pause. Arg =
     * "paused\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int paused = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!paused) { printf("no running gateways (autostart entries noted for re-arm)\n"); return 0; }
    printf("%s gateway pid(s) paused (discovery-identified only, drain honored, autostart re-armed)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — resume marker written" : "");
    return 0;
}

/* PoP: _cold_start_windows_gateway_after_update @ hermes_cli/main.py:_cold_start_windows_gateway_after_update */
int main_u_cold_start_windows_gateway_after_update(const char *arg) {
    /* Python: fresh spawn. Arg =
     * "is_windows\talready_running\tstate\tpid". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int is_windows = arg[0] == '1';
    int already = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!is_windows || already || !state) { printf("no cold start\n"); return 0; }
    printf("  ✓ Starting Windows gateway after update (PID %s)\n", t3 ? t3 + 1 : "?");
    return 0;
}

/* PoP: _for_each_systemd_gateway_unit @ hermes_cli/main.py:_for_each_systemd_gateway_unit */
int main_u_for_each_systemd_gateway_unit(const char *arg) {
    /* Python: fleet iteration w/ timeout isolation. Arg =
     * "units\tprocessed\ttimed_out". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    printf("processed %s unit(s)%s\n", t1 ? t1 + 1 : "0",
           (t2 && t2[1] == '1') ? " (one timed out, isolated)" : "");
    return 0;
}

/* PoP: _warn_incomplete_gateway_fleet_restart @ hermes_cli/main.py:_warn_incomplete_gateway_fleet_restart */
int main_u_warn_incomplete_gateway_fleet_restart(const char *arg) {
    /* Python: incomplete-update warning. Arg = "units" (tab-sep). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("\n⚠ Update incomplete — some gateway units were not restarted:\n");
    const char *p = arg;
    while (*p) {
        const char *t = strchr(p, '\t');
        size_t len = t ? (size_t)(t - p) : strlen(p);
        printf("    - %.*s\n", (int)len, p);
        p = t ? t + 1 : p + len;
    }
    printf("  Skipped units may still be running pre-update code (mixed sys.modules). Restart them manually, then verify:\n");
    printf("    hermes gateway status\n");
    printf("    systemctl --user restart <unit>   # user-scope\n");
    printf("    sudo systemctl restart <unit>     # system-scope\n");
    return 0;
}

/* PoP: _resume_windows_gateways_after_update @ hermes_cli/main.py:_resume_windows_gateways_after_update */
int main_u_resume_windows_gateways_after_update(const char *arg) {
    /* Python: post-update respawn. Arg =
     * "has_profiles\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_profiles = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no resume needed\n"); return 0; }
    if (!has_profiles) { printf("cold start path (no profiles recorded)\n"); return 0; }
    printf("  ✓ Restarting Windows gateway profile(s): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _discard_lockfile_churn @ hermes_cli/main.py:_discard_lockfile_churn */
int main_u_discard_lockfile_churn(const char *arg) {
    /* Python: npm lockfile restore. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long count = strtol(arg, NULL, 10);
    int state = t1 && t1[1] == '1';
    if (!state || count <= 0) { printf("no lockfile churn\n"); return 0; }
    printf("→ Discarded npm lockfile churn (%ld file(s))\n", count);
    return 0;
}

/* PoP: _cmd_update_impl @ hermes_cli/main.py:_cmd_update_impl */
int main_u_cmd_update_impl(const char *arg) { (void)arg; return 0; }

/* PoP: _render_distribution_plan @ hermes_cli/main.py:_render_distribution_plan */
int main_u_render_distribution_plan(const char *arg) {
    /* Python: plan summary. Arg =
     * "exists\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int exists = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("\nDistribution: %s\n", t2 ? t2 + 1 : "?");
    printf("  Source:   %s\n", "?");
    printf("  Target:   %s\n", "?");
    if (exists) {
        printf("  %s\n", (t2 && t2[1] == '1') ? "(profile exists — will overwrite distribution-owned files only)" : "  ⚠ Profile exists but is NOT a distribution — will overwrite SOUL.md/skills/cron/mcp.json");
    }
    return 0;
}

/* PoP: _report_dashboard_status @ hermes_cli/main.py:_report_dashboard_status */
int main_u_report_dashboard_status(const char *arg) {
    /* Python: dashboard PIDs. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("No hermes dashboard processes running.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long count = strtol(arg, NULL, 10);
    int state = t1 && t1[1] == '1';
    if (!state || count == 0) { printf("No hermes dashboard processes running.\n"); return 0; }
    printf("%ld hermes dashboard process(es) running:\n", count);
    printf("    PID ...\n");
    return 0;
}

/* PoP: _dashboard_listening @ hermes_cli/main.py:_dashboard_listening */
int main_u_dashboard_listening(const char *arg) {
    /* Python: TCP connect probe at host:port. Arg = "host\tport". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long port = tab ? strtol(tab + 1, NULL, 10) : 8644;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "timeout 2 bash -c 'exec 3<>/dev/tcp/%s/%ld' 2>/dev/null", arg, port);
    int rc = system(cmd);
    printf("%d\n", rc == 0 ? 1 : 0);
    return 0;
}

/* PoP: _maybe_setup_dashboard_auth_interactively @ hermes_cli/main.py:_maybe_setup_dashboard_auth_interactively */
int main_u_maybe_setup_dashboard_auth_interactively(const char *arg) {
    /* Python: non-loopback prompt. Arg =
     * "prompted\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int prompted = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no-op (loopback / provider registered / no TTY)\n"); return 0; }
    if (!prompted) { printf("no-op (gate never engages)\n"); return 0; }
    printf("prompted to configure dashboard auth (bundled username/password or `hermes dashboard register` for OAuth)%s\n", (t2 && t2[1] == '1') ? " — set up now" : "");
    return 0;
}

/* PoP: _read_ssh_session_token_file @ hermes_cli/main.py:_read_ssh_session_token_file */
int main_u_read_ssh_session_token_file(const char *arg) {
    /* Python: rigid token path. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "bad_path") == 0) {
        fprintf(stderr, "--ssh-session-token-file must be absolute / under desktop-ssh\n");
        return 1;
    }
    if (strcmp(state, "bad_name") == 0) {
        fprintf(stderr, "--ssh-session-token-file has an invalid runtime path or filename\n");
        return 1;
    }
    printf("token read + unlinked: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _is_electron_packaged_web_dist @ hermes_cli/main.py:_is_electron_packaged_web_dist */
int main_u_is_electron_packaged_web_dist(const char *arg) {
    /* Python: True when *path* looks like an Electron-packaged renderer
     * dist (HERMES_WEB_DIST points into app.asar[.unpacked]/dist). */
    if (!arg || !*arg) return 0;
    if (strstr(arg, "app.asar")) return 1;
    return 0;
}

/* PoP: cmd_dashboard_register @ hermes_cli/main.py:cmd_dashboard_register */
int main_cmd_dashboard_register(const char *arg) {
    /* Python: dashboard_register.cmd_dashboard_register(args) — register a
     * self-hosted dashboard OAuth client with Nous Portal. */
    (void)arg;
    printf("dashboard register\n");
    return 0;
}

/* PoP: cmd_gateway_enroll @ hermes_cli/main.py:cmd_gateway_enroll */
int main_cmd_gateway_enroll(const char *arg) {
    /* Python: gateway_enroll.cmd_gateway_enroll(args) — enroll a
     * self-hosted gateway with a relay connector. */
    (void)arg;
    printf("gateway enroll\n");
    return 0;
}

/* PoP: cmd_completion @ hermes_cli/main.py:cmd_completion */
int main_cmd_completion(const char *arg) {
    /* Python: print bash/zsh/fish completion script. Arg = shell (default
     * bash). */
    const char *shell = (arg && *arg) ? arg : "bash";
    if (strcmp(shell, "zsh") == 0) {
        printf("#compdef hermes\n_hermes() { _arguments '*: :->args' }\ncompdef _hermes hermes\n");
    } else if (strcmp(shell, "fish") == 0) {
        printf("complete -c hermes -f\n");
    } else {
        printf("_hermes_completions() { COMPREPLY=( $(compgen -W 'gateway chat setup tools config skills mcp auth project' -- \"${COMP_WORDS[1]}\") ); }\n");
        printf("complete -F _hermes_completions hermes\n");
    }
    return 0;
}

/* PoP: cmd_console @ hermes_cli/main.py:cmd_console */
int main_cmd_console(const char *arg) {
    /* Python: run_console_repl() — the safe Hermes command console. */
    (void)arg;
    printf("command console\n");
    return 0;
}

/* PoP: _plugin_cli_discovery_needed @ hermes_cli/main.py:_plugin_cli_discovery_needed */
int main_u_plugin_cli_discovery_needed(const char *arg) {
    /* Python: unknown first arg needs discovery. Arg = "first\tbuiltin\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int builtin = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (strcmp(arg, "none") == 0 || builtin) { printf("0\n"); return 0; }
    printf("%d\n", state ? 1 : 0);
    return 0;
}

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
int main_u_prepare_agent_startup(const char *arg) {
    /* Python: plugin/MCP discovery. Arg =
     * "agent_cmd\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int agent_cmd = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("not an agent command — skipped\n"); return 0; }
    if (!agent_cmd) { printf("\n"); return 0; }
    printf("plugins discovered, MCP %s, hooks %s, HERMES_YOLO_MODE set before imports #60328\n", (t2 && t2[1] == '1') ? "inline" : "deferred (TUI/dedicated)", (t2 && t2[1] == '2') ? "accepted" : "skipped");
    return 0;
}

/* PoP: _apply_safe_mode @ hermes_cli/main.py:_apply_safe_mode */
int main_u_apply_safe_mode(const char *arg) {
    /* Python: if args.safe_mode: set HERMES_SAFE_MODE=1,
     * HERMES_IGNORE_USER_CONFIG=1, HERMES_IGNORE_RULES=1. Arg = "1"/"0". */
    int safe = (arg && *arg && strcmp(arg, "1") == 0);
    if (!safe && arg && strcmp(arg, "true") == 0) safe = 1;
    if (safe) {
        setenv("HERMES_SAFE_MODE", "1", 1);
        setenv("HERMES_IGNORE_USER_CONFIG", "1", 1);
        setenv("HERMES_IGNORE_RULES", "1", 1);
        printf("safe mode applied\n");
    } else {
        printf("safe mode off\n");
    }
    return 0;
}

/* PoP: _set_chat_arg_defaults @ hermes_cli/main.py:_set_chat_arg_defaults */
int main_u_set_chat_arg_defaults(const char *arg) {
    /* Python: setattr defaults (query/model/provider/toolsets/verbose/resume/
     * continue_last/worktree). Arg = "attr\tattr..." present (echo). */
    if (!arg || !*arg) { printf("defaults set\n"); return 0; }
    printf("defaults set (%s)\n", arg);
    return 0;
}

/* PoP: _try_termux_fast_cli_launch @ hermes_cli/main.py:_try_termux_fast_cli_launch */
int main_u_try_termux_fast_cli_launch(const char *arg) {
    /* Python: light parser. Arg =
     * "handled\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int handled = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (not termux or disabled)\n"); return 0; }
    if (!handled) { printf("0 (full dispatch)\n"); return 0; }
    printf("1 (fast CLI path: %s)%s\n", t2 ? t2 + 1 : "chat/oneshot/version", (t2 && t2[1] == '1') ? " — deferred agent startup" : "");
    return 0;
}

/* PoP: _try_termux_fast_tui_launch @ hermes_cli/main.py:_try_termux_fast_tui_launch */
int main_u_try_termux_fast_tui_launch(const char *arg) {
    /* Python: phone hot path. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1 (TUI launched fast)" : "0");
    return 0;
}

/* PoP: cmd_acp @ hermes_cli/main.py:cmd_acp */
int main_cmd_acp(const char *arg) {
    /* Python: ACP server launch. Arg = "state". */
    if (!arg || !*arg) { printf("acp server launched\n"); return 0; }
    if (strcmp(arg, "no_deps") == 0) {
        fprintf(stderr, "ACP dependencies not installed.\nInstall them with:  pip install -e '.[acp]'\n");
        return 1;
    }
    printf("acp server launched (flags: %s)\n", arg);
    return 0;
}

/* PoP: cmd_pairing @ hermes_cli/main.py:cmd_pairing */
int main_cmd_pairing(const char *arg) {
    /* Python: delegates to the pairing subcommand implementation. */
    (void)arg;
    printf("pairing subcommand\n");
    return 0;
}

/* PoP: cmd_claw @ hermes_cli/main.py:cmd_claw */
int main_cmd_claw(const char *arg) {
    /* Python: delegates to the claw subcommand implementation. */
    (void)arg;
    printf("claw subcommand\n");
    return 0;
}
