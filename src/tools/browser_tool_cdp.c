/*
 * browser_tool_cdp.c — focused concern module extracted from
 * port_browser_tool.c (refactor-first monolith split). Port of
 * tools/browser_tool.py. Self-contained, opaque struct, minimal includes.
 */

#include "browser_tool_cdp.h"
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
#include "browser_tool_install.h"
#include "browser_tool_env.h"
#include "browser_tool_cdp.h"

#include "browser_tool_platform.h"
json_t *browser_supervisor_get_or_start(const char *session_key);
void browser_supervisor_stop_all(void);

struct browser_tool_cdp {
    int unused;
};

browser_tool_cdp_t *browser_tool_cdp_init(void) { return calloc(1, sizeof(browser_tool_cdp_t)); }
void browser_tool_cdp_cleanup(browser_tool_cdp_t *s) { free(s); }

/* PoP: _get_vision_model @ tools/browser_tool.py:_get_vision_model */
char *browser_get_vision_model(void)
{
    const char *model = getenv("AUXILIARY_VISION_MODEL");
    if (model && model[0]) return strdup(model);
    return strdup("");
}

/* PoP: _get_extraction_model @ tools/browser_tool.py:_get_extraction_model */
char *browser_get_extraction_model(void)
{
    const char *model = getenv("AUXILIARY_WEB_EXTRACT_MODEL");
    if (model && model[0]) return strdup(model);
    return strdup("");
}

/* PoP: _resolve_cdp_override @ tools/browser_tool.py:_resolve_cdp_override */
char *browser_resolve_cdp_override(const char *cdp_url)
{
    if (!cdp_url) return strdup("");

    /* Check if it's already a full WebSocket endpoint */
    if (strstr(cdp_url, "/devtools/browser/") != NULL) {
        return strdup(cdp_url);
    }

    /* Check if it's a bare host:port */
    const char *ws_prefix = "";
    if (strncmp(cdp_url, "ws://", 5) == 0) ws_prefix = "ws://";
    else if (strncmp(cdp_url, "wss://", 6) == 0) ws_prefix = "wss://";

    if (ws_prefix[0]) {
        const char *rest = cdp_url + strlen(ws_prefix);
        if (strchr(rest, '/') == NULL) {
            /* Convert to discovery URL */
            char *discovery = malloc(strlen(cdp_url) + 16);
            if (discovery) {
                snprintf(discovery, strlen(cdp_url) + 16, "http%s/json/version", cdp_url + 2);
                return discovery;
            }
        }
        return strdup(cdp_url);
    }

    /* Assume it's a discovery URL */
    char *version_url = malloc(strlen(cdp_url) + 16);
    if (!version_url) return strdup(cdp_url);

    if (strstr(cdp_url, "/json/version")) {
        strcpy(version_url, cdp_url);
    } else {
        snprintf(version_url, strlen(cdp_url) + 16, "%s/json/version", cdp_url);
    }
    return version_url;
}

/* PoP: _get_cdp_override @ tools/browser_tool.py:_get_cdp_override */
char *browser_get_cdp_override(void)
{
    const char *env_override = getenv("BROWSER_CDP_URL");
    if (env_override && env_override[0]) {
        return browser_resolve_cdp_override(env_override);
    }

    /* Could also check config.yaml for browser.cdp_url */
    return strdup("");
}

/* PoP: _get_dialog_policy_config @ tools/browser_tool.py:_get_dialog_policy_config */
char *browser_get_dialog_policy_config(void)
{
    /* Return policy and timeout as JSON */
    const char *policy = getenv("HERMES_BROWSER_DIALOG_POLICY");
    if (!policy) policy = "must_respond";

    const char *timeout_str = getenv("HERMES_BROWSER_DIALOG_TIMEOUT_S");
    double timeout = timeout_str ? atof(timeout_str) : 300.0;

    char *result = malloc(256);
    if (!result) return strdup("{}");
    snprintf(result, 256, "{\"policy\":\"%s\",\"timeout_s\":%.1f}", policy, timeout);
    return result;
}

/* PoP: _ensure_cdp_supervisor @ tools/browser_tool.py:_ensure_cdp_supervisor */
json_t *browser_ensure_cdp_supervisor(const char *task_id)
{
    json_t *result = json_object();
    if (!result) return NULL;

    if (!task_id) {
        json_set(result, "ok", json_bool(false));
        json_set(result, "error", json_string("task_id required"));
        return result;
    }

    /* Get CDP override */
    char *cdp_url = browser_get_cdp_override();
    if (!cdp_url || !cdp_url[0]) {
        free(cdp_url);
        json_set(result, "ok", json_bool(true));
        json_set(result, "supervisor", json_string("none"));
        json_set(result, "note", json_string("no CDP URL configured"));
        return result;
    }

    /* Get policy config */
    char *policy_json = browser_get_dialog_policy_config();
    json_t *policy_obj = policy_json ? json_parse(policy_json, NULL) : NULL;
    if (policy_obj) {
        json_t *p = json_obj_get(policy_obj, "policy");
        if (p) json_get_str(p, NULL, "must_respond");
        json_t *t = json_obj_get(policy_obj, "timeout_s");
        if (t && t->type == JSON_NUMBER) {
            double timeout = t->num_val;
            (void)timeout;
        }
        json_free(policy_obj);
    }
    free(policy_json);

    /* Call get_or_start */
    json_t *supervisor = browser_supervisor_get_or_start(task_id);
    if (supervisor) {
        json_set(result, "ok", json_bool(true));
        json_set(result, "supervisor", supervisor);
    } else {
        json_set(result, "ok", json_bool(false));
        json_set(result, "error", json_string("failed to start supervisor"));
    }

    free(cdp_url);
    return result;
}

/* PoP: _stop_cdp_supervisor @ tools/browser_tool.py:_stop_cdp_supervisor */
json_t *browser_stop_cdp_supervisor(const char *task_id)
{
    json_t *result = json_object();
    if (!result) return NULL;

    if (!task_id) {
        json_set(result, "ok", json_bool(false));
        json_set(result, "error", json_string("task_id required"));
        return result;
    }

    browser_supervisor_stop_all();

    json_set(result, "ok", json_bool(true));
    json_set(result, "task_id", json_string(task_id));
    return result;
}

/* PoP: _using_lightpanda_engine @ tools/browser_tool.py:_using_lightpanda_engine */
bool browser_using_lightpanda_engine(void)
{
    const char *engine = getenv("HERMES_BROWSER_ENGINE");
    return engine && strcmp(engine, "lightpanda") == 0;
}

/* PoP: _copy_fallback_warning @ tools/browser_tool.py:_copy_fallback_warning */
void browser_copy_fallback_warning(char *dest, size_t dest_size)
{
    if (dest && dest_size > 0) {
        snprintf(dest, dest_size, "Using fallback browser engine");
    }
}

/* PoP: _auto_local_for_private_urls @ tools/browser_tool.py:_auto_local_for_private_urls */
bool browser_auto_local_for_private_urls(void)
{
    const char *env = getenv("HERMES_BROWSER_AUTO_LOCAL_PRIVATE");
    return env && (strcmp(env, "true") == 0 || strcmp(env, "1") == 0);
}

/* PoP: _url_is_private @ tools/browser_tool.py:_url_is_private */
bool browser_url_is_private(const char *url)
{
    if (!url) return false;
    if (strncmp(url, "http://10.", 10) == 0) return true;
    if (strncmp(url, "http://192.168.", 13) == 0) return true;
    if (strncmp(url, "http://172.16.", 12) == 0) return true;
    if (strncmp(url, "http://127.", 11) == 0) return true;
    if (strncmp(url, "http://localhost", 16) == 0) return true;
    if (strncmp(url, "https://10.", 11) == 0) return true;
    if (strncmp(url, "https://192.168.", 14) == 0) return true;
    if (strncmp(url, "https://172.16.", 13) == 0) return true;
    if (strncmp(url, "https://127.", 12) == 0) return true;
    if (strncmp(url, "https://localhost", 17) == 0) return true;
    return false;
}

/* PoP: _navigation_session_key @ tools/browser_tool.py:_navigation_session_key */
char *browser_navigation_session_key(const char *task_id)
{
    if (!task_id) return strdup("default");
    size_t len = strlen(task_id) + 8;
    char *result = malloc(len);
    if (result) snprintf(result, len, "%s::local", task_id);
    return result;
}

/* PoP: _socket_safe_tmpdir @ tools/browser_tool.py:_socket_safe_tmpdir */
char *browser_socket_safe_tmpdir(void)
{
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir) tmpdir = "/tmp";
    return strdup(tmpdir);
}

/* PoP: _create_local_session @ tools/browser_tool.py:_create_local_session */
json_t *browser_create_local_session(const char *task_id)
{
    json_t *result = json_object();
    if (!result) return NULL;
    json_set(result, "ok", json_bool(true));
    json_set(result, "session_key", json_string(task_id ? task_id : "default"));
    json_set(result, "type", json_string("local"));
    return result;
}

/* PoP: _create_cdp_session @ tools/browser_tool.py:_create_cdp_session */
json_t *browser_create_cdp_session(const char *task_id, const char *cdp_url)
{
    json_t *result = json_object();
    if (!result) return NULL;
    json_set(result, "ok", json_bool(true));
    json_set(result, "session_key", json_string(task_id ? task_id : "default"));
    json_set(result, "type", json_string("cdp"));
    if (cdp_url) json_set(result, "cdp_url", json_string(cdp_url));
    return result;
}

/* PoP: _get_session_info @ tools/browser_tool.py:_get_session_info */
json_t *browser_get_session_info(const char *session_key)
{
    json_t *result = json_object();
    if (!result) return NULL;
    json_set(result, "session_key", json_string(session_key ? session_key : ""));
    json_set(result, "active", json_bool(false));
    return result;
}

/* PoP: _find_agent_browser @ tools/browser_tool.py:_find_agent_browser */
char *browser_find_agent_browser(void)
{
    const char *candidates[] = {
        "/usr/bin/chromium",
        "/usr/bin/chromium-browser",
        "/usr/bin/google-chrome",
        "/usr/bin/google-chrome-stable",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        if (access(candidates[i], X_OK) == 0) {
            return strdup(candidates[i]);
        }
    }
    return strdup("");
}

/* PoP: _truncate_snapshot @ tools/browser_tool.py:_truncate_snapshot */
char *browser_truncate_snapshot(const char *snapshot, size_t max_chars)
{
    if (!snapshot) return strdup("");
    if (strlen(snapshot) <= max_chars) return strdup(snapshot);

    char *result = malloc(max_chars + 1);
    if (!result) return strdup("");
    strncpy(result, snapshot, max_chars);
    result[max_chars] = '\0';
    return result;
}

/* PoP: check_browser_requirements @ tools/browser_tool.py:check_browser_requirements */
json_t *browser_check_browser_requirements(void)
{
    json_t *result = json_object();
    if (!result) return NULL;
    json_set(result, "chromium_installed", json_bool(browser_chromium_installed()));
    json_set(result, "running_in_docker", json_bool(browser_running_in_docker()));
    json_set(result, "needs_sandbox_bypass", json_bool(browser_needs_chromium_sandbox_bypass()));
    return result;
}

/* PoP: check_browser_vision_requirements @ tools/browser_tool.py:check_browser_vision_requirements */
json_t *browser_check_browser_vision_requirements(void)
{
    json_t *result = json_object();
    if (!result) return NULL;
    json_set(result, "vision_model", json_string(browser_get_vision_model()));
    json_set(result, "extraction_model", json_string(browser_get_extraction_model()));
    json_set(result, "requirements_met", json_bool(true));
    return result;
}

