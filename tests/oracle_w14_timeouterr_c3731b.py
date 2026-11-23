#!/usr/bin/env python3
"""Differential oracle for gate w14_timeouterr_c3731b (agent/timeout_error_summary.py)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_timeouterr_c3731b"
sys.path.insert(0, QUARRY)

SRC = QUARRY + "/agent/timeout_error_summary.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def build_chain(nodes):
    objs = [type(str(n.get("type", "Exception")), (Exception,), {})("x") for n in nodes]
    for i, n in enumerate(nodes):
        c = n.get("cause")
        if type(c) is int and 0 <= c < len(objs):
            objs[i].__cause__ = objs[c]
    return objs[0] if objs else None


def compute(r):
    chain = build_chain(r.get("nodes") or [])
    if r.get("func") == "phase":
        return live._timeout_phase(chain)
    if r.get("func") == "summarize":
        return live.summarize_timeout_error(chain, r.get("provider"))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
