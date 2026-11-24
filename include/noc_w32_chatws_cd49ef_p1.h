/*
 * noc_w32_chatws_cd49ef_p1.h - C surface for upstream
 * hermes_cli/web_routers/chat_ws.py, part 1 (gate w32_chatws_cd49ef_p1).
 *
 * The console half of the chat websocket: what a sequence of client frames
 * makes the /api/console handler send back, what one raw message decodes to,
 * which channel/topic key a query string may name, and the session id an
 * active-session marker file yields.
 *
 * Every socket, engine and file is a seam; each function below takes the seam
 * values as one request object and returns the derived value.
 */
#ifndef NOC_W32_CHATWS_CD49EF_P1_H
#define NOC_W32_CHATWS_CD49EF_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The console prompt suffix every frame carries. */
#define W32_CHATWS_CD49EF_CONSOLE_PROMPT "hermes> "
/* _VALID_CHANNEL_RE bound. */
#define W32_CHATWS_CD49EF_CHANNEL_MAX 128

/* req {query:{profile}, msgs:[{type,text|bytes_hex}], results:[{status,command,
 *      output,confirmation_message}]}
 *   -> {sent:[frame...], closed:[[code, reason]...]} */
json_t *w32_chatws_cd49ef_p1_console_run(const json_t *req);

/* req {text|null, bytes_hex|null} -> {payload: obj|null, error: str|null} */
json_t *w32_chatws_cd49ef_p1_console_payload(const json_t *req);

/* req {channel} -> the validated channel string, or null when invalid. */
json_t *w32_chatws_cd49ef_p1_channel_key(const json_t *req);

/* req {doc} -> the marker's session id (str) or null. */
json_t *w32_chatws_cd49ef_p1_marker_session(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
