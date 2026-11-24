#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/admin/_lost_and_found.py (gate
w30_lostandfou_6760c6_p2).

The statement builders are exercised through the live functions themselves: a
fake destination connection records the exact ``execute(sql, params)`` call the
lane would make, so ``_insert_prefix_row`` / ``_insert_named_row`` produce the
SQL and the ordered parameter list byte-for-byte, with no database.  Likewise
``_placeholder_titles`` runs for real against a fake ``sessions`` table that
already holds the fixture's taken titles.

``records`` and the per-position consensus are loops inside
``map_lost_and_found_rows`` / ``infer_physical_layouts`` that cannot be called in
isolation; they are rebuilt here line-for-line from the source (the record
classifier itself is still the live ``classify_lost_and_found_row``).
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_lostandfou_6760c6_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.admin import _lost_and_found as live  # noqa: E402
from persistence.sqlite.admin import _recovery  # noqa: E402

WANTED = {
    "quoted_columns",
    "insert_sql",
    "select_sql",
    "copy_columns",
    "sessions_prefix_columns",
    "insert_prefix_plan",
    "insert_named_plan",
    "row_invariants_hold",
    "layout_consensus",
    "records",
    "stub_plan",
    "fts_rebuild_sql",
    "stub_title",
    "unrecognized_widths",
    "recover_details",
    "wal_refusal",
}


class _Cursor:
    rowcount = 1


class _Dest:
    """Records the statement the lane hands sqlite."""

    def __init__(self):
        self.sql = None
        self.params = None

    def execute(self, sql, params=None):
        self.sql = sql
        self.params = list(params) if params is not None else None
        return _Cursor()


class _TitleCursor:
    def __init__(self, row):
        self._row = row
        self.rowcount = 1

    def fetchone(self):
        return self._row


class _TitleConn:
    """A `sessions` table that already contains the taken titles."""

    def __init__(self, taken):
        self.taken = set(taken)

    def execute(self, sql, params=None):
        return _TitleCursor((1,) if params and params[0] in self.taken else None)


def quoted_columns(req):
    columns, placeholders = _recovery._quoted_columns(list(req.get("columns") or []))
    return {"columns": columns, "placeholders": placeholders}


def insert_sql(req):
    columns, placeholders = _recovery._quoted_columns(list(req.get("columns") or []))
    return 'INSERT OR IGNORE INTO "%s" (%s) VALUES (%s)' % (req.get("table", ""), columns, placeholders)


def select_sql(req):
    columns, _placeholders = _recovery._quoted_columns(list(req.get("columns") or []))
    return 'SELECT %s FROM "%s"' % (columns, req.get("table", ""))


def copy_columns(req):
    source = list(req.get("source") or [])
    dest = list(req.get("dest") or [])
    return {
        "columns": [c for c in dest if c in source],
        "create_auxiliary": bool(not dest and req.get("auxiliary")),
    }


def sessions_prefix_columns(req):
    defaults = {int(k): v for k, v in (req.get("defaults") or {}).items()}
    columns, remapped = live._sessions_prefix_columns(list(req.get("columns") or []), defaults,
                                                      tuple(req.get("cells") or ()))
    return {"columns": list(columns), "defaults": {str(k): v for k, v in remapped.items()}}


def insert_prefix_plan(req):
    dest = _Dest()
    subs = {int(k): v for k, v in (req.get("substitutes") or {}).items()} or None
    live._insert_prefix_row(dest, req.get("table", ""), list(req.get("columns") or []),
                            list(req.get("cells") or []), subs)
    return {"sql": dest.sql, "params": dest.params}


def insert_named_plan(req):
    dest = _Dest()
    subs = {int(k): v for k, v in (req.get("substitutes") or {}).items()}
    layout = req.get("layout")
    layout = None if layout is None else list(layout)
    live._insert_named_row(dest, req.get("table", ""), layout, tuple(req.get("cells") or ()),
                           list(req.get("dest_columns") or []), subs, req.get("overrides") or None)
    return {"sql": dest.sql, "params": dest.params}


def row_invariants_hold(req):
    return bool(live._row_invariants_hold(req.get("kind"), list(req.get("layout") or []),
                                          [tuple(row) for row in req.get("rows") or []]))


def layout_consensus(req):
    survivors = [list(layout) for layout in req.get("survivors") or []]
    if not survivors:
        return []
    width = len(survivors[0])
    out = []
    for index in range(width):
        names = {layout[index] for layout in survivors}
        out.append(names.pop() if len(names) == 1 else None)
    return out


def records(req):
    out = []
    for table in req.get("tables") or []:
        if list(table.get("columns") or [])[:3] != ["rootpgno", "pgno", "nfield"]:
            continue
        for row in table.get("rows") or []:
            try:
                nfield = int(row[2]) if row[2] is not None else 0
            except (TypeError, ValueError):
                out.append({"kind": None, "lf_rowid": None, "nfield": 0, "cells": []})
                continue
            cells = tuple(row[4:4 + max(nfield, 0)])
            out.append({
                "kind": live.classify_lost_and_found_row(nfield, cells),
                "lf_rowid": row[3],
                "nfield": nfield,
                "cells": list(cells),
            })
    return out


def stub_plan(req):
    label = req.get("label", "best-effort recovered")
    orphan_ids = {}
    for entry in req.get("orphans") or []:
        session_id = str(entry.get("session_id"))
        if entry.get("setdefault") and session_id in orphan_ids:
            continue
        started = entry.get("started_at")
        orphan_ids[session_id] = {
            "started_at": float(started) if started is not None else 0.0,
            "message_count": int(entry.get("message_count") or 0),
        }
    titles = _recovery._placeholder_titles(_TitleConn(req.get("taken_titles") or []), label)
    return [
        {
            "session_id": session_id,
            "started_at": info["started_at"],
            "message_count": info["message_count"],
            "title": next(titles),
        }
        for session_id, info in sorted(orphan_ids.items())
    ]


def fts_rebuild_sql(req):
    table = req.get("table", "")
    return 'INSERT INTO "%s" ("%s") VALUES (\'rebuild\')' % (table, table)


def stub_title(req):
    return "[%s %d] session metadata was unreadable" % (req.get("label", "best-effort recovered"),
                                                        int(req.get("sequence") or 0))


def unrecognized_widths(req):
    widths = list(req.get("widths") or [])
    width = req.get("width")
    if width not in widths:
        widths.append(width)
    return widths


def recover_details(req):
    attempts = req.get("attempts") or []
    usable = bool(attempts[-1]["usable"]) if attempts else False
    if usable:
        return {"usable": True, "needs_header_zeroing": False, "details": "", "message": ""}
    needs_zeroing = any("not a database" in a["dump_stderr_tail"] for a in attempts)
    details = "; ".join(
        f"[{a['command']}] dump rc={a['dump_returncode']} load rc={a['load_returncode']} "
        f"{a['dump_stderr_tail'] or a['load_stderr_tail']}".strip()
        for a in attempts
    )
    return {
        "usable": False,
        "needs_header_zeroing": needs_zeroing,
        "details": details,
        "message": "sqlite3 .recover did not produce a usable lost_and_found database: " + details,
    }


def wal_refusal(req):
    version = tuple(req.get("version") or ())
    if not live._wal_reset_vulnerable(version):
        return None
    version_str = ".".join(str(part) for part in version)
    return {
        "reason": "wal_reset_vulnerable",
        "binary": req.get("binary", ""),
        "version": version_str,
        "detail": f"reports version {version_str}, which has " + live._WAL_RESET_VULNERABLE_GUIDANCE,
    }


HANDLERS = {
    "quoted_columns": quoted_columns,
    "insert_sql": insert_sql,
    "select_sql": select_sql,
    "copy_columns": copy_columns,
    "sessions_prefix_columns": sessions_prefix_columns,
    "insert_prefix_plan": insert_prefix_plan,
    "insert_named_plan": insert_named_plan,
    "row_invariants_hold": row_invariants_hold,
    "layout_consensus": layout_consensus,
    "records": records,
    "stub_plan": stub_plan,
    "fts_rebuild_sql": fts_rebuild_sql,
    "stub_title": stub_title,
    "unrecognized_widths": unrecognized_widths,
    "recover_details": recover_details,
    "wal_refusal": wal_refusal,
}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = HANDLERS[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
