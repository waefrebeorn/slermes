/*
 * noc_w29_hostedroom_8927a5_p1.h - C surface for upstream
 * persistence/sqlite/gateway/hosted_room_driver.py (gate w29_hostedroom_8927a5_p1).
 *
 * The statement text and the schema verdicts of the hosted-room driver: every
 * fenced UPDATE the state machine emits, the task-table DDL and its status
 * migration, the task index, the task column order, the lease/task tables'
 * column-and-foreign-key validation, and the step decision between
 * "initialize" and "migrate".  No connection, no transaction, no clock.
 */
#ifndef NOC_W29_HOSTEDROOM_8927A5_P1_H
#define NOC_W29_HOSTEDROOM_8927A5_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* UPDATE ... SET <set_clause> WHERE room_id=? AND task_id=? AND <fence> */
json_t *w29_hostedroom_8927a5_p1_task_update(const char *set_clause, const char *fence);

/* The same statement fenced on status=<status> plus both generations. */
json_t *w29_hostedroom_8927a5_p1_generation_update(const char *set_clause, const char *status);

/* One module-level statement by name: select_lease, select_task,
 * settle_running_sql, settle_stopping_sql, requeue_running_sql,
 * cancel_queued_sql, begin_stop_sql, complete_stop_sql. */
json_t *w29_hostedroom_8927a5_p1_named_sql(const char *name);

/* "SELECT * FROM hosted_room_driver_tasks WHERE room_id=?" (+ status filter)
 * followed by the event order; status NULL means every status. */
json_t *w29_hostedroom_8927a5_p1_tasks_in_order_sql(const char *status);

/* CREATE INDEX [IF NOT EXISTS] idx_hosted_room_driver_tasks_status ... */
json_t *w29_hostedroom_8927a5_p1_task_index_sql(int if_not_exists);

/* The task column order, as an array. */
json_t *w29_hostedroom_8927a5_p1_task_columns(void);

/* One lease-first generation transition: {status, set, generation_stale,
 * stale, sql}, or null for an unknown name. */
json_t *w29_hostedroom_8927a5_p1_transition(const char *name);

/* The task-table DDL under another table name (the migration's _next table). */
json_t *w29_hostedroom_8927a5_p1_task_table_ddl(const char *table);

/* The lease-table DDL. */
json_t *w29_hostedroom_8927a5_p1_leases_table_ddl(void);

/* The statements _initialize_store executes, in order: the schema or the
 * status-constraint migration, each with its validation pragmas. */
json_t *w29_hostedroom_8927a5_p1_schema_sql(int objects_exist);

/* "migrate" when the driver tables already exist, else "initialize". */
json_t *w29_hostedroom_8927a5_p1_step_mode(int objects_exist);

/* Both driver tables and the status index exist. */
json_t *w29_hostedroom_8927a5_p1_schema_objects_exist(const json_t *tables, int has_index);

/* {"error","message"} when the columns or the hosted_rooms foreign keys
 * differ, else null. */
json_t *w29_hostedroom_8927a5_p1_validate_schema(const json_t *lease_columns, const json_t *task_columns,
                                                 int lease_fk, int task_fk);

/* The step's ready(): {"ready","error","message"} from
 * {tables, has_index, task_sql, lease_columns, task_columns, lease_fk, task_fk}. */
json_t *w29_hostedroom_8927a5_p1_schema_ready(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
