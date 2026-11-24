#ifndef NOC_W18_BACKUPCRON_66B2EF_H
#define NOC_W18_BACKUPCRON_66B2EF_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_backupcron_66b2ef_prompt_degraded @ hermes_cli/backup_cron_prompts.py:_prompt_degraded */
int w18_backupcron_66b2ef_prompt_degraded(const struct json_t *job);

/* PoP: w18_backupcron_66b2ef_cron_jobs_list @ hermes_cli/backup_cron_prompts.py:_cron_jobs_list */
struct json_t *w18_backupcron_66b2ef_cron_jobs_list(const struct json_t *doc);

/* PoP: w18_backupcron_66b2ef_snapshot_prompts_by_id @ hermes_cli/backup_cron_prompts.py:_snapshot_prompts_by_id */
struct json_t *w18_backupcron_66b2ef_snapshot_prompts_by_id(const struct json_t *snap_doc);

/* PoP: w18_backupcron_66b2ef_restore_degraded_prompts @ hermes_cli/backup_cron_prompts.py:_restore_degraded_prompts
 * Returns {"ids":[...],"jobs":<mutated copy>}; input is never mutated. */
struct json_t *w18_backupcron_66b2ef_restore_degraded_prompts(const struct json_t *jobs,
                                                              const struct json_t *snap_prompts,
                                                              int apply);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_BACKUPCRON_66B2EF_H */
