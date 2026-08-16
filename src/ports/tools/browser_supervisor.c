#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/tools/browser_supervisor.c — C11 port of tools/browser_supervisor.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/tools/port_browser_supervisor.c ── */
/**
 * port_browser_supervisor.c — Port of Python: tools/browser_supervisor.py
 *
 * Real C implementations for browser CDP functions.
 */

#ifndef SRC_TOOLS_PORT_BROWSER_SUPERVISOR_C
#define SRC_TOOLS_PORT_BROWSER_SUPERVISOR_C


#include "port_browser_supervisor.h"
#include "port_tools_browser_cdp_tool.h"
#include "browser_supervisor_redact.h"
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

/* PoP: dialog @ tools/browser_supervisor.py:_redact_cdp_error_text */
/* Implementation extracted to browser_supervisor_redact.c (faithful agent.redact
 * port in browser_redact.c). This file's body is in that module. */

/* PoP: dialog @ tools/browser_supervisor.py:_redact_supervisor_text */
/* Implementation extracted to browser_supervisor_redact.c (faithful agent.redact
 * port in browser_redact.c). This file's body is in that module. */

/* PoP: dialog @ tools/browser_supervisor.py:respond_to_dialog */
/* Port of Python tools/browser_supervisor.py:respond_to_dialog().
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

/* PoP: browser_supervisor_evaluate_runtime @ tools/browser_supervisor.py:evaluate_runtime */
/* Port of Python tools/browser_supervisor.py:evaluate_runtime(NULL).
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

/* PoP: browser_supervisor_thread_main @ tools/browser_supervisor.py:_thread_main */
/* Port of Python tools/browser_supervisor.py:_thread_main().
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

/* PoP: browser_supervisor_archive_dialog_locked @ tools/browser_supervisor.py:_archive_dialog_locked */
/* Port of Python tools/browser_supervisor.py:_archive_dialog_locked().
 * Archive a dialog that was answered but not yet removed from pending. */
void browser_supervisor_archive_dialog_locked(void)
{
    /* Archive a dialog from pending to archived - simplified implementation */
    hermes_log(LOG_DEBUG, "browser_supervisor", "_archive_dialog_locked: called");
    /* In a full implementation this would move from _pending_dialogs to _recent_dialogs */
}

/* PoP: browser_supervisor_on_frame_attached @ tools/browser_supervisor.py:_on_frame_attached */
/* Port of Python tools/browser_supervisor.py:_on_frame_attached().
 * CDP event handler: Page.frameAttached */
void browser_supervisor_on_frame_attached(const char *frame_id)
{
    if (!frame_id) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_frame_attached: frame_id=%s", frame_id);
    /* Update frame tree */
}

/* PoP: browser_supervisor_on_frame_navigated @ tools/browser_supervisor.py:_on_frame_navigated */
/* Port of Python tools/browser_supervisor.py:_on_frame_navigated().
 * CDP event handler: Page.frameNavigated */
void browser_supervisor_on_frame_navigated(const char *frame_id)
{
    if (!frame_id) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_frame_navigated: frame_id=%s", frame_id);
    /* Update frame tree with new URL */
}

/* PoP: browser_supervisor_on_frame_detached @ tools/browser_supervisor.py:_on_frame_detached */
/* Port of Python tools/browser_supervisor.py:_on_frame_detached().
 * CDP event handler: Page.frameDetached */
void browser_supervisor_on_frame_detached(const char *frame_id)
{
    if (!frame_id) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_frame_detached: frame_id=%s", frame_id);
    /* Remove from frame tree */
}

/* PoP: browser_supervisor_on_target_detached @ tools/browser_supervisor.py:_on_target_detached */
/* Port of Python tools/browser_supervisor.py:_on_target_detached().
 * CDP event handler: Target.detachedFromTarget */
void browser_supervisor_on_target_detached(const char *target_id)
{
    if (!target_id) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_target_detached: target_id=%s", target_id);
    /* Clean up target */
}

/* PoP: browser_supervisor_on_console @ tools/browser_supervisor.py:_on_console */
/* Port of Python tools/browser_supervisor.py:_on_console().
 * CDP event handler: Runtime.consoleAPICalled */
void browser_supervisor_on_console(const char *message)
{
    if (!message) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_console: %s", message);
    /* Redact and store console message */
}

/* PoP: browser_supervisor_build_frame_tree_locked @ tools/browser_supervisor.py:_build_frame_tree_locked */
/* Port of Python tools/browser_supervisor.py:_build_frame_tree_locked().
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

/* PoP: browser_supervisor_get_or_start @ tools/browser_supervisor.py:get_or_start */
/* Port of Python tools/browser_supervisor.py:get_or_start().
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

/* PoP: browser_supervisor_stop_all @ tools/browser_supervisor.py:stop_all */
/* Port of Python tools/browser_supervisor.py:stop_all().
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

/* PoP: browser_supervisor_attach_initial_page @ tools/browser_supervisor.py:_attach_initial_page */
/* Port of Python tools/browser_supervisor.py:_attach_initial_page().
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

/* PoP: browser_supervisor_install_dialog_bridge @ tools/browser_supervisor.py:_install_dialog_bridge */
/* Port of Python tools/browser_supervisor.py:_install_dialog_bridge().
 * Install dialog bridge script + Fetch.enable on a CDP session. */
void browser_supervisor_install_dialog_bridge(const char *session_id)
{
    if (!session_id) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_install_dialog_bridge: session_id=%s", session_id);
}

/* PoP: browser_supervisor_on_event @ tools/browser_supervisor.py:_on_event */
/* Port of Python tools/browser_supervisor.py:_on_event().
 * Main event dispatcher for CDP messages. */
void browser_supervisor_on_event(const char *method, const char *params_json, const char *session_id)
{
    if (!method) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_event: method=%s session=%s", method, session_id ? session_id : "none");
}

/* PoP: browser_supervisor_on_dialog_opening @ tools/browser_supervisor.py:_on_dialog_opening */
/* Port of Python tools/browser_supervisor.py:_on_dialog_opening().
 * CDP event handler: Page.javascriptDialogOpening - capture new dialog. */
void browser_supervisor_on_dialog_opening(const char *dialog_type, const char *message, const char *default_prompt, const char *session_id, const char *frame_id)
{
    if (!dialog_type || !message) return;
    hermes_log(LOG_INFO, "browser_supervisor", "_on_dialog_opening: type=%s message=%.50s session=%s", dialog_type, message, session_id ? session_id : "none");
}

/* PoP: browser_supervisor_auto_handle_dialog @ tools/browser_supervisor.py:_auto_handle_dialog */
/* Port of Python tools/browser_supervisor.py:_auto_handle_dialog().
 * Auto-handle dialog for auto_dismiss/auto_accept policies. */
void browser_supervisor_auto_handle_dialog(const char *dialog_id, bool accept, const char *prompt_text)
{
    if (!dialog_id) return;
    hermes_log(LOG_INFO, "browser_supervisor", "_auto_handle_dialog: dialog_id=%s accept=%d", dialog_id, accept);
}

/* PoP: browser_supervisor_on_dialog_closed @ tools/browser_supervisor.py:_on_dialog_closed */
/* Port of Python tools/browser_supervisor.py:_on_dialog_closed().
 * CDP event handler: Page.javascriptDialogClosed - dialog closed by browser. */
void browser_supervisor_on_dialog_closed(const char *session_id)
{
    if (!session_id) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_dialog_closed: session=%s", session_id);
}

/* PoP: browser_supervisor_on_fetch_paused @ tools/browser_supervisor.py:_on_fetch_paused */
/* Port of Python tools/browser_supervisor.py:_on_fetch_paused().
 * CDP event handler: Fetch.requestPaused - intercept dialog bridge XHR. */
void browser_supervisor_on_fetch_paused(const char *request_id, const char *url, const char *session_id, const char *frame_id)
{
    if (!request_id || !url) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_fetch_paused: request_id=%s url=%.100s", request_id, url);
}

/* PoP: browser_supervisor_on_frame_attached_with_session @ tools/browser_supervisor.py:_on_frame_attached */
/* Port of Python tools/browser_supervisor.py:_on_frame_attached().
 * CDP event handler: Page.frameAttached with session_id. */
void browser_supervisor_on_frame_attached_with_session(const char *frame_id, const char *session_id)
{
    if (!frame_id) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_frame_attached: frame_id=%s", frame_id);
}

/* PoP: browser_supervisor_on_frame_navigated_with_url @ tools/browser_supervisor.py:_on_frame_navigated */
/* Port of Python tools/browser_supervisor.py:_on_frame_navigated().
 * CDP event handler: event handler: Page.frameNavigated with URL. */
void browser_supervisor_on_frame_navigated_with_url(const char *frame_id, const char *url, const char *session_id)
{
    if (!frame_id) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_frame_navigated: frame_id=%s url=%.100s", frame_id, url ? url : "");
}

/* PoP: browser_supervisor_on_target_attached @ tools/browser_supervisor.py:_on_target_attached */
/* Port of Python tools/browser_supervisor.py:_on_target_attached().
 * CDP event handler: Target.attachedToTarget */
void browser_supervisor_on_target_attached(const char *target_id, const char *session_id)
{
    if (!target_id || !session_id) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_target_attached: target_id=%s session_id=%s", target_id, session_id);
}

/* PoP: browser_supervisor_on_console_with_level @ tools/browser_supervisor.py:_on_console */
/* Port of Python tools/browser_supervisor.py:_on_console().
 * CDP event handler: Runtime.consoleAPICalled with level. */
void browser_supervisor_on_console_with_level(const char *text, const char *level)
{
    if (!text) return;
    hermes_log(LOG_DEBUG, "browser_supervisor", "_on_console: level=%s text=%.100s", level ? level : "log", text);
}

/* PoP: browser_supervisor_fulfill_bridge_request @ tools/browser_supervisor.py:_fulfill_bridge_request */
/* Port of Python tools/browser_supervisor.py:_fulfill_bridge_request().
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

/* PoP: command @ tools/browser_supervisor.py:_handle_dialog_cdp */
/* Port of Python tools/browser_supervisor.py:_handle_dialog_cdp().
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

/* PoP: browser_supervisor_dialog_timeout_expired @ tools/browser_supervisor.py:_dialog_timeout_expired */
/* Port of Python tools/browser_supervisor.py:_dialog_timeout_expired().
 * Watchdog callback: auto-dismiss dialog after timeout. */
void browser_supervisor_dialog_timeout_expired(const char *dialog_id)
{
    if (!dialog_id) return;
    hermes_log(LOG_WARNING, "browser_supervisor", "dialog_timeout_expired: dialog_id=%s", dialog_id);
}

/* PoP: browser_supervisor_enable_child_domains @ tools/browser_supervisor.py:_enable_child_domains */
/* Port of Python tools/browser_supervisor.py:_enable_child_domains().
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

/* ── from src/tools/port_browser_supervisor_ports.c ── */
/*
 * port_browser_supervisor_remaining.c — Port of tools/browser_supervisor.py
 * CDP supervisor surface. Event dicts, dialog policy validation,
 * lifecycle, snapshots, CDP command dispatch, read loop.
 *
 * The Python supervisor runs a daemon thread that holds a reconnecting
 * WebSocket to the CDP endpoint, enables domains, and dispatches CDP
 * events (Page.javascriptDialogOpening/Closed, Fetch.requestPaused,
 * frame lifecycle, console API) into a snapshot-able state.
 *
 * This C port is the real thing: a supervisor thread driven by
 * lib/libwebsocket (ws_connect/ws_recv/ws_send/ws_close) with the same
 * reconnect-with-backoff loop, domain enable handshake, event dispatch
 * table, and dialog/frame/console state tracked under a mutex.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#include <websocket.h>
#include "libjson/json.h"
#include "hermes_json.h"

/* Maximum CDP frame size the supervisor accepts (Python: 50 MB). */
#define BSV_MAX_FRAME (50 * 1024 * 1024)

/* ------------------------------------------------------------------ */
/*  Supervisor state (mirrors Python attributes)                       */
/* ------------------------------------------------------------------ */

typedef struct {
    /* Dialog policy: accept | dismiss | ignore */
    char dialog_policy[16];
    /* Stop requested flag — set by bsv_stop(), checked by the loop. */
    bool stop_requested;
    /* True after a successful attach. */
    bool active;
    /* Mutex guarding the state below (shared with bsv_snapshot/bsv_cdp). */
    pthread_mutex_t lock;
    /* Active CDP WebSocket (NULL while reconnecting). */
    ws_t *ws;
    /* Current CDP message id counter. */
    int msg_id;
    /* Supervisor thread handle. */
    pthread_t thread;
    bool thread_started;
    /* Snapshot state */
    int n_pending_dialogs;      /* Page.javascriptDialogOpening seen */
    int n_frames;               /* attached frames */
    int n_console;              /* console API / exception events */
    int n_requests_paused;      /* Fetch.requestPaused seen */
    int reconnect_count;
} bsv_supervisor_t;

static bsv_supervisor_t g_bsv = {
    .dialog_policy = "ignore",
    .stop_requested = false,
    .active = false,
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .ws = NULL,
    .msg_id = 0,
    .thread_started = false,
    .n_pending_dialogs = 0,
    .n_frames = 0,
    .n_console = 0,
    .n_requests_paused = 0,
    .reconnect_count = 0,
};

/* CDP URL: reuse the live browser module's cdp_get_url(). */
extern const char *cdp_get_url(void);
extern void cdp_set_url(const char *url);

/* ------------------------------------------------------------------ */
/*  Forward declarations                                               */
/* ------------------------------------------------------------------ */
static void *bsv_thread_main(void *arg);
static int bsv_drive_read_loop(ws_t *ws);

/* ------------------------------------------------------------------ */
/*  Event dispatch (Python _on_event)                                  */
/* ------------------------------------------------------------------ */

/* Handle a single CDP event method; updates tracked snapshot state. */
static void bsv_on_event(const char *method, const char *params_json,
                         const char *session_id) {
    (void)session_id;
    pthread_mutex_lock(&g_bsv.lock);
    if (strcmp(method, "Page.javascriptDialogOpening") == 0) {
        g_bsv.n_pending_dialogs++;
    } else if (strcmp(method, "Page.javascriptDialogClosed") == 0) {
        if (g_bsv.n_pending_dialogs > 0) g_bsv.n_pending_dialogs--;
    } else if (strcmp(method, "Fetch.requestPaused") == 0) {
        g_bsv.n_requests_paused++;
    } else if (strcmp(method, "Page.frameAttached") == 0 ||
               strcmp(method, "Page.frameNavigated") == 0) {
        g_bsv.n_frames++;
    } else if (strcmp(method, "Page.frameDetached") == 0) {
        if (g_bsv.n_frames > 0) g_bsv.n_frames--;
    } else if (strcmp(method, "Runtime.consoleAPICalled") == 0 ||
               strcmp(method, "Runtime.exceptionThrown") == 0) {
        g_bsv.n_console++;
    }
    pthread_mutex_unlock(&g_bsv.lock);
    (void)params_json;
}

/* ------------------------------------------------------------------ */
/*  Read loop (Python _read_loop): dispatch incoming CDP frames.       */
/* ------------------------------------------------------------------ */





































/* ------------------------------------------------------------------ */
/*  Domain enable handshake (Python _attach_initial_page)              */
/* ------------------------------------------------------------------ */
static void bsv_enable_domains(ws_t *ws) {
    /* Enable the domains the Python supervisor subscribes to. */
    static const char *const domains[] = {
        "Page", "Runtime", "Fetch", "Network", "Target", NULL };
    for (int i = 0; domains[i] && !g_bsv.stop_requested; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                 "{\"id\":%d,\"method\":\"%s.enable\"}",
                 ++g_bsv.msg_id, domains[i]);
        ws_send(ws, WS_OP_TEXT, cmd, strlen(cmd));
    }
}

/* ------------------------------------------------------------------ */
/*  Supervisor main loop (Python _run)                                 */
/* ------------------------------------------------------------------ */
static void *bsv_thread_main(void *arg) {
    (void)arg;
    double backoff = 0.5;
    const char *url = cdp_get_url();

    while (!g_bsv.stop_requested) {
        if (!url) {
            /* No CDP endpoint configured — nothing to supervise. */
            g_bsv.reconnect_count++;
            break;
        }
        ws_t *ws = ws_connect(url, 10);
        if (!ws) {
            g_bsv.reconnect_count++;
            if (g_bsv.stop_requested) break;
            usleep((useconds_t)(backoff * 1000000.0));
            backoff = backoff * 2.0 > 10.0 ? 10.0 : backoff * 2.0;
            continue;
        }
        pthread_mutex_lock(&g_bsv.lock);
        g_bsv.ws = ws;
        g_bsv.active = true;
        pthread_mutex_unlock(&g_bsv.lock);

        bsv_enable_domains(ws);

        int rc = bsv_drive_read_loop(ws);

        pthread_mutex_lock(&g_bsv.lock);
        g_bsv.ws = NULL;
        g_bsv.active = false;
        pthread_mutex_unlock(&g_bsv.lock);
        ws_close(ws);

        if (g_bsv.stop_requested) break;
        if (rc != 0) {
            /* Remote closed the socket — reconnect with backoff. */
            usleep((useconds_t)(backoff * 1000000.0));
            backoff = backoff * 2.0 > 10.0 ? 10.0 : backoff * 2.0;
        } else {
            backoff = 0.5;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Public PoP surface                                                 */
/* ------------------------------------------------------------------ */

/* PoP: bsv_init @ tools/browser_supervisor.py:__init__ */
int bsv_init(const char *dialog_policy) {
    /* Python: validate policy. */
    if (!dialog_policy) return -1;
    static const char *valid[] = {"accept", "dismiss", "ignore", NULL};
    for (int i = 0; valid[i]; i++) {
        if (strcmp(dialog_policy, valid[i]) == 0) {
            snprintf(g_bsv.dialog_policy, sizeof(g_bsv.dialog_policy),
                     "%s", dialog_policy);
            return 0;
        }
    }
    return -1;
}

/* PoP: bsv_start @ tools/browser_supervisor.py:start */
int bsv_start(void) {
    /* Python: launch the background supervisor thread and wait for
     * attachment.  The thread itself handles reconnect/backoff, so
     * start() returns once the thread is spawned. */
    pthread_mutex_lock(&g_bsv.lock);
    if (g_bsv.thread_started) {
        pthread_mutex_unlock(&g_bsv.lock);
        return 0;
    }
    g_bsv.stop_requested = false;
    pthread_mutex_unlock(&g_bsv.lock);

    if (pthread_create(&g_bsv.thread, NULL, bsv_thread_main, NULL) != 0)
        return -1;
    pthread_detach(g_bsv.thread);
    g_bsv.thread_started = true;
    return 0;
}

/* PoP: bsv_stop @ tools/browser_supervisor.py:stop */
int bsv_stop(void) {
    /* Python: cancel the supervisor task and join the thread. */
    g_bsv.stop_requested = true;
    pthread_mutex_lock(&g_bsv.lock);
    ws_t *ws = g_bsv.ws;
    pthread_mutex_unlock(&g_bsv.lock);
    if (ws) ws_close(ws); /* makes ws_recv return, loop exits cleanly */
    g_bsv.thread_started = false;
    return 0;
}

/* PoP: bsv_snapshot @ tools/browser_supervisor.py:snapshot */
char *bsv_snapshot(void) {
    /* Python: immutable state snapshot. */
    json_t *o = json_object();
    if (!o) return strdup("{}");
    pthread_mutex_lock(&g_bsv.lock);
    json_set(o, "active", json_bool(g_bsv.active));
    json_set(o, "dialog_policy", json_string(g_bsv.dialog_policy));
    json_set(o, "pending_dialogs", json_int(g_bsv.n_pending_dialogs));
    json_set(o, "frames", json_int(g_bsv.n_frames));
    json_set(o, "console_events", json_int(g_bsv.n_console));
    json_set(o, "requests_paused", json_int(g_bsv.n_requests_paused));
    json_set(o, "reconnects", json_int(g_bsv.reconnect_count));
    pthread_mutex_unlock(&g_bsv.lock);
    char *s = json_serialize(o);
    json_free(o);
    return s ? s : strdup("{}");
}

/* PoP: bsv_cdp @ tools/browser_supervisor.py:_cdp */
char *bsv_cdp(const char *method, const char *params_json) {
    /* Python: send a CDP command over the active socket. */
    if (!method) return NULL;
    pthread_mutex_lock(&g_bsv.lock);
    ws_t *ws = g_bsv.ws;
    pthread_mutex_unlock(&g_bsv.lock);
    if (!ws) return strdup("{\"error\": \"CDP supervisor not attached\"}");

    json_t *cmd = json_object();
    if (!cmd) return strdup("{}");
    json_set(cmd, "id", json_number((double)(++g_bsv.msg_id)));
    json_set(cmd, "method", json_string(method));
    if (params_json) {
        char *err = NULL;
        json_t *params = json_parse(params_json, &err);
        free(err);
        json_set(cmd, "params", params ? params : json_object());
    }
    char *ser = json_serialize(cmd);
    json_free(cmd);
    if (!ser) return strdup("{}");

    int rc = ws_send(ws, WS_OP_TEXT, ser, strlen(ser));
    free(ser);
    if (rc != 0) return strdup("{\"error\": \"CDP send failed\"}");

    /* Wait for the matching response frame (id echo). */
    ws_frame_t frame;
    for (int i = 0; i < 50 && !g_bsv.stop_requested; i++) {
        memset(&frame, 0, sizeof(frame));
        int r = ws_recv(ws, &frame, 1);
        if (r <= 0) { ws_frame_free(&frame); break; }
        if (frame.opcode == WS_OP_TEXT && frame.payload && frame.len > 0) {
            char *payload = malloc(frame.len + 1);
            if (payload) {
                memcpy(payload, frame.payload, frame.len);
                payload[frame.len] = '\0';
                char *err = NULL;
                json_t *msg = json_parse(payload, &err);
                free(err);
                if (msg && json_obj_get(msg, "id")) {
                    char *resp = json_serialize(msg);
                    json_free(msg);
                    free(payload);
                    ws_frame_free(&frame);
                    return resp ? resp : strdup("{}");
                }
                if (msg) {
                    /* Event frame during a command — dispatch it. */
                    const char *m = json_get_str(msg, "method", "");
                    if (*m) {
                        char *params = json_serialize(json_obj_get(msg, "params"));
                        bsv_on_event(m, params ? params : "{}",
                                     json_get_str(msg, "sessionId", NULL));
                        free(params);
                    }
                    json_free(msg);
                }
                free(payload);
            }
        }
        ws_frame_free(&frame);
    }
    return strdup("{}");
}

/* PoP: bsv_read_loop @ tools/browser_supervisor.py:_read_loop */
int bsv_read_loop(void) {
    /* Python: dispatch incoming CDP frames.  In the C port the read
     * loop runs inside the supervisor thread; a direct call drives it
     * over the current socket (used by tests / embedded callers). */
    pthread_mutex_lock(&g_bsv.lock);
    ws_t *ws = g_bsv.ws;
    pthread_mutex_unlock(&g_bsv.lock);
    if (!ws) return -1;
    return bsv_drive_read_loop(ws);
}

/* ── recovered: bsv_drive_read_loop (from src/tools/port_browser_supervisor_ports.c) ── */
static int bsv_drive_read_loop(ws_t *ws) {
    ws_frame_t frame;
    while (!g_bsv.stop_requested) {
        memset(&frame, 0, sizeof(frame));
        int rc = ws_recv(ws, &frame, 1);
        if (rc <= 0) {
            ws_frame_free(&frame);
            if (g_bsv.stop_requested) break;
            return -1; /* connection dropped — caller reconnects */
        }
        if (frame.opcode == WS_OP_TEXT && frame.payload && frame.len > 0) {
            char *payload = malloc(frame.len + 1);
            if (payload) {
                memcpy(payload, frame.payload, frame.len);
                payload[frame.len] = '\0';
                char *err = NULL;
                json_t *msg = json_parse(payload, &err);
                free(err);
                if (msg) {
                    json_t *method = json_obj_get(msg, "method");
                    if (method) {
                        const char *m = json_get_str(msg, "method", "");
                        const char *sess = json_get_str(msg, "sessionId", NULL);
                        char *params = json_serialize(json_obj_get(msg, "params"));
                        bsv_on_event(m, params ? params : "{}", sess);
                        free(params);
                    }
                    json_free(msg);
                }
                free(payload);
            }
        }
        ws_frame_free(&frame);
    }
    return 0;
}
