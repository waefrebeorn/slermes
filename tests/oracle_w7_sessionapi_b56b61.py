#!/usr/bin/env python3
"""Differential oracle for gateway/session_api.py pure helpers (w7_sessionapi_b56b61)."""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn); pm.__path__ = [os.path.join(Q, *parts[:i])]; sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items(): setattr(m, k, v)
    sys.modules[name] = m
    return m


stub("persistence.core.errors", AuthorityRefusal=type("A", (Exception,), {}),
     StoreUnavailable=type("S", (Exception,), {}))
stub("gateway.session", SessionEntry=object, SessionSource=object, _is_path_unsafe=lambda s: False)
stub("gateway.session_contract", SessionRef=object)
stub("gateway.session_persistence", authority_persistence=lambda a: a)
stub("gateway.config", Platform=types.SimpleNamespace(LOCAL="local", API_SERVER="api_server"))

LIVE_NAME = "w7_sessionapi_b56b61_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/gateway/session_api.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "api_generation_id":
        return live.api_generation_id(r.get("session_id", ""), int(r.get("generation", 0)))
    if f == "room_session_id":
        return live.room_session_id(r.get("identity") or [], int(r.get("generation", 0)))
    if f == "api_storage_source":
        b = r.get("store", {}).get("binding")
        store = types.SimpleNamespace(
            bindings=types.SimpleNamespace(api_binding=lambda sid: (
                types.SimpleNamespace(storage_source=b) if isinstance(b, str) else None)))
        return live.api_storage_source(store, r.get("session_id", ""), r.get("fallback", ""))
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
    try:
        py = compute(r)
    except Exception as exc:
        print("LIVE ERROR %r: %r" % (r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH sessionapi %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w7_sessionapi_b56b61: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
