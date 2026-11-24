/*
 * noc_w23_hostedroom_95f0aa.h -- public C API for the hosted Bot Mode room
 * validators and grant helpers (gate w23_hostedroom_95f0aa).
 *
 * Upstream: persistence/gateway/hosted_rooms.py. The repository Protocols are
 * sqlite; what remains pure is the grant identity, the grant's hard expiry,
 * the room-driver identifier rule, the stable task identity and the bounded
 * constants the room driver shares.
 */
#ifndef NOC_W23_HOSTEDROOM_95F0AA_H
#define NOC_W23_HOSTEDROOM_95F0AA_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MAX_OWED_REVOKE_SECONDS, MAX_OWED_REVOKES_PER_MEMBER, MAX_OWED_REVOKES and
 * the task-status sets, as the JSON object the gateway reads them from. */
json_t *w23_hostedroom_95f0aa_constants(void);

/* grant_digest(grant): the grant's SHA-256 hex digest; NULL for a non-string. */
char *w23_hostedroom_95f0aa_grant_digest(const char *grant);

/* room_grant_hard_expiry(grant, now=...): req {"grant": <str>, "now": <num|null>}.
 * An unreadable grant lives now + MAX_OWED_REVOKE_SECONDS. */
json_t *w23_hostedroom_95f0aa_hard_expiry(const json_t *req);

/* _task_identifier(value, label=...): req {"value": <any>, "label": <str>}.
 * {"ok":true,"value":<stripped>} or {"ok":false,"error":<message>}. */
json_t *w23_hostedroom_95f0aa_task_identifier(const json_t *req);

/* HostedTaskIdentity(room_id, task_id, thread_id, turn_id): the same shape,
 * each field checked and stripped in declaration order. */
json_t *w23_hostedroom_95f0aa_task_identity(const json_t *req);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W23_HOSTEDROOM_95F0AA_H */
