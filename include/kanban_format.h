/*
 * kanban_format.h — Pure kanban CLI formatting/parsing helpers
 * (faithful C11 port of hermes_cli/kanban.py helpers + kanban_db.Task shape).
 *
 * Self-contained, no network, no DB: the string/parse logic that turns a
 * task into a display line / dict, and parses the --workspace/--branch/
 * --duration flags. Mirrors the Python _fmt_ts / _fmt_task_line /
 * _task_to_dict / _run_state_kwargs / _parse_workspace_flag /
 * _parse_branch_flag / _parse_duration helpers.
 */

#ifndef KANBAN_FORMAT_H
#define KANBAN_FORMAT_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Task shape (matches kanban_db.Task fields surfaced by _task_to_dict). */
typedef struct {
    char *id;
    char *title;
    char *body;
    char *assignee;
    char *status;
    char *priority;
    char *tenant;
    char *workspace_kind;
    char *workspace_path;
    char *branch_name;
    char *project_id;
    char *created_by;
    long  created_at;
    long  started_at;
    long  completed_at;
    char *result;
    int   max_retries;
    char *session_id;
    char *workflow_template_id;
    char *current_step_key;
    char **skills;
    int   skills_n;
} kanban_task_t;

/* status -> display icon (mirrors _STATUS_ICONS) */
const char *kanban_status_icon(const char *status);

/* _fmt_ts: ts (unix seconds) -> "YYYY-MM-DD HH:MM", or "" if 0 */
char *kanban_fmt_ts(long ts);

/* _fmt_task_line: one-line display (icon id status assignee [tenant] title) */
char *kanban_fmt_task_line(const kanban_task_t *t);

/* _task_to_dict: serialize task to a JSON object string (caller frees) */
char *kanban_task_to_json(const kanban_task_t *t);

/* _run_state_kwargs: (state_type, state_name) -> JSON.
   Returns malloc'd "{}" if both None, "{\"state_type\":..,\"state_name\":..}"
   if both set, or NULL if only one is set (mismatched -> invalid). */
char *kanban_run_state_kwargs(const char *state_type, const char *state_name);

/* _parse_workspace_flag: value -> (kind, path|NULL).
   Returns 0 on success (fills *out_kind / *out_path; path may be NULL or
   malloc'd). Returns -1 on invalid value (and sets *errmsg malloc'd). */
int kanban_parse_workspace_flag(const char *value, char **out_kind, char **out_path, char **errmsg);

/* _parse_branch_flag: normalize optional branch name.
   Returns malloc'd branch on success, NULL on invalid (and *errmsg malloc'd). */
char *kanban_parse_branch_flag(const char *value, char **errmsg);

/* _parse_duration: "30s"/"5m"/"2h"/"1d" or bare int -> seconds.
   Returns -2 for empty/None, -1 on malformed, else >=0 seconds. */
long kanban_parse_duration(const char *value);

/* free a kanban_task_t (shallow: does not free skills array elements,
   which are typically borrowed; frees the malloc'd string fields). */
void kanban_task_free(kanban_task_t *t);

#ifdef __cplusplus
}
#endif

#endif /* KANBAN_FORMAT_H */
