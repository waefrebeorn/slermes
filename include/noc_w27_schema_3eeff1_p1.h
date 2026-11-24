/*
 * noc_w27_schema_3eeff1_p1.h - C surface for upstream
 * persistence/sqlite/_schema.py, gate w27_schema_3eeff1_p1.
 *
 * The schema builder's fragment layer: identifier quoting, the pinned legacy
 * inline reinsert fragment, the broad-UPDATE-trigger predicate, the statement
 * splitter every DDL script is fed through (sqlite3.complete_statement's rule),
 * the drop/create elision for triggers already settled in sqlite_master, the
 * _positive_int guard, and the module's pinned SQL/budget constants.
 *
 * Seams (not here): the connection, the write lock, admission, logging.
 */
#ifndef NOC_W27_SCHEMA_3EEFF1_P1_H
#define NOC_W27_SCHEMA_3EEFF1_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _positive_int(name, value). `kind` is the Python type of the value
 * ("int"|"float"|"bool"|"str"|"null"|"list"|"dict"); returns {"ok":true} or
 * {"error":"TypeError"|"ValueError","message":<exact upstream text>}. */
json_t *w27_schema_3eeff1_positive_int(const char *name, const char *kind, double num, const char *text);

/* _q(ident): double-quote an SQL identifier. Caller frees. */
char *w27_schema_3eeff1_quote_ident(const char *ident);

/* _legacy_inline_reinsert_sql(table, indent, delete_first). Caller frees. */
char *w27_schema_3eeff1_legacy_inline_reinsert_sql(const char *table, int indent, int delete_first);

/* _fts_update_trigger_needs_narrowing(sql) — NULL/empty is False. */
int w27_schema_3eeff1_fts_update_trigger_needs_narrowing(const char *sql);

/* sqlite3.complete_statement(sql): last significant token is a top-level ';'. */
int w27_schema_3eeff1_statement_complete(const char *sql);

/* _execute_ddl_script_transactional's statement stream:
 * {"statements":[...]} or {"error":"OperationalError","message":"incomplete FTS DDL statement"}. */
json_t *w27_schema_3eeff1_ddl_script_statements(const char *ddl);

/* _execute_ddl_skipping_settled_triggers(cursor, ddl) with `stored` standing
 * in for sqlite_master's trigger name->sql map (null = empty map):
 * {"statements":[...]} or {"error":...,"message":...}. */
json_t *w27_schema_3eeff1_ddl_skipping_settled(const char *ddl, const json_t *stored);

/* The module constants (budgets, marker keys, pinned SQL text) as one object. */
json_t *w27_schema_3eeff1_constants(void);

#ifdef __cplusplus
}
#endif
#endif
