/*
 * noc_w30_lostandfou_6760c6_p1.h - C surface for upstream
 * persistence/sqlite/admin/_lost_and_found.py (gate w30_lostandfou_6760c6_p1).
 *
 * The value-shape core of the page-level salvage lane: which table a
 * schema-less ``lost_and_found`` record came from, which cell shapes are even
 * possible for a session id / source / epoch / token / path / url, and how a
 * salvaged cell is coerced or vetoed against a declared column type.  The
 * sqlite connection, PRAGMA execution and .recover subprocess are seams: the
 * pragma rows are fed in as fixture values (``{"rows": [[cid, name, type,
 * notnull, dflt_value, pk], ...]}``).
 *
 * JSON numbers arrive as doubles, so an integral JSON number is treated as a
 * Python ``int`` (that is what ``json.loads`` of a bare integer literal
 * yields); fixtures therefore spell floats with a fractional part wherever the
 * int/float distinction is observable.
 */
#ifndef NOC_W30_LOSTANDFOU_6760C6_P1_H
#define NOC_W30_LOSTANDFOU_6760C6_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"value": any} -> bool                                    _is_session_id */
json_t *w30_lostandfou_6760c6_p1_is_session_id(const json_t *req);

/* {"value": any} -> bool                                 _looks_like_source */
json_t *w30_lostandfou_6760c6_p1_looks_like_source(const json_t *req);

/* {"cells": [...]} -> 1 | 3 | null                        _session_source_index */
json_t *w30_lostandfou_6760c6_p1_session_source_index(const json_t *req);

/* {"nfield": int, "cells": [...]} -> "sessions"|"messages"|
 * "session_model_usage"|null                      classify_lost_and_found_row */
json_t *w30_lostandfou_6760c6_p1_classify_row(const json_t *req);

/* {"cells": [...]} -> number                             _heuristic_started_at */
json_t *w30_lostandfou_6760c6_p1_heuristic_started_at(const json_t *req);

/* {"text": str} -> {"type": "int"|"float"|"str", "value": ...}
 *                                                        _parse_sql_default */
json_t *w30_lostandfou_6760c6_p1_parse_sql_default(const json_t *req);

/* {"rows": [[cid, name, type, notnull, dflt_value, pk], ...]} ->
 * {"<index>": substitute}                                  _notnull_defaults */
json_t *w30_lostandfou_6760c6_p1_notnull_substitutes(const json_t *req);

/* {"rows": [...]} -> {name: declared_type}                  _declared_types */
json_t *w30_lostandfou_6760c6_p1_declared_types(const json_t *req);

/* {"kind", "name", "value", "dest_types": {name: type}} -> bool   _cell_fits */
json_t *w30_lostandfou_6760c6_p1_cell_fits(const json_t *req);

/* {"value": any, "declared": str} -> bool                _type_conflicts */
json_t *w30_lostandfou_6760c6_p1_type_conflicts(const json_t *req);

/* {"name": "id"|"source"|"started_at"|"timestamp"|"session_id"|"role"|
 *  "model", "value": any} -> bool                         _sentinel_holds */
json_t *w30_lostandfou_6760c6_p1_sentinel_holds(const json_t *req);

/* {"kind", "name", "value"} -> bool                     _text_shape_holds */
json_t *w30_lostandfou_6760c6_p1_text_shape_holds(const json_t *req);

/* {"name": "is_epoch"|"is_nonempty_str"|"is_token"|"is_json_start"|
 *  "is_path"|"is_url", "value": any} -> bool                  predicates */
json_t *w30_lostandfou_6760c6_p1_predicate(const json_t *req);

/* {"version": [major, minor, patch]} -> bool   is_sqlite_wal_reset_vulnerable */
json_t *w30_lostandfou_6760c6_p1_version_vulnerable(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
