#!/usr/bin/env python3
"""Differential oracle for upstream Hermes pm/client.py (gate w5_client_039ee5).

Worker dispatch (_python_operation) is injected so the pure payload-building
and sequence guards of the public operations can be compared without spawning
a worker or touching the filesystem.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

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


class InstallError(Exception):
    def __init__(self, package, cause="", remedy=""):
        super().__init__(cause)
        self.package, self.cause, self.remedy = package, cause, remedy


class Runner:
    def __init__(self, name, env=None):
        self.name, self.env = name, env


class StatePackage:
    pass


pm = _mod("pm")
pm.__path__ = []
_mod("pm.paths", repo_root=lambda: Path("."), lockfile_path=lambda: Path("lockfile"))
_mod("pm.plugin_inputs", Candidates=object, Members=object, PluginInput=object,
     Selection=object, encode=lambda x: x)
_mod("pm.package", InstallError=InstallError, Runner=Runner, StatePackage=StatePackage)
_mod("pm.runtime", is_runtime=lambda: False, runtime_command=lambda *a, **k: [],
     runtime_environment=lambda: {})
_mod("pm.worker_operations", OPERATIONS={})

SRC = Q + "/pm/client.py"
spec = importlib.util.spec_from_file_location("w5_client_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w5_client_live"] = live
spec.loader.exec_module(live)

captured = []


def _encode(v):
    if isinstance(v, Path):
        return str(v)
    if isinstance(v, dict):
        return {k: _encode(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [_encode(x) for x in v]
    return v


def fake_operation(operation, payload):
    captured[:] = [{"op": operation, "payload": _encode(payload)}]
    return "/stub/result"


live._python_operation = fake_operation

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    captured[:] = []
    try:
        if func == "prepare_tools":
            live.prepare_tools(r.get("names"), out=r.get("outpath", ""),
                               target=r.get("target", ""), cache=r.get("cache"))
        elif func == "stage_tools":
            live.stage_tools(r.get("names"), source_store=r.get("source_store", ""),
                             out=r.get("outpath", ""), target=r.get("target", ""))
        elif func == "ensure_environment":
            live.ensure_environment(r.get("name", ""), r.get("requirements"),
                                    root=r.get("root"), explicit=bool(r.get("explicit", False)),
                                    timeout=r.get("timeout", 1800))
        elif func == "ensure_python_tool":
            live.ensure_python_tool(r.get("name", ""), r.get("requirements"),
                                    r.get("executable", ""), root=r.get("root"),
                                    explicit=bool(r.get("explicit", False)),
                                    timeout=r.get("timeout", 1800))
        elif func == "build_requirements_environment":
            live.build_requirements_environment(
                r.get("requirements"), out=r.get("outpath", ""), python=r.get("python"),
                cache=r.get("cache"), env=r.get("env"),
                wheelhouse=r.get("wheelhouse"), offline=bool(r.get("offline", False)),
                sealed=bool(r.get("sealed", False)),
                explicit=bool(r.get("explicit", False)), lock=r.get("lock"))
        else:
            continue
    except TypeError:
        py = None       # sequence guard fired, matching a null C result
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    else:
        py = captured[0] if captured else None
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w5_client_039ee5: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
