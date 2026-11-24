#ifndef NOC_W19_PROVIDER_3254DD_H
#define NOC_W19_PROVIDER_3254DD_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_provider_3254dd_coerce_domain_list @ plugins/web/xai/provider.py:_coerce_domain_list
 * Strip each string member, drop empties, cap at the xAI hard limit of 5. Never NULL. */
struct json_t *w19_provider_3254dd_coerce_domain_list(const struct json_t *value);

/* PoP: w19_provider_3254dd_coerce @ plugins/web/xai/provider.py:_coerce
 * cast is "int" / "float" / "str"; returns a fresh node, or a copy of `def` when the
 * cast raises TypeError/ValueError (Python semantics). */
struct json_t *w19_provider_3254dd_coerce(const char *cast, const struct json_t *value,
                                          const struct json_t *def);

/* PoP: w19_provider_3254dd_web_search_tool @ plugins/web/xai/provider.py:_web_search_tool
 * {"type":"web_search","filters":{...}} , {"type":"web_search"}, or json_null() when both
 * allowed_domains and excluded_domains are set (xAI rejects the combination). */
struct json_t *w19_provider_3254dd_web_search_tool(const struct json_t *cfg);

/* PoP: w19_provider_3254dd_build_prompt @ plugins/web/xai/provider.py:_build_prompt
 * Caller frees the returned malloc'd string. */
char *w19_provider_3254dd_build_prompt(const char *query, long long limit);

/* PoP: w19_provider_3254dd_collect_output_text @ plugins/web/xai/provider.py:_collect_output_text
 * [text_blocks, annotations] pair. */
struct json_t *w19_provider_3254dd_collect_output_text(const struct json_t *response_data);

/* PoP: w19_provider_3254dd_try_parse_json_results @ plugins/web/xai/provider.py:_try_parse_json_results
 * Array of rows, or json_null() for None. */
struct json_t *w19_provider_3254dd_try_parse_json_results(const char *text, long long limit);

/* PoP: w19_provider_3254dd_results_from_annotations @
 * plugins/web/xai/provider.py:_results_from_annotations */
struct json_t *w19_provider_3254dd_results_from_annotations(const struct json_t *annotations,
                                                            const char *joined_text,
                                                            long long limit);

/* PoP: w19_provider_3254dd_extract_results @ plugins/web/xai/provider.py:_extract_results */
struct json_t *w19_provider_3254dd_extract_results(const struct json_t *response_data,
                                                   long long limit);

/* PoP: w19_provider_3254dd_web_config @ plugins/web/xai/provider.py:_load_xai_web_config
 * cfg["web"]["xai"] when it is a dict, else {}. */
struct json_t *w19_provider_3254dd_web_config(const struct json_t *root);

/* PoP: w19_provider_3254dd_limit_clamp @ plugins/web/xai/provider.py:search
 * max(1, min(_coerce(int, limit, 5), 100)). */
struct json_t *w19_provider_3254dd_limit_clamp(const struct json_t *limit_value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_PROVIDER_3254DD_H */
