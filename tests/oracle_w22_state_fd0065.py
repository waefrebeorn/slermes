#!/usr/bin/env python3
"""Differential oracle for agent/pet/state.py (gate w22_state_fd0065).

The module is pure: `derive_pet_state` ranks seven coarse activity signals and
`todos_all_done` folds a todo list into the JUMP celebrate beat. Both are called
live -- no seam is needed -- and the state travels as the live ``PetState.name``.

  state  -- live `derive_pet_state(**flags)` -> member name ("IDLE", "RUN", ...)
  todos  -- live `todos_all_done(todos)`; a TypeError from `list(todos)` on a
            truthy non-iterable is reported as -1, which is what the C side
            returns for the same input.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "agent" / "pet" / "state.py"
GATE = "w22_state_fd0065"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("pet_state_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["pet_state_live"] = live
_spec.loader.exec_module(live)

WANTED = {"state", "todos"}

DERIVE_KWARGS = ("busy", "awaiting_input", "error", "celebrate", "just_completed",
                 "tool_running", "reasoning")


def expected(r):
    f = r.get("func")
    if f == "state":
        flags = r.get("flags")
        flags = flags if isinstance(flags, dict) else {}
        return live.derive_pet_state(**{k: v for k, v in flags.items() if k in DERIVE_KWARGS}).name
    if f == "todos":
        try:
            return 1 if live.todos_all_done(r.get("todos")) else 0
        except TypeError:
            return -1
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
