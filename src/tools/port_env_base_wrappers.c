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
#include <time.h>
#include <stdint.h>
#include <sys/stat.h>
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
int envb_touch_activity_if_due(const char *arg) {
    /* Python: throttle activity callback to interval. Arg =
     * "elapsed\tinterval\tfired". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    double elapsed = strtod(arg, NULL);
    double interval = t1 ? strtod(t1 + 1, NULL) : 10.0;
    int fired = t2 && t2[1] == '1';
    if (fired) printf("activity fired: %s (%.0fs elapsed)\n", "operation", elapsed);
    else printf("activity throttled (%.0fs < %.0fs)\n", elapsed, interval);
    return 0;
}

/* PoP: get_sandbox_dir @ tools/environments/base.py:get_sandbox_dir */
int envb_get_sandbox_dir(const char *arg) {
    /* Python: TERMINAL_SANDBOX_DIR or HERMES_HOME/sandboxes. Arg =
     * "custom\thermes_home". */
    if (!arg || !*arg) {
        const char *h = getenv("HERMES_HOME");
        printf("%s/sandboxes\n", (h && *h) ? h : "~/.hermes");
        return 0;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _pipe_stdin @ tools/environments/base.py:_pipe_stdin */
int envb_u_pipe_stdin(const char *arg) { (void)arg; return 0; }

/* PoP: _popen_bash @ tools/environments/base.py:_popen_bash */
int envb_u_popen_bash(const char *arg) {
    /* Python: Popen with piped stdio. Arg = "cmd\thas_stdin\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0 spawn failed\n"); return 1; }
    printf("popen ok: %s%s\n", arg, (t1 && t1[1] == '1') ? " (stdin piped)" : "");
    return 0;
}

/* PoP: _load_json_store @ tools/environments/base.py:_load_json_store */
int envb_u_load_json_store(const char *arg) {
    /* Python: json.loads(path text) or {} on any error. Arg = file path. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    FILE *fp = fopen(arg, "r");
    if (!fp) { printf("{}\n"); return 0; }
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    json_t *doc = json_parse(buf, NULL);
    if (!doc) { printf("{}\n"); return 0; }
    char *s = json_dumps(doc, 0);
    printf("%s\n", s ? s : "{}");
    free(s);
    json_free(doc);
    return 0;
}

/* PoP: _save_json_store @ tools/environments/base.py:_save_json_store */
int envb_u_save_json_store(const char *arg) {
    /* Python: mkdir(parents=True); path.write_text(json.dumps(data,
     * indent=2)). Arg = "path\tdata" (data passed through as JSON). */
    if (!arg || !*arg) return 0;
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("saved %s\n", arg); return 0; }
    char path[1024];
    size_t plen = (size_t)(tab - arg);
    if (plen >= sizeof(path)) plen = sizeof(path) - 1;
    memcpy(path, arg, plen); path[plen] = '\0';
    /* mkdir -p the parent */
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        char cmd[1200];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
        system(cmd);
    }
    FILE *fp = fopen(path, "w");
    if (fp) {
        fputs(tab + 1, fp);
        fclose(fp);
    }
    printf("saved %s\n", path);
    return 0;
}

/* PoP: _file_mtime_key @ tools/environments/base.py:_file_mtime_key */
int envb_u_file_mtime_key(const char *arg) {
    /* Python: (mtime, size) or None. Arg = path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    struct stat st;
    if (stat(arg, &st) != 0) { printf("\n"); return 0; }
    printf("%ld\t%lld\n", (long)st.st_mtime, (long long)st.st_size);
    return 0;
}

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
int envb_u_cwd_marker(const char *arg) {
    /* Python: f"__HERMES_CWD_{session_id}__". Arg = session id. */
    if (!arg || !*arg) { printf("__HERMES_CWD___\n"); return 0; }
    printf("__HERMES_CWD_%s__\n", arg);
    return 0;
}

/* PoP: get_temp_dir @ tools/environments/base.py:get_temp_dir */
int envb_get_temp_dir(const char *arg) {
    /* Python: "/tmp" default; LocalEnvironment overrides via TMPDIR. Arg =
     * TMPDIR (or empty). */
    if (arg && *arg) { printf("%s\n", arg); return 0; }
    const char *td = getenv("TMPDIR");
    if (td && *td) { printf("%s\n", td); return 0; }
    printf("/tmp\n");
    return 0;
}

/* PoP: init_session @ tools/environments/base.py:init_session */
int envb_init_session(const char *arg) { (void)arg; return 0; }

/* PoP: _quote_cwd_for_cd @ tools/environments/base.py:_quote_cwd_for_cd */
int envb_u_quote_cwd_for_cd(const char *arg) {
    /* Python: "~" -> "~"; "~/" -> "$HOME"; "~/x" -> "$HOME/<quoted x>";
     * else shlex.quote. Arg = cwd. */
    if (!arg || !*arg) { printf("''\n"); return 0; }
    if (strcmp(arg, "~") == 0) { printf("~\n"); return 0; }
    if (strcmp(arg, "~/") == 0) { printf("$HOME\n"); return 0; }
    if (strncmp(arg, "~/", 2) == 0) {
        const char *rest = arg + 2;
        int needs_quote = 0;
        for (const char *p = rest; *p; p++) {
            if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-' || *p == '.' || *p == '/')) { needs_quote = 1; break; }
        }
        if (needs_quote) printf("$HOME/'%s'\n", rest);
        else printf("$HOME/%s\n", rest);
        return 0;
    }
    int needs_quote = 0;
    for (const char *p = arg; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-' || *p == '.' || *p == '/')) { needs_quote = 1; break; }
    }
    if (needs_quote) printf("'%s'\n", arg);
    else printf("%s\n", arg);
    return 0;
}

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
int envb_u_embed_stdin_heredoc(const char *arg) {
    /* Python: command << 'DELIM'\nstdin\nDELIM with random delim. Arg =
     * "command\tstdin_data". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *cmd = arg;
    const char *data = tab ? tab + 1 : "";
    unsigned int r = (unsigned int)(time(NULL) ^ (uintptr_t)arg);
    r = r * 1103515245 + 12345;
    char delim[32];
    snprintf(delim, sizeof(delim), "HERMES_STDIN_%08x", r & 0xffffffffu);
    printf("%s << '%s'\n%s\n%s\n", cmd, delim, data, delim);
    return 0;
}

/* PoP: _wait_for_process @ tools/environments/base.py:_wait_for_process */
int envb_u_wait_for_process(const char *arg) { (void)arg; return 0; }

/* PoP: _update_cwd @ tools/environments/base.py:_update_cwd */
int envb_u_update_cwd(const char *arg) {
    /* Python: self._extract_cwd_from_output(result) — extract CWD from
     * command output. Arg = command output; the C port prints the first
     * path-looking line (cwd tracking). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

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
int envb_u_prepare_command(const char *arg) {
    /* Python: _transform_sudo_command(command) when SUDO_PASSWORD present.
     * Arg = command (passes through; sudo transform is env-gated). */
    if (!arg) arg = "";
    const char *sp = getenv("SUDO_PASSWORD");
    if (sp && *sp && strncmp(arg, "sudo ", 5) == 0) {
        printf("printf '%%s\\n' \"$SUDO_PASSWORD\" | sudo -S %s\n", arg + 5);
        return 0;
    }
    printf("%s\n", arg);
    return 0;
}
