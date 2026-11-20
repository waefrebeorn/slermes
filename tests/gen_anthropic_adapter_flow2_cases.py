#!/usr/bin/env python3
"""Generate tests/fixtures/anthropic_adapter_flow2.jsonl — scripted seam cases
for the 12 UNCLAIMED agent/anthropic_adapter.py FLOW defs ported in
src/agent/anthropic_adapter_flow2.c.

Each case is {"func": <live_name>, "in": {...args..., "io": {seam: script}}}.
>=5 cases per function cover decision branches, short-circuit traces, implicit
and scripted raises (Exception vs BaseException), fail-closed missing scripts,
and $seq queues.  Scripts obey the fixture domain: ASCII; seam args use ABSENCE
(not explicit null) for None; integral JSON numbers; [str,[int,int]] version
tuples.  The oracle drives the LIVE quarry defs under the SAME scripts; the C
harness emits the DECISION SEQUENCE for the identical inputs.

After deriving every case the generator re-drives it in pure scripted mode via
the verification oracle (its own seams) and flags NameError/AttributeError
seeding gaps before writing the fixture.

Run: python tests/gen_anthropic_adapter_flow2_cases.py
"""
import importlib.util
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "anthropic_adapter_flow2.jsonl"

_spec = importlib.util.spec_from_file_location("oracle_aa2", HERE / "oracle_anthropic_adapter_flow2.py")
oracle = importlib.util.module_from_spec(_spec)
sys.modules["oracle_aa2"] = oracle
_spec.loader.exec_module(oracle)


def C(func, inp, io=None):
    d = dict(inp)
    if io is not None:
        d["io"] = io
    return {"func": func, "in": d}


CASES = []


def add(func, inp, io=None):
    CASES.append(C(func, inp, io))


# ── 1. _model_matches (pure) ────────────────────────────────────────────
add("_model_matches", {"model": "claude-opus-4-7", "substrings": ["claude-opus-4-6", "claude-opus-4-7"]})
add("_model_matches", {"model": "CLAUDE-OPUS-4.7", "substrings": ["claude-opus"]})
add("_model_matches", {"model": "gpt-4o", "substrings": ["claude-3", "haiku"]})
add("_model_matches", {"model": "", "substrings": ["claude"]})
add("_model_matches", {"model": "claude-3-5-sonnet", "substrings": ["claude-3-5-sonnet", "claude-3-opus"]})
add("_model_matches", {"model": "minimax", "substrings": []})

# ── 2. _beta_header (pure) ──────────────────────────────────────────────
add("_beta_header", {"betas": []})
add("_beta_header", {"betas": ["interleaved-thinking-2025-05-14"]})
add("_beta_header", {"betas": ["a", "b", "c"]})
add("_beta_header", {"betas": ["context-1m-2025-08-07", "fine-grained-tool-streaming-2025-05-14"]})
add("_beta_header", {"betas": ["single-with-dashes"]})

# ── 3. _normalize_to_mcp_wire (pure) ────────────────────────────────────
add("_normalize_to_mcp_wire", {"name": "read_file"})
add("_normalize_to_mcp_wire", {"name": "mcp_foo"})
add("_normalize_to_mcp_wire", {"name": "mcp__foo"})
add("_normalize_to_mcp_wire", {"name": "mcp_server_tool"})
add("_normalize_to_mcp_wire", {"name": ""})
add("_normalize_to_mcp_wire", {"name": "memory"})

# ── 4. _apply_oauth_prose_aliases (pure, word-boundary replace) ─────────
add("_apply_oauth_prose_aliases", {"text": "Use session_search to find things."})
add("_apply_oauth_prose_aliases", {"text": "session_search"})
add("_apply_oauth_prose_aliases", {"text": "The tools/session_search_tool.py file"})
add("_apply_oauth_prose_aliases", {"text": "session_search and session_search"})
add("_apply_oauth_prose_aliases", {"text": "xsession_search"})
add("_apply_oauth_prose_aliases", {"text": "session_search."})

# ── 5. _attribution_headers (seam get_version_info) ─────────────────────
add("_attribution_headers", {}, {"get_version_info": {"base_version": "2.1.74"}})
add("_attribution_headers", {}, {"get_version_info": {"base_version": "1.0"}})
add("_attribution_headers", {}, {"get_version_info": {"base_version": ""}})
add("_attribution_headers", {}, {"get_version_info": {"base_version": "9.9.9-beta"}})
add("_attribution_headers", {}, {"get_version_info": [{"base_version": "1.0"}, {"base_version": "2.0"}]})
add("_attribution_headers", {}, {})   # fail-closed: no script

# ── 6. _client_timeout (seam httpx.Timeout) ─────────────────────────────
add("_client_timeout", {"timeout": 30.0}, {"httpx.Timeout": {"kind": "Timeout"}})
add("_client_timeout", {"timeout": 0}, {"httpx.Timeout": {"kind": "Timeout"}})
add("_client_timeout", {"timeout": -5}, {"httpx.Timeout": {"kind": "Timeout"}})
add("_client_timeout", {"timeout": None}, {"httpx.Timeout": {"kind": "Timeout"}})
add("_client_timeout", {"timeout": "abc"}, {"httpx.Timeout": {"kind": "Timeout"}})
add("_client_timeout", {"timeout": 0.5}, {"httpx.Timeout": [{"kind": "T"}, {"kind": "U"}]})
add("_client_timeout", {"timeout": 10}, {"httpx.Timeout": {"__raise__": "ValueError", "msg": "bad timeout"}})

# ── 7. _auth_style (4 seams, short-circuits + raise) ────────────────────
add("_auth_style", {"api_key": "sk-ant-api03-x", "base_url": "https://x", "normalized_base_url": "https://x"},
    {"is_kimi_coding_endpoint": True})
add("_auth_style", {"api_key": "sk-ant-api03-x", "base_url": "https://x", "normalized_base_url": "https://x"},
    {"is_kimi_coding_endpoint": False, "requires_bearer_auth": True})
add("_auth_style", {"api_key": "k", "base_url": "https://x", "normalized_base_url": "https://x"},
    {"is_kimi_coding_endpoint": False, "requires_bearer_auth": False, "is_third_party_anthropic_endpoint": True})
add("_auth_style", {"api_key": "sk-ant-oat01-x", "base_url": "https://x", "normalized_base_url": "https://x"},
    {"is_kimi_coding_endpoint": False, "requires_bearer_auth": False,
     "is_third_party_anthropic_endpoint": False, "is_oauth_token": True})
add("_auth_style", {"api_key": "sk-ant-api03-x", "base_url": "https://x", "normalized_base_url": "https://x"},
    {"is_kimi_coding_endpoint": False, "requires_bearer_auth": False,
     "is_third_party_anthropic_endpoint": False, "is_oauth_token": False})
add("_auth_style", {"api_key": "k", "base_url": "https://x", "normalized_base_url": "https://x"},
    {"is_kimi_coding_endpoint": [False, True], "requires_bearer_auth": True})
add("_auth_style", {"api_key": "k", "base_url": "https://x", "normalized_base_url": "https://x"},
    {"is_kimi_coding_endpoint": False,
     "requires_bearer_auth": {"__raise__": "RuntimeError", "msg": "bearer probe failed"}})

# ── 8. _thinking_kwargs (5 seams, many branches) ───────────────────────
add("_thinking_kwargs", {"reasoning_config": {"enabled": False}, "model": "claude-sonnet-5-5", "effective_max_tokens": 8000},
    {"model_matches": True})
add("_thinking_kwargs", {"reasoning_config": {"enabled": False}, "model": "claude-opus-4-7", "effective_max_tokens": 8000},
    {"model_matches": False, "accepts_thinking_disable": True})
add("_thinking_kwargs", {"reasoning_config": {"enabled": False}, "model": "claude-fable", "effective_max_tokens": 8000},
    {"model_matches": False, "accepts_thinking_disable": False})
add("_thinking_kwargs", {"reasoning_config": {}, "model": "claude-haiku-4-5", "effective_max_tokens": 8000},
    {"is_pre_thinking_haiku": True})
add("_thinking_kwargs", {"reasoning_config": {"effort": "high"}, "model": "claude-opus-4-7", "effective_max_tokens": 20000},
    {"is_pre_thinking_haiku": False, "supports_adaptive_thinking": True})
add("_thinking_kwargs", {"reasoning_config": {"effort": "xhigh"}, "model": "claude-sonnet-4-6", "effective_max_tokens": 20000},
    {"is_pre_thinking_haiku": False, "supports_adaptive_thinking": True, "supports_xhigh_effort": False})
add("_thinking_kwargs", {"reasoning_config": {"effort": "xhigh"}, "model": "claude-opus-4-7", "effective_max_tokens": 20000},
    {"is_pre_thinking_haiku": False, "supports_adaptive_thinking": True, "supports_xhigh_effort": True})
add("_thinking_kwargs", {"reasoning_config": {"effort": "low"}, "model": "minimax", "effective_max_tokens": 1000},
    {"is_pre_thinking_haiku": False, "supports_adaptive_thinking": False})
add("_thinking_kwargs", {"reasoning_config": {"effort": "weird"}, "model": "qwen3", "effective_max_tokens": 50000},
    {"is_pre_thinking_haiku": False, "supports_adaptive_thinking": False})
add("_thinking_kwargs", {"reasoning_config": {"effort": "high"}, "model": "claude-opus-4-7", "effective_max_tokens": 8000},
    {"is_pre_thinking_haiku": False,
     "supports_adaptive_thinking": {"__raise__": "ValueError", "msg": "thinking probe failed"}})

# ── 9. _is_pre_thinking_haiku (seam claude_family_version) ──────────────
add("_is_pre_thinking_haiku", {"model": "claude-3-5-haiku"}, {"claude_family_version": [["haiku", [3, 5]]]})
add("_is_pre_thinking_haiku", {"model": "claude-haiku-4-5"}, {"claude_family_version": [["haiku", [4, 5]]]})
add("_is_pre_thinking_haiku", {"model": "claude-haiku-5-5"}, {"claude_family_version": [["haiku", [5, 5]]]})
add("_is_pre_thinking_haiku", {"model": "claude-haiku-5-6"}, {"claude_family_version": [["haiku", [5, 6]]]})
add("_is_pre_thinking_haiku", {"model": "claude-haiku-6-0"}, {"claude_family_version": [["haiku", [6, 0]]]})
add("_is_pre_thinking_haiku", {"model": "claude-fable"}, {})   # no "haiku": short-circuit, no seam
add("_is_pre_thinking_haiku", {"model": "claude-haiku-x"}, {"claude_family_version": None})
add("_is_pre_thinking_haiku", {"model": "claude-haiku-4"}, {"claude_family_version": [["opus", [4, 0]]]})
add("_is_pre_thinking_haiku", {"model": "claude-haiku-4-5"},
    {"claude_family_version": {"__raise__": "RuntimeError", "msg": "family parse failed"}})

# ── 10. _base_client_kwargs (3 seams) ──────────────────────────────────
add("_base_client_kwargs", {"base_url": "http://localhost:1234/v1", "timeout": 10},
    {"client_timeout": {"kind": "Timeout"}, "normalize_base_url_text": "http://localhost:1234/v1",
     "is_azure_anthropic_endpoint": False})
add("_base_client_kwargs", {"base_url": "https://api.anthropic.com", "timeout": 30},
    {"client_timeout": {"kind": "Timeout"}, "normalize_base_url_text": "https://api.anthropic.com/",
     "is_azure_anthropic_endpoint": False})
add("_base_client_kwargs", {"base_url": "https://x.openai.azure.com", "timeout": 20},
    {"client_timeout": {"kind": "Timeout"}, "normalize_base_url_text": "https://x.openai.azure.com/",
     "is_azure_anthropic_endpoint": True})
add("_base_client_kwargs", {"base_url": "https://x/v1", "timeout": 15},
    {"client_timeout": {"kind": "Timeout"}, "normalize_base_url_text": "https://x?api-version=2024",
     "is_azure_anthropic_endpoint": True})
add("_base_client_kwargs", {"base_url": "https://x", "timeout": 5},
    {"client_timeout": {"kind": "Timeout"}, "normalize_base_url_text": "",
     "is_azure_anthropic_endpoint": False})
add("_base_client_kwargs", {"base_url": "https://x", "timeout": 5},
    {"client_timeout": [{"kind": "T"}, {"kind": "U"}], "normalize_base_url_text": "https://x",
     "is_azure_anthropic_endpoint": False})
add("_base_client_kwargs", {"base_url": "https://x", "timeout": 5},
    {"client_timeout": {"kind": "Timeout"},
     "normalize_base_url_text": {"__raise__": "ValueError", "msg": "normalize failed"}})

# ── 11. _custom_provider_extra_headers (Exception vs BaseException) ────
add("_custom_provider_extra_headers", {}, {})
add("_custom_provider_extra_headers", {"base_url": ""}, {})
add("_custom_provider_extra_headers", {"base_url": "https://x"},
    {"get_custom_provider_extra_headers": {"HTTP-Referer": "https://ref", "X-Title": "T"}})
add("_custom_provider_extra_headers", {"base_url": "https://x"},
    {"get_custom_provider_extra_headers": {}})
add("_custom_provider_extra_headers", {"base_url": "https://x"},
    {"get_custom_provider_extra_headers": {"__raise__": "ValueError", "msg": "cfg error"},
     "logger.debug": None})
add("_custom_provider_extra_headers", {"base_url": "https://x"},
    {"get_custom_provider_extra_headers": {"__raise__": "KeyError", "msg": "missing"},
     "logger.debug": None})
add("_custom_provider_extra_headers", {"base_url": "https://x"},
    {"get_custom_provider_extra_headers": {"__raise__": "SystemExit", "msg": "shutdown"}})
add("_custom_provider_extra_headers", {"base_url": "https://x"},
    {"get_custom_provider_extra_headers": {"__raise__": "ValueError", "msg": "cfg error"},
     "logger.debug": {"__raise__": "RuntimeError", "msg": "logger failed"}})

# ── 12. buffer_anthropic_tool_input (stateful mutator) ──────────────────
_BETA = "fine-grained-tool-streaming-2025-05-14"
add("buffer_anthropic_tool_input", {"api_kwargs": {"tools": [{"name": "a"}, {"name": "b"}]}, "base_url": "https://x"},
    {"common_betas_for_base_url": [["interleaved-thinking-2025-05-14", _BETA]]})
add("buffer_anthropic_tool_input", {"api_kwargs": {"tools": [{"name": "a"}]}, "base_url": "https://x"},
    {"common_betas_for_base_url": [["interleaved-thinking-2025-05-14"]]})
add("buffer_anthropic_tool_input", {"api_kwargs": {"tools": []}, "base_url": "https://x"},
    {"common_betas_for_base_url": [[_BETA]]})
add("buffer_anthropic_tool_input", {"api_kwargs": {"max_tokens": 100}, "base_url": "https://x"},
    {"common_betas_for_base_url": [[_BETA]]})
add("buffer_anthropic_tool_input", {"api_kwargs": {"tools": [{"name": "a"}]}},
    {"common_betas_for_base_url": [[_BETA]]})
add("buffer_anthropic_tool_input", {"api_kwargs": {"tools": [{"name": "a"}]}, "base_url": "https://x"},
    {"common_betas_for_base_url": {"__raise__": "RuntimeError", "msg": "betas failed"}})


# ── verify seeding (pure scripted drive via the oracle) ─────────────────
def verify():
    bad = 0
    counts = {}
    for i, rec in enumerate(CASES):
        counts[rec["func"]] = counts.get(rec["func"], 0) + 1
        out = oracle.run_case(rec)
        spec = out["ret"]
        if isinstance(spec, dict) and spec.get("__raise__") in ("NameError", "AttributeError"):
            bad += 1
            msg = spec.get("msg", "")
            if any(("'" + nm + "'") in msg for nm in oracle.PORTED) or "NameError" in str(spec):
                print(f"VERIFY case {i} {rec['func']}: seeding gap: {spec}")
    for func in oracle.PORTED:
        if counts.get(func, 0) < 5:
            bad += 1
            print(f"VERIFY: {func} has only {counts.get(func, 0)} cases (<5)")
    print(f"VERIFY: {len(CASES)} cases across {len(counts)} funcs, {bad} suspicious")
    return bad


def main():
    if verify():
        sys.exit(1)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8", newline="\n") as f:
        for rec in CASES:
            f.write(json.dumps(rec, ensure_ascii=False, separators=(",", ":")))
            f.write("\n")
    print(f"WROTE {OUT} ({len(CASES)} cases)")


if __name__ == "__main__":
    main()
