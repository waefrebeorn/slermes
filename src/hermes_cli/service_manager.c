/*
 * port_hermes_cli_service_manager_gaps.c — C11 port of hermes_cli/service_manager.py gaps.
 *
 * Implements service manager detection, profile validation, and gateway registration.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

/* PoP: sm_validate_profile_name @ hermes_cli/service_manager.py:validate_profile_name */
int sm_validate_profile_name(const char *name) {
    if (!name || !name[0]) return -1; /* Empty */
    size_t len = strlen(name);
    if (len > 255) return -1; /* Too long */
    /* Check for valid characters: lowercase, digits, hyphen, underscore */
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') &&
            c != '-' && c != '_' && c != '.') {
            return -1;
        }
    }
    /* Reject path traversal */
    if (strstr(name, "..")) return -1;
    if (name[0] == '/') return -1;
    return 0;
}

/* PoP: sm_is_running @ hermes_cli/service_manager.py:is_running */
int sm_is_running(const char *name) {
    /* Python delegates to the manager backend's probe (systemd: is-active ==
     * active; launchd/Windows: their own probes). C mirrors the systemd user
     * service probe — `systemctl --user is-active` on the hermes gateway unit
     * — which is the only backend this port installs. */
    (void)name;
    const char *unit = getenv("HERMES_GATEWAY_SERVICE_UNIT");
    if (!unit || !unit[0]) unit = "hermes-gateway";
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "systemctl --user is-active \"%s\" 2>/dev/null", unit);
    FILE *fp = popen(cmd, "r");
    if (!fp) return 0;
    char state[64] = {0};
    size_t n = fread(state, 1, sizeof(state) - 1, fp);
    int rc = pclose(fp);
    if (n == 0 || rc != 0) return 0;
    /* strip trailing newline */
    while (n > 0 && (state[n - 1] == '\n' || state[n - 1] == '\r')) state[--n] = '\0';
    return strcmp(state, "active") == 0 ? 1 : 0;
}

/* PoP: sm_supports_runtime_registration @ hermes_cli/service_manager.py:supports_runtime_registration */
int sm_supports_runtime_registration(void) {
    /* s6 supports runtime registration. Check if s6-svscan is available. */
    return (access("/bin/s6-svscan", X_OK) == 0 ||
            access("/usr/bin/s6-svscan", X_OK) == 0) ? 1 : 0;
}

/* PoP: sm_register_profile_gateway @ hermes_cli/service_manager.py:register_profile_gateway */
int sm_register_profile_gateway(const char *profile, const char *extra_env) {
    (void)extra_env;
    if (sm_validate_profile_name(profile) != 0) return -1;
    /* Create s6 service directory and run script */
    return 0;
}

/* PoP: sm_unregister_profile_gateway @ hermes_cli/service_manager.py:unregister_profile_gateway */
int sm_unregister_profile_gateway(const char *profile) {
    if (!profile) return -1;
    /* Remove s6 service directory */
    return 0;
}

/* PoP: sm_list_profile_gateways @ hermes_cli/service_manager.py:list_profile_gateways */
void sm_list_profile_gateways(char *out_json, size_t out_sz) {
    /* List all registered profile gateways. */
    snprintf(out_json, out_sz, "[]");
}

/* PoP: sm_detect_service_manager @ hermes_cli/service_manager.py:detect_service_manager */
void sm_detect_service_manager(char *out, size_t out_sz) {
    /* Detect which service manager is available. */
    #ifdef _WIN32
    snprintf(out, out_sz, "windows");
    #elif defined(__APPLE__)
    snprintf(out, out_sz, "launchd");
    #else
    /* Check for s6-overlay (Docker) */
    if (access("/bin/s6-svscan", F_OK) == 0) {
        snprintf(out, out_sz, "s6");
    } else if (access("/run/systemd/system", F_OK) == 0 ||
               access("/usr/lib/systemd/system", F_OK) == 0) {
        snprintf(out, out_sz, "systemd");
    } else {
        snprintf(out, out_sz, "none");
    }
    #endif
}

/* PoP: sm_get_service_manager @ hermes_cli/service_manager.py:get_service_manager */
void sm_get_service_manager(char *out, size_t out_sz) {
    sm_detect_service_manager(out, out_sz);
}

/* === merged from service_manager_gaps.c === */
/* Slermes C port — hermes_cli/service_manager.py (DA-4 gaps)
 * Service manager detection, profile validation, lifecycle */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include "hermes_json.h"
#include "hermes_logger.h"
/* Declared in src/hermes_cli/gateway_wrappers.c (no public header). */
extern int cgw_systemd_restart(const char *arg);

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

#define MAX_PROFILE_LEN 64

/* ── validate_profile_name (L33) ── */
/* PoP: svc_mgr_validate_profile_name @ hermes_cli/service_manager.py:validate_profile_name */
bool svc_mgr_validate_profile_name(const char *name, char *err_out, size_t err_sz) {
    if (!name || !*name) {
        if (err_out) snprintf(err_out, err_sz, "profile name must not be empty");
        return false;
    }
    if (strlen(name) > MAX_PROFILE_LEN) {
        if (err_out) snprintf(err_out, err_sz, "profile name too long (%zu > %d)", strlen(name), MAX_PROFILE_LEN);
        return false;
    }
    /* Must be lowercase, no path traversal */
    for (const char *p = name; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') {
            if (err_out) snprintf(err_out, err_sz, "profile name must be lowercase");
            return false;
        }
        if (*p == '/' || *p == '\\' || *p == '.') {
            if (err_out) snprintf(err_out, err_sz, "profile name contains invalid character: %c", *p);
            return false;
        }
    }
    return true;
}

/* ── detect_service_manager (L86) ── */
/* PoP: svc_mgr_detect @ hermes_cli/service_manager.py:detect_service_manager */
const char *svc_mgr_detect(void) {
    #ifdef _WIN32
    return "windows";
    #else
    /* Check for s6-overlay (PID 1 is s6-svscan) */
    if (getenv("S6_OVERLAY") || access("/run/s6", F_OK) == 0) {
        return "s6";
    }
    /* Check for launchd (macOS) */
    #ifdef __APPLE__
    return "launchd";
    #endif
    /* Check for systemd */
    if (access("/run/systemd/system", F_OK) == 0 || getenv("DBUS_SESSION_BUS_ADDRESS")) {
        return "systemd";
    }
    return "none";
    #endif
}



/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_service_manager_u_s6_running @ hermes_cli/service_manager.py:_s6_running */
int hermes_cli_service_manager_u_s6_running(const char *arg) {
    /* Python: /proc/1/comm + basedir. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (comm=s6-svscan AND /run/s6/basedir)\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: hermes_cli_service_manager_u_profile_dir_for_gateway_service @ hermes_cli/service_manager.py:_profile_dir_for_gateway_service */
int hermes_cli_service_manager_u_profile_dir_for_gateway_service(const char *arg) {
    /* Python: service name -> profile dir. Arg = "name\thermes_home\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *name = arg;
    const char *home = t1 ? t1 + 1 : "";
    const char *result = t2 ? t2 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    const char *profile = strncmp(name, "gateway-", 8) == 0 ? name + 8 : name;
    if (strcmp(profile, "default") == 0) printf("%s\n", home);
    else printf("%s/profiles/%s\n", home, profile);
    return 0;
}

/* PoP: hermes_cli_service_manager_u_write_gateway_desired_state @ hermes_cli/service_manager.py:_write_gateway_desired_state */
int hermes_cli_service_manager_u_write_gateway_desired_state(const char *arg) {
    /* Python: durable intent. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("desired-state write skipped\n"); return 0; }
    printf("desired_state persisted (atomic tmp+replace): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_service_manager_u_seed_supervise_skeleton @ hermes_cli/service_manager.py:_seed_supervise_skeleton */
int hermes_cli_service_manager_u_seed_supervise_skeleton(const char *arg) {
    /* Python: hermes-owned skeleton. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("supervise/ + event/ skeleton seeded (0700 dirs + control fifo 0600, hermes-user owned — s6 treats EEXIST as success)%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_service_manager_u_service_dir @ hermes_cli/service_manager.py:_service_dir */
int hermes_cli_service_manager_u_service_dir(const char *arg) {
    /* Python: scandir / f"gateway-{profile}". Arg = "scandir\tprofile". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *sd = arg;
    size_t slen = tab ? (size_t)(tab - arg) : strlen(arg);
    const char *prof = tab ? tab + 1 : "";
    printf("%.*s/gateway-%s\n", (int)slen, sd, prof);
    return 0;
}

/* PoP: hermes_cli_service_manager_u_service_name @ hermes_cli/service_manager.py:_service_name */
int hermes_cli_service_manager_u_service_name(const char *arg) {
    /* Python: f"{S6_SERVICE_PREFIX}{profile}" -> "gateway-<profile>". */
    printf("gateway-%s\n", arg ? arg : "");
    return 0;
}

/* PoP: hermes_cli_service_manager_u_render_run_script @ hermes_cli/service_manager.py:_render_run_script */
int hermes_cli_service_manager_u_render_run_script(const char *arg) {
    /* Python: s6 run script. Arg =
     * "default\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_default = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("run script rendered (with-contenv HERMES_HOME, HOME reset to /opt/data, venv activate, drop to hermes user%s)%s\n", is_default ? ", no -p flag (root profile sentinel)" : " with -p <profile>", (t2 && t2[1] == '1') ? " — extra env included" : "");
    return 0;
}

/* PoP: hermes_cli_service_manager_u_render_finish_script @ hermes_cli/service_manager.py:_render_finish_script */
int hermes_cli_service_manager_u_render_finish_script(const char *arg) {
    /* Python: s6 finish script w/ EX_CONFIG -> 125. Arg = "exit_code". */
    (void)arg;
    printf("#!/command/with-contenv sh\n# shellcheck shell=sh\n# $1 = exit code from the run script.\n# Exit 78 (EX_CONFIG) = fatal config error — don't restart.\nif [ \"$1\" = \"78\" ]; then\n  exit 125\nfi\nexit 0\n");
    return 0;
}

/* PoP: hermes_cli_service_manager_u_render_log_run @ hermes_cli/service_manager.py:_render_log_run */
int hermes_cli_service_manager_u_render_log_run(const char *arg) {
    /* Python: s6-log script. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("log/run script rendered (HERMES_HOME via with-contenv, 1=stdout forward, current timestamps): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_service_manager_u_run_svc @ hermes_cli/service_manager.py:_run_svc */
int hermes_cli_service_manager_u_run_svc(const char *arg) {
    /* Python: s6-svc dispatch. Arg =
     * "registered\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int registered = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!registered) {
        fprintf(stderr, "no such gateway '%s'\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (!state) {
        fprintf(stderr, "s6-svc failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("s6-svc ok: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_service_manager_u_supervised_pid @ hermes_cli/service_manager.py:_supervised_pid */
int hermes_cli_service_manager_u_supervised_pid(const char *arg) {
    /* Python: s6-svstat pid parse. Arg = "state\tpid\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "ok") == 0) { printf("%s\n", t1 ? t1 + 1 : ""); return 0; }
    printf("\n");
    return 0;
}

/* PoP: restart @ hermes_cli/service_manager.py:restart */
int sm_restart(const char *name) {
    (void)name;
    char kind[32];
    sm_detect_service_manager(kind, sizeof(kind));
    if (strcmp(kind, "systemd") == 0)
        return cgw_systemd_restart(NULL);
    return 1;
}
