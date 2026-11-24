/*
 * noc_w31_hostedroom_50fa41_p1.h - C surface for the request-building and
 * error-classification half of upstream Hermes
 * tui_gateway/hosted_room_peer_http.py (gate w31_hostedroom_50fa41_p1).
 *
 * Everything here is decided by values: the scoped-run URL path, the wire
 * headers/body of a peer request, the JSON error-code extractor, the HTTP
 * status classifier and the retry/backoff arithmetic.  The sockets, the clock
 * and the credentialed opener stay in the caller -- their answers arrive in
 * the request object, never from this file.
 */
#ifndef NOC_W31_HOSTEDROOM_50FA41_P1_H
#define NOC_W31_HOSTEDROOM_50FA41_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Peer response byte budgets (hosted_room_peer_http.py module constants). */
#define W31_HRP_MAX_PEER_RESPONSE_BYTES (16 * 1024 * 1024)
#define W31_HRP_MAX_PEER_ERROR_RESPONSE_BYTES (16 * 1024)

/* in {"code": any} -> the code string when it matches ^[a-z][a-z0-9_]{0,63}$,
 * else null. */
json_t *w31_hostedroom_50fa41_p1_valid_code(const json_t *in);

/* in {"detail": str|null} -> the machine error code inside a peer error body
 * (legacy dispatch-message normalisation included), else null. */
json_t *w31_hostedroom_50fa41_p1_response_error_code(const json_t *in);

/* in {"run_id": any, "suffix": [str]} -> "/v1/runs/<quoted>(/<suffix>...)". */
json_t *w31_hostedroom_50fa41_p1_run_path(const json_t *in);

/* in {"full": obj} -> the projected run status (approval lifted out of
 * pending_controls). */
json_t *w31_hostedroom_50fa41_p1_run_status(const json_t *in);

/* in {"cached": obj|null, "poll_min_seconds", "poll_max_seconds"} -> the next
 * doubled, clamped poll delay. */
json_t *w31_hostedroom_50fa41_p1_next_poll_delay(const json_t *in);

/* in {"grant": any} -> {"ok":true,"value":grant} or the refusal envelope. */
json_t *w31_hostedroom_50fa41_p1_require_room_grant(const json_t *in);

/* in {base_url, method, path, api_key, room_grant|null, body|null,
 *     headers|null, declared|null, response} -> {"request": <wire echo>,
 *     "ok":true,"value":payload} or {"request": ..., "ok":false,"error":...}.
 * The response carrier models the socket seam:
 *   {"kind":"ok","raw":str,"declared":num|null}
 *   {"kind":"http","status":num,"detail":str}
 *   {"kind":"unreachable","gaierror":bool,"oserror":bool,"errno_name":str|null} */
json_t *w31_hostedroom_50fa41_p1_request(const json_t *in);

/* in {"dispatch": {...}} -> {"body": {...}, "idempotency_key": str}. */
json_t *w31_hostedroom_50fa41_p1_admission_body(const json_t *in);

/* in {room_id, home_install_id, authority_gateway_id, authority_epoch,
 *     member_id, grant_id, ttl_seconds?, status_ttl_seconds?, api_key} ->
 * envelope carrying the invitation body. */
json_t *w31_hostedroom_50fa41_p1_invitation_body(const json_t *in);

/* in {"catalog": {...}, "capability_digest": any, "execution_policy_digest":
 * any} -> null when the frozen digests still match, else the 403 drift error. */
json_t *w31_hostedroom_50fa41_p1_digest_check(const json_t *in);

/* in {"ambiguous": bool, "gaierror": bool, "oserror": bool,
 *     "errno_name": str|null} -> the unreachable-endpoint error verdict. */
json_t *w31_hostedroom_50fa41_p1_pre_admission(const json_t *in);

/* in {"method","path","status","detail"} -> the classified PeerRunsHTTPError
 * projection for one HTTP error response. */
json_t *w31_hostedroom_50fa41_p1_http_error(const json_t *in);

/* in {"kind": str, "budget": "size"|"time", "ambiguous": bool} -> the budget
 * overrun error projection. */
json_t *w31_hostedroom_50fa41_p1_budget_error(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W31_HOSTEDROOM_50FA41_P1_H */
