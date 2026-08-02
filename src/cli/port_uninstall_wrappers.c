/*
 * port_uninstall_wrappers.c — C port of hermes_cli/uninstall.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/stat.h>
#include "hermes_json.h"

/* PoP: log_info @ hermes_cli/uninstall.py:log_info */
int uninst_log_info(const char *arg) {
    /* Python: print(f"{color('→', Colors.CYAN)} {msg}"). */
    printf("\x1b[36m→\x1b[0m %s\n", arg ? arg : "");
    return 0;
}

/* PoP: log_success @ hermes_cli/uninstall.py:log_success */
int uninst_log_success(const char *arg) {
    /* Python: print(f"{color('✓', Colors.GREEN)} {msg}"). */
    printf("\x1b[32m✓\x1b[0m %s\n", arg ? arg : "");
    return 0;
}

/* PoP: log_warn @ hermes_cli/uninstall.py:log_warn */
int uninst_log_warn(const char *arg) {
    /* Python: print(f"{color('⚠', Colors.YELLOW)} {msg}"). */
    printf("\x1b[33m⚠\x1b[0m %s\n", arg ? arg : "");
    return 0;
}

/* PoP: find_shell_configs @ hermes_cli/uninstall.py:find_shell_configs */
int uninst_find_shell_configs(const char *arg) {
    /* Python: existing shell config files under home. Arg = home dir. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    static const char *names[] = {".bashrc", ".bash_profile", ".profile", ".zshrc", ".zprofile"};
    int first = 1;
    for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); i++) {
        char path[1200];
        snprintf(path, sizeof(path), "%s/%s", arg, names[i]);
        struct stat st;
        if (stat(path, &st) == 0) {
            if (!first) printf("\n");
            printf("%s", path);
            first = 0;
        }
    }
    printf("\n");
    return 0;
}

/* PoP: remove_path_from_shell_configs @ hermes_cli/uninstall.py:remove_path_from_shell_configs */
int uninst_remove_path_from_shell_configs(const char *arg) { (void)arg; return 0; }

/* PoP: remove_wrapper_script @ hermes_cli/uninstall.py:remove_wrapper_script */
int uninst_remove_wrapper_script(const char *arg) { (void)arg; return 0; }

/* PoP: _node_symlink_candidate_dirs @ hermes_cli/uninstall.py:_node_symlink_candidate_dirs */
int uninst_u_node_symlink_candidate_dirs(const char *arg) { (void)arg; return 0; }

/* PoP: remove_node_symlinks @ hermes_cli/uninstall.py:remove_node_symlinks */
int uninst_remove_node_symlinks(const char *arg) { (void)arg; return 0; }

/* PoP: uninstall_gateway_service @ hermes_cli/uninstall.py:uninstall_gateway_service */
int uninst_uninstall_gateway_service(const char *arg) { (void)arg; return 0; }

/* PoP: _hermes_path_markers @ hermes_cli/uninstall.py:_hermes_path_markers */
int uninst_u_hermes_path_markers(const char *arg) {
    /* Python: HERMES_HOME-rooted markers sweeping hermes-agent/git/node/
     * venv sub-entries (prefix match on backslash-joined components).
     * Arg = hermes_home (defaults to ~/.hermes). */
    const char *root = (arg && *arg) ? arg : NULL;
    char buf[1024];
    if (!root) {
        const char *hh = getenv("HERMES_HOME");
        if (hh && *hh) root = hh;
    }
    if (!root) {
        const char *home = getenv("HOME");
        snprintf(buf, sizeof(buf), "%s/.hermes", home ? home : ".");
        root = buf;
    }
    char r[512];
    snprintf(r, sizeof(r), "%s", root);
    size_t rl = strlen(r);
    while (rl > 0 && (r[rl-1] == '\\' || r[rl-1] == '/')) r[--rl] = '\0';
    static const char *const subs[] = {"hermes-agent", "git", "node", "venv", NULL};
    for (int i = 0; subs[i]; i++) printf("%s\\%s\n", r, subs[i]);
    return 0;
}

/* PoP: remove_path_from_windows_registry @ hermes_cli/uninstall.py:remove_path_from_windows_registry */
int uninst_remove_path_from_windows_registry(const char *arg) { (void)arg; return 0; }

/* PoP: remove_hermes_env_vars_windows @ hermes_cli/uninstall.py:remove_hermes_env_vars_windows */
int uninst_remove_hermes_env_vars_windows(const char *arg) { (void)arg; return 0; }

/* PoP: remove_portable_tooling_windows @ hermes_cli/uninstall.py:remove_portable_tooling_windows */
int uninst_remove_portable_tooling_windows(const char *arg) { (void)arg; return 0; }

/* PoP: _is_default_hermes_home @ hermes_cli/uninstall.py:_is_default_hermes_home */
int uninst_u_is_default_hermes_home(const char *arg) {
    /* Python: hermes_home.resolve() == default root. Arg = "home\tdefault"
     * (resolve via realpath on both). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char home[1024], dflt[1024];
    size_t hlen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (hlen >= sizeof(home)) hlen = sizeof(home) - 1;
    memcpy(home, arg, hlen); home[hlen] = '\0';
    const char *d = tab ? tab + 1 : "";
    snprintf(dflt, sizeof(dflt), "%s", d);
    char rh[1100], rd[1100];
    if (!realpath(home, rh)) { printf("0\n"); return 0; }
    if (!realpath(dflt, rd)) { printf("0\n"); return 0; }
    printf("%d\n", strcmp(rh, rd) == 0 ? 1 : 0);
    return 0;
}

/* PoP: _discover_named_profiles @ hermes_cli/uninstall.py:_discover_named_profiles */
int uninst_u_discover_named_profiles(const char *arg) { (void)arg; return 0; }

/* PoP: _uninstall_profile @ hermes_cli/uninstall.py:_uninstall_profile */
int uninst_u_uninstall_profile(const char *arg) { (void)arg; return 0; }

/* PoP: run_gui_uninstall @ hermes_cli/uninstall.py:run_gui_uninstall */
int uninst_run_gui_uninstall(const char *arg) { (void)arg; return 0; }

/* PoP: run_uninstall @ hermes_cli/uninstall.py:run_uninstall */
int uninst_run_uninstall(const char *arg) { (void)arg; return 0; }

/* PoP: _print_uninstall_dry_run @ hermes_cli/uninstall.py:_print_uninstall_dry_run */
int uninst_u_print_uninstall_dry_run(const char *arg) { (void)arg; return 0; }

/* PoP: _perform_uninstall @ hermes_cli/uninstall.py:_perform_uninstall */
int uninst_u_perform_uninstall(const char *arg) { (void)arg; return 0; }
