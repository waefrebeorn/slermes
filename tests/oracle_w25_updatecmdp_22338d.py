#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_cmd_pause_gate.py (gate w25_updatecmdp_22338d).

The live context manager and settle function run for real; the pause record is
the seam -- install root, the tree read (found at the start, now at the settle),
the torn-path read and the record write are fixed answers.  What is compared is
the decision those answers make: which baseline a step binds to, what it merges
into it, and the verdict the moved HEAD gets.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_updatecmdp_22338d"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import update_cmd_pause_gate as live  # noqa: E402

WANTED = {"checkout_move", "settle"}


def _copy(value):
    return json.loads(json.dumps(value))


class Record:
    """The pause record's four seams, answering from the fixture."""

    def __init__(self, states, torn):
        self.pr = live._pause_record
        self.saved = (self.pr.install_root, self.pr.tree_state, self.pr.write,
                      self.pr.torn_by_move)
        self.states = states
        self.torn = torn
        self.reads = 0
        self.written = []

    def __enter__(self):
        self.pr.install_root = lambda: "ROOT"
        self.pr.tree_state = self._tree_state
        self.pr.write = lambda token, **kwargs: self.written.append(_copy(token))
        self.pr.torn_by_move = lambda root, found, changes: _copy(self.torn)
        return self

    def _tree_state(self, root):
        index, self.reads = self.reads, self.reads + 1
        value = self.states[index] if index < len(self.states) else None
        return _copy(value) if value is not None else None

    def __exit__(self, *exc):
        (self.pr.install_root, self.pr.tree_state, self.pr.write,
         self.pr.torn_by_move) = self.saved
        return False


def checkout_move(req):
    token = _copy(req.get("token"))
    with Record([req.get("found"), req.get("now")], req.get("torn")) as record:
        step = {}
        with live._checkout_move(token, *(req.get("targets") or []),
                                 paths=tuple(req.get("paths") or []),
                                 revert=bool(req.get("revert"))) as box:
            if req.get("step_whole"):
                box["whole"] = True
            step = box
    return {"step": step, "token": token, "writes": len(record.written)}


def settle(req):
    token = _copy(req.get("token"))
    baselines = token.get("baselines") or []
    index = req.get("baseline_index", 0)
    baseline = baselines[index]
    step = {"whole": True} if req.get("step_whole") else {}
    with Record([req.get("now")], req.get("torn")) as record:
        live._settle_checkout_move(token, baseline, _copy(req.get("found")), "ROOT", step)
    return {"token": token, "baseline": baseline, "writes": len(record.written)}


def expected(r):
    f = r.get("func")
    if f == "checkout_move":
        return checkout_move(r)
    if f == "settle":
        return settle(r)
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
