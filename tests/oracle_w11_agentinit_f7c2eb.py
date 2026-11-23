#!/usr/bin/env python3
"""Differential oracle for agent/agent_init_fallback.py (gate w11_agentinit_f7c2eb)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/agent/agent_init_fallback.py"
spec = importlib.util.spec_from_file_location("w11_agentinit_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_agentinit_live"] = live
spec.loader.exec_module(live)


def labels(chain):
    return [f"{f['model']} ({f['provider']})" for f in chain]


def message(chain):
    if not chain:
        return ""
    ls = labels(chain)
    if len(chain) == 1:
        return f"\U0001f504 Fallback model: {ls[0]}"
    return f"\U0001f504 Fallback chain ({len(chain)} providers): " + " \u2192 ".join(ls)


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    got = r.get("out")
    try:
        if func == "entries":
            want = live._fallback_entries(r.get("fallback_model"))
        elif func == "labels":
            want = labels(r.get("chain", []) or [])
        elif func == "message":
            want = message(r.get("chain", []) or [])
        else:
            continue
    except BaseException as ex:
        print("MISMATCH %s oracle raised %s: %s" % (func, type(ex).__name__, ex), file=sys.stderr)
        mism += 1
        cases += 1
        continue
    cases += 1
    if want != got:
        mism += 1
        print("MISMATCH in=%s py=%r c=%r" % (json.dumps(r, ensure_ascii=False), want, got), file=sys.stderr)
print("ORACLE w11_agentinit_f7c2eb: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or cases == 0 else 0)
