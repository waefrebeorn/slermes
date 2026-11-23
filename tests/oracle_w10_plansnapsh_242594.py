#!/usr/bin/env python3
"""Differential oracle for live agent/plan_snapshot_sync.py (w10_plansnapsh_242594).

`agent.credits_tracker._window_seed_fields` is stubbed as
`getattr(window, "seed", None)`; a window with a truthy `seed` field is a
window-plan snapshot. JSON objects become SimpleNamespaces for getattr access.
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

agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = []
sys.modules["agent"] = agent_pkg
credits = types.ModuleType("agent.credits_tracker")
credits._window_seed_fields = lambda w: getattr(w, "seed", None) if w is not None else None
sys.modules["agent.credits_tracker"] = credits

SRC = Q + "/agent/plan_snapshot_sync.py"
spec = importlib.util.spec_from_file_location("ps_242594_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["ps_242594_live"] = live
spec.loader.exec_module(live)


def mk(x):
    if isinstance(x, dict):
        return types.SimpleNamespace(**{k: mk(v) for k, v in x.items()})
    if isinstance(x, list):
        return [mk(v) for v in x]
    return x


def compute(r):
    f = r.get("func")
    if f == "snapshot_full_ids":
        ids = live._snapshot_full_ids(mk(r.get("snapshot")))
        return None if ids is None else sorted(ids)
    if f == "plan_snapshot_is_stale":
        return live.plan_snapshot_is_stale(mk(r.get("state")), mk(r.get("snapshot")))
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

print(f"ORACLE w10_plansnapsh_242594: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
