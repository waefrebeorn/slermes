#ifndef NOC_W9_SCHEDULERLI_102FCC_H
#define NOC_W9_SCHEDULERLI_102FCC_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_schedulerli_102fcc_watchdog @ cron/scheduler_liveness.py:_inactivity_watchdog_loop
 * Drives the watchdog loop with an injected identity meter / fake stop / fake
 * future. Returns JSON true when limit seconds of inactivity were observed.
 * Caller frees the returned node. */
struct json_t *w9_schedulerli_102fcc_watchdog(const struct json_t *in);

/* PoP: w9_schedulerli_102fcc_stamper @ cron/scheduler_liveness.py:ExecutionProgressStamper.tick
 * Returns JSON true when a progress stamp would be written. Caller frees. */
struct json_t *w9_schedulerli_102fcc_stamper(const struct json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_SCHEDULERLI_102FCC_H */
