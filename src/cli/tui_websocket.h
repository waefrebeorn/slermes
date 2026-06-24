#ifndef TUI_WEBSOCKET_H
#define TUI_WEBSOCKET_H

/*
 * tui_websocket.h — TUI WebSocket server for remote TUI connections.
 *
 * Uses the async WebSocket event loop (websocket_async.c/h) for poll-based
 * multiplexing with connect/message/disconnect callbacks.
 *
 * Port of Python: tui_gateway.ws — WSTransport, handle_ws, async event loop.
 *
 * Usage:
 *   tui_ws_server_t *srv = tui_ws_start(8080, NULL, NULL);
 *   tui_ws_set_message_cb(srv, my_msg_cb, NULL);
 *   while (running) {
 *       tui_ws_poll(srv, 50);  // 50ms timeout, interleaves with ncurses
 *   }
 *   tui_ws_stop(srv);
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque server handle */
typedef struct tui_ws_server_t tui_ws_server_t;

/* Opaque client handle */
typedef struct tui_ws_client_t tui_ws_client_t;

/* Callback type for WS events.
 * client: the connection that triggered the event.
 * userdata: opaque pointer from registration.
 * For message events, opcode (1=text, 2=binary), data, and len are valid.
 * For connect/disconnect events, data is NULL and len is 0.
 */
typedef void (*tui_ws_callback_t)(tui_ws_client_t *client,
                                   void *userdata,
                                   int opcode,
                                   const char *data,
                                   int len);

/* ── API ── */

/* Start a WebSocket server with async event loop.
 * Port of Python: tui_gateway.ws.handle_ws
 * cert_path/key_path: NULL for ws://, file paths for wss://
 * Returns NULL on failure.
 */
tui_ws_server_t *tui_ws_start(int port,
                               const char *cert_path,
                               const char *key_path);

/* Register connect callback (fired on new client connection).
 * Port of Python: server.on("connect")
 */
void tui_ws_set_connect_cb(tui_ws_server_t *srv,
                            tui_ws_callback_t cb,
                            void *userdata);

/* Register message callback (fired on received message).
 * Port of Python: ws.on("message")
 */
void tui_ws_set_message_cb(tui_ws_server_t *srv,
                            tui_ws_callback_t cb,
                            void *userdata);

/* Register disconnect callback (fired on client disconnect).
 * Port of Python: ws.on("close")
 */
void tui_ws_set_disconnect_cb(tui_ws_server_t *srv,
                               tui_ws_callback_t cb,
                               void *userdata);

/* Poll the async event loop once.
 * timeout_ms: max milliseconds (0 = non-blocking, -1 = indefinite).
 * Returns events handled, 0 on timeout, -1 on error.
 * Port of Python: asyncio.run_once()
 */
int tui_ws_poll(tui_ws_server_t *srv, int timeout_ms);

/* Send a text frame to a client.
 * Port of Python: WSTransport.write
 */
bool tui_ws_send(tui_ws_client_t *client,
                  const char *data,
                  size_t len);

/* Get server port. */
int tui_ws_port(tui_ws_server_t *srv);

/* Stop server, close all connections, free resources.
 * Port of Python: ws.close()
 */
void tui_ws_stop(tui_ws_server_t *srv);

/* Close a client connection. */
void tui_ws_client_close(tui_ws_client_t *client);

/* Check if client is connected. */
bool tui_ws_is_connected(tui_ws_client_t *client);

/* Get client peer label (for logging). */
const char *tui_ws_peer_label(tui_ws_client_t *client);

#ifdef __cplusplus
}
#endif

#endif /* TUI_WEBSOCKET_H */
