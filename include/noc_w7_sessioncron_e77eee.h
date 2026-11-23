#ifndef NOC_W7_SESSIONCRON_E77EEE_H
#define NOC_W7_SESSIONCRON_E77EEE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_sessioncron_e77eee_scheduler_execution_id @ gateway/session_cron.py:scheduler_execution_id
 * request_id.removeprefix("cron:<job_id>:") -> JSON string. */
struct json_t *w7_sessioncron_e77eee_scheduler_execution_id(const char *job_id,
                                                             const char *request_id);

/* PoP: w7_sessioncron_e77eee_validate_create_params @ gateway/session_cron.py:_create
 * The exact admission guard at the top of _create: keys == {job_id, request_id,
 * extra_prompt}, non-empty string job id, non-empty request id of length <= 256,
 * extra_prompt None or a string. */
int w7_sessioncron_e77eee_validate_create_params(const struct json_t *params);

/* PoP: w7_sessioncron_e77eee_no_owner_message @ gateway/session_cron.py:deliver_to_local_chat
 * The no-owner branch: "local chat <chat_id>: no gateway in this process owns its session". */
struct json_t *w7_sessioncron_e77eee_no_owner_message(const char *chat_id);

/* PoP: w7_sessioncron_e77eee_run_job_flags @ gateway/session_cron.py:execute
 * {key: job[key] for key in RUN_JOB_FLAGS if key in job}. */
struct json_t *w7_sessioncron_e77eee_run_job_flags(const struct json_t *job);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_SESSIONCRON_E77EEE_H */
