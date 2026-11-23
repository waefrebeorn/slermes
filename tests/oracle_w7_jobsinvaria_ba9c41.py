#!/usr/bin/env python3
"""Differential oracle for cron/jobs_invariants.py (gate w7_jobsinvaria_ba9c41).

display_hermes_home, cron.lifecycle_guard._resolve_script_path and the two
cron.jobs_time normalizers are injected deterministic collaborators; the live
module's three invariant helpers are loaded with
importlib.util.spec_from_file_location and executed for every case.
"""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

R = {}


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_mod("hermes_constants", display_hermes_home=lambda: "/home/test/.hermes")
cron_pkg = _mod("cron")
cron_pkg.__path__ = []

SRC = QUARRY + "/cron/jobs_invariants.py"
spec = importlib.util.spec_from_file_location("w7_jobsinvaria_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


class FakePath:
    def __init__(self, is_file):
        self._is_file = bool(is_file)

    def is_file(self):
        return self._is_file


def install_collaborators(r):
    global R
    R = r
    live.display_hermes_home = lambda: r.get("home", "/home/test/.hermes")

    def resolve(script):
        if r.get("script_path") is None:
            return None
        return FakePath(r.get("script_is_file", False))

    _mod("cron.lifecycle_guard", _resolve_script_path=resolve)

    def norm_tz(v):
        if r.get("tz_error"):
            raise ValueError("timezone error")
        return v

    def norm_grace(v):
        if r.get("grace_error"):
            raise ValueError("grace error")
        if r.get("grace_none"):
            return None
        return 5

    _mod("cron.jobs_time", normalize_timezone=norm_tz, normalize_grace_minutes=norm_grace)


def call_live(func, r):
    install_collaborators(r)
    try:
        if func == "script_is_command_line":
            return bool(live._script_is_command_line(r.get("script", "")))
        if func == "mode_error":
            live._validate_job_mode_invariants(
                r.get("monitor_script"), r.get("monitor_url"),
                bool(r.get("no_agent", False)), r.get("script"))
            return ""
        if func == "schedule_error":
            live.validate_schedule_knobs(
                r.get("schedule"), r.get("timezone"), r.get("grace_minutes"))
            return ""
        return None
    except ValueError as exc:
        return str(exc)


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    if func not in ("script_is_command_line", "mode_error", "schedule_error"):
        continue
    cases += 1
    try:
        py = call_live(func, r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR case=%s %r" % (r.get("case"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH case=%s\n  py=%r\n  c =%r" % (r.get("case"), py, got), file=sys.stderr)

print("ORACLE w7_jobsinvaria_ba9c41: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
