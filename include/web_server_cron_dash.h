/*
 * web_server_cron_dash.h — dashboard cron adapter layer (faithful C11 port
 * of the _normalize_dashboard_cron_* / _cron_profile_* / _annotate_cron_job
 * cluster in hermes_cli/web_server.py).
 *
 * Error contract: functions that can raise HTTPException in Python return
 * NULL/false and fill (*status, *detail); detail is malloc'd.
 */
#ifndef WEB_SERVER_CRON_DASH_H
#define WEB_SERVER_CRON_DASH_H

#include <stdbool.h>

#include "libjson/json.h"

/* _normalize_dashboard_cron_script: validate a script path against the
 * profile sandbox <profile_home>/scripts. Returns malloc'd relative path,
 * or NULL: *status==0 → value empty (Python returns None);
 * *status==400 → HTTPException with *detail. */
char *ws_cron_normalize_script(const char *value, const char *profile_home,
                               int *status, char **detail);

/* _validate_dashboard_cron_effective_job: true when valid, else false with
 * 400 + detail. */
bool ws_cron_validate_effective_job(const json_t *job, int *status,
                                    char **detail);

/* _normalize_dashboard_cron_updates: returns a NEW normalized dict, or NULL
 * on script-validation failure (status/detail set). */
json_t *ws_cron_normalize_updates(const json_t *updates,
                                  const char *profile_home, int *status,
                                  char **detail);

/* _cron_default_profile: malloc'd profile name. */
char *ws_cron_default_profile(void);

/* _cron_profile_home: resolve profile query → (name, home). On success
 * returns true and sets malloc'd *name_out / *home_out. On failure returns
 * false with status/detail. `profile` may be NULL/empty. */
bool ws_cron_profile_home(const char *profile, char **name_out,
                          char **home_out, int *status, char **detail);

/* _annotate_cron_job: returns a NEW annotated copy of job. */
json_t *ws_cron_annotate_job(const json_t *job, const char *profile,
                             const char *home);

#endif /* WEB_SERVER_CRON_DASH_H */
