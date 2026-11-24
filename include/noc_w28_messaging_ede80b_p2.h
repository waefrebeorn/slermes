/*
 * noc_w28_messaging_ede80b_p2.h — C surface for upstream
 * hermes_cli/web_routers/messaging.py (gate w28_messaging_ede80b_p2).
 *
 * Telegram QR onboarding: user-id normalization, the allowed-id list rules, the
 * pairing-service expiry parse, and the request/response shaping of the three
 * endpoints (start, poll, apply).  The pairing service HTTP call and the
 * pairing table are seams — their payloads arrive as fixture inputs.
 *
 * Every entry point returns an envelope object:
 *   {"ok": true,  "value": <json>}
 *   {"ok": false, "status": <int>, "detail": <string>}
 */
#ifndef NOC_W28_MESSAGING_EDE80B_P2_H
#define NOC_W28_MESSAGING_EDE80B_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* str(value or "").strip() when it fullmatches \d+, else null. */
json_t *w28_messaging_ede80b_p2_normalize_user_id(const json_t *value);

/* body.allowed_user_ids -> deduped id list, or the 400 the handler raises
 * (non-numeric first, empty second). */
json_t *w28_messaging_ede80b_p2_normalize_allowed_ids(const json_t *ids);

/* ISO-8601 expiry -> epoch seconds; null where the live code would fall back to
 * time.time()+600 (the clock read is the seam). */
json_t *w28_messaging_ede80b_p2_parse_expiry_ts(const json_t *value);

/* record -> {"status":"ready","bot_username","owner_user_id","expires_at"}. */
json_t *w28_messaging_ede80b_p2_ready_payload(const json_t *record);

/* pairing-service start response -> the endpoint body, or 502 when a field is
 * missing. */
json_t *w28_messaging_ede80b_p2_start_payload(const json_t *payload);

/* (record, polled service payload) -> the endpoint verdict: ready (also the
 * short-circuit when the record already holds a token), waiting, 410, or 502
 * for an unknown status. */
json_t *w28_messaging_ede80b_p2_status_payload(const json_t *record,
                                               const json_t *payload);

#ifdef __cplusplus
}
#endif
#endif
