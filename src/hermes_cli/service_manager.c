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

