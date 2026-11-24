#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/base_post_delivery.py
(gate w22_basepostde_1b117c).

Both FunctionDefs are exec'd out of the live AST; the async one is then *run*
against real callback objects, with the timeout real:

  generation -- live turn_run_generation over a SimpleNamespace carrying the
                fixture's `event` attributes (getattr semantics, not dict lookup).
  callback   -- live run_post_delivery_callback under asyncio.run with an
                instrumented callback: a real function that may raise, may
                return a custom awaitable whose __await__ flags entry, and may
                sleep past a 10 ms timeout when `timed_out` is set. `invoked` and
                `awaited` are observed side effects, not assumptions.
"""
import ast
import asyncio
import json
import os
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "gateway" / "platforms" / "base_post_delivery.py"
GATE = "w22_basepostde_1b117c"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

G = {"asyncio": asyncio, "contextlib": __import__("contextlib"), "inspect": __import__("inspect"),
     "Any": object}
_tree = ast.parse(SRC.read_text(encoding="utf-8"))
_want = {"turn_run_generation", "run_post_delivery_callback"}
for _node in ast.walk(_tree):
    if isinstance(_node, (ast.FunctionDef, ast.AsyncFunctionDef)) and _node.name in _want:
        _node.decorator_list = []
        exec(compile(ast.Module(body=[_node], type_ignores=[]), str(SRC), "exec"), G)
missing = _want - set(G)
if missing:  # pragma: no cover - both functions must exist
    raise SystemExit("missing %s in %s" % (sorted(missing), SRC))
turn_run_generation = G["turn_run_generation"]
run_post_delivery_callback = G["run_post_delivery_callback"]

WANTED = {"generation", "callback"}


class Awaitable:
    """A real awaitable: flags entry, then optionally sleeps past the timeout."""

    def __init__(self, slow, state):
        self.slow = slow
        self.state = state

    def __await__(self):
        self.state["awaited"] = True
        if self.slow:
            yield from asyncio.sleep(10).__await__()
        return None


def generation(attrs):
    event = types.SimpleNamespace(**(attrs or {}))
    return turn_run_generation(event)


def callback(desc):
    desc = desc or {}
    state = {"invoked": False, "awaited": False}
    if desc.get("callable"):
        def cb():
            state["invoked"] = True
            if desc.get("raises"):
                raise RuntimeError("post-delivery callback exploded")
            if desc.get("awaitable"):
                return Awaitable(bool(desc.get("timed_out")), state)
            return None
        callback_arg = cb
    else:
        callback_arg = object()
    timeout = 0.01 if desc.get("timed_out") else 1.0
    raised = False
    ret = "NEVER-RUN"
    try:
        ret = asyncio.run(run_post_delivery_callback(callback_arg, timeout))
    except BaseException:  # noqa: BLE001 - the module promises this never fires
        raised = True
    return {"invoked": state["invoked"], "awaited": state["awaited"],
            "raised": raised, "returned_none": ret is None}


def expected(r):
    f = r.get("func")
    if f == "generation":
        return generation(r.get("event"))
    if f == "callback":
        return callback(r.get("desc"))
    raise AssertionError("unknown func %r" % (f,))


def check_isolation():
    """The runner isolates HOME/SLERMES_HOME to one temp dir: assert it, don't assume."""
    home = os.environ.get("HOME")
    slermes = os.environ.get("SLERMES_HOME")
    same = bool(home and slermes and
                os.path.normcase(os.path.abspath(home)) ==
                os.path.normcase(os.path.abspath(slermes)))
    if not same or not os.path.isdir(home):
        print("HARNESS-ENV not isolated: HOME=%r SLERMES_HOME=%r" % (home, slermes),
              file=sys.stderr)
        sys.exit(2)


def main():
    check_isolation()
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
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
