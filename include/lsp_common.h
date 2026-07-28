/*
 * lsp_common.h — public API for agent/lsp/{protocol,client}.py.
 *
 * Minimal LSP JSON-RPC 2.0 framer + async-style client over a child
 * process (stdin/stdout pipes), faithful to the Python design:
 *   - protocol.py: Content-Length framing, envelope builders, classify
 *   - client.py:   one (server, workspace) client, document version
 *                  freshness, request/response correlation, diagnostics.
 *
 * Mapping notes (Python async -> C11 threads + blocking I/O):
 *   - asyncio reader loop  -> dedicated reader pthread
 *   - asyncio.Future table  -> pending request table (id -> result/error)
 *   - asyncio.Event counter -> monotonic push counter + condvar
 *   - os.environ + create_subprocess_exec(start_new_session=True)
 *                          -> fork/execvp in own process group, pipes
 *
 * PoP: lsp_protocol @ agent/lsp/protocol.py:LSPProtocolError
 * PoP: lsp_protocol @ agent/lsp/protocol.py:encode_message
 * PoP: lsp_protocol @ agent/lsp/protocol.py:read_message
 * PoP: lsp_client  @ agent/lsp/client.py:LSPClient
 * PoP: lsp_client  @ agent/lsp/client.py:_DocState
 */
#ifndef LSP_COMMON_H
#define LSP_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── protocol: error codes ───────────────────────────────────────────── */
#define LSP_ERR_CONTENT_MODIFIED  (-32801)
#define LSP_ERR_REQUEST_CANCELLED (-32800)
#define LSP_ERR_METHOD_NOT_FOUND  (-32601)

/* Protocol/framing error (the wire itself is broken). */
typedef struct {
    char *message;   /* owned */
} lsp_protocol_error_t;

/* A JSON-RPC error response returned by the server (protocol-conformant). */
typedef struct {
    int  code;
    char *message;   /* owned */
    char *data_json; /* owned, may be NULL */
} lsp_request_error_t;

/* ── protocol: framing ────────────────────────────────────────────────── */

/* Encode a JSON-RPC envelope as a Content-Length framed byte string.
 * Caller frees the returned buffer. */
char *lsp_encode_message(const char *json_body);

/* Read one Content-Length framed message from the fd (blocking).
 * Returns a malloc'd JSON string (NUL-terminated) on success,
 * NULL on clean EOF, and sets *proto_err on malformed framing.
 * The fd is advanced past the body on success. */
char *lsp_read_message(int fd, lsp_protocol_error_t *proto_err);

/* Envelope builders — return malloc'd JSON strings (caller frees). */
char *lsp_make_request(int id, const char *method, const char *params_json);
char *lsp_make_notification(const char *method, const char *params_json);
char *lsp_make_response(int id, const char *result_json);
char *lsp_make_error_response(int id, int code, const char *message,
                              const char *data_json);

typedef enum {
    LSP_MSG_INVALID = 0,
    LSP_MSG_REQUEST,
    LSP_MSG_RESPONSE,
    LSP_MSG_NOTIFICATION
} lsp_msg_kind_t;

/* Classify a parsed JSON-RPC message. key is set to the request id
 * (for request/response) or method name (notification); NULL otherwise.
 * For RESPONSE, *out_id is filled with the numeric id. */
lsp_msg_kind_t lsp_classify_message(const char *json, char **out_key,
                                    int *out_id);

/* ── client ──────────────────────────────────────────────────────────── */

typedef struct lsp_client lsp_client_t;

/* Server → client request handler. params_json is owned by caller; return a
 * malloc'd JSON result string (caller frees) or NULL to send methodNotFound.
 * Set *err_code / *err_message to return a JSON-RPC error instead. */
typedef char *(*lsp_server_request_handler)(const char *method,
                                            const char *params_json,
                                            int *err_code, char **err_message,
                                            void *user);

/* Notification handler (server → client, no reply). */
typedef void (*lsp_notification_handler)(const char *method,
                                         const char *params_json, void *user);

lsp_client_t *lsp_client_create(const char *server_id,
                                const char *workspace_root,
                                char **command,   /* NULL-terminated argv */
                                char **env,       /* NULL-terminated "K=V", optional */
                                const char *cwd,
                                const char *init_options_json);

void lsp_client_destroy(lsp_client_t *c);

/* Lifecycle. Returns 0 on success, negative on failure (message in err_out,
 * caller frees). start() spawns + initialize handshake. */
int lsp_client_start(lsp_client_t *c, char **err_out);
int lsp_client_shutdown(lsp_client_t *c);

bool lsp_client_is_running(lsp_client_t *c);
const char *lsp_client_state(lsp_client_t *c);

/* Document sync. open_file returns the document version (>=0) or -1 on error.
 * change_file bumps version and sends a full-document didChange. */
int lsp_client_open_file(lsp_client_t *c, const char *path, const char *text);
int lsp_client_change_file(lsp_client_t *c, const char *path, const char *text);

/* Diagnostics. diagnostics_for returns a malloc'd JSON array string of
 * publishDiagnostics entries (caller frees) or NULL if none/freshness
 * not satisfied. wait_for_diagnostics blocks until fresh push for the
 * given version or timeout_ms elapses. */
char *lsp_client_diagnostics_for(lsp_client_t *c, const char *path);
int lsp_client_wait_for_diagnostics(lsp_client_t *c, const char *path,
                                    int version, int timeout_ms);

/* Register a server→client request handler / notification handler. */
void lsp_client_set_request_handler(lsp_client_t *c,
                                    lsp_server_request_handler h, void *user);
void lsp_client_set_notification_handler(lsp_client_t *c,
                                         const char *method,
                                         lsp_notification_handler h, void *user);

#ifdef __cplusplus
}
#endif

#endif /* LSP_COMMON_H */
