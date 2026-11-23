#ifndef NOC_W7_RESTARTACK_3C2A4B_H
#define NOC_W7_RESTARTACK_3C2A4B_H

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_restartack_3c2a4b_send_restart_ack @ gateway/restart_ack.py:send_restart_ack
 * Pure decision core of the async /restart acknowledgement.
 *
 * The delegated adapter classification (_is_retryable_error / _is_timeout_error)
 * and the await/timeout are injected as booleans from the fixture so the whole
 * function is deterministic and side-effect free. Returns 1 when the ack counts
 * as delivered (the reply path must NOT post a second copy), 0 otherwise. */
int w7_restartack_3c2a4b_send_restart_ack(int adapter_none, int platform_local,
        int send_raises, int result_success, int result_retryable,
        int error_retryable, int error_timeout);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_RESTARTACK_3C2A4B_H */
