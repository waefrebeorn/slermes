#!/usr/bin/env python3
import os
"""
sta_oracle_usage_pricing.py — oracle for t_port_usage_pricing.c.

Recomputes each case from the LIVE agent/usage_pricing.py and emits one JSON
line per case (same shape as the C harness). The runner (run_oracle.sh) diffs
the two streams byte-for-byte.

The LIVE package is imported so the oracle tracks the canonical Python exactly.
"""
import json
import sys
import importlib.util

# Locate the LIVE agent/usage_pricing.py (installed package, not the dev tree
# which may have drifted). Walk sys.path for the hermes package.
def _load_usage_pricing():
    # Deterministic LIVE-Python resolution: prefer the canonical dev repo
    # (parent of slermes/) over any installed/stale copy on sys.path
    # (e.g. ~/.hermes/hermes-agent), which would manufacture false FAPs.
    _repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    if _repo not in sys.path:
        sys.path.insert(0, _repo)
    for base in sys.path:
        cand = f"{base}/agent/usage_pricing.py"
        try:
            spec = importlib.util.spec_from_file_location("live_usage_pricing", cand)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
        except Exception:
            continue
    # Fallback: importlib of the installed module name
    import agent.usage_pricing as mod  # type: ignore
    return mod


up = _load_usage_pricing()

import types as _types

def _as_obj(d):
    """Wrap a dict into a namespace-like object so the Python port's
    getattr(response_usage, "input_tokens", 0) style field access works
    (JSON fixtures are dicts; the Python API expects attribute access)."""
    if d is None:
        return None
    if not isinstance(d, dict):
        return d
    ns = _types.SimpleNamespace()
    for k, v in d.items():
        if isinstance(v, dict):
            setattr(ns, k, _as_obj(v))
        else:
            setattr(ns, k, v)
    return ns


def emit_normalize_usage(c):
    provider = c.get("provider")
    api_mode = c.get("api_mode")
    usage = _as_obj(c.get("usage") or {})
    cu = up.normalize_usage(usage, provider=provider, api_mode=api_mode)
    return {
        "fn": "normalize_usage",
        "input_tokens": cu.input_tokens,
        "output_tokens": cu.output_tokens,
        "cache_read_tokens": cu.cache_read_tokens,
        "cache_write_tokens": cu.cache_write_tokens,
        "reasoning_tokens": cu.reasoning_tokens,
    }


def emit_resolve_billing_route(c):
    model = c.get("model", "")
    provider = c.get("provider")
    base_url = c.get("base_url")
    r = up.resolve_billing_route(model, provider=provider, base_url=base_url)
    return {
        "fn": "resolve_billing_route",
        "provider": r.provider,
        "model": r.model,
        "base_url": r.base_url,
        "billing_mode": r.billing_mode,
    }


def emit_format_token_count(c):
    value = int(c.get("value", 0))
    return {
        "fn": "format_token_count",
        "value": value,
        "out": up.format_token_count_compact(value),
    }


def emit_bedrock_norm(c):
    model = c.get("model", "")
    return {
        "fn": "bedrock_norm",
        "in": model,
        "out": up._normalize_bedrock_model_name(model),
    }


def emit_anthropic_norm(c):
    model = c.get("model", "")
    return {
        "fn": "anthropic_norm",
        "in": model,
        "out": up._normalize_anthropic_model_name(model),
    }


DISPATCH = {
    "normalize_usage": emit_normalize_usage,
    "resolve_billing_route": emit_resolve_billing_route,
    "format_token_count": emit_format_token_count,
    "bedrock_norm": emit_bedrock_norm,
    "anthropic_norm": emit_anthropic_norm,
}


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_usage_pricing.py <cases.json>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op")
        fn = DISPATCH.get(op)
        if fn is None:
            out = {"fn": op if op else "UNKNOWN"}
        else:
            out = fn(c)
        # ensure_ascii=False → raw UTF-8, matching the C harness byte-for-byte.
        sys.stdout.write(json.dumps(out, ensure_ascii=False, separators=(",", ":")) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
