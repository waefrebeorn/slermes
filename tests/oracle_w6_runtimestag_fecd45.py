#!/usr/bin/env python3
"""Differential oracle for pm/runtime_stage.py (gate w6_runtimestag_fecd45).

stage_runtime is entirely effectful.  Every collaborator (PythonEnvironment,
uv_cache_dir, runtime_environment, TemporaryDirectory, shutil.copyfile,
subprocess.run, Path.mkdir) is replaced with a recorder, then the LIVE function
runs and its deterministic decision trace is compared to the C port.  cwd and
all paths are injected from the fixture; no process environment is mutated.
"""
import importlib.util, json, os, sys, types
from pathlib import Path
from unittest import mock

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

PKG = "w6_runtimestag_live_pkg"
pkg = types.ModuleType(PKG)
pkg.__path__ = [Q + "/pm"]
sys.modules[PKG] = pkg
spec = importlib.util.spec_from_file_location(PKG + ".runtime_stage", Q + "/pm/runtime_stage.py")
live = importlib.util.module_from_spec(spec)
sys.modules[PKG + ".runtime_stage"] = live
spec.loader.exec_module(live)

import pm.environment, pm.packages, pm.runtime, shutil, subprocess, tempfile
from pm.package import InstallError


def np(s):
    return s.replace("\\", "/") if isinstance(s, str) else s


def compute(r):
    cwd = r.get("cwd", "")
    project = r.get("project", "")
    destination = r.get("destination", "")
    wheelhouse = r.get("wheelhouse")
    cache = r.get("cache")
    offline = bool(r.get("offline", False))
    uv_def = r.get("uv_cache_default", "")
    executable = r.get("executable", "")
    rc = int(r.get("check_rc", 0))
    stderr = r.get("check_stderr", "")

    tr = {"copies": [], "created": False, "sync": None, "argv": None}

    class FakeEnv:
        def __init__(self, **kw):
            self.executable = executable
            self.kw = kw
        def create(self):
            tr["created"] = True
        def sync(self, *a, **kw):
            tr["sync"] = "locked"
        def install_wheelhouse(self, *a, **kw):
            tr["sync"] = "wheelhouse"

    class FakeTD:
        def __init__(self, **kw):
            pass
        def __enter__(self):
            return "C:\\snapshot"
        def __exit__(self, *a):
            return False

    def fake_copy(src, dst):
        tr["copies"].append(Path(src).name)

    def fake_run(argv, **kw):
        tr["argv"] = list(argv)
        return types.SimpleNamespace(returncode=rc, stderr=stderr, stdout="")

    real_getcwd = os.getcwd
    os.getcwd = lambda: cwd
    err = None
    dest_abs = cache_abs = None
    try:
        with mock.patch.object(pm.environment, "PythonEnvironment", FakeEnv), \
             mock.patch.object(pm.packages, "uv_cache_dir", lambda: Path(uv_def)), \
             mock.patch.object(pm.runtime, "runtime_environment", lambda: {"fake": True}), \
             mock.patch.object(tempfile, "TemporaryDirectory", FakeTD), \
             mock.patch.object(shutil, "copyfile", fake_copy), \
             mock.patch.object(subprocess, "run", fake_run), \
             mock.patch.object(Path, "mkdir", lambda self, *a, **k: None):
            dest_abs = np(str(Path(destination).absolute()))
            cache_abs = np(str(Path(cache).absolute())) if cache is not None else np(str(Path(uv_def)))
            kwargs = dict(offline=offline)
            if wheelhouse is not None:
                kwargs["wheelhouse"] = Path(wheelhouse)
            if cache is not None:
                kwargs["cache"] = Path(cache)
            try:
                live.stage_runtime(Path("C:/uv"), Path("C:/python"), Path(destination),
                                   project=Path(project), **kwargs)
            except InstallError as exc:
                err = list(exc.args)
    finally:
        os.getcwd = real_getcwd

    return {
        "project": np(str(Path(project))),
        "destination": dest_abs,
        "cache": cache_abs,
        "cache_source": "default" if cache is None else "explicit",
        "offline": bool(offline or wheelhouse is not None),
        "copy_files": tr["copies"],
        "created": tr["created"],
        "sync_mode": tr["sync"],
        "check_argv": [np(x) for x in (tr["argv"] or [])],
        "error": err,
        "result": executable,
    }


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
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_runtimestag_fecd45: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
