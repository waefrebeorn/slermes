#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/session_readiness (noc_w12_sessionrea_defea4.h)."""
import asyncio
import contextlib
import importlib.util
import json
import os
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

from persistence.core.errors import AuthorityRefusal


def _stub(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class FakeHTTPException(Exception):
    pass


_stub("fastapi", HTTPException=FakeHTTPException)

RES = {"dir": None, "raise": False}


class _Dir:
    def __init__(self, p):
        self.p = p

    def resolve(self):
        return Path(self.p)


def _resolve_profile_dir(_name):
    if RES["raise"]:
        raise FakeHTTPException()
    return _Dir(RES["dir"] or "")


_stub("hermes_cli.web_server_profiles", _resolve_profile_dir=_resolve_profile_dir)
_stub("gateway.run", _profile_runtime_scope=lambda *a, **k: contextlib.nullcontext())
_stub("hermes_cli.main", _has_any_provider_configured=lambda **k: True)
_rr = types.ModuleType("hermes_cli.runtime_readiness")
_rr.check_runtime_readiness = lambda provider, strict_profile_scope: {"ok": True, "provider": provider}
_rr.readiness_failure = lambda exc: {"ok": False, "error": str(exc)}
sys.modules["hermes_cli.runtime_readiness"] = _rr


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


live = _load("gateway/session_readiness")


def compute(r):
    RES["dir"] = r.get("resolved_profile_dir")
    RES["raise"] = bool(r.get("resolve_raises", False))
    actor = types.SimpleNamespace(capabilities=set(r.get("caps", [])), profile_id=r["actor_profile_id"])
    authority = types.SimpleNamespace(profile_id=r["authority_profile_id"])
    try:
        asyncio.run(live.readiness(authority, actor, r["params"], runtime=bool(r.get("runtime", False))))
        return None
    except AuthorityRefusal as exc:
        return exc.reason


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
        print(f"ORACLE w12_sessionrea_defea4: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w12_sessionrea_defea4 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w12_sessionrea_defea4: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
