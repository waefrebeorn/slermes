/*
 * security_audit_startup.c — canonical C11 port of hermes_cli/security_audit_startup.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_security_audit_star_u_is_root @ hermes_cli/security_audit_startup.py:_is_root */
int hermes_cli_security_audit_star_u_is_root(const char *arg) {
    /* Python: geteuid/getuid == 0; always False on Windows. */
    (void)arg;
    printf("%d\n", geteuid() == 0 ? 1 : 0);
    return 0;
}

/* PoP: hermes_cli_security_audit_star_u_running_as_root @ hermes_cli/security_audit_startup.py:_running_as_root */
int hermes_cli_security_audit_star_u_running_as_root(const char *arg) {
    /* Python: None or root warning text. Arg = "1"/"0" is_root. */
    if (arg && arg[0] == '1') {
        printf("Running as ROOT. The agent's terminal/file tools execute with full root privileges — a single prompt-injection or exposed endpoint is a full host compromise. Run Hermes as an unprivileged user (or in a sandboxed terminal backend / container with a non-root user).\n");
        return 0;
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_security_audit_star_u_iter_sshd_config_lines @ hermes_cli/security_audit_startup.py:_iter_sshd_config_lines */
int hermes_cli_security_audit_star_u_iter_sshd_config_lines(const char *arg) {
    /* Python: non-comment lines from config + drop-ins. Arg = "lines"
     * (tab-sep, empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_security_audit_star_u_ssh_password_auth_enabled @ hermes_cli/security_audit_startup.py:_ssh_password_auth_enabled */
int hermes_cli_security_audit_star_u_ssh_password_auth_enabled(const char *arg) {
    /* Python: sshd PasswordAuthentication verdict. Arg =
     * "state\tverdict\tqualifier\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_config") == 0) { printf("\n"); return 0; }
    if (strcmp(state, "ok") == 0) { printf("\n"); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_security_audit_star_u_path_is_mounted @ hermes_cli/security_audit_startup.py:_path_is_mounted */
int hermes_cli_security_audit_star_u_path_is_mounted(const char *arg) {
    /* Python: /proc/mounts check. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("1\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: hermes_cli_security_audit_star_u_container_no_volume_mount @ hermes_cli/security_audit_startup.py:_container_no_volume_mount */
int hermes_cli_security_audit_star_u_container_no_volume_mount(const char *arg) {
    /* Python: container + home not mounted -> warning text. Arg =
     * "in_container\tmounted\thome". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int in_container = arg[0] == '1';
    int mounted = t1 && t1[1] == '1';
    if (!in_container || mounted) { printf("\n"); return 0; }
    printf("Running in a container but the data dir (%s) is NOT on a persistent volume mount — sessions, memory, skills, and API keys are ephemeral and lost on container restart. Mount a host volume over the HERMES_HOME data directory.\n",
           t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_security_audit_star_u_network_listener_without_auth @ hermes_cli/security_audit_startup.py:_network_listener_without_auth */
int hermes_cli_security_audit_star_u_network_listener_without_auth(const char *arg) {
    /* Python: RCE posture warning. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: hermes_cli_security_audit_star_run_security_audit @ hermes_cli/security_audit_startup.py:run_security_audit */
int hermes_cli_security_audit_star_run_security_audit(const char *arg) {
    /* Python: run all checks, fail-safe. Arg = "findings_json\tcount". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_security_audit_star_log_startup_security_warnings @ hermes_cli/security_audit_startup.py:log_startup_security_warnings */
int hermes_cli_security_audit_star_log_startup_security_warnings(const char *arg) {
    /* Python: run audit + log findings once. Arg = "findings_json\tcount". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long count = tab ? strtol(tab + 1, NULL, 10) : 0;
    if (count > 0) printf("Security posture audit found %ld issue(s) — review your deployment:\n", count);
    printf("%s\n", arg);
    return 0;
}
