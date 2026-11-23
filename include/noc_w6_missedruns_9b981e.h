#ifndef NOC_W6_MISSEDRUNS_9B981E_H
#define NOC_W6_MISSEDRUNS_9B981E_H

/* Port of upstream Hermes cron/missed_runs.py::job_catches_up.
 *
 * The two module lookups the Python function performs lazily are injected as
 * explicit seam arguments:
 *   grace_override_present -- cron.jobs_time.grace_seconds_override(...) is not
 *                             None for this job (an explicit grace_minutes).
 *   config_catch_up        -- bool(cron.jobs._cron_config_number(
 *                             "catch_up_missed", True, lambda raw: raw is not False))
 * job_json is the job dict. Returns 1 (catch up) or 0 (skip).
 */
#include "hermes_json.h"

int w6_missedruns_9b981e_job_catches_up(const char *job_json,
                                        int grace_override_present,
                                        int config_catch_up);

#endif /* NOC_W6_MISSEDRUNS_9B981E_H */
