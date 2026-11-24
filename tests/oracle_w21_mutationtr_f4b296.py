#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/mutation_transcript.py
(gate w21_mutationtr_f4b296).

`rewind_in_transaction` is imported from the live module and executed for real
over a fake sqlite3 connection that records every `execute(sql, params)` and
answers the four row-returning statements from the fixture.

Seams, named:
* the sqlite connection (`conn.execute(...).fetchone()/fetchall()`) -- the
  fixture's `db` block; the port emits the same statement log.
* `messages.py`'s `_check_transcript_write_guards` / `_split_rewind_target` /
  `_insert_message_rows` / `_active_transcript_counts` -- the fixture's
  `guards` / `split` / (recorded) / `counts`. Those four live in another module
  (the messages gate); what this module owns is when they are called, with what
  arguments, and what it does with their answers -- all compared here.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "persistence" / "sqlite" / "conversation" / "_store" / "mutation_transcript.py"
GATE = "w21_mutationtr_f4b296"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("mutation_transcript_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["mutation_transcript_live"] = live
_spec.loader.exec_module(live)


def _exc(name, message):
    """An exception whose type name and message are the fixture's."""
    return type(name, (Exception,), {})(message)


class Cursor:
    def __init__(self, one=None, many=None):
        self._one = one
        self._many = many if many is not None else []

    def fetchone(self):
        return self._one

    def fetchall(self):
        return self._many


class FakeConn:
    """Answers the statements rewind_in_transaction issues, and logs them all."""

    def __init__(self, db):
        self.db = db or {}
        self.statements = []

    def execute(self, sql, params=()):
        self.statements.append({"sql": sql, "params": list(params)})
        text = sql.strip()
        if text.startswith("SELECT id FROM messages WHERE session_id = ? AND active = 1 ORDER BY id"):
            return Cursor(many=[[row] for row in (self.db.get("active_ids") or [])])
        if text.startswith("SELECT * FROM messages WHERE id = ? AND session_id = ?"):
            return Cursor(one=self.db.get("row"))
        if text.startswith("SELECT id FROM messages WHERE session_id = ? AND id >= ? AND active = 1"):
            return Cursor(many=[[row] for row in (self.db.get("rewound_ids") or [])])
        if text.startswith("SELECT last_insert_rowid()"):
            return Cursor(one=[self.db.get("last_rowid")])
        if text.startswith("SELECT MAX(id) FROM messages"):
            return Cursor(one=[self.db.get("head_id")])
        return Cursor()


class Holder:
    """The store holder: none of its state is read by the ported body."""


def do_rewind(r):
    db = r.get("db") or {}
    conn = FakeConn(db)
    recorded = {}
    guards = r.get("guards") or {}
    split = r.get("split") or {}

    def fake_guards(holder, conn_, session_id, compression_lock_holder, **kwargs):
        recorded["guard_args"] = {
            "session_id": session_id,
            "compression_lock_holder": compression_lock_holder,
            "reject_active_turn_lease": kwargs.get("reject_active_turn_lease"),
            "reject_active_compression_lock": kwargs.get("reject_active_compression_lock"),
        }
        if guards.get("action") == "raise":
            raise _exc(guards.get("type", "Exception"), guards.get("message", ""))

    def fake_split(holder, target_row, expected_target_content, preserve_compaction_handoff):
        recorded["split_args"] = {
            "target_row": target_row,
            "expected_target_content": expected_target_content,
            "preserve_compaction_handoff": preserve_compaction_handoff,
        }
        if split.get("action") == "raise":
            raise _exc(split.get("type", "Exception"), split.get("message", ""))
        if split.get("action") == "return":
            return split.get("value")
        return None

    def fake_insert(holder, conn_, session_id, messages, **kwargs):
        recorded.setdefault("inserts", []).extend(messages)

    def fake_counts(conn_, session_id):
        return tuple(r.get("counts") or [0, 0])

    live._check_transcript_write_guards = fake_guards
    live._split_rewind_target = fake_split
    live._insert_message_rows = fake_insert
    live._active_transcript_counts = fake_counts

    try:
        target_row, ids, head_id, replacement_message_id = live.rewind_in_transaction(
            Holder(), conn, r.get("session_id", ""), r.get("target_message_id"),
            preserve_compaction_handoff=bool(r.get("preserve_compaction_handoff", False)),
            expected_active_ids=r.get("expected_active_ids"),
            expected_target_content=r.get("expected_target_content"))
    except Exception as exc:  # noqa: BLE001 - the port reports raised errors this way
        return {"status": "raised", "type": type(exc).__name__, "message": str(exc)}
    return {
        "status": "ok",
        "target_row": target_row,
        "ids": ids,
        "head_id": head_id,
        "replacement_message_id": replacement_message_id,
        "guard_args": recorded.get("guard_args"),
        "split_args": recorded.get("split_args"),
        "inserts": recorded.get("inserts"),
        "statements": conn.statements,
    }


def expected(r):
    f = r.get("func")
    if f == "rewind":
        return do_rewind(r)
    if f == "placeholders":
        return ",".join("?" for _ in range(int(r.get("count") or 0)))
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"rewind", "placeholders"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
