#ifndef NOC_W16_DELEGATETO_DD6882_H
#define NOC_W16_DELEGATETO_DD6882_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_delegateto_dd6882_normalize_event @ tools/delegate_tool_progress.py:_normalize_event */
struct json_t *w16_delegateto_dd6882_normalize_event(const char *event);
/* PoP: w16_delegateto_dd6882_format_duration @ tools/delegate_tool_progress.py:_format_duration */
struct json_t *w16_delegateto_dd6882_format_duration(const struct json_t *seconds);
/* PoP: w16_delegateto_dd6882_short @ tools/delegate_tool_progress.py:_short */
struct json_t *w16_delegateto_dd6882_short(const char *text, long long n);
/* PoP: w16_delegateto_dd6882_child_task_message @ tools/delegate_tool_progress.py:child_task_message */
struct json_t *w16_delegateto_dd6882_child_task_message(const char *goal, const struct json_t *context);
/* PoP: w16_delegateto_dd6882_child_system_prompt @ tools/delegate_tool_progress.py:_build_child_system_prompt */
struct json_t *w16_delegateto_dd6882_child_system_prompt(const struct json_t *opts);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_DELEGATETO_DD6882_H */
