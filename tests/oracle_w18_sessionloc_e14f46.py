#!/usr/bin/env python3
"""Differential oracle for gateway/session_local_recovery.py (gate w18_sessionloc_e14f46).

The module's many top-level imports are stubbed so the pure
``_merge_live_in_stored_order`` can be called against the live source.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class _Refusal(Exception):
    pass


gateway = _mod("gateway")
gateway.__path__ = []
_mod("gateway.config", Platform=object)
_mod("gateway.session", SessionEntry=object, SessionSource=object)
_mod("gateway.session_contract", SessionRef=object)
_mod("gateway.session_persistence", authority_persistence=lambda a: None)
persistence = _mod("persistence")
persistence.__path__ = []
core = _mod("persistence.core")
core.__path__ = []
_mod("persistence.core.errors", AuthorityRefusal=_Refusal)
pa = _mod("persistence.authority")
pa.__path__ = []
_mod("persistence.authority.bindings", local_session_id=lambda *a: "")

SRC = Q + "/gateway/session_local_recovery.py"
spec = importlib.util.spec_from_file_location("w18_sessionloc_e14f46_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w18_sessionloc_e14f46_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "merge":
        return live._merge_live_in_stored_order(r.get("display", []), r.get("live", []))
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
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_sessionloc_e14f46: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
