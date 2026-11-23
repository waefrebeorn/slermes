#!/usr/bin/env python3
"""Differential oracle for agent/prompt_cache_routes.py (gate w11_promptcach_850c92).

All exercised helpers are pure; the module's top-level imports are stdlib.
Entries, gaps, sessions and the keep mask are injected per case from the
fixture, so no filesystem or clock seam is needed.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

SRC = QUARRY + "/agent/prompt_cache_routes.py"
spec = importlib.util.spec_from_file_location("w11_promptcach_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_promptcach_live"] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "finite":
        return live._finite(r.get("value"))
    if f == "numbers":
        return live._numbers(r.get("value"))
    if f == "well_formed":
        return live._well_formed(r.get("entry"))
    if f == "push":
        entry = json.loads(json.dumps(r.get("entry") or {}))
        live._push(entry, r.get("field", "hit_gaps"), r.get("gap", 0),
                   r.get("session_id", ""))
        return entry
    if f == "sessions":
        return live._sessions(r.get("entry") or {}, r.get("field", ""),
                              r.get("keep"))
    if f == "routine_hit":
        return live._routine_hit(r.get("entry") or {}, r.get("gap_s"),
                                 r.get("read", 0), r.get("write", 0),
                                 r.get("now", 0))
    if f == "learned_lifetime":
        return live.learned_lifetime(r.get("entry") or {}, r.get("tier_s", 0))
    if f == "display_rule":
        return live.display_rule(r.get("rule"))
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
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')}: python raised {exc!r}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w11_promptcach_850c92: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w11_promptcach_850c92: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
