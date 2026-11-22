#!/usr/bin/env python3
"""Oracle for agent/prompt_caching.py:effective_cache_ttl.
AST-extracts the LIVE function and feeds caller-resolved membership booleans
through stubbed constants/helpers."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/prompt_caching.py"
ns = {"str": str, "Optional": object, "Any": object, "dict": dict}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name == "effective_cache_ttl":
        n.decorator_list = []
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["effective_cache_ttl"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    ns["_fast_lane_config_fields"] = lambda cfg: ("", "", False)
    ns["_normalize_aux_provider"] = lambda p: ""
    ns["_fallback_provider_from_label"] = lambda label: ""
    provider_lower = str(r.get("provider", "")).lower()
    ns["MEASURED_1H_PROVIDERS"] = {provider_lower} if r.get("measured") else set()
    ns["NO_1H_TIER_MODELS"] = {"__flat__"} if r.get("no_1h") else set()
    ns["ALIBABA_FAMILY_PROVIDERS"] = {provider_lower} if r.get("alibaba") else set()
    ns["_flat_model"] = lambda model: "__flat__"
    ns["_catalog_route"] = lambda provider, base_url: bool(r.get("catalog"))
    ns["is_qwen_model"] = lambda model: bool(r.get("qwen"))
    ns["route_ttl_ceiling"] = lambda provider, base_url: r.get("ceiling", "")
    ns["_get_auxiliary_task_config"] = lambda kind: {}
    py = live(r.get("ttl"), provider=r.get("provider", ""), auto=bool(r.get("auto")))
    c = r.get("out")
    cases += 1
    if py != c:
        mism += 1
        print(f"MISMATCH {r} py={py!r} c={c!r}", file=sys.stderr)
print(f"ORACLE effective_ttl: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
