#!/usr/bin/env python3
"""Differential oracle for gateway/session_policy_refresh.py (gate w18_sessionpol_e8bda4)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/gateway/session_policy_refresh.py"
spec = importlib.util.spec_from_file_location("w18_sessionpol_e8bda4_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w18_sessionpol_e8bda4_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "follow":
        t = dict(r.get("target", {}))
        live._follow(t, r.get("live", {}), r.get("key", ""))
        return t
    if func == "server_name":
        return live._server_name(set(r.get("servers", [])), r.get("name", ""))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_sessionpol_e8bda4: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
