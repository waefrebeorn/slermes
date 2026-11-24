#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/admin/_lost_and_found.py (gate
w30_lostandfou_6760c6_p1).

The live module is imported as a package module and its pure shape/coercion
helpers are called directly.  The sqlite handle is the seam: ``_notnull_defaults``
and ``_declared_types`` only ever call ``conn.execute('PRAGMA table_info(...)')``
and iterate the rows, so a fake connection that hands back the fixture's PRAGMA
rows exercises the real functions without opening a database.

Compared:
  * ``_is_session_id`` / ``_looks_like_source`` / ``_session_source_index``;
  * ``classify_lost_and_found_row`` and ``_heuristic_started_at``;
  * ``_parse_sql_default`` / ``_notnull_defaults`` / ``_declared_types``;
  * ``_type_conflicts`` / ``_cell_fits`` / ``_sentinel_holds`` / ``_text_shape_holds``
    and the small predicates they are built from;
  * ``sqlite_support._is_sqlite_wal_reset_vulnerable``, the salvage-shell gate.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_lostandfou_6760c6_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.admin import _lost_and_found as live  # noqa: E402

WANTED = {
    "is_session_id",
    "looks_like_source",
    "session_source_index",
    "classify_row",
    "heuristic_started_at",
    "parse_sql_default",
    "notnull_substitutes",
    "declared_types",
    "cell_fits",
    "type_conflicts",
    "sentinel_holds",
    "text_shape_holds",
    "predicate",
    "version_vulnerable",
}

PREDICATES = {
    "is_epoch": live._is_epoch,
    "is_nonempty_str": live._is_nonempty_str,
    "is_token": live._is_token,
    "is_json_start": live._is_json_start,
    "is_path": live._is_path,
    "is_url": live._is_url,
}


class FakeConn:
    """Enough of sqlite3.Connection for the PRAGMA-driven helpers."""

    def __init__(self, rows):
        self.rows = [list(row) for row in rows]

    def execute(self, sql, params=None):  # noqa: ARG002 - the SQL is the seam
        return iter(self.rows)


def encode_default(value):
    """The port tags the coercion result so int/float/str stay distinct."""
    if isinstance(value, bool):
        return {"type": "int", "value": int(value)}
    if isinstance(value, int):
        return {"type": "int", "value": value}
    if isinstance(value, float):
        return {"type": "float", "value": value}
    return {"type": "str", "value": str(value)}


def is_session_id(req):
    return bool(live._is_session_id(req.get("value")))


def looks_like_source(req):
    return bool(live._looks_like_source(req.get("value")))


def session_source_index(req):
    return live._session_source_index(tuple(req.get("cells") or ()))


def classify_row(req):
    return live.classify_lost_and_found_row(int(req.get("nfield") or 0), tuple(req.get("cells") or ()))


def heuristic_started_at(req):
    return live._heuristic_started_at(tuple(req.get("cells") or ()))


def parse_sql_default(req):
    text = req.get("text")
    return encode_default(live._parse_sql_default("" if text is None else str(text)))


def notnull_substitutes(req):
    subs = live._notnull_defaults(FakeConn(req.get("rows") or []), "t")
    return {str(index): value for index, value in subs.items()}


def declared_types(req):
    return live._declared_types(FakeConn(req.get("rows") or []), "t")


def cell_fits(req):
    return bool(live._cell_fits(req.get("kind"), req.get("name"), req.get("value"),
                               req.get("dest_types") or {}))


def type_conflicts(req):
    return bool(live._type_conflicts(req.get("value"), req.get("declared") or ""))


def sentinel_holds(req):
    return bool(live._sentinel_holds(req.get("name"), req.get("value")))


def text_shape_holds(req):
    return bool(live._text_shape_holds(req.get("kind"), req.get("name"), req.get("value")))


def predicate(req):
    return bool(PREDICATES[req.get("name")](req.get("value")))


def version_vulnerable(req):
    return bool(live._wal_reset_vulnerable(tuple(req.get("version") or ())))


HANDLERS = {
    "is_session_id": is_session_id,
    "looks_like_source": looks_like_source,
    "session_source_index": session_source_index,
    "classify_row": classify_row,
    "heuristic_started_at": heuristic_started_at,
    "parse_sql_default": parse_sql_default,
    "notnull_substitutes": notnull_substitutes,
    "declared_types": declared_types,
    "cell_fits": cell_fits,
    "type_conflicts": type_conflicts,
    "sentinel_holds": sentinel_holds,
    "text_shape_holds": text_shape_holds,
    "predicate": predicate,
    "version_vulnerable": version_vulnerable,
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
