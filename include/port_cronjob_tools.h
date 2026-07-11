#ifndef SLERMES_PORT_CRONJOB_TOOLS_H
#define SLERMES_PORT_CRONJOB_TOOLS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct json_t json_t;
typedef struct port_cronjob_tools_state port_cronjob_tools_state_t;

/* Lifecycle */
port_cronjob_tools_state_t *port_cronjob_tools_state_init(void);
void port_cronjob_tools_state_cleanup(port_cronjob_tools_state_t *state);

/* Public API */
char *check_invisible_unicode(const char *prompt);
json_t *strip_invisible_unicode(const char *prompt);
json_t *scan_cron_skill_assembled(const char *assembled);
json_t *origin_from_env(void);
char *local_delivery_notice(const json_t *job, const char *user_deliver);

/* Residual-façade closure (v558): previously-missing ports from
 * tools/cronjob_tools.py, now implemented / honestly demoted. */
bool cronjob_check_cronjob_requirements(void);
char *cronjob_validate_cron_script_path(const char *script);
json_t *cronjob_format_job(const json_t *job);
char *cronjob_validate_cron_base_url(const char *provider, const char *base_url);
void cronjob_notify_provider_jobs_changed_safe(void);
json_t *cronjob_execute_job_now(const json_t *job);
json_t *cronjob_dispatch(const json_t *args);

#endif /* SLERMES_PORT_CRONJOB_TOOLS_H */
