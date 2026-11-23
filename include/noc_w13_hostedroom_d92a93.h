#ifndef NOC_W13_HOSTEDROOM_D92A93_H
#define NOC_W13_HOSTEDROOM_D92A93_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w13_hostedroom_d92a93_routed_event_id @ gateway/hosted_room_router.py:routed_event_id
 * Returns "droute:<discussion_event_id>". */
struct json_t *w13_hostedroom_d92a93_routed_event_id(const struct json_t *discussion_event_id);
/* PoP: w13_hostedroom_d92a93_continue_count @ gateway/hosted_room_router.py:continue_count
 * events is a JSON array of {"kind","payload"}; returns the count of room.continued
 * events naming discussion_event_id. */
struct json_t *w13_hostedroom_d92a93_continue_count(const struct json_t *events,
                                                     const struct json_t *discussion_event_id);
/* PoP: w13_hostedroom_d92a93_routed_payload @ gateway/hosted_room_router.py:routed_payload
 * member_ids null/array; returns {discussion_event_id,thread_id,member_ids,everyone,fallback}. */
struct json_t *w13_hostedroom_d92a93_routed_payload(const struct json_t *discussion_event_id,
                                                     const struct json_t *thread_id,
                                                     const struct json_t *member_ids);
/* PoP: w13_hostedroom_d92a93_one_line @ gateway/hosted_room_router.py:_one_line
 * Whitespace-flattened text truncated to 600 chars with a "..." suffix. */
struct json_t *w13_hostedroom_d92a93_one_line(const struct json_t *text);
/* PoP: w13_hostedroom_d92a93_parse_router_answer @ gateway/hosted_room_router.py:parse_router_answer
 * Strict {"members":[...]|"everyone"} parser; handles maps casefolded handle -> member id.
 * Returns a JSON array of member ids (empty for "everyone"), or null when invalid. */
struct json_t *w13_hostedroom_d92a93_parse_router_answer(const struct json_t *text,
                                                          const struct json_t *handles);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_HOSTEDROOM_D92A93_H */
