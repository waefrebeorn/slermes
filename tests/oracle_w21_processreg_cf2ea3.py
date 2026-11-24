#!/usr/bin/env python3
"""Differential oracle for tools/process_registry_env_scope.py
(gate w21_processreg_cf2ea3).

The live module is pure stdlib, so the real ProcessEnvScopeMixin runs against a
registry double that supplies the four things it mixes into: _running,
_any_running, _finish_exited (and no _lock contention here). Sessions are built
from the fixture's env_ref tag, task_id and exited flag; the env under test is
one object, so `env_ref is env` is an identity question the fixture spells out.
"""
import importlib.util
import json
import os
import sys
import threading
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
SRC = Q + "/tools/process_registry_env_scope.py"
_spec = importlib.util.spec_from_file_location("w21_prsc_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["w21_prsc_live"] = live
_spec.loader.exec_module(live)

class Registry(live.ProcessEnvScopeMixin):
    def __init__(self, sessions):
        self._running = {i: s for i, s in enumerate(sessions)}
        self._lock = threading.Lock()
        self.finished = []

    def _any_running(self, pred):
        return any(pred(s) for s in self._running.values())

    def _finish_exited(self, session, code, reason=None, source=None):
        self.finished.append((session, code, reason, source))
        return True


def objects(tags):
    """One fresh object per env tag, so `env_ref is env` is a real identity test."""
    return {tag: types.SimpleNamespace() for tag in tags}


def make_session(spec, objs):
    s = types.SimpleNamespace()
    s.env_ref = objs.get(spec.get("env_ref")) if spec.get("env_ref") else None
    s.task_id = spec.get("task_id")
    s.exited = spec.get("exited", False)
    return s


def make_env(world, objs):
    tag = world.get("env_tag")
    if not tag:
        return None
    env = objs.setdefault(tag, types.SimpleNamespace())
    if "cleanup_stops_processes" in world:
        env.cleanup_stops_processes = world["cleanup_stops_processes"]
    return env


def expected(r):
    f = r.get("func")
    if f == "env_probe":
        sle = os.environ.get("SLERMES_HOME") or ""
        home = os.environ.get("HOME") or ""
        return {"sler": bool(sle), "home": bool(home), "same": bool(sle) and sle == home}
    if f == "in_env":
        objs = objects(("e", "other"))
        session = make_session(r.get("session") or {}, objs)
        # The env object under test is the one the tag names.
        tag = r.get("env_tag")
        env = objs.get(tag) if tag else None
        return bool(live._in_env(session, r.get("key") or "", env))
    w = r.get("world") if isinstance(r.get("world"), dict) else {}
    objs = objects(("e", "other"))
    sessions = [make_session(s, objs) for s in (w.get("sessions") or [])]
    env = make_env(w, objs)
    reg = Registry(sessions)
    if f == "has_running":
        return bool(reg.has_running_in_env(w.get("key") or "", env))
    if f == "settle":
        return reg.settle_env_closed(env)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"in_env", "has_running", "settle", "env_probe"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE w21_processreg_cf2ea3: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
