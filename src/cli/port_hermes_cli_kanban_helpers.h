#ifndef PORT_HERMES_CLI_KANBAN_HELPERS_H
#define PORT_HERMES_CLI_KANBAN_HELPERS_H

/* Minimal task shape used by _task_to_dict / _fmt_task_line ports.
 * Mirrors the Python kb.Task attribute set. Declared here so command
 * handlers (cli_cmd_kanban.c) can build one from JSON and render it. */
typedef struct {
    const char *id;
    const char *title;
    const char *body;
    const char *assignee;
    const char *status;
    int         priority;
    const char *tenant;
    const char *workspace_kind;
    const char *workspace_path;
    const char *branch_name;
    const char *project_id;
    const char *created_by;
    long        created_at;
    long        started_at;
    long        completed_at;
    const char *result;
    char      **skills;
    int         skills_len;
    int         max_retries;
    const char *session_id;
    const char *workflow_template_id;
    const char *current_step_key;
} kb_task_t;

/* Render one task as a compact status line. Returns malloc'd string. */
char *fmt_task_line(const kb_task_t *t);

/* Format a unix timestamp as "YYYY-MM-DD HH:MM". Returns malloc'd string. */
char *fmt_kanban_ts(long ts);

#endif /* PORT_HERMES_CLI_KANBAN_HELPERS_H */
