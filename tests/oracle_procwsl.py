#!/usr/bin/env python3
"""Differential oracle for tools/process_registry_wsl.py (gate procwsl)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/tools/process_registry_wsl.py"
spec = importlib.util.spec_from_file_location("procwsl_live", SRC)
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
    if r.get("func") != "is_wsl":
        continue
    cmd = r.get("command")  # None when JSON null (non-str -> False)
    want = bool(live._is_wsl_launcher_command(cmd))
    got = r.get("out")
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE procwsl: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
