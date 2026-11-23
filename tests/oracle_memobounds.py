#!/usr/bin/env python3
"""Oracle for agent/model_metadata_memo_bounds.py (gate memobounds).

Both live functions mutate module-level memo dicts on the `agent.model_metadata`
facade. The oracle seeds those dicts from the fixture, stubs time.time() for the
remember path, drives the REAL drop_expired/evict_oldest imported from the
quarry's agent.cache_bounds, and compares the post-call state to the C seam.
"""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = [QUARRY + "/agent"]
sys.modules.setdefault("agent", agent_pkg)

# Real cache_bounds helpers, loaded before the live module's `from ... import`.
_load("agent.cache_bounds", QUARRY + "/agent/cache_bounds.py")

mm = types.ModuleType("agent.model_metadata")
sys.modules["agent.model_metadata"] = mm

live = _load("agent.model_metadata_memo_bounds",
             QUARRY + "/agent/model_metadata_memo_bounds.py")

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    if func == "remember":
        mm._endpoint_model_metadata_cache = dict(r.get("values") or {})
        mm._endpoint_model_metadata_cache_time = dict(r.get("stamps") or {})
        mm._ENDPOINT_MODEL_CACHE_TTL = r.get("ttl", 0)
        now = r.get("now", 0)
        live.time = types.SimpleNamespace(time=lambda: now)
        live.remember_endpoint_models(r.get("memo_key", ""), r.get("cache_value") or {})
        py = mm._endpoint_model_metadata_cache
    elif func == "bound":
        mm._codex_oauth_context_cache = dict(r.get("context") or {})
        mm._codex_oauth_max_context_cache = dict(r.get("max_context") or {})
        live.CODEX_CACHE_MAX = r.get("limit", 128)
        live.bound_codex_caches(r.get("cache_key", ""))
        py = {"context": mm._codex_oauth_context_cache,
              "max_context": mm._codex_oauth_max_context_cache}
    else:
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE memobounds: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
