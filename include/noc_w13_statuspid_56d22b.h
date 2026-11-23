#ifndef NOC_W13_STATUSPID_56D22B_H
#define NOC_W13_STATUSPID_56D22B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_statuspid_56d22b_previous @ gateway/status_pid_records.py:names_a_previous_gateway
 * ctx fields: pid_record, lock_record, live_pids (array of ints),
 * live_start_times (object: decimal pid -> start time). Returns whether the
 * PID file names a different, gone (or recycled) gateway instance. */
struct json_t *w13_statuspid_56d22b_previous(const struct json_t *ctx);

/* PoP: w13_statuspid_56d22b_strict @ gateway/status_pid_records.py:strict_identity_records
 * ctx fields: pid_record (the _read_pid_record seam result), lock_record,
 * pid_exists, live_pids, live_start_times. Returns the JSON array of records
 * the strict identity check must agree on. */
struct json_t *w13_statuspid_56d22b_strict(const struct json_t *ctx);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_STATUSPID_56D22B_H */
