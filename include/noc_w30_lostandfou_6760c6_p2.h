/*
 * noc_w30_lostandfou_6760c6_p2.h - C surface for upstream
 * persistence/sqlite/admin/_lost_and_found.py (gate w30_lostandfou_6760c6_p2).
 *
 * The statement/report shaping half of the salvage lane: the INSERT/SELECT
 * text built for a salvaged row stream, the column order the positional
 * fallback zips onto a sessions record, the per-position layout consensus, the
 * record extraction from a ``lost_and_found`` fixture, the stub-parent plan,
 * and the .recover diagnostics message.  Every sqlite handle is a seam: the
 * fixtures carry the rows, the columns and the attempts, and the port returns
 * the bytes the lane would have handed sqlite.
 *
 * Params are compared as Python values (int/float equality is numeric), so the
 * port never needs to re-tag numbers.
 */
#ifndef NOC_W30_LOSTANDFOU_6760C6_P2_H
#define NOC_W30_LOSTANDFOU_6760C6_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"columns": [...]} -> {"columns": "\"a\", \"b\"", "placeholders": "?, ?"}
 *                                                        _quoted_columns */
json_t *w30_lostandfou_6760c6_p2_quoted_columns(const json_t *req);

/* {"table", "columns"} -> INSERT OR IGNORE ...            _execute_insert */
json_t *w30_lostandfou_6760c6_p2_insert_sql(const json_t *req);

/* {"table", "columns"} -> SELECT ... FROM ...          _copy_direct_tables */
json_t *w30_lostandfou_6760c6_p2_select_sql(const json_t *req);

/* {"source": [...], "dest": [...], "auxiliary": bool} ->
 * {"columns": [shared...], "create_auxiliary": bool}   _copy_direct_tables */
json_t *w30_lostandfou_6760c6_p2_copy_columns(const json_t *req);

/* {"columns": [...], "defaults": {"<i>": any}, "cells": [...]} ->
 * {"columns": [...], "defaults": {"<i>": any}}      _sessions_prefix_columns */
json_t *w30_lostandfou_6760c6_p2_sessions_prefix_columns(const json_t *req);

/* {"table", "columns": [...], "cells": [...],
 *  "substitutes": {"<i>": any} | null} ->
 * {"sql": str, "params": [...]}                       _insert_prefix_row */
json_t *w30_lostandfou_6760c6_p2_insert_prefix_plan(const json_t *req);

/* {"table", "layout": [...] | null, "cells": [...], "dest_columns": [...],
 *  "substitutes": {"<i>": any} | null, "overrides": {...} | null} ->
 * {"sql": str, "params": [...]}                        _insert_named_row */
json_t *w30_lostandfou_6760c6_p2_insert_named_plan(const json_t *req);

/* {"kind": str, "layout": [name|null, ...], "rows": [[...], ...]} -> bool
 *                                                     _row_invariants_hold */
json_t *w30_lostandfou_6760c6_p2_row_invariants_hold(const json_t *req);

/* {"survivors": [[name|null, ...], ...]} -> [name|null, ...]
 *                                             infer_physical_layouts consensus */
json_t *w30_lostandfou_6760c6_p2_layout_consensus(const json_t *req);

/* {"tables": [{"columns": [...], "rows": [[...], ...]}]} ->
 * [{"kind", "lf_rowid", "nfield", "cells"}]        map_lost_and_found_rows */
json_t *w30_lostandfou_6760c6_p2_records(const json_t *req);

/* {"orphans": [{"session_id", "started_at", "message_count",
 *               "setdefault": bool}], "taken_titles": [...], "label"} ->
 * [{"session_id", "started_at", "message_count", "title"}]
 *                                            stub_missing_parent_sessions */
json_t *w30_lostandfou_6760c6_p2_stub_plan(const json_t *req);

/* {"table": "messages_fts"} -> the rebuild statement     rebuild_fts_indexes */
json_t *w30_lostandfou_6760c6_p2_fts_rebuild_sql(const json_t *req);

/* {"label": str, "sequence": int} -> "[label N] session metadata was
 * unreadable"                                            _placeholder_titles */
json_t *w30_lostandfou_6760c6_p2_stub_title(const json_t *req);

/* {"widths": [...], "width": int} -> widths with width appended once
 *                                       unrecognized_layout_widths bookkeeping */
json_t *w30_lostandfou_6760c6_p2_unrecognized_widths(const json_t *req);

/* {"attempts": [{"command", "dump_returncode", "load_returncode",
 *                "dump_stderr_tail", "load_stderr_tail", "usable"}]} ->
 * {"usable", "needs_header_zeroing", "details", "message"}
 *                                   run_cli_lost_and_found_recover */
json_t *w30_lostandfou_6760c6_p2_recover_details(const json_t *req);

/* {"binary", "version": [major, minor, patch]} ->
 * {"reason", "binary", "version", "detail"} | null   find_sqlite3_cli */
json_t *w30_lostandfou_6760c6_p2_wal_refusal(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
