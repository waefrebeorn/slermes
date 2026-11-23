#!/usr/bin/env python3
"""Differential oracle for upstream Hermes pm/store.py (gate w5_store_63be39).

The module's pm.filesystem import is stubbed so the live module loads; the
OS probes behind current_target are injected per case so it stays pure.
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


pm = _mod("pm")
pm.__path__ = []
_mod("pm.filesystem", is_junction=lambda p: False,
     long_root=lambda p: p, retry_held=lambda fn: fn())

SRC = Q + "/pm/store.py"
spec = importlib.util.spec_from_file_location("w5_store_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w5_store_live"] = live
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
    try:
        if func == "entry_component":
            try:
                py = live.entry_component(r.get("name", ""))
            except ValueError:
                py = None
        elif func == "fetch_entry_name":
            try:
                py = live.fetch_entry_name(r.get("digest", ""))
            except ValueError:
                py = None
        elif func == "current_target":
            machine = r.get("machine", "")
            live._native_machine = lambda: machine
            live.sys = types.SimpleNamespace(platform=r.get("sys_platform", ""))
            live.platform = types.SimpleNamespace(machine=lambda: machine)
            live._is_bionic_libc = lambda: bool(r.get("bionic", False))
            live._is_musl_libc = lambda: bool(r.get("musl", False))
            try:
                py = live.current_target()
            except RuntimeError:
                py = None
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w5_store_63be39: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
