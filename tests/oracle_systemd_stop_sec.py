#!/usr/bin/env python3
"""Oracle for gateway/restart.py:resolve_systemd_timeout_stop_sec.
AST-extracts LIVE _seconds and the resolver; passes all 5 values explicitly."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/gateway/restart.py"
ns = {"float": float, "max": max, "int": int, "object": object,
      "DEFAULT_GATEWAY_CRON_DRAIN_TIMEOUT": 30.0,
      "CRON_DRAIN_CLEANUP_RESERVE_S": 10.0,
      "SYSTEMD_STOP_HEADROOM_S": 30.0,
      "SYSTEMD_TIMEOUT_STOP_SEC_FLOOR": 60.0}
src = open(SRC, encoding="utf-8").read()
t = ast.parse(src)
for n in t.body:
    if isinstance(n, ast.FunctionDef) and n.name in ("_seconds", "resolve_systemd_timeout_stop_sec"):
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["resolve_systemd_timeout_stop_sec"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    py = int(live(r["drain"], r["cron"],
                  cleanup_reserve_s=r["cleanup"], headroom_s=r["headroom"],
                  floor_s=r["floor"]))
    c = int(r["out"])
    cases += 1
    if py != c:
        mism += 1
        print(f"MISMATCH {r} py={py} c={c}", file=sys.stderr)
print(f"ORACLE systemd_stop_sec: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
