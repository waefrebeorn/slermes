#!/usr/bin/env python3
"""Differential oracle for gateway/session_route_identity.py (gate w18_sessionrou_e77702).

External seams are stubbed with deterministic implementations that the C port
mirrors verbatim: normalize_provider (full alias table), resolve_provider
(identity; the AuthError path yields the same canonical spelling),
_get_named_custom_provider (providers:/custom_providers lookup) and the real
custom_provider_slug body.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


ALIASES = {
    "glm": "zai", "z-ai": "zai", "z.ai": "zai", "zhipu": "zai", "google": "gemini",
    "google-gemini": "gemini", "google-ai-studio": "gemini", "x-ai": "xai",
    "x.ai": "xai", "grok": "xai", "xai-oauth": "xai-oauth", "x-ai-oauth": "xai-oauth",
    "grok-oauth": "xai-oauth", "xai-grok-oauth": "xai-oauth", "kimi": "kimi-coding",
    "kimi-for-coding": "kimi-coding", "moonshot": "kimi-coding",
    "kimi-cn": "kimi-coding-cn", "moonshot-cn": "kimi-coding-cn", "step": "stepfun",
    "stepfun-coding-plan": "stepfun", "arcee-ai": "arcee", "arceeai": "arcee",
    "gmi-cloud": "gmi", "gmicloud": "gmi", "actual-computer": "actual",
    "actualcomputer": "actual", "aci": "actual", "minimax-china": "minimax-cn",
    "minimax_cn": "minimax-cn", "minimax-portal": "minimax-oauth",
    "minimax-global": "minimax-oauth", "minimax_oauth": "minimax-oauth",
    "alibaba_coding": "alibaba-coding-plan", "alibaba-coding": "alibaba-coding-plan",
    "alibaba_coding_plan": "alibaba-coding-plan", "claude": "anthropic",
    "claude-code": "anthropic", "aigateway": "ai-gateway", "vercel": "ai-gateway",
    "vercel-ai-gateway": "ai-gateway", "opencode": "opencode-zen", "zen": "opencode-zen",
    "qwen-portal": "qwen-oauth", "qwen-cli": "qwen-oauth", "qwen-oauth": "qwen-oauth",
    "hf": "huggingface", "hugging-face": "huggingface", "huggingface-hub": "huggingface",
    "mimo": "xiaomi", "xiaomi-mimo": "xiaomi", "tencent": "tencent-tokenhub",
    "tokenhub": "tencent-tokenhub", "tencent-cloud": "tencent-tokenhub",
    "tencentmaas": "tencent-tokenhub", "tokenplan": "tencent-tokenplan",
    "tencent-lkeap": "tencent-tokenplan", "aws": "bedrock", "aws-bedrock": "bedrock",
    "amazon-bedrock": "bedrock", "amazon": "bedrock", "go": "opencode-go",
    "opencode-go-sub": "opencode-go", "kilo": "kilocode", "kilo-code": "kilocode",
    "kilo-gateway": "kilocode", "lmstudio": "lmstudio", "lm-studio": "lmstudio",
    "lm_studio": "lmstudio", "chatgpt": "openai-codex", "chatgpt-codex": "openai-codex",
    "local": "custom", "ollama": "custom", "ollama_cloud": "ollama-cloud",
    "vllm": "custom", "llamacpp": "custom", "llama.cpp": "custom", "llama-cpp": "custom",
}


def normalize_provider(p):
    n = (p or "openrouter").strip().lower()
    return ALIASES.get(n, n)


def slug(display_name, provider_key=""):
    identity = str(provider_key or "").strip() or str(display_name or "").strip()
    normalized = identity.lower().replace(" ", "-")
    return normalized if normalized.startswith("custom:") else "custom:" + normalized


def named(requested, config=None, resolve_credentials=False):
    name = str(requested or "").strip().lower()
    if not name or name == "auto":
        return None
    cfg = config or {}
    providers = cfg.get("providers")
    if isinstance(providers, dict):
        for key, entry in providers.items():
            k = str(key).strip().lower()
            if k == name or "custom:" + k == name:
                return dict(entry) if isinstance(entry, dict) else None
    custom = cfg.get("custom_providers")
    if isinstance(custom, list):
        for entry in custom:
            if isinstance(entry, dict):
                nm = str(entry.get("name") or "").strip().lower().replace(" ", "-")
                if nm == name or "custom:" + nm == name:
                    return dict(entry)
    return None


class AuthError(Exception):
    pass


cli = _mod("hermes_cli")
cli.__path__ = []
_mod("hermes_cli.models", normalize_provider=normalize_provider)
_mod("hermes_cli.auth", AuthError=AuthError,
     resolve_provider=lambda p: p)
_mod("hermes_cli.runtime_provider_custom", _get_named_custom_provider=named)
_mod("hermes_cli.providers", custom_provider_slug=slug)

SRC = Q + "/gateway/session_route_identity.py"
spec = importlib.util.spec_from_file_location("w18_sessionrou_e77702_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w18_sessionrou_e77702_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "resolved_route":
        return list(live.resolved_route(r.get("model_cfg", {}), r.get("config", {})))
    if func == "configured_entry_key":
        return live.configured_entry_key(r.get("identity", ""), r.get("config", {}))
    if func == "profile_route_identity":
        return live.profile_route_identity(r.get("config", {}))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_sessionrou_e77702: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
