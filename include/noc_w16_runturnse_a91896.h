#ifndef NOC_W16_RUNTRURNSE_A91896_H
#define NOC_W16_RUNTRURNSE_A91896_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_runturnse_a91896_pending_setup_busy_notice @ gateway/run_turn_setup_guard.py:pending_setup_busy_notice */
struct json_t *w16_runturnse_a91896_pending_setup_busy_notice(int enabled, int allowed,
        double timeout, double started, double now, double busy_ack_ts,
        int agent_is_pending, int state_is_none);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_RUNTRURNSE_A91896_H */
