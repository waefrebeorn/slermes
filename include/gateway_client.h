/*
 * gateway_client.h — WebSocket client + JSON-RPC for gateway communication
 *
 * Connects to the Hermes gateway via WebSocket, sends JSON-RPC requests,
 * and handles streaming responses (server-sent events over WS).
 *
 * PoP: gateway_connect        @ apps/shared/src/json-rpc-gateway.ts
 * PoP: gateway_disconnect     @ apps/shared/src/json-rpc-gateway.ts
 * PoP: gateway_send           @ apps/shared/src/json-rpc-gateway.ts
 * PoP: gateway_stream         @ apps/desktop/src/app/chat/index.tsx
 * PoP: gateway_is_connected   @ apps/shared/src/json-rpc-gateway.ts
 * PoP: gateway_set_url        @ electron/connection-config.cjs
 */

#ifndef GATEWAY_CLIENT_H
#define GATEWAY_CLIENT_H

#include "websocket.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Configuration ─────────────────────────────────────────────────────── */
#define GW_MAX_URL      1024
#define GW_MAX_TOKEN    2048
#define GW_MAX_METHOD   128
#define GW_MAX_PARAMS   4096
#define GW_MAX_RESPONSE 65536
#define GW_MAX_PENDING  128
#define GW_TIMEOUT_MS   30000

/* ── Connection state ──────────────────────────────────────────────────── */
typedef enum {
    GW_DISCONNECTED = 0,
    GW_CONNECTING,
    GW_CONNECTED,
    GW_ERROR,
} gw_state_t;

/* ── JSON-RPC request ──────────────────────────────────────────────────── */
typedef struct {
    int         id;
    char        method[GW_MAX_METHOD];
    char        params_json[GW_MAX_PARAMS];
    bool        active;
} gw_request_t;

/* ── Stream callback ────────────────────────────────────────────────────── */
typedef void (*gw_stream_cb)(const char *session_id, const char *delta, void *ctx);
typedef void (*gw_event_cb)(const char *event_type, const char *payload, void *ctx);
typedef void (*gw_state_cb)(gw_state_t state, const char *error, void *ctx);

/* ── Gateway client handle ─────────────────────────────────────────────── */
typedef struct {
    ws_t        *ws;
    char         url[GW_MAX_URL];
    char         token[GW_MAX_TOKEN];
    gw_state_t   state;
    int          next_request_id;

    /* Pending requests */
    gw_request_t pending[GW_MAX_PENDING];

    /* Callbacks */
    gw_stream_cb on_stream;
    void        *stream_ctx;
    gw_event_cb  on_event;
    void        *event_ctx;
    gw_state_cb  on_state;
    void        *state_ctx;

    /* Receive buffer */
    char         recv_buf[GW_MAX_RESPONSE];
    int          recv_len;
} gw_client_t;

/* ── Lifecycle ───────────────────────────────────────────────────────────── */

/* PoP: gateway_connect @ apps/shared/src/json-rpc-gateway.ts */
/* Create a gateway client and connect to the given URL.
 * url: ws:// or wss:// URL of the Hermes gateway.
 * token: auth token (may be empty for local connections).
 * Returns NULL on failure. */
gw_client_t *gw_client_create(const char *url, const char *token);

/* PoP: gateway_disconnect @ apps/shared/src/json-rpc-gateway.ts */
/* Disconnect and free the client. */
void gw_client_destroy(gw_client_t *gw);

/* ── Connection ──────────────────────────────────────────────────────────── */

/* PoP: gateway_is_connected @ apps/shared/src/json-rpc-gateway.ts */
bool gw_client_is_connected(const gw_client_t *gw);

gw_state_t gw_client_state(const gw_client_t *gw);

/* PoP: gateway_set_url @ electron/connection-config.cjs */
/* Update the gateway URL (disconnects if connected). */
bool gw_client_set_url(gw_client_t *gw, const char *url);

/* Reconnect using the stored URL. */
bool gw_client_reconnect(gw_client_t *gw);

/* ── JSON-RPC ────────────────────────────────────────────────────────────── */

/* PoP: gateway_send @ apps/shared/src/json-rpc-gateway.ts */
/* Send a JSON-RPC request. Returns request ID, or -1 on failure.
 * method: RPC method name (e.g. "session.create").
 * params_json: JSON object string for params (e.g. "{\"cols\":96}").
 * If params_json is NULL, sends empty params {}. */
int gw_client_request(gw_client_t *gw, const char *method, const char *params_json);

/* Send a JSON-RPC notification (no response expected). */
bool gw_client_notify(gw_client_t *gw, const char *method, const char *params_json);

/* ── Streaming ───────────────────────────────────────────────────────────── */

/* PoP: gateway_stream @ apps/desktop/src/app/chat/index.tsx */
/* Set the callback for streaming deltas (assistant text chunks).
 * session_id: the session being streamed.
 * delta: text delta (may be partial, caller should buffer).
 * ctx: user context pointer. */
void gw_client_set_stream_callback(gw_client_t *gw, gw_stream_cb cb, void *ctx);

/* Set the callback for gateway events (tool calls, errors, etc.). */
void gw_client_set_event_callback(gw_client_t *gw, gw_event_cb cb, void *ctx);

/* Set the callback for connection state changes. */
void gw_client_set_state_callback(gw_client_t *gw, gw_state_cb cb, void *ctx);

/* ── Receive loop ────────────────────────────────────────────────────────── */

/* Process one incoming message (non-blocking).
 * Returns: 1 = message processed, 0 = no data, -1 = error/disconnect.
 * Call this regularly (e.g. from the main event loop). */
int gw_client_poll(gw_client_t *gw, int timeout_ms);

/* Block and process messages until timeout. */
int gw_client_run_once(gw_client_t *gw, int timeout_ms);

/* ── Convenience helpers ─────────────────────────────────────────────────── */

/* Build a JSON params object from key-value string pairs.
 * keys_and_values: alternating key, value strings. NULL-terminated.
 * Example: gw_build_params("cols", "96", "rows", "24", NULL) */
char *gw_build_params(const char *keys_and_values, ...);

#ifdef __cplusplus
}
#endif

#endif /* GATEWAY_CLIENT_H */
