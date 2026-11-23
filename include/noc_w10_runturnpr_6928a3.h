#ifndef NOC_W10_RUNTRUNPR_6928A3_H
#define NOC_W10_RUNTRUNPR_6928A3_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_runturnpr_6928a3_session_override_snapshot @ gateway/run_turn_prepare.py:_session_override_snapshot */
struct json_t *w10_runturnpr_6928a3_session_override_snapshot(const struct json_t *override);
/* PoP: w10_runturnpr_6928a3_recover_or_remember_model @ gateway/run_turn_prepare.py:_recover_or_remember_model */
struct json_t *w10_runturnpr_6928a3_recover_or_remember_model(const char *skey, const struct json_t *model,
                                                             const struct json_t *state_last,
                                                             const struct json_t *star_last);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_RUNTRUNPR_6928A3_H */
