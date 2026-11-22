#!/usr/bin/env python3
"""Oracle for hermes_cli/gateway.py:_get_cron_drain_timeout composed with
gateway/restart.py:parse_cron_drain_timeout / _parse_timeout_keeping_zero."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

RESTART = "C:/Users/eman5/hermes-agent/gateway/restart.py"
ns = {"float": float, "math": __import__("math"), "object": object,
      "DEFAULT_GATEWAY_CRON_DRAIN_TIMEOUT": 30.0}
src = open(RESTART, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name in ("_parse_timeout_keeping_zero", "parse_cron_drain_timeout"):
        exec(compile(ast.Module(body=[n], type_ignores=[]), RESTART, "exec"), ns)
# default resolved at import: 30.0 from DEFAULT_CONFIG["agent"]["cron_drain_timeout"]
parse = ns["parse_cron_drain_timeout"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    raw = r.get("raw")
    try:
        py = float(parse(raw))
    except Exception as e:
        print(f"SKIP {raw!r}: {e}", file=sys.stderr)
        continue
    try:
        c = float(r["out"])
    except (TypeError, ValueError):
        c = None
    cases += 1
    if py != c:
        mism += 1
        print(f"MISMATCH raw={raw!r} py={py} c={c}", file=sys.stderr)
print(f"ORACLE cron_drain: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
