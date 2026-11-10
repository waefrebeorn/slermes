/*
 * kanban_workers_db.h — kanban worker DB/bookkeeping helpers.
 *
 * Module-internal header (kanban_workers.c <-> kanban_workers_db.c).
 */
#ifndef KANBAN_WORKERS_DB_H
#define KANBAN_WORKERS_DB_H

#include "sqlite3.h"

int kdb_record_task_failure(sqlite3 *conn, const char *task_id, const char *error, const char *outcome, int failure_limit, int release_claim, int end_run, const char *event_payload_extra_json);
int kb_is_managed_scratch(const char *path);
int kdb_try_cleanup_parent_workspaces(sqlite3 *conn, const char *task_id);
int kb_table_has_drifted(sqlite3 *conn, const char *table);
int kb_worker_survived_termination(const char *term_json);
void kb_defer_reclaim_for_live_worker(sqlite3 *conn, const char *task_id, const char *claim_lock, long now, const char *term_json, const char *reason);
void kb_vec_push(char ***vec, char *s);

#endif /* KANBAN_WORKERS_DB_H */
