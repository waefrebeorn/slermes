/*
 * tui_websocket.c — TUI WebSocket server for remote TUI connections.
 *
 * Uses the async WebSocket event loop (websocket_async.h) for callback-based
 * connection/message/disconnect handling. Mirrors Python's tui_gateway.ws
 * async transport with on_connect/on_message/on_disconnect callbacks.
 *
 * Port of Python: tui_gateway.ws — WSTransport, handle_ws, async event loop
 * C implementation: poll()-based async loop (websocket_async.c)
 *
 * MIT License — WuBu Hermes Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

#include "tui_websocket.h"
#include "tui_eventpub.h"
#include "../../lib/libwebsocket/websocket.h"
#include "../../lib/libwebsocket/websocket_async.h"

/* ── Server struct ── */
struct tui_ws_server_t {
    ws_server_t     *server;
    int              port;
    ws_async_t      *evloop;   /* Async event loop for multiplexing */
    tui_ws_callback_t on_message;
    tui_ws_callback_t on_connect;
    tui_ws_callback_t on_disconnect;
    void            *cb_data;
};

/* ── Client struct ── */
struct tui_ws_client_t {
    ws_t            *ws;
    bool             connected;
    char             peer_label[64];
};

/* ── Internal: create ws_t from server → wrap in client ── */
static tui_ws_client_t *client_from_ws(ws_t *ws) {
    if (!ws) return NULL;
    tui_ws_client_t *client = (tui_ws_client_t *)calloc(1, sizeof(tui_ws_client_t));
    if (!client) return NULL;
    client->ws = ws;
    client->connected = true;
    snprintf(client->peer_label, sizeof(client->peer_label), "ws:%d", ws_get_fd(ws));
    return client;
}

/* ── Callback: new connection accepted ── */
static void on_connect_cb(ws_t *ws, void *userdata) {
    tui_ws_server_t *srv = (tui_ws_server_t *)userdata;
    if (!srv || !srv->on_connect) return;

    tui_ws_client_t *client = client_from_ws(ws);
    if (!client) return;

    /* Fire connect callback with proper tui_ws_callback_t signature */
    srv->on_connect(client, srv->cb_data, 0, NULL, 0);
}

/* ── Callback: message received ── */
static void on_message_cb(ws_t *ws, const ws_frame_t *frame, void *userdata) {
    tui_ws_server_t *srv = (tui_ws_server_t *)userdata;
    if (!srv || !srv->on_message) return;

    /* Build a temporary client for the callback */
    tui_ws_client_t client;
    memset(&client, 0, sizeof(client));
    client.ws = ws;
    client.connected = true;
    snprintf(client.peer_label, sizeof(client.peer_label), "ws:%d", ws_get_fd(ws));

    /* Copy frame data into a buffer for the callback */
    char buf[65536];
    size_t copy = frame->len < sizeof(buf) - 1 ? frame->len : sizeof(buf) - 1;
    memcpy(buf, frame->payload, copy);
    buf[copy] = '\0';

    srv->on_message(&client, srv->cb_data, frame->opcode, buf, (int)copy);
}

/* ── Callback: connection closed ── */
static void on_disconnect_cb(ws_t *ws, void *userdata) {
    tui_ws_server_t *srv = (tui_ws_server_t *)userdata;
    if (!srv || !srv->on_disconnect) return;

    tui_ws_client_t client;
    memset(&client, 0, sizeof(client));
    client.ws = ws;
    client.connected = false;
    snprintf(client.peer_label, sizeof(client.peer_label), "ws:%d", ws_get_fd(ws));

    srv->on_disconnect(&client, srv->cb_data, 0, NULL, 0);

    /* Emit gateway disconnect event */
    tui_eventpub_connection(false, "ws_disconnect");
    tui_eventpub_flush();
}

/* ── API: Start server with async event loop ── */
/* Port of Python: tui_gateway.ws.handle_ws — async WS server startup */

tui_ws_server_t *tui_ws_start(int port, const char *cert_path, const char *key_path) {
    tui_ws_server_t *srv = (tui_ws_server_t *)calloc(1, sizeof(tui_ws_server_t));
    if (!srv) return NULL;

    srv->server = ws_server_listen(port, cert_path, key_path);
    if (!srv->server) {
        free(srv);
        return NULL;
    }

    srv->port = ws_server_port(srv->server);

    /* Create async event loop */
    srv->evloop = ws_async_create(32);
    if (!srv->evloop) {
        ws_server_close(srv->server);
        free(srv);
        return NULL;
    }

    /* Register callbacks */
    ws_async_set_connect_cb(srv->evloop, on_connect_cb, srv);
    ws_async_set_message_cb(srv->evloop, on_message_cb, srv);
    ws_async_set_disconnect_cb(srv->evloop, on_disconnect_cb, srv);

    /* Add server to event loop */
    ws_async_add_server(srv->evloop, srv->server);

    /* Emit gateway connect event */
    tui_eventpub_connection(true, "ws_started");
    tui_eventpub_flush();

    return srv;
}

/* Port of Python: tui_gateway.ws — register connect callback */
void tui_ws_set_connect_cb(tui_ws_server_t *srv, tui_ws_callback_t cb, void *userdata) {
    if (!srv) return;
    srv->on_connect = cb;
    srv->cb_data = userdata;
}

/* Port of Python: tui_gateway.ws — register message callback */
void tui_ws_set_message_cb(tui_ws_server_t *srv, tui_ws_callback_t cb, void *userdata) {
    if (!srv) return;
    srv->on_message = cb;
    srv->cb_data = userdata;
}

/* Port of Python: tui_gateway.ws — register disconnect callback */
void tui_ws_set_disconnect_cb(tui_ws_server_t *srv, tui_ws_callback_t cb, void *userdata) {
    if (!srv) return;
    srv->on_disconnect = cb;
    srv->cb_data = userdata;
}

/* Port of Python: asyncio.run_once() — non-blocking poll */
int tui_ws_poll(tui_ws_server_t *srv, int timeout_ms) {
    if (!srv || !srv->evloop) return 0;
    return ws_async_poll(srv->evloop, timeout_ms);
}

/* Port of Python: tui_gateway.ws.WSTransport.write — send text frame */
bool tui_ws_send(tui_ws_client_t *client, const char *data, size_t len) {
    if (!client || !client->ws || !client->connected) return false;
    int ret = ws_send(client->ws, WS_OP_TEXT, data, len);
    return ret == 0;
}

/* Port of Python: tui_gateway.ws — get port */
int tui_ws_port(tui_ws_server_t *srv) {
    return srv ? srv->port : -1;
}

/* Port of Python: tui_gateway.ws — stop server and free */
void tui_ws_stop(tui_ws_server_t *srv) {
    if (!srv) return;

    if (srv->evloop) {
        ws_async_destroy(srv->evloop);
        srv->evloop = NULL;
    }
    if (srv->server) {
        ws_server_close(srv->server);
        srv->server = NULL;
    }
    tui_eventpub_connection(false, "ws_stopped");
    tui_eventpub_flush();
    free(srv);
}

/* Port of Python: tui_gateway.ws.WSTransport — close client */
void tui_ws_client_close(tui_ws_client_t *client) {
    if (!client) return;
    if (client->ws) ws_close(client->ws);
    free(client);
}

/* Port of Python: tui_gateway.ws — connected check */
bool tui_ws_is_connected(tui_ws_client_t *client) {
    return client && client->connected;
}

/* Port of Python: tui_gateway.ws — client peer label */
const char *tui_ws_peer_label(tui_ws_client_t *client) {
    if (!client) return "unknown";
    return client->peer_label;
}
