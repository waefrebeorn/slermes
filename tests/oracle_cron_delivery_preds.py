#!/usr/bin/env python3
"""Oracle for two re-ported cron delivery predicates.

Ast-extracts LIVE _inchannel_seed_allowed and _cron_delivery_notify_enabled
from cron/scheduler_delivery.py (no import) and replays on the echoed input
the harness emits. C emits {"fn","in","out"}.
"""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/cron/scheduler_delivery.py"
FUNS = ("_inchannel_seed_allowed", "_cron_delivery_notify_enabled")
ns = {"Optional": object, "dict": dict, "bool": bool, "str": str}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name in FUNS:
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
seed = ns["_inchannel_seed_allowed"]
notify = ns["_cron_delivery_notify_enabled"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    inp = r.get("in", {})
    if r.get("fn") == "seed":
        py = int(bool(seed(is_dm=bool(inp.get("is_dm")), user_id=inp.get("user_id"))))
    elif r.get("fn") == "notify":
        cfg = inp.get("cfg")  # inline JSON object or None
        py = int(bool(notify(cfg)))
    else:
        continue
    c = r.get("out")
    cases += 1
    if py != c:
        mism += 1
        print(f"MISMATCH {r} py={py} c={c}", file=sys.stderr)
print(f"ORACLE cron_delivery_preds: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
