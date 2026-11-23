#!/usr/bin/env python3
"""Differential oracle for gate w11_stalemodul_bf7d5c (hermes_cli/stale_modules.py)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
# parent-package bootstrap (in-function imports / module registration seams)
if "hermes_cli" not in sys.modules:
    _hp = types.ModuleType("hermes_cli")
    _hp.__path__ = [os.path.join(Q, "hermes_cli")]
    sys.modules["hermes_cli"] = _hp
NAME = "w11_stalemodul_bf7d5c_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "hermes_cli", "stale_modules.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

DEFAULT = {"utils": ("file_signature", "file_cache_version")}


def compute(r):
    req = r.get("required", None)
    modules = r.get("modules", {}) or {}
    checks = DEFAULT if req is None else {k: tuple(v) for k, v in req.items()}
    names = set(checks) | set(modules.keys())
    saved = {n: sys.modules.get(n) for n in names}
    had = {n: (n in sys.modules) for n in names}
    try:
        for n in names:
            sys.modules.pop(n, None)
        for n, attrs in modules.items():
            if attrs is None:
                sys.modules[n] = None
            else:
                mo = types.ModuleType(n)
                for a in attrs:
                    setattr(mo, a, True)
                sys.modules[n] = mo
        return list(live.drop_stale_root_modules(None if req is None else checks))
    finally:
        for n in names:
            sys.modules.pop(n, None)
        for n in names:
            if had[n]:
                sys.modules[n] = saved[n]


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
print("ORACLE w11_stalemodul_bf7d5c: %d cases, %d mismatches" % (n, m))
sys.exit(1 if m else 0)
