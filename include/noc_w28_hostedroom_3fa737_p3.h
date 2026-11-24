/*
 * noc_w28_hostedroom_3fa737_p3.h - C surface for the arithmetic/scheduling layer
 * of upstream Hermes tui_gateway/hosted_room_driver.py (gate
 * w28_hostedroom_3fa737_p3).
 *
 * The numbers the supervisor and the worker are tuned by: the unavailable-route
 * backoff, the lease renewal and cache rules, the ambiguous-room expiry, the
 * deadline settlement payload, the rotating room window, the approval payload
 * and the constructor's argument validation.
 */
#ifndef NOC_W28_HOSTEDROOM_3FA737_P3_H
#define NOC_W28_HOSTEDROOM_3FA737_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _defer_unavailable_route: {"delay", "next_attempt_at"} -- lo on the first
 * failure, then min(hi, max(lo, previous*2)). */
json_t *w28_hostedroom_3fa737_p3_retry_delay(const json_t *previous, double lo, double hi,
                                             double now);

/* _renew_lease_if_needed's decision: {"renewed"} after the "a keeper renewed it
 * later" substitution and unless the caller forces one. */
json_t *w28_hostedroom_3fa737_p3_renew_decision(const json_t *lease, const json_t *known,
                                                double now, double ttl, int force);

/* _cache_lease: {"keep_known", "expires_at"} -- a stale write never replaces a
 * later expiry of the same process/generation. */
json_t *w28_hostedroom_3fa737_p3_cache_lease_keep(const json_t *known, const json_t *incoming);

/* _mark_ambiguous: the expiry the room waits out (the later of the attempt's and
 * the cache's of the same generation). */
json_t *w28_hostedroom_3fa737_p3_ambiguous_expiry(double attempt_expires_at, const json_t *known,
                                                  int attempt_generation);

/* _complete_acknowledged_stop: {"branch": "cancel"|"deadline", ...} -- a deadline
 * Stop settles an explicit failure, a user Stop completes the cancel. */
json_t *w28_hostedroom_3fa737_p3_deadline_settlement(const json_t *task, double timeout);

/* _run_cycle's window: {"next_cursor", "chosen"} for one cursor step and the
 * bounded fan-out over rooms that are not already running (chosen sorted). */
json_t *w28_hostedroom_3fa737_p3_cycle_plan(long long cursor, const json_t *rooms,
                                            const json_t *active, long long max_concurrent,
                                            int stop);

/* _report_pending_action: {"called", "room_id", "member_id", "action"} with the
 * safe choices filter ({once, deny}, defaulted when empty). */
json_t *w28_hostedroom_3fa737_p3_approval_action(const json_t *spec);

/* HostedRoomRuntime.__init__ validation: {"ok": bool, "error": str|null}. */
json_t *w28_hostedroom_3fa737_p3_validate_config(const json_t *cfg);

#ifdef __cplusplus
}
#endif
#endif
