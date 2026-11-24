#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/matrix/voice_mention.py (gate w19_voicementi_2db922).

Extracts the live MSC3245 checks and the ParkedVoices state machine from the AST. The
only seam is the monotonic clock: each replay op carries the `now` the call would see,
which the stubbed `time.monotonic` returns, so prune windows, park ordering, the
per-sender cap and the in-flight floor replay identically on both sides.
"""
import ast
import asyncio
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
SRC = Q + "/plugins/platforms/matrix/voice_mention.py"
GATE = "w19_voicementi_2db922"
_ANN = __import__("__future__").annotations.compiler_flag

NOW = {"now": 0.0}


def _exec_node(node, g):
    mod = ast.Module(body=[node], type_ignores=[])
    exec(compile(mod, SRC, "exec", flags=_ANN, dont_inherit=True), g)


_live = types.ModuleType("voice_mention_live")
_live.__package__ = "plugins.platforms.matrix"
sys.modules["voice_mention_live"] = _live
g = _live.__dict__
g.update({"asyncio": asyncio,
          "time": SimpleNamespace(monotonic=lambda: NOW["now"]),
          "Dict": object, "List": object, "Optional": object, "Tuple": object})

CONSTS = {"CLAIM_WINDOW_SECONDS", "SETTLE_TIMEOUT_SECONDS", "MAX_PARKED_PER_SENDER"}
tree = ast.parse(open(SRC, encoding="utf-8").read())
for node in tree.body:
    if isinstance(node, ast.Assign) and any(getattr(t, "id", "") in CONSTS for t in node.targets):
        _exec_node(node, g)
    elif isinstance(node, ast.AnnAssign) and getattr(node.target, "id", "") in CONSTS:
        _exec_node(node, g)
    elif isinstance(node, ast.FunctionDef) and node.name in ("has_voice_marker", "is_voice_event"):
        _exec_node(node, g)
    elif isinstance(node, ast.ClassDef) and node.name in ("VoiceGate", "ParkedVoices"):
        _exec_node(node, g)

has_voice_marker = g["has_voice_marker"]
is_voice_event = g["is_voice_event"]
ParkedVoices = g["ParkedVoices"]


def replay(ops):
    pv = ParkedVoices()
    gates = {}
    trace = []
    for op in ops:
        NOW["now"] = float(op.get("now", 0.0))
        name = op.get("op")
        room = op.get("room", "")
        sender = op.get("sender", "")
        if name == "mark":
            trace.append(pv.mark())
        elif name == "begin":
            gate = pv.begin(room, sender)
            gates[gate.seq] = gate
            trace.append(gate.seq)
        elif name == "release":
            pv.release(room, sender, gates.get(op.get("gate")))
            trace.append(None)
        elif name == "park":
            pv.park(room, sender, gates[op.get("gate")], op.get("event_id"),
                    op.get("content") or {}, op.get("relates_to") or {})
            trace.append(None)
        elif name == "claim":
            got = pv.claim(room, sender, op.get("before", 0))
            trace.append(None if got is None else list(got))
        elif name == "pending":
            trace.append(bool(pv.pending(room, sender)))
        elif name == "prune":
            pv._prune()
            trace.append(None)
        else:
            trace.append("UNKNOWN_OP")
    return trace


def run_case(r):
    f = r.get("func")
    if f == "has_voice_marker":
        return bool(has_voice_marker(r.get("content") or {}))
    if f == "is_voice_event":
        return bool(is_voice_event(r.get("content") or {}))
    if f == "parked_replay":
        return replay(r.get("ops") or [])
    return "SKIP"


FUNCS = {"has_voice_marker", "is_voice_event", "parked_replay"}


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
        if f not in FUNCS:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as exc:  # noqa: BLE001 - report, never abort the oracle
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
