#!/usr/bin/env python3
"""Differential oracle for tools/connectors/turn.py (gate w24_turn_6ea05a).

The live module is imported as a package module and its pure surface helpers are
called on value-built agents (``types.SimpleNamespace``): no agent runtime, no
ContextVar leak.  ``scoped_connection_surface`` is a context manager, so the
walk below drives it for real -- set, read, reset -- and the C side must agree
on what the variable holds inside and after each block.
"""
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_turn_6ea05a"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tools.connectors import turn as live  # noqa: E402

WANTED = {"agent_surface", "tool_drops", "surface_scope", "constants"}
DEFAULT_SURFACE = live.connection_surface()  # read before any scope is entered


def agent_of(raw):
    """Only the attributes the request names exist: absent != present-None."""
    ns = types.SimpleNamespace()
    spec = raw if isinstance(raw, dict) else {}
    for key in ("connection_callback", "side_agent"):
        if key in spec:
            setattr(ns, key, spec[key])
    return ns


def walk(ops):
    out = []
    if not isinstance(ops, list):
        return out
    for op in ops:
        if not isinstance(op, dict):
            out.append("UNKNOWN_OP")
            continue
        kind = op.get("op")
        if kind == "get":
            out.append(live.connection_surface())
        elif kind == "scope":
            with live.scoped_connection_surface(op.get("surface")):
                inner = walk(op.get("ops"))
            out.append({"inside": inner, "after": live.connection_surface()})
        else:
            out.append("UNKNOWN_OP")
    return out


def expected(req):
    func = req.get("func")
    if func == "agent_surface":
        return live.agent_connection_surface(agent_of(req.get("agent")))
    if func == "tool_drops":
        return sorted(live.side_agent_tool_drops(agent_of(req.get("agent"))))
    if func == "surface_scope":
        return walk(req.get("ops"))
    if func == "constants":
        return {
            "card": live.CARD,
            "side": live.SIDE,
            "link": live.LINK,
            "default_surface": DEFAULT_SURFACE,
            "side_agent_tool_drops": sorted(live.SIDE_AGENT_TOOL_DROPS),
        }
    raise AssertionError("unknown func %r" % (func,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except ValueError:
            continue
        if not isinstance(req, dict) or req.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(req)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (req.get("func"), req.get("case"), exc), file=sys.stderr)
            continue
        got = req.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (req.get("func"), req.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
