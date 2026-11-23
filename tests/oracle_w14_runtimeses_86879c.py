#!/usr/bin/env python3
"""Differential oracle for gate w14_runtimeses_86879c (agent/runtime_session_store.py)."""
import importlib.util
import json
import os
import sys
import time
import types

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


_pkg("agent")
_mod("agent.runtime_session_compression",
     RuntimeSessionCompressionMixin=type("RuntimeSessionCompressionMixin", (), {}))
_mod("agent.runtime_session_lifecycle",
     RuntimeSessionLifecycleMixin=type("RuntimeSessionLifecycleMixin", (), {}))
_mod("hermes_dir_acl", make_user_dir=lambda *a, **k: None)

NAME = "w14_runtimeses_86879c_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "agent", "runtime_session_store.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)
S = live.RuntimeSessionStore


def _self(row):
    fs = types.SimpleNamespace(get_session=lambda sid: row)
    fs.get_compression_failure_cooldown_row = types.MethodType(
        S.get_compression_failure_cooldown_row, fs)
    return fs


def compute(r):
    f = r.get("func")
    if f == "apply_row_annotations":
        msgs = json.loads(json.dumps(r.get("messages") or []))
        live.apply_row_annotations(msgs, r.get("annotations") or [])
        return msgs
    if f == "session_number":
        cast = float if r.get("cast_kind") == "float" else int
        return S._session_number(_self(r.get("row") or {}), "sid", r.get("column", ""),
                                 cast, r.get("zero", 0))
    if f == "compression_cooldown":
        old = time.time
        time.time = lambda: r.get("now", 0.0)
        try:
            return S.get_compression_failure_cooldown(_self(r.get("row") or {}), "sid")
        finally:
            time.time = old
    return None


def main():
    cases = mism = 0
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
        cases += 1
        py, got = compute(r), r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
    print("ORACLE w14_runtimeses_86879c: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
