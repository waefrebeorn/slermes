#!/usr/bin/env python3
"""Differential oracle for agent/context_pin.py (gate w6_contextpin_633375)."""
import importlib.util, json, sys
sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/context_pin.py"
spec = importlib.util.spec_from_file_location("w6_contextpin_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w6_contextpin_live"] = live
spec.loader.exec_module(live)

def val(kind, raw):
    kind = kind or "int"
    if kind == "int":
        return int(raw)
    if kind == "bool":
        return raw == "true"
    if kind == "float":
        return float(raw)
    if kind == "none":
        return None
    return raw

def compute(r):
    c = val(r.get("context_kind", "int"), r.get("context", "0"))
    p = val(r.get("config_kind", "int"), r.get("config", "0"))
    f = r.get("func")
    if f == "is_context_pinned":
        return live.is_context_pinned(c, p)
    if f == "context_pin_suffix":
        return live.context_pin_suffix(c, p)
    if f == "context_window_marker":
        return live.context_window_marker(c, p, model=r.get("model", ""))
    if f == "context_window_suffix":
        return live.context_window_suffix(c, p, model=r.get("model", ""))
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
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_contextpin_633375: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
