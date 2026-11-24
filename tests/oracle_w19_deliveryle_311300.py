#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/gateway/_delivery_ledger.py (gate w19_deliveryle_311300).

The live obligation verbs are executed against a real in-memory SQLite ledger built from the
fixture's rows: same table DDL (the module's own ``_ensure_obligations_table``), same SQL. The
compared value is the claimed tuple list plus the rows as the transaction left them.
"""
import ast
import contextlib
import dataclasses
import importlib.util
import json
import sqlite3
import sys
import types
import __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
Q = REPO.parent / "hermes-agent"
SRC = Q / "persistence" / "sqlite" / "gateway" / "_delivery_ledger.py"
GATE = "w19_deliveryle_311300"
_ANN = __future__.annotations.compiler_flag

WANT_FUNCS = {"_ensure_obligations_table", "_prune_unlocked", "_claimed_row", "_expected_profile"}
WANT_METHODS = {"_obligation_write", "_obligation_read", "sweep_recoverable_obligations",
                "sweep_failed_obligations_for_runtime", "list_runtime_retry_candidates"}

if str(Q) not in sys.path:
    sys.path.insert(0, str(Q))

import persistence.authority.coordination as coord  # noqa: E402


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {
        "__name__": "dl_live",
        "time": types.SimpleNamespace(time=lambda: 0.0),
        "translated": contextlib.nullcontext,
        "add_column_if_missing": lambda *a, **k: True,
        "Optional": object, "Any": object, "Sequence": object, "Callable": object,
        "TypeVar": lambda *a, **k: object,
    }
    for name in dir(coord):
        if name.isupper() or name in ("ClaimedObligation", "FailedObligationRow", "OwnerStamp",
                                      "RecordedOwner", "RetryCandidate", "RetryDue",
                                      "flood_not_before", "is_flood_error"):
            g[name] = getattr(coord, name)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANT_FUNCS:
            _run(node, g)
        elif isinstance(node, ast.ClassDef) and node.name == "DeliveryObligationVerbs":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in WANT_METHODS:
                    _run(sub, g)
    return g


def _run(node, g):
    mod = ast.Module(body=[node], type_ignores=[])
    exec(compile(mod, str(SRC), "exec", flags=_ANN, dont_inherit=True), g)


G = extract()

COLS = ("obligation_id, session_key, platform, chat_id, thread_id, content, state, attempts, "
        "created_at, updated_at, owner_pid, owner_started_at, last_error, adapter_profile")


class Host:
    """The store seam the verbs expect: setup + write + read on one connection."""

    def __init__(self, conn):
        self.conn = conn

    def _ledger_setup(self, fn):
        fn(self.conn)
        self.conn.commit()

    def _ledger_write(self, fn):
        out = fn(self.conn)
        self.conn.commit()
        return out

    def _ledger_read(self, fn):
        return fn(self.conn)


# The verbs reach their store through the class's own table-step wrappers.
for _name in ("_obligation_write", "_obligation_read"):
    setattr(Host, _name, G[_name])


def fresh(rows):
    conn = sqlite3.connect(":memory:")
    conn.row_factory = sqlite3.Row
    G["_ensure_obligations_table"](conn)
    for row in rows or []:
        conn.execute("INSERT INTO delivery_obligations (%s) VALUES (%s)" % (COLS, ",".join("?" * 14)),
                     [row.get(c) for c in COLS.split(", ")])
    conn.commit()
    return conn


def dump_rows(conn):
    return [dict(r) for r in conn.execute("SELECT %s FROM delivery_obligations ORDER BY rowid" % COLS)]


def case_expected_profile(r):
    return G["_expected_profile"](r.get("profile"))


def case_sweep_recoverable(r):
    conn = fresh(r.get("rows"))
    owner = coord.OwnerStamp(pid=r.get("owner_pid"), started_at=r.get("owner_started_at"))
    dead = frozenset(coord.RecordedOwner(pid=d.get("pid"), started_at=d.get("started_at"))
                     for d in (r.get("dead_owners") or []))
    platforms = r.get("platforms")
    targets = r.get("targets")
    claimed = G["sweep_recoverable_obligations"](
        Host(conn), now=r.get("now"), deliverable_platforms=None if platforms is None else frozenset(platforms),
        deliverable_targets=None if targets is None else frozenset(tuple(t) for t in targets),
        owner=owner, dead_owners=dead)
    return {"claimed": [dataclasses.asdict(c) for c in claimed], "rows": dump_rows(conn)}


def case_sweep_runtime(r):
    conn = fresh(r.get("rows"))
    owner = coord.OwnerStamp(pid=r.get("owner_pid"), started_at=r.get("owner_started_at"))
    due = tuple(coord.RetryDue(**d) for d in (r.get("due") or []))
    claimed = G["sweep_failed_obligations_for_runtime"](
        Host(conn), r.get("platform", ""), now=r.get("now"), profile=r.get("profile"),
        owner=owner, due=due)
    return {"claimed": [dataclasses.asdict(c) for c in claimed], "rows": dump_rows(conn)}


def case_runtime_candidates(r):
    conn = fresh(r.get("rows"))
    owner = coord.OwnerStamp(pid=r.get("owner_pid"), started_at=r.get("owner_started_at"))
    got = G["list_runtime_retry_candidates"](Host(conn), r.get("platform", ""), profile=r.get("profile"),
                                             owner=owner)
    return [dataclasses.asdict(c) for c in got]


def case_prune(r):
    conn = fresh(r.get("rows"))
    G["_prune_unlocked"](conn, r.get("now"))
    conn.commit()
    return dump_rows(conn)


CASES = {
    "expected_profile": case_expected_profile,
    "sweep_recoverable": case_sweep_recoverable,
    "sweep_runtime": case_sweep_runtime,
    "runtime_candidates": case_runtime_candidates,
    "prune": case_prune,
}


def norm(v):
    return json.loads(json.dumps(v, default=str))


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
        if f not in CASES:
            continue
        cases += 1
        try:
            py = norm(CASES[f](r))
        except BaseException as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
