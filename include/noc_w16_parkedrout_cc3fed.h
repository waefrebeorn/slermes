#ifndef NOC_W16_PARKEDROUT_CC3FED_H
#define NOC_W16_PARKEDROUT_CC3FED_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w16_parkedrout_cc3fed_refused_info @ gateway/parked_route_refusal.py:ParkedRouteRefused */
struct json_t *w16_parkedrout_cc3fed_refused_info(void);
/* PoP: w16_parkedrout_cc3fed_profile_refusal @ gateway/parked_route_refusal.py:ParkedProfileRefusal */
struct json_t *w16_parkedrout_cc3fed_profile_refusal(const struct json_t *profile);
/* PoP: w16_parkedrout_cc3fed_notice_due @ gateway/parked_route_refusal.py:_notice_due */
struct json_t *w16_parkedrout_cc3fed_notice_due(const struct json_t *sent, const struct json_t *now,
                                              const struct json_t *profile, const struct json_t *platform,
                                              const struct json_t *chat_id);
/* PoP: w16_parkedrout_cc3fed_constants @ gateway/parked_route_refusal.py:PARKED_ROUTE_NOTICE */
struct json_t *w16_parkedrout_cc3fed_constants(void);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_PARKEDROUT_CC3FED_H */
