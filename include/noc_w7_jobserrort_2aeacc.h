#ifndef NOC_W7_JOBSERRORT_2AEACC_H
#define NOC_W7_JOBSERRORT_2AEACC_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: jobserrortext_sanitize @ cron/jobs_error_text.py:sanitize_persisted_error
 * Scrub and bound error text bound for jobs.json / executions.db. Falsy inputs
 * are returned unchanged; a redactor failure stores the fixed placeholder;
 * successful redaction is truncated to MAX_PERSISTED_ERROR_CHARS (2000).
 * The redactor is an injected deterministic collaborator (see the oracle). */
json_t *jobserrortext_sanitize(const json_t *error);

/* PoP: jobserrortext_scrub_retained @ cron/jobs_error_text.py:scrub_retained_stamps
 * Scrub the sticky stamps a run outcome keeps (last_failure / last_fire_error).
 * Python mutates the job in place; this C port returns the resulting job and
 * leaves it to the harness to emit. */
json_t *jobserrortext_scrub_retained(const json_t *job);

#ifdef __cplusplus
}
#endif
#endif
