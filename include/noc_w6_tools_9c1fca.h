#ifndef NOC_W6_TOOLS_9C1FCA_H
#define NOC_W6_TOOLS_9C1FCA_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_tools_9c1fca_tool_kind @ acp_adapter/tools.py:get_tool_kind */
const char *w6_tools_9c1fca_tool_kind(const char *tool_name);

/* PoP: w6_tools_9c1fca_plural @ acp_adapter/tools.py:_plural (count is a JSON number) */
struct json_t *w6_tools_9c1fca_plural(const struct json_t *count, const char *word, const char *suffix);

/* PoP: w6_tools_9c1fca_nonempty @ acp_adapter/tools.py:_nonempty */
struct json_t *w6_tools_9c1fca_nonempty(const struct json_t *result);

/* PoP: w6_tools_9c1fca_failure @ acp_adapter/tools.py:_failure */
struct json_t *w6_tools_9c1fca_failure(const struct json_t *data, const char *prefix);

/* PoP: w6_tools_9c1fca_truncate_text @ acp_adapter/tools.py:_truncate_text */
struct json_t *w6_tools_9c1fca_truncate_text(const char *text, long long limit);

/* PoP: w6_tools_9c1fca_fenced_text @ acp_adapter/tools.py:_fenced_text */
struct json_t *w6_tools_9c1fca_fenced_text(const char *text, const char *language);

/* PoP: w6_tools_9c1fca_json_loads_maybe @ acp_adapter/tools.py:_json_loads_maybe */
struct json_t *w6_tools_9c1fca_json_loads_maybe(const struct json_t *value);

/* PoP: w6_tools_9c1fca_coerce_args @ acp_adapter/tools.py:coerce_tool_args */
struct json_t *w6_tools_9c1fca_coerce_args(const struct json_t *raw);

/* PoP: w6_tools_9c1fca_more @ acp_adapter/tools.py:_more (items is a JSON array) */
struct json_t *w6_tools_9c1fca_more(const struct json_t *items, long long shown, const char *unit);

/* PoP: w6_tools_9c1fca_arg @ acp_adapter/tools.py:_arg (keys is a JSON array of strings) */
struct json_t *w6_tools_9c1fca_arg(const struct json_t *args, const struct json_t *keys,
                                   const char *default_value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W6_TOOLS_9C1FCA_H */
