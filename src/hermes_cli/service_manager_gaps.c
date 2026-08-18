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
