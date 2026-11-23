#!/usr/bin/env python3
"""Oracle for upstream Hermes hermes_cli/plugin_compat.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/hermes_cli/plugin_compat.py"
spec = importlib.util.spec_from_file_location("plugcompat_live", SRC)
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
    if func == "compat_report":
        py = live.compat_report(r.get("manifests"), force=r.get("force", False))
    elif func == "removal_in_effect":
        py = live.removal_in_effect(r.get("today"))
    elif func == "summary_lines":
        py = live.summary_lines(r.get("report"), today=r.get("today"))
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE plugcompat: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
