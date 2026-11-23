/*
 * missed_runs.c -- faithful C11 port of upstream Hermes cron/missed_runs.py.
 *
 * PoP: w6_missedruns_9b981e_job_catches_up @ cron/missed_runs.py:job_catches_up
 */
#include "noc_w6_missedruns_9b981e.h"

#include <string.h>

/* PoP: w6_missedruns_9b981e_job_catches_up @ cron/missed_runs.py:job_catches_up */
int w6_missedruns_9b981e_job_catches_up(const char *job_json,
                                        int grace_override_present,
                                        int config_catch_up)
{
    json_t *job = job_json ? json_parse(job_json, NULL) : NULL;
    if (!job || job->type != JSON_OBJECT) {
        if (job) json_free(job);
        return 0;
    }

    /* value = job.get("catch_up"); if isinstance(value, bool): return value */
    json_t *cu = json_obj_get(job, "catch_up");
    if (cu && cu->type == JSON_BOOL) {
        int v = cu->bool_val ? 1 : 0;
        json_free(job);
        return v;
    }

    /* if (job.get("schedule") or {}).get("kind") == "once": return False */
    json_t *sched = json_obj_get(job, "schedule");
    if (sched && sched->type == JSON_OBJECT) {
        const char *kind = json_get_str(sched, "kind", NULL);
        if (kind && strcmp(kind, "once") == 0) {
            json_free(job);
            return 0;
        }
    }

    /* if grace_seconds_override(job.get("grace_minutes")) is not None: False */
    if (grace_override_present) {
        json_free(job);
        return 0;
    }

    /* return bool(_cron_config_number("catch_up_missed", True,
     *                                 lambda raw: raw is not False)) */
    json_free(job);
    return config_catch_up ? 1 : 0;
}
