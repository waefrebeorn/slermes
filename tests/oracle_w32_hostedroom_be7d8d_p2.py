#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/gateway/hosted_room_policy.py
(gate w32_hostedroom_be7d8d_p2: statement text, dispatch and coercions).

The live module is imported as a package module.  ``publication_exists`` is
driven through a scripted connection and a stub file handle -- ``_hosted_read``
runs the verb's read against a recorder that captures the exact statement the
deferred/terminal branch built -- and ``_settled_message`` through a one-row
connection, so both predicates are the live ones.  Everything else is compared
against the live module's own constants (the DDL tuple, the statement
constants, ``_APPLY_BY_KIND``, ``_TERMINAL_KINDS``, ``_text``); no file is
opened, no transaction is begun.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w32_hostedroom_be7d8d_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))

from persistence.sqlite.gateway import hosted_room_policy as live  # noqa: E402

WANTED = {
    "text", "apply_handler", "named_sql", "schema_ddl", "reopened",
    "publication_query", "event_seqs", "settled_check", "receipt",
    "transcript_retention",
}

NAMED = {
    "delete_active": "_DELETE_ACTIVE_EVENTS_SQL",
    "transcript": "_TRANSCRIPT_EVENTS_SQL",
    "room_columns": "_ROOM_EVENT_COLUMNS",
}

# JSON carries no int/float distinction; the fixture names the Python type.
TYPE_COERCE = {
    "int": int,
    "float": float,
    "bool": bool,
    "str": str,
    "null": lambda v: None,
}


class Row(dict):
    """A sqlite3.Row stand-in that answers by name and by position."""

    def __init__(self, values):
        super().__init__(values)
        self._v = list(values.values())

    def __getitem__(self, key):
        if isinstance(key, int):
            return self._v[key]
        return super().__getitem__(key)


class Cur:
    def __init__(self, rows=None):
        self.rows = list(rows or [])

    def fetchone(self):
        return self.rows[0] if self.rows else None


class Recorder:
    """Answers every execute with one fixed row (or none) and records the SQL."""

    def __init__(self, row=None):
        self.row = row

    def execute(self, sql, params=()):
        return Cur([self.row] if self.row is not None else [])


class _Files:
    def __init__(self, conn):
        self.conn = conn

    def _hosted_read(self, placement, fn):
        return fn(self.conn)


def text(req):
    kind = req.get("value_type") or "int"
    coerce = TYPE_COERCE.get(kind)
    mapping = req.get("mapping")
    mapping = mapping if isinstance(mapping, dict) else {}
    # The transport echoes numbers without their Python type; the fixture names
    # it, so the value handed to the live coercion carries that type again.
    typed = {k: (coerce(v) if coerce and isinstance(v, (int, float)) and not isinstance(v, bool) else v)
             for k, v in mapping.items()}
    return live._text(typed, str(req.get("key") or ""))


def apply_handler(req):
    handler = live._APPLY_BY_KIND.get(req.get("kind"))
    return handler.__name__ if handler is not None else None


def named_sql(req):
    name = req.get("name")
    attr = NAMED.get(name)
    return getattr(live, attr) if attr else None


def schema_ddl(req):
    return list(live._SCHEMA_DDL)


def reopened(req):
    kinds = list(live._REOPENED_KINDS)
    return {"kinds": kinds, "placeholders": ",".join("?" * len(kinds)), "count": len(kinds)}


def publication_query(req):
    status = req.get("status") or ""
    captured = {}

    class Conn:
        def execute(self, sql, params=()):
            captured["sql"] = sql
            captured["params"] = tuple(params)
            return Cur()

    repo = live.SQLiteHostedRoomPolicyRepository.__new__(live.SQLiteHostedRoomPolicyRepository)
    repo._files = _Files(Conn())
    repo._prepare = lambda placement, bound: live.MAX_ACTIVE_POLICY_EVENTS
    repo.publication_exists(placement=None, room_id="room", task_id="dtask:abc",
                            status=status, execution_generation=3)
    sql = captured["sql"]
    params = captured["params"]
    exact = "execution_generation" in sql
    kinds = [params[2]] if exact else ["turn.settled", "turn.failed", "turn.cancelled"]
    return {"sql": sql, "kinds": kinds, "uses_generation": exact}


def event_seqs(req):
    source = req.get("source_event_seq")
    ctx = req.get("event_seqs")
    ctx = ctx if isinstance(ctx, list) else []
    seqs = sorted({int(source), *(int(s) for s in ctx)})
    return {"seqs": seqs, "placeholders": ",".join("?" * len(seqs)), "count": len(seqs)}


def settled_check(req):
    row = req.get("row")
    conn = None
    if isinstance(row, dict):
        conn = Recorder(Row({
            "room_id": "room",
            "seq": row.get("seq", 1),
            "event_id": row.get("event_id", "dmessage:abc"),
            "kind": row.get("kind"),
            "actor_json": json.dumps(row.get("actor") or {}),
            "authority_epoch": row.get("authority_epoch"),
            "payload_json": json.dumps(row.get("payload") or {}),
            "created_at": row.get("created_at", 0.0),
        }))
    else:
        conn = Recorder()
    event = live._settled_message(conn, "room", str(req.get("discussion_event_id") or ""),
                                  req.get("message_event_id"))
    return event is not None


def receipt(req):
    kind = live._text({"kind": req.get("kind")}, "kind")
    payload = req.get("payload")
    payload = payload if isinstance(payload, dict) else {}
    terminal = kind in live._TERMINAL_KINDS
    task_id = live._text(payload, "task_id")
    execution_generation = int(payload.get("execution_generation") or 0) if kind == "turn.deferred" else 0
    member_id = live._text(payload, "member_id")
    seen_through_seq = int(payload.get("seen_through_seq") or 0)
    if kind == "turn.settled" and payload.get("message_event_id"):
        path = "settled"
    elif terminal:
        path = "transcript"
    else:
        path = None
    return {
        "terminal": terminal,
        "publish": bool(terminal and task_id),
        "task_id": task_id,
        "execution_generation": execution_generation,
        "member_id": member_id,
        "seen_through_seq": seen_through_seq,
        "watermark": bool(terminal and member_id and seen_through_seq > 0),
        "path": path,
    }


def transcript_retention(req):
    seqs = req.get("seqs")
    seqs = seqs if isinstance(seqs, list) else []
    distinct = sorted({int(s) for s in seqs if isinstance(s, (int, float))})
    offset = live.MAX_THREAD_TRANSCRIPT_EVENTS - 1
    cutoff = distinct[-1 - offset] if len(distinct) > offset else None
    return {"cutoff": cutoff, "delete_below": cutoff, "offset": offset}


def expected(r):
    f = r.get("func")
    if f not in WANTED:
        raise AssertionError("unknown func %r" % (f,))
    return globals()[f](r)


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
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
