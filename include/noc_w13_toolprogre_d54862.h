#ifndef NOC_W13_TOOLPROGRE_D54862_H
#define NOC_W13_TOOLPROGRE_D54862_H

struct json_t;
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: toolprogr_cap_tui_verbose_text @ tui_gateway/tool_progress.py:_cap_tui_verbose_text */
char *toolprogr_cap_tui_verbose_text(const char *text);

/* PoP: toolprogr_fmt_tool_duration @ tui_gateway/tool_progress.py:_fmt_tool_duration
 * seconds null -> "". */
char *toolprogr_fmt_tool_duration(const struct json_t *seconds);

/* PoP: toolprogr_count_list @ tui_gateway/tool_progress.py:_count_list
 * path is a JSON array of keys. Returns length of list at path, or null. */
struct json_t *toolprogr_count_list(const struct json_t *obj, const struct json_t *path);

/* PoP: toolprogr_tool_summary @ tui_gateway/tool_progress.py:_tool_summary
 * result is a JSON-encoded string. Returns string or null. */
struct json_t *toolprogr_tool_summary(const char *name, const char *result,
                                      const struct json_t *duration_s);

/* PoP: toolprogr_normalize_todo_state @ tui_gateway/tool_progress.py:_normalize_todo_state */
struct json_t *toolprogr_normalize_todo_state(const struct json_t *value);

/* PoP: toolprogr_tool_result_needs_user @ tui_gateway/tool_progress.py:_tool_result_needs_user */
struct json_t *toolprogr_tool_result_needs_user(const char *result);

/* PoP: toolprogr_int_or_skip @ tui_gateway/tool_progress.py:_int_or_skip */
struct json_t *toolprogr_int_or_skip(const struct json_t *v);

/* PoP: toolprogr_str_list @ tui_gateway/tool_progress.py:_str_list */
struct json_t *toolprogr_str_list(const struct json_t *v);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_TOOLPROGRE_D54862_H */
