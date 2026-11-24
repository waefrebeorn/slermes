/*
 * noc_w25_schedulerw_4f882f.h - C surface for upstream
 * cron/scheduler_worktree_maintenance.py (gate w25_schedulerw_4f882f).
 *
 * One process ticks every served profile, so several profiles can reach the
 * same repo in one tick.  The process-wide semaphore, ``time.monotonic`` and
 * ``os.path.normcase(os.path.realpath(...))`` are seams; the values decide the
 * rest: which repos are due for a prune, what interval a claimant waits, and
 * the wait budget itself.
 */
#ifndef NOC_W25_SCHEDULERW_4F882F_H
#define NOC_W25_SCHEDULERW_4F882F_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* repos: ["<canonical repo key>", ...] (the path git resolves to, supplied as
 * input), interval_s, state: {"now": <stamp>, "last_prune": {key: <stamp>}} ->
 * the subset still due, in input order.  A repo with no stamp, or one stamped
 * at least interval_s ago, is due and re-stamped at ``now``; claiming is
 * visible to the rest of the same call (as the live dict update is). */
json_t *w25_schedulerw_4f882f_claim_due(const json_t *repos,
                                        const json_t *interval_s,
                                        const json_t *state);

/* claim_sweep_slot's wait: SWEEP_WAIT_SECONDS when wait_seconds is None,
 * otherwise the value passed through untouched (``is None``, not coercion). */
json_t *w25_schedulerw_4f882f_sweep_slot_timeout(const json_t *wait_seconds);

/* The sweep wait budget. */
json_t *w25_schedulerw_4f882f_constants(void);

#ifdef __cplusplus
}
#endif
#endif
