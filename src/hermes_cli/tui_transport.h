#ifndef TUI_TRANSPORT_H
#define TUI_TRANSPORT_H

/*
 * tui_transport.h — TUI transport layer abstraction.
 *
 * Provides reliable message transport between the TUI frontend and
 * the agent process. Supports FIFO, Pipe, and Unix socket transports with
 * message-frame awareness, reconnection, callback notifications,
 * TeeTransport mirroring, and per-request transport binding.
 *
 * Port of Python: tui_gateway.transport — Transport protocol, StdioTransport,
 * TeeTransport, and contextvar-based transport binding.
 *
 * Usage:
 *   tui_transport_t t;
 *   tui_transport_init(&t, "/tmp/hermes-tui-rpc", TUI_TRANSPORT_FIFO);
 *   if (tui_transport_connect(&t)) {
 *       tui_transport_send(&t, "hello", 5);
 *       char buf[4096];
 *       int n = tui_transport_recv(&t, buf, sizeof(buf), 100);
 *   }
 *   tui_transport_shutdown(&t);
 *
 * TeeTransport usage:
 *   tui_transport_tee_t tee;
 *   tui_transport_tee_init(&tee, &primary, NULL);
 *   tui_transport_tee_add(&tee, &secondary);
 *   tee.base.send(&tee.base, "data", 4);  // writes to both
 *
 * Transport binding (contextvar equivalent):
 *   tui_transport_bind(&t);
 *   tui_transport_current()->send(...);  // writes to bound transport
 *   tui_transport_reset(old);
 */

#include <stdbool.h>
#include <stddef.h>

/* ── Transport type ── */
typedef enum {
    TUI_TRANSPORT_FIFO,    /* Named FIFO (default, used by tui_fullscreen) */
    TUI_TRANSPORT_PIPE,    /* Anonymous pipe (stdin/stdout to subprocess) */
    TUI_TRANSPORT_SOCKET,  /* Unix domain stream socket (socketpair) */
} tui_transport_type_t;

/* ── Connection state ── */
typedef enum {
    TUI_TRANSPORT_IDLE,
    TUI_TRANSPORT_CONNECTING,
    TUI_TRANSPORT_CONNECTED,
    TUI_TRANSPORT_DISCONNECTED,
    TUI_TRANSPORT_ERROR,
} tui_transport_state_t;

/* ── Connection state change callback ── */
typedef void (*tui_transport_callback_t)(tui_transport_state_t old_state,
                                          tui_transport_state_t new_state,
                                          void *user_data);

/* ── Transport instance ──
 * All fields are internal. Use API functions only.
 */
typedef struct {
    tui_transport_type_t  type;          /* Transport type */
    tui_transport_state_t state;         /* Current connection state */
    char                 *path;          /* Transport path (FIFO name / socket path) */
    
    /* File descriptors */
    int                   read_fd;       /* Read file descriptor */
    int                   write_fd;      /* Write file descriptor */
    
    /* Read buffer */
    char                 *buf;           /* Internal read buffer */
    size_t                buf_size;      /* Buffer capacity */
    size_t                read_pos;      /* Current read position */
    size_t                msg_start;     /* Start of current message in buffer */
    
    /* Reconnection */
    int                   max_retries;   /* 0 = no reconnect */
    int                   retry_delay_ms; /* Delay between retries (ms) */
    
    /* Callbacks */
    tui_transport_callback_t on_state_change;
    void                   *callback_data;
    
    /* Internal flags */
    bool                  owned_path;    /* Free path on shutdown */
} tui_transport_t;

/* ── Defaults ── */
#define TUI_TRANSPORT_DEFAULT_BUF_SIZE 65536
#define TUI_TRANSPORT_DEFAULT_RETRIES  3
#define TUI_TRANSPORT_DEFAULT_RETRY_MS 200

/* ── API ── */

/* Initialize transport instance.
 * Call before any other transport function.
 * path: FIFO path or socket path (copied internally).
 * type: transport type (use TUI_TRANSPORT_FIFO for default).
 * Returns false on OOM.
 */
bool tui_transport_init(tui_transport_t *t,
                         const char *path,
                         tui_transport_type_t type);

/* Configure reconnection parameters. */
void tui_transport_set_retry(tui_transport_t *t,
                              int max_retries, int delay_ms);

/* Register state change callback. */
void tui_transport_set_callback(tui_transport_t *t,
                                 tui_transport_callback_t cb,
                                 void *user_data);

/* Connect to transport endpoint.
 * For FIFO: creates the FIFO (mkfifo) and opens read fd.
 * For socket: creates + binds/listens (server side) or connects (client).
 * Returns true on success.
 */
bool tui_transport_connect(tui_transport_t *t);

/* Try to reconnect (for transport error recovery).
 * Returns true if reconnected.
 */
bool tui_transport_reconnect(tui_transport_t *t);

/* Poll for incoming data.
 * Returns:
 *   > 0   — bytes available from next message (call tui_transport_recv)
 *   0     — no data available (EAGAIN)
 *   -1    — transport error (call tui_transport_reconnect or shutdown)
 */
int tui_transport_poll(tui_transport_t *t);

/* Receive a full message.
 * Reads the next available message from the internal buffer.
 * Returns number of bytes written to buf, or -1 on error.
 * Timeout is in milliseconds (-1 = block indefinitely, 0 = non-blocking).
 */
int tui_transport_recv(tui_transport_t *t, char *buf, size_t sz, int timeout_ms);

/* Send a message.
 * Writes len bytes from buf to the write fd.
 * Returns true on success.
 */
bool tui_transport_send(tui_transport_t *t, const char *buf, size_t len);

/* Send a formatted string message (printf-style).
 * Returns true on success.
 */
bool tui_transport_sendf(tui_transport_t *t, const char *fmt, ...);

/* Check if transport is connected. */
bool tui_transport_is_connected(const tui_transport_t *t);

/* Get current state. */
tui_transport_state_t tui_transport_state(const tui_transport_t *t);

/* Get human-readable state name. */
const char *tui_transport_state_name(tui_transport_state_t state);

/* Shutdown and release all resources.
 * Closes file descriptors and optionally unlinks FIFO/socket path.
 * After shutdown, init must be called again to reuse.
 */
void tui_transport_shutdown(tui_transport_t *t);

/* Send a JSON-RPC 2.0 notification message (convenience).
 * Builds {"jsonrpc":"2.0","method":"%s","params":%s}\n and sends it.
 */
bool tui_transport_send_rpc(tui_transport_t *t,
                             const char *method, const char *params_json);

/* ── TeeTransport — mirrors writes to multiple transports ── */

/* A virtual transport that writes to one primary + N secondaries.
 * Python equivalent: tui_gateway.transport.TeeTransport.
 * The primary's return value determines success/failure;
 * secondary failures are silently swallowed.
 */
#define TUI_TRANSPORT_TEE_MAX 8

typedef struct {
    tui_transport_t     *primary;       /* Primary transport */
    tui_transport_t     *secondaries[TUI_TRANSPORT_TEE_MAX];
    int                  secondary_count;
} tui_transport_tee_t;

/* Initialize TeeTransport with a primary (required) and optional
 * varargs list of additional transports (NULL-terminated).
 * Python equivalent: TeeTransport(primary, *secondaries)
 */
void tui_transport_tee_init(tui_transport_tee_t *tee,
                             tui_transport_t *primary, ...);

/* Add a secondary transport (best-effort mirror). */
bool tui_transport_tee_add(tui_transport_tee_t *tee,
                            tui_transport_t *secondary);

/* Write to primary + all secondaries. Returns primary's result. */
bool tui_transport_tee_write(tui_transport_tee_t *tee,
                              const char *buf, size_t len);

/* Close primary + all secondaries. */
void tui_transport_tee_close(tui_transport_tee_t *tee);

/* ── Transport binding (contextvar equivalent) ── */

/* Bind a transport for the current "request" (thread-local).
 * Python equivalent: bind_transport(transport) / current_transport()
 * Uses __thread for per-thread isolation without contextvar overhead.
 * Returns the previous binding for restore via tui_transport_reset().
 */
tui_transport_t *tui_transport_bind(tui_transport_t *t);

/* Get the currently bound transport, or NULL if none. */
tui_transport_t *tui_transport_current(void);

/* Restore a previous transport binding. */
void tui_transport_reset(tui_transport_t *old);

/* Clear the current binding without restoring. */
void tui_transport_clear_binding(void);

#endif /* TUI_TRANSPORT_H */
