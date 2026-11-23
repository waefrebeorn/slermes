#!/usr/bin/env python3
"""Oracle for upstream Hermes gateway/webhook_targets.py."""
import importlib.util, json, sys
sys.path.insert(0, "C:/Users/eman5/hermes-agent")  # live lazy platform_registry import
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/gateway/webhook_targets.py"
spec = importlib.util.spec_from_file_location("whtarget_live", SRC)
live = importlib.util.module_from_spec(spec)
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
    if func == "known":
        py = bool(live.is_known_platform(r["name"]))
    elif func == "dispatch":
        py = bool(live.is_dispatchable_target(r.get("target")))
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE whtarget: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
