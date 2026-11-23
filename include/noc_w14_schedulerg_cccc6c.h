#ifndef NOC_W14_SCHEDULERG_CCCC6C_H
#define NOC_W14_SCHEDULERG_CCCC6C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_schedulerg_cccc6c_gateway_unavailable @ cron/scheduler_gateway_gate.py:_gateway_unavailable
 * env_value and the discovered {state, reason_code} are injected seams (no
 * process-environment mutation). Returns a static hint or NULL. */
const char *w14_schedulerg_cccc6c_gateway_unavailable(const char *env_value,
                                                     const char *state,
                                                     const char *reason_code);

/* PoP: w14_schedulerg_cccc6c_refuse_jobs @ cron/scheduler_gateway_gate.py:refuse_agent_jobs_without_gateway
 * Returns {"jobs": [...], "refused_ids": [...], "detail": str|null}; caller frees. */
struct json_t *w14_schedulerg_cccc6c_refuse_jobs(const struct json_t *due_jobs,
                                                const char *env_value,
                                                const char *state,
                                                const char *reason_code);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SCHEDULERG_CCCC6C_H */
