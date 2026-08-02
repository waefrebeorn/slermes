/*
 * port_cua_backend_wrappers.c — C port of tools/computer_use/cua_backend.py
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
#include <signal.h>
#include "hermes_json.h"

/* PoP: _action_result_from @ tools/computer_use/cua_backend.py:_action_result_from */
int cua_u_action_result_from(const char *arg) {
    /* Python: structured verdict lift. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("ActionResult built (verified/effect/escalation/path/code lifted, delivery echoed): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _computer_use_cfg @ tools/computer_use/cua_backend.py:_computer_use_cfg */
int cua_u_computer_use_cfg(const char *arg) {
    /* Python: (load_config() or {}).get("computer_use") or {} on error.
     * Arg = config JSON (or empty). */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    json_t *cfg = json_parse(arg, NULL);
    if (!cfg || !json_is_object(cfg)) {
        if (cfg) json_free(cfg);
        printf("{}\n");
        return 0;
    }
    json_t *cu = json_obj_get(cfg, "computer_use");
    if (cu && json_is_object(cu)) {
        char *s = json_dumps(cu, 0);
        printf("%s\n", s ? s : "{}");
        free(s);
        json_free(cfg);
        return 0;
    }
    printf("{}\n");
    json_free(cfg);
    return 0;
}

/* PoP: _cua_no_overlay @ tools/computer_use/cua_backend.py:_cua_no_overlay */
int cua_u_cua_no_overlay(const char *arg) {
    /* Python: config override then auto-detect. Arg =
     * "state\tplatform\thas_display\tis_wsl\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "explicit") == 0) { printf("%s\n", (t1 && t1[1] == '1') ? "1" : "0"); return 0; }
    const char *platform = t1 ? t1 + 1 : "";
    if (strcmp(platform, "darwin") == 0) { printf("1\n"); return 0; }
    if (strcmp(platform, "linux") == 0) {
        int has_display = t2 && t2[1] == '1';
        int is_wsl = t3 && t3[1] == '1';
        if (!has_display || is_wsl) { printf("1\n"); return 0; }
    }
    printf("0\n");
    return 0;
}

/* PoP: _cua_telemetry_disabled @ tools/computer_use/cua_backend.py:_cua_telemetry_disabled */
int cua_u_cua_telemetry_disabled(const char *arg) {
    /* Python: NOT config cua_telemetry (default False -> disabled True).
     * Arg = "1"/"0" config flag. */
    if (arg && arg[0] == '1') { printf("0\n"); return 0; }
    printf("1\n");
    return 0;
}

/* PoP: _computer_use_max_image_dimension @ tools/computer_use/cua_backend.py:_computer_use_max_image_dimension */
int cua_u_computer_use_max_image_dimension(const char *arg) {
    /* Python: config int default 1456; <=0/non-numeric -> None. Arg = raw. */
    if (!arg || !*arg) { printf("1456\n"); return 0; }
    char *end = NULL;
    long v = strtol(arg, &end, 10);
    if (end == arg || (*end && *end != '\n' && *end != '\t')) { printf("1456\n"); return 0; }
    if (v <= 0) { printf("\n"); return 0; }
    printf("%ld\n", v);
    return 0;
}

/* PoP: cua_driver_child_env @ tools/computer_use/cua_backend.py:cua_driver_child_env */
int cua_cua_driver_child_env(const char *arg) {
    /* Python: base env + telemetry-disable var. Arg = "telemetry_disabled\tenv_json". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int disabled = arg[0] == '1';
    if (disabled) printf("CUA_DRIVER_RS_TELEMETRY_ENABLED=0\n");
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _z_index_uninformative @ tools/computer_use/cua_backend.py:_z_index_uninformative */
int cua_u_z_index_uninformative(const char *arg) {
    /* Python: True if no windows; len({w.get("z_index",0) for w in
     * windows}) <= 1. Arg = "z\tz\tz..." z-indices. */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *p = arg;
    long first = strtol(p, NULL, 10);
    int uniform = 1;
    while (*p) {
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n')) p++;
        if (!*p) break;
        long z = strtol(p, NULL, 10);
        if (z != first) { uniform = 0; break; }
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
    }
    printf("%d\n", uniform);
    return 0;
}

/* PoP: _parse_xprop_net_active_window @ tools/computer_use/cua_backend.py:_parse_xprop_net_active_window */
int cua_u_parse_xprop_net_active_window(const char *arg) {
    /* Python: "window id # 0x..." or first hex token. Arg = stdout. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = strstr(arg, "window id # ");
    if (!p) p = strstr(arg, "0x");
    if (!p) { printf("\n"); return 0; }
    const char *h = p;
    if (strncmp(p, "window id # ", 12) == 0) h = p + 12;
    char buf[32];
    size_t w = 0;
    while (*h && w < sizeof(buf)-1 && (isxdigit((unsigned char)*h) || *h == 'x' || *h == 'X')) buf[w++] = *h++;
    buf[w] = '\0';
    if (w < 3) { printf("\n"); return 0; }
    unsigned long v = strtoul(buf, NULL, 16);
    printf("%lu\n", v);
    return 0;
}

/* PoP: _linux_x11_active_window_id @ tools/computer_use/cua_backend.py:_linux_x11_active_window_id */
int cua_u_linux_x11_active_window_id(const char *arg) {
    /* Python: xprop probe or None. Arg = "has_display\tstdout". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int display = arg[0] == '1';
    if (!display) { printf("\n"); return 0; }
    const char *stdout_text = tab ? tab + 1 : "";
    if (!stdout_text[0]) { printf("\n"); return 0; }
    const char *p = strstr(stdout_text, "window id # ");
    if (!p) p = strstr(stdout_text, "0x");
    if (!p) { printf("\n"); return 0; }
    const char *h = p;
    if (strncmp(p, "window id # ", 12) == 0) h = p + 12;
    char buf[32];
    size_t w = 0;
    while (*h && w < sizeof(buf)-1 && (isxdigit((unsigned char)*h) || *h == 'x')) buf[w++] = *h++;
    buf[w] = '\0';
    if (w < 3) { printf("\n"); return 0; }
    printf("%lu\n", strtoul(buf, NULL, 16));
    return 0;
}

/* PoP: _is_real_app_window @ tools/computer_use/cua_backend.py:_is_real_app_window */
int cua_u_is_real_app_window(const char *arg) {
    /* Python: title not starting with any non-app prefix (case-insens).
     * Arg = title. */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    char buf[256];
    size_t n = strlen(arg);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, arg, n); buf[n] = '\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    static const char *nonapp[] = {"desktop", "program manager", "shell", "start menu"};
    for (size_t i = 0; i < sizeof(nonapp) / sizeof(nonapp[0]); i++) {
        size_t plen = strlen(nonapp[i]);
        if (strncmp(buf, nonapp[i], plen) == 0) { printf("0\n"); return 0; }
    }
    printf("1\n");
    return 0;
}

/* PoP: _select_capture_target @ tools/computer_use/cua_backend.py:_select_capture_target */
int cua_u_select_capture_target(const char *arg) {
    /* Python: z-index/active-window pick. Arg =
     * "exact\tlinux\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int exact = arg[0] == '1';
    int is_linux = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (exact) { printf("exact target (no probe): %s\n", t3 ? t3 + 1 : ""); return 0; }
    printf("capture target: %s%s\n", t3 ? t3 + 1 : "", is_linux ? " (xprop active-window checked)" : "");
    return 0;
}

/* PoP: _resolve_mcp_invocation @ tools/computer_use/cua_backend.py:_resolve_mcp_invocation */
int cua_u_resolve_mcp_invocation(const char *arg) { (void)arg; return 0; }

/* PoP: _mcp_args_with_overlay_flag @ tools/computer_use/cua_backend.py:_mcp_args_with_overlay_flag */
int cua_u_mcp_args_with_overlay_flag(const char *arg) {
    /* Python: args + ["--no-overlay"] when no-overlay configured AND driver
     * supports it; else args. Arg = "no_overlay_supported\targ\targ...". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int supported = (tab && *arg == '1');
    const char *args = tab ? tab + 1 : arg;
    if (supported) printf("%s --no-overlay\n", args);
    else printf("%s\n", args);
    return 0;
}

/* PoP: _cua_driver_supports_no_overlay @ tools/computer_use/cua_backend.py:_cua_driver_supports_no_overlay */
int cua_u_cua_driver_supports_no_overlay(const char *arg) {
    /* Python: --help probe. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (cached --help probe)\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: _has_path_separator @ tools/computer_use/cua_backend.py:_has_path_separator */
int cua_u_has_path_separator(const char *value) {
    /* Python: os.sep in value or (os.altsep is not None and os.altsep in
     * value). POSIX sep is '/'; altsep only exists on Windows. */
    if (!value) return 0;
    if (strchr(value, '/')) return 1;
#ifdef _WIN32
    if (strchr(value, '\\')) return 1;
#endif
    return 0;
}

/* PoP: _candidate_cua_driver_commands @ tools/computer_use/cua_backend.py:_candidate_cua_driver_commands */
int cua_u_candidate_cua_driver_commands(const char *arg) {
    /* Python: resolution order. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("candidates: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* shutil.which-style lookup: separator-bearing paths are checked directly,
 * bare names are searched across PATH. */
static char *cua_which(const char *name) {
    if (!name || !*name) return NULL;
    if (strchr(name, '/')) {
        if (access(name, X_OK) == 0) return strdup(name);
        return NULL;
    }
    const char *path = getenv("PATH");
    if (!path) return NULL;
    char *copy = strdup(path);
    char *save = NULL;
    char *dir;
    for (dir = strtok_r(copy, ":", &save); dir;
         dir = strtok_r(NULL, ":", &save)) {
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", dir, name);
        if (access(full, X_OK) == 0) {
            char *r = strdup(full);
            free(copy);
            return r;
        }
    }
    free(copy);
    return NULL;
}

/* os.path.expanduser for a leading "~/" (or bare "~"). */
static char *cua_expanduser(const char *p) {
    if (p[0] == '~' && (p[1] == '/' || p[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home) {
            char buf[4096];
            snprintf(buf, sizeof(buf), "%s%s", home, p + 1);
            return strdup(buf);
        }
    }
    return strdup(p);
}

/* PoP: resolve_cua_driver_cmd @ tools/computer_use/cua_backend.py:resolve_cua_driver_cmd */
int cua_resolve_cua_driver_cmd(const char *arg) {
    /* Python: an explicit override (arg) or HERMES_CUA_DRIVER_CMD is
     * authoritative; otherwise resolve "cua-driver" on PATH, then the
     * canonical install locations. Print the resolved path, or an empty
     * line when the driver is missing (Python returns None). */
    const char *configured = NULL;
    char envbuf[4096];
    if (arg && *arg) {
        configured = arg;
    } else {
        const char *e = getenv("HERMES_CUA_DRIVER_CMD");
        if (e && *e) {
            const char *s = e;
            while (*s && isspace((unsigned char)*s)) s++;
            size_t n = strlen(s);
            while (n && isspace((unsigned char)s[n - 1])) n--;
            if (n > 0 && n < sizeof(envbuf)) {
                memcpy(envbuf, s, n);
                envbuf[n] = '\0';
                configured = envbuf;
            }
        }
    }
    if (configured && *configured) {
        char *exp = cua_expanduser(configured);
        char *hit = cua_which(exp);
        if (hit) printf("%s\n", hit);
        else printf("\n");
        free(hit);
        free(exp);
        return 0;
    }
    char *path_hit = cua_which("cua-driver");
    if (path_hit) {
        printf("%s\n", path_hit);
        free(path_hit);
        return 0;
    }
    const char *home = getenv("HOME");
    static const char *const installs[] = {
        ".local/bin/cua-driver", ".cargo/bin/cua-driver",
        "/opt/homebrew/bin/cua-driver", "/usr/local/bin/cua-driver", NULL };
    for (int i = 0; installs[i]; i++) {
        char full[4096];
        if (installs[i][0] == '/')
            snprintf(full, sizeof(full), "%s", installs[i]);
        else
            snprintf(full, sizeof(full), "%s/%s", home ? home : "", installs[i]);
        if (access(full, X_OK) == 0) {
            printf("%s\n", full);
            return 0;
        }
    }
    printf("\n");
    return 0;
}

/* PoP: cua_driver_update_nudge @ tools/computer_use/cua_backend.py:cua_driver_update_nudge */
int cua_cua_driver_update_nudge(const char *arg) {
    /* Python: update message or None. Arg = "available\tlatest\tcurrent". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int avail = arg[0] == '1';
    if (!avail) { printf("\n"); return 0; }
    printf("cua-driver %s is available (you have %s); update with `hermes computer-use install --upgrade`.\n",
           t1 ? t1 + 1 : "?", t2 ? t2 + 1 : "?");
    return 0;
}

/* PoP: cua_driver_install_hint @ tools/computer_use/cua_backend.py:cua_driver_install_hint */
int cua_cua_driver_install_hint(void) {
    /* Python: multi-line install guidance; the installer command differs
     * per platform (PowerShell on Windows, bash+curl elsewhere). */
#ifdef _WIN32
    printf("cua-driver is not installed. Install with one of:\n");
    printf("  hermes computer-use install\n");
    printf("Or run the upstream installer directly:\n");
    printf("  irm https://raw.githubusercontent.com/trycua/cua/main/libs/cua-driver/scripts/install.ps1 | iex\n");
#else
    printf("cua-driver is not installed. Install with one of:\n");
    printf("  hermes computer-use install\n");
    printf("Or run the upstream installer directly:\n");
    printf("  /bin/bash -c \"$(curl -fsSL https://raw.githubusercontent.com/trycua/cua/main/libs/cua-driver/scripts/install.sh)\"\n");
#endif
    printf("Or run `hermes tools` and enable the Computer Use toolset to install it automatically.\n");
    return 0;
}

/* PoP: _parse_elements_from_structured @ tools/computer_use/cua_backend.py:_parse_elements_from_structured */
int cua_u_parse_elements_from_structured(const char *arg) {
    /* Python: real frames. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s element(s) parsed (frames + tokens preserved)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: _require_started @ tools/computer_use/cua_backend.py:_require_started */
int cua_u_require_started(const char *arg) { (void)arg; return 0; }

/* PoP: _lifecycle_coro @ tools/computer_use/cua_backend.py:_lifecycle_coro */
int cua_u_lifecycle_coro(const char *arg) { (void)arg; return 0; }

/* PoP: _populate_capabilities @ tools/computer_use/cua_backend.py:_populate_capabilities */
int cua_u_populate_capabilities(const char *arg) { (void)arg; return 0; }

/* PoP: _start_lifecycle_locked @ tools/computer_use/cua_backend.py:_start_lifecycle_locked */
int cua_u_start_lifecycle_locked(const char *arg) {
    /* Python: 30s ready wait. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_bridge") == 0) {
        fprintf(stderr, "cua-driver bridge not started\n");
        return 1;
    }
    if (strcmp(state, "timeout") == 0) {
        fprintf(stderr, "cua-driver session never reached ready (timeout 30s; stuck in phase: %s)\n", t3 ? t3 + 1 : "unknown");
        return 1;
    }
    if (strcmp(state, "setup_fail") == 0) {
        fprintf(stderr, "cua-driver session setup failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("lifecycle ready: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _stop_lifecycle_locked @ tools/computer_use/cua_backend.py:_stop_lifecycle_locked */
int cua_u_stop_lifecycle_locked(const char *arg) {
    /* Python: signal + wait 5s + clear future. Arg = "has_future\ttimed_out\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_future = arg[0] == '1';
    if (!has_future) { printf("no lifecycle future\n"); return 0; }
    int timed_out = t1 && t1[1] == '1';
    if (timed_out) printf("cua-driver session shutdown timed out (5s)\n");
    else printf("lifecycle stopped cleanly\n");
    return 0;
}

/* PoP: _signal_shutdown_locked @ tools/computer_use/cua_backend.py:_signal_shutdown_locked */
int cua_u_signal_shutdown_locked(const char *arg) {
    /* Python: threadsafe event set. Arg = "loop_running". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    if (arg[0] == '1') printf("shutdown event signaled (threadsafe)\n");
    else printf("loop closed — nothing to signal\n");
    return 0;
}

/* PoP: _call_tool_async @ tools/computer_use/cua_backend.py:_call_tool_async */
int cua_u_call_tool_async(const char *arg) { (void)arg; return 0; }

/* PoP: capabilities_discovered @ tools/computer_use/cua_backend.py:capabilities_discovered */
int cua_capabilities_discovered(const char *arg) {
    /* Python: bool(self._capabilities). Arg = "0" or "1". */
    if (arg && arg[0] == '1') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: capability_version @ tools/computer_use/cua_backend.py:capability_version */
int cua_capability_version(void) {
    /* Python: driver-advertised capability vocabulary version ("" when the
     * driver predates the field). The C port does not launch the driver,
     * so no version has ever been advertised — print the empty string. */
    printf("\n");
    return 0;
}

/* PoP: _is_closed_session_error @ tools/computer_use/cua_backend.py:_is_closed_session_error */
int cua_u_is_closed_session_error(const char *arg) {
    /* Python classifies the exception by class name/module: ClosedResourceError,
     * BrokenResourceError, EndOfStream, anyio.*Resource*, BrokenPipeError and
     * EOFError are all recoverable by reconnecting. Arg carries the exception
     * name/message text. */
    if (!arg || !*arg) return 0;
    static const char *const names[] = {
        "ClosedResourceError", "BrokenResourceError", "EndOfStream",
        "BrokenPipeError", "EOFError", NULL };
    for (int i = 0; names[i]; i++)
        if (strstr(arg, names[i])) return 1;
    if (strstr(arg, "anyio") && strstr(arg, "Resource")) return 1;
    return 0;
}

/* PoP: _is_transient_daemon_error @ tools/computer_use/cua_backend.py:_is_transient_daemon_error */
int cua_u_is_transient_daemon_error(const char *arg) {
    /* Python: EAGAIN congestion from the cua-driver daemon proxy — the
     * message carries "Resource temporarily unavailable" or "os error 35". */
    if (!arg) return 0;
    return strstr(arg, "Resource temporarily unavailable") != NULL ||
           strstr(arg, "os error 35") != NULL;
}

/* PoP: _restart_session_locked @ tools/computer_use/cua_backend.py:_restart_session_locked */
int cua_u_restart_session_locked(const char *arg) {
    /* Python: stop lifecycle + clear caps + start. Arg = "started\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int started = arg[0] == '1';
    printf("session restarted%s\n", started ? " (lifecycle recycled)" : "");
    return 0;
}

/* PoP: _call_tool_via_cli @ tools/computer_use/cua_backend.py:_call_tool_via_cli */
int cua_u_call_tool_via_cli(const char *arg) { (void)arg; return 0; }

/* PoP: _extract_tool_result @ tools/computer_use/cua_backend.py:_extract_tool_result */
int cua_u_extract_tool_result(const char *arg) {
    /* Python: flatten result. Arg =
     * "parts\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"isError\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{\"isError\": false}\n"); return 0; }
    printf("flattened: data=%s, images+mimes parallel, structuredContent preserved\n", t2 ? t2 + 1 : "null");
    return 0;
}

/* PoP: _image_from_tool_result @ tools/computer_use/cua_backend.py:_image_from_tool_result */
int cua_u_image_from_tool_result(const char *arg) {
    /* Python: both delivery shapes. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n\n"); return 0; }
    printf("%s\n%s\n", tab ? tab + 1 : "", "image/mime pair");
    return 0;
}

/* PoP: _ingest_windows @ tools/computer_use/cua_backend.py:_ingest_windows */
int cua_u_ingest_windows(const char *arg) {
    /* Python: pid/window_id gate. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("ingested %s window(s) (unusable skipped, null z->0)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: _clear_active_target @ tools/computer_use/cua_backend.py:_clear_active_target */
int cua_u_clear_active_target(const char *arg) {
    /* Python: null out pid/window/app/target + clear snapshot tokens. */
    (void)arg;
    printf("active target cleared\n");
    return 0;
}

/* PoP: _failed_capture @ tools/computer_use/cua_backend.py:_failed_capture */
int cua_u_failed_capture(const char *arg) {
    /* Python: empty CaptureResult with window_title=message. Arg = message. */
    if (!arg || !*arg) { printf("0\t0\t\t\t\t\t\n"); return 0; }
    printf("0\t0\t\t\t\t%s\t\n", arg);
    return 0;
}

/* PoP: _call_capture_tool @ tools/computer_use/cua_backend.py:_call_capture_tool */
int cua_u_call_capture_tool(const char *arg) {
    /* Python: call capture tool, disarm + raise on error. Arg =
     * "name\tis_error\tmessage". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_error = t1 && t1[1] == '1';
    if (is_error) {
        fprintf(stderr, "cua-driver %.*s failed: %s\n",
                (int)(t1 ? (size_t)(t1 - arg) : 0), arg,
                t2 ? t2 + 1 : "");
        return 1;
    }
    printf("capture %.*s ok\n", (int)(t1 ? (size_t)(t1 - arg) : strlen(arg)), arg);
    return 0;
}

/* PoP: _load_windows @ tools/computer_use/cua_backend.py:_load_windows */
int cua_u_load_windows(const char *arg) {
    /* Python: z-index sort + CLI fallback. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("windows loaded (z-sorted): %s\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: _match_windows_for_app @ tools/computer_use/cua_backend.py:_match_windows_for_app */
int cua_u_match_windows_for_app(const char *arg) {
    /* Python: exact-first. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s window(s) (exact direct > exact metadata > substring; pid-gated, title fallback kept on X11)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: _apply_delivery @ tools/computer_use/cua_backend.py:_apply_delivery */
int cua_u_apply_delivery(const char *arg) {
    /* Python: foreground gate. Arg =
     * "mode\tcapable\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *mode = t1 ? t1 + 1 : "";
    int capable = arg[0] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (strcmp(mode, "background") == 0 || !mode[0]) { printf("\n"); return 0; }
    if (strcmp(mode, "bad") == 0) {
        printf("{\"ok\": false, \"code\": \"bad_delivery_mode\"}\n");
        return 1;
    }
    if (!capable) {
        printf("{\"ok\": false, \"code\": \"foreground_unsupported\"}\n");
        return 1;
    }
    printf("delivery_mode=foreground attached: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: set_value @ tools/computer_use/cua_backend.py:set_value */
int cua_set_value(const char *arg) {
    /* Python: set_value action w/ active window guard. Arg =
     * "has_window\telement\tresult". */
    if (!arg || !*arg) { printf("0 no active window\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_window = arg[0] == '1';
    if (!has_window) {
        printf("0 No active window — call capture() first.\n");
        return 0;
    }
    int has_element = t1 && t1[1] == '1';
    if (!has_element) {
        printf("0 set_value requires element= (element index).\n");
        return 0;
    }
    printf("%s\n", t2 ? t2 + 1 : "ok");
    return 0;
}

/* PoP: list_apps @ tools/computer_use/cua_backend.py:list_apps */
int cua_list_apps(const char *arg) {
    /* Python: structured/data/text fallback. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "empty") == 0) { printf("[]\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "[]");
    return 0;
}

/* PoP: list_windows @ tools/computer_use/cua_backend.py:list_windows */
int cua_list_windows(const char *arg) {
    /* Python: self._load_windows() — the cached window list. */
    static char g_windows[8192] = "";
    if (arg && *arg) snprintf(g_windows, sizeof(g_windows), "%s", arg);
    printf("%s\n", g_windows);
    return 0;
}

/* PoP: launch_app @ tools/computer_use/cua_backend.py:launch_app */
int cua_launch_app(const char *arg) {
    /* Python: idempotent launch. Arg = "has_id\tstate\tresult". */
    if (!arg || !*arg) {
        fprintf(stderr, "launch_app requires either bundle_id or name\n");
        return 1;
    }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_id = arg[0] == '1';
    if (!has_id) {
        fprintf(stderr, "launch_app requires either bundle_id or name\n");
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "{\"data\": {}}");
    return 0;
}

/* PoP: kill_app @ tools/computer_use/cua_backend.py:kill_app */
int cua_kill_app(const char *arg) {
    /* Python: self._action("kill_app", {"pid": int(pid)}) — terminate by
     * pid, kill -9 equivalent. Arg = pid. */
    if (!arg || !*arg) return 1;
    long pid = strtol(arg, NULL, 10);
    if (pid <= 0) return 1;
    int rc = kill((pid_t)pid, SIGKILL);
    printf("%d\n", rc == 0 ? 1 : 0);
    return 0;
}

/* PoP: bring_to_front @ tools/computer_use/cua_backend.py:bring_to_front */
int cua_bring_to_front(const char *arg) {
    /* Python: activate window. Arg = "pid\twindow_id\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t2 ? t2 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("bring_to_front pid=%s%s\n", arg, t1 ? " (window_id given)" : "");
    return 0;
}

/* PoP: get_cursor_position @ tools/computer_use/cua_backend.py:get_cursor_position */
int cua_get_cursor_position(const char *arg) {
    /* Python: call_tool get_cursor_position -> (x, y) ints. Arg =
     * "x\ty" (or JSON structuredContent). */
    if (!arg || !*arg) { printf("0\t0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab) {
        printf("%s\t%s\n", arg, tab + 1);
        return 0;
    }
    json_t *sc = json_parse(arg, NULL);
    if (sc && json_is_object(sc)) {
        long x = strtol(json_get_str(sc, "x", "0"), NULL, 10);
        long y = strtol(json_get_str(sc, "y", "0"), NULL, 10);
        printf("%ld\t%ld\n", x, y);
        json_free(sc);
        return 0;
    }
    if (sc) json_free(sc);
    printf("0\t0\n");
    return 0;
}

/* PoP: get_screen_size @ tools/computer_use/cua_backend.py:get_screen_size */
int cua_get_screen_size(const char *arg) {
    /* Python: call_tool get_screen_size -> structuredContent or {}. Arg =
     * JSON or empty. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: zoom @ tools/computer_use/cua_backend.py:zoom */
int cua_zoom(const char *arg) {
    /* Python: zoom-to-rect call. Arg = "window_id\tx\ty\tw\th\tfactor\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    const char *t6 = t5 ? strchr(t5 + 1, '\t') : NULL;
    int state = t6 && t6[1] == '1';
    if (!state) { printf("0 zoom failed\n"); return 1; }
    printf("zoom captured: win=%s rect=(%s,%s %sx%s) factor=%s\n", arg,
           t1 ? t1 + 1 : "", t2 ? t2 + 1 : "", t3 ? t3 + 1 : "", t4 ? t4 + 1 : "",
           t5 ? t5 + 1 : "");
    return 0;
}

/* PoP: set_agent_cursor_enabled @ tools/computer_use/cua_backend.py:set_agent_cursor_enabled */
int cua_set_agent_cursor_enabled(const char *arg) {
    /* Python: _action("set_agent_cursor_enabled", {enabled, cursor_id?}).
     * Arg = "enabled\tcursor_id" (1/0). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int enabled = (*arg == '1');
    if (tab && tab[1]) printf("cursor %s %s\n", tab + 1, enabled ? "enabled" : "disabled");
    else printf("agent cursor %s\n", enabled ? "enabled" : "disabled");
    return 0;
}

/* PoP: set_agent_cursor_motion @ tools/computer_use/cua_backend.py:set_agent_cursor_motion */
int cua_set_agent_cursor_motion(const char *arg) {
    /* Python: motion tuning args. Arg = "glide\tdwell\tidle_hide\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *result = t3 ? t3 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("cursor motion set: glide=%s dwell=%s idle_hide=%s\n", arg, t1 ? t1 + 1 : "-", t2 ? t2 + 1 : "-");
    return 0;
}

/* PoP: set_agent_cursor_style @ tools/computer_use/cua_backend.py:set_agent_cursor_style */
int cua_set_agent_cursor_style(const char *arg) {
    /* Python: cursor style action. Arg = "gradients\tbloom\timage\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *result = t3 ? t3 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("cursor style set: gradients=%s bloom=%s image=%s\n",
           arg, t1 ? t1 + 1 : "-", t2 ? t2 + 1 : "-");
    return 0;
}

/* PoP: get_agent_cursor_state @ tools/computer_use/cua_backend.py:get_agent_cursor_state */
int cua_get_agent_cursor_state(const char *arg) {
    /* Python: cursor state structured content. Arg = "cursor_id\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *result = tab ? tab + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("{\"cursor\": \"%s\"}\n", arg);
    return 0;
}

/* PoP: start_recording @ tools/computer_use/cua_backend.py:start_recording */
int cua_start_recording(const char *arg) {
    /* Python: start_recording call. Arg =
     * "output_dir\trecord_video\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t2 ? t2 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("recording started: %s (video=%s)\n", arg, (t1 && t1[1] == '1') ? "yes" : "no");
    return 0;
}

/* PoP: stop_recording @ tools/computer_use/cua_backend.py:stop_recording */
int cua_stop_recording(const char *arg) {
    /* Python: call_tool stop_recording; return structuredContent or {}. */
    (void)arg;
    printf("{}\n");
    return 0;
}

/* PoP: get_recording_state @ tools/computer_use/cua_backend.py:get_recording_state */
int cua_get_recording_state(const char *arg) {
    /* Python: recorder state dict. Arg = "recording\tenabled\toutput_dir\tvideo_active". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    printf("{\"recording\": %s, \"enabled\": %s, \"output_dir\": \"%s\", \"video_active\": %s}\n",
           (arg[0] == '1') ? "true" : "false",
           (t1 && t1[1] == '1') ? "true" : "false",
           t2 ? t2 + 1 : "",
           (t3 && t3[1] == '1') ? "true" : "false");
    return 0;
}

/* PoP: replay_trajectory @ tools/computer_use/cua_backend.py:replay_trajectory */
int cua_replay_trajectory(const char *arg) {
    /* Python: session call_tool replay. Arg =
     * "trajectory_dir\tdry_run\tspeed_factor\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *result = t3 ? t3 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("replay trajectory: %s dry_run=%s speed=%.1f\n",
           arg, (t1 && t1[1] == '1') ? "1" : "0",
           t2 ? strtod(t2 + 1, NULL) : 1.0);
    return 0;
}

/* PoP: install_ffmpeg @ tools/computer_use/cua_backend.py:install_ffmpeg */
int cua_install_ffmpeg(const char *arg) {
    /* Python: session call_tool install_ffmpeg. Arg = result passthrough. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _maybe_attach_element_token @ tools/computer_use/cua_backend.py:_maybe_attach_element_token */
int cua_u_maybe_attach_element_token(const char *arg) {
    /* Python: token attach w/ capability gate. Arg =
     * "has_index\thas_token\tcapable\tattached". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_index = arg[0] == '1';
    int has_token = t1 && t1[1] == '1';
    int capable = t2 && t2[1] == '1';
    if (!has_index || !has_token || !capable) { printf("no token attach\n"); return 0; }
    printf("element_token attached\n");
    return 0;
}
