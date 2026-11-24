/*
 * noc_w27_schema_3eeff1_p2.h - C surface for upstream
 * persistence/sqlite/_schema.py, gate w27_schema_3eeff1_p2.
 *
 * The statement-sequence layer: which SQL a rebuild emits, in what order, and
 * whether the caller's transaction is reused; the PRIMARY KEY read-back from
 * PRAGMA table_info rows; the "index is external-content over the raw table"
 * predicate; the FTS rebuild/trigger-drop/trigger-count decisions.
 *
 * Seams (not here): the connection, the write lock, executescript.
 */
#ifndef NOC_W27_SCHEMA_3EEFF1_P2_H
#define NOC_W27_SCHEMA_3EEFF1_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _fts_index_is_misaligned_source over the stored CREATE text (null = no row). */
int w27_schema_3eeff1_fts_index_is_misaligned_source(const json_t *table_sql);

/* _live_pk_columns over PRAGMA table_info rows [[cid,name,type,notnull,dflt,pk],...];
 * `missing` = the PRAGMA raised (OperationalError). Array of names or null. */
json_t *w27_schema_3eeff1_live_pk_columns(const json_t *rows, int missing);

/* _rebuild_table_statements(...) -> [ALTER, ddl, copy_sql, DROP, *indexes]. */
json_t *w27_schema_3eeff1_rebuild_table_statements(const char *table, const char *legacy_name,
                                                   const char *ddl, const char *copy_sql,
                                                   const json_t *indexes);

/* _rebuild_table(...) with the transaction ownership branch:
 * {"statements":[...]} plus {"error","message"} when fail_at >= 0 raises. */
json_t *w27_schema_3eeff1_rebuild_table(const char *table, const char *legacy_name, const char *ddl,
                                        const char *copy_sql, const json_t *indexes,
                                        int in_transaction, int fail_at);

/* _rebuild_fts_indexes(legacy=, include_trigram=) -> statement list. */
json_t *w27_schema_3eeff1_rebuild_fts_indexes(int legacy, int include_trigram);

/* _drop_fts_triggers() -> the six DROP TRIGGER IF EXISTS statements. */
json_t *w27_schema_3eeff1_drop_fts_triggers(void);

/* _fts_triggers_missing(cursor, names) given the COUNT(*) the cursor returned
 * (-1 = the query failed). */
int w27_schema_3eeff1_fts_triggers_missing(const json_t *names, int found);

#ifdef __cplusplus
}
#endif
#endif
