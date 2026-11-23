#!/usr/bin/env python3
"""Oracle for upstream Hermes hermes_cli/config_secret_display.py (secdisplay gate)."""
import importlib.util, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)  # quarry hermes_cli, not the slermes shadow copy
SRC = PYROOT + "/hermes_cli/config_secret_display.py"
spec = importlib.util.spec_from_file_location("secdisplay_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)

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
    if func == "maskable":
        py = live._maskable_secret_text(r.get("value"))
    elif func == "redact":
        py = live.redact_config_value(r.get("value"))
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE secdisplay: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
