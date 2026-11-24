/*
 * noc_w31_hostedroom_50fa41_p2.h - C surface for the session/run state-machine
 * half of upstream Hermes tui_gateway/hosted_room_peer_http.py
 * (gate w31_hostedroom_50fa41_p2).
 *
 * These are the decisions that keep a peer member turn fenced to one room
 * authority: the scope/observation binding, the accepted/close shapes, the
 * admission-recovery and poll backoff, the history/status projections and the
 * approval/stop/refresh outcomes.  HTTP, the clock, the durable receipt store
 * and the in-memory tables stay in the caller; each function takes their
 * answers as input and returns what the values decide.
 */
#ifndef NOC_W31_HOSTEDROOM_50FA41_P2_H
#define NOC_W31_HOSTEDROOM_50FA41_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* in {"api_key","poll_min_seconds","poll_max_seconds"} -> {"error": str|null}
 * for the constructor's two value guards. */
json_t *w31_hostedroom_50fa41_p2_api_checks(const json_t *in);

/* in {"room_id","profile","source","grant","expected_session_id"|null} ->
 * envelope carrying {"session_id","title","source"}. */
json_t *w31_hostedroom_50fa41_p2_prepare(const json_t *in);

/* in {"scope": {...}} -> envelope carrying the 7-field receipt scope. */
json_t *w31_hostedroom_50fa41_p2_room_scope(const json_t *in);

/* in {"dispatch": {...}, "run_id", "session_id", "replayed"} -> the accepted
 * turn projection. */
json_t *w31_hostedroom_50fa41_p2_accepted(const json_t *in);

/* in {"dispatch": {...}, "run_id", "session_id"} -> the durable receipt record
 * _save_receipt stores. */
json_t *w31_hostedroom_50fa41_p2_receipt_record(const json_t *in);

/* in {"existing": obj|null, "prepared": obj|null} -> envelope carrying the
 * session id _session_id resolves. */
json_t *w31_hostedroom_50fa41_p2_session_id_choice(const json_t *in);

/* in {"dispatch": {...}, "existing": obj|null, "result": {...},
 *     "session": str} -> {"ok":true,"value":{"closed":true}} or the accepted
 * shape close_admission returns. */
json_t *w31_hostedroom_50fa41_p2_close_result(const json_t *in);

/* in {"dispatch","existing"|null,"backoff"|null,"now","poll_min_seconds",
 *     "poll_max_seconds","admit"} -> {"action","accepted","backoff","error"}
 * for recover_dispatch. */
json_t *w31_hostedroom_50fa41_p2_recover_decision(const json_t *in);

/* in {"cached"|null,"record","now","poll_min_seconds","poll_max_seconds",
 *     "fetched"} -> {"action","status","cache","terminal_key","resolved",
 *     "error"} for _poll_receipt. */
json_t *w31_hostedroom_50fa41_p2_poll_cache(const json_t *in);

/* in {"key": [task_id,generation]|null, "record"|null,
 *     "explicit"|null, "query": {...}} -> {"record": obj|null} or refusal. */
json_t *w31_hostedroom_50fa41_p2_observation_receipt(const json_t *in);

/* in {"receipt"|null, "status": {...}} -> the history projection (array). */
json_t *w31_hostedroom_50fa41_p2_history_view(const json_t *in);

/* in {"receipt"|null, "status": {...}} -> the status projection. */
json_t *w31_hostedroom_50fa41_p2_status_view(const json_t *in);

/* in {"record"|null,"request_id","choice","cached"|null,"status"|null,
 *     "action"|null} -> {"action","body","identity","result","error"}. */
json_t *w31_hostedroom_50fa41_p2_approve_decision(const json_t *in);

/* in {"record"|null, "run_id"} -> {"dropped": bool, "return": true|"seam"}. */
json_t *w31_hostedroom_50fa41_p2_forget_decision(const json_t *in);

/* in {"record"|null,"result"|null,"error"|null} ->
 * {"action","terminal","result","error"} for stop_receipt. */
json_t *w31_hostedroom_50fa41_p2_stop_decision(const json_t *in);

/* in {"record"|null,"error"|null} -> {"action","run_id","error"} for
 * refresh_receipt. */
json_t *w31_hostedroom_50fa41_p2_refresh_decision(const json_t *in);

/* in {"refreshed": {...}} -> envelope carrying the replacement grant string. */
json_t *w31_hostedroom_50fa41_p2_refreshed_grant(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W31_HOSTEDROOM_50FA41_P2_H */
