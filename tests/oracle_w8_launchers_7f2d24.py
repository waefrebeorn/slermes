#!/usr/bin/env python3
"""Differential oracle for hermes_cli/_launchers.py pure helpers (w8_launchers_7f2d24)."""
import importlib.util, json, os, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [os.path.join(Q, *parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


stub("pm.environments", owning_home_root=lambda r: None,
     payload_command_dir=lambda r: None, store_root=lambda r: r)
# parent-package bootstrap for hermes_cli (the live module's own package).
hc = types.ModuleType("hermes_cli")
hc.__path__ = [Q + "/hermes_cli"]
sys.modules["hermes_cli"] = hc

LIVE_NAME = "w8_launchers_7f2d24_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/hermes_cli/_launchers.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "pin_default_home_flag":
        return live.PIN_DEFAULT_HOME_FLAG
    if f == "windows_bin_launchers":
        return list(live.WINDOWS_BIN_LAUNCHERS)
    if f == "entry_points":
        return {k: list(v) for k, v in live.ENTRY_POINTS.items()}
    if f == "inline_string_literal":
        return live._inline_string_literal(r.get("value", ""))
    if f == "first_token":
        result = live._first_token(r.get("line", ""))
        return None if result is None else str(result)
    if f == "merge_user_path":
        return live._merge_user_path(r.get("existing", ""), r.get("entry", ""))
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
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print("LIVE ERROR %r: %r" % (r, exc), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w8_launchers_7f2d24: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
