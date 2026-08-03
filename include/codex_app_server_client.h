/*
 * codex_app_server_client.h — JSON-RPC 2.0 client for `codex app-server`.
 *
 * Spawns `codex app-server` as a subprocess, speaks newline-delimited
 * JSON-RPC 2.0 over stdio. Threaded reader dispatches replies to
 * pending requests and routes notifications to a queue.
 *
 * Maps to Python agent/transports/codex_app_server.py (399 lines).
 */

#ifndef CODEX_APP_SERVER_CLIENT_H
#define CODEX_APP_SERVER_CLIENT_H

#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque client handle */
typedef struct codex_client_t codex_client_t;

/* Error codes */
#define CODEX_ERR_TIMEOUT   -1
#define CODEX_ERR_CLOSED    -2
#define CODEX_ERR_FORK      -3
#define CODEX_ERR_PIPE      -4
#define CODEX_ERR_EXEC      -5
#define CODEX_ERR_JSON      -6
#define CODEX_ERR_RPC       -7

/* Create a new client. Spawns `codex app-server` subprocess.
 * Returns NULL on failure (check errno). */
codex_client_t *codex_client_new(const char *codex_bin,
                                  const char *codex_home,
                                  const char **extra_args,
                                  int extra_args_count);

/* Get the lazily-created per-process client (creates on first call). */
codex_client_t *codex_client_get_active(void);

/* Lifecycle: initialize handshake. Returns 0 on success. */
int codex_client_initialize(codex_client_t *c,
                            const char *client_name,
                            const char *client_title,
                            const char *client_version,
                            double timeout_sec);

/* Close stdin, terminate subprocess, wait for exit. */
void codex_client_close(codex_client_t *c);

/* Free all resources. */
void codex_client_free(codex_client_t *c);

/* Send a JSON-RPC request, block for response.
 * Returns malloc'd JSON response string (caller frees), or NULL on error.
 * timeout_sec: max seconds to wait for response. */
char *codex_client_request(codex_client_t *c,
                           const char *method,
                           const char *params_json,  /* NULL = empty object */
                           double timeout_sec);

/* Send a JSON-RPC notification (no response expected). */
int codex_client_notify(codex_client_t *c,
                        const char *method,
                        const char *params_json);

/* Reply to a server-initiated request. */
int codex_client_respond(codex_client_t *c,
                         int request_id,
                         const char *result_json);

/* Send an error response to a server-initiated request. */
int codex_client_respond_error(codex_client_t *c,
                                const char *request_id_str,
                                int error_code,
                                const char *error_message);

/* Pop next notification from queue. Returns malloc'd JSON string or NULL.
 * timeout_sec: 0 = non-blocking, >0 = block up to timeout. */
char *codex_client_take_notification(codex_client_t *c, double timeout_sec);

/* Pop next server-initiated request. Returns malloc'd JSON string or NULL. */
char *codex_client_take_server_request(codex_client_t *c, double timeout_sec);

/* Check if subprocess is alive. */
bool codex_client_is_alive(codex_client_t *c);

/* Get last n stderr lines. Returns malloc'd string (caller frees). */
char *codex_client_stderr_tail(codex_client_t *c, int n_lines);

/* Get last error message. */
const char *codex_client_last_error(codex_client_t *c);

#ifdef __cplusplus
}
#endif

#endif /* CODEX_APP_SERVER_CLIENT_H */
