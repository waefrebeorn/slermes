#!/usr/bin/env python3
"""Differential oracle for agent/api_request_hooks.py (gate w9_apirequest_302923).

Imports the LIVE module via importlib with a parent-package bootstrap for the
one import-time dependency (agent.usage_pricing). Compares each harness "out"
to the live helper result computed from the echoed input keys.
"""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _mod(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return m

agent_pkg = _mod("agent")
up = _mod("agent.usage_pricing")
up.normalize_usage = lambda *a, **k: None
setattr(agent_pkg, "usage_pricing", up)

SRC = QUARRY + "/agent/api_request_hooks.py"
spec = importlib.util.spec_from_file_location("w9_apirequest_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w9_apirequest_live"] = live
spec.loader.exec_module(live)
M = live.ApiRequestHooksMixin

_MISSING = object()

def compute(r):
    func = r.get("func")
    if func == "hook_payload_max_chars":
        key = "HERMES_PLUGIN_PAYLOAD_MAX_CHARS"
        env = r.get("env_value")
        old = os.environ.get(key, _MISSING)
        try:
            if env is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = env
            return M._hook_payload_max_chars()
        finally:
            if old is _MISSING:
                os.environ.pop(key, None)
            else:
                os.environ[key] = old
    if func == "is_sensitive_hook_key":
        return M._is_sensitive_hook_key(r.get("key"))
    if func == "hook_jsonable":
        return M._hook_jsonable(
            r.get("value"),
            depth=r.get("depth", 0),
            max_depth=r.get("max_depth", 8),
            max_string=r.get("max_string", 8000),
            max_sequence=r.get("max_sequence", 200),
        )
    if func in ("sanitize_hook_payload", "request_payload"):
        limit = r.get("limit", 50000)
        orig = M._hook_payload_max_chars
        M._hook_payload_max_chars = staticmethod(lambda: limit)
        try:
            if func == "sanitize_hook_payload":
                return M._sanitize_hook_payload(r.get("value"))
            class _Probe(M):
                pass
            return _Probe()._api_request_payload_for_hook(r.get("api_kwargs"))
        finally:
            M._hook_payload_max_chars = orig
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
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')}: python raised {exc!r}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w9_apirequest_302923: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w9_apirequest_302923: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
