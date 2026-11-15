/*
 * port_model_switch.h -- pure helpers ported from hermes_cli/model_switch.py.
 *
 * Model-id extraction, the non-agentic Hermes warning, the opaque-prefix
 * display normaliser, and the version/suffix sort key used to rank alias
 * candidates. Nothing here touches config, network or the filesystem.
 */
#ifndef PORT_MODEL_SWITCH_H
#define PORT_MODEL_SWITCH_H

#include <stddef.h>

#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _clean -- str(value or "").strip(); falsy values normalise to "". */
char *ms_clean(const json_t *value);

/* _declared_item_id -- "id" when a non-blank str, else "name" verbatim (any
 * JSON type; a non-string result is dropped by _declared_model_ids). */
const json_t *ms_declared_item_id(const json_t *item);

/* _declared_model_ids -- de-duplicated, stripped, order-preserving model IDs
 * from a str / {"id": {...}} mapping / list. Caller owns the array. */
json_t *ms_declared_model_ids(const json_t *value);

/* entry_models_discovered -- entry-level models_discovered, or the legacy
 * in-mapping __discovered_model_catalog__ sentinel. */
int ms_entry_models_discovered(const json_t *entry);

/* _models_config_is_allowlist -- an intentional ID allowlist, never a
 * per-model metadata mapping and never a Hermes-persisted catalog. */
int ms_models_config_is_allowlist(const json_t *value, int discovered);

/* format_model_for_display -- strips the opaque proxy prefix (display only). */
char *ms_format_model_for_display(const char *model_name);

/* is_nous_hermes_non_agentic -- real Nous Hermes 3/4 chat families only. */
int ms_is_nous_hermes_non_agentic(const char *model_name);

/* _check_hermes_model_warning -- the warning string, or "". */
const char *ms_check_hermes_model_warning(const char *model_name);

/* _split_version_suffix -- "v2.5-pro" -> ([2.5], "pro"); "-omni" -> ([], "omni").
 * `nums` receives up to `cap` components; `suffix` is the trailing buffer. */
void ms_split_version_suffix(const char *rest, double *nums, size_t *n_nums,
                             size_t cap, char *suffix, size_t suffix_cap);

/* _model_sort_key -- best-guess-first ranking tuple as a JSON array:
 * [negated version components..., suffix_rank, suffix, date0, date1].
 * Caller owns the array. */
json_t *ms_model_sort_key(const char *model_id, const char *prefix);

#ifdef __cplusplus
}
#endif

#endif /* PORT_MODEL_SWITCH_H */
