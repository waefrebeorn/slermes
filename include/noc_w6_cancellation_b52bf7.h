#ifndef NOC_W6_CANCELLATION_B52BF7_H
#define NOC_W6_CANCELLATION_B52BF7_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_cancellation_b52bf7_valid_execution_id @ cron/cancellation.py:_marker_path
 * The execution-id validation gate (strip; reject empty, separators, "."/".."). */
int w6_cancellation_b52bf7_valid_execution_id(const char *execution_id);

/* PoP: w6_cancellation_b52bf7_marker_name @ cron/cancellation.py:_marker_path
 * "<stripped id>.cancel", or NULL where Python raises ValueError. Caller frees. */
char *w6_cancellation_b52bf7_marker_name(const char *execution_id);

/* PoP: w6_cancellation_b52bf7_cancel_reason @ cron/cancellation.py:cancel_reason
 * getattr(reason) or the legacy default; NULL/empty -> default. Borrowed string. */
const char *w6_cancellation_b52bf7_cancel_reason(const char *reason);

/* PoP: w6_cancellation_b52bf7_cancel_run_none @ cron/cancellation.py:cancel_run
 * The no-run-in-flight result dict. Caller frees. */
struct json_t *w6_cancellation_b52bf7_cancel_run_none(void);

/* PoP: w6_cancellation_b52bf7_cancel_run @ cron/cancellation.py:cancel_run
 * The cancelled result dict for a claimed execution record. `reason` is an
 * injected seam for the request_cancel() side effect (which is filesystem
 * I/O in Python). Caller frees. */
struct json_t *w6_cancellation_b52bf7_cancel_run(const struct json_t *record,
                                                 const char *reason);

/* PoP: w6_cancellation_b52bf7_combined_is_set @ cron/cancellation.py:_CombinedCancelEvent.is_set
 * `events` is a JSON array of {"set":bool,"reason":str|null} descriptors. */
int w6_cancellation_b52bf7_combined_is_set(const struct json_t *events);

/* PoP: w6_cancellation_b52bf7_combined_reason @ cron/cancellation.py:_CombinedCancelEvent.reason
 * The first set event's reason, or NULL. Caller frees. */
char *w6_cancellation_b52bf7_combined_reason(const struct json_t *events);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W6_CANCELLATION_B52BF7_H */
