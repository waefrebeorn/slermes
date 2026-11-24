#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/session_local_adoption.py (gate w18_sessionloc_866cb3).

Top-level import ``persistence.core.errors`` and the lazy
``persistence.authority.bindings.SessionBinding`` are stubbed: SessionBinding is
a pure kwargs recorder, so the live ``_binding`` exposes exactly the field
mapping/defaulting the C port implements.
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


class AuthorityRefusal(Exception):
    def __init__(self, reason, field=None, detail=None, **kw):
        self.reason = reason
        self.field = field
        self.detail = detail
        super().__init__(reason)


persistence = _mod("persistence")
persistence.__path__ = []
core = _mod("persistence.core")
core.__path__ = []
_mod("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)
authority = _mod("persistence.authority")
authority.__path__ = []
_mod("persistence.authority.bindings",
     SessionBinding=lambda **kwargs: dict(kwargs))

SRC = Q + "/gateway/session_local_adoption.py"
spec = importlib.util.spec_from_file_location("w18_sessionloc_866cb3_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w18_sessionloc_866cb3_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "binding":
        b = dict(live._binding(r.get("receipt", {})))
        # Python stores lineage as a tuple; C emits a JSON array (list).
        b["lineage"] = list(b["lineage"])
        return b
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
print(f"ORACLE w18_sessionloc_866cb3: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
