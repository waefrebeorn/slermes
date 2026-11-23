#ifndef NOC_W10_SKILLSHUB_6C5E0D_H
#define NOC_W10_SKILLSHUB_6C5E0D_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pure data-model / validator helpers of tools/skills_hub_models.py.
 * Path validators return the normalized string on success, or
 * {"__raise__":"ValueError"} on the Python ValueError. */
json_t *w10_skillshub_6c5e0d_matches_query(const char *query_lower, const json_t *fields);
json_t *w10_skillshub_6c5e0d_dedupe_by_trust(const json_t *results);
json_t *w10_skillshub_6c5e0d_normalize_bundle_path(const char *path_value, const char *field_name, int allow_nested);
json_t *w10_skillshub_6c5e0d_validate_skill_name(const char *name);
json_t *w10_skillshub_6c5e0d_validate_install_parent_path(const char *category);
json_t *w10_skillshub_6c5e0d_validate_bundle_rel_path(const char *rel_path);
json_t *w10_skillshub_6c5e0d_hermes_tags(const json_t *fm);
json_t *w10_skillshub_6c5e0d_query_is_concrete(const char *query);
json_t *w10_skillshub_6c5e0d_source_url_for_bundle(const json_t *bundle);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_SKILLSHUB_6C5E0D_H */
