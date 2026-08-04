/*
 * port_startup_fast.c — C11 port of pure helpers from hermes_cli/_startup_fast.py.
 *
 * Faithful translations of the deterministic, stdlib-only startup helpers.
 * Reuses libpath (lib/libpath/path.h) for path resolution.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "port_startup_fast.h"
#include "libpath/path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* Constants / helpers                                                */
/* ------------------------------------------------------------------ */

#define SHELL_HOME_LEN 256

static void sf_resolve_home(char *buf, size_t bufsize)
{
    const char *hermes_home = getenv("HERMES_HOME");
    if (hermes_home && *hermes_home) {
        snprintf(buf, bufsize, "%s", hermes_home);
        return;
    }
    const char *home = getenv("HOME");
    if (home && *home)
        snprintf(buf, bufsize, "%s/.hermes", home);
    else
        snprintf(buf, bufsize, "/.hermes");
}

/* ------------------------------------------------------------------ */
/* is_termux_env                                                       */
/* ------------------------------------------------------------------ */

/* PoP: is_termux_env @ hermes_cli/_startup_fast.py:is_termux_env */
bool sf_is_termux_env(void)
{
    const char *termux_version = getenv("TERMUX_VERSION");
    if (termux_version && *termux_version)
        return true;
    const char *prefix = getenv("PREFIX");
    if (prefix) {
        if (strstr(prefix, "com.termux/files/usr") != NULL)
            return true;
        if (strncmp(prefix, "/data/data/com.termux/", 22) == 0)
            return true;
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* is_termux_fast_version_argv                                         */
/* ------------------------------------------------------------------ */

static bool sf_argv_matches_one(int argc, char **argv, const char *target)
{
    if (argc != 2) return false;
    return strcmp(argv[1], target) == 0;
}

/* PoP: is_termux_fast_version_argv @ hermes_cli/_startup_fast.py:is_termux_fast_version_argv */
bool sf_is_termux_fast_version_argv(int argc, char **argv)
{
    /* argv in (["--version"], ["-V"], ["version"]) */
    return sf_argv_matches_one(argc, argv, "--version")
        || sf_argv_matches_one(argc, argv, "-V")
        || sf_argv_matches_one(argc, argv, "version");
}

/* ------------------------------------------------------------------ */
/* is_global_fast_version_argv                                         */
/* ------------------------------------------------------------------ */

/* PoP: is_global_fast_version_argv @ hermes_cli/_startup_fast.py:is_global_fast_version_argv */
bool sf_is_global_fast_version_argv(int argc, char **argv)
{
    /* argv in (["--version"], ["-V"]) */
    return sf_argv_matches_one(argc, argv, "--version")
        || sf_argv_matches_one(argc, argv, "-V");
}

/* ------------------------------------------------------------------ */
/* is_container_startup_environment                                    */
/* ------------------------------------------------------------------ */

/* PoP: is_container_startup_environment @ hermes_cli/_startup_fast.py:is_container_startup_environment */
bool sf_is_container_startup_environment(void)
{
    if (access("/.dockerenv", F_OK) == 0)
        return true;
    if (access("/run/.containerenv", F_OK) == 0)
        return true;
    FILE *f = fopen("/proc/1/cgroup", "r");
    if (!f) return false;
    char line[512];
    bool found = false;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "docker") || strstr(line, "podman") || strstr(line, "/lxc/")) {
            found = true;
            break;
        }
    }
    fclose(f);
    return found;
}

/* ------------------------------------------------------------------ */
/* active_profile_may_override_home                                    */
/* ------------------------------------------------------------------ */

/* PoP: active_profile_may_override_home @ hermes_cli/_startup_fast.py:active_profile_may_override_home */
bool sf_active_profile_may_override_home(const char *hermes_root)
{
    if (!hermes_root) return false;
    char active_profile[4096];
    snprintf(active_profile, sizeof(active_profile), "%s/active_profile", hermes_root);
    if (access(active_profile, R_OK) != 0)
        return false;
    FILE *f = fopen(active_profile, "r");
    if (!f) return false;
    char buf[256];
    if (!fgets(buf, sizeof(buf), f)) {
        fclose(f);
        return false;
    }
    fclose(f);
    /* strip whitespace */
    size_t len = strlen(buf);
    while (len > 0 && isspace((unsigned char)buf[len - 1])) len--;
    buf[len] = '\0';
    return (len > 0 && strcmp(buf, "default") != 0);
}

/* ------------------------------------------------------------------ */
/* container_mode_may_be_active                                        */
/* ------------------------------------------------------------------ */

/* PoP: container_mode_may_be_active @ hermes_cli/_startup_fast.py:container_mode_may_be_active */
bool sf_container_mode_may_be_active(void)
{
    if (getenv("HERMES_DEV") && strcmp(getenv("HERMES_DEV"), "1") == 0)
        return false;
    if (sf_is_container_startup_environment())
        return false;
    char hermes_root[4096];
    sf_resolve_home(hermes_root, sizeof(hermes_root));
    const char *hermes_home = getenv("HERMES_HOME");
    if (hermes_home && *hermes_home) {
        char container_mode[4096];
        snprintf(container_mode, sizeof(container_mode), "%s/.container-mode", hermes_home);
        if (access(container_mode, F_OK) == 0)
            return true;
        /* parent_name != "profiles" and active_profile_may_override_home */
        char parent[4096];
        strncpy(parent, hermes_home, sizeof(parent) - 1);
        parent[sizeof(parent) - 1] = '\0';
        /* dirname */
        char *last = strrchr(parent, '/');
        if (last) {
            char parent_name[256];
            if (last == parent) {
                strcpy(parent_name, "/");
            } else {
                *last = '\0';
                char *base = strrchr(parent, '/');
                if (base) {
                    base++;
                    strncpy(parent_name, base, sizeof(parent_name) - 1);
                    parent_name[sizeof(parent_name) - 1] = '\0';
                } else {
                    strncpy(parent_name, parent, sizeof(parent_name) - 1);
                    parent_name[sizeof(parent_name) - 1] = '\0';
                }
            }
            if (strcmp(parent_name, "profiles") != 0)
                return sf_active_profile_may_override_home(hermes_home);
        }
        return false;
    }
    /* default home */
    if (sf_active_profile_may_override_home(hermes_root))
        return true;
    char container_mode[4096];
    snprintf(container_mode, sizeof(container_mode), "%s/.container-mode", hermes_root);
    return access(container_mode, F_OK) == 0;
}

/* ------------------------------------------------------------------ */
/* read_openai_version                                                 */
/* ------------------------------------------------------------------ */

/* PoP: read_openai_version @ hermes_cli/_startup_fast.py:read_openai_version */
char *sf_read_openai_version(void)
{
    /* Walk PATH-like dirs looking for openai/_version.py, then parse
     * __version__ = "..." */
    const char *path_env = getenv("PYTHONPATH");
    if (!path_env) path_env = getenv("PATH");
    if (!path_env) return NULL;

    char penv[8192];
    strncpy(penv, path_env, sizeof(penv) - 1);
    penv[sizeof(penv) - 1] = '\0';

    char *saveptr;
    char *dir = strtok_r(penv, ":", &saveptr);
    while (dir) {
        char vfile[4096];
        if (*dir == '\0')
            snprintf(vfile, sizeof(vfile), "%s/openai/_version.py", ".");
        else
            snprintf(vfile, sizeof(vfile), "%s/openai/_version.py", dir);
        FILE *f = fopen(vfile, "r");
        if (f) {
            char line[256];
            char *result = NULL;
            while (fgets(line, sizeof(line), f)) {
                char *p = line;
                while (*p == ' ' || *p == '\t') p++;
                if (strncmp(p, "__version__", 11) != 0) continue;
                p += 11;
                while (*p == ' ' || *p == '\t') p++;
                if (*p != '=') continue;
                p++;
                while (*p == ' ' || *p == '\t') p++;
                /* skip quote */
                if (*p == '"' || *p == '\'') p++;
                char *end = p;
                while (*end && *end != '"' && *end != '\'' && *end != '\n' && *end != '\r')
                    end++;
                size_t vlen = end - p;
                if (vlen > 0) {
                    result = malloc(vlen + 1);
                    if (result) {
                        memcpy(result, p, vlen);
                        result[vlen] = '\0';
                    }
                }
                fclose(f);
                return result;
            }
            fclose(f);
        }
        dir = strtok_r(NULL, ":", &saveptr);
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* read_install_method                                                 */
/* ------------------------------------------------------------------ */

/* PoP: read_install_method @ hermes_cli/_startup_fast.py:read_install_method */
char *sf_read_install_method(void)
{
    char hermes_root[4096];
    sf_resolve_home(hermes_root, sizeof(hermes_root));
    char stamp[4096];
    snprintf(stamp, sizeof(stamp), "%s/.install_method", hermes_root);
    FILE *f = fopen(stamp, "r");
    if (!f) return NULL;
    char buf[256];
    if (!fgets(buf, sizeof(buf), f)) {
        fclose(f);
        return NULL;
    }
    fclose(f);
    /* strip + lowercase */
    size_t len = strlen(buf);
    size_t s = 0;
    while (s < len && isspace((unsigned char)buf[s])) s++;
    while (len > s && isspace((unsigned char)buf[len - 1])) len--;
    char *result = NULL;
    if (len > s) {
        result = malloc(len - s + 1);
        if (result) {
            for (size_t i = 0; i < len - s; i++)
                result[i] = (char)tolower((unsigned char)buf[s + i]);
            result[len - s] = '\0';
        }
    }
    return result;
}

/* ------------------------------------------------------------------ */
/* _resolved_home                                                      */
/* ------------------------------------------------------------------ */

/* PoP: _resolved_home @ hermes_cli/_startup_fast.py:_resolved_home */
char *sf_resolved_home(void)
{
    char buf[4096];
    sf_resolve_home(buf, sizeof(buf));
    return strdup(buf);
}

/* ------------------------------------------------------------------ */
/* project_root_str                                                    */
/* ------------------------------------------------------------------ */

/* PoP: project_root_str @ hermes_cli/_startup_fast.py:project_root_str */
char *sf_project_root_str(void)
{
    /* os.path.realpath(os.path.join(os.path.dirname(__file__), os.pardir))
     * __file__ = src/hermes_cli/_startup_fast.py (from source tree)
     * parent = hermes_cli/.. = repo root
     * We approximate: realpath of the project root at build time.
     */
    const char *root = getenv("SLERMES_PROJECT_ROOT");
    if (root) return strdup(root);
    /* Fallback: use a compile-time define or the current directory */
    char *cwd = getcwd(NULL, 0);
    return cwd;
}
