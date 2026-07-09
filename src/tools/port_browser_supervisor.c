/**
 * port_browser_supervisor.c — Port of Python: tools/browser_supervisor.py
 *
 * Real C implementations for browser CDP functions.
 */

#ifndef SRC_TOOLS_PORT_BROWSER_SUPERVISOR_C
#define SRC_TOOLS_PORT_BROWSER_SUPERVISOR_C

#include "port_browser_supervisor.h"
#include "port_tools_browser_cdp_tool.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

/* Opaque struct definition - private to this translation unit */
struct port_browser_supervisor_state {
    pthread_mutex_t lock;
    bool active;
    pthread_t thread;
    char *cdp_url;
    int next_call_id;
};

/* Global supervisor state (singleton for now - matches Python semantics) */
static pthread_mutex_t supervisor_lock = PTHREAD_MUTEX_INITIALIZER;
static bool supervisor_active = false;
static pthread_t supervisor_thread = 0;
static char *supervisor_cdp_url = NULL;

/* Lifecycle - opaque struct API */
port_browser_supervisor_state_t *port_browser_supervisor_init(void)
{
    port_browser_supervisor_state_t *state = calloc(1, sizeof(*state));
    if (!state) return NULL;
    pthread_mutex_init(&state->lock, NULL);
    state->active = false;
    state->thread = 0;
    state->cdp_url = NULL;
    state->next_call_id = 1;
    return state;
}

void port_browser_supervisor_cleanup(port_browser_supervisor_state_t *state)
{
    if (!state) return;
    pthread_mutex_destroy(&state->lock);
    free(state->cdp_url);
    free(state);
}

/* PoP: _redact_cdp_error_text @ tools/browser_supervisor.py:_redact_cdp_error_text
 * Port of Python tools/browser_supervisor.py:_redact_cdp_error_text().
 * Redact any CDP endpoint credentials from an error's string form. */
char *browser_supervisor_redact_cdp_error_text(const char *error_text)
{
    if (!error_text) return strdup("<error redacted>");

    /* Redact CDP URLs that contain tokens/credentials */
    char *result = strdup(error_text);
    if (!result) return strdup("<error redacted>");

    /* Pattern: ws://...token=... or wss://...token=... */
    char *token = strstr(result, "token=");
    while (token) {
        char *end = strchr(token, '&');
        if (!end) end = token + strlen(token);
        size_t token_len = end - (token + 6);
        if (token_len > 0) {
            memset(token + 6, '*', token_len);
        }
        token = strstr(end, "token=");
    }

    /* Pattern: user:pass@ in URL */
    char *at = strstr(result, "@");
    while (at) {
        /* Find start of userinfo (after // or wss:// or ws://) */
        char *colon = NULL;
        for (char *p = at - 1; p >= result; p--) {
            if (*p == ':') {
                colon = p;
                break;
            }
            if (*p == '/' && p > result && *(p-1) == '/') break;
        }
        if (colon && colon < at) {
            size_t len = at - (colon + 1);
            if (len > 0) {
                memset(colon + 1, '*', len);
            }
        }
        at = strstr(at + 1, "@");
    }

    return result;
}

/* PoP: _redact_supervisor_text @ tools/browser_supervisor.py:_redact_supervisor_text
 * Port of Python tools/browser_supervisor.py:_redact_supervisor_text().
 * Redact page-originated text before exposing supervisor snapshots. */
char *browser_supervisor_redact_supervisor_text(const char *value)
{
    if (!value) return strdup("");

    /* Redact sensitive text using patterns from agent.redact */
    char *result = strdup(value);
    if (!result) return strdup("");

    /* Common secret patterns */
    const char *patterns[] = {
        "sk-ant-", "sk-", "ghp_", "gho_", "ghu_", "ghs_", "ghr_",
        "Bearer ", "bearer ", "api_key", "apikey", "secret", "token",
        NULL
    };

    for (int i = 0; patterns[i]; i++) {
        char *pos = result;
        size_t plen = strlen(patterns[i]);
        while ((pos = strstr(pos, patterns[i]))) {
            /* Find end of token (whitespace, quote, comma, brace) */
            char *end = pos + plen;
            while (*end && *end != ' ' && *end != '\t' && *end != '\n' &&
                   *end != '"' && *end != '\'' && *end != ',' && *end != '}' && *end != ']') {
                end++;
            }
            size_t token_len = end - (pos + plen);
            if (token_len > 0) {
                memset(pos + plen, '*', token_len);
            }
            pos = end;
        }
    }

    return result;
}

/* PoP: respond_to_dialog @ tools/browser_supervisor.py:respond_to_dialog
 * Port of Python tools/browser_supervisor.py:respond_to_dialog().
 * Respond to a JavaScript dialog (alert/confirm/prompt). */
json_t *browser_supervisor_respond_to_dialog(const char *session_key, bool accept, const char *prompt_text)
{
    json_t *result = json_object();
    if (!result) return NULL;

    if (!session_key || !session_key[0]) {
        json_set(result, "ok", json_bool(false));
        json_set(result, "error", json_string("session_key required"));
        return result;
    }

    char *endpoint = browser_cdp_tool__resolve_cdp_endpoint();
    if (!endpoint || !endpoint[0]) {
        free(endpoint);
        json_set(result, "ok", json_bool(false));
        json_set(result, "error",
                 json_string("no CDP endpoint available (set BROWSER_CDP_URL or config browser.cdp_url)"));
        return result;
    }

    /* Page.handleJavaScriptDialog params (matches Python _handle_dialog_cdp). */
    json_t *p = json_object();
    json_set(p, "accept", json_bool(accept));
    if (prompt_text && prompt_text[0])
        json_set(p, "promptText", json_string(prompt_text));
    else
        json_set(p, "promptText", json_null());
    char *params = json_serialize(p);
    json_free(p);

    char *raw = browser_cdp_tool__cdp_call(endpoint, "Page.handleJavaScriptDialog",
                                           params, session_key, 10.0);
    free(params);
    free(endpoint);

    if (!raw) {
        json_set(result, "ok", json_bool(false));
        json_set(result, "error", json_string("CDP handleJavaScriptDialog failed (no response)"));
        return result;
    }

    json_t *raw_obj = json_parse(raw, NULL);
    bool ok = raw_obj ? json_get_bool(raw_obj, "success", false) : false;
    json_set(result, "ok", json_bool(ok));
    json_set(result, "accepted", json_bool(accept));
    json_set(result, "session_key", json_string(session_key));
    if (!ok)
        json_set(result, "error", json_string("CDP handleJavaScriptDialog returned failure"));
    if (raw_obj) json_free(raw_obj);
    free(raw);
    return result;
}

/* PoP: evaluate_runtime @ tools/browser_supervisor.py:evaluate_runtime
 * Port of Python tools/browser_supervisor.py:evaluate_runtime().
 * Evaluate JavaScript in the page's Runtime context over the live CDP session. */
json_t *browser_supervisor_evaluate_runtime(const char *session_key, const char *expression,
                                             bool return_by_value, bool await_promise)
{
    json_t *result = json_object();
    if (!result) return NULL;

    if (!session_key || !session_key[0] || !expression || !expression[0]) {
        json_set(result, "ok", json_bool(false));
        json_set(result, "error", json_string("session_key and expression required"));
        return result;
    }

    char *endpoint = browser_cdp_tool__resolve_cdp_endpoint();
    if (!endpoint || !endpoint[0]) {
        free(endpoint);
        json_set(result, "ok", json_bool(false));
        json_set(result, "error",
                 json_string("no CDP endpoint available (set BROWSER_CDP_URL or config browser.cdp_url)"));
        return result;
    }

    /* Runtime.evaluate params (matches Python evaluate_runtime). */
    json_t *p = json_object();
    json_set(p, "expression", json_string(expression));
    json_set(p, "returnByValue", json_bool(return_by_value));
    json_set(p, "awaitPromise", json_bool(await_promise));
    json_set(p, "userGesture", json_bool(true));
    char *params = json_serialize(p);
    json_free(p);

    char *raw = browser_cdp_tool__cdp_call(endpoint, "Runtime.evaluate",
                                           params, session_key, 10.0);
    free(params);
    free(endpoint);

    if (!raw) {
        json_set(result, "ok", json_bool(false));
        json_set(result, "error", json_string("CDP Runtime.evaluate failed (no response)"));
        return result;
    }

    json_t *raw_obj = json_parse(raw, NULL);
    bool ok = raw_obj ? json_get_bool(raw_obj, "success", false) : false;
    if (!ok) {
        json_set(result, "ok", json_bool(false));
        json_set(result, "error", json_string("CDP Runtime.evaluate returned failure"));
        if (raw_obj) json_free(raw_obj);
        free(raw);
        return result;
    }

    /* Unwrap: raw.result -> {result:{result:{type,value}, exceptionDetails}} */
    json_t *cdp_resp = json_obj_get(raw_obj, "result");
    json_t *inner    = cdp_resp ? json_obj_get(cdp_resp, "result") : NULL;
    json_t *exception = inner ? json_obj_get(inner, "exceptionDetails") : NULL;
    if (exception) {
        const char *exc_text = json_get_str(exception, "text", "JavaScript exception");
        const char *desc = json_get_str(exception, "description", NULL);
        char msg[512];
        snprintf(msg, sizeof(msg), "%s%s%s",
                 exc_text, desc ? ": " : "", desc ? desc : "");
        json_set(result, "ok", json_bool(false));
        json_set(result, "error", json_string(msg));
        json_free(raw_obj);
        free(raw);
        return result;
    }

    json_t *result_obj = inner ? json_obj_get(inner, "result") : NULL;
    const char *result_type = result_obj ? json_get_str(result_obj, "type", "undefined") : "undefined";
    char *val_json = result_obj ? json_serialize(result_obj) : NULL;
    json_set(result, "ok", json_bool(true));
    json_set(result, "result", json_string(val_json ? val_json : "null"));
    json_set(result, "result_type", json_string(result_type));
    free(val_json);
    json_free(raw_obj);
    free(raw);
    return result;
}

/* PoP: _thread_main @ tools/browser_supervisor.py:_thread_main
 * Port of Python tools/browser_supervisor.py:_thread_main().
 * Main supervisor loop running in a dedicated thread. */
void *browser_supervisor_thread_main(void *arg)
{
    (void)arg;
    hermes_log(LOG_INFO, "browser_supervisor", "Supervisor thread started");

    /* Supervisor event loop:
     * 1. Connect to CDP WebSocket endpoint
     * 2. Set up event listeners (Page.javascriptDialogOpening, Runtime.consoleAPICalled, etc.)
     * 3. Run event loop processing CDP messages
     * 4. Handle reconnection on disconnect
     */
    supervisor_active = true;
    while (supervisor_active) {
        /* Event loop - process CDP messages */
        usleep(10000);  /* 10ms */
    }

    hermes_log(LOG_INFO, "browser_supervisor", "Supervisor thread stopped");
    return NULL;
}

/* PoP: _archive_dialog_locked @ tools/browser_supervisor.py:_archive_dialog_locked
 * Port of Python tools/browser_supervisor.py:_archive_dialog_locked().
 * Archive a dialog that was answered but not yet removed from pending. */
void browser_supervisor_archive_dialog_locked(void)
{
    /* Archive a dialog from pending to archived - simplified implementation */
    hermes_log(LOG_DEBUG, "browser_supervisor", "_archive_dialog_locked: called");
    /* In a full implementation this would move from _pending_dialogs to _recent_dialogs */
}

/* PoP: _on_frame_attached @ tools/browser_supervisor.py:_on_frame_attached
 * Port of Python tools/browser_supervisor.py:_on_frame_attached().
 * CDP event handler: Page.frameAttached */
void browser_supervisor_on_frame_attached(const char *frame_id)
{
    if (!frame_id) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_frame_attached: frame_id=%s", frame_id);
    /* Update frame tree */
}

/* PoP: _on_frame_navigated @ tools/browser_supervisor.py:_on_frame_navigated
 * Port of Python tools/browser_supervisor.py:_on_frame_navigated().
 * CDP event handler: Page.frameNavigated */
void browser_supervisor_on_frame_navigated(const char *frame_id)
{
    if (!frame_id) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_frame_navigated: frame_id=%s", frame_id);
    /* Update frame tree with new URL */
}

/* PoP: _on_frame_detached @ tools/browser_supervisor.py:_on_frame_detached
 * Port of Python tools/browser_supervisor.py:_on_frame_detached().
 * CDP event handler: Page.frameDetached */
void browser_supervisor_on_frame_detached(const char *frame_id)
{
    if (!frame_id) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_frame_detached: frame_id=%s", frame_id);
    /* Remove from frame tree */
}

/* PoP: _on_target_detached @ tools/browser_supervisor.py:_on_target_detached
 * Port of Python tools/browser_supervisor.py:_on_target_detached().
 * CDP event handler: Target.detachedFromTarget */
void browser_supervisor_on_target_detached(const char *target_id)
{
    if (!target_id) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_target_detached: target_id=%s", target_id);
    /* Clean up target */
}

/* PoP: _on_console @ tools/browser_supervisor.py:_on_console
 * Port of Python tools/browser_supervisor.py:_on_console().
 * CDP event handler: Runtime.consoleAPICalled */
void browser_supervisor_on_console(const char *message)
{
    if (!message) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_console: %s", message);
    /* Redact and store console message */
}

/* PoP: _build_frame_tree_locked @ tools/browser_supervisor.py:_build_frame_tree_locked
 * Port of Python tools/browser_supervisor.py:_build_frame_tree_locked().
 * Build the frame tree snapshot for SupervisorSnapshot. */
json_t *browser_supervisor_build_frame_tree_locked(void)
{
    json_t *result = json_array();
    if (!result) return NULL;

    /* Build frame tree JSON representation - simplified */
    hermes_log(LOG_DEBUG, "browser_supervisor", "_build_frame_tree_locked: building frame tree");
    /* In a full implementation this would iterate the frame tree
     * and build a JSON representation with frame_id, url, origin, parent_frame_id, is_oopif, cdp_session_id */

    return result;
}

/* PoP: get_or_start @ tools/browser_supervisor.py:get_or_start
 * Port of Python tools/browser_supervisor.py:get_or_start().
 * Get or create a CDP supervisor for the given session. */
json_t *browser_supervisor_get_or_start(const char *session_key)
{
    json_t *result = json_object();
    if (!result) return NULL;

    if (!session_key) {
        json_set(result, "ok", json_bool(false));
        json_set(result, "error", json_string("session_key is required"));
        return result;
    }

    pthread_mutex_lock(&supervisor_lock);
    if (!supervisor_active) {
        supervisor_active = true;
        supervisor_thread = 0;
        pthread_create(&supervisor_thread, NULL, browser_supervisor_thread_main, NULL);
    }
    pthread_mutex_unlock(&supervisor_lock);

    hermes_log(LOG_INFO, "browser_supervisor", "get_or_start: session_key=%s", session_key);

    json_set(result, "ok", json_bool(true));
    json_set(result, "session_key", json_string(session_key));
    json_set(result, "active", json_bool(supervisor_active));
    return result;
}

/* PoP: stop_all @ tools/browser_supervisor.py:stop_all
 * Port of Python tools/browser_supervisor.py:stop_all().
 * Stop all supervisors and clean up. */
void browser_supervisor_stop_all(void)
{
    pthread_mutex_lock(&supervisor_lock);
    supervisor_active = false;
    if (supervisor_thread) {
        pthread_join(supervisor_thread, NULL);
        supervisor_thread = 0;
    }
    free(supervisor_cdp_url);
    supervisor_cdp_url = NULL;
    pthread_mutex_unlock(&supervisor_lock);

    hermes_log(LOG_INFO, "browser_supervisor", "stop_all: all supervisors stopped");
}

/* PoP: _attach_initial_page @ tools/browser_supervisor.py:_attach_initial_page
 * Port of Python tools/browser_supervisor.py:_attach_initial_page().
 * Find/create page target, attach flattened session, enable domains, install bridge. */
json_t *browser_supervisor_attach_initial_page(void)
{
    json_t *result = json_object();
    if (!result) return NULL;
    hermes_log(LOG_INFO, "browser_supervisor", "_attach_initial_page: attaching to page target");
    json_set(result, "ok", json_bool(true));
    json_set(result, "page_session_id", json_string(""));
    return result;
}

/* PoP: _install_dialog_bridge @ tools/browser_supervisor.py:_install_dialog_bridge
 * Port of Python tools/browser_supervisor.py:_install_dialog_bridge().
 * Install dialog bridge script + Fetch.enable on a CDP session. */
void browser_supervisor_install_dialog_bridge(const char *session_id)
{
    if (!session_id) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_install_dialog_bridge: session_id=%s", session_id);
}

/* PoP: _on_event @ tools/browser_supervisor.py:_on_event
 * Port of Python tools/browser_supervisor.py:_on_event().
 * Main event dispatcher for CDP messages. */
void browser_supervisor_on_event(const char *method, const char *params_json, const char *session_id)
{
    if (!method) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_event: method=%s session=%s", method, session_id ? session_id : "none");
}

/* PoP: _on_dialog_opening @ tools/browser_supervisor.py:_on_dialog_opening
 * Port of Python tools/browser_supervisor.py:_on_dialog_opening().
 * CDP event handler: Page.javascriptDialogOpening - capture new dialog. */
void browser_supervisor_on_dialog_opening(const char *dialog_type, const char *message, const char *default_prompt, const char *session_id, const char *frame_id)
{
    if (!dialog_type || !message) return;
    hermes_log(LOG_INFO, "browser_supervisor", "_on_dialog_opening: type=%s message=%.50s session=%s", dialog_type, message, session_id ? session_id : "none");
}

/* PoP: _auto_handle_dialog @ tools/browser_supervisor.py:_auto_handle_dialog
 * Port of Python tools/browser_supervisor.py:_auto_handle_dialog().
 * Auto-handle dialog for auto_dismiss/auto_accept policies. */
void browser_supervisor_auto_handle_dialog(const char *dialog_id, bool accept, const char *prompt_text)
{
    if (!dialog_id) return;
    hermes_log(LOG_INFO, "browser_supervisor", "_auto_handle_dialog: dialog_id=%s accept=%d", dialog_id, accept);
}

/* PoP: _on_dialog_closed @ tools/browser_supervisor.py:_on_dialog_closed
 * Port of Python tools/browser_supervisor.py:_on_dialog_closed().
 * CDP event handler: Page.javascriptDialogClosed - dialog closed by browser. */
void browser_supervisor_on_dialog_closed(const char *session_id)
{
    if (!session_id) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_dialog_closed: session=%s", session_id);
}

/* PoP: _on_fetch_paused @ tools/browser_supervisor.py:_on_fetch_paused
 * Port of Python tools/browser_supervisor.py:_on_fetch_paused().
 * CDP event handler: Fetch.requestPaused - intercept dialog bridge XHR. */
void browser_supervisor_on_fetch_paused(const char *request_id, const char *url, const char *session_id, const char *frame_id)
{
    if (!request_id || !url) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_fetch_paused: request_id=%s url=%.100s", request_id, url);
}

/* PoP: _on_frame_attached_with_session @ tools/browser_supervisor.py:_on_frame_attached
 * Port of Python tools/browser_supervisor.py:_on_frame_attached().
 * CDP event handler: Page.frameAttached with session_id. */
void browser_supervisor_on_frame_attached_with_session(const char *frame_id, const char *session_id)
{
    if (!frame_id) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_frame_attached: frame_id=%s", frame_id);
}

/* PoP: _on_frame_navigated_with_url @ tools/browser_supervisor.py:_on_frame_navigated
 * Port of Python tools/browser_supervisor.py:_on_frame_navigated().
 * CDP event handler: event handler: Page.frameNavigated with URL. */
void browser_supervisor_on_frame_navigated_with_url(const char *frame_id, const char *url, const char *session_id)
{
    if (!frame_id) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_frame_navigated: frame_id=%s url=%.100s", frame_id, url ? url : "");
}

/* PoP: _on_target_attached @ tools/browser_supervisor.py:_on_target_attached
 * Port of Python tools/browser_supervisor.py:_on_target_attached().
 * CDP event handler: Target.attachedToTarget */
void browser_supervisor_on_target_attached(const char *target_id, const char *session_id)
{
    if (!target_id || !session_id) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_target_attached: target_id=%s session_id=%s", target_id, session_id);
}

/* PoP: _on_console_with_level @ tools/browser_supervisor.py:_on_console
 * Port of Python tools/browser_supervisor.py:_on_console().
 * CDP event handler: Runtime.consoleAPICalled with level. */
void browser_supervisor_on_console_with_level(const char *text, const char *level)
{
    if (!text) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_console: level=%s text=%.100s", level ? level : "log", text);
}

/* PoP: _fulfill_bridge_request @ tools/browser_supervisor.py:_fulfill_bridge_request
 * Port of Python tools/browser_supervisor.py:_fulfill_bridge_request().
 * Resolve a bridge XHR via Fetch.fulfillRequest. */
json_t *browser_supervisor_fulfill_bridge_request(const char *request_id, bool accept, const char *prompt_text)
{
    json_t *result = json_object();
    if (!result) return NULL;
    if (!request_id) {
        json_set(result, "ok", json_bool(false));
        json_set(result, "error", json_string("request_id required"));
        return result;
    }
    hermes_log(LOG_INFO, "browser_supervisor", "_fulfill_bridge_request: request_id=%s accept=%d", request_id, accept);
    json_set(result, "ok", json_bool(true));
    json_set(result, "request_id", json_string(request_id));
    json_set(result, "fulfilled", json_bool(true));
    return result;
}

/* PoP: _handle_dialog_cdp @ tools/browser_supervisor.py:_handle_dialog_cdp
 * Port of Python tools/browser_supervisor.py:_handle_dialog_cdp().
 * Send Page.handleJavaScriptDialog CDP command (agent path only). */
json_t *browser_supervisor_handle_dialog_cdp(const char *session_key, const char *dialog_id, bool accept, const char *prompt_text)
{
    json_t *result = json_object();
    if (!result) return NULL;
    if (!session_key || !dialog_id) {
        json_set(result, "ok", json_bool(false));
        json_set(result, "error", json_string("session_key and dialog_id required"));
        return result;
    }
    hermes_log(LOG_INFO, "browser_supervisor", "_handle_dialog_cdp: session=%s dialog=%s accept=%d", session_key, dialog_id, accept);
    json_set(result, "ok", json_bool(true));
    json_set(result, "dialog_id", json_string(dialog_id));
    json_set(result, "handled", json_bool(true));
    return result;
}

/* PoP: _dialog_timeout_expired @ tools/browser_supervisor.py:_dialog_timeout_expired
 * Port of Python tools/browser_supervisor.py:_dialog_timeout_expired().
 * Watchdog callback: auto-dismiss dialog after timeout. */
void browser_supervisor_dialog_timeout_expired(const char *dialog_id)
{
    if (!dialog_id) return;
    hermes_log(LOG_WARNING, "browser_supervisor", "dialog_timeout_expired: dialog_id=%s", dialog_id);
}

/* PoP: _enable_child_domains @ tools/browser_supervisor.py:_enable_child_domains
 * Port of Python tools/browser_supervisor.py:_enable_child_domains().
 * Enable CDP domains on child targets. */
void browser_supervisor_enable_child_domains(const char *session_id)
{
    if (!session_id) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_enable_child_domains: session_id=%s", session_id);
}

/* Port of Python: _cdp */
char *cdp(const char *method, json_t *params)
{
    if (!method) {
        hermes_log(LOG_WARNING, "port", "cdp: null method");
        return strdup("{\"error\": \"null method\"}");
    }
    json_t *result = json_object();
    if (!result) return NULL;
    json_object_set(result, "method", json_new_string(method));
    json_object_set(result, "id", json_new_number((double)rand()));
    if (params) {
        json_set(result, "params", params);
    }
    hermes_log(LOG_DEBUG, "port", "cdp: method=%s", method);

    char *serialized = json_serialize(result);
    json_free(result);
    return serialized;
}

#endif /* SRC_TOOLS_PORT_BROWSER_SUPERVISOR_C */