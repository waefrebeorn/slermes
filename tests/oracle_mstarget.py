#!/usr/bin/env python3
"""Oracle for upstream Hermes plugins/platforms/matrix/send_targets.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/plugins/platforms/matrix/send_targets.py"
spec = importlib.util.spec_from_file_location("mstarget_live", SRC)
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
    if func == "parse":
        res = live._parse_send_target(r["ref"])
        py = list(res) if res is not None else None
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE mstarget: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
