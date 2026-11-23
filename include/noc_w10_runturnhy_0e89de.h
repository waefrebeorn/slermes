#ifndef NOC_W10_RUNTRUNHY_0E89DE_H
#define NOC_W10_RUNTRUNHY_0E89DE_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_runturnhy_0e89de_read_config @ gateway/run_turn_hygiene.py:GatewayTurnHygieneMixin._hmwa_hygiene_read_config */
struct json_t *w10_runturnhy_0e89de_read_config(const struct json_t *data, const struct json_t *hs_in);
/* PoP: w10_runturnhy_0e89de_bound_payload @ gateway/run_turn_hygiene.py:GatewayTurnHygieneMixin._bound_hygiene_payload */
struct json_t *w10_runturnhy_0e89de_bound_payload(const struct json_t *history, long long limit);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_RUNTRUNHY_0E89DE_H */
