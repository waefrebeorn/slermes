#ifndef NOC_W20_HOSTEDROOM_8052FE_H
#define NOC_W20_HOSTEDROOM_8052FE_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of tui_gateway/hosted_room_router_call.py (gate w20_hostedroom_8052fe).
 *
 * The router call behind `routing: auto` rooms: the roster/prompt inputs, the
 * recent-message digest and the one-in-flight-per-(room,message) claim gate.
 * The LLM call, the thread spawn and the store's claim rows are seams; the
 * decisions they feed are ported.
 */

/* PoP: w20_hostedroom_8052fe_member_roster @ tui_gateway/hosted_room_router_call.py:_member_roster
 * One {"handle","name","description"} per member. `homes` maps a profile name
 * to the description its `read_profile_meta` returned (absent -> no meta). */
json_t *w20_hostedroom_8052fe_member_roster(const json_t *room, const json_t *homes);

/* PoP: w20_hostedroom_8052fe_recent_messages @ tui_gateway/hosted_room_router_call.py:_recent_messages
 * The last few user/member messages before the routed one, as
 * [{"author","text"}]; `events` is the page `hosted_rooms.read_events`
 * returned and `members` the room's member list. */
json_t *w20_hostedroom_8052fe_recent_messages(const json_t *events, const json_t *members,
                                              const json_t *before_seq);

/* PoP: w20_hostedroom_8052fe_start @ tui_gateway/hosted_room_router_call.py:RouterCalls.start
 * The claim gate, run synchronously: `already_inflight` is `key in
 * self._inflight`, `claim` is the store's claim answer (true/false, or
 * {"__raise__": {"type","message"}}), `call_result` what the router answered,
 * `record_raises`/`release_raises` script the two failure paths, `now` the
 * clock reading. Answer:
 *   {"started":bool,"raised":bool,"claim_called":bool,
 *    "expires_at":n,"now":n,"record_called":bool,"recorded":...,
 *    "record_ok":bool,"release_called":bool,"release_ok":bool,
 *    "inflight_after":bool,"logs":[...]} */
json_t *w20_hostedroom_8052fe_start(const char *room_id, const char *discussion_event_id,
                                    int already_inflight, const json_t *claim,
                                    const json_t *call_result, int record_raises,
                                    int release_raises, const json_t *now);

/* PoP: w20_hostedroom_8052fe_handles @ tui_gateway/hosted_room_router_call.py:ask_router
 * `{str(m["handle"]).casefold(): str(m["member_id"])}` as [[handle, id]] in
 * member order. ASCII case folding (the handles this seam carries are ASCII). */
json_t *w20_hostedroom_8052fe_handles(const json_t *members);

/* PoP: w20_hostedroom_8052fe_claim_ttl @ tui_gateway/hosted_room_router_call.py:CLAIM_TTL_SECONDS */
json_t *w20_hostedroom_8052fe_claim_ttl(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_HOSTEDROOM_8052FE_H */
