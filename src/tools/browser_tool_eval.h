#ifndef SLERMES_BROWSER_TOOL_EVAL_H
#define SLERMES_BROWSER_TOOL_EVAL_H

#include <stdbool.h>
#include <stdio.h>
#include <json.h>

typedef struct browser_tool_eval browser_tool_eval_t;

browser_tool_eval_t *browser_tool_eval_init(void);
void browser_tool_eval_cleanup(browser_tool_eval_t *s);

char *browser_redact_browser_output(const char *value_json);
char *browser_blocked_private_page_action(const char *effective_task_id, const char *action);
bool browser_eval_ssrf_guard_active(const char *effective_task_id);
char *browser_expression_targets_private_url(const char *expression);
char *browser_current_page_private_url(const char *effective_task_id);
bool browser_allow_unsafe_browser_evaluate(void);
char *browser_decode_js_string_literal(const char *literal);
char **browser_decoded_js_string_literals(const char *expression, int *out_count);
char *browser_sensitive_browser_eval_token_reason(const char *expression);
char *browser_risky_browser_eval_reason(const char *expression);
char *browser_enforce_browser_eval_policy(const char *expression);

#endif /* SLERMES_BROWSER_TOOL_EVAL_H */
