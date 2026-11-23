#ifndef NOC_W12_APPROVALHU_7FC5FF_H
#define NOC_W12_APPROVALHU_7FC5FF_H

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_approvalhu_7fc5ff_clamped_window_seconds @ tools/approval_human_wait.py:_clamped_window_seconds */
double w12_approvalhu_7fc5ff_clamped_window_seconds(double started, double now, double ceiling);

/* PoP: w12_approvalhu_7fc5ff_ceiling @ tools/approval_human_wait.py:human_wait_ceiling
 * The config timeout and MAX_SAFE_TIMEOUT_S are injected as explicit args
 * (the live function reads them from caches/lazy imports). */
double w12_approvalhu_7fc5ff_ceiling(double approval_timeout, double max_safe_timeout);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_APPROVALHU_7FC5FF_H */
