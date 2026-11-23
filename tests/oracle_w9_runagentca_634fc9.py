#!/usr/bin/env python3
"""Differential oracle for gateway/run_agent_cache.py (gate w9_runagentca_634fc9)."""
import importlib.util
import json
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
import gateway  # parent-package bootstrap

SRC = QUARRY + "/gateway/run_agent_cache.py"
NAME = "run_agent_cache_live_w9_runagentca"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func in ("first", "tuple"):
        entry = tuple(r["entry"]) if r.get("as_tuple") else r.get("entry")
        fn = live._first_agent if func == "first" else live._tuple_agent
        py = fn(entry)
        return list(py) if isinstance(py, tuple) else py
    if func == "config_sig":
        return live.GatewayAgentCacheMixin._agent_config_signature(
            model=r.get("model", ""),
            runtime=r.get("runtime") or {},
            enabled_toolsets=r.get("enabled_toolsets") or [],
            ephemeral_prompt=r.get("ephemeral_prompt") or "",
            cache_keys=r.get("cache_keys") or {},
            user_id=r.get("user_id"),
            user_id_alt=r.get("user_id_alt"),
            skip_context_files=bool(r.get("skip_context_files", False)),
        )
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
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w9_runagentca_634fc9: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
