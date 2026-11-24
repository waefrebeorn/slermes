#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/prune_rewrite.py
(gate w19_prunerewri_17bd63).

Extracts the live FunctionDefs via ast.parse and runs them against a scripted connection, so the
real chunked SELECTs execute while the C side ports the comparator they feed.
"""
import ast
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = Path("C:/Users/eman5/hermes-agent")
SRC = Q / "persistence" / "sqlite" / "conversation" / "_store" / "prune_rewrite.py"
GATE = "w19_prunerewri_17bd63"

PERSISTED_MARKER = "__db_persisted"
ROW_SNAPSHOT = "__row_snapshot"


def _stub_module(dotted, **attrs):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        name = ".".join(parts[:i])
        if name not in sys.modules:
            mod = types.ModuleType(name)
            mod.__path__ = []
            sys.modules[name] = mod
    mod = sys.modules[dotted]
    for key, value in attrs.items():
        setattr(mod, key, value)
    return mod


def message_uid_or_none(message):
    uid = message.get("message_uid") if isinstance(message, dict) else None
    return uid if isinstance(uid, str) else None


def transcript_row_snapshot(row):
    return row.get(ROW_SNAPSHOT)


def row_identity(role, content, tool_call_id, calls):
    """The loader-lens identity, joined so both sides compare one string."""
    ids = [c.get("id") if isinstance(c, dict) else None for c in (calls or [])]
    return "\x1f".join([str(role),
                        "" if content is None else str(content),
                        "" if tool_call_id is None else str(tool_call_id),
                        ",".join("" if i is None else str(i) for i in ids)])


def parse_tool_calls(value):
    return value if isinstance(value, list) else None


def decode_content(value):
    return value


def id_chunks(mapping):
    keys = list(mapping)
    return [keys] if keys else []


def placeholders(chunk):
    return ",".join("?" for _ in chunk)


_stub_module("session_rules")
_stub_module("session_rules.turns", _DB_PERSISTED_MARKER=type("M", (), {"__str__": lambda s: PERSISTED_MARKER})(),
             message_uid_or_none=message_uid_or_none)
_stub_module("persistence.sqlite.conversation._store.transcript_repair",
             transcript_row_snapshot=transcript_row_snapshot)


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "w19_prune_live", "__builtins__": __builtins__,
         "_DB_PERSISTED_MARKER_KEY": PERSISTED_MARKER, "DB_ROW_SNAPSHOT": ROW_SNAPSHOT,
         "_id_chunks": id_chunks, "_placeholders": placeholders,
         "message_uid_or_none": message_uid_or_none,
         "transcript_row_snapshot": transcript_row_snapshot,
         "_row_identity": row_identity, "_parse_tool_calls": parse_tool_calls,
         "_decode_content": decode_content, "Any": object, "Optional": object}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in (
                "held_row_mismatches", "rewritten_held_row_ids"):
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
        if isinstance(node, ast.Assign) and any(
                getattr(t, "id", None) == "_PRUNE_FIXED_COLUMNS" for t in node.targets):
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
    return g


G = extract()


class FakeCursor:
    def __init__(self, rows):
        self._rows = rows

    def fetchall(self):
        return list(self._rows)

    def fetchone(self):
        return self._rows[0] if self._rows else None


class FakeConn:
    """Answers the two SELECTs held_row_mismatches issues, from the fixture's row list."""

    def __init__(self, rows):
        self.rows = rows
        self.queries = []

    def execute(self, sql, params=()):
        self.queries.append(sql)
        if "id IN" in sql:
            session_id, ids = params[0], set(params[1:])
            return FakeCursor([r for r in self.rows
                               if r.get("id") in ids and r.get("session_id") == session_id])
        session_id, role, uid = params
        matches = [r for r in self.rows
                   if r.get("session_id") == session_id and int(r.get("active") or 0)
                   and r.get("role") == role and r.get("message_uid") == uid]
        matches.sort(key=lambda r: r["id"], reverse=True)
        return FakeCursor(matches[:1])


class Holder:
    def _read_retrying_ioerr(self, fn):  # pragma: no cover - only rewritten_held_row_ids
        return fn(None)


def run_case(r):
    f = r.get("func")
    if f == "exact_id":
        message = r.get("message") or {}
        rid = message.get("_row_id")
        if isinstance(rid, int) and not isinstance(rid, bool) and rid > 0:
            return rid
        return -1
    if f == "call_ids":
        parsed = G["_parse_tool_calls"](r.get("tool_calls"))
        return [call.get("id") if isinstance(call, dict) else None for call in (parsed or [])]
    if f == "held_row_mismatches":
        conn = FakeConn(r.get("stored") or [])
        return G["held_row_mismatches"](Holder(), conn, r.get("session_id") or "",
                                       r.get("held") or [])
    raise KeyError(f)


FUNCS = {"exact_id", "call_ids", "held_row_mismatches"}


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
        f = r.get("func")
        if f not in FUNCS:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
