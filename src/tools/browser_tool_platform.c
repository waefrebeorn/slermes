/*
 * browser_tool_platform.c — focused concern module extracted from
 * port_browser_tool.c (refactor-first monolith split). Port of
 * tools/browser_tool.py. Self-contained, opaque struct, minimal includes.
 */

#include "browser_tool_platform.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

struct browser_tool_platform {
    int unused;
};

browser_tool_platform_t *browser_tool_platform_init(void) { return calloc(1, sizeof(browser_tool_platform_t)); }
void browser_tool_platform_cleanup(browser_tool_platform_t *s) { free(s); }

/* PoP: _running_in_docker @ tools/browser_tool.py:_running_in_docker */
/* PoP: browser_running_in_docker @ tools/browser_tool.py:_running_in_docker */
bool browser_running_in_docker(void)
{
    if (access("/.dockerenv", F_OK) == 0) return true;

    FILE *f = fopen("/proc/1/cgroup", "r");
    if (f) {
        char buf[512];
        if (fgets(buf, sizeof(buf), f)) {
            fclose(f);
            if (strstr(buf, "docker")) return true;
        } else {
            fclose(f);
        }
    }
    return false;
}

/* PoP: _is_local_mode @ tools/browser_tool.py:_is_local_mode */
/* PoP: browser_is_local_mode @ tools/browser_tool.py:_is_local_mode */
bool browser_is_local_mode(void)
{
    /* Simplified - no CDP override and no cloud provider */
    const char *cdp_override = getenv("BROWSER_CDP_URL");
    if (cdp_override && strlen(cdp_override) > 0) return false;
    /* Would check cloud provider config here */
    return true;
}

/* PoP: browser_bare_task_id_for_session_key @ tools/browser_tool.py:_bare_task_id_for_session_key */
char *browser_bare_task_id_for_session_key(const char *session_key)
{
    const char *suffix = "::local";
    size_t suffix_len = strlen(suffix);

    if (!session_key) {
        char *def = malloc(8);
        if (def) strcpy(def, "default");
        return def;
    }

    size_t len = strlen(session_key);
    if (len > suffix_len && strcmp(session_key + len - suffix_len, suffix) == 0) {
        char *result = malloc(len - suffix_len + 1);
        if (result) {
            memcpy(result, session_key, len - suffix_len);
            result[len - suffix_len] = '\0';
        }
        return result;
    }

    char *result = malloc(len + 1);
    if (result) strcpy(result, session_key);
    return result;
}

/* PoP: browser_session_info_owned_by_task @ tools/browser_tool.py:_session_info_owned_by_task */
bool browser_session_info_owned_by_task(const char *session_info_json, const char *task_id, const char *session_key)
{
    if (!session_info_json || !task_id || !session_key) return false;

    /* Parse JSON for owner_task_id and session_key fields */
    /* Simplified implementation - looks for keys in JSON string */
    char owner_key[64];
    snprintf(owner_key, sizeof(owner_key), "\"owner_task_id\":\"");
    char *owner_pos = strstr(session_info_json, owner_key);
    if (owner_pos) {
        owner_pos += strlen(owner_key);
        char *end = strchr(owner_pos, '"');
        if (end) {
            size_t owner_len = end - owner_pos;
            if (owner_len != strlen(task_id) || strncmp(owner_pos, task_id, owner_len) != 0) {
                return false;
            }
        }
    }

    char key_key[64];
    snprintf(key_key, sizeof(key_key), "\"session_key\":\"");
    char *key_pos = strstr(session_info_json, key_key);
    if (key_pos) {
        key_pos += strlen(key_key);
        char *end = strchr(key_pos, '"');
        if (end) {
            size_t key_len = end - key_pos;
            if (key_len != strlen(session_key) || strncmp(key_pos, session_key, key_len) != 0) {
                return false;
            }
        }
    }

    return true;
}

/* PoP: browser_get_session_inactivity_timeout @ tools/browser_tool.py:_get_session_inactivity_timeout */
/* PoP: browser_get_session_inactivity_timeout @ tools/browser_tool.py:_get_session_inactivity_timeout */
int browser_get_session_inactivity_timeout(void)
{
    int result = 120; /* DEFAULT_SESSION_INACTIVITY_TIMEOUT */

    const char *env_timeout = getenv("BROWSER_INACTIVITY_TIMEOUT");
    if (env_timeout) {
        int val = atoi(env_timeout);
        if (val >= 30) result = val;
    }

    /* Floor at 30s */
    if (result < 30) result = 30;

    return result;
}

/* PoP: browser_agent_browser_candidate_present @ tools/browser_tool.py:_agent_browser_candidate_present */
bool browser_agent_browser_candidate_present(const char *path)
{
    if (!path) return false;

    /* Check for "npx agent-browser" pattern */
    if (strstr(path, "npx") == path && strstr(path, "agent-browser")) {
        return true;
    }

    /* Check if file exists and is executable */
    struct stat st;
    if (stat(path, &st) == 0) {
        #ifdef _WIN32
        return true; /* Windows - rely on extension */
        #else
        return (st.st_mode & S_IXUSR) != 0;
        #endif
    }

    return false;
}

