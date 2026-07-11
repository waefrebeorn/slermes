#ifndef CRON_SUGGESTIONS_H
#define CRON_SUGGESTIONS_H

#include "hermes_json.h"
#include <stdbool.h>

/* Port of Python: cron/suggestions.py
 * JSON file store for one-tap cron job suggestions.
 * Caller owns any json_t* returned (free with json_free). */

/* _secure_file */
void     cron_sugg_secure_file(const char *path);
/* _ensure_dir */
void     cron_sugg_ensure_dir(void);
/* _load_raw -> {"suggestions":[...], "updated_at":iso} */
json_t  *cron_sugg_load_raw(void);
/* _save_raw (consumes a list of suggestion objects) */
bool     cron_sugg_save_raw(json_t *suggestions_list);
/* load_suggestions -> array of all records */
json_t  *cron_sugg_load_suggestions(void);
/* list_pending -> array of pending records */
json_t  *cron_sugg_list_pending(void);
/* add_suggestion -> new record or NULL if skipped */
json_t  *cron_sugg_add(const char *title, const char *description,
                         const char *source, json_t *job_spec,
                         const char *dedup_key);
/* get_suggestion -> record by id / 1-based pending index / title, or NULL */
json_t  *cron_sugg_get(const char *ref);
/* _set_status -> true if a record was changed */
bool     cron_sugg_set_status(const char *suggestion_id, const char *status);
/* dismiss_suggestion -> true if dismissed */
bool     cron_sugg_dismiss(const char *ref);
/* accept_suggestion -> created job spec (json_t*) or NULL */
json_t  *cron_sugg_accept(const char *ref, json_t *origin);
/* clear_resolved -> count of accepted records removed */
int      cron_sugg_clear_resolved(void);

#endif /* CRON_SUGGESTIONS_H */
