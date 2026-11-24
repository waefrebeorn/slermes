#ifndef NOC_W16_RUNTRURNPE_0C61CF_H
#define NOC_W16_RUNTRURNPE_0C61CF_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_runturnpe_0c61cf_add_failed_turn_notice @ gateway/run_turn_persistence.py:GatewayTurnPersistenceMixin._hmwa_add_failed_turn_notice */
struct json_t *w16_runturnpe_0c61cf_add_failed_turn_notice(const struct json_t *response,
                                                           const struct json_t *notice);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_RUNTRURNPE_0C61CF_H */
