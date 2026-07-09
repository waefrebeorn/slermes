#!/usr/bin/env python3
"""Faithfulness oracle for port_gateway_response_filters.c.

Recomputes the LIVE gateway/response_filters.py functions for the same inputs
and asserts the C port would match (C returns 0/1; Python bool). The C port
uses the 4-marker set from LIVE_GATEWAY_SILENT_MARKERS.
"""
import sys, json
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
from gateway.response_filters import (
    _canonical_silence_candidate,
    is_intentional_silence_response,
    is_intentional_silence_agent_result,
    is_partial_silence_marker,
)

# (fn, args) -> expected C int
cases = []
# _canonical_silence_candidate (string -> string)
for inp, exp in [("", ""), ("  no reply  ", "NO REPLY"), ("NO_REPLY", "NO_REPLY"),
                 ("  a  b  c ", "A B C"), ("[SILENT]", "[SILENT]")]:
    cases.append(("canon", [inp], exp, "str"))
# is_intentional_silence_response (str -> bool)
for inp, exp in [("NO_REPLY", 1), ("NO REPLY", 1), ("[SILENT]", 1), ("SILENT", 1),
                 ("no reply", 1), ("noreply", 0), ("NO_RESPONSE", 0), ("[SILENCE]", 0),
                 ("hello", 0), ("", 0), ("NO_REPLY now", 0), ("x"*70, 0)]:
    cases.append(("resp", [inp], exp, "bool"))
# is_intentional_silence_agent_result (agent_result, response) -> bool
for ar, resp, exp in [({"failed": False}, "NO_REPLY", 1),
                      ({"failed": True}, "NO_REPLY", 0),
                      ({"failed": "true"}, "NO_REPLY", 0),
                      ({"ok": 1}, "SILENT", 1),
                      (None, "NO_REPLY", 0)]:
    cases.append(("agent", [json.dumps(ar), resp], exp, "bool"))
# is_partial_silence_marker (str -> bool)
for inp, exp in [("NO", 1), ("NO_", 1), ("NO_REPLY", 1), ("NO REPLY", 1),
                 ("SIL", 1), ("SILENT", 1), ("[SILENT]", 1), ("[SIL", 1),
                 ("hello", 0), ("NO_REPLY now", 0), ("", 0), ("   ", 0),
                 ("xyz NO_REPLY", 0), ("noreply", 0), ("no reply", 1), ("silent", 1)]:
    cases.append(("partial", [inp], exp, "bool"))

ok = True
for fn, args, exp, kind in cases:
    if fn == "canon":
        got = _canonical_silence_candidate(args[0])
        match = (got == exp)
        if not match:
            print("MISMATCH canon", repr(args[0]), "PY=", repr(got), "EXP=", repr(exp)); ok=False
    elif fn == "resp":
        got = 1 if is_intentional_silence_response(args[0]) else 0
        if got != exp: print("MISMATCH resp", repr(args[0]), "PY=", got, "EXP=", exp); ok=False
    elif fn == "agent":
        got = 1 if is_intentional_silence_agent_result(json.loads(args[0]), args[1]) else 0
        if got != exp: print("MISMATCH agent", args, "PY=", got, "EXP=", exp); ok=False
    elif fn == "partial":
        got = 1 if is_partial_silence_marker(args[0]) else 0
        if got != exp: print("MISMATCH partial", repr(args[0]), "PY=", got, "EXP=", exp); ok=False

print("ORACLE", "OK" if ok else "BAD", f"({len(cases)} cases, {0 if ok else 'see above'} mismatches)")
sys.exit(0 if ok else 1)
