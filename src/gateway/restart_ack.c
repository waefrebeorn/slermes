/*
 * restart_ack.c — faithful C11 port of gateway/restart_ack.py.
 *
 * PoP: w7_restartack_3c2a4b_send_restart_ack @ gateway/restart_ack.py:send_restart_ack
 *
 * Upstream send_restart_ack is async and delegates error classification to
 * BasePlatformAdapter. The observable decision is:
 *   - a missing adapter or a LOCAL adapter is a no-op (viewer reads the RETURNED reply)
 *   - any exception from adapter.send -> not delivered (reply path is the fallback)
 *   - never_sent = result.retryable or _is_retryable_error(result.error)
 *   - delivered = bool(result.success) or (not never_sent and _is_timeout_error(error))
 * The adapter classification results and the await outcome are injected as
 * booleans by the harness, keeping the C function pure and the port faithful.
 */
#include "noc_w7_restartack_3c2a4b.h"

int w7_restartack_3c2a4b_send_restart_ack(int adapter_none, int platform_local,
        int send_raises, int result_success, int result_retryable,
        int error_retryable, int error_timeout) {
    /* if adapter is None or adapter.platform is Platform.LOCAL: return False */
    if (adapter_none || platform_local) return 0;
    /* any adapter failure: the reply path is the fallback, never a lost /restart */
    if (send_raises) return 0;
    /* never_sent = retryable or _is_retryable_error(error) */
    int never_sent = result_retryable || error_retryable;
    int delivered = result_success ? 1 : 0;
    if (!never_sent && error_timeout) delivered = 1;
    return delivered;
}
