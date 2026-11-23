#!/usr/bin/env python3
"""Oracle for upstream Hermes cron/jobs_tristate.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/cron/jobs_tristate.py"
spec = importlib.util.spec_from_file_location("crontri_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)
cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    fn = live.normalize_catch_up if func == "catch_up" else live.normalize_wrap_response if func == "wrap_response" else None
    if fn is None: continue
    try: py = {"ok": True, "value": fn(r.get("value"))}
    except ValueError as e: py = {"ok": False, "error": str(e)}
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE crontri: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
