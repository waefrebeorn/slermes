/*
 * port_delegate_tool_ports.h — Public API for the port_delegate_tool_ports.c
 * gap closures (tools/delegate_tool.py helpers).
 *
 * Faithful C11 ports of _sanitize_tool_target, _summarize_tool_arguments,
 * _sanitize_tool_input_summary, _subagent_stop_tool_call_history,
 * _build_child_preserving_parent_tools, _parent_finalization_lock,
 * _finalize_child_results, _run_child_lifecycle, plus the existing
 * interrupt/list/delegate/_load_config ports.
 */
#ifndef PORT_DELEGATE_TOOL_PORTS_H
#define PORT_DELEGATE_TOOL_PORTS_H

#include <stdbool.h>
#include "hermes_json.h"

/* ---- Existing ports (unchanged) ---- */
int  dlg_interrupt_subagent(const char *subagent_id);
char *dlg_list_active_subagents(void);
char *dlg_delegate_task(const char *tasks_json);
char *dlg_load_config(const char *config_yaml);

/* ---- Gap ports ---- */

/* _sanitize_tool_target: strip/bound a side-effect target value.
 * URL keys get user:pass@ credentials stripped (netloc rebuilt to host[:port]).
 * Non-URL str values are truncated to 1024 chars. NULL/empty/non-str → NULL.
 * Returns a malloc'd string (caller frees). */
char *dlg_sanitize_tool_target(const char *key, const char *value);

/* List variant: value is a JSON array string. Returns NULL-terminated array
 * of cleaned strings (caller frees each + the array). *out_count set. */
char **dlg_sanitize_tool_target_list(const char *key, const char *json_array, int *out_count);

/* _summarize_tool_arguments: parse arguments JSON, return {"argument_keys":[...],
 * "targets":{...}} with sorted/truncated keys and sanitized targets. */
json_t *dlg_summarize_tool_arguments(const char *arguments);

/* _sanitize_tool_input_summary: normalize an already-built summary dict. */
json_t *dlg_sanitize_tool_input_summary(const char *summary_json);

/* _subagent_stop_tool_call_history: build metadata-only history from a
 * JSON tool_trace list. Returns a malloc'd json_t array. */
json_t *dlg_subagent_stop_tool_call_history(const char *tool_trace_json);

/* _build_child_preserving_parent_tools: snapshot parent toolset, build child,
 * restore parent set, stamp saved names on child. Returns 0 on success. */
int dlg_build_child_preserving_parent_tools(const char *task_json);

/* _parent_finalization_lock: return the lock that serializes lifecycle
 * side effects (process-global fallback). */
void *dlg_parent_finalization_lock(void *parent_agent);

/* _finalize_child_results: apply summary budget, cost rollup, and hook
 * invocation to a JSON results array. Returns 0 on success. */
int dlg_finalize_child_results(char *results_json,
                               const char *task_list_json,
                               const char *children_json,
                               const char *parent_session_id);

/* _run_child_lifecycle: run one child then finalize its results.
 * Returns a malloc'd JSON result string (caller frees). */
char *dlg_run_child_lifecycle(int task_index, const char *goal,
                              const char *child_json,
                              const char *parent_session_id);

#endif /* PORT_DELEGATE_TOOL_PORTS_H */
