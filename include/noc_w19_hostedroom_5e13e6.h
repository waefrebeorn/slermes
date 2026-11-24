#ifndef NOC_W19_HOSTEDROOM_5E13E6_H
#define NOC_W19_HOSTEDROOM_5E13E6_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_hostedroom_5e13e6_reservation_superseded @ persistence/sqlite/gateway/hosted_room_peers.py:_reservation_superseded
 * A newer epoch, or the same epoch under another gateway, outranks the claim. */
int w19_hostedroom_5e13e6_reservation_superseded(json_t *row, const char *gateway_id, long long epoch);

/* PoP: w19_hostedroom_5e13e6_reservation_values @ persistence/sqlite/gateway/hosted_room_peers.py:_reservation_values
 * (room_id, member_id, target_profile, authority_gateway_id, int(authority_epoch or 0)) of claims. */
json_t *w19_hostedroom_5e13e6_reservation_values(json_t *claims);

/* PoP: w19_hostedroom_5e13e6_remote_run_identity @ persistence/sqlite/gateway/hosted_room_peers.py:_remote_run_identity
 * The receipt key: the nine identity columns of a record, in table order. */
json_t *w19_hostedroom_5e13e6_remote_run_identity(json_t *record);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_HOSTEDROOM_5E13E6_H */
