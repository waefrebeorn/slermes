#!/usr/bin/env python3
"""Oracle for upstream Hermes gateway/session_skin.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/gateway/session_skin.py"
spec = importlib.util.spec_from_file_location("session_skin_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)
shape = live.resolved_skin()
cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    if func == "shape": py = shape
    elif func == "keys": py = sorted(shape.keys())
    elif func == "missing": py = None
    elif isinstance(func, str) and func in shape: py = shape[func]
    else: continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE session_skin: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
