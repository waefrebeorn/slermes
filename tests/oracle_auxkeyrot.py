#!/usr/bin/env python3
"""Oracle for agent/auxiliary_key_rotation.py (gate auxkeyrot).

The live function is driven with a seeded fake ContextVar and the REAL
_normalize_api_key from the quarry. The C side receives the same
old/new/current/runtime facts and its resulting api_key is compared.
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


# Bootstrap the parent package so `agent.*` imports resolve from the quarry.
agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = [QUARRY + "/agent"]
sys.modules.setdefault("agent", agent_pkg)

# Real normalizer from the quarry (the live fn imports it via auxiliary_client).
norm = _load("agent.auxiliary_custom_credentials",
             QUARRY + "/agent/auxiliary_custom_credentials.py")

fake_ac = types.ModuleType("agent.auxiliary_client")


class _Ctx:
    def __init__(self, value):
        self.value = value

    def get(self):
        return self.value


sys.modules["agent.auxiliary_client"] = fake_ac

live = _load("agent.auxiliary_key_rotation",
             QUARRY + "/agent/auxiliary_key_rotation.py")

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "rotate":
        continue
    old = r.get("old")
    new = r.get("new")
    current = r.get("current")
    is_dict = bool(r.get("runtime_is_dict", True))

    seed = {"api_key": current} if is_dict else None
    fake_ac._RUNTIME_MAIN_CONTEXT = _Ctx(seed)
    fake_ac._normalize_api_key = norm._normalize_api_key
    live.rotate_runtime_main_api_key(old, new)

    py = seed.get("api_key") if isinstance(seed, dict) else None
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE auxkeyrot: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
