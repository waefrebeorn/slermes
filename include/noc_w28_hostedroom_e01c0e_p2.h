/*
 * noc_w28_hostedroom_e01c0e_p2.h - C surface for the status/predicate half of
 * upstream tui_gateway/hosted_room_service.py (gate w28_hostedroom_e01c0e_p2).
 *
 * What a room reports to Desktop and the watcher: which peer routes exist and
 * in what state, the memory-only change signature, the grant-refresh terminal
 * test, the turn budget and the tuning constants. The runtime status dict, the
 * route map, the pending actions and the stall tracker arrive as fixtures.
 */
#ifndef NOC_W28_HOSTEDROOM_E01C0E_P2_H
#define NOC_W28_HOSTEDROOM_E01C0E_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _grant_revoke_is_terminal(exc): status in {401,403} and the peer's error code */
json_t *hrs_grant_revoke_is_terminal(const json_t *status_code, const char *error_code);

/* _route_statuses(room_id=None): [{"room_id","member_id","status"}], sorted by key */
json_t *hrs_route_statuses(const json_t *routes, const char *room_id);

/* live_status_sig(): {room_id: sorted rows} plus "" -> [worker_running] */
json_t *hrs_live_status_sig(const json_t *fixture);

/* _hosted_room_turn_timeout_seconds(): (env or 1800) + terminal grace */
json_t *hrs_turn_timeout_seconds(const json_t *env_value);

/* the contention/backoff budget constants, status families included */
json_t *hrs_constants(void);

/* _authority(room): [str(authority_gateway_id), int(authority_epoch)] */
json_t *hrs_authority(const json_t *room);

#ifdef __cplusplus
}
#endif
#endif
