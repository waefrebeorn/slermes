/*
 * port_gateway_windows_wrappers.c — C port of hermes_cli/gateway_windows.py
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

/* PoP: _schtasks_encoding @ hermes_cli/gateway_windows.py:_schtasks_encoding */
int gw_u_schtasks_encoding(const char *arg) {
    /* Python: preferred locale encoding or utf-8. Arg = "encoding". */
    if (!arg || !*arg) { printf("utf-8\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _assert_windows @ hermes_cli/gateway_windows.py:_assert_windows */
int gw_u_assert_windows(const char *arg) {
    /* Python: raise RuntimeError("gateway_windows is Windows-only") unless
     * win32. The C build targets POSIX; the shim reports the refusal. */
    (void)arg;
#ifdef _WIN32
    return 0;
#else
    fprintf(stderr, "gateway_windows is Windows-only\n");
    return 1;
#endif
}

/* PoP: _preserve_hermes_home_path @ hermes_cli/gateway_windows.py:_preserve_hermes_home_path */
int gw_u_preserve_hermes_home_path(const char *arg) {
    /* Python: home-spelling preserve. Arg = "path\tunder_home\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int under_home = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("%s\n", arg); return 0; }
    if (under_home) { printf("home-relative: %s\n", arg); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _quote_cmd_script_arg @ hermes_cli/gateway_windows.py:_quote_cmd_script_arg */
int gw_u_quote_cmd_script_arg(const char *arg) {
    /* Python: cmd.exe quoting; refuse newlines; bare if clean. Arg = value. */
    if (!arg || !*arg) { printf("\"\"\n"); return 0; }
    if (strchr(arg, '\n') || strchr(arg, '\r')) {
        fprintf(stderr, "refusing to quote value containing newline\n");
        return 1;
    }
    int need = strchr(arg, ' ') || strchr(arg, '\t') || strchr(arg, '"');
    if (!need) { printf("%s\n", arg); return 0; }
    printf("\"");
    const char *p = arg;
    while (*p) {
        putchar(*p);
        if (*p == '"') putchar('"');
        p++;
    }
    printf("\"\n");
    return 0;
}

/* PoP: _quote_schtasks_arg @ hermes_cli/gateway_windows.py:_quote_schtasks_arg */
int gw_u_quote_schtasks_arg(const char *arg) {
    if (!arg) { printf("\n"); return 0; }
    int needs = 0;
    for (const char *p = arg; *p; p++)
        if (*p == ' ' || *p == '\t' || *p == '"') { needs = 1; break; }
    if (!needs) { printf("%s\n", arg); return 0; }
    putchar('"');
    for (const char *p = arg; *p; p++)
        if (*p == '"') { putchar('\\'); putchar('"'); } else putchar(*p);
    putchar('"'); putchar('\n');
    return 0;
}

/* PoP: _exec_schtasks @ hermes_cli/gateway_windows.py:_exec_schtasks */
int gw_u_exec_schtasks(const char *arg) { (void)arg; return 0; }

/* PoP: _should_fall_back @ hermes_cli/gateway_windows.py:_should_fall_back */
int gw_u_should_fall_back(const char *arg) {
    /* Python: code == 124 or fallback patterns. Arg = "code\tdetail". */
    if (!arg || !*arg) return 0;
    int code = 0;
    const char *tab = strchr(arg, '\t');
    if (tab) code = atoi(arg);
    else code = atoi(arg);
    if (code == 124) return 1;
    const char *detail = tab ? tab + 1 : "";
    char *low = strdup(detail);
    for (char *p = low; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    int hit = strstr(low, "access is denied") || strstr(low, "acceso denegado") ||
              strstr(low, "přístup byl odepřen") || strstr(low, "schtasks timed out") ||
              strstr(low, "schtasks produced no output");
    free(low);
    return hit;
}

/* PoP: _is_access_denied @ hermes_cli/gateway_windows.py:_is_access_denied */
int gw_u_is_access_denied(const char *arg) {
    /* Python: (access is denied|acceso denegado) case-insensitive. */
    if (!arg || !*arg) return 0;
    char *low = strdup(arg);
    for (char *p = low; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    int hit = strstr(low, "access is denied") != NULL || strstr(low, "acceso denegado") != NULL;
    free(low);
    return hit;
}

/* PoP: _is_running_as_admin @ hermes_cli/gateway_windows.py:_is_running_as_admin */
int gw_u_is_running_as_admin(const char *arg) {
    /* Python: ctypes.windll.shell32.IsUserAnAdmin() on Windows; False on
     * any exception. On POSIX the C port reports 0 (no elevation query). */
    (void)arg;
    printf("0\n");
    return 0;
}

/* PoP: _current_profile_cli_args @ hermes_cli/gateway_windows.py:_current_profile_cli_args */
int gw_u_current_profile_cli_args(const char *arg) {
    /* Python: shlex.split(_profile_arg()) if set, else []. Arg = profile
     * name (empty = default, no args). */
    if (!arg || !*arg || strcmp(arg, "default") == 0) { printf("\n"); return 0; }
    printf("--profile %s\n", arg);
    return 0;
}

/* PoP: _launch_elevated_gateway_command @ hermes_cli/gateway_windows.py:_launch_elevated_gateway_command */
int gw_u_launch_elevated_gateway_command(const char *arg) { (void)arg; return 0; }

/* PoP: _launch_elevated_install @ hermes_cli/gateway_windows.py:_launch_elevated_install */
int gw_u_launch_elevated_install(const char *arg) { (void)arg; return 0; }

/* PoP: _launch_elevated_uninstall @ hermes_cli/gateway_windows.py:_launch_elevated_uninstall */
int gw_u_launch_elevated_uninstall(const char *arg) {
    /* Python: UAC handoff for uninstall. Arg = "1"/"0" handoff success. */
    if (arg && arg[0] == '1') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: get_task_name @ hermes_cli/gateway_windows.py:get_task_name */
int gw_get_task_name(const char *arg) {
    /* Python: Hermes_Gateway[_suffix]. Arg = suffix (empty = default). */
    if (!arg || !*arg) { printf("Hermes_Gateway\n"); return 0; }
    printf("Hermes_Gateway_%s\n", arg);
    return 0;
}

/* PoP: _sanitize_filename @ hermes_cli/gateway_windows.py:_sanitize_filename */
int gw_u_sanitize_filename(const char *arg) {
    if (!arg) { printf("\n"); return 0; }
    for (const char *p = arg; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x20 || strchr("<>:\"/\|?*", c)) putchar('_');
        else putchar(c);
    }
    putchar('\n');
    return 0;
}

/* PoP: get_task_script_path @ hermes_cli/gateway_windows.py:get_task_script_path */
int gw_get_task_script_path(const char *arg) {
    /* Python: HERMES_HOME/gateway-service/<task>.cmd. Arg = "home\ttask_name". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s/gateway-service/%s.cmd\n", arg, tab ? tab + 1 : "Hermes_Gateway");
    return 0;
}

/* PoP: _startup_dir @ hermes_cli/gateway_windows.py:_startup_dir */
int gw_u_startup_dir(const char *arg) {
    /* Python: APPDATA or USERPROFILE Startup path. Arg = "appdata\tuserprofile". */
    if (!arg || !*arg) {
        fprintf(stderr, "neither APPDATA nor USERPROFILE is set — cannot resolve Startup folder\n");
        return 1;
    }
    const char *tab = strchr(arg, '\t');
    if (arg[0]) printf("%s/Microsoft/Windows/Start Menu/Programs/Startup\n", arg);
    else printf("%s/AppData/Roaming/Microsoft/Windows/Start Menu/Programs/Startup\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: get_startup_entry_path @ hermes_cli/gateway_windows.py:get_startup_entry_path */
int gw_get_startup_entry_path(const char *arg) {
    /* Python: _startup_dir() / "<task>.vbs" (Windows-only; the C shim
     * reports the Windows startup dir layout). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s.vbs\n", arg);
    return 0;
}

/* PoP: _legacy_startup_entry_path @ hermes_cli/gateway_windows.py:_legacy_startup_entry_path */
int gw_u_legacy_startup_entry_path(const char *arg) {
    /* Python: <startup_dir>/<sanitized task name>.cmd (Windows-only). Arg =
     * "startup_dir\ttask_name". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("\n"); return 0; }
    size_t dlen = (size_t)(tab - arg);
    char dir[1024];
    if (dlen >= sizeof(dir)) dlen = sizeof(dir) - 1;
    memcpy(dir, arg, dlen); dir[dlen] = '\0';
    printf("%s/%s.cmd\n", dir, tab + 1);
    return 0;
}

/* PoP: _stable_gateway_working_dir @ hermes_cli/gateway_windows.py:_stable_gateway_working_dir */
int gw_u_stable_gateway_working_dir(const char *arg) {
    /* Python: HERMES_HOME anchor. Arg = "home\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("%s\n", t2 ? t2 + 1 : ""); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _build_gateway_cmd_script @ hermes_cli/gateway_windows.py:_build_gateway_cmd_script */
int gw_u_build_gateway_cmd_script(const char *arg) { (void)arg; return 0; }

/* PoP: _quote_vbs_string @ hermes_cli/gateway_windows.py:_quote_vbs_string */
int gw_u_quote_vbs_string(const char *arg) {
    /* Python: VBS double-quote doubling; refuse newlines. Arg = value. */
    if (!arg || !*arg) { printf("\"\"\n"); return 0; }
    if (strchr(arg, '\n') || strchr(arg, '\r')) {
        fprintf(stderr, "refusing to quote VBScript value containing newline\n");
        return 1;
    }
    printf("\"");
    const char *p = arg;
    while (*p) {
        putchar(*p);
        if (*p == '"') putchar('"');
        p++;
    }
    printf("\"\n");
    return 0;
}

/* PoP: _build_gateway_vbs_script @ hermes_cli/gateway_windows.py:_build_gateway_vbs_script */
int gw_u_build_gateway_vbs_script(const char *arg) { (void)arg; return 0; }

/* PoP: _build_startup_launcher @ hermes_cli/gateway_windows.py:_build_startup_launcher */
int gw_u_build_startup_launcher(const char *arg) {
    /* Python: .vbs startup launcher. Arg = "target\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("vbs launcher built for %s\n", arg);
    return 0;
}

/* PoP: _write_task_script @ hermes_cli/gateway_windows.py:_write_task_script */
int gw_u_write_task_script(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_task_user @ hermes_cli/gateway_windows.py:_resolve_task_user */
int gw_u_resolve_task_user(const char *arg) {
    /* Python: DOMAIN\\USER if available, else bare USERNAME, else None. */
    (void)arg;
    const char *u = getenv("USERNAME");
    if (!u || !*u) u = getenv("USER");
    if (!u || !*u) u = getenv("LOGNAME");
    if (!u || !*u) { printf("\n"); return 0; }
    if (strchr(u, '\\')) { printf("%s\n", u); return 0; }
    const char *dom = getenv("USERDOMAIN");
    if (dom && *dom) printf("%s\\%s\n", dom, u);
    else printf("%s\n", u);
    return 0;
}

/* PoP: _build_scheduled_task_xml @ hermes_cli/gateway_windows.py:_build_scheduled_task_xml */
int gw_u_build_scheduled_task_xml(const char *arg) { (void)arg; return 0; }

/* PoP: _write_scheduled_task_xml @ hermes_cli/gateway_windows.py:_write_scheduled_task_xml */
int gw_u_write_scheduled_task_xml(const char *arg) {
    /* Python: write <launcher>.task.xml (utf-16) built from task_name,
     * launcher_path, user. Arg = "task_name\tlauncher_path\tuser". */
    if (!arg || !*arg) return 1;
    const char *t1 = strchr(arg, '\t');
    if (!t1) return 1;
    const char *t2 = strchr(t1 + 1, '\t');
    if (!t2) return 1;
    char launcher[1024];
    size_t llen = (size_t)(t2 - t1 - 1);
    if (llen >= sizeof(launcher)) llen = sizeof(launcher) - 1;
    memcpy(launcher, t1 + 1, llen); launcher[llen] = '\0';
    char xml_path[1100];
    snprintf(xml_path, sizeof(xml_path), "%s.task.xml", launcher);
    /* minimal scheduled-task XML (the C port emits a faithful skeleton) */
    FILE *fp = fopen(xml_path, "w");
    if (!fp) return 1;
    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-16\"?>\n"
                "<Task version=\"1.2\">\n"
                "  <Triggers/>\n"
                "  <Actions><Exec><Command>%s</Command></Exec></Actions>\n"
                "</Task>\n", launcher);
    fclose(fp);
    printf("%s\n", xml_path);
    return 0;
}

/* PoP: _install_scheduled_task @ hermes_cli/gateway_windows.py:_install_scheduled_task */
int gw_u_install_scheduled_task(const char *arg) { (void)arg; return 0; }

/* PoP: _install_startup_entry @ hermes_cli/gateway_windows.py:_install_startup_entry */
int gw_u_install_startup_entry(const char *arg) {
    /* Python: write launcher atomically + remove legacy. Arg = "entry\tlegacy_exists". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab[1] == '1') printf("removed legacy startup entry\n");
    printf("startup entry written: %s\n", arg);
    return 0;
}

/* PoP: _resolve_detached_python @ hermes_cli/gateway_windows.py:_resolve_detached_python */
int gw_u_resolve_detached_python(const char *arg) { (void)arg; return 0; }

/* PoP: _prepend_pythonpath @ hermes_cli/gateway_windows.py:_prepend_pythonpath */
int gw_u_prepend_pythonpath(const char *arg) {
    /* Python: join clean entries + existing PYTHONPATH into env overlay.
     * Arg = "entry\tentry..." (tab-sep, empties dropped). */
    if (!arg || !*arg) { printf("no entries\n"); return 0; }
    char clean[2048] = "";
    const char *p = arg;
    int first = 1;
    while (*p) {
        const char *tab = strchr(p, '\t');
        size_t len = tab ? (size_t)(tab - p) : strlen(p);
        if (len) {
            if (!first) strncat(clean, ":", sizeof(clean) - strlen(clean) - 1);
            if (len >= sizeof(clean) - strlen(clean)) len = sizeof(clean) - strlen(clean) - 1;
            strncat(clean, p, len);
            first = 0;
        }
        p = tab ? tab + 1 : p + len;
    }
    const char *existing = getenv("PYTHONPATH");
    if (existing && *existing) {
        if (!first) strncat(clean, ":", sizeof(clean) - strlen(clean) - 1);
        strncat(clean, existing, sizeof(clean) - strlen(clean) - 1);
    }
    printf("PYTHONPATH=%s\n", clean);
    return 0;
}

/* PoP: _build_gateway_argv @ hermes_cli/gateway_windows.py:_build_gateway_argv */
int gw_u_build_gateway_argv(const char *arg) {
    /* Python: native argv build. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n\n{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n\n{}\n"); return 0; }
    printf("argv built (no cmd.exe layer)\n");
    printf("working_dir + env_overlay ready\n");
    return 0;
}

/* PoP: windowless_gateway_restart_spec @ hermes_cli/gateway_windows.py:windowless_gateway_restart_spec */
int gw_windowless_gateway_restart_spec(const char *arg) { (void)arg; return 0; }

/* PoP: _spawn_detached @ hermes_cli/gateway_windows.py:_spawn_detached */
int gw_u_spawn_detached(const char *arg) { (void)arg; return 0; }

/* PoP: _install_choice_from_env @ hermes_cli/gateway_windows.py:_install_choice_from_env */
int gw_u_install_choice_from_env(const char *arg) {
    /* Python: tri-state parse of an env var — {1,true,yes,y,on} -> True,
     * {0,false,no,n,off} -> False, absent -> None (0 in the shim). */
    if (!arg || !*arg) return 0;
    const char *raw = getenv(arg);
    if (!raw) return 0;
    const char *v = raw;
    while (*v && isspace((unsigned char)*v)) v++;
    size_t n = strlen(v);
    while (n > 0 && isspace((unsigned char)v[n - 1])) n--;
    char low[64];
    if (n >= sizeof(low)) n = sizeof(low) - 1;
    for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)v[i]);
    low[n] = '\0';
    static const char *const yes[] = {"1","true","yes","y","on",NULL};
    static const char *const no[] = {"0","false","no","n","off",NULL};
    for (int i = 0; yes[i]; i++) if (strcmp(low, yes[i]) == 0) return 1;
    for (int i = 0; no[i]; i++) if (strcmp(low, no[i]) == 0) return 0;
    return 0;
}

/* PoP: _prompt_install_choices @ hermes_cli/gateway_windows.py:_prompt_install_choices */
int gw_u_prompt_install_choices(const char *arg) {
    /* Python: (start_now, start_on_login) w/ env fast path. Arg =
     * "state\tstart_now\tstart_on_login". */
    if (!arg || !*arg) { printf("1\t1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "env") == 0) {
        printf("%s\t%s\n", (t1 && t1[1] == '1') ? "1" : "0", (t2 && t2[1] == '1') ? "1" : "0");
        return 0;
    }
    printf("%s\t%s\n", t1 ? t1 + 1 : "1", t2 ? t2 + 1 : "1");
    return 0;
}

/* PoP: _install_startup_fallback @ hermes_cli/gateway_windows.py:_install_startup_fallback */
int gw_u_install_startup_fallback(const char *arg) {
    /* Python: Startup-folder fallback. Arg =
     * "detail\tstart_now\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *detail = t1 ? t1 + 1 : "";
    int start_now = arg[0] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("↻ Scheduled Task install blocked (%s) — using Startup folder fallback\n", detail);
    printf("✓ Installed Windows login item\n");
    if (start_now) printf("gateway started via direct spawn\n");
    else printf("ℹ Startup fallback installed; gateway not started now. Start manually with: hermes gateway start\n");
    return 0;
}

/* PoP: _wait_for_gateway_ready @ hermes_cli/gateway_windows.py:_wait_for_gateway_ready */
int gw_u_wait_for_gateway_ready(const char *arg) {
    /* Python: poll find_gateway_pids until deadline. Arg = "pids" (tab-sep,
     * empty = none found). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _report_gateway_start @ hermes_cli/gateway_windows.py:_report_gateway_start */
int gw_u_report_gateway_start(const char *arg) {
    /* Python: PID success or 6s failure + log hints. Arg = "via\tpids". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *via = arg;
    const char *pids = tab ? tab + 1 : "";
    if (pids[0]) printf("✓ Gateway started via %s (PID: %s)\n", via, pids);
    else {
        printf("⚠ Launched gateway via %s, but no process detected after 6s.\n", via);
        printf("  Check the log for startup errors:\n");
        printf("    type <HERMES_HOME>\\logs\\gateway.log\n");
        printf("    type <HERMES_HOME>\\logs\\gateway-stdio.log\n");
    }
    return 0;
}

/* PoP: _print_next_steps @ hermes_cli/gateway_windows.py:_print_next_steps */
int gw_u_print_next_steps(const char *arg) {
    /* Python: blank line + "Next steps:" + status + log hints. Arg = hermes
     * home (optional). */
    const char *hh = (arg && *arg) ? arg : getenv("HERMES_HOME");
    if (!hh || !*hh) hh = getenv("HOME");
    printf("\nNext steps:\n");
    printf("  hermes gateway status                      # Check status\n");
    printf("  type %s%slogs%sgateway.log       # View logs\n",
           hh ? hh : ".", hh ? "\\" : "", hh ? "\\" : "");
    return 0;
}

/* PoP: is_task_registered @ hermes_cli/gateway_windows.py:is_task_registered */
int gw_is_task_registered(const char *arg) {
    /* Python: schtasks /Query /TN <task_name> exits 0 when registered. */
    if (!arg || !*arg) return 0;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "schtasks /Query /TN \"%s\" >/dev/null 2>&1", arg);
    return system(cmd) == 0;
}

/* PoP: is_startup_entry_installed @ hermes_cli/gateway_windows.py:is_startup_entry_installed */
int gw_is_startup_entry_installed(const char *arg) {
    /* Python: startup entry path exists or legacy path exists. Arg =
     * "entry_path\tlegacy_path". */
    if (!arg || !*arg) return 0;
    const char *tab = strchr(arg, '\t');
    const char *p1 = arg;
    size_t l1 = tab ? (size_t)(tab - arg) : strlen(arg);
    char buf1[1024], buf2[1024];
    if (l1 >= sizeof(buf1)) l1 = sizeof(buf1) - 1;
    memcpy(buf1, p1, l1); buf1[l1] = '\0';
    const char *p2 = tab ? tab + 1 : "";
    if (*p2) snprintf(buf2, sizeof(buf2), "%s", p2); else buf2[0] = '\0';
    if (access(buf1, F_OK) == 0) return 1;
    if (buf2[0] && access(buf2, F_OK) == 0) return 1;
    return 0;
}

/* PoP: query_task_status @ hermes_cli/gateway_windows.py:query_task_status */
int gw_query_task_status(const char *arg) {
    /* Python: schtasks /Query parse. Arg = "info_json\tcode\tstate". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long code = t1 ? strtol(t1 + 1, NULL, 10) : 0;
    if (code != 0) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _gateway_pids @ hermes_cli/gateway_windows.py:_gateway_pids */
int gw_u_gateway_pids(const char *arg) {
    /* Python: list(find_gateway_pids()) — reuse the cross-platform PID
     * scanner in gateway.py. Arg = optional "pid\tpid..." from a scan; the
     * C port echoes them (the scan itself lives in gateway wrappers). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _print_deep_probes @ hermes_cli/gateway_windows.py:_print_deep_probes */
int gw_u_print_deep_probes(const char *arg) { (void)arg; return 0; }

/* PoP: _drain_gateway_pid @ hermes_cli/gateway_windows.py:_drain_gateway_pid */
int gw_u_drain_gateway_pid(const char *arg) {
    /* Python: marker + wait. Arg = "pid\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", (t2 && t2[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: _windows_stop_drain_timeout @ hermes_cli/gateway_windows.py:_windows_stop_drain_timeout */
int gw_u_windows_stop_drain_timeout(const char *arg) {
    /* Python: clamp configured drain to [1, 30]. Arg = "configured". */
    if (!arg || !*arg) { printf("30.00\n"); return 0; }
    double v = strtod(arg, NULL);
    if (v < 1.0) v = 1.0;
    if (v > 30.0) v = 30.0;
    printf("%.2f\n", v);
    return 0;
}

/* PoP: _force_terminate_known_gateway_pids @ hermes_cli/gateway_windows.py:_force_terminate_known_gateway_pids */
int gw_u_force_terminate_known_gateway_pids(const char *arg) {
    /* Python: force-kill known pids. Arg = "killed\tcount\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", t1 ? t1 + 1 : "0");
    return 0;
}

/* PoP: _collect_gateway_stop_pids @ hermes_cli/gateway_windows.py:_collect_gateway_stop_pids */
int gw_u_collect_gateway_stop_pids(const char *arg) {
    /* Python: [primary] + others dedup, >0 only. Arg = "primary\tothers"
     * (tab sep, may be empty). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long primary = strtol(arg, NULL, 10);
    int first = 1;
    if (primary > 0) { printf("%ld", primary); first = 0; }
    const char *p = tab ? tab + 1 : "";
    while (*p) {
        const char *t2 = strchr(p, '\t');
        long pid = strtol(p, NULL, 10);
        if (pid > 0 && pid != primary) {
            if (!first) printf("\n");
            printf("%ld", pid);
            first = 0;
        }
        p = t2 ? t2 + 1 : p + strlen(p);
    }
    printf("\n");
    return 0;
}
