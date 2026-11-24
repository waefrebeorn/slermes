#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/_schema.py (gate w27_schema_3eeff1_p1).

The module is imported as a package module (its relative imports need the
package context) and its fragment layer is exercised with no database at all:
the only seam is the cursor, and a recording stand-in supplies exactly what the
builder reads from it (``sqlite_master``'s trigger name -> sql map).

  * ``_q`` / ``_legacy_inline_reinsert_sql`` — pinned statement text;
  * ``_fts_update_trigger_needs_narrowing`` — the broad AFTER UPDATE predicate;
  * ``_execute_ddl_script_transactional`` — statement stream of a DDL script
    (whitespace preserved: the trace harness compares it verbatim);
  * ``_execute_ddl_skipping_settled_triggers`` — which DROP/CREATE pairs are
    elided because sqlite_master already holds the same body;
  * ``_positive_int`` — the guard's exact TypeError/ValueError text;
  * the module's pinned SQL and budget constants.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_schema_3eeff1_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite import _schema as live  # noqa: E402

WANTED = {
    "positive_int", "quote_ident", "legacy_inline_reinsert_sql", "needs_narrowing",
    "complete", "ddl_script", "ddl_settled", "constants",
}

STORED_QUERY = "SELECT name, sql FROM sqlite_master WHERE type = 'trigger'"


class RecordingCursor:
    """Minimal cursor: records every execute(), answers only the trigger map."""

    def __init__(self, stored):
        self.stored = dict(stored or {})
        self.calls = []

    def execute(self, sql, params=()):
        self.calls.append(sql)
        return self

    def fetchall(self):
        return list(self.stored.items())


def py_value(kind, num, text):
    if kind == "int":
        return int(num)
    if kind == "float":
        return float(num)
    if kind == "bool":
        return bool(num)
    if kind == "str":
        return text
    if kind == "null":
        return None
    if kind == "list":
        return []
    if kind == "dict":
        return {}
    raise AssertionError("unknown kind %r" % (kind,))


def positive_int(req):
    value = py_value(req.get("kind"), req.get("num"), req.get("text"))
    try:
        live._positive_int(req.get("name"), value)
    except TypeError as exc:
        return {"error": "TypeError", "message": str(exc)}
    except ValueError as exc:
        return {"error": "ValueError", "message": str(exc)}
    return {"ok": True}


def ddl_script(req):
    cur = RecordingCursor(None)
    try:
        live._execute_ddl_script_transactional(cur, req.get("ddl") or "")
    except Exception as exc:  # noqa: BLE001 - the raise text is the compared value
        return {"error": type(exc).__name__, "message": str(exc)}
    return {"statements": cur.calls}


def ddl_settled(req):
    cur = RecordingCursor(req.get("stored"))
    try:
        live._execute_ddl_skipping_settled_triggers(cur, req.get("ddl") or "")
    except Exception as exc:  # noqa: BLE001
        return {"error": type(exc).__name__, "message": str(exc)}
    return {"statements": [s for s in cur.calls if s != STORED_QUERY]}


def constants():
    return {
        "fts_holder_escalate_attempts": live._FTS_HOLDER_ESCALATE_ATTEMPTS,
        "fts_holder_escalate_seconds": live._FTS_HOLDER_ESCALATE_SECONDS,
        "fts_holder_futile_attempts": live._FTS_HOLDER_FUTILE_ATTEMPTS,
        "fts_holder_futile_seconds": live._FTS_HOLDER_FUTILE_SECONDS,
        "fts_stale_retry_seconds": live._FTS_STALE_RETRY_SECONDS,
        "fts_stale_retry_max_seconds": live._FTS_STALE_RETRY_MAX_SECONDS,
        "message_uid_backfill_done": live._MESSAGE_UID_BACKFILL_DONE,
        "message_uid_backfill_cursor": live._MESSAGE_UID_BACKFILL_CURSOR,
        "message_uid_backfill_chunk": live._MESSAGE_UID_BACKFILL_CHUNK,
        "message_uid_backfill_budget_s": live._MESSAGE_UID_BACKFILL_BUDGET_S,
        "clear_rebuild_markers_sql": live._CLEAR_REBUILD_MARKERS_SQL,
        "drop_retired_tool_high_water_sql": live._DROP_RETIRED_TOOL_HIGH_WATER_SQL,
        "legacy_inline_concat_sql": live._LEGACY_INLINE_CONCAT_SQL,
        "title_unique_index_sql": live._TITLE_UNIQUE_INDEX_SQL,
        "stale_key_upsert_sql": live._STALE_KEY_UPSERT_SQL,
        "state_meta_upsert_sql": live._STATE_META_UPSERT_SQL,
        "session_model_usage_index_sql": list(live._SESSION_MODEL_USAGE_INDEX_SQL),
        "fts_triggers": list(live._FTS_TRIGGERS),
        "fts_trash_prefix": live._FTS_TRASH_PREFIX,
        "schema_version": live.SCHEMA_VERSION,
        "fts_storage_version": live.FTS_STORAGE_VERSION,
        "fts_stale_key": live.FTS_STALE_KEY,
        "fts_rebuild_deferral_key": live.FTS_REBUILD_DEFERRAL_KEY,
    }


def expected(r):
    f = r.get("func")
    if f == "positive_int":
        return positive_int(r)
    if f == "quote_ident":
        return live._q(r.get("ident"))
    if f == "legacy_inline_reinsert_sql":
        return live._legacy_inline_reinsert_sql(
            r.get("table"), int(r.get("indent") or 0),
            delete_first=bool(r.get("delete_first")))
    if f == "needs_narrowing":
        return live._fts_update_trigger_needs_narrowing(r.get("sql"))
    if f == "complete":
        import sqlite3
        return sqlite3.complete_statement(r.get("sql") or "")
    if f == "ddl_script":
        return ddl_script(r)
    if f == "ddl_settled":
        return ddl_settled(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = total = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        total += 1
        try:
            r = json.loads(line)
        except ValueError:
            print("BAD JSON line %d" % total, file=sys.stderr)
            mism += 1
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            print("UNHANDLED %r" % (r.get("func") if isinstance(r, dict) else r,), file=sys.stderr)
            mism += 1
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    if total != cases:
        print("ORACLE %s: %d fixture lines, only %d cases compared" % (GATE, total, cases),
              file=sys.stderr)
        mism += 1
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
