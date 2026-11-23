#ifndef NOC_W10_SKILLSHUB_742005_H
#define NOC_W10_SKILLSHUB_742005_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pure helpers of tools/skills_hub_github.py. Every symbol is prefixed with
 * the gate id. Strings that may be None are returned as JSON null. */
json_t *w10_skillshub_742005_github_provider_for(const char *repo);
json_t *w10_skillshub_742005_provider_filter_of(const char *source_filter);
json_t *w10_skillshub_742005_tap_cache_key(const char *repo, const char *path, const char *bucket);
json_t *w10_skillshub_742005_split_repo_id(const char *identifier);
json_t *w10_skillshub_742005_skill_file_path(const char *skill_path, const char *filename);
json_t *w10_skillshub_742005_skip_bundle_file(const char *rel_path);
json_t *w10_skillshub_742005_is_rate_limit_response(int status_code, const json_t *headers);
json_t *w10_skillshub_742005_tree_members(const json_t *entries, const char *prefix);
json_t *w10_skillshub_742005_parse_skillsh_groupings(const char *content);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_SKILLSHUB_742005_H */
