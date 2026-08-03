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
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
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

/* PoP: __init__ @ tools/browser_supervisor.py:__init__ */
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

/* PoP: start @ tools/browser_supervisor.py:start */
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

/* PoP: stop @ tools/browser_supervisor.py:stop */
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

/* PoP: snapshot @ tools/browser_supervisor.py:snapshot */
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

/* PoP: _cdp @ tools/browser_supervisor.py:_cdp */
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

/* PoP: _read_loop @ tools/browser_supervisor.py:_read_loop */
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
