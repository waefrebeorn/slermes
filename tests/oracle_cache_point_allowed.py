#!/usr/bin/env python3
"""Oracle for agent/bedrock_adapter.py:cache_point_allowed."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/bedrock_adapter.py"
ns = {"str": str, "dict": dict, "set": set, "Optional": object}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name == "cache_point_allowed":
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["cache_point_allowed"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    inp = r["in"]
    # Seed the module-level rejection map from the emitted input.
    ns["_CACHE_POINT_REJECTIONS"] = {
        k.lower(): set(v) for k, v in inp.get("rejections", {}).items()
    }
    py = int(bool(live(inp.get("model_id", ""), inp.get("placement", ""))))
    cases += 1
    if py != r["out"]:
        mism += 1
        print(f"MISMATCH {inp} py={py} c={r['out']}", file=sys.stderr)
print(f"ORACLE cache_point_allowed: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
