#!/usr/bin/env python3
import os
"""
sta_oracle_tool_guardrails.py — oracle for t_port_tool_guardrails.c.

Recomputes each case from the LIVE agent/tool_guardrails.py:_result_hash and
emits one JSON line per case (same shape as the C harness). The runner diffs
them byte-for-byte. The hash is deterministic, so this verifies the C
canonical-JSON (sorted keys) + SHA256 path exactly.
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
        cand = f"{base}/agent/tool_guardrails.py"
        try:
            spec = importlib.util.spec_from_file_location("live_tg", cand)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
        except Exception:
            continue
    import agent.tool_guardrails as mod  # type: ignore
    return mod


tg = _load()


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_tool_guardrails.py <values.json>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        values = json.load(f)
    for value in values:
        out = tg._result_hash(value)
        rec = {"value": value if isinstance(value, str) else "", "out": out}
        sys.stdout.write(json.dumps(rec, ensure_ascii=False, separators=(",", ":")) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
