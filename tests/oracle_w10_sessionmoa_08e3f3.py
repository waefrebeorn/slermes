#!/usr/bin/env python3
"""Differential oracle for gate w10_sessionmoa_08e3f3 (gateway/session_moa_turn.py)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = [os.path.join(Q, *name.split("."))]
        sys.modules[name] = m
    return m


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# Top-level import: from gateway.config import Platform (unused by these funcs).
_pkg("gateway")
_mod("gateway.config", Platform=types.SimpleNamespace(LOCAL="local"))

NAME = "w10_sessionmoa_08e3f3_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "gateway", "session_moa_turn.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "moa_override":
        return live.moa_override(r.get("preset"))
    if f == "stamp":
        env = dict(r.get("envelope") or {})
        ev = types.SimpleNamespace(_moa_preset=r.get("preset"))
        live.stamp_moa_turn(ev, env)
        return env
    if f == "restore":
        ev = types.SimpleNamespace()
        live.restore_moa_turn(r.get("envelope") or {}, ev)
        return getattr(ev, "_moa_preset", None)
    return None


n = m = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if not isinstance(r, dict) or "out" not in r:
        continue
    n += 1
    py, got = compute(r), r.get("out")
    if py != got:
        m += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w10_sessionmoa_08e3f3: %d cases, %d mismatches" % (n, m))
sys.exit(1 if m else 0)
