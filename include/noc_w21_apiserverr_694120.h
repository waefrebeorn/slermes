#ifndef NOC_W21_APISERVERR_694120_H
#define NOC_W21_APISERVERR_694120_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of gateway/platforms/api_server_room_dispatch.py (gate
 * w21_apiserverr_694120).
 *
 * The live entry point is an aiohttp handler that mints a session through an
 * authority connection; what is ported is every decision it takes before that
 * I/O: the room-only-field refusal, the accepted-key set, the grant/identity/
 * digest/input/idempotency validations, and the error classification. */

/* PoP: w21_apiserverr_694120_dispatch_error @ gateway/platforms/api_server_room_dispatch.py:_room_dispatch_error
 * {"message":..., "code":...} for a raised dispatch exception: the exception
 * text is kept as "invalid_room_dispatch" unless it names an execution-policy
 * or capability-catalog change, which are reworded for the caller. */
json_t *w21_apiserverr_694120_dispatch_error(const char *message);

/* PoP: w21_apiserverr_694120_normalize @ gateway/platforms/api_server_room_dispatch.py:_normalize_room_dispatch
 * The mind of `_normalize_room_dispatch` against a scripted grant/catalog:
 *   case = {
 *     "body": {...},              # the caller's JSON body
 *     "grant_token": "..."|"",    # self._room_grant_token(request)
 *     "headers": {"Idempotency-Key": "..."},
 *     "api_profile": "...",       # _api_server._api_request_profile
 *     "local_install": "...",     # hosted_rooms.local_authority_gateway_id()
 *     "catalog": {"catalog_digest": "...",
 *                 "execution_policy": {"policy_digest": "..."}},
 *     "grant_error": "..."|null,  # verify_room_grant raised ValueError(text)
 *     "session_id": "..."         # _ensure_hosted_member_session() answer
 *   }
 * Answer: {"kind":"error","message":...,"code":...,"status":400|403} |
 *         {"kind":"passthrough","body":...} |
 *         {"kind":"ok","body":{"input":...,"session_id":...,
 *                              "hosted_room_dispatch":{...},
 *                              "_room_execution_policy":{...}}}. */
json_t *w21_apiserverr_694120_normalize(const json_t *case_);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_APISERVERR_694120_H */
