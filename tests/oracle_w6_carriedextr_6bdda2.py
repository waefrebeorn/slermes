#!/usr/bin/env python3
"""Oracle for pm/carried_extras.py (w6_carriedextr_6bdda2)."""
import importlib.util, json, sys, os, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = r"C:\Users\eman5\hermes-agent"
sys.path.insert(0, QUARRY)

def load(name, rel):
    parent = name.rsplit(".", 1)[0]
    if parent not in sys.modules:
        __import__(parent)
    spec = importlib.util.spec_from_file_location(name, os.path.join(QUARRY, *rel))
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m

live = load("pm.carried_extras", ["pm", "carried_extras.py"])
import pm.extras as pex

class _Pkg:
    def project_root(self): return "/p"

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    try:
        if func == "baseline":
            pex.pre_pm_baseline = lambda root: list(r["baseline"])
            pex.committed_generation_extras = lambda root: []
            ex, ca, au = live.with_pre_pm_baseline(
                _Pkg(), r["extras"], carried=r["carried"], automatic=r["automatic"],
                plugins=None if r.get("plugins_none") else object())
            py = {"extras": list(ex), "carried": list(ca), "automatic": list(au)}
        else:
            continue
    except Exception as e:
        mism += 1; cases += 1
        print(f"ORACLE w6_carriedextr_6bdda2 ERROR case={r.get('case')} {e!r}", file=sys.stderr)
        continue
    got = r.get("out"); cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH w6_carriedextr_6bdda2 case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_carriedextr_6bdda2: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
