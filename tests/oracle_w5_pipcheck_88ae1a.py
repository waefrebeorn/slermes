#!/usr/bin/env python3
"""Differential oracle for pm/pip_check.py (gate w5_pipcheck_88ae1a).

The C harness receives already-parsed lock facts; the same facts are rendered
to canonical uv.lock TOML for a FakeLock the live stdlib/tomllib parser reads.
"""
import importlib.util
import json
import os
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


LIVE_NAME = "w5_pipcheck_88ae1a_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/pm/pip_check.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


def _lock(entries, packages):
    lines = ["[manifest]", "overrides = ["]
    for entry in entries or []:
        lines.append("{" + ", ".join("%s = %s" % (k, json.dumps(v)) for k, v in entry.items()) + "},")
    lines.append("]")
    for name, version in packages or []:
        lines += ["[[package]]", "name = %s" % json.dumps(name), "version = %s" % json.dumps(version)]
    text = "\n".join(lines) + "\n"

    class FakeLock:
        def is_file(self):
            return True

        def read_text(self, encoding=None):
            return text

    return FakeLock()


def compute(r):
    f = r.get("func")
    if f == "normalize":
        return live._normalize(r.get("name") or "")
    if f == "override_requirements":
        lock = _lock(r.get("entries"), [])
        try:
            return live.override_requirements(lock)
        except ValueError:
            return {"__error__": "ValueError"}
    if f == "overridden_versions":
        lock = _lock(r.get("entries"), r.get("packages"))
        return {k: sorted(v) for k, v in live.overridden_versions(lock).items()}
    if f == "unexpected":
        lock = None
        if r.get("lock") is not None:
            lock = _lock(r["lock"].get("overrides"), r["lock"].get("packages"))
        return live.unexpected_incompatibilities(r.get("report", ""), lock)
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
        print("MISMATCH pipcheck %s args=%r py=%r c=%r" % (r.get("func"), r, py, got), file=sys.stderr)

print("ORACLE w5_pipcheck_88ae1a: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
