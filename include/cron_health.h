/*
 * cron_health.h — Port of agent/monitoring/cron_health.py.
 *
 * Pure content-free cron telemetry projection: error classification,
 * job-key derivation, timestamp/duration parsing, execution-event projection.
 *
 * C11, no C++ — reuses libcrypto (sha256), hermes_regex (POSIX re),
 * hermes_time (now), libdatetime (iso parse).
 */
#ifndef CRON_HEALTH_H
#define CRON_HEALTH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "hermes_json.h"

#define cron_now_ns() ({ struct timespec _ts; clock_gettime(CLOCK_REALTIME, &_ts); \
                         (int64_t)_ts.tv_sec * 1000000000 + _ts.tv_nsec; })

#ifdef __cplusplus
extern "C" {
#endif

struct cron_execution_event {
    char  *status;       /* owned */
    char  *job_key;      /* owned — "sha256:<24 hex chars>" */
    char  *source;       /* owned */
    int    duration_ms;  /* -1 when unknown (mirrors Optional[int]) */
    char  *delivery_outcome; /* owned, NULL when None */
    char  *error_class;  /* owned, NULL when None */
    int64_t ts_ns;       /* epoch nanoseconds */
};

/* PoP: _job_key @ agent/monitoring/cron_health.py:_job_key
 * sha256(job_id)[:24] prefixed "sha256:". Returns malloc'd string (caller free). */
char *cron_job_key(const char *raw);

/* PoP: classify_cron_error @ agent/monitoring/cron_health.py:classify_cron_error
 * Reduce free-form error text to a bounded class. String literal; never NULL. */
const char *classify_cron_error(const char *raw);

/* PoP: project_execution_event @ agent/monitoring/cron_health.py:project_execution_event
 * Build a CronExecutionEvent from a cron run record dict + optional delivery outcome.
 * Returns a populated event_t (caller frees via cron_execution_event_free). */
struct cron_execution_event project_execution_event(
    const json_t *record, const char *delivery_outcome);

void cron_execution_event_free(struct cron_execution_event *ev);

/* PoP: _duration_ms @ agent/monitoring/cron_health.py:_duration_ms
 * Best-effort duration in ms from a record dict's started/claimed/finished times. */
int cron_duration_ms(const json_t *record);

/* PoP: _parse_time @ agent/monitoring/cron_health.py:_parse_time
 * Parse an ISO-8601 timestamp to epoch seconds; returns -1 on failure. */
double cron_parse_time(const char *raw);

#ifdef __cplusplus
}
#endif
#endif /* CRON_HEALTH_H */
