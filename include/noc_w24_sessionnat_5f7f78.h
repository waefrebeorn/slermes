/*
 * noc_w24_sessionnat_5f7f78.h - C surface for upstream
 * gateway/session_native_recovery_routes.py (gate w24_sessionnat_5f7f78).
 *
 * The value-level core of "rebuild queued native routes": which stored
 * admission envelope belongs to a queued route, whether a verified admission
 * replaces the old primary-bot guess, and the bounded background retry plan a
 * failed durable recovery follows.  The runner, the authorities, the admission
 * store and the asyncio task stay in the caller; every function here decides
 * from the values alone.
 */
#ifndef W24_SESSIONNAT_5F7F78_H
#define W24_SESSIONNAT_5F7F78_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of background retry delays (_RECOVERY_RETRY_DELAYS). */
#define W24_SESSIONNAT_5F7F78_RETRY_DELAYS 4

/* (payload['native_text_v1'], entry.session_key) -> bool: the envelope names
 * this route and carries relay provenance.  A non-object envelope is the live
 * AttributeError seam and answers null. */
json_t *w24_sessionnat_5f7f78_relay_match(const json_t *envelope, const json_t *session_key);

/* (runner._running, [attempt results]) -> {"attempts", "delays", "gave_up"}:
 * the delays _retry_recovery sleeps before it recovers, stops, or gives up. */
json_t *w24_sessionnat_5f7f78_retry_plan(const json_t *running, const json_t *attempts);

/* (payload, refused, fallback) -> the verified source, or the runner's restored
 * fallback when there is no payload or the authority refused it. */
json_t *w24_sessionnat_5f7f78_recovered_source(const json_t *payload, const json_t *refused,
                                               const json_t *fallback);

/* _RECOVERY_RETRY_DELAYS, as an object. */
json_t *w24_sessionnat_5f7f78_constants(void);

#ifdef __cplusplus
}
#endif
#endif
