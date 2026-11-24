/*
 * noc_w27_schema_3eeff1_p3.h - C surface for upstream
 * persistence/sqlite/_schema.py, gate w27_schema_3eeff1_p3.
 *
 * The capability/degradation layer: which SQLite errors mean "this runtime
 * cannot serve FTS5 / the trigram tokenizer", which stored CREATE text means a
 * legacy inline index or a stale tool_calls projection, the provenance rank a
 * title may be replaced at, whether demoted v22 shadow tables still await
 * teardown, and the "base #N" lineage numbering.
 *
 * Seams (not here): the connection, sqlite_master, psutil, logging.
 */
#ifndef NOC_W27_SCHEMA_3EEFF1_P3_H
#define NOC_W27_SCHEMA_3EEFF1_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _is_trigram_unavailable_error: only the trigram tokenizer is missing. */
int w27_schema_3eeff1_is_trigram_unavailable_error(const char *message);

/* _is_fts5_unavailable_error: no FTS5 module, or an optional tokenizer missing. */
int w27_schema_3eeff1_is_fts5_unavailable_error(const char *message);

/* _db_has_legacy_inline_fts over the stored messages_fts CREATE (null = absent). */
int w27_schema_3eeff1_db_has_legacy_inline_fts(const json_t *table_sql);

/* _db_has_trigram_tool_calls_projection over the stored trigram CREATE. */
int w27_schema_3eeff1_db_has_trigram_tool_calls_projection(const json_t *table_sql);

/* retired_cjk_index_present over the sqlite_master names the lookup would see. */
int w27_schema_3eeff1_retired_cjk_index_present(const json_t *names);

/* _title_rank(source); NULL (pre-provenance row) ranks as user. */
int w27_schema_3eeff1_title_rank(const char *source);

/* _has_fts_trash(conn) over the sqlite_master table names it would match. */
int w27_schema_3eeff1_has_fts_trash(const json_t *table_names);

/* next_title_in_lineage(conn, base) over the titles the lookup returned.
 * Returns the next title; caller frees. */
char *w27_schema_3eeff1_next_title_in_lineage(const char *base_title, const json_t *titles);

#ifdef __cplusplus
}
#endif
#endif
