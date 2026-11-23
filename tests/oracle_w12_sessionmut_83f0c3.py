#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/session_mutations (noc_w12_sessionmut_83f0c3.h)."""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)


def _stub(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# session_mutations pulls the persistence chain at import; stub the two gateway
# helpers it binds at module scope.
_stub("gateway.session_persistence", authority_persistence=lambda *a, **k: None, owner_rule=lambda: None)
_stub("gateway.session_messaging_picks", CHAT_PICKS=frozenset(), is_chat_pick=lambda *a, **k: False)


def _load(modpath):
    parts = modpath.split("/")
    for i in range(1, len(parts)):
        pname = ".".join(parts[:i])
        if pname in sys.modules:
            continue
        pdir = os.path.join(QUARRY, *parts[:i])
        init = os.path.join(pdir, "__init__.py")
        sp = importlib.util.spec_from_file_location(pname, init, submodule_search_locations=[pdir])
        pm = importlib.util.module_from_spec(sp)
        sys.modules[pname] = pm
        try:
            sp.loader.exec_module(pm)
        except Exception:
            pass
    name = ".".join(parts)
    sp = importlib.util.spec_from_file_location(name + "_live", os.path.join(QUARRY, modpath + ".py"))
    live = importlib.util.module_from_spec(sp)
    sys.modules[name + "_live"] = live
    sp.loader.exec_module(live)
    return live


live = _load("gateway/session_mutations")


def compute(r):
    authority_id = r.get("authority_id", "")
    registry = r.get("registry")
    authority = types.SimpleNamespace()
    runner = types.SimpleNamespace()
    authority.runner = runner
    other = types.SimpleNamespace()
    if isinstance(registry, list):
        runner.session_authorities = [authority if x == authority_id else other for x in registry]
    sa = r.get("session_authority")
    if sa is not None:
        runner.session_authority = authority if sa == authority_id else other
    return bool(live._runtime_owns_authority(authority))


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
        print(f"ORACLE w12_sessionmut_83f0c3: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w12_sessionmut_83f0c3 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w12_sessionmut_83f0c3: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
