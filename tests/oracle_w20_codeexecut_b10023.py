#!/usr/bin/env python3
"""Differential oracle for tools/code_execution_lifecycle.py (w20_codeexecut_b10023).

The live module needs cron.lifecycle_guard only inside lifecycle_guard_rejection,
so that import is stubbed with predicates driven by the fixture line; the static
judgement itself (_is_inert_python, the three tables, _MAX_MATCH_CHARS and the
rejection text) is the real upstream code. Every harness line is replayed
through it and `out` is compared.
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

HOST_KILL_TEXT = "Blocked: this looks like a host interpreter kill; run it outside the gateway."

STATE = {"contains": False, "host_kill": False, "match": None}


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_mk("cron")
_mk("cron.lifecycle_guard")
guard = sys.modules["cron.lifecycle_guard"]
guard.HOST_INTERPRETER_KILL_REJECTION = HOST_KILL_TEXT
guard.contains_gateway_lifecycle_command = lambda code: bool(STATE["contains"])
guard.contains_host_interpreter_kill = lambda code: bool(STATE["host_kill"])
guard.first_gateway_lifecycle_match = lambda code: STATE["match"]

NAME = "w20_codeexecut_b10023_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "tools", "code_execution_lifecycle.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def expected(r):
    f = r.get("func")
    if f == "is_inert_python":
        return bool(live._is_inert_python(r.get("code") or ""))
    if f == "safe_builtin":
        return r.get("name") in live._SAFE_BUILTINS
    if f == "inert_import":
        return r.get("name") in live._INERT_IMPORTS
    if f == "sink_attr":
        name = r.get("name") or ""
        return bool(name in live._SINK_ATTRS or name in live._INTROSPECTION_ATTRS
                    or name.startswith(live._SINK_ATTR_PREFIXES + live._INTROSPECTION_PREFIXES))
    if f == "max_match_chars":
        return int(live._MAX_MATCH_CHARS)
    if f == "rejection":
        STATE["contains"] = bool(r.get("contains", True))
        STATE["host_kill"] = bool(r.get("host_kill", False))
        STATE["match"] = r.get("match") or None
        return live.lifecycle_guard_rejection(r.get("code") or "")
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001 - report the live failure verbatim
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_codeexecut_b10023: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
