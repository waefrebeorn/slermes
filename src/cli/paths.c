/*
 * paths.c — Path resolution for Hermes C (P21: hermes_constants port).
 * Resolves SLERMES_HOME, XDG-style subdirs, and profile-aware paths.
 */


/* PoP: Hermes paths (port of hermes_constants) */

#include "hermes_core_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <shlobj.h>
#endif

/* Active profile name (NULL means default/unset) */
static char active_profile[64] = "";

/* ================================================================
 *  Internal: resolve base SLERMES_HOME directory
 * ================================================================ */

void hermes_user_home(char *buf, size_t sz);  /* public: see below */

static void resolve_slermes_home(char *buf, size_t sz) {
    extern const char *hc_get_hermes_home_override(void);
    const char *ov = hc_get_hermes_home_override();
    if (ov && *ov) { snprintf(buf, sz, "%s", ov); return; }
    const char *env = getenv("SLERMES_HOME");
    if (env && env[0]) {
        snprintf(buf, sz, "%s", env);
        return;
    }
    hermes_user_home(buf, sz);
    size_t n = strlen(buf);
    snprintf(buf + n, sz > n ? sz - n : 0, "/.slermes");
}

/* Where the Windows profile actually is.
 *
 * getenv("HOME") is not it: launched from MSYS bash it reads /home/eman5, and from a stripped
 * environment it is unset while USERPROFILE is empty too -- which produced
 * 'C:\msys64\home\eman5/.slermes/config.yaml' (no config, no skills, empty tool set) while
 * C:\Users\eman5\.slermes\config.yaml sat there. Ask Windows when the environment cannot say. */
void hermes_user_home(char *buf, size_t sz) {
    if (!buf || sz == 0) return;
    buf[0] = '\0';

    /* MSYS rewrites HOME to its own fake profile (C:\msys64\home\eman5), which looks like a
     * Windows path and quietly won over the real one: the server reported "No config" while
     * C:\Users\eman5\.slermes\config.yaml existed. Ask Windows first, and refuse an MSYS home. */
    const char *prof = getenv("USERPROFILE");
    if (prof && prof[0] && !strstr(prof, "\\msys64\\") && !strstr(prof, "/msys64/")) {
        snprintf(buf, sz, "%s", prof);
        return;
    }

#ifdef _WIN32
    char tmp[MAX_PATH];
    if (SHGetFolderPathA(NULL, 0x0028 /* CSIDL_PROFILE */, NULL, 0, tmp) == S_OK && tmp[0]) {
        snprintf(buf, sz, "%s", tmp);
        return;
    }
#endif

    const char *home = getenv("HOME");
    if (home && home[0] && !strstr(home, "msys64")) { snprintf(buf, sz, "%s", home); return; }
    snprintf(buf, sz, "%s", "/tmp");
}

/* ================================================================
 *  Public API
 * ================================================================ */

void hermes_get_home(char *buf, size_t sz) {
    resolve_slermes_home(buf, sz);
}

void hermes_config_dir(char *buf, size_t sz) {
    hermes_get_home(buf, sz);
    /* Already points to ~/.slermes/ which IS the config dir */
}

void hermes_data_dir(char *buf, size_t sz) {
    char home[HERMES_PATH_MAX];
    hermes_get_home(home, sizeof(home));
    snprintf(buf, sz, "%s/data", home);
}

void hermes_cache_dir(char *buf, size_t sz) {
    char home[HERMES_PATH_MAX];
    hermes_get_home(home, sizeof(home));
    snprintf(buf, sz, "%s/cache", home);
}

void hermes_log_dir(char *buf, size_t sz) {
    char home[HERMES_PATH_MAX];
    hermes_get_home(home, sizeof(home));
    snprintf(buf, sz, "%s/logs", home);
}

void hermes_resolve_path(const char *sub, char *buf, size_t sz) {
    char home[HERMES_PATH_MAX];
    hermes_get_home(home, sizeof(home));
    if (sub && sub[0])
        snprintf(buf, sz, "%s/%s", home, sub);
    else
        snprintf(buf, sz, "%s", home);
}

void hermes_display_home(void) {
    char buf[HERMES_PATH_MAX];

    hermes_get_home(buf, sizeof(buf));
    printf("SLERMES_HOME:  %s\n", buf);

    hermes_config_dir(buf, sizeof(buf));
    printf("Config dir:    %s\n", buf);

    hermes_data_dir(buf, sizeof(buf));
    printf("Data dir:      %s\n", buf);

    hermes_cache_dir(buf, sizeof(buf));
    printf("Cache dir:     %s\n", buf);

    hermes_log_dir(buf, sizeof(buf));
    printf("Log dir:       %s\n", buf);

    const char *profile = hermes_get_profile();
    if (profile && profile[0])
        printf("Profile:       %s\n", profile);
    else
        printf("Profile:       (default)\n");
}

void hermes_set_profile(const char *name) {
    if (name && name[0])
        snprintf(active_profile, sizeof(active_profile), "%s", name);
    else
        active_profile[0] = '\0';
}

/* Port of Python agent/coding_context.py:get_profile(). */
const char *hermes_get_profile(void) {
    return active_profile[0] ? active_profile : NULL;
}
