#!/usr/bin/env python3
"""Oracle for upstream Hermes pm/__init__.py (gate w5_init_703dd3).

pm/__init__.py is a lazy PEP 562 facade: it imports only sys at load time,
so no pm parent-package bootstrap is required. Load it under a flat name and
read _HOME/__all__ directly.
"""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path: sys.path.insert(0, Q)

spec = importlib.util.spec_from_file_location("live_pminit_w5", Q + "/pm/__init__.py")
live = importlib.util.module_from_spec(spec)
sys.modules["live_pminit_w5"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    if func == "all":
        py = list(live.__all__)
    elif func == "home":
        py = live._HOME.get(r.get("name", ""))
    elif func == "has":
        py = r.get("name", "") in live._HOME
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} name={r.get('name')!r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w5_init_703dd3: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
