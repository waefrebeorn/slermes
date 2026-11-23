#ifndef NOC_JOBSTIME_H
#define NOC_JOBSTIME_H

/* C port of cron/jobs_time.py. All exports jobstime_-prefixed.
 *
 * Value-kind codes used by the scalar helpers (JSON numbers are doubles in the
 * harness, so 64-bit ints arrive as decimal strings + strtoll per PORT-KIT):
 *   0 = None, 1 = bool, 2 = int, 3 = float, 4 = str
 * A status of 0 means "None result", 1 means "value in out/char*", 2 means
 * "ValueError would be raised".
 */
int  jobstime_normalize_grace_minutes(int kind, long long ival, double dval,
                                      const char *sval, long long *out);
int  jobstime_normalize_timezone(int kind, const char *sval, char **out);
int  jobstime_zone_for_job(int is_dict, const char *val_str, char **out);
int  jobstime_grace_seconds_override(int kind, long long ival, double dval,
                                     const char *sval, long long *out);
void jobstime_prune_inherited_knobs(void *record);

long long   jobstime_min_grace_minutes(void);
long long   jobstime_max_grace_minutes(void);
const char *jobstime_timezone_key(void);
const char *jobstime_grace_key(void);

#endif
