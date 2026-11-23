#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/session_local_mcp (noc_w12_sessionloc_cc8104.h)."""
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


# session_local_mcp imports authority_persistence at module scope; stub the
# heavy gateway.session_persistence chain (pulls in an uninstalled ruamel).
_stub("gateway.session_persistence", authority_persistence=lambda *a, **k: None)


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


live = _load("gateway/session_local_mcp")


def compute(r):
    func = r["func"]
    if func == "session_scope":
        authority = types.SimpleNamespace(profile_id=r.get("profile_id", ""))
        return live.session_scope(authority, r.get("sid", ""))
    if func == "frozen_toolset":
        return bool(live._frozen_toolset(r.get("data"), r.get("toolset", "")))
    raise ValueError("unknown func " + str(func))


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
        print(f"ORACLE w12_sessionloc_cc8104: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w12_sessionloc_cc8104 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w12_sessionloc_cc8104: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
