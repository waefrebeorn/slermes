#!/usr/bin/env python3
"""Oracle for hermes_cli/cli_panel.py:_Panel (gate clipanel)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
SRC = QUARRY + "/hermes_cli/cli_panel.py"
spec = importlib.util.spec_from_file_location("clipanel_live", SRC)
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
    if r.get("func") != "panel":
        continue
    p = live._Panel(r.get("border", "B"), r.get("width", 0),
                    r.get("title", ""), r.get("title_style", ""))
    for o in r.get("ops", []):
        op = o.get("op")
        if op == "row":
            p.row(o.get("style", ""), o.get("text", ""))
        elif op == "blank":
            p.blank()
        elif op == "close":
            p.close()
    py = [[style, text] for style, text in p.lines]
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH {r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE clipanel: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
