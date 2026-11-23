#!/usr/bin/env python3
"""Differential oracle for live agent/history_commentary.py (w10_historycom_1bfe19).

The three module-level collaborators (redaction, hermes_constants, utils) are
stubbed so the live module loads; the ported functions are pure over their
arguments and never call them.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


agent_pkg = _mod("agent")
agent_pkg.__path__ = []
_mod("agent.redact", redact_sensitive_text=lambda s: s)
_mod("hermes_constants",
     reset_hermes_home_override=lambda token: None,
     set_hermes_home_override=lambda home: None)
_mod("utils", is_truthy_value=lambda v, default=False: default if v is None else bool(v))

SRC = Q + "/agent/history_commentary.py"
spec = importlib.util.spec_from_file_location("hc_1bfe19_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hc_1bfe19_live"] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "commentary_items":
        return live.commentary_items(r.get("message", {}))
    if f == "final_items":
        return live._final_items(r.get("message", {}))
    if f == "without_flattened":
        return live._without_flattened_commentary(r.get("reasoning", ""), r.get("commentary", []))
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
    try:
        py = compute(r)
    except Exception as exc:
        print(f"LIVE ERROR {r.get('func')}: {exc!r}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w10_historycom_1bfe19: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
