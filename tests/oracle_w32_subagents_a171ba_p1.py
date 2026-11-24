#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/authority/subagents.py
(gate w32_subagents_a171ba_p1: row shaping + write statement builders).

The live module is imported as a package module; the ledger connections are
stubbed by a fake ``execute`` that records ``(sql, params)`` and answers the
canned ``fetchone()`` rows.  That is the seam: no store is opened and what is
compared is exactly what the values decide -- the shaped record / route, the
exact statement text and parameter tuple of each write verb, and the recovery
owner gate.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w32_subagents_a171ba_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))  # hermes_seam_stubs

from persistence.sqlite.authority import subagents as live  # noqa: E402

WANTED = {"record_shape", "route_shape", "upsert", "apply_limits",
          "insert_completion", "recover_guard", "constants"}


class FakeConn:
    """Records every statement; answers canned fetchone()/fetchall() rows."""

    def __init__(self, fetch=None, rowcount=1):
        self.fetch = list(fetch or [])
        self.rowcount = rowcount
        self.calls = []
        self._last = []

    def execute(self, sql, params=()):
        self.calls.append({"sql": sql, "params": list(params), "rc": self.rowcount})
        self._last = list(self.fetch)
        self.fetch = []
        return self

    def fetchone(self):
        return self._last.pop(0) if self._last else None

    def fetchall(self):
        out = list(self._last)
        self._last = []
        return out


class FakeVerbs(live.SubagentLedgerVerbs):
    """The verbs with their ledger seam replaced by the fake connection."""

    def __init__(self, conn):
        self.conn = conn

    def _ledger_write(self, fn):
        return fn(self.conn)

    def _ledger_read(self, fn):
        return fn(self.conn)


def record_shape(r):
    rec = live._record(tuple(r.get("row") or []))
    return {
        "subagent_id": rec.subagent_id,
        "state": rec.state.value,
        "pinned": rec.pinned,
        "stop_cause": rec.stop_cause,
        "owner_pid": rec.owner_pid,
        "owner_started_at": rec.owner_started_at,
        "document": dict(rec.document),
        "created_at": rec.created_at,
        "updated_at": rec.updated_at,
        "expires_at": rec.expires_at,
    }


def route_shape(r):
    conn = FakeConn([tuple(r.get("row") or [])])
    got = live.SubagentLedgerVerbs.get_delegation_route(
        FakeVerbs(conn), delegation_id=r.get("delegation_id"))
    if got is None:
        return None
    return {
        "delegation_id": str(got.delegation_id),
        "origin_session": got.origin_session,
        "origin_ui_session_id": got.origin_ui_session_id,
        "parent_session_id": got.parent_session_id,
        "origin_session_id": got.origin_session_id,
        "task": got.task,
        "result": got.result,
        "dispatched_at": got.dispatched_at,
        "updated_at": got.updated_at,
    }


def _write(w):
    return live.SubagentWrite(state=w.get("state"), pinned=bool(w.get("pinned")),
                              stop_cause=w.get("stop_cause"), owner_pid=w.get("owner_pid"),
                              owner_started_at=w.get("owner_started_at"),
                              document=dict(w.get("document") or {}),
                              expires_at=w.get("expires_at"))


def upsert(r):
    conn = FakeConn()
    live._upsert(conn, r.get("subagent_id"), _write(r.get("write") or {}), r.get("now"))
    call = conn.calls[-1]
    params = call["params"]
    # params[6] is json.dumps(record): compare the value, not its spacing
    params[6] = json.loads(params[6]) if isinstance(params[6], str) else params[6]
    return {"sql": call["sql"], "params": params}


def apply_limits(r):
    conn = FakeConn()
    lim = r.get("limits") or {}
    live._apply_limits(conn, live.SubagentLimits(ttl_hours=lim.get("ttl_hours"),
                                                 max_resumable=lim.get("max_resumable")),
                       r.get("now"))
    return {"stmts": [{"sql": c["sql"], "params": c["params"]} for c in conn.calls]}


def insert_completion(r):
    comp = r.get("completion") or {}
    conn = FakeConn()
    live._insert_completion(conn, live.GaveUpCompletion(
        unit_id=comp.get("unit_id"), unit_updated_at=comp.get("unit_updated_at"),
        event=dict(comp.get("event") or {}), result=dict(comp.get("result") or {}),
        owner_pid=comp.get("owner_pid"), owner_started_at=comp.get("owner_started_at")),
        r.get("now"))
    call = conn.calls[-1]
    params = call["params"]
    for i in (7, 8, 11):  # json.dumps(event), json.dumps(result), json.dumps(task)
        params[i] = json.loads(params[i]) if isinstance(params[i], str) else params[i]
    return {"sql": call["sql"], "params": params}


def recover_guard(r):
    rec = r.get("recovery") or {}
    row = r.get("row")
    conn = FakeConn([tuple(row)] if isinstance(row, list) else [])
    w = live.SubagentWrite(state="resumable", pinned=True, stop_cause=None, owner_pid=None,
                           owner_started_at=None, document={"goal": "g"}, expires_at=None)
    live.recover_subagents(conn, [live.SubagentRecovery(
        subagent_id="sa-1", owner_pid=rec.get("owner_pid"),
        owner_started_at=rec.get("owner_started_at"), write=w)], None, 5.0)
    # SELECT then, only when the guard passes, the upsert INSERT
    return len(conn.calls) == 2


def constants(_r):
    return {
        "columns": live._COLUMNS,
        "run_only": list(live._RUN_ONLY),
        "expired_retention_seconds": float(live.SUBAGENT_EXPIRED_RETENTION_SECONDS),
    }


def expected(r):
    f = r.get("func")
    if f == "record_shape":
        return record_shape(r)
    if f == "route_shape":
        return route_shape(r)
    if f == "upsert":
        return upsert(r)
    if f == "apply_limits":
        return apply_limits(r)
    if f == "insert_completion":
        return insert_completion(r)
    if f == "recover_guard":
        return recover_guard(r)
    if f == "constants":
        return constants(r)
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if (mism or cases == 0) else 0)


if __name__ == "__main__":
    main()
