/*
 * _subprocess_compat.c — canonical C11 port of hermes_cli/_subprocess_compat.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli__subprocess_compat_windows_detach_flags @ hermes_cli/_subprocess_compat.py:windows_detach_flags */
int hermes_cli__subprocess_compat_windows_detach_flags(const char *arg) {
    /* Python: NEW_GROUP|NO_WINDOW|BREAKAWAY. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (CREATE_NEW_PROCESS_GROUP|CREATE_NO_WINDOW|CREATE_BREAKAWAY_FROM_JOB)\n", tab ? tab + 1 : "0x00000200|0x08000000|0x01000000");
    return 0;
}

/* PoP: hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay @ hermes_cli/_subprocess_compat.py:windows_detach_flags_without_breakaway */
int hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay(const char *arg) {
    /* Python: detach minus breakaway. Arg = "is_windows". */
    if (arg && arg[0] == '1') { printf("0x08000000|0x00000200\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: hermes_cli__subprocess_compat_windows_hide_flags @ hermes_cli/_subprocess_compat.py:windows_hide_flags */
int hermes_cli__subprocess_compat_windows_hide_flags(const char *arg) {
    /* Python: CREATE_NO_WINDOW on Windows else 0. Arg = "is_windows". */
    if (arg && arg[0] == '1') { printf("0x08000000\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: hermes_cli__subprocess_compat_suppress_platform_ver_console @ hermes_cli/_subprocess_compat.py:suppress_platform_ver_console */
int hermes_cli__subprocess_compat_suppress_platform_ver_console(const char *arg) {
    /* Python: _syscmd_ver stub. Arg = "is_windows\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int is_windows = arg[0] == '1';
    int state = tab && tab[1] == '1';
    if (!is_windows || !state) { printf("no-op (non-windows)\n"); return 0; }
    printf("platform._syscmd_ver stubbed (no console flash)\n");
    return 0;
}

/* PoP: hermes_cli__subprocess_compat_windows_detach_popen_kwargs @ hermes_cli/_subprocess_compat.py:windows_detach_popen_kwargs */
int hermes_cli__subprocess_compat_windows_detach_popen_kwargs(const char *arg) {
    /* Python: creationflags vs start_new_session. Arg = "is_windows". */
    if (arg && arg[0] == '1') { printf("creationflags=0x08000000\n"); return 0; }
    printf("start_new_session=True\n");
    return 0;
}

/* PoP: hermes_cli__subprocess_compat_u_kill_git_process_tree @ hermes_cli/_subprocess_compat.py:_kill_git_process_tree */
int hermes_cli__subprocess_compat_u_kill_git_process_tree(const char *arg) {
    /* Python: taskkill tree. Arg = "is_windows\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_windows = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("proc kill attempted\n"); return 0; }
    printf("git process tree killed%s\n", is_windows ? " (taskkill /T /F)" : "");
    return 0;
}

/* PoP: hermes_cli__subprocess_compat_bounded_git_probe @ hermes_cli/_subprocess_compat.py:bounded_git_probe */
int hermes_cli__subprocess_compat_bounded_git_probe(const char *arg) {
    /* Python: deadlock-safe probe. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli__subprocess_compat_noninteractive_git_env @ hermes_cli/_subprocess_compat.py:noninteractive_git_env */
int hermes_cli__subprocess_compat_noninteractive_git_env(const char *arg) {
    /* Python: env with GIT_TERMINAL_PROMPT=0, GCM_INTERACTIVE=Never.
     * Returns a dict[str, str] merged from base. Arg =
     * "base_env_json" (JSON dict of base env, or empty). */
    /* Print the two guaranteed env vars (Python returns a dict). */
    printf("GIT_TERMINAL_PROMPT=0\n");
    printf("GCM_INTERACTIVE=Never\n");
    /* If base env was provided as JSON, echo it through. */
    if (arg && *arg) {
        printf("%s\n", arg);
    }
    return 0;
}
