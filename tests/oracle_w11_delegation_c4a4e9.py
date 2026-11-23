#!/usr/bin/env python3
"""Differential oracle for agent/delegation_registry.py (gate w11_delegation_c4a4e9)."""
import importlib.util, json, sys
try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w11_delegation_c4a4e9"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/agent/delegation_registry.py"
_spec = importlib.util.spec_from_file_location("delegation_registry_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["delegation_registry_live"] = live
_spec.loader.exec_module(live)

def live_out(rec):
    func = rec.get("func")
    if func == "configured_backend":
        return live.configured_backend(rec.get("config"))
    if func == "lower_key":
        return live.lower_key(rec.get("name"))
    if func == "bundled_name_matches":
        return bool(live._BUNDLED_NAME_RE.match(rec.get("name")))
    return None

def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        try: rec = json.loads(line)
        except ValueError: continue
        if rec.get("func") not in ("configured_backend","lower_key","bundled_name_matches"): continue
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
