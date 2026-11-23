#!/usr/bin/env python3
"""Oracle for upstream Hermes gateway/session_tool_events.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/gateway/session_tool_events.py"
spec = importlib.util.spec_from_file_location("stool_live", SRC)
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
    if func == "bounded_args":
        py = live.bounded_args(r["args"])
    elif func == "bounded_result":
        py = live.bounded_result(r["result"], r.get("is_error", False))
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        shown = (str(py)[:200] + "...") if len(str(py)) > 200 else repr(py)
        print(f"MISMATCH {func} py={shown} c={str(got)[:200]!r}", file=sys.stderr)
print(f"ORACLE stool: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
