#!/usr/bin/env python3
"""Differential oracle for hermes_startup_watchdog.py (gate w5_hermesstart_a84a61).

Env values, the monotonic clock and the handle class are injected per case; the
live module's own decision functions (and the handle's kick/lease/arm code) are
what actually compute the expected values. No process environment is mutated.
"""
import importlib.util
import json
import sys
import threading
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

spec = importlib.util.spec_from_file_location("w5_hermesstart_live", Q + "/hermes_startup_watchdog.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w5_hermesstart_live"] = live
spec.loader.exec_module(live)


def _set_env(disabled_val, timeout_val):
    live.os = types.SimpleNamespace(environ={
        live.ENV_STARTUP_WATCHDOG: disabled_val,
        live.ENV_STARTUP_WATCHDOG_TIMEOUT_S: timeout_val,
    })


def _fresh_handle():
    h = object.__new__(live.StartupWatchdogHandle)
    h.timeout_s = 100.0
    h._state = live._ARMED
    h._state_lock = threading.Lock()
    h._deadline = 0.0
    h._lease_until = 0.0
    h._lease_phase = None
    h._lease_count = 0
    return h


class _FakePath:
    def __init__(self, s):
        self.s = s
    def joinpath(self, *parts):
        return _FakePath("/".join([self.s] + list(parts)))
    def __str__(self):
        return self.s


class _FakeHandle:
    def __init__(self, timeout_s, exit_code):
        self.timeout_s = timeout_s
        self.exit_code = exit_code
    def _start(self):
        return True
    def is_alive(self):
        return True


def compute(r):
    f = r.get("func")
    if f == "disabled":
        _set_env(r.get("env_value", ""), "")
        return live.startup_watchdog_disabled()
    if f == "resolve_timeout":
        _set_env("", r.get("raw", ""))
        return live.resolve_startup_watchdog_timeout()
    if f == "dump_path":
        return str(live.get_startup_watchdog_dump_path(_FakePath(r.get("home", ""))))
    if f == "kick_extra":
        live.time = types.SimpleNamespace(monotonic=lambda: 0.0, sleep=lambda s: None)
        h = _fresh_handle()
        h.kick(r.get("extra_s", ""))
        return h._deadline - h.timeout_s
    if f == "lease_clamp":
        live.time = types.SimpleNamespace(monotonic=lambda: 0.0, sleep=lambda s: None)
        h = _fresh_handle()
        h.lease(r.get("expected_s", ""), r.get("phase", ""))
        return h._lease_until if h._lease_count else None
    if f == "arm_timeout":
        _set_env("0" if r.get("disabled", False) else "", r.get("env_raw", ""))
        live.time = types.SimpleNamespace(monotonic=lambda: 0.0, sleep=lambda s: None)
        live._handle = None
        live.StartupWatchdogHandle = _FakeHandle
        h = live.arm_startup_watchdog(r.get("timeout_s"))
        return h.timeout_s if h else None
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
        print("MISMATCH hermesstart %s args=%r py=%r c=%r" % (r.get("func"), r, py, got), file=sys.stderr)

print("ORACLE w5_hermesstart_a84a61: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
