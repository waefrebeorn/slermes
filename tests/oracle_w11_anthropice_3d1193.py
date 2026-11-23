#!/usr/bin/env python3
"""Differential oracle for agent/anthropic_endpoints.py (gate w11_anthropice_3d1193)."""
import importlib.util, json, sys
try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w11_anthropice_3d1193"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/agent/anthropic_endpoints.py"
_spec = importlib.util.spec_from_file_location("anthropic_endpoints_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["anthropic_endpoints_live"] = live
_spec.loader.exec_module(live)

FUNCS = {
    "normalize_base_url_text": live._normalize_base_url_text,
    "normalized_lower": live._normalized_lower,
    "is_third_party_anthropic_endpoint": live._is_third_party_anthropic_endpoint,
    "is_kimi_coding_endpoint": live._is_kimi_coding_endpoint,
    "is_opencode_endpoint": live._is_opencode_endpoint,
    "model_name_is_kimi_family": live._model_name_is_kimi_family,
    "is_kimi_family_endpoint": live._is_kimi_family_endpoint,
    "model_name_is_deepseek_thinking": live._model_name_is_deepseek_thinking,
    "is_deepseek_anthropic_endpoint": live._is_deepseek_anthropic_endpoint,
    "is_minimax_anthropic_endpoint": live._is_minimax_anthropic_endpoint,
    "is_azure_anthropic_endpoint": live._is_azure_anthropic_endpoint,
    "is_bedrock_runtime_endpoint": live._is_bedrock_runtime_endpoint,
    "is_vertex_ai_endpoint": live._is_vertex_ai_endpoint,
    "is_claude_platform_endpoint": live._is_claude_platform_endpoint,
    "base_url_needs_context_1m_beta": live._base_url_needs_context_1m_beta,
}

def live_out(rec):
    func = rec.get("func")
    if func == "requires_bearer_auth":
        live._is_nous_portal_endpoint = lambda b: bool(rec.get("nous_override"))
        return live._requires_bearer_auth(rec.get("base_url"))
    fn = FUNCS.get(func)
    if fn is None: return None
    if func in ("is_kimi_family_endpoint",):
        return fn(rec.get("base_url"), rec.get("model"))
    if func in ("model_name_is_kimi_family", "model_name_is_deepseek_thinking"):
        return fn(rec.get("model"))
    return fn(rec.get("base_url"))

def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        try: rec = json.loads(line)
        except ValueError: continue
        if rec.get("func") not in set(FUNCS) | {"requires_bearer_auth"}: continue
        cases += 1
        try: py = live_out(rec)
        except Exception as exc: py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} func={rec.get('func')} C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)"); return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0

if __name__ == "__main__":
    sys.exit(main())
