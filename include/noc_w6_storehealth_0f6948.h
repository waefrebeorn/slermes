#ifndef NOC_W6_STOREHEALTH_0F6948_H
#define NOC_W6_STOREHEALTH_0F6948_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_storehealth_0f6948_probe_interval @ cron/store_health.py:PROBE_INTERVAL_SECONDS */
double w6_storehealth_0f6948_probe_interval(void);

/* PoP: w6_storehealth_0f6948_fix_hint @ cron/store_health.py:FIX_HINT
 * FIX_HINT.format(store=...). Caller frees. */
char *w6_storehealth_0f6948_fix_hint(const char *store);

/* PoP: w6_storehealth_0f6948_describe_error @ cron/store_health.py:describe_error
 * errno.errorcode.get(errno or 0, "") + ": " + (strerror or str(exc)); the
 * bare text is returned when the code is unknown. Caller frees. */
char *w6_storehealth_0f6948_describe_error(int errnum, const char *text);

/* PoP: w6_storehealth_0f6948_unwritable_errno @ cron/store_health.py:UNWRITABLE_ERRNOS
 * Membership test for the store-degrading errno set. */
int w6_storehealth_0f6948_unwritable_errno(int errnum);

/* PoP: w6_storehealth_0f6948_outage_covers @ cron/store_health.py:outage_covers
 * `records` is a JSON array: element 0 is the current degraded record, element
 * 1 (if present) the just-ended recovered record.  Each has "since" and an
 * optional "recovered_at" (null when still degraded). Returns 1 when a run due
 * at `due_at` fell inside either outage window. */
int w6_storehealth_0f6948_outage_covers(const struct json_t *records,
                                        double due_at, double grace);

/* PoP: w6_storehealth_0f6948_run_keys @ cron/store_health.py:_run_keys
 * Distinct (id, next_run_at) pairs as a NEW JSON array of 2-element arrays,
 * sorted for deterministic output (Python returns a set). Caller frees. */
struct json_t *w6_storehealth_0f6948_run_keys(const struct json_t *jobs);

/* PoP: w6_storehealth_0f6948_skipped_runs @ cron/store_health.py:StoreDegraded.skipped_runs
 * len() of the distinct skipped-run set, supplied as a JSON array. */
int w6_storehealth_0f6948_skipped_runs(const struct json_t *skipped);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W6_STOREHEALTH_0F6948_H */
