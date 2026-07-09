#!/usr/bin/env python3
"""Faithfulness oracle for port_agent_error_classifier.c upstream-provider fns.

Reads JSON lines emitted by t_port_error_classifier_upstream.c and recomputes
the SAME functions from the LIVE agent/error_classifier.py:
  _is_openrouter_upstream_error(body, provider)
  _extract_upstream_provider_name(body)
"""
import sys, os, json
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from agent.error_classifier import (
    _is_openrouter_upstream_error,
    _extract_upstream_provider_name,
)

mism = 0
n = 0
for line in sys.stdin:
    line = line.strip()
    if not line or not line.startswith("{"):
        continue
    rec = json.loads(line)
    fn = rec["fn"]
    n += 1
    try:
        if fn == "is_upstream":
            body = json.loads(rec["body"]) if isinstance(rec["body"], str) else rec["body"]
            prov = rec["provider"]
            exp = 1 if _is_openrouter_upstream_error(body, prov) else 0
            got = rec["out"]
        elif fn == "provider_name":
            body = json.loads(rec["body"]) if isinstance(rec["body"], str) else rec["body"]
            pn = _extract_upstream_provider_name(body)
            exp = pn if pn is not None else ""
            got = rec["out"]
        else:
            print("UNKNOWN FN", fn); continue
    except Exception as e:
        print("ORACLE ERROR", fn, repr(rec), e)
        mism += 1
        continue
    if exp != got:
        mism += 1
        print(f"MISMATCH fn={fn} rec={rec} PY={exp!r} C={got!r}")
print(f"ERROR_CLASSIFIER oracle: {n} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
