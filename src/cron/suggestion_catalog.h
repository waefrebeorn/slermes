/* Slermes C11 port of cron/suggestion_catalog.py
 *
 * Curated catalog of starter cron-job suggestions — the "catalog" source of the
 * unified suggestion surface. Each entry is a ready-to-run create_job spec
 * offered as a suggestion; nothing auto-schedules.
 *
 * PoP: exact port. Semantic source of truth = cron/suggestion_catalog.py.
 */
#ifndef SLERMES_SUGGESTION_CATALOG_H
#define SLERMES_SUGGESTION_CATALOG_H

#include "hermes_json.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A curated starter automation offered as a suggestion. */
typedef struct {
    const char *key;          /* stable dedup key */
    const char *title;
    const char *description;
    const char *prompt;       /* job_spec.prompt */
    const char *schedule;     /* job_spec.schedule */
    const char *name;         /* job_spec.name */
    const char *deliver;      /* job_spec.deliver */
} catalog_entry_t;

/* The curated CATALOG. `count` receives the entry count. */
const catalog_entry_t *suggestion_catalog(size_t *count);

/* classify_items_script_path: absolute path to the urgency classifier script
 * shipped with cron/. Writes into `buf` (bufsz) and returns buf. Resolves
 * <install-root>/cron/scripts/classify_items.py, where install-root is
 * $HERMES_ROOT if set, else the current working directory's parent context. */
char *classify_items_script_path(char *buf, size_t bufsz);

/* add_fn signature matching cron_sugg_add: returns a new record (json_t*, owned
 * by caller of seed) or NULL when the store skips the entry. */
typedef json_t *(*catalog_add_fn)(const char *title, const char *description,
                                  const char *source, json_t *job_spec,
                                  const char *dedup_key);

/* seed_catalog_suggestions: register catalog entries as pending suggestions.
 *   add_fn : injectable store hook; pass NULL to use cron_sugg_add.
 *   keys   : NULL-terminated array of dedup keys to restrict to; NULL = all.
 * Returns a json_t array of the records actually created (caller frees). */
json_t *seed_catalog_suggestions(catalog_add_fn add_fn, const char *const *keys);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_SUGGESTION_CATALOG_H */
