#!/usr/bin/env python3
"""Differential oracle for pm/build_operations.py (gate w7_buildoperat_e2e0da).

Heavy collaborators are registered as scripted stubs; filesystem facts come
from real scratch directories built from the echoed fixture keys. Both sides
return {"ok":true} or {"error":...,"message":...} for each validation path.
"""
import contextlib
import importlib.util
import json
import os
import shutil
import sys
import types
from pathlib import Path

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

_pm = types.ModuleType("pm")
_pm.__path__ = [QUARRY + "/pm"]
sys.modules.setdefault("pm", _pm)


def mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class InstallError(Exception):
    def __init__(self, *args):
        super().__init__(*args)
        self._parts = args

    def __str__(self):
        return " ".join(str(a) for a in self._parts)


class StatePackage:
    pass


class ToolPkg:
    name = "tool"

    def store_entry(self, version, target):
        return "entry"

    def binary(self, entry, target):
        return None

    def env(self, entry, target):
        return {}


class Facts:
    def __init__(self, *a, **k):
        pass

    def get(self, name):
        return {"entry": "entry"}

    def installed(self, *a):
        return True

    def env_for(self, *a):
        return {}


class Store:
    def __init__(self, root):
        self.root = root

    def install_lock(self):
        return contextlib.nullcontext()


class InstalledPackage:
    def __init__(self, *a):
        pass


class Lockfile:
    def version(self, name):
        return "1.0"


class FakeEnv:
    executable = "/fake/python"

    def check_lock(self, source):
        pass

    def export_requirements(self, source, out, extras=()):
        pass

    def install_requirements(self, requirements, wheelhouse=None, lock=None):
        pass

    def prune_cache(self, ci=False):
        pass


holder = {"state": False}

mod("pm.filesystem", long_root=lambda p: p, native=lambda p: str(p))
mod("pm.install", InstalledPackage=InstalledPackage,
    _install=lambda *a, **k: None, _lockfile=lambda: Lockfile(),
    _identity=lambda *a, **k: "identity", _entry_verified=lambda *a, **k: True)
mod("pm.lock", Lockfile=Lockfile, Facts=Facts)
mod("pm.package", InstallError=InstallError, StatePackage=StatePackage,
    compose_env=lambda envs, base: base)
mod("pm.environment", managed_environment=lambda *a, **k: FakeEnv(),
    _fresh_build=lambda *a, **k: contextlib.nullcontext())


def _require_install_allowed(explicit):
    if not explicit:
        raise RuntimeError("install not allowed")


def _requirements(requirements):
    return list(requirements) if not isinstance(requirements, str) else [requirements]


mod("pm.operations", _require_install_allowed=_require_install_allowed,
    _requirements=_requirements)


def walk(names):
    return [StatePackage()] if holder["state"] else [ToolPkg()]


mod("pm.registry", walk=walk)
mod("pm.store", Store=Store, digest_matches=lambda *a: True)

spec = importlib.util.spec_from_file_location(
    "w7_buildoperat_live", QUARRY + "/pm/build_operations.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w7_buildoperat_live"] = live
spec.loader.exec_module(live)


def outcome(fn):
    try:
        fn()
    except Exception as exc:
        cls = type(exc).__name__
        msg = str(exc)
        if cls == "InstallError":
            for prefix in ("venv project manifest is missing", "tools source facts missing"):
                if msg.startswith(prefix):
                    msg = prefix
                    break
        return {"error": cls, "message": msg}
    return {"ok": True}


def compute(r):
    func = r.get("func")
    holder["state"] = bool(r.get("state_present", False))
    case = r.get("case") or "x"
    scratch = Path(".testbin") / ("w7_bop_" + case)
    shutil.rmtree(scratch, ignore_errors=True)
    scratch.mkdir(parents=True, exist_ok=True)
    if r.get("facts_present", False) or func in ("stage_tools", "verified_tools"):
        (scratch / "facts.json").write_text("{}", encoding="utf-8")
    if func == "prepare_tools":
        names = r.get("names") if isinstance(r.get("names"), str) else list(r.get("names") or [])
        return outcome(lambda: live.prepare_tools(
            names, out=scratch / "out", target="linux-amd64"))
    if func == "verified_tools":
        if r.get("facts_present", False):
            names = r.get("names") if isinstance(r.get("names"), str) else list(r.get("names") or [])
            return outcome(lambda: live.verified_tools(
                names, source_store=scratch, target="linux-amd64"))
        return outcome(lambda: live.verified_tools(
            r.get("names") if isinstance(r.get("names"), str) else list(r.get("names") or []),
            source_store=scratch / "missing-root", target="linux-amd64"))
    if func == "stage_tools":
        names = r.get("names") if isinstance(r.get("names"), str) else list(r.get("names") or [])
        source = scratch / "source"
        destination = scratch / "destination"
        if r.get("overlap", False):
            source, destination = scratch / "same", scratch / "same"
        if r.get("facts_present", False):
            source.mkdir(parents=True, exist_ok=True)
            (source / "facts.json").write_text("{}", encoding="utf-8")
        return outcome(lambda: live.stage_tools(
            names, source_store=source, out=destination, target="linux-amd64"))
    if func == "check_project_lock":
        if r.get("manifest_present", False):
            (scratch / "pyproject.toml").write_text("[project]\n", encoding="utf-8")
        source = scratch if r.get("manifest_present", False) else scratch / "no-manifest"
        return outcome(lambda: live.check_project_lock(
            source, offline=bool(r.get("offline", False)), explicit=bool(r.get("explicit", False))))
    if func == "build_env":
        return outcome(lambda: live.build_requirements_environment(
            [], out=scratch / "new-venv", explicit=bool(r.get("explicit", False))))
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
        print("ORACLE w7_buildoperat_e2e0da: harness-side error %r case %r" % (exc, r.get("case")),
              file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH w7_buildoperat_e2e0da case=%s func=%s py=%r c=%r" %
              (r.get("case"), r.get("func"), py, got), file=sys.stderr)

print("ORACLE w7_buildoperat_e2e0da: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
