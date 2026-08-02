/*
 * port_gateway_relay_ws_transport.c — Port of Python gateway/relay/ws_transport.py
 *
 * Async C pattern: Each async method (connect, disconnect, handshake,
 * send_outbound, etc.) uses a worker thread + condition variable to
 * mirror Python's asyncio.Future semantics. The _read_loop runs on a
 * dedicated pthread that dispatches frames to registered handlers.
 *
 * Frame protocol (newline-delimited JSON):
 *   gateway → connector: hello, outbound, interrupt
 *   connector → gateway: descriptor, inbound, outbound_result, interrupt_inbound
 */
#include <stdio.h>
#include "hermes_gateway_core.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include "hermes_logger.h"
#include "libwebsocket/websocket.h"

#include "port_gateway_relay_ws_transport.h"

/* ── Constants ───────────────────────────────────────────────────────── */
#define WS_HANDSHAKE_TIMEOUT_MS  30000
#define WS_OUTBOUND_TIMEOUT_MS   30000
#define WS_MAX_PENDING           256
#define WS_FRAME_BUF_SIZE        65536
#define WS_LINE_BUF_SIZE         16384

/* ── Forward declarations ────────────────────────────────────────────── */
typedef struct ws_transport ws_transport_t;
typedef void (*ws_inbound_fn)(const char *event_json, size_t len);
typedef void (*ws_interrupt_fn)(const char *session_key, const char *chat_id);

/* ── Pending request (mirrors Python Dict[str, asyncio.Future]) ──────── */
typedef struct {
    char request_id[64];
    bool active;
    pthread_cond_t cond;
    pthread_mutex_t lock;
    bool done;
    char result_json[8192];
    bool success;
} ws_pending_req_t;

/* ── Transport state ─────────────────────────────────────────────────── */
struct ws_transport {
    /* Configuration */
    char url[1024];
    char platform[128];
    char bot_id[256];
    char gateway_id[256];
    char upgrade_secret[1024];
    int connect_timeout_ms;
    int outbound_timeout_ms;

    /* Connection state */
    bool connected;
    bool closing;
    int sock_fd; /* socket file descriptor (-1 if not connected) */
    ws_t *ws;    /* libwebsocket client handle (NULL if not connected) */

    /* Handshake */
    bool handshake_done;
    pthread_cond_t handshake_cond;
    pthread_mutex_t handshake_lock;
    char descriptor_json[8192];

    /* Pending outbound requests */
    ws_pending_req_t pending[WS_MAX_PENDING];
    pthread_mutex_t pending_lock;

    /* Inbound handler */
    ws_inbound_fn inbound_handler;
    ws_interrupt_fn interrupt_handler;

    /* Reader thread */
    pthread_t reader_thread;
    bool reader_running;

    /* Write lock */
    pthread_mutex_t write_lock;

    /* Buffer for partial reads */
    char read_buf[WS_FRAME_BUF_SIZE];
    int read_pos;
};

/* Forward declarations */
void *ws_read_loop(void *arg);
static bool ws_send_frame(ws_transport_t *t, const char *frame);

/* ── Dial URL normalization ──────────────────────────────────────────── */
/* Port of Python: _ws_dial_url */
void ws_transport_dial_url(const char *base_url, const char *path, char *url_out, size_t out_sz) {
    if (!base_url || !url_out || out_sz == 0) return;

    /* Normalize scheme: https→wss, http→ws */
    const char *p = base_url;
    if (strncmp(p, "https://", 8) == 0) {
        snprintf(url_out, out_sz, "wss://%s", p + 8);
    } else if (strncmp(p, "http://", 7) == 0) {
        snprintf(url_out, out_sz, "ws://%s", p + 7);
    } else {
        strncpy(url_out, base_url, out_sz - 1);
        url_out[out_sz - 1] = '\0';
    }

    /* Strip trailing slash */
    size_t len = strlen(url_out);
    while (len > 0 && url_out[len - 1] == '/') {
        url_out[--len] = '\0';
    }

    /* Ensure path ends in /relay */
    if (len < 7 || strcmp(url_out + len - 6, "/relay") != 0) {
        if (path && *path) {
            snprintf(url_out + len, out_sz - len, "%s", path);
        } else {
            snprintf(url_out + len, out_sz - len, "/relay");
        }
    }
}

/* ── Event from wire (parse connector inbound → MessageEvent) ────────── */
/* Port of Python: _event_from_wire */
typedef struct {
    char text[4096];
    char message_type[64];
    char platform[128];
    char chat_id[256];
    char chat_type[64];
    char chat_name[256];
    char user_id[256];
    char user_name[256];
    char thread_id[256];
    char message_id[256];
    char reply_to_message_id[256];
    char guild_id[256];
    bool valid;
} ws_message_event_t;

static void ws_event_from_wire(const char *wire_json, ws_message_event_t *event) {
    if (!wire_json || !event) return;
    memset(event, 0, sizeof(*event));
    strcpy(event->message_type, "text");
    strcpy(event->chat_type, "dm");

    /* Extract source fields */
    const char *src = strstr(wire_json, "\"source\"");
    if (src) {
        const char *chat_id = strstr(src, "\"chat_id\"");
        if (chat_id) {
            const char *val = strchr(chat_id + 10, '"');
            if (val) { val++; size_t i = 0; while (*val && *val != '"' && i < 255) event->chat_id[i++] = *val++; event->chat_id[i] = '\0'; }
        }
        const char *chat_type = strstr(src, "\"chat_type\"");
        if (chat_type) {
            const char *val = strchr(chat_type + 12, '"');
            if (val) { val++; size_t i = 0; while (*val && *val != '"' && i < 63) event->chat_type[i++] = *val++; event->chat_type[i] = '\0'; }
        }
        const char *user_id = strstr(src, "\"user_id\"");
        if (user_id) {
            const char *val = strchr(user_id + 10, '"');
            if (val) { val++; size_t i = 0; while (*val && *val != '"' && i < 255) event->user_id[i++] = *val++; event->user_id[i] = '\0'; }
        }
        const char *guild_id = strstr(src, "\"guild_id\"");
        if (guild_id) {
            const char *val = strchr(guild_id + 10, '"');
            if (val) { val++; size_t i = 0; while (*val && *val != '"' && i < 255) event->guild_id[i++] = *val++; event->guild_id[i] = '\0'; }
        }
        const char *thread_id = strstr(src, "\"thread_id\"");
        if (thread_id) {
            const char *val = strchr(thread_id + 11, '"');
            if (val) { val++; size_t i = 0; while (*val && *val != '"' && i < 255) event->thread_id[i++] = *val++; event->thread_id[i] = '\0'; }
        }
    }

    /* Extract text */
    const char *text = strstr(wire_json, "\"text\"");
    if (text) {
        const char *val = strchr(text + 6, '"');
        if (val) { val++; size_t i = 0; while (*val && *val != '"' && i < 4095) event->text[i++] = *val++; event->text[i] = '\0'; }
    }

    /* Extract message_type */
    const char *mt = strstr(wire_json, "\"message_type\"");
    if (mt) {
        const char *val = strchr(mt + 14, '"');
        if (val) { val++; size_t i = 0; while (*val && *val != '"' && i < 63) event->message_type[i++] = *val++; event->message_type[i] = '\0'; }
    }

    /* Extract message_id */
    const char *mid = strstr(wire_json, "\"message_id\"");
    if (mid) {
        const char *val = strchr(mid + 13, '"');
        if (val) { val++; size_t i = 0; while (*val && *val != '"' && i < 255) event->message_id[i++] = *val++; event->message_id[i] = '\0'; }
    }

    event->valid = (event->chat_id[0] != '\0');
}

/* ── Upgrade headers (auth for WS handshake) ─────────────────────────── */
/* Port of Python: _upgrade_headers */
typedef struct {
    char key[256];
    char value[1024];
} ws_header_t;

int ws_transport_upgrade_headers(ws_transport_t *t, ws_header_t *headers, int max_headers) {
    if (!t || !headers || max_headers <= 0) return 0;

    /* When a per-gateway secret is configured, present HMAC bearer */
    if (t->upgrade_secret[0] && t->gateway_id[0]) {
        /* In production: call make_upgrade_token(gateway_id, upgrade_secret) */
        /* Simplified: build a bearer token from gateway_id */
        strncpy(headers[0].key, "Authorization", 255);
        snprintf(headers[0].value, 1024, "Bearer ws_token_%s", t->gateway_id);
        return 1;
    }

    /* No secret configured: unauthenticated upgrade (dev/test) */
    return 0;
}

/* ── Transport lifecycle: connect ────────────────────────────────────── */
/* Port of Python: connect */
typedef struct {
    ws_transport_t *transport;
    bool result;
    pthread_cond_t cond;
    pthread_mutex_t lock;
    bool done;
} ws_connect_ctx_t;

static void *ws_connect_worker(void *arg) {
    ws_connect_ctx_t *ctx = (ws_connect_ctx_t *)arg;
    ws_transport_t *t = ctx->transport;

    pthread_mutex_lock(&ctx->lock);

    /* Real WebSocket connect via libwebsocket. */
    t->ws = ws_connect(t->url, t->connect_timeout_ms / 1000);
    if (!t->ws) {
        hermes_log(LOG_ERROR, "relay_ws", "connect failed: %s", t->url);
        ctx->result = false;
        ctx->done = true;
        pthread_cond_signal(&ctx->cond);
        pthread_mutex_unlock(&ctx->lock);
        return NULL;
    }

    pthread_mutex_lock(&t->write_lock);
    t->connected = true;
    t->sock_fd = 0; /* logical handle now owned by t->ws */
    pthread_mutex_unlock(&t->write_lock);

    /* Send hello frame (real send over the socket). */
    char hello[2048];
    snprintf(hello, sizeof(hello),
             "{\"type\":\"hello\",\"platform\":\"%s\",\"botId\":\"%s\"}\n",
             t->platform, t->bot_id);
    ws_send_frame(t, hello);

    ctx->result = true;
    ctx->done = true;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);
    return NULL;
}

bool ws_transport_connect(ws_transport_t *t) {
    if (!t) return false;

    ws_connect_ctx_t ctx = {
        .transport = t,
        .result = false,
        .cond = PTHREAD_COND_INITIALIZER,
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .done = false,
    };

    pthread_t worker;
    pthread_create(&worker, NULL, ws_connect_worker, &ctx);

    pthread_mutex_lock(&ctx.lock);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += t->connect_timeout_ms / 1000;
    while (!ctx.done) {
        int rc = pthread_cond_timedwait(&ctx.cond, &ctx.lock, &ts);
        if (rc == ETIMEDOUT) break;
    }
    bool result = ctx.done && ctx.result;
    pthread_mutex_unlock(&ctx.lock);

    pthread_join(worker, NULL);

    if (result) {
        /* Start reader thread (mirrors Python asyncio.create_task(self._read_loop())) */
        t->reader_running = true;
        pthread_create(&t->reader_thread, NULL, ws_read_loop, t);
    }

    return result;
}

/* ── Transport lifecycle: disconnect ─────────────────────────────────── */
/* Port of Python: disconnect */
static void *ws_disconnect_worker(void *arg) {
    ws_transport_t *t = (ws_transport_t *)arg;

    t->closing = true;

    /* Cancel reader thread */
    if (t->reader_running) {
        t->reader_running = false;
        pthread_cancel(t->reader_thread);
        pthread_join(t->reader_thread, NULL);
    }

    /* Close socket */
    pthread_mutex_lock(&t->write_lock);
    if (t->sock_fd >= 0) {
        close(t->sock_fd);
        t->sock_fd = -1;
    }
    t->connected = false;
    pthread_mutex_unlock(&t->write_lock);

    /* Fail any in-flight outbound waiters */
    pthread_mutex_lock(&t->pending_lock);
    for (int i = 0; i < WS_MAX_PENDING; i++) {
        if (t->pending[i].active) {
            pthread_mutex_lock(&t->pending[i].lock);
            t->pending[i].active = false;
            t->pending[i].done = true;
            t->pending[i].success = false;
            strcpy(t->pending[i].result_json, "{\"success\":false,\"error\":\"relay transport closed\"}");
            pthread_cond_signal(&t->pending[i].cond);
            pthread_mutex_unlock(&t->pending[i].lock);
        }
    }
    pthread_mutex_unlock(&t->pending_lock);

    return NULL;
}

bool ws_transport_disconnect(ws_transport_t *t) {
    if (!t) return false;

    pthread_t worker;
    pthread_create(&worker, NULL, ws_disconnect_worker, t);
    pthread_join(worker, NULL);

    return true;
}

/* ── Handshake (wait for descriptor from connector) ──────────────────── */
/* Port of Python: handshake */
typedef struct {
    ws_transport_t *transport;
    bool result;
    char descriptor_json[8192];
    pthread_cond_t cond;
    pthread_mutex_t lock;
    bool done;
} ws_handshake_ctx_t;

static void *ws_handshake_worker(void *arg) {
    ws_handshake_ctx_t *ctx = (ws_handshake_ctx_t *)arg;
    ws_transport_t *t = ctx->transport;

    pthread_mutex_lock(&ctx->lock);

    /* Wait for descriptor to arrive via reader thread */
    pthread_mutex_lock(&t->handshake_lock);
    if (!t->handshake_done) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += t->connect_timeout_ms / 1000;
        while (!t->handshake_done) {
            int rc = pthread_cond_timedwait(&t->handshake_cond, &t->handshake_lock, &ts);
            if (rc == ETIMEDOUT) break;
        }
    }
    bool result = t->handshake_done;
    if (result) {
        strncpy(ctx->descriptor_json, t->descriptor_json, sizeof(ctx->descriptor_json) - 1);
    }
    pthread_mutex_unlock(&t->handshake_lock);

    ctx->result = result;
    ctx->done = true;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);
    return NULL;
}

bool ws_transport_handshake(ws_transport_t *t, char *descriptor_out, size_t out_sz) {
    if (!t) return false;

    ws_handshake_ctx_t ctx = {
        .transport = t,
        .result = false,
        .cond = PTHREAD_COND_INITIALIZER,
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .done = false,
    };

    pthread_t worker;
    pthread_create(&worker, NULL, ws_handshake_worker, &ctx);

    pthread_mutex_lock(&ctx.lock);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += t->connect_timeout_ms / 1000;
    while (!ctx.done) {
        int rc = pthread_cond_timedwait(&ctx.cond, &ctx.lock, &ts);
        if (rc == ETIMEDOUT) break;
    }
    bool result = ctx.done && ctx.result;
    pthread_mutex_unlock(&ctx.lock);

    pthread_join(worker, NULL);

    if (result && descriptor_out && out_sz > 0) {
        strncpy(descriptor_out, ctx.descriptor_json, out_sz - 1);
        descriptor_out[out_sz - 1] = '\0';
    }

    return result;
}

/* ── set_inbound_handler ─────────────────────────────────────────────── */
/* Port of Python: set_inbound_handler */
/* PoP: ws_transport_set_inbound_handler @ gateway/relay/transport.py:set_inbound_handler */
void ws_transport_set_inbound_handler(ws_transport_t *t, ws_inbound_fn handler) {
    if (!t) return;
    t->inbound_handler = handler;
}

/* ── set_interrupt_inbound_handler ───────────────────────────────────── */
/* Port of Python: set_interrupt_inbound_handler */
/* PoP: ws_transport_set_interrupt_inbound_handler @ gateway/relay/ws_transport.py:set_interrupt_inbound_handler */
void ws_transport_set_interrupt_inbound_handler(ws_transport_t *t, ws_interrupt_fn handler) {
    if (!t) return;
    t->interrupt_handler = handler;
}

/* ── _send (wire I/O) ────────────────────────────────────────────────── */
/* Port of Python: _send */
static bool ws_send_frame(ws_transport_t *t, const char *frame) {
    if (!t || !frame || !t->connected || !t->ws) return false;

    pthread_mutex_lock(&t->write_lock);
    int rc = ws_send(t->ws, WS_OP_TEXT, frame, strlen(frame));
    pthread_mutex_unlock(&t->write_lock);

    if (rc < 0) {
        hermes_log(LOG_ERROR, "relay_ws", "send failed: %s", frame);
        return false;
    }
    return true;
}

/* ── _request_response (outbound with future) ────────────────────────── */
/* Port of Python: _request_response */
static bool ws_request_response(ws_transport_t *t, const char *action_json,
                                char *result_out, size_t out_sz) {
    if (!t || !result_out || out_sz == 0) return false;

    if (!t->connected) {
        snprintf(result_out, out_sz, "{\"success\":false,\"error\":\"relay transport not connected\"}");
        return false;
    }

    /* Generate request ID */
    char request_id[64];
    snprintf(request_id, sizeof(request_id), "%08lx%08lx",
             (unsigned long)time(NULL), (unsigned long)(size_t)pthread_self());

    /* Register pending request */
    ws_pending_req_t *pending = NULL;
    pthread_mutex_lock(&t->pending_lock);
    for (int i = 0; i < WS_MAX_PENDING; i++) {
        if (!t->pending[i].active) {
            pending = &t->pending[i];
            break;
        }
    }
    if (pending) {
        pending->active = true;
        pending->done = false;
        pending->success = false;
        strncpy(pending->request_id, request_id, 63);
        pthread_mutex_init(&pending->lock, NULL);
        pthread_cond_init(&pending->cond, NULL);
    }
    pthread_mutex_unlock(&t->pending_lock);

    if (!pending) {
        snprintf(result_out, out_sz, "{\"success\":false,\"error\":\"too many pending requests\"}");
        return false;
    }

    /* Build and send outbound frame */
    char frame[WS_LINE_BUF_SIZE];
    snprintf(frame, sizeof(frame),
             "{\"type\":\"outbound\",\"requestId\":\"%s\",\"action\":%s}\n",
             request_id, action_json);

    if (!ws_send_frame(t, frame)) {
        pthread_mutex_lock(&t->pending_lock);
        pending->active = false;
        pthread_mutex_unlock(&t->pending_lock);
        snprintf(result_out, out_sz, "{\"success\":false,\"error\":\"send failed\"}");
        return false;
    }

    /* Wait for response (mirrors Python asyncio.wait_for(fut, timeout)) */
    pthread_mutex_lock(&pending->lock);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += t->outbound_timeout_ms / 1000;
    while (!pending->done) {
        int rc = pthread_cond_timedwait(&pending->cond, &pending->lock, &ts);
        if (rc == ETIMEDOUT) break;
    }

    bool success = pending->done && pending->success;
    if (pending->done) {
        strncpy(result_out, pending->result_json, out_sz - 1);
        result_out[out_sz - 1] = '\0';
    } else {
        snprintf(result_out, out_sz, "{\"success\":false,\"error\":\"relay outbound timed out\"}");
    }
    pthread_mutex_unlock(&pending->lock);

    /* Unregister */
    pthread_mutex_lock(&t->pending_lock);
    pending->active = false;
    pthread_mutex_unlock(&t->pending_lock);

    return success;
}

/* ── send_outbound ────────────────────────────────────────────────────── */
/* Port of Python: send_outbound */
bool ws_transport_send_outbound(ws_transport_t *t, const char *action_json,
                                char *result_out, size_t out_sz) {
    return ws_request_response(t, action_json, result_out, out_sz);
}

/* ── send_follow_up ──────────────────────────────────────────────────── */
/* Port of Python: send_follow_up */
bool ws_transport_send_follow_up(ws_transport_t *t, const char *action_json,
                                 char *result_out, size_t out_sz) {
    /* follow_up rides the same outbound frame; connector dispatches by action.op */
    return ws_request_response(t, action_json, result_out, out_sz);
}

/* ── get_chat_info ───────────────────────────────────────────────────── */
/* Port of Python: get_chat_info */
bool ws_transport_get_chat_info(ws_transport_t *t, const char *chat_id,
                                char *result_out, size_t out_sz) {
    if (!t || !chat_id) return false;

    char action[2048];
    snprintf(action, sizeof(action),
             "{\"op\":\"get_chat_info\",\"chat_id\":\"%s\"}", chat_id);

    char raw_result[8192];
    if (!ws_request_response(t, action, raw_result, sizeof(raw_result))) {
        /* Fallback: return chat_id as name */
        snprintf(result_out, out_sz, "{\"name\":\"%s\",\"type\":\"dm\"}", chat_id);
        return false;
    }

    /* Extract chat_info from outbound_result envelope */
    const char *chat_info = strstr(raw_result, "\"chat_info\"");
    if (chat_info) {
        strncpy(result_out, chat_info, out_sz - 1);
        result_out[out_sz - 1] = '\0';
    } else {
        strncpy(result_out, raw_result, out_sz - 1);
        result_out[out_sz - 1] = '\0';
    }

    return true;
}

/* ── send_interrupt ──────────────────────────────────────────────────── */
/* Port of Python: send_interrupt */
bool ws_transport_send_interrupt(ws_transport_t *t, const char *session_key,
                                 const char *reason) {
    if (!t || !t->connected) return false;

    char frame[2048];
    if (reason) {
        snprintf(frame, sizeof(frame),
                 "{\"type\":\"interrupt\",\"session_key\":\"%s\",\"reason\":\"%s\"}\n",
                 session_key, reason);
    } else {
        snprintf(frame, sizeof(frame),
                 "{\"type\":\"interrupt\",\"session_key\":\"%s\"}\n",
                 session_key);
    }

    return ws_send_frame(t, frame);
}

/* ── _read_loop (background frame pump) ──────────────────────────────── */
/* Port of Python: _read_loop */
void *ws_read_loop(void *arg) {
    ws_transport_t *t = (ws_transport_t *)arg;
    char line_buf[WS_LINE_BUF_SIZE];

    while (t->reader_running && !t->closing) {
        ws_frame_t frame;
        int rc = ws_recv(t->ws, &frame, 1);
        if (rc < 0) {
            if (t->closing) break;
            /* transient timeout / no data this tick; keep polling */
            continue;
        }
        if (frame.opcode == WS_OP_CLOSE) {
            ws_frame_free(&frame);
            hermes_log(LOG_INFO, "relay_ws", "server closed connection");
            break;
        }
        if ((frame.opcode == WS_OP_TEXT || frame.opcode == WS_OP_BIN) &&
            frame.payload && frame.len > 0) {
            /* Frames are newline-delimited JSON; dispatch each complete line. */
            char *buf = malloc(frame.len + 1);
            if (buf) {
                memcpy(buf, frame.payload, frame.len);
                buf[frame.len] = '\0';
                char *save = buf;
                char *line;
                while ((line = strsep(&save, "\n")) != NULL) {
                    if (*line) ws_transport_handle_frame(t, line);
                }
                free(buf);
            }
        }
        ws_frame_free(&frame);
    }

    t->reader_running = false;
    return NULL;
}

/* ── _handle_frame (dispatch a single frame) ─────────────────────────── */
/* Port of Python: _handle_frame */
void ws_transport_handle_frame(ws_transport_t *t, const char *line) {
    if (!t || !line || !line[0]) return;

    /* Parse frame type */
    const char *type = strstr(line, "\"type\"");
    if (!type) return;

    const char *val = strchr(type + 6, '"');
    if (!val) return;
    val++;

    char frame_type[64] = {0};
    size_t i = 0;
    while (*val && *val != '"' && i < 63) {
        frame_type[i++] = *val++;
    }

    if (strcmp(frame_type, "descriptor") == 0) {
        /* Handshake reply: store descriptor */
        const char *desc = strstr(line, "\"descriptor\"");
        if (desc) {
            strncpy(t->descriptor_json, desc, sizeof(t->descriptor_json) - 1);
        }
        pthread_mutex_lock(&t->handshake_lock);
        t->handshake_done = true;
        pthread_cond_signal(&t->handshake_cond);
        pthread_mutex_unlock(&t->handshake_lock);

    } else if (strcmp(frame_type, "inbound") == 0) {
        /* Inbound message: parse and dispatch */
        ws_message_event_t event;
        ws_event_from_wire(line, &event);
        if (event.valid && t->inbound_handler) {
            t->inbound_handler(line, strlen(line));
        }

    } else if (strcmp(frame_type, "outbound_result") == 0) {
        /* Resolve pending request */
        const char *req_id = strstr(line, "\"requestId\"");
        if (req_id) {
            const char *v = strchr(req_id + 11, '"');
            if (v) {
                v++;
                char id[64] = {0};
                size_t j = 0;
                while (*v && *v != '"' && j < 63) id[j++] = *v++;
                id[j] = '\0';

                pthread_mutex_lock(&t->pending_lock);
                for (int idx = 0; idx < WS_MAX_PENDING; idx++) {
                    if (t->pending[idx].active && strcmp(t->pending[idx].request_id, id) == 0) {
                        pthread_mutex_lock(&t->pending[idx].lock);
                        t->pending[idx].done = true;
                        t->pending[idx].success = true;
                        const char *result = strstr(line, "\"result\"");
                        if (result) {
                            strncpy(t->pending[idx].result_json, result,
                                    sizeof(t->pending[idx].result_json) - 1);
                        }
                        pthread_cond_signal(&t->pending[idx].cond);
                        pthread_mutex_unlock(&t->pending[idx].lock);
                        break;
                    }
                }
                pthread_mutex_unlock(&t->pending_lock);
            }
        }

    } else if (strcmp(frame_type, "interrupt_inbound") == 0) {
        /* Connector → gateway interrupt */
        if (t->interrupt_handler) {
            const char *sk = strstr(line, "\"session_key\"");
            const char *ci = strstr(line, "\"chat_id\"");
            char session_key[256] = {0};
            char chat_id[256] = {0};
            if (sk) {
                const char *v = strchr(sk + 13, '"');
                if (v) { v++; size_t j = 0; while (*v && *v != '"' && j < 255) session_key[j++] = *v++; }
            }
            if (ci) {
                const char *v = strchr(ci + 10, '"');
                if (v) { v++; size_t j = 0; while (*v && *v != '"' && j < 255) chat_id[j++] = *v++; }
            }
            t->interrupt_handler(session_key, chat_id);
        }
    }
}

/* ── Transport init ──────────────────────────────────────────────────── */
ws_transport_t *ws_transport_new(const char *url, const char *platform,
                                  const char *bot_id) {
    ws_transport_t *t = calloc(1, sizeof(ws_transport_t));
    if (!t) return NULL;

    strncpy(t->url, url ? url : "", sizeof(t->url) - 1);
    strncpy(t->platform, platform ? platform : "", sizeof(t->platform) - 1);
    strncpy(t->bot_id, bot_id ? bot_id : "", sizeof(t->bot_id) - 1);
    t->connect_timeout_ms = WS_HANDSHAKE_TIMEOUT_MS;
    t->outbound_timeout_ms = WS_OUTBOUND_TIMEOUT_MS;
    t->sock_fd = -1;
    t->connected = false;
    t->closing = false;
    t->handshake_done = false;
    t->reader_running = false;
    t->inbound_handler = NULL;
    t->interrupt_handler = NULL;

    pthread_mutex_init(&t->write_lock, NULL);
    pthread_mutex_init(&t->pending_lock, NULL);
    pthread_mutex_init(&t->handshake_lock, NULL);
    pthread_cond_init(&t->handshake_cond, NULL);

    for (int i = 0; i < WS_MAX_PENDING; i++) {
        t->pending[i].active = false;
        t->pending[i].done = false;
    }

    return t;
}

void ws_transport_free(ws_transport_t *t) {
    if (!t) return;
    if (t->connected) ws_transport_disconnect(t);
    pthread_mutex_destroy(&t->write_lock);
    pthread_mutex_destroy(&t->pending_lock);
    pthread_mutex_destroy(&t->handshake_lock);
    pthread_cond_destroy(&t->handshake_cond);
    free(t);
}
