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

/* Resolve the home directory */
static const char *resolve_home(void) {
    if (s_slermes_home[0]) return s_slermes_home;

    const char *env = getenv(SLERMES_HOME_ENV);
    if (env && env[0]) {
        snprintf(s_slermes_home, sizeof(s_slermes_home), "%s", env);
        return s_slermes_home;
    }

    const char *user_home = NULL;
#ifdef _WIN32
    /* Windows: USERPROFILE is authoritative. HOME is a POSIX convention that
     * only exists when the binary is launched from MSYS/Cygwin, where it points
     * at /c/msys64/home/<user> -- a *different* directory from the real profile.
     * Preferring HOME there silently relocated config, skills and plugins to the
     * toolchain's home, so `slermes status` read one config from a CABI shell and
     * another from cmd.exe. */
    user_home = getenv("USERPROFILE");
    if (!user_home || !user_home[0]) {
        const char *drive = getenv("HOMEDRIVE");
        const char *path = getenv("HOMEPATH");
        static char winhome[512];
        if (drive && path && drive[0] && path[0]) {
            snprintf(winhome, sizeof winhome, "%s%s", drive, path);
            user_home = winhome;
        }
    }
    if (!user_home || !user_home[0]) user_home = getenv("HOME");
#else
    user_home = getenv("HOME");
    if (!user_home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) user_home = pw->pw_dir;
    }
#endif

    if (user_home && user_home[0]) {
        snprintf(s_slermes_home, sizeof(s_slermes_home),
                 "%s/%s", user_home, SLERMES_HOME_DEFAULT);
    } else {
#ifdef _WIN32
        const char *tmp = getenv("TEMP");
        if (!tmp || !tmp[0]) tmp = ".";
        snprintf(s_slermes_home, sizeof(s_slermes_home),
                 "%s/%s", tmp, SLERMES_HOME_DEFAULT);
#else
        snprintf(s_slermes_home, sizeof(s_slermes_home),
                 "/tmp/%s", SLERMES_HOME_DEFAULT);
#endif
    }
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
