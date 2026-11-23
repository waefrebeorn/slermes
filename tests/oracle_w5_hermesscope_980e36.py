#!/usr/bin/env python3
"""Oracle for upstream Hermes hermes_scope.py (w5_hermesscope_980e36 gate)."""
import importlib.util, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/hermes_scope.py"
spec = importlib.util.spec_from_file_location("hermesscope_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hermesscope_live"] = live
spec.loader.exec_module(live)

def valid(fn, *args):
    try:
        fn(*args)
        return 1
    except Exception:
        return 0

def make_scope(tenant, agent):
    s = object.__new__(live.Scope)
    object.__setattr__(s, "tenant", tenant)
    object.__setattr__(s, "agent", agent)
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
    if func == "principal":
        py = valid(live.Principal, r.get("v"))
    elif func == "system":
        py = valid(live.System, r.get("v"))
    elif func == "group":
        py = valid(live.Group, r.get("v"))
    elif func == "fence":
        py = valid(live.Fence, r.get("epoch"), r.get("generation"))
    elif func == "is_local":
        py = int(make_scope(r.get("tenant", ""), "x").is_local)
    elif func == "home":
        py = make_scope(r.get("tenant", ""), r.get("agent", "")).home()
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w5_hermesscope_980e36: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
