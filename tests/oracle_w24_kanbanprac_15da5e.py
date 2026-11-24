#!/usr/bin/env python3
"""Differential oracle for hermes_cli/kanban_pr_acceptance_store.py (gate w24_kanbanprac_15da5e).

``prepare_acceptance`` is driven for real: a fake connection answers the two
task reads, ``write_txn`` becomes a null transaction and ``collect_acceptance``
records what it was handed instead of shelling out to ``gh``.  That is the seam
-- the decision chain, the snapshot re-read and the publish-once binding all run
as source code, and the C ``decide`` is compared against them.

``record_acceptance``'s tail is the receipt it was given: the returned value and
the ``last_failure_error`` text.  ``_PR``/``_REPO`` are compared by their own
patterns, ``validate_contract`` (the collaborator this module validates the
contract through) by its live function.
"""
import contextlib
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_kanbanprac_15da5e"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import kanban_pr_acceptance as acceptance  # noqa: E402
from hermes_cli import kanban_pr_acceptance_store as live  # noqa: E402

WANTED = {"decide", "pr_match", "contract_ok", "record_outcome", "constants"}

FAKE_RECEIPT = {"ok": True, "classification": "accepted", "recovery": ""}


class FakeCursor:
    def __init__(self, rows):
        self.rows = list(rows)

    def fetchone(self):
        return self.rows[0] if self.rows else None


class FakeConn:
    """Answers the snapshot read, the assignee read and the contract UPDATE."""

    def __init__(self, snapshot, assignee):
        self.snapshot = snapshot
        self.assignee = assignee
        self.updates = []

    def execute(self, sql, params=()):
        text = " ".join(str(sql).split())
        if text.startswith("SELECT current_run_id"):
            return FakeCursor([self.snapshot] if self.snapshot is not None else [])
        if text.startswith("UPDATE tasks SET completion_contract"):
            self.updates.append(tuple(params))
            return FakeCursor([])
        if text.startswith("SELECT assignee"):
            return FakeCursor([{"assignee": self.assignee}])
        raise AssertionError("unexpected SQL: %s" % text)


def decide(req):
    snap = req.get("snapshot")
    snapshot = None
    if isinstance(snap, dict):
        snapshot = (snap.get("run_id"), snap.get("status"), snap.get("contract"))
    seen = {}

    def fake_collect(contract, published_pr, *, assignee=None):
        seen["args"] = (contract, published_pr, assignee)
        return dict(FAKE_RECEIPT)

    live.write_txn = lambda conn: contextlib.nullcontext()
    live.collect_acceptance = fake_collect
    conn = FakeConn(snapshot, req.get("assignee"))
    res = live.prepare_acceptance(conn, req.get("task_id", "t1"),
                                  req.get("expected_run_id"), req.get("metadata"))
    out = {"outcome": None, "contract": None, "published_pr": None, "assignee": None}
    if res is False:
        out["outcome"] = "missing" if snapshot is None else "refuse"
        if snapshot is not None:
            out["contract"] = snapshot[2]
    elif res is None:
        out["outcome"] = "skip"
        out["contract"] = snapshot[2]
    else:
        contract, published_pr, assignee = seen["args"]
        out["outcome"] = "prepare"
        out["contract"] = contract
        out["published_pr"] = published_pr
        out["assignee"] = assignee
    return out


def pr_match(url):
    match = acceptance._PR.fullmatch(url) if isinstance(url, str) else None
    return match.group(1) if match else None


def contract_ok(value):
    try:
        return {"ok": True, "value": acceptance.validate_contract(value)}
    except ValueError as exc:
        return {"ok": False, "error": str(exc)}


def record_outcome(req):
    receipt = req.get("receipt")
    if not isinstance(receipt, dict):
        receipt = {}
    try:
        if not req.get("snapshot_matches"):
            return {"result": False, "detail": None}
        # receipt["ok"] is read first, exactly as the source does.
        ok = receipt["ok"]
        if not ok:
            detail = ("PR acceptance %s: %s %s"
                      % (receipt["classification"], receipt.get("detail", ""), receipt["recovery"]))
        else:
            detail = None
        return {"result": ok, "detail": detail}
    except KeyError:
        return {"error": "KeyError"}


def expected(r):
    f = r.get("func")
    if f == "decide":
        return decide(r)
    if f == "pr_match":
        return pr_match(r.get("url"))
    if f == "contract_ok":
        return contract_ok(r.get("value"))
    if f == "record_outcome":
        return record_outcome(r)
    if f == "constants":
        return {"pr_pattern": acceptance._PR.pattern, "pr_pattern_flags": acceptance._PR.flags,
                "repo_pattern": acceptance._REPO.pattern}
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
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
