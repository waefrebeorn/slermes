#ifndef NOC_W13_MAINTUILA_9FECF1_H
#define NOC_W13_MAINTUILA_9FECF1_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w13_maintuila_9fecf1_bypass @ hermes_cli/main_tui_launch.py:_bypass_chat_launch */
struct json_t *w13_maintuila_9fecf1_bypass(const struct json_t *args);
/* PoP: w13_maintuila_9fecf1_split_comma @ hermes_cli/main_tui_launch.py:_split_comma_items */
struct json_t *w13_maintuila_9fecf1_split_comma(const struct json_t *items, int split_non_str);
/* PoP: w13_maintuila_9fecf1_unforwarded_flags @ hermes_cli/main_tui_launch.py:_refuse_unforwarded_tui_flags */
struct json_t *w13_maintuila_9fecf1_unforwarded_flags(const struct json_t *args);
/* PoP: w13_maintuila_9fecf1_heap_mb @ hermes_cli/main_tui_launch.py:_resolve_tui_heap_mb
 * limit is the cgroup byte limit (0/negative == unconstrained). */
struct json_t *w13_maintuila_9fecf1_heap_mb(const struct json_t *limit, const struct json_t *default_mb);
/* PoP: w13_maintuila_9fecf1_normalize_toolsets @ hermes_cli/main_tui_launch.py:_normalize_tui_toolsets
 * Fallback branch (oneshot normalizer unavailable): split_non_str=False. */
struct json_t *w13_maintuila_9fecf1_normalize_toolsets(const struct json_t *toolsets);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_MAINTUILA_9FECF1_H */
