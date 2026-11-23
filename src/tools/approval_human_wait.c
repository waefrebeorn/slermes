/*
 * approval_human_wait.c — C home for upstream Hermes tools/approval_human_wait.py.
 * PoP: w12_approvalhu_7fc5ff_clamped_window_seconds @ tools/approval_human_wait.py:_clamped_window_seconds
 * PoP: w12_approvalhu_7fc5ff_ceiling @ tools/approval_human_wait.py:human_wait_ceiling
 */
#include "noc_w12_approvalhu_7fc5ff.h"

/* HUMAN_WAIT_MARGIN_S mirrored from tools/approval_human_wait.py. */
static const double W12_AHW_MARGIN_S = 60.0;

double w12_approvalhu_7fc5ff_clamped_window_seconds(double started, double now, double ceiling) {
    /* min(max(0.0, now - started), ceiling) */
    double elapsed = now - started;
    if (elapsed < 0.0) elapsed = 0.0;
    return (elapsed < ceiling) ? elapsed : ceiling;
}

double w12_approvalhu_7fc5ff_ceiling(double approval_timeout, double max_safe_timeout) {
    /* min(MAX_SAFE_TIMEOUT_S, float(timeout) + HUMAN_WAIT_MARGIN_S) */
    double value = approval_timeout + W12_AHW_MARGIN_S;
    return (value < max_safe_timeout) ? value : max_safe_timeout;
}
