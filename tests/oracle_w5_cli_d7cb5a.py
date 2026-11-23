#!/usr/bin/env python3
"""Differential oracle for pm/cli.py (w5_cli_d7cb5a).

The IO-heavy top-level imports are stubbed so the LIVE pure seams
_fmt_bytes / _install_flag_error / _split_without can be called directly.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

DEFAULTS = ["chromium", "cua-driver"]
DROPPABLE = ["matrix", "firecrawl"]
CANON = {"agent-browser": "chromium"}
ALL_TARGETS = ("win32-x64", "win32-arm64", "linux-x64", "linux-arm64",
               "linux-x64-musl", "linux-arm64-musl", "linux-arm64-bionic",
               "darwin-x64", "darwin-arm64")


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


pm = _mod("pm")
pm.__path__ = []
noop = lambda *a, **k: None
_mod("pm.termux_libs", load_table=noop, pins=noop, retired=noop, index=noop,
     repair=noop, save_table=noop)
_mod("pm.install", _facts=noop, _lockfile=noop, _reclaim_set_aside=noop, _store=noop,
     ensure=noop, stage_only=noop)
_mod("pm.operations", lock_project=noop)
_mod("pm.package", InstallError=type("InstallError", (Exception,), {}),
     Package=object, Runner=object, compose_env=noop)
_mod("pm.paths", repo_root=lambda: ".")
_mod("pm.registry", get_package=noop, source_install_packages=noop, tool_roots=noop)
_mod("pm.store", ALL_TARGETS=ALL_TARGETS, current_target=lambda: "linux-x64", hash_url=noop)
_mod("pm.update", Resolved=object, resolve_package=noop,
     reuse_index_responses=lambda: (lambda f: f))
_mod("pm.defaults", canonical_default_name=lambda n: CANON.get(n, n),
     default_package_names=lambda: DEFAULTS)
_mod("pm.extras", droppable_extras=lambda root: DROPPABLE)

SRC = Q + "/pm/cli.py"
spec = importlib.util.spec_from_file_location("pmcli_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["pmcli_live"] = live
spec.loader.exec_module(live)

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
    got = r.get("out")
    DEFAULTS[:] = r.get("defaults", [])
    DROPPABLE[:] = r.get("droppable", [])

    if func == "fmt_bytes":
        py = live._fmt_bytes(int(r.get("n", 0)))
    elif func == "flag_error":
        args = SimpleNamespace(names=[None] * int(r.get("n_names", 0)),
                               without=r.get("without", []))
        py = live._install_flag_error(
            args, extras=r.get("extras", []), cross_target=r.get("cross_target") or None,
            tools_only=bool(r.get("tools_only")), trust_recorded=bool(r.get("trust_recorded")),
            test_environment=("" if r.get("test_env") else None))
    elif func == "split_without":
        a, b, c = live._split_without(r.get("names", []))
        py = {"defaults": list(a), "extras": list(b), "unknown": list(c)}
    else:
        continue

    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w5_cli_d7cb5a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
