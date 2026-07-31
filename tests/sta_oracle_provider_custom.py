#!/usr/bin/env python3
import os
"""
sta_oracle_provider_custom.py — oracle for t_port_provider_custom.c.

Recomputes each case from the LIVE agent/agent_init.py
(_custom_provider_extra_body_for_agent / _merge_custom_provider_extra_body /
_custom_provider_model_matches) and emits one JSON line per case (same shape
as the C harness). The runner diffs them byte-for-byte.
"""
import json
import sys
import importlib.util


def _load():
    # Deterministic LIVE-Python resolution: prefer the canonical dev repo
    # (parent of slermes/) over any installed/stale copy on sys.path
    # (e.g. ~/.hermes/hermes-agent), which would manufacture false FAPs.
    _repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    if _repo not in sys.path:
        sys.path.insert(0, _repo)
    for base in sys.path:
        cand = f"{base}/agent/agent_init.py"
        try:
            spec = importlib.util.spec_from_file_location("live_init", cand)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
        except Exception:
            continue
    import agent.agent_init as mod  # type: ignore
    return mod


ai = _load()


def _resolve(provider, model, base_url, custom_providers, existing):
    eb = ai._custom_provider_extra_body_for_agent(
        provider=provider, model=model, base_url=base_url,
        custom_providers=custom_providers or [])
    if not eb:
        result = dict(existing) if existing else None
    else:
        result = dict(eb)
        if isinstance(existing, dict):
            result.update(existing)
    return eb, result


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_provider_custom.py <cases.json>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        cases = json.load(f)
    for i, c in enumerate(cases):
        provider = c.get("provider", "")
        model = c.get("model", "")
        base_url = c.get("base_url", "")
        cps = c.get("custom_providers", [])
        existing = c.get("existing_extra_body")
        eb, merged = _resolve(provider, model, base_url, cps, existing)
        rec = {
            "case": i,
            "extra_body": eb,
            "merged": merged,
        }
        sys.stdout.write(json.dumps(rec, ensure_ascii=False, separators=(",", ":")) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
