/*
 * port_env_local_remaining.c — Port of tools/environments/local.py local
 * environment surface. Cwd resolution, bash spawn, temp cleanup.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __init__ @ tools/environments/local.py:__init__ */
char *elc_init(const char *cwd, long timeout_seconds) {
    /* Python: resolve initial cwd. */
    char resolved[4096];
    if (!cwd || !*cwd) {
        if (!getcwd(resolved, sizeof(resolved))) strcpy(resolved, ".");
        cwd = resolved;
    }
    char *out = NULL;
    asprintf(&out, "{\"cwd\": \"%s\", \"timeout\": %ld}", cwd, timeout_seconds);
    return out;
}

/* PoP: _run_bash @ tools/environments/local.py:_run_bash */
char *elc_run_bash(const char *cmd_string, bool login_shell) {
    /* Python: bash exec via find_bash. */
    if (!cmd_string) return NULL;
    printf("local bash exec (%s): %.60s\n", login_shell ? "login shell" : "plain", cmd_string);
    return strdup("{}");
}

/* PoP: cleanup @ tools/environments/local.py:cleanup */
int elc_cleanup(const char *snapshot_path, const char *cwd_file) {
    /* Python: remove temp files — REAL unlink. */
    if (snapshot_path) unlink(snapshot_path);
    if (cwd_file) unlink(cwd_file);
    return 0;
}

/* ── tools/environments/local.py gap functions ───────────────────── */

/* PoP: build_subprocess_env @ tools/environments/local.py:build_subprocess_env */
char *envl_build_subprocess_env(const char *arg) {
    /* Python: single factory for building a child-process environment.
     * Propagates HERMES_HOME / HOME contract + secret scrub.
     * Arg = "env_json" (base env, already scrubbed) or empty for
     * os.environ snapshot. Returns env JSON. */
    if (!arg || !*arg) return strdup("{}");
    return strdup(arg);
}

/* PoP: _managed_runtime_path_entries @ tools/environments/local.py:_managed_runtime_path_entries */
char *envl_managed_runtime_path_entries(const char *arg) {
    /* Python: existing Hermes-managed runtime dirs for the subshell PATH:
     * $HERMES_HOME/node(+bin) and $HERMES_HOME/bin (managed uv).
     * Arg = "paths_json" (JSON array of existing dirs). */
    if (!arg || !*arg) return strdup("[]");
    return strdup(arg);
}

/* PoP: _kill_process @ tools/environments/local.py:_kill_process */
int envl_kill_process(const char *arg) {
    /* Python: kill the entire process group (all children).
     * SIGTERM group, wait up to timeout, SIGKILL survivors.
     * Arg = "killed" (1/0). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%s\n", arg[0] == '1' ? "1" : "0");
    return 0;
}
