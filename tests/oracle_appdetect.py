#!/usr/bin/env python3
"""Oracle for upstream Hermes tools/approval_detection.py (appdetect gate)."""
import importlib.util, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/tools/approval_detection.py"
spec = importlib.util.spec_from_file_location("appdetect_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["appdetect_live"] = live
spec.loader.exec_module(live)

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
    if func == "lower":
        py = live._lower_preserving_flags(r.get("command", ""))
    elif func == "splice":
        py = live._splice(r.get("command", ""), [tuple(e) for e in r.get("edits", [])])
    elif func == "eval":
        py = live._eval_payload(r.get("args", []))
    elif func == "family":
        py = live._interpreter_family(r.get("executable", ""))
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE appdetect: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
