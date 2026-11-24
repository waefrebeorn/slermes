#!/usr/bin/env python3
"""Differential oracle for hermes_cli/kanban_db_default_assignee.py
(gate w21_kanbandbde_905b79).

The two module functions are lifted out of the live source and executed with
the module's own late-bound collaborators stubbed:

* `_kb` (hermes_cli.kanban_db) -- `write_txn` is a no-op transaction, and
  `_append_event` records the call it receives;
* `_log` -- a logger whose `.debug(...)` swallows whatever it is handed;
* the sqlite connection -- a scripted connection that raises when the case says
  so, answers the conditional UPDATE with the case's rowcount, and the
  post-failure SELECT with the case's row (or None).
"""
import __future__
import ast
import contextlib
import json
import sqlite3
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = Path("C:/Users/eman5/hermes-agent")
SRC = QUARRY / "hermes_cli" / "kanban_db_default_assignee.py"
GATE = "w21_kanbandbde_905b79"
_ANN = __future__.annotations.compiler_flag

EVENTS = []


class _Log:
    def debug(self, *args, **kwargs):
        pass

    def warning(self, *args, **kwargs):
        pass


class _KB:
    @contextlib.contextmanager
    def write_txn(self, conn):
        yield

    def _append_event(self, conn, task_id, kind, payload):
        EVENTS.append({"task_id": task_id, "kind": kind, "payload": payload})


class _RowCount:
    def __init__(self, rowcount):
        self.rowcount = rowcount


class _Select:
    def __init__(self, row):
        self._row = row

    def fetchone(self):
        return self._row


class _Conn:
    def __init__(self, rowcount, raises, row):
        self.rowcount = rowcount
        self.raises = raises
        self.row = row

    def execute(self, sql, params=None):
        if self.raises and sql.strip().upper().startswith("UPDATE"):
            raise sqlite3.OperationalError("disk I/O error")
        if sql.strip().upper().startswith("UPDATE"):
            return _RowCount(self.rowcount)
        return _Select(self.row)


class _Result:
    def __init__(self, result):
        result = result if isinstance(result, dict) else {}
        self.skipped_unassigned = list(result.get("skipped_unassigned") or [])
        self.auto_assigned_default = list(result.get("auto_assigned_default") or [])


def _extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    env = {"__name__": "kanban_default_assignee_live", "_kb": _KB(), "_log": _Log()}
    wanted = {"apply_default_assignee", "fill_default_assignee"}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in wanted:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), env)
    return env


G = _extract()


def expected(r):
    del EVENTS[:]
    conn = _Conn(int(r.get("rowcount", -1) or 0), bool(r.get("raises")),
                 r.get("current_row"))
    dry_run = bool(r.get("dry_run"))
    if r.get("func") == "apply":
        applied = G["apply_default_assignee"](conn, r.get("task_id"), r.get("assignee"),
                                             dry_run=dry_run)
        return {"applied": bool(applied), "events": list(EVENTS)}
    result = _Result(r.get("result"))
    assignee = G["fill_default_assignee"](conn, r.get("task_id"), r.get("default_assignee"),
                                          result, dry_run=dry_run)
    return {"assignee": assignee,
            "skipped_unassigned": result.skipped_unassigned,
            "auto_assigned_default": result.auto_assigned_default,
            "events": list(EVENTS)}


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
        if not isinstance(r, dict) or r.get("func") not in ("apply", "fill"):
            continue
        cases += 1
        try:
            py = expected(r)
        except BaseException as exc:  # noqa: BLE001 - reported, never fatal
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
