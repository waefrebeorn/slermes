/* noc_w19_hostedroom_e90a2b.h - public C API for tui_gateway/hosted_room_peer_transport.py port. */
#ifndef NOC_W19_HOSTEDROOM_E90A2B_H
#define NOC_W19_HOSTEDROOM_E90A2B_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_hostedroom_e90a2b_route_identity @ tui_gateway/hosted_room_peer_transport.py:route_identity */
json_t *w19_hostedroom_e90a2b_route_identity(json_t *stored_rows, const char *home_install_id,
                                             const char *room_id, const char *member_id);
/* PoP: w19_hostedroom_e90a2b_build_member_dispatch @ tui_gateway/hosted_room_peer_transport.py:build_member_dispatch */
json_t *w19_hostedroom_e90a2b_build_member_dispatch(json_t *binding, json_t *route, json_t *args);
/* PoP: w19_hostedroom_e90a2b_failover_order @ tui_gateway/hosted_room_peer_transport.py:FailoverHostedRoomPeerClient._call */
json_t *w19_hostedroom_e90a2b_failover_order(json_t *spec);
/* PoP: w19_hostedroom_e90a2b_failover_call @ tui_gateway/hosted_room_peer_transport.py:FailoverHostedRoomPeerClient._call */
json_t *w19_hostedroom_e90a2b_failover_call(json_t *spec);
/* PoP: w19_hostedroom_e90a2b_validate_coordinates @ tui_gateway/hosted_room_peer_transport.py:PeerHostedRoomTransport._validate_coordinates */
json_t *w19_hostedroom_e90a2b_validate_coordinates(const char *profile, const char *source,
                                                   json_t *title, const char *route_profile,
                                                   const char *room_id);
/* PoP: w19_hostedroom_e90a2b_submit_guard @ tui_gateway/hosted_room_peer_transport.py:PeerHostedRoomTransport.submit */
json_t *w19_hostedroom_e90a2b_submit_guard(const char *profile, const char *source,
                                           const char *route_profile, const char *session_id,
                                           json_t *member_id, const char *route_member_id,
                                           json_t *self_session_id, int attachments_present);
/* PoP: w19_hostedroom_e90a2b_interrupt_decision @ tui_gateway/hosted_room_peer_transport.py:PeerHostedRoomTransport.interrupt */
json_t *w19_hostedroom_e90a2b_interrupt_decision(json_t *dispatch_task_id,
                                                 const char *expected_task_id,
                                                 json_t *task_id, json_t *execution_generation,
                                                 int has_stop_receipt);
/* PoP: w19_hostedroom_e90a2b_validate_source_event_seq @ tui_gateway/hosted_room_peer_transport.py:PeerHostedRoomTransport.__init__ */
json_t *w19_hostedroom_e90a2b_validate_source_event_seq(json_t *source_event_seq);
/* PoP: w19_hostedroom_e90a2b_bind_scope_args @ tui_gateway/hosted_room_peer_transport.py:PeerHostedRoomTransport.__init__ */
json_t *w19_hostedroom_e90a2b_bind_scope_args(json_t *binding, json_t *route);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_HOSTEDROOM_E90A2B_H */
