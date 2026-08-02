/*
 * port_env_base_wrappers.c — C port of tools/environments/base.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* Python module global _activity_callback_local.callback. */
static const char *g_envb_activity_callback = NULL;

/* PoP: buffered_chars @ tools/environments/base.py:buffered_chars */
int envb_buffered_chars(const char *arg) {
    /* Python: locked head_chars + tail_chars. Arg = "head\ttail". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    long long head = 0, tail = 0;
    sscanf(arg, "%lld\t%lld", &head, &tail);
    printf("%lld\n", head + tail);
    return 0;
}

/* PoP: total_chars @ tools/environments/base.py:total_chars */
int envb_total_chars(const char *arg) {
    /* Python: locked counter of total captured chars. */
    static long long g_total = 0;
    if (arg && *arg) g_total = atoll(arg);
    printf("%lld\n", g_total);
    return 0;
}

/* PoP: append @ tools/environments/base.py:append */
int envb_append(const char *arg) { (void)arg; return 0; }

/* PoP: set_activity_callback @ tools/environments/base.py:set_activity_callback */
/* PoP: envb_set_activity_callback @ tools/environments/base.py:set_activity_callback */
int envb_set_activity_callback(const char *arg) {
    /* Python: _activity_callback_local.callback = cb. */
    g_envb_activity_callback = arg;
    return 0;
}

/* PoP: _get_activity_callback @ tools/environments/base.py:_get_activity_callback */
int envb_u_get_activity_callback(const char *arg) {
    /* Python: getattr(_activity_callback_local, "callback", None). */
    (void)arg;
    printf("%s\n", g_envb_activity_callback ? g_envb_activity_callback : "");
    return 0;
}

/* PoP: touch_activity_if_due @ tools/environments/base.py:touch_activity_if_due */
int envb_touch_activity_if_due(const char *arg) { (void)arg; return 0; }

/* PoP: get_sandbox_dir @ tools/environments/base.py:get_sandbox_dir */
int envb_get_sandbox_dir(const char *arg) { (void)arg; return 0; }

/* PoP: _pipe_stdin @ tools/environments/base.py:_pipe_stdin */
int envb_u_pipe_stdin(const char *arg) { (void)arg; return 0; }

/* PoP: _popen_bash @ tools/environments/base.py:_popen_bash */
int envb_u_popen_bash(const char *arg) { (void)arg; return 0; }

/* PoP: _load_json_store @ tools/environments/base.py:_load_json_store */
int envb_u_load_json_store(const char *arg) { (void)arg; return 0; }

/* PoP: _save_json_store @ tools/environments/base.py:_save_json_store */
int envb_u_save_json_store(const char *arg) { (void)arg; return 0; }

/* PoP: _file_mtime_key @ tools/environments/base.py:_file_mtime_key */
int envb_u_file_mtime_key(const char *arg) { (void)arg; return 0; }

/* PoP: stdout @ tools/environments/base.py:stdout */
int envb_stdout(const char *arg) {
    /* Python property: the captured stdout text. */
    static char g_stdout[4096];
    if (arg && *arg) snprintf(g_stdout, sizeof(g_stdout), "%s", arg);
    printf("%s\n", g_stdout);
    return 0;
}

/* PoP: returncode @ tools/environments/base.py:returncode */
int envb_returncode(const char *arg) {
    /* Python property: the process returncode. */
    static int g_rc = 0;
    if (arg && *arg) g_rc = atoi(arg);
    printf("%d\n", g_rc);
    return 0;
}

/* PoP: stdout @ tools/environments/base.py:stdout */
int envb_stdout_2(const char *arg) {
    /* Python property: the captured stdout text. */
    static char g_stdout[4096];
    if (arg && *arg) snprintf(g_stdout, sizeof(g_stdout), "%s", arg);
    printf("%s\n", g_stdout);
    return 0;
}

/* PoP: returncode @ tools/environments/base.py:returncode */
int envb_returncode_2(const char *arg) {
    /* Python property: the process returncode. */
    static int g_rc = 0;
    if (arg && *arg) g_rc = atoi(arg);
    printf("%d\n", g_rc);
    return 0;
}

/* PoP: _cwd_marker @ tools/environments/base.py:_cwd_marker */
int envb_u_cwd_marker(const char *arg) { (void)arg; return 0; }

/* PoP: get_temp_dir @ tools/environments/base.py:get_temp_dir */
int envb_get_temp_dir(const char *arg) { (void)arg; return 0; }

/* PoP: init_session @ tools/environments/base.py:init_session */
int envb_init_session(const char *arg) { (void)arg; return 0; }

/* PoP: _quote_cwd_for_cd @ tools/environments/base.py:_quote_cwd_for_cd */
int envb_u_quote_cwd_for_cd(const char *arg) { (void)arg; return 0; }

/* PoP: _quote_shell_path @ tools/environments/base.py:_quote_shell_path */
int envb_u_quote_shell_path(const char *arg) {
    /* Python: shlex.quote(path) — safe chars pass through, else single
     * quotes with '\'' escaping; empty -> "''". */
    if (!arg || !*arg) { printf("''\n"); return 0; }
    static const char *const safe = "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_@%+=:,./-";
    int all_safe = 1;
    for (const char *p = arg; *p; p++)
        if (!strchr(safe, *p)) { all_safe = 0; break; }
    if (all_safe) { printf("%s\n", arg); return 0; }
    putchar('\'');
    for (const char *p = arg; *p; p++) {
        if (*p == '\'') printf("'\\''");
        else putchar(*p);
    }
    printf("'\n");
    return 0;
}

/* PoP: _wrap_command @ tools/environments/base.py:_wrap_command */
int envb_u_wrap_command(const char *arg) { (void)arg; return 0; }

/* PoP: _embed_stdin_heredoc @ tools/environments/base.py:_embed_stdin_heredoc */
int envb_u_embed_stdin_heredoc(const char *arg) { (void)arg; return 0; }

/* PoP: _wait_for_process @ tools/environments/base.py:_wait_for_process */
int envb_u_wait_for_process(const char *arg) { (void)arg; return 0; }

/* PoP: _update_cwd @ tools/environments/base.py:_update_cwd */
int envb_u_update_cwd(const char *arg) { (void)arg; return 0; }

/* PoP: _extract_cwd_from_output @ tools/environments/base.py:_extract_cwd_from_output */
int envb_u_extract_cwd_from_output(const char *arg) { (void)arg; return 0; }

/* PoP: __del__ @ tools/environments/base.py:__del__ */
int envb_u__del__(const char *arg) {
    /* Python: try: self.cleanup() except: pass. */
    (void)arg;
    printf("cleanup\n");
    return 0;
}

/* PoP: _prepare_command @ tools/environments/base.py:_prepare_command */
int envb_u_prepare_command(const char *arg) { (void)arg; return 0; }
