#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/session_cron_commands.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/gateway/session_cron_commands.py"
spec = importlib.util.spec_from_file_location("session_cron_commands_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["session_cron_commands_live"] = live
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
    if func != "cron_read":
        continue
    cases += 1
    py = live.cron_read(r.get("tokens", []))
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} py={py!r} c={got!r} in={r.get('tokens')!r}", file=sys.stderr)
print(f"ORACLE sesscron: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
