#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/housekeeping/_portability.py
(gate w33_portabilit_b54bb9).

The live module is imported from the quarry and its pure functions are called
directly; the SQLite writer/reader is replaced by a recording fake connection
that answers exactly the three SELECTs and the one UPDATE the ported functions
issue, so the comparison is the decision (which id matched, which parent edge
was kept, how many were detached) and not the SQL.  ``_import_session_row``
itself needs a live holder, so the flag mask and the live/archived message
split are exercised through the same live primitives it uses
(``_coerce_or`` over ``_IMPORT_FLAG_COLS``) rather than through a fake holder.

The decoder's own JSONDecodeError text is CPython's, not libjson's: that
rejection is compared by kind only.
"""
import dataclasses
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w33_portabilit_b54bb9"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))

from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

from persistence.housekeeping import maintenance as mnt  # noqa: E402
from persistence.sqlite.housekeeping import _portability as live  # noqa: E402

WANTED = {
    "constants", "export_row", "text_or_none", "int_or_none", "int_or_bounded",
    "float_or_finite", "json_object_or_none", "non_finite_path", "load_json",
    "message_content", "normalize_message", "normalize_session", "validate_payload",
    "foreign_match", "parent_attach", "import_flags", "message_split",
}


class Cur:
    def __init__(self, rows):
        self.rows = rows

    def fetchall(self):
        return self.rows

    def fetchone(self):
        return self.rows[0] if self.rows else None


class FakeConn:
    """Answers the four statements _portability issues; records the UPDATEs."""

    def __init__(self, rows):
        self.rows = rows or []
        self.updates = []

    def execute(self, sql, params=()):
        flat = " ".join(str(sql).split())
        if flat.startswith("SELECT id, origin_json FROM sessions"):
            tool = params[0]
            return Cur([{"id": r["id"], "origin_json": r["origin_json"]}
                        for r in self.rows
                        if r.get("source") == tool and r.get("origin_json") is not None])
        if flat.startswith("SELECT 1 FROM sessions WHERE id = ?"):
            sid = params[0]
            return Cur([1] if any(r["id"] == sid for r in self.rows) else [])
        if flat.startswith("SELECT parent_session_id FROM sessions"):
            sid = params[0]
            for r in self.rows:
                if r["id"] == sid:
                    return Cur([{"parent_session_id": r.get("parent_session_id")}])
            return Cur([])
        if flat.startswith("UPDATE sessions SET parent_session_id"):
            self.updates.append([params[1], params[0]])
            return Cur([])
        raise AssertionError("unexpected sql %r" % (sql,))


def ok(value):
    return {"ok": True, "value": value}


def bad(exc):
    return {"ok": False, "kind": type(exc).__name__, "error": str(exc)}


def run(fn, *args):
    try:
        return ok(fn(*args))
    except Exception as exc:  # noqa: BLE001
        return bad(exc)


def constants(_req):
    return ok({
        "insert_sql": live._IMPORT_SESSION_INSERT_SQL,
        "session_text_fields": list(live._IMPORT_SESSION_TEXT_FIELDS),
        "message_text_fields": list(live._IMPORT_MESSAGE_TEXT_FIELDS),
        "message_json_fields": list(live._IMPORT_MESSAGE_JSON_FIELDS),
        "passthrough_cols": list(live._IMPORT_PASSTHROUGH_COLS),
        "int_cols": list(live._IMPORT_INT_COLS),
        "float_cols": list(live._IMPORT_FLOAT_COLS),
        "flag_cols": list(live._IMPORT_FLAG_COLS),
        "export_columns": list(live._EXPORT_COLUMNS),
        "max_sessions": mnt.IMPORT_MAX_SESSIONS,
        "max_messages_per_session": mnt.IMPORT_MAX_MESSAGES_PER_SESSION,
        "max_total_messages": mnt.IMPORT_MAX_TOTAL_MESSAGES,
        "max_session_bytes": mnt.IMPORT_MAX_SESSION_BYTES,
        "max_total_bytes": mnt.IMPORT_MAX_TOTAL_BYTES,
        "sqlite_int_min": str(live._SQLITE_INT_MIN),
        "sqlite_int_max": str(live._SQLITE_INT_MAX),
        "epoch_min": float(mnt_epoch()[0]),
        "epoch_max": float(mnt_epoch()[1]),
    })


_EPOCH = []


def mnt_epoch():
    if not _EPOCH:
        from persistence.sqlite.conversation._store import rules

        _EPOCH.extend([rules.EPOCH_MIN, rules.EPOCH_MAX])
    return _EPOCH


def export_row(req):
    return run(lambda: dataclasses.asdict(live._export_row(req.get("session"))))


def text_or_none(req):
    return run(live._import_text_or_none, req.get("value"), req.get("field"))


def int_or_none(req):
    return run(live._import_int_or_none, req.get("value"), req.get("field"))


def int_or_bounded(req):
    return run(live._import_int_or_bounded, req.get("value"), req.get("field"),
               req.get("default", 0))


def float_or_finite(req):
    return run(live._import_float_or_finite, req.get("value"), req.get("field"))


def json_object_or_none(req):
    return run(live._import_json_object_or_none, req.get("value"), req.get("field"))


def non_finite_path(req):
    return run(live._import_non_finite_path, req.get("value"), req.get("path"))


def load_json(req):
    try:
        return ok(live._import_load_json(req.get("text"), req.get("path")))
    except json.JSONDecodeError:
        # The decoder's own text is the seam: libjson is not CPython's decoder.
        return {"ok": False, "kind": "JSONDecodeError", "error": ""}
    except Exception as exc:  # noqa: BLE001
        return bad(exc)


def message_content(req):
    return run(live._validate_import_message_content, req.get("content"), req.get("path"))


def normalize_message(req):
    return run(live._normalize_import_message, req.get("message"), req.get("index", 0))


def normalize_session(req):
    raw = req.get("session")
    messages = req.get("messages")
    if messages is None:
        messages = (raw.get("messages") if isinstance(raw, dict) else None) or []
    return run(live._normalize_import_session, raw, req.get("session_id"), messages)


def validate_payload(req):
    def call():
        normalized, errors = live._validate_import_payload(list(req.get("sessions") or []))
        return [normalized, errors]

    return run(call)


def foreign_match(req):
    conn = FakeConn(req.get("rows") or [])
    return run(live._find_foreign_import_on_conn, conn, req.get("origin"))


def parent_attach(req):
    conn = FakeConn(req.get("rows") or [])
    updates = [tuple(pair) for pair in (req.get("parent_updates") or [])]

    def call():
        detached = live._attach_import_parents(conn, updates)
        return {"detached": detached, "updates": conn.updates}

    return run(call)


def import_flags(req):
    raw = req.get("raw") or {}

    def call():
        params = {col: 1 if live._coerce_or(raw.get(col), int, 0) else 0
                  for col in live._IMPORT_FLAG_COLS}
        params["auto_archived"] &= params["archived"]
        return params

    return run(call)


def message_split(req):
    messages = req.get("messages") or []

    def call():
        keep = [m for m in messages if live._coerce_or(m.get("active"), int, 1)]
        archived = [m for m in messages if not live._coerce_or(m.get("active"), int, 1)]
        return {"live": len(keep), "archived": len(archived),
                "compacted": [1 if live._coerce_or(m.get("compacted"), int, 0) else 0
                              for m in archived]}

    return run(call)


HANDLERS = {
    "constants": constants,
    "export_row": export_row,
    "text_or_none": text_or_none,
    "int_or_none": int_or_none,
    "int_or_bounded": int_or_bounded,
    "float_or_finite": float_or_finite,
    "json_object_or_none": json_object_or_none,
    "non_finite_path": non_finite_path,
    "load_json": load_json,
    "message_content": message_content,
    "normalize_message": normalize_message,
    "normalize_session": normalize_session,
    "validate_payload": validate_payload,
    "foreign_match": foreign_match,
    "parent_attach": parent_attach,
    "import_flags": import_flags,
    "message_split": message_split,
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
            print("MISMATCH %s case=%r\n  py=%r\n  c =%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
