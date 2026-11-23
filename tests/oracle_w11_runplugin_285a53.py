#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/run_plugin_rewire (noc_w11_runplugin_285a53.h)."""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)


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


import asyncio
import contextlib

if "gateway.run" not in sys.modules:
    gr = types.ModuleType("gateway.run")
    gr._profile_runtime_scope = lambda *a, **k: contextlib.nullcontext()
    sys.modules["gateway.run"] = gr
live = _load("gateway/run_plugin_rewire")


class A:
    def __init__(self, fail):
        self.fail = fail

    def rewire_plugin_handlers(self):
        if self.fail:
            raise RuntimeError("fail")


def _runner(adapters, profile_adapters):
    return types.SimpleNamespace(adapters=adapters, _profile_adapters=profile_adapters)


def compute(r):
    adapters = r.get("adapters")
    pad = r.get("profile_adapters")
    pn = r.get("profile_name")
    if r["func"] == "count":
        return asyncio.run(live._count_adapters(_runner(adapters, pad), pn))
    fail = set(r.get("fail") or [])
    inst = object.__new__(live.GatewayPluginRewireMixin)
    if pn is None:
        selected = dict(adapters or {})
    else:
        selected = dict((pad or {}).get(pn) or {})
    objs = {k: A(k in fail) for k in selected}
    inst.adapters = objs
    inst._profile_adapters = {pn: objs} if pn else {}
    return inst._rewire_plugin_handlers(pn)

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
        print(f"ORACLE w11_runplugin_285a53: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w11_runplugin_285a53 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w11_runplugin_285a53: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
