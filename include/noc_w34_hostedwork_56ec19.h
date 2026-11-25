/*
 * noc_w34_hostedwork_56ec19.h - C surface for upstream
 * persistence/sqlite/gateway/_hosted_work.py (gate w34_hostedwork_56ec19).
 *
 * The value-decided core of the hosted-room work-record capture: the storage
 * schema's row-size and usage arithmetic, the disposition a retained row takes
 * when its producer is no longer the room's authority, the exact DDL text of
 * the budget/immutability/replace/authority-transition triggers, the upsert of
 * one work record, the capacity predicate, the authority-history chain read,
 * the history-prefix check of a successor authority, the task/receipt slice a
 * capture carries, and the capture's constants.
 *
 * Every I/O seam stays in the caller: the connection and its savepoints
 * (initialize, _initialize_locked, validate_stored_locked, capture_locked,
 * capture_transition_locked), the canonical JSON/SHA-256 primitives that live
 * in session_rules.hosted_rooms (encode, digest, roster_digest, validate,
 * validate_stored) and normalize_task_payload from session_rules.hosted_tasks.
 * What is compared here is exactly what the values decide.
 *
 * PoP: w34_hostedwork_56ec19_retained_fields   @ persistence/sqlite/gateway/_hosted_work.py:row_size_sql -> session_rules/hosted_rooms.py:retained_fields
 * PoP: w34_hostedwork_56ec19_row_size_sql      @ persistence/sqlite/gateway/_hosted_work.py:row_size_sql
 * PoP: w34_hostedwork_56ec19_usage_sql         @ persistence/sqlite/gateway/_hosted_work.py:usage_sql
 * PoP: w34_hostedwork_56ec19_scope_disposition @ persistence/sqlite/gateway/_hosted_work.py:scope_disposition
 * PoP: w34_hostedwork_56ec19_guards_sql        @ persistence/sqlite/gateway/_hosted_work.py:_guards
 * PoP: w34_hostedwork_56ec19_invalid_guards_sql@ persistence/sqlite/gateway/_hosted_work.py:_invalid_guards
 * PoP: w34_hostedwork_56ec19_save              @ persistence/sqlite/gateway/_hosted_work.py:save_locked
 * PoP: w34_hostedwork_56ec19_budget_refused    @ persistence/sqlite/gateway/_hosted_work.py:_budget
 * PoP: w34_hostedwork_56ec19_read_history      @ persistence/sqlite/gateway/_hosted_work.py:read_history_locked
 * PoP: w34_hostedwork_56ec19_prefix_check      @ persistence/sqlite/gateway/_hosted_work.py:source_prefix_locked
 * PoP: w34_hostedwork_56ec19_capture_tasks     @ persistence/sqlite/gateway/_hosted_work.py:_capture_tasks
 * PoP: w34_hostedwork_56ec19_constants         @ persistence/sqlite/gateway/_hosted_work.py:<module constants>
 */
#ifndef NOC_W34_HOSTEDWORK_56EC19_H
#define NOC_W34_HOSTEDWORK_56EC19_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Work-record storage limits (session_rules.hosted_rooms). */
#define W34_HOSTEDWORK_56EC19_MAX_TASKS 128
#define W34_HOSTEDWORK_56EC19_MAX_RECEIPTS 256
#define W34_HOSTEDWORK_56EC19_MAX_BYTES (128 * 1024)
#define W34_HOSTEDWORK_56EC19_MAX_STORE_BYTES (4 * 1024 * 1024)
#define W34_HOSTEDWORK_56EC19_MAX_STORE_ROWS 512
#define W34_HOSTEDWORK_56EC19_MAX_AUTHORITY_SPANS 1024

/* Table names, exported so the harness need not repeat them. */
extern const char *const W34_HOSTEDWORK_56EC19_SOURCE_TABLE;
extern const char *const W34_HOSTEDWORK_56EC19_TARGET_TABLE;
extern const char *const W34_HOSTEDWORK_56EC19_PENDING_TABLE;
extern const char *const W34_HOSTEDWORK_56EC19_INVALID_TABLE;

/* retained_fields(table) as a JSON array of column names. */
json_t *w34_hostedwork_56ec19_retained_fields(const char *table);

/* row_size_sql(table, prefix): the byte-charged row expression.  Caller frees. */
char *w34_hostedwork_56ec19_row_size_sql(const char *table, const char *prefix);

/* usage_sql(tables): [total_bytes_sql, row_count_sql]. */
json_t *w34_hostedwork_56ec19_usage_sql(const json_t *tables);

/* scope_disposition(table, row, current) where current is [gateway_id, epoch]
 * or null.  Returns the stored disposition string; caller frees. */
char *w34_hostedwork_56ec19_scope_disposition(const char *table, const json_t *row, const json_t *current);

/* _guards(tables): every DDL statement the guard installation runs, in order. */
json_t *w34_hostedwork_56ec19_guards_sql(const json_t *tables);

/* _invalid_guards(): the three immutability triggers of the invalid table. */
json_t *w34_hostedwork_56ec19_invalid_guards_sql(void);

/* save_locked(table, record, record_json, target_install_id, route_generation)
 * -> {"sql": ..., "values": [...]} with the upsert statement and its parameters.
 * record_json is the caller's encode(): canonical JSON is a seam. */
json_t *w34_hostedwork_56ec19_save(const char *table, const json_t *record, const char *record_json,
                                   const char *target_install_id, const char *route_generation);

/* _budget(): 1 when the proposed write would exceed the byte or row budget. */
int w34_hostedwork_56ec19_budget_refused(const json_t *in);

/* read_history_locked over the given rows -> {"history": spans} | {"none": true}
 * | {"error": message}. */
json_t *w34_hostedwork_56ec19_read_history(const json_t *rows, const char *gateway_id, double epoch);

/* source_prefix_locked's verification -> {"spans": spans} | {"error": message}. */
json_t *w34_hostedwork_56ec19_prefix_check(const json_t *spans, const json_t *events, double seq,
                                           const char *gateway_id, double epoch);

/* _capture_tasks over the rows, receipts and normalized payloads ->
 * {"tasks": [...], "receipts": [...], "reason": null|string}. */
json_t *w34_hostedwork_56ec19_capture_tasks(const json_t *in);

/* The capture's limits, tables, transfer kinds and limitations. */
json_t *w34_hostedwork_56ec19_constants(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W34_HOSTEDWORK_56EC19_H */
