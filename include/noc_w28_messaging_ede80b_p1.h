/*
 * noc_w28_messaging_ede80b_p1.h — C surface for upstream
 * hermes_cli/web_routers/messaging.py (gate w28_messaging_ede80b_p1).
 *
 * WhatsApp QR onboarding: the value shaping the start/apply handlers do before
 * anything is written (.env mode, allowlist normalization, phone extraction)
 * and the pairing-event state machine the stdout watcher applies to a session
 * record.  The subprocess, the lock, the expiry timer and the session store are
 * seams; what is ported is what those values decide.
 *
 * Every entry point returns an envelope object:
 *   {"ok": true,  "value": <json>}
 *   {"ok": false, "status": <int>, "detail": <string>}
 */
#ifndef NOC_W28_MESSAGING_EDE80B_P1_H
#define NOC_W28_MESSAGING_EDE80B_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* value -> "bot" | "self-chat", else 400 "WhatsApp mode must be 'bot' or 'self-chat'." */
json_t *w28_messaging_ede80b_p1_normalize_mode(const json_t *value);

/* value -> comma-joined IDs with every space removed and blank parts dropped. */
json_t *w28_messaging_ede80b_p1_normalize_allowed_users(const json_t *value);

/* "12345:7@s.whatsapp.net" -> "12345"; no digits -> null. */
json_t *w28_messaging_ede80b_p1_phone_from_identifier(const json_t *value);

/* first non-empty str(candidate[k] or "").strip() over keys, else null. */
json_t *w28_messaging_ede80b_p1_first_str(const json_t *candidate, const json_t *keys);

/* creds.json payload -> {account_id, account_name, account_phone}. */
json_t *w28_messaging_ede80b_p1_linked_account(const json_t *payload);

/* {qr_payload,status,error,account_id,account_name,account_phone} x bridge event
 * -> the same record after the event. */
json_t *w28_messaging_ede80b_p1_apply_pairing_event(const json_t *record,
                                                    const json_t *payload);

#ifdef __cplusplus
}
#endif
#endif
