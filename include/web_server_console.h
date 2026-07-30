/*
 * web_server_console.h — /api/console frame protocol engine (faithful C11
 * port of the console WS layer in hermes_cli/web_server.py).
 *
 * The Python endpoint is an async WS loop; the portable core is the frame
 * protocol itself: JSON frame decode (_console_json_payload), result → wire
 * frames (_console_send_result), and the session state machine inside
 * console_ws (ping/cancel/busy/confirm/input dispatch with
 * pending-confirmation and generation tracking). Command execution is a
 * callback seam so the server wires in the real console engine.
 */
#ifndef WEB_SERVER_CONSOLE_H
#define WEB_SERVER_CONSOLE_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _CONSOLE_PROMPT / limits (web_server.py module constants). */
#define WS_CONSOLE_PROMPT "hermes> "
#define WS_CONSOLE_COMMAND_TIMEOUT_SECONDS 60.0
#define WS_CONSOLE_OUTPUT_LIMIT 50000
#define WS_CONSOLE_EXECUTOR_MAX_WORKERS 4

/* Python _console_json_payload: decode one WS frame into a JSON object.
 * `text` (may be NULL) wins over `bytes`. Returns the parsed object, or
 * NULL with *error set to a malloc'd message (invalid UTF-8 / not JSON /
 * not an object), or NULL with *error NULL when the frame carries no
 * payload at all. */
json_t *ws_console_json_payload(const char *text, const unsigned char *bytes,
                                size_t bytes_len, char **error);

/* Python _console_profile_from_ws: strip; empty → NULL. Malloc'd. */
char *ws_console_profile_from_query(const char *profile);

/* Console engine result (mirror of console_engine.ConsoleResult). */
typedef struct {
    const char *status;   /* ok | error | confirm_required | clear | exit */
    const char *command;  /* may be NULL */
    const char *output;   /* may be NULL */
    const char *confirmation_message; /* may be NULL */
} ws_console_result_t;

/* Python _console_send_result: append the wire frames for one result to
 * `frames` (a json array). command_id mirrors the Python parameter. */
void ws_console_send_result(json_t *frames, const ws_console_result_t *result,
                            int command_id);

/* Execution seam: run one console line. Return true on success and fill
 * *out (strings must stay valid until the callback returns are copied);
 * return false to signal the Python `except Exception` path with *err_msg
 * (malloc'd or NULL → class-name fallback handled by caller). */
typedef bool (*ws_console_exec_fn)(void *ctx, const char *line,
                                   bool confirmed, ws_console_result_t *out);

/* Session state machine (console_ws locals: pending_confirmation,
 * command_generation, busy flag standing in for active_task). */
typedef struct ws_console_session ws_console_session_t;

ws_console_session_t *ws_console_session_new(void);
void ws_console_session_free(ws_console_session_t *s);

/* Test/wiring hooks mirroring `active_task and not active_task.done()`. */
void ws_console_session_set_busy(ws_console_session_t *s, bool busy);
bool ws_console_session_busy(const ws_console_session_t *s);
const char *ws_console_session_pending(const ws_console_session_t *s);
int ws_console_session_generation(const ws_console_session_t *s);

/* Handle one decoded frame: appends every outgoing payload to `frames`
 * in order. Returns true when the session should close (status "exit" —
 * Python does `await ws.close(code=1000)`). */
bool ws_console_handle_frame(ws_console_session_t *s, const json_t *payload,
                             ws_console_exec_fn exec, void *exec_ctx,
                             json_t *frames);

#ifdef __cplusplus
}
#endif

#endif /* WEB_SERVER_CONSOLE_H */
