#!/usr/bin/env python3
"""Differential oracle for gateway/run_turn.py (gate w6_runturn_6f520a).

The three ported helpers are pure: is_context_overflow_failure_result,
bound_model_input_without_hygiene and hygiene_no_commit_reason (the latter
reads attributes off an injected SimpleNamespace).
"""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

# Parent-package bootstrap for packaging consistency (module uses absolute
# imports, but registering a parent keeps spec loading faithful and isolated).
PKG = "w6_runturn_live_pkg"
pkg = types.ModuleType(PKG)
pkg.__path__ = [Q + "/gateway"]
sys.modules[PKG] = pkg
spec = importlib.util.spec_from_file_location(PKG + ".run_turn", Q + "/gateway/run_turn.py")
live = importlib.util.module_from_spec(spec)
sys.modules[PKG + ".run_turn"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "overflow":
        return live.is_context_overflow_failure_result(
            r.get("result", {}), int(r.get("history_len", 0)))
    if func == "bound":
        return live.bound_model_input_without_hygiene(
            r.get("history", []), int(r.get("limit", 0)))
    if func == "reason":
        obj = types.SimpleNamespace(**(r.get("attrs") or {}))
        return live.hygiene_no_commit_reason(obj)
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
print(f"ORACLE w6_runturn_6f520a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
