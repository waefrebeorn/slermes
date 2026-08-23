/*
 * control_socket.h — C11 port of gateway/control_socket.py
 *
 * Gateway-owned local coordination surface (#92091): a local-only unix
 * socket the gateway binds at startup and removes on clean shutdown,
 * answering versioned JSON verbs (identify/status). One request per
 * connection: a single JSON line in, a single JSON line out, then close.
 * Never a TCP port — filesystem ACLs are the auth boundary.
 */
#ifndef SLERMES_CONTROL_SOCKET_H
#define SLERMES_CONTROL_SOCKET_H

#include "hermes_json.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CONTROL_PROTOCOL_VERSION 1
#define CONTROL_SOCKET_MAX_UNIX_PATH 100
#define CONTROL_SOCKET_MAX_REQUEST_BYTES (64 * 1024)
#define CONTROL_SOCKET_MAX_RESPONSE_BYTES (512 * 1024)
#define CONTROL_SOCKET_DEFAULT_CLIENT_TIMEOUT 2.0

/* Verb handler: returns a malloc'd-ownership json_t* result payload. */
typedef json_t *(*control_socket_verb_fn)(void *ctx);

typedef struct {
    void *ctx;
    control_socket_verb_fn identify; /* NULL → default payload builder */
    control_socket_verb_fn status;   /* NULL → default payload builder */
} control_socket_handlers_t;

/* _home_hash @ gateway/control_socket.py:_home_hash — sha256[:16] of the
 * normcased resolved home path. malloc'd hex, caller frees. */
char *control_socket_home_hash(const char *home);

/* windows_pipe_name @ gateway/control_socket.py:windows_pipe_name */
char *control_socket_windows_pipe_name(const char *home);

/* _pointer_path / _default_socket_path / _fallback_socket_path — malloc'd. */
char *control_socket_pointer_path(const char *home);
char *control_socket_default_socket_path(const char *home);
char *control_socket_fallback_socket_path(const char *home);

/* resolve_server_socket_path @ gateway/control_socket.py:resolve_server_socket_path
 * Returns malloc'd bind path; *out_pointer_file set to malloc'd pointer-file
 * path only when the temp-dir fallback is in use, else NULL. */
char *control_socket_resolve_server_socket_path(const char *home,
                                                char **out_pointer_file);

/* resolve_client_socket_path @ ...:resolve_client_socket_path — malloc'd
 * path or NULL when neither the direct socket nor the pointer target exists. */
char *control_socket_resolve_client_socket_path(const char *home);

/* _detect_supervisor @ ...:_detect_supervisor — from THIS process's env/argv. */
const char *control_socket_detect_supervisor(void);

/* build_identify_payload @ ...:build_identify_payload — caller frees. */
json_t *control_socket_build_identify_payload(void);

/* build_status_payload @ ...:build_status_payload — caller frees. */
json_t *control_socket_build_status_payload(void);

/* GatewayControlServer — thread-backed POSIX unix-socket server. */
typedef struct control_socket_server control_socket_server_t;

/* GatewayControlServer(home) — handlers may be NULL for defaults. */
control_socket_server_t *control_socket_server_new(
    const char *home, const control_socket_handlers_t *handlers);

/* start() — bind + spawn the accept loop on a daemon thread. Non-fatal on
 * failure (returns 0); consumers fall back to the scan layer. */
int control_socket_server_start(control_socket_server_t *srv);

/* stop() — close listener, join thread, remove socket + pointer files. */
void control_socket_server_stop(control_socket_server_t *srv);

/* cleanup_files() @ ...:cleanup_files — atexit-safe best-effort unlink. */
void control_socket_server_cleanup_files(control_socket_server_t *srv);
void control_socket_server_free(control_socket_server_t *srv);

/* handle_request_line @ ...:handle_request_line — one JSON request line in,
 * one JSON response line out (malloc'd, includes trailing '\n'). Never
 * raises; shared by the stream handler and pipe protocol. */
char *control_socket_handle_request_line(control_socket_server_t *srv,
                                         const char *raw, size_t raw_len);

/* query_gateway_control @ ...:query_gateway_control — synchronous client.
 * Returns the verb's result object (caller frees) or NULL on any failure. */
json_t *control_socket_query_gateway_control(const char *home, const char *verb,
                                             double timeout);

/* identify_gateway @ ...:identify_gateway */
json_t *control_socket_identify_gateway(const char *home, double timeout);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_CONTROL_SOCKET_H */
