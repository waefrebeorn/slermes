#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/api_server_turn_boundary.py
(gate w21_apiservert_4998c3).

Executes the LIVE module plus the LIVE anchor it delegates to:
`reanchor_current_turn_user_idx` is lifted out of agent/turn_context.py and
installed as the `agent.turn_context` attribute the module imports, so both the
prefix helpers and the anchor walk are the real implementations.

The session-rules projection (`user_originated_turn_view`) is the seam: the
oracle recomputes it from the live session_rules module for every row, checks
the fixture carries that same array, and then lets the live code rank it. The C
port receives the fixture's copy.
"""
import __future__
import ast
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = Path("C:/Users/eman5/hermes-agent")
SRC = QUARRY / "gateway" / "platforms" / "api_server_turn_boundary.py"
TURN_CTX = QUARRY / "agent" / "turn_context.py"
GATE = "w21_apiservert_4998c3"
_ANN = __future__.annotations.compiler_flag

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from session_rules.turns import user_originated_turn_view  # live projection


def _lift(path, name):
    """Exec one top-level function from *path* in its own namespace."""
    tree = ast.parse(path.read_text(encoding="utf-8"))
    env = {"__name__": "lifted_" + name}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == name:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                         flags=_ANN, dont_inherit=True), env)
            return env[name]
    raise AssertionError("function %r not found in %s" % (name, path))


_reanchor = _lift(TURN_CTX, "reanchor_current_turn_user_idx")

_agent = types.ModuleType("agent")
_agent.__path__ = [str(QUARRY / "agent")]
sys.modules["agent"] = _agent
_tc = types.ModuleType("agent.turn_context")
_tc.reanchor_current_turn_user_idx = _reanchor
sys.modules["agent.turn_context"] = _tc

import importlib.util

_spec = importlib.util.spec_from_file_location("api_server_turn_boundary_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["api_server_turn_boundary_live"] = live
_spec.loader.exec_module(live)


def projection(messages):
    """The live per-row human-authored views, JSON-safe."""
    out = []
    for msg in messages if isinstance(messages, list) else []:
        try:
            view = user_originated_turn_view(msg)
        except Exception:  # noqa: BLE001 - a row the projector rejects has no view
            view = None
        out.append(view)
    return out


def expected(r):
    func = r.get("func")
    if func == "same_prefix":
        return live._same_transcript_prefix(r.get("agent_messages") or [],
                                            r.get("prefix") or [])
    if func == "reanchor":
        return _reanchor(r.get("agent_messages") or [], r.get("user_message"))
    if func == "start_index":
        result = r.get("result")
        if not isinstance(result, dict):
            result = {}
        return live.response_turn_start_index(r.get("conversation_history") or [],
                                             r.get("user_message"), result)
    raise AssertionError("unknown func %r" % (func,))


def main():
    cases = mism = 0
    wanted = {"same_prefix", "reanchor", "start_index"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            if r.get("func") == "start_index":
                result = r.get("result")
                msgs = result.get("messages") if isinstance(result, dict) else None
            else:
                msgs = r.get("agent_messages")
            if r.get("func") != "same_prefix" and isinstance(msgs, list):
                truth = projection(msgs)
                if r.get("live_views") != truth:
                    mism += 1
                    print("MISMATCH %s case=%r fixture live_views=%r live=%r"
                          % (r.get("func"), r.get("case"), r.get("live_views"), truth),
                          file=sys.stderr)
                    continue
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
