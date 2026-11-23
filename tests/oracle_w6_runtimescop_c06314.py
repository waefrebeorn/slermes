#!/usr/bin/env python3
"""Differential oracle for agent/runtime_scope.py (w6_runtimescop_c06314 gate)."""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/agent/runtime_scope.py"
spec = importlib.util.spec_from_file_location("live_runtime_scope", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["live_runtime_scope"] = live
spec.loader.exec_module(live)

def scope_from(d):
    s = types.SimpleNamespace()
    for k in ("tenant", "agent", "actor", "owner", "can_administer"):
        setattr(s, k, d.get(k))
    return s

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    try:
        if func == "claim_fence":
            f = live.claim_fence(r.get("row", {}))
            py = {"epoch": f.epoch, "generation": f.generation}
        elif func == "identity":
            py = list(live._identity(scope_from(r.get("scope", {}))))
        else:
            continue
    except Exception:
        py = None
        if func == "claim_fence":
            py = None
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_runtimescop_c06314: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
