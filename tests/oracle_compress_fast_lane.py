#!/usr/bin/env python3
"""Oracle for agent/auxiliary_client.py:resolve_compression_fast_lane.
AST-extracts the LIVE function and replays it with caller-resolved config
fields and provider normalization injected as stubs."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/auxiliary_client.py"
ns = {"str": str, "Optional": object, "Any": object, "dict": dict, "bool": bool}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name == "resolve_compression_fast_lane":
        n.decorator_list = []
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["resolve_compression_fast_lane"]

class CompressionFastLane:
    def __init__(self, certified_non_reasoning, reasoning_config):
        self.certified_non_reasoning = certified_non_reasoning
        self.reasoning_config = reasoning_config

ns["CompressionFastLane"] = CompressionFastLane

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    actual_norm, provider_norm = r.get("actual_norm", ""), r.get("provider_norm", "")
    ns["_fast_lane_config_fields"] = lambda cfg: (
        r.get("cfg_provider", ""), r.get("cfg_model", ""), bool(r.get("non_reasoning")))
    ns["_fallback_provider_from_label"] = lambda label: "__ACTUAL__"
    ns["_normalize_aux_provider"] = (
        lambda p: actual_norm if p == "__ACTUAL__" else provider_norm)
    result = live(str(r.get("actual_provider_norm", actual_norm) or ""),
                  r.get("actual_model"), route_config={},
                  requested_provider=r.get("requested_provider"),
                  requested_model=r.get("requested_model"))
    py = 1 if getattr(result, "certified_non_reasoning", False) else 0
    c = r["out"]
    cases += 1
    if py != c:
        mism += 1
        print(f"MISMATCH {r} py={py} c={c}", file=sys.stderr)
print(f"ORACLE compress_fast_lane: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
