#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/todo.py
(gate w21_todo_138ed8).

The paged sqlite walk is DB-bound (a StoreHolder plus a live cursor), so the DB
seam is stubbed and the *decisions* run live:

  is_call      -- the nested `is_todo_call` closure is lifted out of the AST and
                  executed against the quarry's `json` module. A live raise is
                  carried as -1 (the caller's `except (ValueError, TypeError)`
                  is what turns it into a skipped row).
  paired       -- the source's own `isinstance(calls, list) and any(...)` over
                  `json.loads(prior["tool_calls"])`, using that same live closure.
  valid        -- the row-content gate transcribed from get_latest_todo_result
                  (json.loads -> dict -> list `todos` -> int(revision or 0)).
  copy_ids     -- live `_todo_copies` with the display hash injected, so no
                  StoreHolder is needed for the dedupe key.
  removed      -- live `_removed_todo_copies` over a stub connection that hands
                  back the fixture rows (the sqlite seam).
  removed_hit  -- live `_todo_copy_removed`, with `_todo_copies` bound to the
                  already-rendered copy list (that rendering is the seam).

Copy identities are canonicalized to the strings the C side carries: the display
hash verbatim, every ("uid", uid) pair as "uid:<uid>".
"""
import ast
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "persistence" / "sqlite" / "conversation" / "_store" / "todo.py"
GATE = "w21_todo_138ed8"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

G = {"json": json}
_tree = ast.parse(SRC.read_text(encoding="utf-8"))
for _node in ast.walk(_tree):
    if isinstance(_node, (ast.FunctionDef, ast.AsyncFunctionDef)) and _node.name == "is_todo_call":
        _node.decorator_list = []
        exec(compile(ast.Module(body=[_node], type_ignores=[]), str(SRC), "exec"), G)
        break
else:  # pragma: no cover - the closure must exist
    raise SystemExit("is_todo_call not found in %s" % SRC)
is_todo_call = G["is_todo_call"]

_spec = importlib.util.spec_from_file_location("todo_store_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["todo_store_live"] = live
_spec.loader.exec_module(live)

WANTED = {"is_call", "paired", "valid", "copy_ids", "removed", "removed_hit"}


def render_copy(c):
    if isinstance(c, tuple):
        return "uid:" + str(c[1])
    return c if isinstance(c, str) else str(c)


class FakeConn:
    """The sqlite seam: `conn.execute(...)` yields the fixture rows."""

    def __init__(self, rows):
        self.rows = rows

    def execute(self, _sql, _params=None):
        return iter(self.rows)


def paired(text, call_id):
    G["call_id"] = call_id
    try:
        calls = json.loads(text) if text else []
        return 1 if isinstance(calls, list) and any(is_todo_call(c) for c in calls) else 0
    except (ValueError, TypeError):
        return 0


def valid(content):
    try:
        data = json.loads(content)
        if not isinstance(data, dict) or not isinstance(data.get("todos"), list):
            return 0
        int(data.get("revision") or 0)
    except (ValueError, TypeError):
        return 0
    return 1


def copy_ids(identity, message_uid, absorbed):
    row = {"message_uid": message_uid, "absorbed_message_uids": absorbed}
    return [render_copy(c) for c in live._todo_copies(None, row, identity)]


def removed(rows):
    norm = [{"id": int(r.get("id") or 0), "display_identity": r.get("display_identity"),
             "message_uid": r.get("message_uid"),
             "absorbed_message_uids": r.get("absorbed_message_uids")} for r in rows]
    got = live._removed_todo_copies(None, FakeConn(norm), ["s1"])
    return {render_copy(k): str(v) for k, v in got.items()}


def removed_hit(removed_in, copies, row_id):
    orig = live._todo_copies
    live._todo_copies = lambda holder, row, identity=None: list(copies)
    try:
        mapped = {k: int(v) for k, v in (removed_in or {}).items()}
        return 1 if live._todo_copy_removed(None, {"id": int(row_id or 0)}, mapped) else 0
    finally:
        live._todo_copies = orig


def expected(r):
    f = r.get("func")
    if f == "is_call":
        G["call_id"] = r.get("call_id") or ""
        try:
            return 1 if is_todo_call(r.get("call")) else 0
        except (ValueError, TypeError):
            return -1
    if f == "paired":
        return paired(r.get("tool_calls"), r.get("call_id") or "")
    if f == "valid":
        return valid(r.get("content"))
    if f == "copy_ids":
        return copy_ids(r.get("identity"), r.get("message_uid"), r.get("absorbed"))
    if f == "removed":
        return removed(r.get("rows") or [])
    if f == "removed_hit":
        return removed_hit(r.get("removed") or {}, r.get("copy_ids") or [], r.get("row_id"))
    raise AssertionError("unknown func %r" % (f,))


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
