#ifndef SLERMES_PORT_HERMES_STATE_KEYS_H
#define SLERMES_PORT_HERMES_STATE_KEYS_H

#include <hermes_json.h>
#include <stddef.h>

/* hermes_state_keys.py — the pure identity derivations the canonical store and
 * the gateway both need (stdlib-only on purpose, so the store never imports
 * gateway/). Plus the two durable-key builders from hermes_state_runtime.py
 * that sit on top of admission_fingerprint.
 *
 * Every function returns a malloc'd string the caller frees, or NULL when the
 * Python function returns None. */

char *hsk_cron_request_id(const char *job_id, const char *execution_id);

/* Half-open [lo, hi) bounds of every key starting with prefix. */
char *hsk_prefix_range(const char *prefix, char **hi);
char *hsk_cron_request_range(const char *job_id, char **hi);

/* (job_id, execution_id) of a cron fire's request id; 1 on success. */
int hsk_cron_run_from_request(const char *request_id, const char *job_id,
                              char **job_out, char **exec_out);

char *hsk_legacy_cron_run_job(const char *session_id);

char *hsk_admission_fingerprint(const char *canonical_target, json_t *payload);
char *hsk_local_identity(const char *profile_id, const char *principal_id,
                         const char *request_id);

char *hsk_profile_from_session_key_namespace(const char *namespace);
char *hsk_profile_from_session_key(const char *session_key);

/* hermes_state_runtime.py */
char *hsr_replacement_binding(const char *principal_id, const char *session_id,
                              const char *request_id);
int hsr_mutation_receipt_key(const char *principal_id, const char *session_id,
                             const char *request_id, const char *operation,
                             json_t *payload, json_t *expected_revision,
                             json_t *expected_generation, char **key_out,
                             char **digest_out);

#endif /* SLERMES_PORT_HERMES_STATE_KEYS_H */
