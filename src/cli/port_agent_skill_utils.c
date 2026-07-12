/*
 * port_agent_skill_utils.c — C port of agent/skill_utils.py
 */

#include "hermes_logger.h"
#include "path.h"
#include "skill_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>



/* Shared raw-config cache keyed by (path, mtime_ns, size) — mirrors Python's
 * module-level _RAW_CONFIG_CACHE. A tiny single-slot cache is enough: the
 * skill build path re-reads the same config.yaml repeatedly. */
static struct {
    char   *path;
    long    mtime_ns;   /* st_mtim.tv_sec*1e9 + tv_nsec */
    long    size;
    char   *content;    /* cached file contents */
    int     valid;
} s_raw_config_cache = {0};

/* PoP: cli_agent_skill_utils__raw_config_cache_clear @ agent/skill_utils.py:_raw_config_cache_clear */
/* Test hook — drop the shared raw config cache. */
void cli_agent_skill_utils__raw_config_cache_clear(void)
{
    free(s_raw_config_cache.path);
    free(s_raw_config_cache.content);
    s_raw_config_cache.path = NULL;
    s_raw_config_cache.content = NULL;
    s_raw_config_cache.mtime_ns = 0;
    s_raw_config_cache.size = 0;
    s_raw_config_cache.valid = 0;
}

/* PoP: cli_agent_skill_utils__load_raw_config @ agent/skill_utils.py:_load_raw_config */

/* Port of Python agent/skill_utils.py:_load_raw_config */
/* Read config.yaml with a shared mtime+size keyed cache. */
char *cli_agent_skill_utils__load_raw_config(void)
{
    const char *config_path = getenv("HERMES_CONFIG_PATH");
    char path[1024];
    if (!config_path || !*config_path) {
        /* Default: $HERMES_HOME/config.yaml */
        const char *home = getenv("HERMES_HOME");
        if (!home || !*home) {
            home = getenv("HOME");
            if (!home || !*home) {
                return strdup("{}");
            }
            snprintf(path, sizeof(path), "%s/.hermes/config.yaml", home);
            config_path = path;
        } else {
            snprintf(path, sizeof(path), "%s/config.yaml", home);
            config_path = path;
        }
    }

    /* Cache lookup keyed by (path, mtime_ns, size) */
    struct stat st;
    long mtime_ns = 0, size = 0;
    int have_stat = (stat(config_path, &st) == 0);
    if (have_stat) {
        mtime_ns = (long)st.st_mtim.tv_sec * 1000000000L + (long)st.st_mtim.tv_nsec;
        size = (long)st.st_size;
        if (s_raw_config_cache.valid && s_raw_config_cache.path &&
            strcmp(s_raw_config_cache.path, config_path) == 0 &&
            s_raw_config_cache.mtime_ns == mtime_ns &&
            s_raw_config_cache.size == size) {
            return strdup(s_raw_config_cache.content);
        }
    }

    /* Read config.yaml */
    FILE *f = fopen(config_path, "r");
    if (!f) {
        hermes_log(LOG_DEBUG, "skill_utils", "Cannot read config: %s", config_path);
        return strdup("{}");
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0) {
        fclose(f);
        return strdup("{}");
    }

    char *content = (char *)malloc((size_t)fsize + 1);
    if (!content) {
        fclose(f);
        return strdup("{}");
    }

    size_t n = fread(content, 1, (size_t)fsize, f);
    fclose(f);
    content[n] = '\0';

    /* Populate cache */
    if (have_stat) {
        free(s_raw_config_cache.path);
        free(s_raw_config_cache.content);
        s_raw_config_cache.path = strdup(config_path);
        s_raw_config_cache.content = strdup(content);
        s_raw_config_cache.mtime_ns = mtime_ns;
        s_raw_config_cache.size = size;
        s_raw_config_cache.valid = 1;
    }

    return content;
}

/* PoP: cli_agent_skill_utils__resolve_for_skill_ownership @ agent/skill_utils.py:_resolve_for_skill_ownership */
/* Expand ~ then resolve to a canonical absolute path. Falls back to an
 * unresolved absolute path when the target does not exist (mirrors Python's
 * expanduser().resolve() with .absolute() fallback). Caller frees. */
char *cli_agent_skill_utils__resolve_for_skill_ownership(const char *path)
{
    if (!path) return NULL;
    /* expanduser: leading ~ → $HOME */
    char expanded[2048];
    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home && *home)
            snprintf(expanded, sizeof(expanded), "%s%s", home, path + 1);
        else
            snprintf(expanded, sizeof(expanded), "%s", path);
    } else {
        snprintf(expanded, sizeof(expanded), "%s", path);
    }
    char *resolved = path_resolve(expanded);   /* realpath — NULL if missing */
    if (resolved) return resolved;
    return path_abs(expanded);                 /* absolute w/o resolving links */
}

/* PoP: cli_agent_skill_utils__is_external_skill_path @ agent/skill_utils.py:is_external_skill_path */
/* Return 1 when path lives under a configured external skills dir. */
int cli_agent_skill_utils__is_external_skill_path(const char *path)
{
    if (!path) return 0;
    char *candidate = cli_agent_skill_utils__resolve_for_skill_ownership(path);
    if (!candidate) return 0;

    const char *config_path = getenv("HERMES_CONFIG_PATH");
    char cfgbuf[1024];
    const char *home = getenv("HERMES_HOME");
    char homebuf[1024];
    if (!home || !*home) {
        const char *h = getenv("HOME");
        if (h && *h) { snprintf(homebuf, sizeof(homebuf), "%s/.hermes", h); home = homebuf; }
        else home = "";
    }
    if (!config_path || !*config_path) {
        snprintf(cfgbuf, sizeof(cfgbuf), "%s/config.yaml", home);
        config_path = cfgbuf;
    }
    char local_skills[1024];
    snprintf(local_skills, sizeof(local_skills), "%s/skills", home);

    size_t count = 0;
    char **ext = skill_get_external_dirs(config_path, home, local_skills, &count);
    int is_external = 0;
    for (size_t i = 0; i < count; i++) {
        char *root = cli_agent_skill_utils__resolve_for_skill_ownership(ext[i]);
        if (root) {
            /* path_within_dir returns NULL when candidate is inside root */
            char *err = path_within_dir(candidate, root);
            if (err == NULL) is_external = 1;
            else free(err);
            free(root);
        }
        free(ext[i]);
        if (is_external) { /* free remaining entries */
            for (size_t j = i + 1; j < count; j++) free(ext[j]);
            break;
        }
    }
    free(ext);
    free(candidate);
    return is_external;
}
