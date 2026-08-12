/*
 * cron_monitor.h — Monitor-mode cron support (port of cron/monitor.py).
 *
 * A monitor job attaches a cheap "monitor source" (monitor_script or
 * monitor_url) to an ordinary LLM cron job. Each tick the source is run
 * FIRST and its exact output bytes are hashed against the last
 * agent-triggering tick:
 *
 *   unchanged → agent run suppressed (silent no_change tick)
 *   changed / first run → MONITOR CHANGE DETECTED context block injected
 *   source failure → ERROR (never a change), hash left untouched
 *
 * State: job["monitor_state"] in jobs.json (last_output_hash +
 * last_changed_at) + OUTPUT_DIR/<job_id>/monitor_last_output.txt
 * (kept only for diff rendering).
 */
#ifndef CRON_MONITOR_H
#define CRON_MONITOR_H

#include <stdbool.h>

#include "hermes_json.h"   /* json_t */

/* Result of one monitor-source evaluation (port of MonitorOutcome dataclass). */
typedef struct {
    bool    ok;
    bool    changed;
    bool    first_run;
    char   *context_block;  /* malloc'd; NULL when no agent run needed */
    char   *error;          /* malloc'd; NULL when ok */
} monitor_outcome_t;

/* Run the job's monitor source (script or URL). Returns (ok, output).
 * output is malloc'd; NULL on hard failure. */
bool cron_monitor_run_source(const char *script, const char *url,
                             const char *workdir, char **output);

/* PoP: check_monitor @ cron/monitor.py:check_monitor */
monitor_outcome_t cron_monitor_check(const char *job_id,
                                     const char *monitor_script,
                                     const char *monitor_url,
                                     const char *workdir,
                                     const char *last_hash,
                                     const char *last_output);

/* PoP: build_monitor_diff @ cron/monitor.py:build_monitor_diff */
char *cron_monitor_build_diff(const char *old_out, const char *new_out);

/* PoP: hash_monitor_output @ cron/monitor.py:hash_monitor_output */
char *cron_monitor_hash_output(const char *output);

/* PoP: job_has_monitor @ cron/monitor.py:job_has_monitor */
bool cron_monitor_has(const char *monitor_script, const char *monitor_url);

#endif /* CRON_MONITOR_H */
