#!/usr/bin/env python3
"""Differential oracle for gate w10_doctorconf_741cee (hermes_cli/doctor_config.py)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items(): setattr(m, k, v)
    sys.modules[name] = m
    return m
# parent-package bootstrap: load hermes_cli submodules without running __init__.py
h = _mod("hermes_cli"); h.__path__ = [os.path.join(Q, "hermes_cli")]
HINTS = ("DEEPINFRA_API_KEY", "OPENROUTER_API_KEY", "OPENAI_API_KEY", "ANTHROPIC_API_KEY", "ANTHROPIC_TOKEN",
    "OPENAI_BASE_URL", "NOUS_API_KEY", "GLM_API_KEY", "ZAI_API_KEY", "Z_AI_API_KEY", "KIMI_API_KEY",
    "KIMI_CN_API_KEY", "GMI_API_KEY", "FIREWORKS_API_KEY", "ACTUAL_API_KEY", "ACTUAL_BASE_URL", "MINIMAX_API_KEY",
    "MINIMAX_CN_API_KEY", "KILOCODE_API_KEY", "DEEPSEEK_API_KEY", "DASHSCOPE_API_KEY", "HF_TOKEN",
    "AI_GATEWAY_API_KEY", "OPENCODE_ZEN_API_KEY", "OPENCODE_GO_API_KEY", "COMMANDCODE_API_KEY", "XIAOMI_API_KEY",
    "TOKENHUB_API_KEY", "TOKENPLAN_API_KEY")
_mod("hermes_cli.doctor", _PROVIDER_ENV_HINTS=HINTS)
NAME = "w10_doctorconf_741cee_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "hermes_cli", "doctor_config.py"))
live = importlib.util.module_from_spec(sp); sys.modules[NAME] = live; sp.loader.exec_module(live)

def canon(v):
    if isinstance(v, (list, tuple)): return [canon(x) for x in v]
    if isinstance(v, dict): return {k: canon(x) for k, x in v.items()}
    return v

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    try:
        if func == "deprecated_config_keys":
            py = canon(live.collect_deprecated_config_keys(r.get("raw_config")))
        elif func == "deprecated_env_vars":
            py = canon(live.collect_deprecated_env_vars(r.get("env_map")))
        elif func == "endpoint_url":
            py = live._endpoint_url(r.get("entry"))
        elif func == "has_provider_env_config":
            py = live._has_provider_env_config(r.get("content", ""))
        else: continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr); continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w10_doctorconf_741cee: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
