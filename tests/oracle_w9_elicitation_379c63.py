#!/usr/bin/env python3
"""Differential oracle for acp_adapter/elicitation.py (gate w9_elicitation_379c63)."""
import importlib.util
import json
import sys

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/acp_adapter/elicitation.py"
spec = importlib.util.spec_from_file_location("w9_elicitation_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "client_supports_form":
        return live.client_supports_form_elicitation(r.get("params"))
    if func == "build_clarify_elicitation":
        return live.build_clarify_elicitation(r.get("session_id", ""), r.get("prompt", {}))
    if func == "elicited_answer":
        return live.elicited_answer(r.get("response"))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w9_elicitation_379c63: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if (mism or not cases) else 0)
