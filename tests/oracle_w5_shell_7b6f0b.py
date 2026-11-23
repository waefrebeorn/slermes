#!/usr/bin/env python3
"""Differential oracle for upstream Hermes pm/shell.py (gate w5_shell_7b6f0b).

windows_bash_candidates is pure data: the env mapping and on_path come from
the fixture. The module's pm.paths / pm.lock imports are stubbed so the live
module loads; the filesystem/process probes are never called.
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
_mod("pm.paths", facts_path=lambda: None, store_root=lambda: None)
_mod("pm.lock", Facts=lambda *a, **k: None)

SRC = Q + "/pm/shell.py"
spec = importlib.util.spec_from_file_location("w5_shell_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w5_shell_live"] = live
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
    if r.get("func") != "candidates":
        continue
    on_path = r.get("on_path")
    env = r.get("env") or {}
    try:
        py = live.windows_bash_candidates(on_path, env)
    except Exception as exc:
        print(f"LIVE ERROR args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH candidates args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w5_shell_7b6f0b: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
