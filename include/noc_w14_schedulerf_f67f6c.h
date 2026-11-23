#ifndef NOC_W14_SCHEDULERF_F67F6C_H
#define NOC_W14_SCHEDULERF_F67F6C_H

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_schedulerf_f67f6c_output_dir @ cron/scheduler_failure_copy.py:cron_output_dir_display */
char *w14_schedulerf_f67f6c_output_dir(const char *job_id, const char *home);

/* PoP: w14_schedulerf_f67f6c_generic @ cron/scheduler_failure_copy.py:generic_failure_notice */
char *w14_schedulerf_f67f6c_generic(const char *job_name, const char *job_id,
                                    const char *cleaned_error, const char *home);

/* PoP: w14_schedulerf_f67f6c_script_timeout @ cron/scheduler_failure_copy.py:script_timeout_notice */
char *w14_schedulerf_f67f6c_script_timeout(const char *job_name, const char *job_id, const char *home);

/* PoP: w14_schedulerf_f67f6c_inactivity @ cron/scheduler_failure_copy.py:inactivity_notice */
char *w14_schedulerf_f67f6c_inactivity(const char *job_name, const char *job_id, const char *home);

/* PoP: w14_schedulerf_f67f6c_blocked_config @ cron/scheduler_failure_copy.py:blocked_config_notice */
char *w14_schedulerf_f67f6c_blocked_config(const char *job_name, const char *reason);

/* PoP: w14_schedulerf_f67f6c_provider_notice @ cron/scheduler_failure_copy.py:provider_failure_notice
 * cause/relogin stand in for agent.turn_failure_copy.failure_cause_gloss and
 * relogin_command_hint (injected seams). NULL cause returns NULL, like Python. */
char *w14_schedulerf_f67f6c_provider_notice(const char *job_name, const char *job_id,
                                           const char *reason, const char *backup_provider_phrase,
                                           const char *cause, const char *relogin);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SCHEDULERF_F67F6C_H */
