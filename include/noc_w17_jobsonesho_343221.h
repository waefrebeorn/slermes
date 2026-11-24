#ifndef NOC_W17_JOBSONESHO_343221_H
#define NOC_W17_JOBSONESHO_343221_H

#include "hermes_json.h"

/* Both return the exact diagnostic text the live function hands to
 * cron.jobs.save_job_output (malloc'd caller-owned), or NULL when the live
 * function decides not to write (wedged with a recorded last_run_at). */
char *w17_jobsonesho_343221_write_wedged(json_t *job, const char *now_iso);
char *w17_jobsonesho_343221_write_missed(json_t *job, const char *next_run,
                                         const char *now_iso, long long grace);

#endif
