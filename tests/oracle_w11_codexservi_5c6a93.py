#!/usr/bin/env python3
"""Differential oracle for agent/codex_service_tiers.py (gate w11_codexservi_5c6a93)."""
import importlib.util, json, sys
try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w11_codexservi_5c6a93"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/agent/codex_service_tiers.py"
_spec = importlib.util.spec_from_file_location("codex_service_tiers_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["codex_service_tiers_live"] = live
_spec.loader.exec_module(live)

def as_out(fset):
    return None if fset is None else sorted(fset)

def live_out(rec):
    func = rec.get("func")
    entry = rec.get("entry")
    if func == "advertised_tiers":
        return as_out(live._advertised_tiers(entry))
    if func == "advertised_cyber_programs":
        return as_out(live._advertised_cyber_programs(entry))
    if func == "daybreak_program":
        return live.DAYBREAK_PROGRAM
    return None

def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        try: rec = json.loads(line)
        except ValueError: continue
        if rec.get("func") not in ("advertised_tiers","advertised_cyber_programs","daybreak_program"): continue
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
