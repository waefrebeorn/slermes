#ifndef NOC_W10_SCHEDULERO_E0C314_H
#define NOC_W10_SCHEDULERO_E0C314_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w10_schedulero_e0c314_claim_dead @ cron/scheduler_ownership.py:_claim_owner_is_dead
 * Input keys: "by", "hostname", "live_pids". Both the host name and the
 * liveness set are injected by the fixture (the Python probes ambient state).
 * Returns JSON true only for a proven-dead local claim. Caller frees. */
struct json_t *w10_schedulero_e0c314_claim_dead(const struct json_t *in);

/* PoP: w10_schedulero_e0c314_record_serves @ cron/scheduler_ownership.py:record_serves_profile
 * Input keys: "record", "home", "same" (injected _same_hermes_home result),
 * "label" (injected _profile_label_for_home result), "process_home".
 * Returns JSON true when the record's served set covers the home. Caller frees. */
struct json_t *w10_schedulero_e0c314_record_serves(const struct json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W10_SCHEDULERO_E0C314_H */
