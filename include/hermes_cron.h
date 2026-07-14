#ifndef HERMES_CRON_H
#define HERMES_CRON_H

/* Focused cron/scheduler header.
 * Extracted from the hermes.h umbrella so cron subsystem consumers
 * (cron/*.c, tools/cronjob.c, tools/curator_backup.c) no longer pull in
 * the god header. */

#include "hermes_core_types.h"

/* P169: SQLite job store */
typedef struct cron_sqlite_store_t cron_sqlite_store_t;
cron_sqlite_store_t *cron_sqlite_open(const char *path);
void cron_sqlite_close(cron_sqlite_store_t *store);
bool cron_sqlite_save_job(cron_sqlite_store_t *store, const char *name,
                           const char *schedule, const char *command,
                           bool active, int retry_count, int max_retries,
                           const char *chain_from, const char *template_name,
                           const char *script_type);
bool cron_sqlite_load_jobs(cron_sqlite_store_t *store);
char *cron_sqlite_list_to_json(cron_sqlite_store_t *store);
/* Inject a human-readable repeat_display field into a cron job JSON object.
 * Mirrors Python cronjob_tools._repeat_display(). */
void cron_inject_repeat_display(json_node_t *job);
bool cron_sqlite_delete_job(cron_sqlite_store_t *store, const char *name);
bool cron_sqlite_update_job(cron_sqlite_store_t *store, const char *name,
                             const char *field, const char *value);

/* P171: Job locking */
void  cron_lock_set_dir(const char *dir);
bool  cron_lock_acquire(const char *lock_name);
void  cron_lock_release(const char *lock_name);
bool  cron_lock_is_locked(const char *lock_name);
bool  cron_shutdown_requested(void);
void  cron_release_all_locks(void);

/* P172: Job retry */
bool cron_job_set_retry(const char *job_name, int max_retries, int backoff_sec);
int  cron_job_get_retry_count(const char *job_name);
int  cron_job_get_max_retries(const char *job_name);

/* P173: Job notification */
bool cron_notify_set_channel(const char *channel_id);
void cron_notify_set_send_fn(bool (*fn)(const char *platform, const char *chat_id, const char *text));
bool cron_notify_on_complete(const char *job_name, bool enabled);
bool cron_notify_on_failure(const char *job_name, bool enabled);

/* P174: Job chaining */
bool cron_chain_set_context(const char *job_name, const char *context_from);
const char *cron_chain_get_context(const char *job_name);
char *cron_chain_get_output(const char *job_name);
void cron_chain_store_output(const char *job_name, const char *output);

/* P176: Cron utility functions (port of cronjob_tools.py helpers) */
char **cron_canonical_skills(const char *skill, json_node_t *skills, size_t *out_count);
char  *normalize_optional_job_value(const char *value, bool strip_trailing_slash);
char  *normalize_deliver_param(json_node_t *deliver);
int    cron_parse_duration(const char *s);   /* Parse "30m", "2h", "1d" -> minutes */
bool   cron_secure_dir(const char *path);     /* chmod 0700 */
bool   cron_secure_file(const char *path);    /* chmod 0600 */
const char *cron_coerce_job_text(const char *value, const char *fallback); /* nullable string coercion */
const char *cron_schedule_display_for_job(json_node_t *job);              /* Extract display string from job schedule */
bool        cron_ensure_dirs(const char *hermes_home);                    /* mkdir -p ~/.hermes/cron/ + ~/.hermes/cron/output/ */
bool        cron_validate_job_id(const char *job_id, char *out_err);      /* Reject path-escape job IDs */
char       *cron_job_output_dir(const char *hermes_home, const char *job_id, char *out_err); /* Build safe output dir path */
char       *cron_normalize_workdir(const char *workdir, char *out_err);   /* Validate + resolve cron workdir path */
json_node_t *cron_apply_skill_fields(json_node_t *job);                  /* Align skill + skills fields in job JSON */

/* P175: Job templating */
bool cron_template_create(const char *name, const char *schedule,
                           const char *command, const char *params_json);
bool cron_template_instantiate(const char *template_name,
                                const char *params_json,
                                char *out_name, size_t out_name_sz,
                                char *out_schedule, size_t out_sched_sz,
                                char *out_command, size_t out_cmd_sz);

/* P176: Scheduler CLI */
char *cron_cmd_handler(const char *args_json, const char *task_id);

/* P177: Script-based jobs */
bool cron_script_set_interpreter(const char *job_name, const char *interpreter);
char *cron_run_script(const char *script_path, const char *interpreter,
                       const char *args, int *exit_code);

/* P178: Watchdog mode */
bool cron_watchdog_enable(void);
void cron_watchdog_disable(void);
bool cron_watchdog_is_active(void);
void cron_watchdog_ping(void);
int  cron_watchdog_check(time_t timeout_sec);

#endif /* HERMES_CRON_H */
