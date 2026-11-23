#ifndef NOC_W7_JOBSINVARIA_BA9C41_H
#define NOC_W7_JOBSINVARIA_BA9C41_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: jobsinvariants_script_is_command_line @ cron/jobs_invariants.py:_script_is_command_line
 * A script with whitespace that resolves to no file is a command line, not a
 * path. `script_path` NULL models _resolve_script_path returning None. */
int jobsinvariants_script_is_command_line(const char *script, const char *script_path,
                                           int script_is_file);

/* PoP: jobsinvariants_mode_error @ cron/jobs_invariants.py:_validate_job_mode_invariants
 * Returns the ValueError message the live validator raises for the injected
 * inputs, or "" when the invariants hold. */
json_t *jobsinvariants_mode_error(const json_t *in);

/* PoP: jobsinvariants_schedule_error @ cron/jobs_invariants.py:validate_schedule_knobs
 * Returns the ValueError message from the two injected normalizers and the
 * schedule-kind check, or "" when the knobs are valid. */
json_t *jobsinvariants_schedule_error(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
