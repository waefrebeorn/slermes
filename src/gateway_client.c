/*
 * gateway_client.c — WebSocket client + JSON-RPC for gateway communication
 *
 * Connects to the Hermes gateway via WebSocket, sends JSON-RPC requests,
 * and handles streaming responses.
 *
 * PoP: gateway_connect      @ apps/shared/src/json-rpc-gateway.ts
 * PoP: gateway_disconnect   @ apps/shared/src/json-rpc-gateway.ts
 * PoP: gateway_send         @ apps/shared/src/json-rpc-gateway.ts
 * PoP: gateway_stream       @ apps/desktop/src/app/chat/index.tsx
 * PoP: gateway_is_connected @ apps/shared/src/json-rpc-gateway.ts
 * PoP: gateway_set_url      @ electron/connection-config.cjs
 */

#include "gateway_client.h"
#include "hermes.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/time.h>
#endif

/* ── Internal helpers ────────────────────────────────────────────────────── */

static int64_t now_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

static void gw_set_state(gw_client_t *gw, gw_state_t state, const char *error) {
    gw->state = state;
    if (gw->on_state)
        gw->on_state(state, error, gw->state_ctx);
}

static int gw_find_pending(gw_client_t *gw, int id) {
    for (int i = 0; i < GW_MAX_PENDING; i++) {
        if (gw->pending[i].active && gw->pending[i].id == id)
            return i;
    }
    return -1;
}

static int gw_alloc_pending(gw_client_t *gw) {
    for (int i = 0; i < GW_MAX_PENDING; i++) {
        if (!gw->pending[i].active) return i;
    }
    return -1;
}

/* Build a JSON-RPC request string */
static char *gw_build_request(int id, const char *method, const char *params_json) {
    char *req = malloc(GW_MAX_PARAMS + 512);
    if (!req) return NULL;

    if (!params_json || !params_json[0])
        params_json = "{}";

    snprintf(req, GW_MAX_PARAMS + 512,
             "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"%s\",\"params\":%s}",
             id, method, params_json);
    return req;
}

/* Simple JSON field extractor (no full parser needed for basic fields) */
static const char *json_find_field(const char *json, const char *field) {
    char key[128];
    snprintf(key, sizeof(key), "\"%s\"", field);
    const char *p = strstr(json, key);
    if (!p) return NULL;
    p += strlen(key);
    while (*p == ' ' || *p == '\t') p++;
    if (*p != ':') return NULL;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/* ── Lifecycle ───────────────────────────────────────────────────────────── */

/* PoP: gateway_connect @ apps/shared/src/json-rpc-gateway.ts */
gw_client_t *gw_client_create(const char *url, const char *token) {
    gw_client_t *gw = calloc(1, sizeof(gw_client_t));
    if (!gw) {
        fprintf(stderr, "gw_client_create: calloc failed");
        return NULL;
    }

    gw->ws = NULL;
    gw->state = GW_DISCONNECTED;
    gw->next_request_id = 1;
    gw->recv_len = 0;

    if (url) strncpy(gw->url, url, GW_MAX_URL - 1);
    if (token) strncpy(gw->token, token, GW_MAX_TOKEN - 1);

    /* Connect */
    if (gw->url[0]) {
        gw_set_state(gw, GW_CONNECTING, NULL);
        gw->ws = ws_connect(gw->url, 10);
        if (gw->ws) {
            gw_set_state(gw, GW_CONNECTED, NULL);
            fprintf(stderr, "gw_client: connected to %s", gw->url);
        } else {
            gw_set_state(gw, GW_ERROR, "Connection failed");
            fprintf(stderr, "gw_client: connection to %s failed", gw->url);
        }
    }

    return gw;
}

/* PoP: gateway_disconnect @ apps/shared/src/json-rpc-gateway.ts */
void gw_client_destroy(gw_client_t *gw) {
    if (!gw) return;

    if (gw->ws) {
        ws_close(gw->ws);
        gw->ws = NULL;
    }

    gw->state = GW_DISCONNECTED;
    free(gw);
}

/* ── Connection ──────────────────────────────────────────────────────────── */

/* PoP: gateway_is_connected @ apps/shared/src/json-rpc-gateway.ts */
bool gw_client_is_connected(const gw_client_t *gw) {
    return gw && gw->state == GW_CONNECTED && gw->ws != NULL;
}

gw_state_t gw_client_state(const gw_client_t *gw) {
    return gw ? gw->state : GW_DISCONNECTED;
}

/* PoP: gateway_set_url @ electron/connection-config.cjs */
bool gw_client_set_url(gw_client_t *gw, const char *url) {
    if (!gw || !url) return false;

    /* Disconnect if connected */
    if (gw->ws) {
        ws_close(gw->ws);
        gw->ws = NULL;
    }

    strncpy(gw->url, url, GW_MAX_URL - 1);
    gw->state = GW_DISCONNECTED;
    return true;
}

bool gw_client_reconnect(gw_client_t *gw) {
    if (!gw || !gw->url[0]) return false;

    if (gw->ws) {
        ws_close(gw->ws);
        gw->ws = NULL;
    }

    gw_set_state(gw, GW_CONNECTING, NULL);
    gw->ws = ws_connect(gw->url, 10);
    if (gw->ws) {
        gw_set_state(gw, GW_CONNECTED, NULL);
        return true;
    }

    gw_set_state(gw, GW_ERROR, "Reconnection failed");
    return false;
}

/* ── JSON-RPC ────────────────────────────────────────────────────────────── */

/* PoP: gateway_send @ apps/shared/src/json-rpc-gateway.ts */
int gw_client_request(gw_client_t *gw, const char *method, const char *params_json) {
    if (!gw || !gw->ws) return -1;

    int id = gw->next_request_id++;
    int slot = gw_alloc_pending(gw);
    if (slot < 0) {
        fprintf(stderr, "gw_client: no pending slots");
        return -1;
    }

    char *req = gw_build_request(id, method, params_json);
    if (!req) return -1;

    int len = (int)strlen(req);
    if (ws_send(gw->ws, WS_OP_TEXT, req, (size_t)len) < 0) {
        free(req);
        return -1;
    }

    gw->pending[slot].id = id;
    strncpy(gw->pending[slot].method, method, GW_MAX_METHOD - 1);
    if (params_json)
        strncpy(gw->pending[slot].params_json, params_json, GW_MAX_PARAMS - 1);
    gw->pending[slot].active = true;

    free(req);
    return id;
}

bool gw_client_notify(gw_client_t *gw, const char *method, const char *params_json) {
    if (!gw || !gw->ws) return false;

    /* Notification: id = 0 (no response expected) */
    char *req = gw_build_request(0, method, params_json);
    if (!req) return false;

    int len = (int)strlen(req);
    int ret = ws_send(gw->ws, WS_OP_TEXT, req, (size_t)len);
    free(req);
    return ret >= 0;
}

/* ── Streaming callbacks ─────────────────────────────────────────────────── */

/* PoP: gateway_stream @ apps/desktop/src/app/chat/index.tsx */
void gw_client_set_stream_callback(gw_client_t *gw, gw_stream_cb cb, void *ctx) {
    if (!gw) return;
    gw->on_stream = cb;
    gw->stream_ctx = ctx;
}

void gw_client_set_event_callback(gw_client_t *gw, gw_event_cb cb, void *ctx) {
    if (!gw) return;
    gw->on_event = cb;
    gw->event_ctx = ctx;
}

void gw_client_set_state_callback(gw_client_t *gw, gw_state_cb cb, void *ctx) {
    if (!gw) return;
    gw->on_state = cb;
    gw->state_ctx = ctx;
}

/* ── Receive loop ────────────────────────────────────────────────────────── */

int gw_client_poll(gw_client_t *gw, int timeout_ms) {
    if (!gw || !gw->ws) return -1;

    ws_frame_t frame;
    int ret = ws_recv(gw->ws, &frame, timeout_ms);
    if (ret <= 0) return ret;

    /* Process the received frame */
    if (frame.opcode == WS_OP_TEXT || frame.opcode == WS_OP_BIN) {
        /* Null-terminate */
        char *msg = malloc(frame.len + 1);
        memcpy(msg, frame.payload, frame.len);
        msg[frame.len] = '\0';

        /* Parse JSON-RPC response */
        const char *id_str = json_find_field(msg, "id");
        const char *method_str = json_find_field(msg, "method");
        const char *delta_str = json_find_field(msg, "delta");
        const char *session_str = json_find_field(msg, "session_id");
        const char *error_str = json_find_field(msg, "error");

        if (delta_str && gw->on_stream) {
            /* Streaming delta */
            char delta_buf[GW_MAX_RESPONSE];
            if (*delta_str == '"') {
                /* Extract string value */
                const char *end = strchr(delta_str + 1, '"');
                if (end) {
                    size_t dlen = (size_t)(end - delta_str - 1);
                    if (dlen < sizeof(delta_buf)) {
                        strncpy(delta_buf, delta_str + 1, dlen);
                        delta_buf[dlen] = '\0';
                        gw->on_stream(session_str ? "" : "", delta_buf, gw->stream_ctx);
                    }
                }
            }
        }

        if (error_str && gw->on_event) {
            gw->on_event("error", error_str, gw->event_ctx);
        }

        /* Mark pending request as complete */
        if (id_str) {
            int id = atoi(id_str);
            int slot = gw_find_pending(gw, id);
            if (slot >= 0) {
                gw->pending[slot].active = false;
            }
        }

        free(msg);
    } else if (frame.opcode == WS_OP_CLOSE) {
        gw_set_state(gw, GW_DISCONNECTED, "Connection closed");
        ws_frame_free(&frame);
        return -1;
    }

    ws_frame_free(&frame);
    return 1;
}

int gw_client_run_once(gw_client_t *gw, int timeout_ms) {
    return gw_client_poll(gw, timeout_ms);
}

/* ── Convenience helpers ─────────────────────────────────────────────────── */

char *gw_build_params(const char *keys_and_values, ...) {
    if (!keys_and_values) return strdup("{}");

    va_list ap;
    va_start(ap, keys_and_values);

    char *result = malloc(GW_MAX_PARAMS);
    if (!result) { va_end(ap); return NULL; }

    int pos = 0;
    pos += snprintf(result + pos, GW_MAX_PARAMS - pos, "{");

    const char *key = keys_and_values;
    int count = 0;

    while (key && *key) {
        const char *value = va_arg(ap, const char *);
        if (!value) value = "null";

        if (count > 0)
            pos += snprintf(result + pos, GW_MAX_PARAMS - pos, ",");

        /* Check if value looks like a number or boolean */
        if (strcmp(value, "true") == 0 || strcmp(value, "false") == 0 ||
            strcmp(value, "null") == 0 || (value[0] >= '0' && value[0] <= '9') ||
            value[0] == '-' || value[0] == '[' || value[0] == '{') {
            pos += snprintf(result + pos, GW_MAX_PARAMS - pos, "\"%s\":%s", key, value);
        } else {
            pos += snprintf(result + pos, GW_MAX_PARAMS - pos, "\"%s\":\"%s\"", key, value);
        }

        key = va_arg(ap, const char *);
        count++;
    }

    va_end(ap);
    pos += snprintf(result + pos, GW_MAX_PARAMS - pos, "}");
    return result;
}
