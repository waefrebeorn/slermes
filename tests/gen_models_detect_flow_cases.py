#!/usr/bin/env python3
"""Generator for tests/fixtures/models_detect_flow.jsonl.

Emits >=5 cases per ported func (current_provider_catalog_match,
current_provider_owns_vendor, provider_has_credentials) covering: skip/custom/
alias gates, exact + bare-part catalog matching, seam Exception catch vs
BaseException passthrough (incl. ExceptionGroup multi-base + synthetic unknown),
null-catalog TypeError, $seq queues/exhaustion, import-failure gating
(ModuleNotFoundError), single/mixed/unclassifiable vendor ownership, and the
full credential ladder (openrouter env key, auth status, credential pool,
short-circuit, placeholder shapes).

Secret-shaped strings are assembled by CONCATENATION so this source file carries
no realistic key literal; the JSONL is written at runtime by json.dumps.

Run:  python tests/gen_models_detect_flow_cases.py
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "models_detect_flow.jsonl"

# Ordered _VENDOR_PREFIXES model (detect_vendor startswith fallback iterates in
# this insertion order on both sides).
VP = {
    "claude": "anthropic", "gpt": "openai", "o1": "openai", "o3": "openai",
    "o4": "openai", "gemini": "google", "gemma": "google", "deepseek": "deepseek",
    "glm": "z-ai", "kimi": "moonshotai", "minimax": "minimax", "grok": "x-ai",
    "qwen": "qwen", "mimo": "xiaomi", "trinity": "arcee-ai", "nemotron": "nvidia",
    "llama": "meta-llama", "step": "stepfun",
}
ALIAS = {"or": "openrouter", "xai": "xai-oauth", "ollama": "custom",
         "codex": "openai-codex", "openai-api": "openai"}
AGG = ["openrouter", "nous", "ai-gateway", "kilocode"]
PM = {
    "openai-codex": ["gpt-6-astra", "gpt-5", "o3-mini"],
    "xai-oauth": ["grok-4.6", "grok-4"],
    "anthropic": ["claude-opus-4-6", "claude-sonnet-4-6"],
    "openai": ["gpt-6", "gpt-5"],
    "qwen": ["qwen3.5-plus", "qwen-max"],
    "mixed": ["gpt-6", "claude-opus"],
    "unclass": ["mystery-model-xyz"],
    "empty": [],
}
ENV = {"aliases": ALIAS, "vendor_prefixes": VP, "aggregators": AGG, "provider_models": PM}

# Secret-shaped values assembled by concatenation (no full key literal in source).
USABLE_KEY = "***" + "abcd1234xyz"
SK_REAL = "sk-" + "abcd1234"
SK_X = "sk-" + "xxxx"
GHP_X = "ghp_" + "xxx"
HF_X = "hf_" + "xxx"


def env(**over):
    e = dict(ENV)
    e.update(over)
    return e


def case(func, inp):
    return {"func": func, "in": inp}


CASES = []
A = CASES.append

# ── current_provider_catalog_match ──────────────────────────────────────────
CM = "current_provider_catalog_match"
A(case(CM, {"model_name": "GPT-6-Astra", "current_provider": "openai-codex", "env": env(),
            "io": {"cached_provider_model_ids": ["gpt-6-astra", "gpt-5"]}}))
A(case(CM, {"model_name": "gpt-6", "current_provider": "", "env": env(), "io": {}}))
A(case(CM, {"model_name": "gpt-6", "current_provider": "auto", "env": env(), "io": {}}))
A(case(CM, {"model_name": "gpt-6", "current_provider": "openrouter", "env": env(), "io": {}}))
A(case(CM, {"model_name": "gpt-6", "current_provider": "custom", "env": env(), "io": {}}))
A(case(CM, {"model_name": "gpt-6", "current_provider": "custom:foo", "env": env(), "io": {}}))
A(case(CM, {"model_name": "gpt-6", "current_provider": "or", "env": env(), "io": {}}))  # alias->openrouter skip
A(case(CM, {"model_name": "", "current_provider": "openai", "env": env(), "io": {}}))
A(case(CM, {"model_name": None, "current_provider": "openai", "env": env(), "io": {}}))
A(case(CM, {"model_name": "claude-opus-4-6", "current_provider": "nous", "env": env(),
            "io": {"cached_provider_model_ids": ["anthropic/claude-opus-4-6", "x"]}}))
A(case(CM, {"model_name": "gpt-9", "current_provider": "openai", "env": env(),
            "io": {"cached_provider_model_ids": ["gpt-5"]}}))
A(case(CM, {"model_name": "claude", "current_provider": "anthropic", "env": env(),
            "io": {"cached_provider_model_ids": ["vendor/claude", "claude"]}}))  # exact preferred
A(case(CM, {"model_name": "gpt-6", "current_provider": "openai", "env": env(),
            "io": {"cached_provider_model_ids": {"__raise__": "RuntimeError", "msg": "boom"}}}))
A(case(CM, {"model_name": "gpt-6", "current_provider": "openai", "env": env(),
            "io": {"cached_provider_model_ids": {"__raise__": "ValueError", "msg": "bad"}}}))
A(case(CM, {"model_name": "gpt-6", "current_provider": "openai", "env": env(),
            "io": {"cached_provider_model_ids": {"__raise__": "SystemExit", "msg": "x"}}}))
A(case(CM, {"model_name": "gpt-6", "current_provider": "openai", "env": env(),
            "io": {"cached_provider_model_ids": {"__raise__": "KeyboardInterrupt", "msg": ""}}}))
A(case(CM, {"model_name": "gpt-6", "current_provider": "openai", "env": env(),
            "io": {"cached_provider_model_ids": {"__raise__": "GeneratorExit", "msg": ""}}}))
A(case(CM, {"model_name": "gpt-6", "current_provider": "openai", "env": env(),
            "io": {"cached_provider_model_ids": {"__raise__": "BaseExceptionGroup", "msg": "g"}}}))
A(case(CM, {"model_name": "gpt-6", "current_provider": "openai", "env": env(),
            "io": {"cached_provider_model_ids": {"__raise__": "ExceptionGroup", "msg": "g"}}}))  # caught
A(case(CM, {"model_name": "gpt-6", "current_provider": "openai", "env": env(),
            "io": {"cached_provider_model_ids": {"__raise__": "CustomError", "msg": "synthetic"}}}))  # caught
A(case(CM, {"model_name": "gpt-6", "current_provider": "openai", "env": env(),
            "io": {"cached_provider_model_ids": None}}))  # TypeError uncaught
A(case(CM, {"model_name": "gpt-6", "current_provider": "openai", "env": env(),
            "io": {"cached_provider_model_ids": []}}))
A(case(CM, {"model_name": "gpt-6", "current_provider": "openai", "env": env(),
            "io": {"cached_provider_model_ids": {"$seq": []}}}))  # exhaustion -> caught
A(case(CM, {"model_name": "gpt-6", "current_provider": "openai", "env": env(),
            "io": {"cached_provider_model_ids": {"$seq": [["gpt-6"], ["x"]]}}}))  # list result wrapped
A(case(CM, {"model_name": "gpt-6", "current_provider": "openai", "env": env(import_fail=["hermes_cli.models"]),
            "io": {}}))
A(case(CM, {"model_name": " gpt-6 ", "current_provider": "  OpenAI  ", "env": env(),
            "io": {"cached_provider_model_ids": ["GPT-6"]}}))
A(case(CM, {"model_name": "gpt-9", "current_provider": "openai", "env": env(),
            "io": {"cached_provider_model_ids": ["gpt-6", 5]}}))  # non-str mid -> AttributeError
A(case(CM, {"model_name": "gpt-6", "current_provider": "openai\x1c", "env": env(),
            "io": {"cached_provider_model_ids": ["gpt-6"]}}))  # extended strip ws
A(case(CM, {"model_name": "gpt-6", "current_provider": "ollama", "env": env(), "io": {}}))  # alias->custom skip

# ── current_provider_owns_vendor ────────────────────────────────────────────
OV = "current_provider_owns_vendor"
A(case(OV, {"model_name": "gpt-6-astra", "current_provider": "openai-codex", "env": env(), "io": {}}))
A(case(OV, {"model_name": "gpt-6", "current_provider": "", "env": env(), "io": {}}))
A(case(OV, {"model_name": "gpt-6", "current_provider": "auto", "env": env(), "io": {}}))
A(case(OV, {"model_name": "gpt-6", "current_provider": "custom:foo", "env": env(), "io": {}}))
A(case(OV, {"model_name": "gpt-6", "current_provider": "or", "env": env(), "io": {}}))  # alias->skip
A(case(OV, {"model_name": "gpt-6", "current_provider": "nous", "env": env(), "io": {}}))  # aggregator
A(case(OV, {"model_name": "", "current_provider": "openai-codex", "env": env(), "io": {}}))
A(case(OV, {"model_name": "mystery-xyz", "current_provider": "openai-codex", "env": env(), "io": {}}))
A(case(OV, {"model_name": "gpt-6", "current_provider": "mixed", "env": env(), "io": {}}))
A(case(OV, {"model_name": "gpt-6", "current_provider": "unclass", "env": env(), "io": {}}))
A(case(OV, {"model_name": "gpt-6", "current_provider": "empty", "env": env(), "io": {}}))
A(case(OV, {"model_name": "gpt-6", "current_provider": "nosuch", "env": env(), "io": {}}))
A(case(OV, {"model_name": "anthropic/claude-opus", "current_provider": "anthropic", "env": env(), "io": {}}))
A(case(OV, {"model_name": "qwen3.5-plus", "current_provider": "qwen", "env": env(), "io": {}}))
A(case(OV, {"model_name": " gpt-6-astra ", "current_provider": " OpenAI-Codex ", "env": env(), "io": {}}))
A(case(OV, {"model_name": "gpt-6", "current_provider": "openai", "env": env(import_fail=["hermes_cli.model_normalize"]),
            "io": {}}))
A(case(OV, {"model_name": "gpt-6", "current_provider": "openai", "env": env(import_fail=["hermes_cli.models"]),
            "io": {}}))
A(case(OV, {"model_name": "gpt-6", "current_provider": "openai", "env": env(provider_models={"openai": None}),
            "io": {}}))  # present null -> TypeError
A(case(OV, {"model_name": "gpt-6", "current_provider": "openai", "env": env(provider_models={"openai": [5]}),
            "io": {}}))  # non-str mid -> AttributeError
A(case(OV, {"model_name": "gpt-6", "current_provider": "openai", "env": env(vendor_prefixes={}), "io": {}}))
A(case(OV, {"model_name": "/claude", "current_provider": "anthropic", "env": env(), "io": {}}))
A(case(OV, {"model_name": "gpt-6-astra", "current_provider": "codex", "env": env(), "io": {}}))  # alias normalize
A(case(OV, {"model_name": "grok-4.6", "current_provider": "xai", "env": env(), "io": {}}))  # xai->xai-oauth

# ── provider_has_credentials ────────────────────────────────────────────────
HC = "provider_has_credentials"
A(case(HC, {"provider": "", "env": env(), "io": {}}))
A(case(HC, {"provider": None, "env": env(), "io": {}}))
A(case(HC, {"provider": "custom", "env": env(), "io": {}}))
A(case(HC, {"provider": "custom:foo", "env": env(), "io": {}}))
A(case(HC, {"provider": "  Custom  ", "env": env(), "io": {}}))
A(case(HC, {"provider": "openrouter", "env": env(), "io": {"get_env_value_prefer_dotenv": USABLE_KEY}}))
A(case(HC, {"provider": "openrouter", "env": env(), "io": {"get_env_value_prefer_dotenv": SK_REAL}}))
A(case(HC, {"provider": "openrouter", "env": env(),
            "io": {"get_env_value_prefer_dotenv": SK_X, "get_auth_status": {"logged_in": True}}}))
A(case(HC, {"provider": "openrouter", "env": env(),
            "io": {"get_env_value_prefer_dotenv": None, "get_auth_status": {"configured": True}}}))
A(case(HC, {"provider": "openrouter", "env": env(),
            "io": {"get_env_value_prefer_dotenv": "abc", "get_auth_status": {"logged_in": True}}}))
A(case(HC, {"provider": "openrouter", "env": env(),
            "io": {"get_env_value_prefer_dotenv": "changeme", "get_auth_status": {"logged_in": True}}}))
A(case(HC, {"provider": "openrouter", "env": env(),
            "io": {"get_env_value_prefer_dotenv": "your_key_here", "get_auth_status": {"logged_in": True}}}))
A(case(HC, {"provider": "anthropic", "env": env(), "io": {"get_auth_status": {"logged_in": True}}}))
A(case(HC, {"provider": "anthropic", "env": env(), "io": {"get_auth_status": {"logged_in": 1}}}))
A(case(HC, {"provider": "anthropic", "env": env(), "io": {"get_auth_status": {"configured": "yes"}}}))
A(case(HC, {"provider": "anthropic", "env": env(),
            "io": {"get_auth_status": None, "load_pool": {"pool": True}, "has_credentials": True, "has_available": True}}))
A(case(HC, {"provider": "anthropic", "env": env(),
            "io": {"get_auth_status": {}, "load_pool": {"pool": True}, "has_credentials": False, "has_available": True}}))
A(case(HC, {"provider": "anthropic", "env": env(),
            "io": {"get_auth_status": None, "load_pool": {"pool": True}, "has_credentials": True, "has_available": False}}))
A(case(HC, {"provider": "anthropic", "env": env(), "io": {"get_auth_status": None, "load_pool": None}}))
A(case(HC, {"provider": "anthropic", "env": env(), "io": {"get_auth_status": {"__raise__": "RuntimeError", "msg": "x"}}}))
A(case(HC, {"provider": "anthropic", "env": env(), "io": {"get_auth_status": {"__raise__": "SystemExit", "msg": "x"}}}))
A(case(HC, {"provider": "anthropic", "env": env(),
            "io": {"get_auth_status": None, "load_pool": {"__raise__": "KeyboardInterrupt", "msg": ""}}}))
A(case(HC, {"provider": "anthropic", "env": env(),
            "io": {"get_auth_status": None, "load_pool": {"pool": True}, "has_credentials": {"__raise__": "ValueError", "msg": "v"}}}))
A(case(HC, {"provider": "anthropic", "env": env(),
            "io": {"get_auth_status": None, "load_pool": {"pool": True}, "has_credentials": True,
                   "has_available": {"__raise__": "GeneratorExit", "msg": ""}}}))
A(case(HC, {"provider": "openrouter", "env": env(),
            "io": {"get_env_value_prefer_dotenv": {"__raise__": "RuntimeError", "msg": "e"}}}))
A(case(HC, {"provider": "openrouter", "env": env(),
            "io": {"get_env_value_prefer_dotenv": {"__raise__": "SystemExit", "msg": "e"}}}))
A(case(HC, {"provider": "anthropic", "env": env(import_fail=["agent.credential_pool"]), "io": {"get_auth_status": None}}))
A(case(HC, {"provider": "anthropic", "env": env(import_fail=["hermes_cli.auth"]), "io": {}}))
A(case(HC, {"provider": "anthropic", "env": env(import_fail=["hermes_cli.config"]), "io": {}}))
A(case(HC, {"provider": "anthropic", "env": env(), "io": {"get_auth_status": "x"}}))  # non-dict truthy
A(case(HC, {"provider": "anthropic", "env": env(), "io": {"get_auth_status": 5}}))  # non-dict truthy
A(case(HC, {"provider": "anthropic", "env": env(),
            "io": {"get_auth_status": {"$seq": [None, {"logged_in": True}]}, "load_pool": {"pool": True},
                   "has_credentials": True, "has_available": True}}))
A(case(HC, {"provider": "anthropic", "env": env(),
            "io": {"get_auth_status": None, "load_pool": {"pool": True}, "has_credentials": {"$seq": []}}}))
A(case(HC, {"provider": "anthropic", "env": env(),
            "io": {"get_auth_status": None, "load_pool": {"__raise__": "ExceptionGroup", "msg": "g"}}}))  # caught
A(case(HC, {"provider": "anthropic", "env": env(),
            "io": {"get_auth_status": None, "load_pool": {"__raise__": "BaseException", "msg": "b"}}}))  # passthrough
A(case(HC, {"provider": "openrouter", "env": env(), "io": {"get_env_value_prefer_dotenv": "abcd"}}))  # len 4 usable
A(case(HC, {"provider": "anthropic", "env": env(),
            "io": {"get_auth_status": {"logged_in": False, "configured": False}, "load_pool": {"pool": True},
                   "has_credentials": True, "has_available": True}}))
A(case(HC, {"provider": "anthropic", "env": env(),
            "io": {"get_auth_status": None, "load_pool": {"pool": True}, "has_credentials": True, "has_available": "yes"}}))
A(case(HC, {"provider": "anthropic", "env": env(),
            "io": {"get_auth_status": None, "load_pool": {"pool": True}, "has_credentials": 1, "has_available": True}}))
A(case(HC, {"provider": "openrouter", "env": env(),
            "io": {"get_env_value_prefer_dotenv": GHP_X, "get_auth_status": {"logged_in": True}}}))
A(case(HC, {"provider": "openrouter", "env": env(),
            "io": {"get_env_value_prefer_dotenv": HF_X, "get_auth_status": {"logged_in": True}}}))


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    for fn, n in counts.items():
        assert n >= 5, f"{fn}: only {n} cases (<5)"
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for fn, n in sorted(counts.items()):
        print(f"  {fn}: {n}")


if __name__ == "__main__":
    main()