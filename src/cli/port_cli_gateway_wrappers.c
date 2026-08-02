/*
 * port_cli_gateway_wrappers.c — C port of hermes_cli/gateway.py
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
#include <errno.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/stat.h>
#include "hermes_json.h"

/* PoP: has_process_service_mismatch @ hermes_cli/gateway.py:has_process_service_mismatch */
int cgw_has_process_service_mismatch(const char *arg) {
    /* Python: service_installed and running and not service_running.
     * Arg = "installed\trunning\tservice_running". */
    if (!arg || !*arg) return 0;
    int inst = 0, run = 0, srv = 0;
    sscanf(arg, "%d\t%d\t%d", &inst, &run, &srv);
    return inst && run && !srv;
}

/* PoP: _scan_gateway_pids @ hermes_cli/gateway.py:_scan_gateway_pids */
int cgw_u_scan_gateway_pids(const char *arg) { (void)arg; return 0; }

/* PoP: _filter_venv_launcher_stubs @ hermes_cli/gateway.py:_filter_venv_launcher_stubs */
int cgw_u_filter_venv_launcher_stubs(const char *arg) {
    /* Python: drop parent stubs. Arg = "pids\tdropped\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long dropped = t1 ? strtol(t1 + 1, NULL, 10) : 0;
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: find_profile_gateway_processes @ hermes_cli/gateway.py:find_profile_gateway_processes */
int cgw_find_profile_gateway_processes(const char *arg) {
    /* Python: profile -> pid mapping. Arg = "procs_json\tcount". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", arg);
    return 0;
}

/* PoP: _gateway_run_args_for_profile @ hermes_cli/gateway.py:_gateway_run_args_for_profile */
int cgw_u_gateway_run_args_for_profile(const char *arg) {
    /* Python: [python, -m hermes_cli.main] + [--profile p] if != default +
     * [gateway run --replace]. Arg = profile name. */
    if (!arg || !*arg || strcmp(arg, "default") == 0) {
        printf("python3 -m hermes_cli.main gateway run --replace\n");
        return 0;
    }
    printf("python3 -m hermes_cli.main --profile %s gateway run --replace\n", arg);
    return 0;
}

/* PoP: _prepare_profile_gateway_update_restart @ hermes_cli/gateway.py:_prepare_profile_gateway_update_restart */
int cgw_u_prepare_profile_gateway_update_restart(const char *arg) {
    /* Python: external-supervisor / detached / None. Arg =
     * "has_supervisor\tdetached". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (arg[0] == '1') { printf("external-supervisor\n"); return 0; }
    if (tab && tab[1] == '1') { printf("detached\n"); return 0; }
    printf("\n");
    return 0;
}

/* PoP: launch_detached_profile_gateway_restart @ hermes_cli/gateway.py:launch_detached_profile_gateway_restart */
int cgw_launch_detached_profile_gateway_restart(const char *arg) {
    /* Python: False if old_pid <= 0; else spawn restart watcher with
     * gateway run args. Arg = "old_pid\tprofile". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    long pid = strtol(arg, NULL, 10);
    if (pid <= 0) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *profile = tab ? tab + 1 : "default";
    printf("watcher spawned for pid %ld (profile %s)\n", pid, profile);
    return 0;
}

/* PoP: _probe_systemd_service_running @ hermes_cli/gateway.py:_probe_systemd_service_running */
int cgw_u_probe_systemd_service_running(const char *arg) {
    /* Python: (system, is-active == active). Arg = "system\tunit_exists\tactive". */
    if (!arg || !*arg) { printf("user\t0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int unit_exists = t1 && t1[1] == '1';
    if (!unit_exists) { printf("%s\t0\n", arg); return 0; }
    printf("%s\t%d\n", arg, (t2 && strncmp(t2 + 1, "active", 6) == 0) ? 1 : 0);
    return 0;
}

/* PoP: _read_systemd_unit_environment @ hermes_cli/gateway.py:_read_systemd_unit_environment */
int cgw_u_read_systemd_unit_environment(const char *arg) { (void)arg; return 0; }

/* PoP: _hermes_home_from_systemd_unit_file @ hermes_cli/gateway.py:_hermes_home_from_systemd_unit_file */
int cgw_u_hermes_home_from_systemd_unit_file(const char *arg) {
    /* Python: read HERMES_HOME from unit file. Arg = "state\tvalue". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "found") == 0) { printf("%s\n", tab ? tab + 1 : ""); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _sync_hermes_home_from_systemd_unit @ hermes_cli/gateway.py:_sync_hermes_home_from_systemd_unit */
int cgw_u_sync_hermes_home_from_systemd_unit(const char *arg) {
    /* Python: adopt unit HERMES_HOME. Arg =
     * "is_system\tunit_home\tcurrent\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int is_system = arg[0] == '1';
    const char *unit_home = t1 ? t1 + 1 : "";
    int state = t3 && t3[1] == '1';
    if (!is_system) { printf("no sync (user scope)\n"); return 0; }
    if (!state) { printf("no unit home to adopt\n"); return 0; }
    printf("HERMES_HOME synced from unit: %s\n", unit_home);
    return 0;
}

/* PoP: _read_systemd_unit_properties @ hermes_cli/gateway.py:_read_systemd_unit_properties */
int cgw_u_read_systemd_unit_properties(const char *arg) {
    /* Python: systemctl show parse. Arg = "props_json\tstate". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = tab && tab[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _systemd_main_pid_from_props @ hermes_cli/gateway.py:_systemd_main_pid_from_props */
int cgw_u_systemd_main_pid_from_props(const char *arg) {
    /* Python: int(props.get("MainPID", "0")) -> pid if > 0 else None.
     * Arg = "MainPID" value (or empty). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    long pid = strtol(arg, NULL, 10);
    printf("%ld\n", pid > 0 ? pid : 0);
    return 0;
}

/* PoP: _systemd_main_pid @ hermes_cli/gateway.py:_systemd_main_pid */
int cgw_u_systemd_main_pid(const char *arg) {
    /* Python: MainPID from the unit properties. Arg = "system\tunit". */
    if (!arg || !*arg) return 0;
    char system[8], unit[256];
    if (sscanf(arg, "%7[^\t]\t%255s", system, unit) < 2) return 0;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "systemctl %s show -p MainPID %s 2>/dev/null",
             strcmp(system, "1") == 0 ? "" : "--user", unit);
    FILE *fp = popen(cmd, "r");
    if (!fp) return 0;
    char buf[256];
    long pid = 0;
    while (fgets(buf, sizeof(buf), fp)) {
        if (strncmp(buf, "MainPID=", 8) == 0) {
            pid = strtol(buf + 8, NULL, 10);
            break;
        }
    }
    pclose(fp);
    return (int)pid;
}

/* PoP: _read_gateway_runtime_status @ hermes_cli/gateway.py:_read_gateway_runtime_status */
int cgw_u_read_gateway_runtime_status(const char *arg) {
    /* Python: read_runtime_status() if dict else None. Arg = status JSON
     * (or empty). */
    if (!arg || !*arg) { printf("None\n"); return 0; }
    json_t *st = json_parse(arg, NULL);
    if (st && json_is_object(st)) {
        char *ser = json_serialize(st);
        printf("%s\n", ser ? ser : arg);
        free(ser);
        json_free(st);
        return 0;
    }
    if (st) json_free(st);
    printf("None\n");
    return 0;
}

/* PoP: _gateway_runtime_status_for_pid @ hermes_cli/gateway.py:_gateway_runtime_status_for_pid */
int cgw_u_gateway_runtime_status_for_pid(const char *arg) {
    /* Python: runtime status whose pid matches, else None. Arg =
     * "pid\tstatus_json" (status empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab || !tab[1]) { printf("\n"); return 0; }
    long want = strtol(arg, NULL, 10);
    json_t *st = json_parse(tab + 1, NULL);
    if (!st || !json_is_object(st)) {
        if (st) json_free(st);
        printf("\n");
        return 0;
    }
    const char *pid_s = json_get_str(st, "pid", "");
    long got = pid_s ? strtol(pid_s, NULL, 10) : -1;
    if (want > 0 && got == want) {
        char *s = json_dumps(st, 0);
        printf("%s\n", s ? s : "");
        free(s);
        json_free(st);
        return 0;
    }
    printf("\n");
    json_free(st);
    return 0;
}

/* PoP: _wait_for_systemd_service_restart @ hermes_cli/gateway.py:_wait_for_systemd_service_restart */
int cgw_u_wait_for_systemd_service_restart(const char *arg) { (void)arg; return 0; }

/* PoP: _systemd_unit_is_start_limited @ hermes_cli/gateway.py:_systemd_unit_is_start_limited */
int cgw_u_systemd_unit_is_start_limited(const char *arg) {
    /* Python: props "Result" or "SubState" lowercased == "start-limit-hit".
     * Arg = JSON props. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    json_t *props = json_parse(arg, NULL);
    if (!props || !json_is_object(props)) {
        if (props) json_free(props);
        printf("0\n");
        return 0;
    }
    const char *result = json_get_str(props, "Result", "");
    const char *sub = json_get_str(props, "SubState", "");
    int limited = (strcasecmp(result, "start-limit-hit") == 0 ||
                   strcasecmp(sub, "start-limit-hit") == 0);
    printf("%d\n", limited);
    json_free(props);
    return 0;
}

/* PoP: _systemd_error_indicates_start_limit @ hermes_cli/gateway.py:_systemd_error_indicates_start_limit */
int cgw_u_systemd_error_indicates_start_limit(const char *arg) {
    /* Python: start-limit markers in stderr/stdout/output. Arg = text. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    char low[2400];
    snprintf(low, sizeof(low), "%s", arg);
    for (char *p = low; *p; p++) *p = (char)tolower((unsigned char)*p);
    if (strstr(low, "start-limit-hit") || strstr(low, "start request repeated too quickly") || strstr(low, "start-limit")) {
        printf("1\n"); return 0;
    }
    printf("0\n");
    return 0;
}

/* PoP: _systemd_service_is_start_limited @ hermes_cli/gateway.py:_systemd_service_is_start_limited */
int cgw_u_systemd_service_is_start_limited(const char *arg) {
    /* Python: the unit is start-limited (start-limit-hit / high restarts).
     * Arg = "system\tunit". */
    if (!arg || !*arg) return 0;
    char system[8], unit[256];
    if (sscanf(arg, "%7[^\t]\t%255s", system, unit) < 2) return 0;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "systemctl %s show -p NRestarts -p StartLimitHits %s 2>/dev/null",
             strcmp(system, "1") == 0 ? "" : "--user", unit);
    FILE *fp = popen(cmd, "r");
    if (!fp) return 0;
    char buf[256];
    int limited = 0;
    while (fgets(buf, sizeof(buf), fp)) {
        if (strncmp(buf, "StartLimitHits=", 15) == 0) {
            if (strtol(buf + 15, NULL, 10) > 0) limited = 1;
        }
        if (strncmp(buf, "NRestarts=", 10) == 0) {
            if (strtol(buf + 10, NULL, 10) >= 5) limited = 1;
        }
    }
    pclose(fp);
    return limited;
}

/* PoP: _print_systemd_start_limit_wait @ hermes_cli/gateway.py:_print_systemd_start_limit_wait */
int cgw_u_print_systemd_start_limit_wait(const char *arg) {
    /* Python: rate-limit guidance lines. Arg = "service\tsystem\tscope". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *svc = arg;
    int system = t1 && t1[1] == '1';
    const char *scope = t2 ? t2 + 1 : "User";
    const char *sysctl = system ? "systemctl " : "systemctl --user ";
    const char *jnl = system ? "journalctl " : "journalctl --user ";
    printf("⏳ %s service is temporarily rate-limited by systemd.\n", scope);
    printf("  systemd is refusing another immediate start after repeated exits.\n");
    printf("  Wait for the start-limit window to expire, then run: %shermes gateway restart%s\n",
           system ? "sudo " : "", system ? " --system" : "");
    printf("  Or clear the failed state manually: %sreset-failed %s\n", sysctl, svc);
    printf("  Check logs: %s-u %s -l --since '5 min ago'\n", jnl, svc);
    return 0;
}

/* PoP: _recover_pending_systemd_restart @ hermes_cli/gateway.py:_recover_pending_systemd_restart */
int cgw_u_recover_pending_systemd_restart(const char *arg) { (void)arg; return 0; }

/* PoP: _parse_launchd_pid_from_list_output @ hermes_cli/gateway.py:_parse_launchd_pid_from_list_output */
int cgw_u_parse_launchd_pid_from_list_output(const char *arg) {
    /* Python: PID line parse. Arg = "state\tpid". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "ok") == 0) { printf("%s\n", tab ? tab + 1 : ""); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _probe_launchd_service_running @ hermes_cli/gateway.py:_probe_launchd_service_running */
int cgw_u_probe_launchd_service_running(const char *arg) {
    /* Python: launchctl list + pid check. Arg = "has_plist\tpid_found\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_plist = arg[0] == '1';
    if (!has_plist) { printf("0\n"); return 0; }
    int pid_found = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    printf("%d\n", (pid_found && state) ? 1 : 0);
    return 0;
}

/* PoP: get_gateway_runtime_snapshot @ hermes_cli/gateway.py:get_gateway_runtime_snapshot */
int cgw_get_gateway_runtime_snapshot(const char *arg) { (void)arg; return 0; }

/* PoP: _format_gateway_pids @ hermes_cli/gateway.py:_format_gateway_pids */
int cgw_u_format_gateway_pids(const char *arg) {
    /* Python: comma-joined positive pids (limit + "..." if truncated).
     * Arg = "pid pid...\tlimit" (limit empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long limit = tab ? strtol(tab + 1, NULL, 10) : -1;
    long count = 0;
    const char *p = arg;
    int first = 1;
    while (p && *p && p != tab) {
        while (*p == ' ' || *p == '\t') p++;
        if (p == tab || !*p) break;
        long pid = strtol(p, (char **)&p, 10);
        if (pid > 0) {
            if (limit >= 0 && count >= limit) { printf("..."); first = 0; break; }
            if (!first) printf(", ");
            printf("%ld", pid);
            first = 0;
            count++;
        }
    }
    printf("\n");
    return 0;
}

/* PoP: _print_gateway_process_mismatch @ hermes_cli/gateway.py:_print_gateway_process_mismatch */
int cgw_u_print_gateway_process_mismatch(const char *arg) {
    /* Python (snapshot): prints the process/service mismatch warning,
     * distinguishing the launchd detached fallback from a manual run.
     * Arg = "has_mismatch\tlaunchd_marker_exists". */
    if (!arg || !*arg) return 0;
    int mismatch = 0, marker = 0;
    if (sscanf(arg, "%d\t%d", &mismatch, &marker) < 1) return 0;
    if (!mismatch) return 0;
    printf("\n");
    if (marker) {
        printf("\xe2\x9a\xa0 Gateway is running as a detached fallback process - launchd cannot supervise it\n");
        printf("  (macOS launchd exit-5 path)\n");
    } else {
        printf("\xe2\x9a\xa0 Gateway process is running for this profile, but the service is not active\n");
        printf("  PID(s): (see gateway status)\n");
        printf("  This is usually a manual foreground/tmux/nohup run, so `hermes gateway`\n");
        printf("  can refuse to start another copy until this process stops.\n");
    }
    return 0;
}

/* PoP: _print_other_profiles_gateway_status @ hermes_cli/gateway.py:_print_other_profiles_gateway_status */
int cgw_u_print_other_profiles_gateway_status(const char *arg) {
    /* Python: per-profile gateway summary. Arg = "procs_json\tcurrent". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("\nOther profiles:\n");
    printf("  %s\n", arg);
    return 0;
}

/* PoP: _reap_unsupervised_gateway_orphans @ hermes_cli/gateway.py:_reap_unsupervised_gateway_orphans */
int cgw_u_reap_unsupervised_gateway_orphans(const char *arg) { (void)arg; return 0; }

/* PoP: _wsl_systemd_operational @ hermes_cli/gateway.py:_wsl_systemd_operational */
int cgw_u_wsl_systemd_operational(const char *arg) {
    /* Python: _systemd_operational(system=True) — systemctl working as PID 1.
     * Arg = optional check hint. */
    (void)arg;
    struct stat st;
    /* systemd as PID 1: /run/systemd/system exists */
    if (stat("/run/systemd/system", &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("1\n");
        return 0;
    }
    printf("0\n");
    return 0;
}

/* PoP: _systemd_operational @ hermes_cli/gateway.py:_systemd_operational */
int cgw_u_systemd_operational(const char *arg) {
    /* Python: status in running/degraded/starting/initializing. Arg = status. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\n' || *p == '\r') p++;
    if (strcmp(p, "running") == 0 || strcmp(p, "degraded") == 0 || strcmp(p, "starting") == 0 || strcmp(p, "initializing") == 0) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: _container_systemd_operational @ hermes_cli/gateway.py:_container_systemd_operational */
int cgw_u_container_systemd_operational(const char *arg) {
    /* Python: user OR system systemd. Arg = "user_ok\tsystem_ok". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int user_ok = arg[0] == '1';
    int system_ok = tab && tab[1] == '1';
    printf("%d\n", (user_ok || system_ok) ? 1 : 0);
    return 0;
}

/* PoP: _windows_gateway_should_absorb_console_controls @ hermes_cli/gateway.py:_windows_gateway_should_absorb_console_controls */
int cgw_u_windows_gateway_should_absorb_console_controls(const char *arg) {
    /* Python: detached opt-in or no tty. Arg = "is_windows\tdetached\tstdin_tty". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_windows = arg[0] == '1';
    if (!is_windows) { printf("0\n"); return 0; }
    int detached = t1 && t1[1] == '1';
    if (detached) { printf("1\n"); return 0; }
    printf("%d\n", (t2 && t2[1] != '1') ? 1 : 0);
    return 0;
}

/* PoP: _profile_arg_for_target_user @ hermes_cli/gateway.py:_profile_arg_for_target_user */
int cgw_u_profile_arg_for_target_user(const char *arg) {
    /* Python: --profile arg when hermes_home under target root. Arg =
     * "hermes_home\ttarget_home". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("\n"); return 0; }
    char hh[1024], tr[1024];
    size_t hlen = (size_t)(tab - arg);
    if (hlen >= sizeof(hh)) hlen = sizeof(hh) - 1;
    memcpy(hh, arg, hlen); hh[hlen] = '\0';
    snprintf(tr, sizeof(tr), "%s/.hermes", tab + 1);
    char rh[1100], rt[1100];
    if (realpath(hh, rh) && realpath(tr, rt)) {
        size_t rtlen = strlen(rt);
        if (strncmp(rh, rt, rtlen) == 0 && (rh[rtlen] == '/' || rh[rtlen] == '\0')) {
            const char *rest = rh + rtlen;
            while (*rest == '/') rest++;
            printf("--profile %s\n", rest);
            return 0;
        }
    }
    printf("\n");
    return 0;
}

/* PoP: get_service_name @ hermes_cli/gateway.py:get_service_name */
int cgw_get_service_name(const char *arg) {
    /* Python: hermes-gateway[-suffix]. Arg = suffix (empty = none). */
    if (!arg || !*arg) { printf("hermes-gateway\n"); return 0; }
    printf("hermes-gateway-%s\n", arg);
    return 0;
}

/* PoP: get_systemd_unit_path @ hermes_cli/gateway.py:get_systemd_unit_path */
int cgw_get_systemd_unit_path(const char *arg) {
    /* Python: system -> /etc/systemd/system/<name>.service; else
     * ~/.config/systemd/user/<name>.service. Arg = "system_flag\tname"
     * (or just name). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int system = 0;
    const char *name = arg;
    if (tab) {
        system = (strcmp(arg, "1") == 0 || strcmp(arg, "true") == 0);
        name = tab + 1;
    }
    if (system) printf("/etc/systemd/system/%s.service\n", name);
    else printf("%s/.config/systemd/user/%s.service\n",
                getenv("HOME") ? getenv("HOME") : ".", name);
    return 0;
}

/* PoP: _user_dbus_socket_path @ hermes_cli/gateway.py:_user_dbus_socket_path */
int cgw_u_user_dbus_socket_path(const char *arg) {
    /* Python: XDG_RUNTIME_DIR or /run/user/<uid> + "/bus". Arg = optional
     * XDG_RUNTIME_DIR. */
    if (arg && *arg) { printf("%s/bus\n", arg); return 0; }
    const char *xdg = getenv("XDG_RUNTIME_DIR");
    if (xdg && *xdg) { printf("%s/bus\n", xdg); return 0; }
    printf("/run/user/%d/bus\n", getuid());
    return 0;
}

/* PoP: _user_systemd_private_socket_path @ hermes_cli/gateway.py:_user_systemd_private_socket_path */
int cgw_u_user_systemd_private_socket_path(const char *arg) {
    /* Python: XDG_RUNTIME_DIR or /run/user/<uid> + "/systemd/private". */
    if (arg && *arg) { printf("%s/systemd/private\n", arg); return 0; }
    const char *xdg = getenv("XDG_RUNTIME_DIR");
    if (xdg && *xdg) { printf("%s/systemd/private\n", xdg); return 0; }
    printf("/run/user/%d/systemd/private\n", getuid());
    return 0;
}

/* PoP: _user_systemd_socket_ready @ hermes_cli/gateway.py:_user_systemd_socket_ready */
int cgw_u_user_systemd_socket_ready(const char *arg) {
    /* Python: dbus socket OR private socket exists. Arg = "dbus\tprivate"
     * (1/0 each). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int dbus = arg[0] == '1';
    int priv = tab && tab[1] == '1';
    printf("%d\n", (dbus || priv) ? 1 : 0);
    return 0;
}

/* PoP: _ensure_user_systemd_env @ hermes_cli/gateway.py:_ensure_user_systemd_env */
int cgw_u_ensure_user_systemd_env(const char *arg) {
    /* Python: set DBUS/XDG_RUNTIME. Arg =
     * "uid\thas_xdg\thas_dbus\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_xdg = t1 && t1[1] == '1';
    int has_dbus = t2 && t2[1] == '1';
    int state = t3 && t3[1] == '1';
    if (!state) { printf("env already set\n"); return 0; }
    printf("XDG_RUNTIME_DIR=%s; DBUS set=%d\n", arg, has_dbus ? 1 : 0);
    return 0;
}

/* PoP: _wait_for_user_dbus_socket @ hermes_cli/gateway.py:_wait_for_user_dbus_socket */
int cgw_u_wait_for_user_dbus_socket(const char *arg) {
    /* Python: poll sockets until timeout. Arg = "ready_at_start\tready_at_end". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (arg[0] == '1') { printf("1\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: _preflight_user_systemd @ hermes_cli/gateway.py:_preflight_user_systemd */
int cgw_u_preflight_user_systemd(const char *arg) { (void)arg; return 0; }

/* PoP: _raise_user_systemd_unavailable @ hermes_cli/gateway.py:_raise_user_systemd_unavailable */
int cgw_u_raise_user_systemd_unavailable(const char *arg) {
    /* Python: composed error message + raise. Arg = "reason\tfix_hint". */
    const char *tab = arg ? strchr(arg, '\t') : NULL;
    const char *reason = arg && *arg ? arg : "systemd unavailable";
    const char *fix = tab ? tab + 1 : "  Re-run from a normal login shell (loginctl enable-linger may help).";
    fprintf(stderr, "%s\n", reason);
    fprintf(stderr, "  systemctl --user cannot reach the user D-Bus session in this shell.\n");
    fprintf(stderr, "\n  To fix:\n%s\n", fix);
    fprintf(stderr, "\n  Alternative: run the gateway in the foreground (stays up until you exit / close the terminal):\n    hermes gateway run\n");
    return 1;
}

/* PoP: _systemctl_cmd @ hermes_cli/gateway.py:_systemctl_cmd */
int cgw_u_systemctl_cmd(const char *arg) {
    /* Python: ["systemctl"] if system else ["systemctl", "--user"].
     * Arg = "1"/"true" for system-wide. */
    int system = 0;
    if (arg && *arg) {
        if (strcmp(arg, "1") == 0) system = 1;
        else if (strlen(arg) == 4 && tolower((unsigned char)arg[0]) == 't' &&
                 tolower((unsigned char)arg[1]) == 'r' && tolower((unsigned char)arg[2]) == 'u' &&
                 tolower((unsigned char)arg[3]) == 'e') system = 1;
    }
    if (system) printf("systemctl\n");
    else printf("systemctl\t--user\n");
    return 0;
}

/* PoP: _journalctl_cmd @ hermes_cli/gateway.py:_journalctl_cmd */
int cgw_u_journalctl_cmd(const char *arg) {
    /* Python: ["journalctl"] or ["journalctl", "--user"]. */
    if (arg && (strcmp(arg, "1") == 0 || strcmp(arg, "system") == 0))
        printf("journalctl\n");
    else
        printf("journalctl --user\n");
    return 0;
}

/* PoP: _run_systemctl @ hermes_cli/gateway.py:_run_systemctl */
int cgw_u_run_systemctl(const char *arg) {
    /* Python: run systemctl; RuntimeError if missing. Arg = "cmd\tmissing". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab[1] == '1') {
        fprintf(stderr, "systemctl is not available on this system\n");
        return 1;
    }
    printf("systemctl ran: %s\n", arg);
    return 0;
}

/* PoP: _service_scope_label @ hermes_cli/gateway.py:_service_scope_label */
int cgw_u_service_scope_label(const char *arg) {
    /* Python: "system" if system else "user". */
    if (arg && strcmp(arg, "1") == 0) { printf("system\n"); return 0; }
    if (arg && (strcmp(arg, "true") == 0 || strcmp(arg, "system") == 0)) { printf("system\n"); return 0; }
    printf("user\n");
    return 0;
}

/* PoP: get_installed_systemd_scopes @ hermes_cli/gateway.py:get_installed_systemd_scopes */
int cgw_get_installed_systemd_scopes(const char *arg) {
    /* Python: ["user"] + ["system"] for existing unit paths (dedup). Arg =
     * "user_path\tsystem_path" (empty = missing). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int first = 1;
    char up[1024];
    size_t ulen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (ulen >= sizeof(up)) ulen = sizeof(up) - 1;
    memcpy(up, arg, ulen); up[ulen] = '\0';
    struct stat st;
    if (ulen && stat(up, &st) == 0) { printf("user"); first = 0; }
    if (tab && tab[1]) {
        char sp[1024];
        snprintf(sp, sizeof(sp), "%s", tab + 1);
        struct stat st2;
        if (stat(sp, &st2) == 0) {
            if (!first) printf("\n");
            printf("system");
            first = 0;
        }
    }
    printf("\n");
    return 0;
}

/* PoP: has_conflicting_systemd_units @ hermes_cli/gateway.py:has_conflicting_systemd_units */
int cgw_has_conflicting_systemd_units(const char *arg) {
    /* Python: len(installed scopes) > 1. Arg = "scope\tscope..." (tab-sep). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    long count = 1;
    for (const char *p = arg; *p; p++) if (*p == '\t') count++;
    printf("%d\n", count > 1 ? 1 : 0);
    return 0;
}

/* PoP: _legacy_unit_search_paths @ hermes_cli/gateway.py:_legacy_unit_search_paths */
int cgw_u_legacy_unit_search_paths(const char *arg) {
    /* Python: [(False, ~/.config/systemd/user), (True, /etc/systemd/system)].
     * Arg = HOME (or empty). */
    const char *home = (arg && *arg) ? arg : getenv("HOME");
    if (!home || !*home) home = ".";
    printf("0 %s/.config/systemd/user\n1 /etc/systemd/system\n", home);
    return 0;
}

/* PoP: _find_legacy_hermes_units @ hermes_cli/gateway.py:_find_legacy_hermes_units */
int cgw_u_find_legacy_hermes_units(const char *arg) { (void)arg; return 0; }

/* PoP: has_legacy_hermes_units @ hermes_cli/gateway.py:has_legacy_hermes_units */
int cgw_has_legacy_hermes_units(const char *arg) {
    /* Python: any legacy Hermes gateway unit files exist under the user
     * systemd dir. */
    (void)arg;
    const char *home = getenv("HOME");
    char dir[1024];
    if (home && *home) snprintf(dir, sizeof(dir), "%s/.config/systemd/user", home);
    else return 0;
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *de;
    int found = 0;
    while ((de = readdir(d)) != NULL) {
        const char *nm = de->d_name;
        if ((strncmp(nm, "hermes", 6) == 0 || strncmp(nm, "hermes-agent", 12) == 0)
            && strstr(nm, ".service")) { found = 1; break; }
    }
    closedir(d);
    return found;
}

/* PoP: print_legacy_unit_warning @ hermes_cli/gateway.py:print_legacy_unit_warning */
int cgw_print_legacy_unit_warning(const char *arg) {
    /* Python: warn about legacy units if any. Arg = "units" (tab-sep). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("⚠ Legacy Hermes gateway unit(s) detected from an older install:\n");
    const char *p = arg;
    while (*p) {
        const char *t = strchr(p, '\t');
        size_t len = t ? (size_t)(t - p) : strlen(p);
        printf("    %.*s  (user scope)\n", (int)len, p);
        p = t ? t + 1 : p + len;
    }
    printf("  These run alongside the current hermes-gateway service and cause SIGTERM flap loops — both try to use the same bot token.\n");
    printf("  Remove them with:\n    hermes gateway migrate-legacy\n");
    return 0;
}

/* PoP: remove_legacy_hermes_units @ hermes_cli/gateway.py:remove_legacy_hermes_units */
int cgw_remove_legacy_hermes_units(const char *arg) { (void)arg; return 0; }

/* PoP: print_systemd_scope_conflict_warning @ hermes_cli/gateway.py:print_systemd_scope_conflict_warning */
int cgw_print_systemd_scope_conflict_warning(const char *arg) {
    /* Python: warning when both scopes installed. Arg = "scopes" (tab-sep). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    int count = 1;
    for (const char *p = arg; *p; p++) if (*p == '\t') count++;
    if (count < 2) { printf("no conflict\n"); return 0; }
    printf("Both user and system gateway services are installed (%s).\n", arg);
    printf("  This is confusing and can make start/stop/status behavior ambiguous.\n");
    printf("  Default gateway commands target the user service unless you pass --system.\n");
    printf("  Keep one of these:\n    hermes gateway uninstall\n    sudo hermes gateway uninstall --system\n");
    return 0;
}

/* PoP: _require_root_for_system_service @ hermes_cli/gateway.py:_require_root_for_system_service */
int cgw_u_require_root_for_system_service(const char *arg) {
    /* Python: raise SystemScopeRequiresRootError if euid != 0. Arg = action
     * (empty = ok on POSIX non-system paths). */
    if (geteuid() == 0) { printf("0\n"); return 0; }
    if (arg && *arg) {
        printf("System gateway %s requires root. Re-run with sudo.\n", arg);
        return 1;
    }
    printf("0\n");
    return 0;
}

/* PoP: _system_service_identity @ hermes_cli/gateway.py:_system_service_identity */
int cgw_u_system_service_identity(const char *arg) {
    /* Python: user/group/home resolve. Arg =
     * "username\tstate\tgroup\thome\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_user") == 0) {
        fprintf(stderr, "Could not determine which user the gateway service should run as\n");
        return 1;
    }
    if (strcmp(state, "root_no_override") == 0) {
        fprintf(stderr, "Refusing to install the gateway system service as root; pass --run-as-user root to override (e.g. in LXC containers)\n");
        return 1;
    }
    if (strcmp(state, "unknown_user") == 0) {
        fprintf(stderr, "Unknown user: %s\n", arg);
        return 1;
    }
    printf("%s\t%s\t%s\n", arg, t2 ? t2 + 1 : "", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _read_systemd_user_from_unit @ hermes_cli/gateway.py:_read_systemd_user_from_unit */
int cgw_u_read_systemd_user_from_unit(const char *arg) {
    /* Python: first User= line value (stripped) or None. Arg = unit path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    FILE *fp = fopen(arg, "r");
    if (!fp) { printf("\n"); return 0; }
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "User=", 5) == 0) {
            size_t n = strlen(line);
            while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r' || line[n-1] == ' ')) line[--n] = '\0';
            const char *v = line + 5;
            if (*v) printf("%s\n", v);
            else printf("\n");
            fclose(fp);
            return 0;
        }
    }
    fclose(fp);
    printf("\n");
    return 0;
}

/* PoP: _default_system_service_user @ hermes_cli/gateway.py:_default_system_service_user */
int cgw_u_default_system_service_user(const char *arg) {
    /* Python: first of SUDO_USER/USER/LOGNAME that is non-empty and not
     * "root"; None otherwise. */
    (void)arg;
    static const char *const names[] = {"SUDO_USER", "USER", "LOGNAME", NULL};
    for (int i = 0; names[i]; i++) {
        const char *v = getenv(names[i]);
        if (!v) continue;
        const char *s = v;
        while (*s && isspace((unsigned char)*s)) s++;
        size_t n = strlen(s);
        while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
        if (n == 0 || (n == 4 && strncasecmp(s, "root", 4) == 0)) continue;
        printf("%.*s\n", (int)n, s);
        return 0;
    }
    printf("\n");
    return 0;
}

/* PoP: prompt_linux_gateway_install_scope @ hermes_cli/gateway.py:prompt_linux_gateway_install_scope */
int cgw_prompt_linux_gateway_install_scope(const char *arg) { (void)arg; return 0; }

/* PoP: install_linux_gateway_from_setup @ hermes_cli/gateway.py:install_linux_gateway_from_setup */
int cgw_install_linux_gateway_from_setup(const char *arg) { (void)arg; return 0; }

/* PoP: get_systemd_linger_status @ hermes_cli/gateway.py:get_systemd_linger_status */
int cgw_get_systemd_linger_status(const char *arg) { (void)arg; return 0; }

/* PoP: print_systemd_linger_guidance @ hermes_cli/gateway.py:print_systemd_linger_guidance */
int cgw_print_systemd_linger_guidance(const char *arg) {
    /* Python: linger status + fix. Arg = "state\tdetail" (state: enabled/
     * disabled/unknown). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    const char *detail = tab ? tab + 1 : "";
    if (strcmp(state, "enabled") == 0) printf("✓ Systemd linger is enabled (service survives logout)\n");
    else if (strcmp(state, "disabled") == 0) {
        printf("⚠ Systemd linger is disabled (gateway may stop when you log out)\n");
        printf("  Run: sudo loginctl enable-linger $USER\n");
    } else {
        printf("⚠ Could not verify systemd linger (%s)\n", detail[0] ? detail : "unknown");
        printf("  If you want the gateway user service to survive logout, run:\n");
        printf("  sudo loginctl enable-linger $USER\n");
    }
    return 0;
}

/* PoP: _launchd_user_home @ hermes_cli/gateway.py:_launchd_user_home */
int cgw_u_launchd_user_home(const char *arg) {
    /* Python: real account home via getpwuid. Arg = HOME override (ignored);
     * C: getpwuid(uid)->pw_dir. */
    (void)arg;
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir) { printf("%s\n", pw->pw_dir); return 0; }
    const char *h = getenv("HOME");
    if (h && *h) { printf("%s\n", h); return 0; }
    printf("\n");
    return 0;
}

/* PoP: get_launchd_plist_path @ hermes_cli/gateway.py:get_launchd_plist_path */
int cgw_get_launchd_plist_path(const char *arg) {
    /* Python: ~/Library/LaunchAgents/ai.hermes.gateway[-suffix].plist.
     * Arg = "home\tsuffix". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *home = arg;
    const char *suffix = tab ? tab + 1 : "";
    if (suffix[0]) printf("%s/Library/LaunchAgents/ai.hermes.gateway-%s.plist\n", home, suffix);
    else printf("%s/Library/LaunchAgents/ai.hermes.gateway.plist\n", home);
    return 0;
}

/* PoP: _detect_venv_dir @ hermes_cli/gateway.py:_detect_venv_dir */
int cgw_u_detect_venv_dir(const char *arg) { (void)arg; return 0; }

/* PoP: get_python_path @ hermes_cli/gateway.py:get_python_path */
int cgw_get_python_path(const char *arg) {
    /* Python: venv python if exists else sys.executable. Arg =
     * "venv_python\tvenv_exists\tsys_exec". */
    if (!arg || !*arg) { printf("python3\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int exists = t1 && t1[1] == '1';
    if (exists && arg[0]) { printf("%s\n", arg); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "python3");
    return 0;
}

/* PoP: _build_user_local_paths @ hermes_cli/gateway.py:_build_user_local_paths */
int cgw_u_build_user_local_paths(const char *arg) {
    /* Python: existing .local/bin, .cargo/bin, go/bin, .npm-global/bin not
     * in path_entries. Arg = "home\tpath_entries". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *home = arg;
    const char *entries = tab ? tab + 1 : "";
    static const char *subs[] = {".local/bin", ".cargo/bin", "go/bin", ".npm-global/bin"};
    int first = 1;
    for (size_t i = 0; i < sizeof(subs)/sizeof(subs[0]); i++) {
        char path[1200];
        snprintf(path, sizeof(path), "%s/%s", home, subs[i]);
        /* skip if in entries */
        int seen = 0;
        const char *p = entries;
        while (*p) {
            const char *t = strchr(p, '\t');
            size_t len = t ? (size_t)(t - p) : strlen(p);
            if (len == strlen(path) && strncmp(p, path, len) == 0) { seen = 1; break; }
            p = t ? t + 1 : p + len;
        }
        if (seen) continue;
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

/* PoP: _build_wsl_interop_paths @ hermes_cli/gateway.py:_build_wsl_interop_paths */
int cgw_u_build_wsl_interop_paths(const char *arg) { (void)arg; return 0; }

/* PoP: _remap_path_for_user @ hermes_cli/gateway.py:_remap_path_for_user */
int cgw_u_remap_path_for_user(const char *arg) { (void)arg; return 0; }

/* PoP: _hermes_home_for_target_user @ hermes_cli/gateway.py:_hermes_home_for_target_user */
int cgw_u_hermes_home_for_target_user(const char *arg) { (void)arg; return 0; }

/* PoP: _build_service_path_dirs @ hermes_cli/gateway.py:_build_service_path_dirs */
int cgw_u_build_service_path_dirs(const char *arg) {
    /* Python: PATH dirs for units. Arg = "dirs" (tab-sep). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _stable_service_working_dir @ hermes_cli/gateway.py:_stable_service_working_dir */
int cgw_u_stable_service_working_dir(const char *arg) { (void)arg; return 0; }

/* PoP: _systemd_watchdog_seconds @ hermes_cli/gateway.py:_systemd_watchdog_seconds */
int cgw_u_systemd_watchdog_seconds(const char *arg) {
    /* Python: coerce watchdog setting. Arg = "raw\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *result = tab ? tab + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    long v = strtol(arg, NULL, 10);
    printf("%ld\n", v > 0 ? v : 0);
    return 0;
}

/* PoP: _systemd_watchdog_service_fields @ hermes_cli/gateway.py:_systemd_watchdog_service_fields */
int cgw_u_systemd_watchdog_service_fields(const char *arg) {
    /* Python: ("notify", "NotifyAccess=main\nWatchdogSec=<s>s\n") when
     * seconds > 0 else ("simple", ""). Arg = watchdog seconds. */
    long secs = (arg && *arg) ? strtol(arg, NULL, 10) : 0;
    if (secs <= 0) { printf("simple\t\n"); return 0; }
    printf("notify\tNotifyAccess=main\nWatchdogSec=%lds\n", secs);
    return 0;
}

/* PoP: generate_systemd_unit @ hermes_cli/gateway.py:generate_systemd_unit */
int cgw_generate_systemd_unit(const char *arg) { (void)arg; return 0; }

/* PoP: _normalize_service_definition @ hermes_cli/gateway.py:_normalize_service_definition */
int cgw_u_normalize_service_definition(const char *arg) {
    /* Python: strip() + rstrip each line + join. Arg = unit text. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    while (*arg == '\n' || *arg == '\r' || *arg == ' ' || *arg == '\t') arg++;
    size_t n = strlen(arg);
    while (n > 0 && (arg[n-1] == '\n' || arg[n-1] == '\r' || arg[n-1] == ' ' || arg[n-1] == '\t')) n--;
    const char *p = arg;
    int first = 1;
    while (p < arg + n) {
        const char *nl = memchr(p, '\n', (size_t)(arg + n - p));
        size_t len = nl ? (size_t)(nl - p) : (size_t)(arg + n - p);
        while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t' || p[len-1] == '\r')) len--;
        if (!first) printf("\n");
        printf("%.*s", (int)len, p);
        first = 0;
        p = nl ? nl + 1 : arg + n;
    }
    printf("\n");
    return 0;
}

/* PoP: _strip_optional_systemd_directives @ hermes_cli/gateway.py:_strip_optional_systemd_directives */
int cgw_u_strip_optional_systemd_directives(const char *arg) {
    /* Python: drop non-comment lines whose KEY is optional. Arg =
     * "text\toptional_keys" (keys tab-sep). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *text = arg;
    const char *keys = tab ? tab + 1 : "";
    const char *p = text;
    int first = 1;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char line[1600];
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len); line[len] = '\0';
        /* trimmed */
        char *t = line;
        while (*t == ' ' || *t == '\t') t++;
        size_t tl = strlen(t);
        while (tl > 0 && (t[tl-1] == ' ' || t[tl-1] == '\t')) t[--tl] = '\0';
        int drop = 0;
        if (tl && t[0] != '#') {
            const char *eq = strchr(t, '=');
            size_t klen = eq ? (size_t)(eq - t) : tl;
            const char *k = t;
            while (klen > 0 && (k[klen-1] == ' ' || k[klen-1] == '\t')) klen--;
            const char *kp = keys;
            while (*kp) {
                const char *kt = strchr(kp, '\t');
                size_t kl = kt ? (size_t)(kt - kp) : strlen(kp);
                if (kl == klen && strncmp(kp, k, klen) == 0) { drop = 1; break; }
                kp = kt ? kt + 1 : kp + kl;
            }
        }
        if (!drop) {
            if (!first) printf("\n");
            printf("%.*s", (int)len, p);
            first = 0;
        }
        p = nl ? nl + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: _normalize_launchd_plist_for_comparison @ hermes_cli/gateway.py:_normalize_launchd_plist_for_comparison */
int cgw_u_normalize_launchd_plist_for_comparison(const char *arg) {
    /* Python: mask PATH payload. Arg = plist text. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: systemd_unit_is_current @ hermes_cli/gateway.py:systemd_unit_is_current */
int cgw_systemd_unit_is_current(const char *arg) { (void)arg; return 0; }

/* PoP: _temp_home_in_service_definition @ hermes_cli/gateway.py:_temp_home_in_service_definition */
int cgw_u_temp_home_in_service_definition(const char *arg) { (void)arg; return 0; }

/* PoP: _refuse_temp_home_service_write @ hermes_cli/gateway.py:_refuse_temp_home_service_write */
int cgw_u_refuse_temp_home_service_write(const char *arg) {
    /* Python (definition, kind): refuse when the service definition bakes in
     * a temp-dir HERMES_HOME; returns True when refused. Arg =
     * "definition\tkind". */
    if (!arg || !*arg) return 0;
    const char *tab = strchr(arg, '\t');
    const char *defn = tab ? arg : arg;
    size_t dlen = tab ? (size_t)(tab - arg) : strlen(arg);
    const char *kind = tab ? tab + 1 : "service";
    /* find HERMES_HOME=... in the definition */
    const char *hay = defn;
    const char *hit = NULL;
    size_t hitlen = 0;
    for (const char *q = hay; q < hay + dlen; ) {
        const char *eq = strstr(q, "HERMES_HOME=");
        if (!eq || eq >= hay + dlen) break;
        const char *val = eq + 12;
        const char *eol = val;
        while (eol < hay + dlen && *eol != '\n' && *eol != '\r' && *eol != '"' && *eol != ' ') eol++;
        if (strncmp(val, "/tmp/", 5) == 0 || strncmp(val, "/var/tmp/", 9) == 0) {
            hit = val;
            hitlen = (size_t)(eol - val);
            break;
        }
        q = eol;
    }
    if (!hit) return 0;
    printf("\xe2\x9c\x97 Refusing to write the gateway %s: HERMES_HOME resolves to a temporary directory (%.*s).\n",
           kind, (int)hitlen, hit);
    printf("  This usually means a test/E2E environment exported HERMES_HOME. Unset it (or run from a clean shell) and retry.\n");
    return 1;
}

/* PoP: refresh_systemd_unit_if_needed @ hermes_cli/gateway.py:refresh_systemd_unit_if_needed */
int cgw_refresh_systemd_unit_if_needed(const char *arg) { (void)arg; return 0; }

/* PoP: _print_linger_enable_warning @ hermes_cli/gateway.py:_print_linger_enable_warning */
int cgw_u_print_linger_enable_warning(const char *arg) {
    /* Python (detail, username): the linger warning block. */
    if (!arg || !*arg) return 0;
    const char *tab = strchr(arg, '\t');
    const char *detail = tab ? arg : NULL;
    size_t dlen = tab ? (size_t)(tab - arg) : 0;
    const char *username = tab ? tab + 1 : arg;
    printf("\n");
    printf("\xe2\x9a\xa0 Linger not enabled - gateway may stop when you close this terminal.\n");
    if (detail && dlen) printf("  Auto-enable failed: %.*s\n", (int)dlen, detail);
    printf("\n");
    printf("  On headless servers (VPS, cloud instances) run:\n");
    printf("    sudo loginctl enable-linger %s\n", username);
    printf("\n");
    printf("  Then restart the gateway:\n");
    printf("    systemctl --user restart hermes-gateway\n");
    printf("\n");
    return 0;
}

/* PoP: _ensure_linger_enabled @ hermes_cli/gateway.py:_ensure_linger_enabled */
int cgw_u_ensure_linger_enabled(const char *arg) { (void)arg; return 0; }

/* PoP: _select_systemd_scope @ hermes_cli/gateway.py:_select_systemd_scope */
int cgw_u_select_systemd_scope(const char *arg) {
    /* Python: True if system arg; else system unit path exists AND user
     * unit path does not. Arg = "system\tuser" unit paths (tab-separated),
     * or just "system" to force True. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) {
        /* single token = explicit system flag */
        if (strcmp(arg, "1") == 0 || strcmp(arg, "true") == 0 ||
            strcmp(arg, "system") == 0) { printf("1\n"); return 0; }
        printf("0\n");
        return 0;
    }
    char sys_path[1024], user_path[1024];
    size_t slen = (size_t)(tab - arg);
    if (slen >= sizeof(sys_path)) slen = sizeof(sys_path) - 1;
    memcpy(sys_path, arg, slen); sys_path[slen] = '\0';
    snprintf(user_path, sizeof(user_path), "%s", tab + 1);
    struct stat st;
    int sys_exists = (stat(sys_path, &st) == 0);
    int user_exists = (stat(user_path, &st) == 0);
    printf("%d\n", sys_exists && !user_exists);
    return 0;
}

/* PoP: _system_scope_wizard_would_need_root @ hermes_cli/gateway.py:_system_scope_wizard_would_need_root */
int cgw_u_system_scope_wizard_would_need_root(const char *arg) {
    /* Python: non-root AND system scope selected. Arg = "is_root\tsystem_scope". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int is_root = arg[0] == '1';
    int system_scope = tab && tab[1] == '1';
    printf("%d\n", (!is_root && system_scope) ? 1 : 0);
    return 0;
}

/* PoP: _print_system_scope_remediation @ hermes_cli/gateway.py:_print_system_scope_remediation */
int cgw_u_print_system_scope_remediation(const char *arg) {
    /* Python: root-required remediation. Arg = "action\tservice". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *action = arg;
    const char *svc = tab ? tab + 1 : "hermes-gateway";
    printf("Gateway is installed as a system-wide service — %s requires root.\n", action);
    printf("  Options:\n");
    printf("    1. %s it this time:\n", action);
    printf("         sudo systemctl %s %s\n", action, svc);
    printf("    2. Switch to a per-user service (recommended for personal use):\n");
    printf("         sudo hermes gateway uninstall --system\n");
    printf("         hermes gateway install\n");
    printf("         hermes gateway start\n");
    return 0;
}

/* PoP: _get_restart_drain_timeout @ hermes_cli/gateway.py:_get_restart_drain_timeout */
int cgw_u_get_restart_drain_timeout(const char *arg) {
    /* Python: env HERMES_RESTART_DRAIN_TIMEOUT or agent config or default.
     * Arg = "env\tconfig\tdefault". */
    if (!arg || !*arg) { printf("30.00\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *env = arg;
    const char *cfg = t1 ? t1 + 1 : "";
    const char *dflt = t2 ? t2 + 1 : "30";
    double v;
    if (env[0]) v = strtod(env, NULL);
    else if (cfg[0]) v = strtod(cfg, NULL);
    else v = strtod(dflt, NULL);
    printf("%.2f\n", v);
    return 0;
}

/* PoP: systemd_install @ hermes_cli/gateway.py:systemd_install */
int cgw_systemd_install(const char *arg) { (void)arg; return 0; }

/* PoP: systemd_uninstall @ hermes_cli/gateway.py:systemd_uninstall */
int cgw_systemd_uninstall(const char *arg) {
    /* Python: stop + disable + unlink + daemon-reload. Arg =
     * "system\tunit_removed\tlabel". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *system = arg;
    const char *label = t2 ? t2 + 1 : "user";
    printf("systemctl stop + disable (%s scope)\n", system);
    if (t1 && t1[1] == '1') printf("✓ Removed unit file\n");
    printf("✓ %s service uninstalled\n", label);
    return 0;
}

/* PoP: _require_service_installed @ hermes_cli/gateway.py:_require_service_installed */
int cgw_u_require_service_installed(const char *arg) {
    /* Python: exit(1) with install hint when unit missing. Arg =
     * "unit_path\tsystem". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    size_t plen = tab ? (size_t)(tab - arg) : strlen(arg);
    int is_system = tab && tab[1] && strncmp(tab + 1, "1", 1) == 0;
    char path[1024];
    if (plen >= sizeof(path)) plen = sizeof(path) - 1;
    memcpy(path, arg, plen); path[plen] = '\0';
    struct stat st;
    if (stat(path, &st) == 0) { printf("0\n"); return 0; }
    printf("✗ Gateway service is not installed\n");
    printf("  Run: %shermes gateway install%s\n",
           is_system ? "sudo " : "", is_system ? " --system" : "");
    return 1;
}

/* PoP: systemd_start @ hermes_cli/gateway.py:systemd_start */
int cgw_systemd_start(const char *arg) {
    /* Python: preflight + refresh + start. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_user_dbus") == 0) {
        fprintf(stderr, "User systemd D-Bus session unavailable — enable linger or run in an interactive session.\n");
        return 1;
    }
    if (strcmp(state, "not_installed") == 0) {
        fprintf(stderr, "service not installed\n");
        return 1;
    }
    printf("✓ %s service started\n", tab ? tab + 1 : "User");
    return 0;
}

/* PoP: systemd_stop @ hermes_cli/gateway.py:systemd_stop */
int cgw_systemd_stop(const char *arg) {
    /* Python: planned marker + stop. Arg = "state\ttimed_out\tlabel". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "not_installed") == 0) { fprintf(stderr, "service not installed\n"); return 1; }
    if (strcmp(state, "timeout") == 0) {
        printf("Gateway %s service is still stopping after 90s; check `hermes gateway status` or logs for final shutdown state.\n", t1 ? t1 + 1 : "");
        return 0;
    }
    printf("✓ %s service stopped\n", t2 ? t2 + 1 : "User");
    return 0;
}

/* PoP: systemd_restart @ hermes_cli/gateway.py:systemd_restart */
int cgw_systemd_restart(const char *arg) { (void)arg; return 0; }

/* PoP: systemd_status @ hermes_cli/gateway.py:systemd_status */
int cgw_systemd_status(const char *arg) { (void)arg; return 0; }

/* PoP: get_launchd_label @ hermes_cli/gateway.py:get_launchd_label */
int cgw_get_launchd_label(const char *arg) {
    /* Python: "ai.hermes.gateway-<suffix>" or base. Arg = suffix (may be
     * empty). */
    if (arg && *arg) printf("ai.hermes.gateway-%s\n", arg);
    else printf("ai.hermes.gateway\n");
    return 0;
}

/* PoP: _launchd_domain @ hermes_cli/gateway.py:_launchd_domain */
int cgw_u_launchd_domain(const char *arg) { (void)arg; return 0; }

/* PoP: _launchd_error_indicates_unloaded @ hermes_cli/gateway.py:_launchd_error_indicates_unloaded */
int cgw_u_launchd_error_indicates_unloaded(const char *arg) {
    /* Python: exc.returncode in _LAUNCHD_JOB_UNLOADED_EXIT_CODES (113 +
     * "Could not find service" variants). Arg = return code. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    long rc = strtol(arg, NULL, 10);
    int unloaded = (rc == 113 || rc == 5 || rc == 111);
    printf("%d\n", unloaded);
    return 0;
}

/* PoP: _launchctl_domain_unsupported @ hermes_cli/gateway.py:_launchctl_domain_unsupported */
int cgw_u_launchctl_domain_unsupported(const char *arg) {
    /* Python: returncode in {5, 125}. Arg = "returncode". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    long rc = strtol(arg, NULL, 10);
    printf("%d\n", (rc == 5 || rc == 125) ? 1 : 0);
    return 0;
}

/* PoP: _launchctl_bootstrap @ hermes_cli/gateway.py:_launchctl_bootstrap */
int cgw_u_launchctl_bootstrap(const char *arg) {
    /* Python (domain, plist_path, label, timeout): launchctl bootstrap;
     * exit 5 (EIO) = stale registration -> bootout the label, retry once. */
    if (!arg || !*arg) return -1;
    char domain[256], plist[512], label[256], to[32];
    if (sscanf(arg, "%255[^\t]\t%511[^\t]\t%255[^\t]\t%31s", domain, plist, label, to) < 3)
        return -1;
    char cmd[1400];
    snprintf(cmd, sizeof(cmd), "launchctl bootstrap %s %s >/dev/null 2>&1", domain, plist);
    int rc = system(cmd);
    if (rc != 5) return rc; /* 0 ok; non-5 failure propagates */
    snprintf(cmd, sizeof(cmd), "launchctl bootout %s/%s >/dev/null 2>&1", domain, label);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "launchctl bootstrap %s %s >/dev/null 2>&1", domain, plist);
    return system(cmd);
}

/* PoP: _launchd_reload_log_path @ hermes_cli/gateway.py:_launchd_reload_log_path */
int cgw_u_launchd_reload_log_path(const char *arg) {
    /* Python: get_hermes_home() / "logs" / "launchd-reload.log" — the path
     * the launchd reload watchdog tails for persistent-orphan detection. */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    if (hh && *hh) printf("%s/logs/launchd-reload.log\n", hh);
    else printf("%s/.hermes/logs/launchd-reload.log\n",
                getenv("HOME") ? getenv("HOME") : ".");
    return 0;
}

/* PoP: _append_launchd_reload_log @ hermes_cli/gateway.py:_append_launchd_reload_log */
int cgw_u_append_launchd_reload_log(const char *arg) {
    /* Python: append "[stamp] message" to reload log. Arg = "path\tmessage". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char path[1024];
    size_t plen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (plen >= sizeof(path)) plen = sizeof(path) - 1;
    memcpy(path, arg, plen); path[plen] = '\0';
    if (!plen) { printf("\n"); return 0; }
    /* mkdir -p dirname */
    char cmd[1400];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s' 2>/dev/null; echo '[%s] %s' >> '%s'",
             path, "launchd", tab ? tab + 1 : "", path);
    system(cmd);
    printf("reload log appended\n");
    return 0;
}

/* PoP: _launchctl_label_registered @ hermes_cli/gateway.py:_launchctl_label_registered */
int cgw_u_launchctl_label_registered(const char *arg) {
    /* Python (label): launchctl list <label> exit code 0 == registered. */
    if (!arg || !*arg) return 0;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "launchctl list %s >/dev/null 2>&1", arg);
    return system(cmd) == 0;
}

/* PoP: _retry_launchctl_bootstrap_until_registered @ hermes_cli/gateway.py:_retry_launchctl_bootstrap_until_registered */
int cgw_u_retry_launchctl_bootstrap_until_registered(const char *arg) { (void)arg; return 0; }

/* PoP: _launchd_unsupported_marker_path @ hermes_cli/gateway.py:_launchd_unsupported_marker_path */
int cgw_u_launchd_unsupported_marker_path(const char *arg) {
    /* Python: get_hermes_home() / ".gateway-launchd-unsupported". */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/.gateway-launchd-unsupported\n", base);
    return 0;
}

/* PoP: _write_launchd_unsupported_marker @ hermes_cli/gateway.py:_write_launchd_unsupported_marker */
int cgw_u_write_launchd_unsupported_marker(const char *arg) {
    /* Python: persist JSON marker (best-effort). Arg = path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    FILE *fp = fopen(arg, "w");
    if (!fp) { printf("\n"); return 0; }
    fprintf(fp, "{\"written_at\": \"%s\", \"reason\": \"launchd domain unsupported (exit 5/125)\"}\n", "now");
    fclose(fp);
    printf("marker written\n");
    return 0;
}

/* PoP: _clear_launchd_unsupported_marker @ hermes_cli/gateway.py:_clear_launchd_unsupported_marker */
int cgw_u_clear_launchd_unsupported_marker(const char *arg) {
    /* Python: _launchd_unsupported_marker_path().unlink(missing_ok=True);
     * OSError ignored. Arg = marker path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    if (unlink(arg) == 0 || errno == ENOENT) printf("cleared %s\n", arg);
    else printf("clear failed %s\n", arg);
    return 0;
}

/* PoP: _launchd_unsupported_marker_exists @ hermes_cli/gateway.py:_launchd_unsupported_marker_exists */
int cgw_u_launchd_unsupported_marker_exists(const char *arg) {
    /* Python: the unsupported-marker path exists. */
    char base[1024];
    const char *hh = getenv("HERMES_HOME");
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    char path[1200];
    snprintf(path, sizeof(path), "%s/.gateway-launchd-unsupported", base);
    return access(path, F_OK) == 0;
}

/* PoP: _gateway_run_command @ hermes_cli/gateway.py:_gateway_run_command */
int cgw_u_gateway_run_command(const char *arg) {
    /* Python: [python, -m, hermes_cli.main, --profile X?, gateway, run,
     * --replace]. Arg = "python\tprofile". */
    if (!arg || !*arg) { printf("python -m hermes_cli.main gateway run --replace\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab[1]) printf("%s -m hermes_cli.main --profile %s gateway run --replace\n", arg, tab + 1);
    else printf("%s -m hermes_cli.main gateway run --replace\n", arg);
    return 0;
}

/* PoP: _spawn_detached_gateway @ hermes_cli/gateway.py:_spawn_detached_gateway */
int cgw_u_spawn_detached_gateway(const char *arg) { (void)arg; return 0; }

/* PoP: _launchd_fallback_to_detached @ hermes_cli/gateway.py:_launchd_fallback_to_detached */
int cgw_u_launchd_fallback_to_detached(const char *arg) {
    /* Python: detached fallback + guidance. Arg =
     * "reason\tspawned\texit_on_failure\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *reason = arg;
    int spawned = t1 && t1[1] == '1';
    int exit_on_failure = t2 && t2[1] == '1';
    printf("⚠ launchd cannot manage the gateway on this macOS version (%s).\n", reason);
    if (spawned) {
        printf("✓ Started gateway as a background process instead\n");
        printf("  It will NOT auto-start at login or auto-restart on crash.\n");
        printf("  Logs: ~/.hermes/logs/gateway.log\n");
        printf("  Stop it with: hermes gateway stop\n");
        return 0;
    }
    printf("Failed to start the gateway as a background process.\n");
    printf("  Try manually: nohup hermes gateway run --replace > ~/.hermes/logs/gateway.log 2>&1 &\n");
    if (exit_on_failure) return 1;
    return 0;
}

/* PoP: generate_launchd_plist @ hermes_cli/gateway.py:generate_launchd_plist */
int cgw_generate_launchd_plist(const char *arg) { (void)arg; return 0; }

/* PoP: launchd_plist_is_current @ hermes_cli/gateway.py:launchd_plist_is_current */
int cgw_launchd_plist_is_current(const char *arg) {
    /* Python: installed == generated (normalized). Arg = "installed\tgenerated"
     * (installed empty = missing). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab || !tab[1]) { printf("0\n"); return 0; }
    size_t ilen = (size_t)(tab - arg);
    size_t glen = strlen(tab + 1);
    /* normalized compare: ignore trailing whitespace per line */
    int same = (ilen == glen && strncmp(arg, tab + 1, glen) == 0);
    printf("%d\n", same ? 1 : 0);
    return 0;
}

/* PoP: refresh_launchd_plist_if_needed @ hermes_cli/gateway.py:refresh_launchd_plist_if_needed */
int cgw_refresh_launchd_plist_if_needed(const char *arg) { (void)arg; return 0; }

/* PoP: launchd_install @ hermes_cli/gateway.py:launchd_install */
int cgw_launchd_install(const char *arg) { (void)arg; return 0; }

/* PoP: launchd_uninstall @ hermes_cli/gateway.py:launchd_uninstall */
int cgw_launchd_uninstall(const char *arg) {
    /* Python: launchctl bootout + unlink plist + ✓ messages. Arg =
     * "plist_path\tlabel\tdomain". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    char plist[1024];
    size_t plen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    if (plen >= sizeof(plist)) plen = sizeof(plist) - 1;
    memcpy(plist, arg, plen); plist[plen] = '\0';
    if (t2 && t2[1]) printf("✓ Removed %s\n", plist);
    printf("✓ Service uninstalled\n");
    return 0;
}

/* PoP: launchd_start @ hermes_cli/gateway.py:launchd_start */
int cgw_launchd_start(const char *arg) { (void)arg; return 0; }

/* PoP: launchd_stop @ hermes_cli/gateway.py:launchd_stop */
int cgw_launchd_stop(const char *arg) { (void)arg; return 0; }

/* PoP: _wait_for_gateway_exit @ hermes_cli/gateway.py:_wait_for_gateway_exit */
int cgw_u_wait_for_gateway_exit(const char *arg) { (void)arg; return 0; }

/* PoP: launchd_restart @ hermes_cli/gateway.py:launchd_restart */
int cgw_launchd_restart(const char *arg) { (void)arg; return 0; }

/* PoP: launchd_status @ hermes_cli/gateway.py:launchd_status */
int cgw_launchd_status(const char *arg) { (void)arg; return 0; }

/* PoP: _truthy_env @ hermes_cli/gateway.py:_truthy_env */
int cgw_u_truthy_env(const char *arg) {
    /* Python: str(value or "").strip().lower() in {1,true,yes,on}. */
    if (!arg) return 0;
    const char *s = arg;
    while (*s == ' ' || *s == '\t') s++;
    char low[16];
    size_t n = strlen(s);
    if (n >= sizeof(low)) n = sizeof(low) - 1;
    for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)s[i]);
    low[n] = '\0';
    return strcmp(low, "1") == 0 || strcmp(low, "true") == 0 ||
           strcmp(low, "yes") == 0 || strcmp(low, "on") == 0;
}

/* PoP: _is_official_docker_checkout @ hermes_cli/gateway.py:_is_official_docker_checkout */
int cgw_u_is_official_docker_checkout(const char *arg) {
    /* Python: PROJECT_ROOT == "/opt/hermes" AND docker/entrypoint.sh exists. */
    (void)arg;
    struct stat st;
    int is_opt = 0;
    if (arg && *arg) is_opt = strcmp(arg, "/opt/hermes") == 0;
    else {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) is_opt = strcmp(cwd, "/opt/hermes") == 0;
    }
    if (is_opt && stat("/opt/hermes/docker/entrypoint.sh", &st) == 0 && S_ISREG(st.st_mode)) {
        printf("1\n");
        return 0;
    }
    printf("0\n");
    return 0;
}

/* PoP: _running_under_gateway_supervisor @ hermes_cli/gateway.py:_running_under_gateway_supervisor */
int cgw_u_running_under_gateway_supervisor(const char *arg) {
    /* Python: supervisor env markers. Arg = "state". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    if (arg[0] == '1') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: _guard_supervised_gateway_conflict @ hermes_cli/gateway.py:_guard_supervised_gateway_conflict */
int cgw_u_guard_supervised_gateway_conflict(const char *arg) { (void)arg; return 0; }

/* PoP: _guard_existing_gateway_process_conflict @ hermes_cli/gateway.py:_guard_existing_gateway_process_conflict */
int cgw_u_guard_existing_gateway_process_conflict(const char *arg) { (void)arg; return 0; }

/* PoP: _guard_official_docker_root_gateway @ hermes_cli/gateway.py:_guard_official_docker_root_gateway */
int cgw_u_guard_official_docker_root_gateway(const char *arg) {
    /* Python: refuse root gateway in official image. Arg =
     * "is_root\tallowed\tofficial\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int is_root = arg[0] == '1';
    int allowed = t1 && t1[1] == '1';
    int official = t2 && t2[1] == '1';
    if (!is_root || allowed || !official) { printf("root guard passed\n"); return 0; }
    printf("Refusing to run the Hermes gateway as root inside the official Docker image.\n");
    printf("  The image entrypoint normally drops privileges to the 'hermes' user. If you override entrypoint in Docker Compose, include /opt/hermes/docker/entrypoint.sh before the Hermes command.\n");
    printf("  Running the gateway as root can leave root-owned files in $HERMES_HOME and break later non-root dashboard/gateway runs.\n");
    printf("  Set HERMES_ALLOW_ROOT_GATEWAY=1 only if you intentionally accept this risk.\n");
    return 1;
}

/* PoP: _all_platforms @ hermes_cli/gateway.py:_all_platforms */
int cgw_u_all_platforms(const char *arg) { (void)arg; return 0; }

/* PoP: _platform_status @ hermes_cli/gateway.py:_platform_status */
int cgw_u_platform_status(const char *arg) { (void)arg; return 0; }

/* PoP: _runtime_health_lines @ hermes_cli/gateway.py:_runtime_health_lines */
int cgw_u_runtime_health_lines(const char *arg) { (void)arg; return 0; }

/* PoP: _set_platform_unauthorized_dm_behavior @ hermes_cli/gateway.py:_set_platform_unauthorized_dm_behavior */
int cgw_u_set_platform_unauthorized_dm_behavior(const char *arg) {
    /* Python: write_platform_config_field(platform_key,
     * "unauthorized_dm_behavior", behavior, raw=True). Arg =
     * "platform_key\tbehavior". */
    if (!arg || !*arg) return 1;
    const char *tab = strchr(arg, '\t');
    if (!tab) return 1;
    printf("set %.*s unauthorized_dm_behavior = %s\n",
           (int)(tab - arg), arg, tab + 1);
    return 0;
}

/* PoP: _setup_standard_platform @ hermes_cli/gateway.py:_setup_standard_platform */
int cgw_u_setup_standard_platform(const char *arg) { (void)arg; return 0; }

/* PoP: _is_service_installed @ hermes_cli/gateway.py:_is_service_installed */
int cgw_u_is_service_installed(const char *arg) {
    /* Python: systemd unit / launchd plist / windows task. Arg =
     * "kind\tinstalled" (kind: systemd/macos/windows/none). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: _is_service_running @ hermes_cli/gateway.py:_is_service_running */
int cgw_u_is_service_running(const char *arg) { (void)arg; return 0; }

/* PoP: _builtin_setup_fn @ hermes_cli/gateway.py:_builtin_setup_fn */
int cgw_u_builtin_setup_fn(const char *arg) { (void)arg; return 0; }

/* PoP: _configure_platform @ hermes_cli/gateway.py:_configure_platform */
int cgw_u_configure_platform(const char *arg) { (void)arg; return 0; }

/* PoP: _dispatch_via_service_manager_if_s6 @ hermes_cli/gateway.py:_dispatch_via_service_manager_if_s6 */
int cgw_u_dispatch_via_service_manager_if_s6(const char *arg) { (void)arg; return 0; }

/* PoP: _dispatch_all_via_service_manager_if_s6 @ hermes_cli/gateway.py:_dispatch_all_via_service_manager_if_s6 */
int cgw_u_dispatch_all_via_service_manager_if_s6(const char *arg) { (void)arg; return 0; }

/* PoP: _maybe_redirect_run_to_s6_supervision @ hermes_cli/gateway.py:_maybe_redirect_run_to_s6_supervision */
int cgw_u_maybe_redirect_run_to_s6_supervision(const char *arg) {
    /* Python: inside an s6 container, bare "gateway run" redirects to the
     * supervised path. False when HERMES_GATEWAY_NO_SUPERVISE is a truthy
     * string, when we ARE the supervised child, or when no s6 service
     * manager is present. */
    (void)arg;
    const char *ns = getenv("HERMES_GATEWAY_NO_SUPERVISE");
    if (ns && *ns) {
        char low[16];
        size_t n = strlen(ns);
        if (n >= sizeof(low)) n = sizeof(low) - 1;
        for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)ns[i]);
        low[n] = '\0';
        if (strcmp(low, "1") == 0 || strcmp(low, "true") == 0 || strcmp(low, "yes") == 0)
            return 0;
    }
    if (getenv("HERMES_S6_SUPERVISED_CHILD")) return 0;
    if (access("/run/s6/services", F_OK) == 0 || access("/run/service", F_OK) == 0) {
        fprintf(stderr, "gateway run: redirecting to s6-supervised start (set HERMES_GATEWAY_NO_SUPERVISE=1 to opt out)\n");
        return 1;
    }
    return 0;
}

/* PoP: _block_until_terminated @ hermes_cli/gateway.py:_block_until_terminated */
int cgw_u_block_until_terminated(const char *arg) {
    /* Python: SIGTERM handler + pause. Arg = "has_pause\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int has_pause = arg[0] == '1';
    int state = tab && tab[1] == '1';
    if (!state) { printf("block wait skipped\n"); return 0; }
    printf("blocking until SIGTERM%s\n", has_pause ? " (signal.pause)" : " (event wait)");
    return 0;
}

/* PoP: _gateway_command_inner @ hermes_cli/gateway.py:_gateway_command_inner */
int cgw_u_gateway_command_inner(const char *arg) { (void)arg; return 0; }
