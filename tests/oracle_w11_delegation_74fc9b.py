#!/usr/bin/env python3
"""Differential oracle for agent/delegation_backends.py (gate w11_delegation_74fc9b)."""
import importlib.util, json, sys, types
try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w11_delegation_74fc9b"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/agent/delegation_backends.py"
_spec = importlib.util.spec_from_file_location("delegation_backends_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["delegation_backends_live"] = live
_spec.loader.exec_module(live)

def _obj_with(d, name):
    o = types.SimpleNamespace()
    if isinstance(d, dict) and name in d:
        setattr(o, name, d[name])
    return o

def live_out(rec):
    func = rec.get("func")
    if func == "route_keys":
        provider = _obj_with(rec.get("provider") or {}, "route_keys")
        return list(live._route_keys(provider))
    if func == "delegate_def":
        return live._delegate_def(rec.get("tools"))
    if func == "pinned_backend":
        agent = _obj_with(rec.get("agent") or {}, "_delegation_backend")
        return live.pinned_backend(agent)
    return None

def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        try: rec = json.loads(line)
        except ValueError: continue
        if rec.get("func") not in ("route_keys","delegate_def","pinned_backend"): continue
        cases += 1
        try: py = live_out(rec)
        except Exception as exc: py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} func={rec.get('func')} C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)"); return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0

if __name__ == "__main__":
    sys.exit(main())
