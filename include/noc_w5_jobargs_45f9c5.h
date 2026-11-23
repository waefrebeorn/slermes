#ifndef NOC_W5_JOBARGS_45F9C5_H
#define NOC_W5_JOBARGS_45F9C5_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_jobargs_45f9c5_split_monitor_arg @ cron/job_args.py:_split_monitor_arg
 * Returns [monitor_script, monitor_url] (null entries stay null). */
struct json_t *w5_jobargs_45f9c5_split_monitor_arg(const struct json_t *monitor,
                                                   const struct json_t *monitor_script,
                                                   const struct json_t *monitor_url);

/* PoP: w5_jobargs_45f9c5_repeat_display @ cron/job_args.py:_repeat_display */
struct json_t *w5_jobargs_45f9c5_repeat_display(const struct json_t *job);

/* PoP: w5_jobargs_45f9c5_clean_str_list @ cron/job_args.py:_clean_str_list */
struct json_t *w5_jobargs_45f9c5_clean_str_list(const struct json_t *items);

/* PoP: w5_jobargs_45f9c5_canonical_skills @ cron/job_args.py:_canonical_skills */
struct json_t *w5_jobargs_45f9c5_canonical_skills(const struct json_t *skill,
                                                  const struct json_t *skills);

/* PoP: w5_jobargs_45f9c5_normalize_optional @ cron/job_args.py:_normalize_optional_job_value */
struct json_t *w5_jobargs_45f9c5_normalize_optional(const struct json_t *value,
                                                    int strip_trailing_slash);

/* PoP: w5_jobargs_45f9c5_normalize_deliver @ cron/job_args.py:_normalize_deliver_param */
struct json_t *w5_jobargs_45f9c5_normalize_deliver(const struct json_t *value);

/* PoP: w5_jobargs_45f9c5_apply_continuity @ cron/job_args.py:_apply_continuity */
struct json_t *w5_jobargs_45f9c5_apply_continuity(const struct json_t *context_from,
                                                  int continuity);

/* PoP: w5_jobargs_45f9c5_base_url_refused @ cron/job_args.py:_base_url_refused */
struct json_t *w5_jobargs_45f9c5_base_url_refused(const char *bu, const char *prov, const char *why);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_JOBARGS_45F9C5_H */
