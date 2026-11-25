/*
 * noc_w34_computehos_e233d0.h - C surface for upstream
 * tui_gateway/compute_host_bridge.py (gate w34_computehos_e233d0).
 *
 * The value core of the compute-host (turn isolation) bridge: who is isolated,
 * which session the host owns, the turn.start payload the child is handed, the
 * registry-lease vouch that rides with it, the session_key/history_version
 * adoption a host frame causes, the control frame's defaults, the compress RPC
 * wait budget and the ack->error envelope.
 *
 * Caller seams (kept out of this file, because they are transport/config, not
 * values): the HostSupervisor process, sockets, write_json/_emit fan-out, the
 * session table, history locks, config.yaml reads, cwd/source/auth resolution
 * and the vault redaction registry.
 */
#ifndef NOC_W34_COMPUTEHOS_E233D0_H
#define NOC_W34_COMPUTEHOS_E233D0_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* `_COMPUTE_HOST_COMPRESS_WAIT_CAP_SECS`: the compress RPC wait cap, in seconds.
 * Stays below the desktop's 660s SESSION_COMPRESS_TIMEOUT_MS so a slow host
 * answers `pending` instead of the client timing out (#97948). */
#define W34_COMPUTEHOS_E233D0_COMPRESS_WAIT_CAP_SECS 630.0

/* `_turn_isolation_enabled(cfg)`.  `child_env_is_1` is
 * os.environ["HERMES_COMPUTE_HOST_CHILD"] == "1" (env is the seam); a falsy cfg
 * means the config.yaml read, which is also a seam, so it answers 0. */
int w34_computehos_e233d0_turn_isolation_enabled(const json_t *cfg, int child_env_is_1);

/* `_session_uses_compute_host(session, cfg)`. */
int w34_computehos_e233d0_session_uses_compute_host(const json_t *session, const json_t *cfg,
                                                   int child_env_is_1);

/* `_active_session_lease_vouch(session)` -> {"lease_id","session_id"} or null.
 * `session.active_session_lease` is read as an object (the lease's attributes). */
json_t *w34_computehos_e233d0_active_session_lease_vouch(const json_t *session);

/* `_metadata_mirror(session)` -> the mirrored metadata object ({} when none). */
json_t *w34_computehos_e233d0_metadata_mirror(const json_t *session);

/* `_open_request_matches(session, request_id)`. */
int w34_computehos_e233d0_open_request_matches(const json_t *session, const json_t *request_id);

/* `_compute_host_adopt_frame_meta(session, frame)` ->
 * {"session_key": <str>, "history_version": <raw|nil>, "lease_rotated": <bool>}.
 * The lease transfer itself is a registry seam. */
json_t *w34_computehos_e233d0_adopt_frame_meta(const json_t *session, const json_t *frame);

/* `_send_compute_host_control` frame head: dict(payload or {}) with
 * setdefault("type","control") / setdefault("command", command). */
json_t *w34_computehos_e233d0_control_frame(const json_t *payload, const json_t *command);

/* `_compute_host_compress_wait_seconds(cfg)`: compression ceiling + 30s of slack,
 * floored at 120s and capped at the 630s wait cap.  The auxiliary-compression
 * timeout floor (a config read) is a seam; fixtures keep the ceiling above it. */
double w34_computehos_e233d0_compress_wait_seconds(const json_t *cfg);

/* `_compute_host_ack_error(rid, ack, code, default)` -> `_err` envelope for a
 * "control.error"/"error" ack, else null. */
json_t *w34_computehos_e233d0_ack_error(const json_t *rid, const json_t *ack, int code,
                                        const char *fallback);

/* `_compute_host_turn_frame(rid, sid, session, text, image_paths,
 * queued_prompt_generation, display_kind, display_metadata)`.
 * Pass NULL for an omitted (Python None) argument; `image_paths` NULL takes the
 * session's attached_images. */
json_t *w34_computehos_e233d0_turn_frame(const json_t *rid, const json_t *sid,
                                        const json_t *session, const json_t *text,
                                        const json_t *image_paths,
                                        const json_t *queued_prompt_generation,
                                        const json_t *display_kind,
                                        const json_t *display_metadata);

#ifdef __cplusplus
}
#endif
#endif
