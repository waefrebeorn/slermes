#!/usr/bin/env python3
"""Differential oracle for gate w10_gatewayage_356e4e (hermes_cli/gateway_agent.py)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path: sys.path.insert(0, Q)
def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items(): setattr(m, k, v)
    sys.modules[name] = m
    return m
h = _mod("hermes_cli"); h.__path__ = [os.path.join(Q, "hermes_cli")]
NAME = "w10_gatewayage_356e4e_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "hermes_cli", "gateway_agent.py"))
live = importlib.util.module_from_spec(sp); sys.modules[NAME] = live; sp.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    try:
        if func == "effort_word":
            py = live._effort_word(r.get("config"))
        elif func == "review_summary_line":
            py = live.review_summary_line(r.get("payload"))
        elif func == "clarify_reply":
            reply = r.get("reply")
            cb = None if reply is None else (lambda q: reply)
            py = live.clarify_reply(cb, r.get("params") or {})
        else: continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr); continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w10_gatewayage_356e4e: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
