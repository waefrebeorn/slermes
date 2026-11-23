#!/usr/bin/env python3
"""Differential oracle for upstream Hermes cron/scheduler_worker_env (noc_w11_schedulerw_1873fb.h)."""
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


live = _load("cron/scheduler_worker_env")

class FakePath(str):
    def resolve(self):
        return self

SEP = ";"


def compute(r):
    env = dict(r.get("env") or {})
    pure = r.get("purelib")
    dep = r.get("dep")
    old = (live._installed_purelib, live._committed_dependency_site_packages, live.Path, os.pathsep)
    try:
        # Explicit seams: inject sysconfig purelib and committed site-packages.
        live._installed_purelib = lambda: (FakePath(pure) if pure is not None else None)
        live._committed_dependency_site_packages = lambda p: (FakePath(dep) if dep is not None else None)
        live.Path = FakePath          # Path(root).resolve() becomes the raw string
        os.pathsep = SEP              # Windows CRT keeps ';'
        live.pin_hermes_tree_on_pythonpath(env, r.get("root") or "")
    finally:
        (live._installed_purelib, live._committed_dependency_site_packages,
         live.Path, os.pathsep) = old
    return dict(env)

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
        print(f"ORACLE w11_schedulerw_1873fb: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w11_schedulerw_1873fb {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w11_schedulerw_1873fb: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
