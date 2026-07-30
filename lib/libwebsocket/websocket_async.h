#ifndef WEBSOCKET_ASYNC_H
#define WEBSOCKET_ASYNC_H

/*
 * websocket_async.h — Asynchronous WebSocket event loop for C.
 *
 * Poll-based multiplexing layer over the synchronous libwebsocket.
 * Mirrors Python's asyncio event loop with connect/message/disconnect callbacks.
 *
 * Usage:
 *   ws_async_t *ev = ws_async_create(100);  // max 100 connections
 *   ws_async_set_server_cb(ev, on_connect, NULL);
 *   ws_async_set_message_cb(ev, on_message, NULL);
 *   ws_async_set_disconnect_cb(ev, on_disconnect, NULL);
 *   ws_async_add_server(ev, server);
 *   while (running) {
 *       ws_async_poll(ev, 50);  // 50ms timeout, returns >0 if events fired
 *   }
 *   ws_async_destroy(ev);
 */

#include <stdbool.h>
#include <stddef.h>
#include "websocket.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Callback types ── */

/* Called when a new WebSocket client connects via the server.
 * ws: the new connection handle.
 * userdata: opaque pointer from registration.
 */
typedef void (*ws_async_connect_cb_t)(ws_t *ws, void *userdata);

/* Called when a message (text/binary) arrives on a connection.
 * ws: the connection that received the message.
 * frame: the received frame (caller must ws_frame_free() after callback returns).
 * userdata: opaque pointer from registration.
 */
typedef void (*ws_async_message_cb_t)(ws_t *ws, const ws_frame_t *frame, void *userdata);

/* Called when a WebSocket connection is closed or errors.
 * ws: the connection that disconnected.
 * userdata: opaque pointer from registration.
 */
typedef void (*ws_async_disconnect_cb_t)(ws_t *ws, void *userdata);

/* ── Async event loop handle (opaque) ── */
typedef struct ws_async_t ws_async_t;

/* ── API ── */

/* Create an async event loop with capacity for max_connections.
 * Returns NULL on OOM.
 * Python equivalent: asyncio.get_event_loop()
 */
ws_async_t *ws_async_create(int max_connections);

/* Destroy the event loop, close all connections, free resources.
 * Python equivalent: loop.close()
 */
void ws_async_destroy(ws_async_t *ev);

/* Register connect callback (fired when a new client connects via a server).
 * Python equivalent: server.on("connect", handler)
 */
void ws_async_set_connect_cb(ws_async_t *ev, ws_async_connect_cb_t cb, void *userdata);

/* Register message callback (fired when data arrives on any connection).
 * Python equivalent: ws.on("message", handler)
 */
void ws_async_set_message_cb(ws_async_t *ev, ws_async_message_cb_t cb, void *userdata);

/* Register disconnect callback (fired when a connection drops).
 * Python equivalent: ws.on("close", handler)
 */
void ws_async_set_disconnect_cb(ws_async_t *ev, ws_async_disconnect_cb_t cb, void *userdata);

/* Add a WebSocket server to the event loop.
 * The server's listen fd is polled; when a new connection arrives,
 * ws_server_accept() is called and the connect callback fires.
 * Python equivalent: server.start() / loop.create_server()
 * Returns true on success.
 */
bool ws_async_add_server(ws_async_t *ev, ws_server_t *server);

/* Remove a server from the event loop.
 * Does NOT close the server (caller must ws_server_close() it).
 */
void ws_async_remove_server(ws_async_t *ev, ws_server_t *server);

/* Add an existing WebSocket connection to the event loop.
 * The connection's fd is polled; messages trigger the message callback.
 * Python equivalent: loop.add_reader(fd, callback)
 * Returns true on success.
 */
bool ws_async_add_connection(ws_async_t *ev, ws_t *ws);

/* Remove a connection from the event loop without closing it.
 */
void ws_async_remove_connection(ws_async_t *ev, ws_t *ws);

/* Poll the event loop once.
 * timeout_ms: max milliseconds to wait (0 = non-blocking, -1 = indefinite).
 * Returns: number of events handled, 0 on timeout, -1 on error.
 * Python equivalent: loop.run_once()
 */
int ws_async_poll(ws_async_t *ev, int timeout_ms);

/* Get the number of active connections tracked by the event loop. */
int ws_async_connection_count(const ws_async_t *ev);

/* Check if the event loop has any servers registered. */
bool ws_async_has_servers(const ws_async_t *ev);

#ifdef __cplusplus
}
#endif

#endif /* WEBSOCKET_ASYNC_H */
