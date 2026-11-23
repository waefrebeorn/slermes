#ifndef NOC_W10_HOSTEDROOM_1CFF4F_H
#define NOC_W10_HOSTEDROOM_1CFF4F_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w10_hostedroom_1cff4f_parse_assignment @ gateway/hosted_room_lead.py:parse_assignment
 * Input keys: "text", "members" (array of {"handle","member_id"}). Returns the
 * JSON array of member ids named by the lead's answer, in roster order. Caller frees. */
struct json_t *w10_hostedroom_1cff4f_parse_assignment(const struct json_t *in);

/* PoP: w10_hostedroom_1cff4f_assignment @ gateway/hosted_room_lead.py:assignment
 * Input keys: "events", "discussion_event_id". Returns the id string of the
 * first matching room.assignment event, else JSON null. Caller frees. */
struct json_t *w10_hostedroom_1cff4f_assignment(const struct json_t *in);

/* PoP: w10_hostedroom_1cff4f_is_routing_task @ gateway/hosted_room_lead.py:is_routing_task
 * Input keys: "payload". Returns JSON true only when payload["lead_routing"] is exactly true. */
struct json_t *w10_hostedroom_1cff4f_is_routing_task(const struct json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W10_HOSTEDROOM_1CFF4F_H */
