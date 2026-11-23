#ifndef NOC_SKILLSEARCH_H
#define NOC_SKILLSEARCH_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: skillsearch_provider_filter @ tools/skills_hub_search.py:_provider_filter_of */
struct json_t *skillsearch_provider_filter(const char *source_filter);
/* PoP: skillsearch_select_active @ tools/skills_hub_search.py:_select_active_sources */
struct json_t *skillsearch_select_active(const struct json_t *sources, const char *source_filter);
/* PoP: skillsearch_index_miss_fallback @ tools/skills_hub_search.py:_index_miss_fallback_sources */
struct json_t *skillsearch_index_miss_fallback(const struct json_t *all_ids, const struct json_t *active_ids,
                                               const char *query, const struct json_t *source_counts,
                                               const char *provider_filter);
#ifdef __cplusplus
}
#endif
#endif /* NOC_SKILLSEARCH_H */
