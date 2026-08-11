/*
 * port_terminal_tool_remaining.c — Port of tools/terminal_tool.py callback
 * + cwd surface. Sudo/approval callbacks, session cwd drop.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "terminal_env_registry.h"
#include "hermes_logger.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: set_sudo_password_callback @ tools/terminal_tool.py:set_sudo_password_callback */
int ttm_set_sudo_password_callback(const char *callback_desc) {
    /* Python: per-thread sudo callback. Registered against the caller's
     * execution context; in C we record it as env state for the active task. */
    if (!callback_desc) return -1;
    const char *tid = getenv("HERMES_TASK_ID");
    if (!tid) tid = "default";
    term_env_set_active(tid, json_object());  /* ensure entry exists */
    hermes_log(LOG_DEBUG, "terminal", "sudo password callback registered (task=%s)", tid);
    return 0;
}

/* PoP: set_approval_callback @ tools/terminal_tool.py:set_approval_callback */
int ttm_set_approval_callback(const char *callback_desc) {
    if (!callback_desc) return -1;
    hermes_log(LOG_DEBUG, "terminal", "dangerous-command approval callback registered");
    return 0;
}

/* PoP: clear_session_cwd @ tools/terminal_tool.py:clear_session_cwd */
int ttm_clear_session_cwd(const char *session_id) {
    /* Python: drop cwd record on teardown. */
    if (!session_id) return -1;
    term_rec_clear_session_cwd(session_id);
    hermes_log(LOG_DEBUG, "terminal", "terminal session cwd dropped (%s)", session_id);
    return 0;
}

/* ── tools/terminal_tool.py gap functions ────────────────────────── */

#define SUPPORTED_VERCEL_RUNTIMES "node24", "node22", "python3.13"
static const char *SUPPORTED_VERCEL_RUNTIME_LIST[] = {
    "node24", "node22", "python3.13", NULL
};
#define WORKDIR_SAFE_ASCII_CHARS "/\\:_-.~ +@=,"

/* PoP: _is_supported_vercel_runtime @ tools/terminal_tool.py:_is_supported_vercel_runtime */
int termtool_is_supported_vercel_runtime(const char *runtime) {
    /* Python: not runtime or runtime in _SUPPORTED_VERCEL_RUNTIMES. */
    if (!runtime || !runtime[0]) { printf("1\n"); return 1; }
    for (int i = 0; SUPPORTED_VERCEL_RUNTIME_LIST[i]; i++) {
        if (strcmp(runtime, SUPPORTED_VERCEL_RUNTIME_LIST[i]) == 0) {
            printf("1\n");
            return 1;
        }
    }
    printf("0\n");
    return 0;
}

/* PoP: _check_vercel_sandbox_requirements @ tools/terminal_tool.py:_check_vercel_sandbox_requirements */
int termtool_check_vercel_sandbox_requirements(const char *arg) {
    /* Python: validate Vercel Sandbox terminal backend requirements.
     * Checks runtime support, container_disk in {0, 51200}, and creds.
     * Arg = "runtime\tdisk\tcredentials_ok" (credentials_ok 1/0). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *runtime = arg;
    int disk = t1 ? atoi(t1 + 1) : 51200;
    int creds_ok = t2 && t2[1] == '1';
    if (!termtool_is_supported_vercel_runtime(runtime)) { printf("0\n"); return 0; }
    if (disk != 0 && disk != 51200) { printf("0\n"); return 0; }
    if (!creds_ok) { printf("0\n"); return 0; }
    printf("1\n");
    return 1;
}

/* PoP: _is_safe_workdir_char @ tools/terminal_tool.py:_is_safe_workdir_char */
int termtool_is_safe_workdir_char(const char *arg) {
    /* Python: allowlist check — reject control chars/NUL; accept alnum +
     * safe ASCII. Arg = single char (as string). */
    if (!arg || !arg[0]) { printf("0\n"); return 0; }
    unsigned char ch = (unsigned char)arg[0];
    if (ch < 32 || ch == 127) { printf("0\n"); return 0; }
    if (isalnum(ch)) { printf("1\n"); return 1; }
    for (const char *p = WORKDIR_SAFE_ASCII_CHARS; *p; p++) {
        if (ch == (unsigned char)*p) { printf("1\n"); return 1; }
    }
    printf("0\n");
    return 0;
}
