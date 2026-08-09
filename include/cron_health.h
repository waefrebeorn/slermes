/*
 * cron_health.h — Port of agent/monitoring/cron_health.py.
 *
 * Content-free cron telemetry projection: error classification, job-key
 * derivation, timestamp/duration parsing, execution-event projection.
 *
 * C11, no C++ — reuses libcrypto (sha256), hermes_regex (POSIX re),
 * libdatetime (iso parse).
 */
#ifndef CRON_HEALTH_H
#define CRON_HEALTH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "hermes_json.h"

/* Wall-clock epoch nanoseconds (CLOCK_REALTIME). */
#define cron_now_ns() ({ struct timespec _ts; clock_gettime(CLOCK_REALTIME, &_ts); \
                         (int64_t)_ts.tv_sec * 1000000000 + _ts.tv_nsec; })

#ifdef __cplusplus
extern "C" {
#endif

struct cron_execution_event {
    char  *status;            /* owned */
    char  *job_key;           /* owned — "sha256:<24 hex chars>" */
    char  *source;            /* owned */
    int    duration_ms;       /* -1 when unknown (mirrors Optional[int]) */
    char  *delivery_outcome;  /* owned, NULL when None */
    char  *error_class;       /* owned, NULL when None */
    int64_t ts_ns;            /* epoch nanoseconds */
};

/* PoP: _job_key @ agent/monitoring/cron_health.py:_job_key */
char *cron_job_key(const char *raw);

/* PoP: classify_cron_error @ agent/monitoring/cron_health.py:classify_cron_error */
const char *classify_cron_error(const char *raw);

/* PoP: project_execution_event @ agent/monitoring/cron_health.py:project_execution_event */
struct cron_execution_event project_execution_event(
    const json_t *record, const char *delivery_outcome);

void cron_execution_event_free(struct cron_execution_event *ev);

/* PoP: _duration_ms @ agent/monitoring/cron_health.py:_duration_ms */
int cron_duration_ms(const json_t *record);

/* PoP: _parse_time @ agent/monitoring/cron_health.py:_parse_time */
double cron_parse_time(const char *raw);

/* PoP: _now @ agent/monitoring/cron_health.py:_now */
double cron_now(void);

/* PoP: _is_overdue @ agent/monitoring/cron_health.py:_is_overdue
 * Returns true when a job's next_run_at is past the grace window. */
bool cron_is_overdue(const json_t *job, double now_epoch);

/* PoP: emit_execution_state @ agent/monitoring/cron_health.py:emit_execution_state
 * Best-effort lifecycle emit; terminal states synchronously cross the queue
 * barrier (flush).  Safe no-op when the monitoring emitter is unavailable. */
void cron_emit_execution_state(const json_t *record, const char *delivery_outcome);

/* PoP: build_cron_health_snapshot @ agent/monitoring/cron_health.py:build_cron_health_snapshot
 * Project runtime cron state into metrics. Returns a JSON object
 * ({"metrics": [...], "events": []}) matching the Python
 * CronHealthSnapshot.to_dict() shape. Caller frees (json_free). */
json_t *cron_build_health_snapshot(void);

/* Serialize a cron_execution_event to the Python to_dict() shape
 * {"event":"cron_execution", status, job_key, ...}. */
json_t *cron_execution_event_to_json(const struct cron_execution_event *ev);

#ifdef __cplusplus
}
#endif
#endif /* CRON_HEALTH_H */
