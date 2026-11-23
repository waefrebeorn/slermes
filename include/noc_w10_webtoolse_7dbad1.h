#ifndef NOC_W10_WEBTOOLSE_7DBAD1_H
#define NOC_W10_WEBTOOLSE_7DBAD1_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_webtoolse_7dbad1_web_extract_url @ tools/web_tools_extract.py:_web_extract_url
 * Returns a malloc'd stripped URL string, or NULL for Python None; caller frees. */
char *w10_webtoolse_7dbad1_web_extract_url(const struct json_t *value);
/* PoP: w10_webtoolse_7dbad1_disabled_plugin_error @ tools/web_tools_extract.py:_disabled_plugin_error
 * Returns a malloc'd error string; caller frees. */
char *w10_webtoolse_7dbad1_disabled_plugin_error(const char *capability, const char *disabled_key);
/* PoP: w10_webtoolse_7dbad1_result_entry @ tools/web_tools_extract.py:_result_entry
 * {"url":..., "title":"", "content":"", "error":...}; caller frees. */
struct json_t *w10_webtoolse_7dbad1_result_entry(const char *url, const struct json_t *error);
/* PoP: w10_webtoolse_7dbad1_extract_error_json @ tools/web_tools_extract.py:_extract_error_json
 * json.dumps({"success": False, "error": error}, ensure_ascii=False) as a string; caller frees. */
char *w10_webtoolse_7dbad1_extract_error_json(const char *error);
/* PoP: w10_webtoolse_7dbad1_merge_in_order @ tools/web_tools_extract.py:_merge_in_order
 * Rebuild a total-long result list from fixed positions + fetched results; caller frees. */
struct json_t *w10_webtoolse_7dbad1_merge_in_order(long total, const struct json_t *fixed,
        const struct json_t *fetch_positions, const struct json_t *fetch_urls,
        const struct json_t *results);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_WEBTOOLSE_7DBAD1_H */
