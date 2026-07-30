#ifndef HERMES_KANBAN_H
#define HERMES_KANBAN_H

/* Reusable, self-contained Kanban backend API.
 *
 * Extracted from src/tools/kanban.c so that both the kanban tool handlers AND
 * higher-level orchestration ports (e.g. kanban_swarm) share one real storage
 * implementation instead of duplicating the file-based JSON logic. The C
 * backend is file-based (one JSON file per task under $SLERMES_HOME/kanban/,
 * dependency edges in links.json) — a faithful port of the Python sqlite
 * kanban_db module for the operations the swarm needs.
 */

#include "hermes_core_types.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----- Primitives (shared with the tool handlers) ----- */

/* Read a task's full JSON, or NULL if missing. Caller owns the json_t*. */
json_t *kanban_read_task(const char *tid);
/* Persist a task JSON; returns true on success. */
bool kanban_write_task(const char *tid, json_t *j);
/* Generate a new UUID-style task id into buf. */
void kanban_gen_id(char *buf, size_t sz);
/* Current local time as ISO8601 into buf. */
void kanban_now_iso(char *buf, size_t sz);
/* Append an event to a task JSON (mutates task). Returns true. */
bool kanban_add_event(const char *tid, const char *kind,
                      const char *payload, json_t *task);
/* Return array of parent task ids for tid (caller owns). */
json_t *kanban_get_parent_ids(const char *tid);

/* ----- High-level operations (the reusable API) ----- */

typedef struct {
    const char *title;
    const char *assignee;
    const char *body;
    const char *tenant;
    const char *workspace_kind;
    const char *workspace_path;
    const char *created_by;
    const char *initial_status;   /* NULL -> "running" (or "triage" if triage) */
    int priority;
    bool triage;
    const char *skills;           /* comma-separated skill list, or NULL */
    const char *idempotency_key;  /* NULL ok */
    const char *parents_json;     /* JSON array of parent ids, or NULL */
    const char *metadata_json;    /* JSON object merged into task, or NULL */
    int max_runtime_seconds;      /* 0 = unset */
} kanban_task_spec_t;

/* Create a task. Returns a malloc'd task id (caller frees) on success,
 * or NULL on failure. Implements idempotency: if idempotency_key is set and a
 * task with that key already exists, returns its id without creating a
 * duplicate. Parent links (parents_json) are established via kanban_link_tasks. */
char *kanban_create_task(const kanban_task_spec_t *spec);

/* Add a comment authored by `author` to task tid. Returns true on success. */
bool kanban_add_comment(const char *tid, const char *author, const char *body);

/* Mark task tid done with optional summary/result and optional metadata_json
 * (merged into the task). Returns true on success. */
bool kanban_complete_task(const char *tid, const char *summary,
                           const char *result, const char *metadata_json);

/* Add a parent->child dependency edge (cycles/self-links rejected). Returns
 * true on success or when the edge already exists. */
bool kanban_link_tasks(const char *parent_id, const char *child_id);

/* Block / unblock / heartbeat a task by id (engine-backed). */
bool kanban_block_task(const char *tid, const char *reason, const char *kind);
bool kanban_unblock_task(const char *tid);
bool kanban_heartbeat(const char *tid);

/* List all task ids on the default board. Returns a NULL-terminated array
 * (caller frees with kanban_all_task_ids_free). `limit` <= 0 => no cap. */
char **kanban_all_task_ids(int *limit);
void kanban_all_task_ids_free(char **list);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_KANBAN_H */
