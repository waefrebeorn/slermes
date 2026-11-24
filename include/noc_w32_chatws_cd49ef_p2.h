/*
 * noc_w32_chatws_cd49ef_p2.h - C surface for upstream
 * hermes_cli/web_routers/chat_ws.py, part 2 (gate w32_chatws_cd49ef_p2).
 *
 * The sidecar half of the chat websocket: the /api/ws, /api/pub and
 * /api/events pre-accept gate, the lazily created event-channel state, the
 * single-writer lease holder a /api/pty attach has to give way to, the
 * broadcast fan-out, and the /api/pty channel marker state.
 *
 * Every socket, lock and registry read is a seam; each function below takes
 * the seam values as one request object and returns the derived value.
 */
#ifndef NOC_W32_CHATWS_CD49EF_P2_H
#define NOC_W32_CHATWS_CD49EF_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Sidecar close codes. */
#define W32_CHATWS_CD49EF_CLOSE_BAD_AUTH 4401
#define W32_CHATWS_CD49EF_CLOSE_FORBIDDEN 4403

/* req {chat_enabled, auth_ok, request_allowed}
 *   -> {accepted: bool, closed: [code...]} */
json_t *w32_chatws_cd49ef_p2_sidecar_gate(const json_t *req);

/* req {has_channels, has_lock, channels:{ch:[sub...]}}
 *   -> {created: bool, channels:{ch: count}} */
json_t *w32_chatws_cd49ef_p2_event_state(const json_t *req);

/* req {entries:[{session_id,pid}], session_id, raises} -> pid | true | null. */
json_t *w32_chatws_cd49ef_p2_lease_holder(const json_t *req);

/* req {channels:{ch:[sub...]}, channel, payload, failing:[sub...]}
 *   -> [[sub, payload]...] for the sends that succeeded, in send order. */
json_t *w32_chatws_cd49ef_p2_broadcast(const json_t *req);

/* req {channel, marker_channels:[ch...], active_file, resume, force_fresh,
 *      marker_doc}
 *   -> {file, preexisting, resume} */
json_t *w32_chatws_cd49ef_p2_marker_state(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
