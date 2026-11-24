#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/authority/subagents.py
(gate w32_subagents_a171ba_p2: notes, cancels, decisions and predicates).

The ledger connection is stubbed (a fake ``execute`` that records the statement
and answers a canned ``fetchone()`` row); the compression-chain walk and the
clock are seams the caller supplies values for.  What is compared is exactly
what those values decide: the exact note/cancel/hand-back statements, the
supersession advisory, and the owed / unstarted-claim / attempt predicates.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w32_subagents_a171ba_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))  # hermes_seam_stubs

from persistence.sqlite.authority import subagents as live  # noqa: E402
from persistence.authority import subagents as dom  # noqa: E402

WANTED = {"note", "cancel_expired", "cancel_manual_claim", "parent_note",
          "predicates", "hand_back", "refusal", "constants"}


class FakeConn:
    """Records every statement; answers canned fetchone() rows."""

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


def _states(values):
    return tuple(dom.SubagentState(v) for v in values)


def note(r):
    conn = FakeConn(rowcount=r.get("rowcount", 1))
    state = r.get("state")
    from_states = r.get("from_states")
    if from_states is None:
        from_states = [dom.SubagentState.RESUMABLE.value]
    ok = live._note(
        conn, r.get("subagent_id"),
        state=dom.SubagentState(state) if state else None,
        note=r.get("note"), now=r.get("now"),
        from_states=_states(from_states),
        unpin=bool(r.get("unpin")), unstarted_claim=bool(r.get("unstarted_claim")))
    call = conn.calls[-1]
    return {"sql": call["sql"], "params": call["params"], "ok": bool(ok)}


def cancel_expired(r):
    conn = FakeConn(rowcount=r.get("rowcount", 1))
    ok = live._cancel_expired(conn, r.get("subagent_id"), r.get("note"), r.get("now"))
    call = conn.calls[-1]
    return {"sql": call["sql"], "params": call["params"], "ok": bool(ok)}


def cancel_manual_claim(r):
    conn = FakeConn(rowcount=r.get("rowcount", 1))
    ok = live._cancel_manual_claim(conn, r.get("subagent_id"),
                                   state=dom.SubagentState(r.get("state")),
                                   note=r.get("note"), now=r.get("now"))
    call = conn.calls[-1]
    return {"sql": call["sql"], "params": call["params"], "ok": bool(ok)}


def parent_note(r):
    return dom.parent_superseded_note(r.get("tip_end_reason"),
                                      tip_exists=bool(r.get("tip_exists")))


def _record(d):
    return dom.SubagentRecord(
        subagent_id=d.get("subagent_id"), state=dom.SubagentState(d.get("state")),
        pinned=bool(d.get("pinned")), stop_cause=d.get("stop_cause"),
        owner_pid=d.get("owner_pid"), owner_started_at=d.get("owner_started_at"),
        document=dict(d.get("document") or {}), created_at=d.get("created_at"),
        updated_at=d.get("updated_at"), expires_at=d.get("expires_at"))


def predicates(r):
    rec = _record(r.get("record") or {})
    return {"owed": bool(rec.owed),
            "unstarted_claim": bool(live._unstarted_auto_claim(rec)),
            "attempts": live._attempts(rec)}


def hand_back(r):
    found = r.get("found")
    conn = FakeConn([tuple(found)] if isinstance(found, list) else [])
    live.SubagentLedgerVerbs.hand_back_subagent(
        FakeVerbs(conn), subagent_id=r.get("subagent_id"),
        state=dom.SubagentState(r.get("state")), promised=r.get("promised"),
        owner_pid=r.get("owner_pid"))
    if len(conn.calls) < 2:  # no claimed row answered: nothing written
        return None
    call = conn.calls[-1]
    return {"sql": call["sql"], "params": call["params"]}


def refusal(r):
    conn = FakeConn()
    live.SubagentLedgerVerbs.settle_auto_refusal(
        FakeVerbs(conn), subagent_id=r.get("subagent_id"),
        kind=dom.AutoRefusal(r.get("kind")), attempts=r.get("attempts"),
        note=r.get("note"), owner_pid=r.get("owner_pid"))
    call = conn.calls[-1]
    return {"sql": call["sql"], "params": call["params"]}


def constants(_r):
    return {
        "owed_sql": live.OWED_SQL,
        "cancel_debt": live.CANCEL_DEBT,
        "keep_waiting_units_sql": live.KEEP_WAITING_UNITS_SQL,
        "cancel_states": [s.value for s in live._CANCEL_STATES],
        "auto_resume_max_attempts": float(dom.AUTO_RESUME_MAX_ATTEMPTS),
        "gateway_shutdown": dom.GATEWAY_SHUTDOWN,
        "auto_resume_cancelled": dom.AUTO_RESUME_CANCELLED,
    }


def expected(r):
    f = r.get("func")
    if f == "note":
        return note(r)
    if f == "cancel_expired":
        return cancel_expired(r)
    if f == "cancel_manual_claim":
        return cancel_manual_claim(r)
    if f == "parent_note":
        return parent_note(r)
    if f == "predicates":
        return predicates(r)
    if f == "hand_back":
        return hand_back(r)
    if f == "refusal":
        return refusal(r)
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
