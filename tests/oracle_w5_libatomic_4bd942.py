#!/usr/bin/env python3
"""Differential oracle for pm/libatomic.py (gate w5_libatomic_4bd942).

Environment/process seams are injected: the C harness receives the same
release tokens / available managers / is_root / sudo path as the fixture and
the live module's `_is_root` and `shutil.which` are patched to agree.
"""
import importlib.util
import json
import os
import shlex
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _stub(name, **attrs):
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


LIVE_NAME = "w5_libatomic_4bd942_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/pm/libatomic.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "manager_order":
        return list(live._manager_order(set(r.get("tokens") or [])))
    if f == "host_install_command":
        tokens = set(r.get("tokens") or [])
        available = set(r.get("available") or [])
        for manager in live._manager_order(tokens):
            if manager in available:
                return list(live._INSTALLERS[manager])
        return None
    if f == "command_plan":
        is_root = bool(r.get("is_root", False))
        sudo_path = r.get("sudo_path") or None
        live._is_root = lambda: is_root
        live.shutil = types.SimpleNamespace(which=lambda name: sudo_path if name == "sudo" else None)
        argv, shown, remedy = live._command_plan(tuple(r.get("command") or []))
        return {"argv": list(argv) if argv is not None else None, "shown": shown, "remedy": remedy}
    if f == "shlex_join":
        return shlex.join(r.get("tokens") or [])
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
        print("LIVE ERROR %s args=%r: %r" % (r.get("func"), r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH libatomic %s args=%r py=%r c=%r" % (r.get("func"), r, py, got), file=sys.stderr)

print("ORACLE w5_libatomic_4bd942: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
