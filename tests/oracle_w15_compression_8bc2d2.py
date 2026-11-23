#!/usr/bin/env python3
"""Differential oracle for agent/compression_automation.py (gate w15_compression_8bc2d2)."""
import contextlib
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def _pkg(name, path):
    m = types.ModuleType(name)
    m.__path__ = [path]
    sys.modules[name] = m


_pkg("agent", QUARRY + "/agent")
_pkg("agent.prompt_cache_lifecycle", QUARRY + "/agent/prompt_cache_lifecycle")
_pkg("hermes_cli", QUARRY + "/hermes_cli")

_CALLS = []


def _rec(fn, old, new, reason=None):
    entry = {"fn": fn, "args": [old, new]}
    if reason is not None:
        entry["kwargs"] = {"reason": reason}
    _CALLS.append(entry)


def _mod(name, symbols):
    m = types.ModuleType(name)
    for k, v in symbols.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_mod("agent.bot_chain", {
    "carry_chain": lambda old, new: _rec("carry_chain", old, new)})
_mod("agent.prompt_cache_lifecycle", {
    "carry_missed_route": lambda old, new: _rec("carry_missed_route", old, new)})
_mod("agent.prompt_cache_lifecycle.tier", {
    "carry_cache_tier": lambda old, new: _rec("carry_cache_tier", old, new)})


class RuntimeSessionStore:
    pass


_mod("agent.runtime_session_store", {"RuntimeSessionStore": RuntimeSessionStore})


@contextlib.contextmanager
def _swallow(msg):
    yield


_mod("agent.conversation_compression", {"_swallow": _swallow})
_mod("hermes_cli.goals", {
    "migrate_goal_to_session": lambda old, new, reason=None: _rec(
        "migrate_goal_to_session", old, new, reason)})
_mod("hermes_cli.heartbeat", {
    "migrate_heartbeat_to_session": lambda old, new: _rec(
        "migrate_heartbeat_to_session", old, new)})
_mod("hermes_cli.loops", {
    "migrate_loop_to_session": lambda old, new, reason=None: _rec(
        "migrate_loop_to_session", old, new, reason)})

spec = importlib.util.spec_from_file_location(
    "agent.compression_automation_live", QUARRY + "/agent/compression_automation.py")
live = importlib.util.module_from_spec(spec)
sys.modules["agent.compression_automation_live"] = live
spec.loader.exec_module(live)


class _Agent:
    def __init__(self, session_id, runtime_store):
        self.session_id = session_id
        self._session_db = RuntimeSessionStore() if runtime_store else object()


def compute(r):
    if r.get("func") != "carry":
        return None
    _CALLS.clear()
    agent = _Agent(r.get("new_sid", ""), bool(r.get("is_runtime_store", False)))
    live.carry_compression_automation(agent, r.get("old_sid", ""))
    return list(_CALLS)


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
        print("PYERROR %r %r" % (r, exc), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)
print("ORACLE w15_compression_8bc2d2: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
