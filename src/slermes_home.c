/*
 * slermes_home.c — Slermes home directory infrastructure
 *
 * MIT License — Slermes Fork
 */
#include "slermes_home.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <pwd.h>

/* ── Static home path buffer ──────────────────────────────────────── */
static char s_slermes_home[1024] = {0};
static bool s_initialized = false;

/* Resolve the default (pre-profile) home — byte-parity with hermes's
 * get_default_hermes_root():
 *
 *   1. Native home is <HOME>/.hermes (POSIX) — the slermes identity uses
 *      SLERMES_HOME as its native-home override (test/container shims).
 *   2. If HERMES_HOME is unset            -> native home.
 *   3. If HERMES_HOME is under native home -> native home (profile mode:
 *      HERMES_HOME may be <native>/profiles/<name>; the root is native).
 *   4. Else (Docker/custom):
 *        - if HERMES_HOME's parent dir is "profiles" -> grandparent
 *        - else HERMES_HOME itself is the root.
 */
static const char *resolve_home(void) {
    if (s_slermes_home[0]) return s_slermes_home;

    char native[1024];
    const char *slenv = getenv(SLERMES_HOME_ENV);
    if (slenv && slenv[0]) {
        snprintf(native, sizeof(native), "%s", slenv);
    } else {
        const char *user_home = getenv("HOME");
        if (!user_home) {
            struct passwd *pw = getpwuid(getuid());
            if (pw) user_home = pw->pw_dir;
        }
        if (user_home && user_home[0])
            snprintf(native, sizeof(native), "%s/%s", user_home, SLERMES_HOME_DEFAULT);
        else
            snprintf(native, sizeof(native), "/tmp/%s", SLERMES_HOME_DEFAULT);
    }

    const char *env_home = getenv("HERMES_HOME");
    if (!env_home || !env_home[0]) {
        snprintf(s_slermes_home, sizeof(s_slermes_home), "%s", native);
        return s_slermes_home;
    }

    /* HERMES_HOME set: is it under the native home? (profile mode) */
    size_t nl = strlen(native);
    if (strncmp(env_home, native, nl) == 0 &&
        (env_home[nl] == '\0' || env_home[nl] == '/')) {
        /* under native (normal or profile mode) -> native is the root */
        snprintf(s_slermes_home, sizeof(s_slermes_home), "%s", native);
        return s_slermes_home;
    }

    /* Docker / custom deployment: HERMES_HOME outside native home. */
    size_t el = strlen(env_home);
    if (el > 0) {
        /* strip trailing slash */
        while (el > 1 && env_home[el - 1] == '/') el--;
        /* if the immediate parent dir is "profiles", root = grandparent */
        const char *last_slash = NULL;
        for (const char *p = env_home; p < env_home + el; p++)
            if (*p == '/') last_slash = p;
        if (last_slash) {
            const char *parent = last_slash + 1;
            size_t plen = (size_t)(env_home + el - parent);
            if (plen == 8 && strncmp(parent, "profiles", 8) == 0) {
                /* root = grandparent */
                const char *prev_slash = NULL;
                for (const char *p = env_home; p < last_slash; p++)
                    if (*p == '/') prev_slash = p;
                size_t glen = prev_slash ? (size_t)(prev_slash - env_home)
                                         : (size_t)(last_slash - env_home);
                if (glen == 0) {
                    snprintf(s_slermes_home, sizeof(s_slermes_home), "/");
                } else {
                    memcpy(s_slermes_home, env_home, glen);
                    s_slermes_home[glen] = '\0';
                }
                return s_slermes_home;
            }
        }
        /* not a profile path: HERMES_HOME itself is the root */
        snprintf(s_slermes_home, sizeof(s_slermes_home), "%.*s", (int)el, env_home);
        return s_slermes_home;
    }

    snprintf(s_slermes_home, sizeof(s_slermes_home), "%s", native);
    return s_slermes_home;
}

/* ── Public API ────────────────────────────────────────────────────── */

const char *slermes_home(void) {
    return resolve_home();
}

char *slermes_path(char *buf, size_t bufsz, const char *relpath) {
    snprintf(buf, bufsz, "%s/%s", slermes_home(), relpath ? relpath : "");
    return buf;
}

/* Create directory if it doesn't exist */
static bool mkdir_p(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode);

    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

bool slermes_init(void) {
    if (s_initialized) return true;

    const char *home = slermes_home();
    if (access(home, F_OK) != 0) {
        if (mkdir_p(home) != true) {
            fprintf(stderr, "slermes: failed to create home %s\n", home);
            return false;
        }
    }

    /* Create standard subdirectories */
    static const char *dirs[] = {
        SLERMES_DIR_SESSIONS,
        SLERMES_DIR_SKILLS,
        SLERMES_DIR_CRON,
        SLERMES_DIR_PROFILES,
        SLERMES_DIR_CACHE,
        SLERMES_DIR_PLUGINS,
        SLERMES_DIR_LOGS,
        NULL
    };

    for (int i = 0; dirs[i]; i++) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", home, dirs[i]);
        mkdir(path, 0755);
    }

    s_initialized = true;
    return true;
}

bool slermes_initialized(void) {
    return s_initialized;
}

/* Default config YAML */
const char *slermes_default_config(void) {
    return
        "# Slermes Agent Configuration\n"
        "# See https://slermes.nousresearch.com/docs for full reference\n"
        "\n"
        "provider: nous\n"
        "model:\n"
        "  default: hermes-4-flash\n"
        "  base_url: https://inference-api.nousresearch.com/v1\n"
        "\n"
        "skills:\n"
        "  dir: \"${SLERMES_HOME}/skills\"\n"
        "\n"
        "gateway:\n"
        "  enabled: false\n"
        "\n"
        "desktop:\n"
        "  sidebar_width: 237\n"
        "  titlebar_height: 34\n"
        "  statusbar_height: 20\n"
        "  padding: 16\n"
        "  theme: dark\n";
}
