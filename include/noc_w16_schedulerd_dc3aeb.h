#ifndef NOC_W16_SCHEDULERD_DC3AEB_H
#define NOC_W16_SCHEDULERD_DC3AEB_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w16_schedulerd_dc3aeb_parse_room_deliver_token @ cron/scheduler_delivery_room.py:parse_room_deliver_token */
struct json_t *w16_schedulerd_dc3aeb_parse_room_deliver_token(const struct json_t *part);
/* PoP: w16_schedulerd_dc3aeb_resolve_room_target @ cron/scheduler_delivery_room.py:resolve_room_target */
struct json_t *w16_schedulerd_dc3aeb_resolve_room_target(const struct json_t *job, const struct json_t *address);
/* PoP: w16_schedulerd_dc3aeb_profile_hint @ cron/scheduler_delivery_room.py:_profile_hint */
struct json_t *w16_schedulerd_dc3aeb_profile_hint(const struct json_t *room_id, const struct json_t *profile);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_SCHEDULERD_DC3AEB_H */
