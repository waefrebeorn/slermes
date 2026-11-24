#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_supervised_restart.py
(gate w20_gatewaysup_735d69).

The live module is imported for real and its three decisions are run against
scripted collaborators: the control socket's identify answer, the live argv
capture, the runtime-status record, the pidfile reader and the detached starter.
poll_until is replaced with a virtual-clock equivalent (same loop, same
min(interval, remaining) arithmetic) so the replacement wait is deterministic
without sleeping.
"""
import importlib.util
import json
import os
import pathlib
import sys
import types

sys.dont_write_bytecode = True
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

import gateway.control_socket as control_socket          # noqa: E402
import gateway.status as gateway_status                  # noqa: E402
import hermes_cli.gateway as gw                          # noqa: E402
import hermes_cli.gateway_runtime_start as gw_start      # noqa: E402

NAME = "w20_gatewaysup_735d69_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "hermes_cli", "gateway_supervised_restart.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

HOME = "C:/temp/hermes-home"
HOME_PATH = pathlib.Path(HOME)


def virtual_poll_until(predicate, timeout, interval):
    """agent.deadline.poll_until's loop on a clock that advances by sleeping."""
    deadline = 0.0 + timeout
    now = 0.0
    while True:
        result = predicate()
        if result:
            return result
        remaining = deadline - now
        if remaining <= 0:
            return result
        now += min(interval, remaining)


class Sequence:
    def __init__(self, values):
        self.values = list(values)
        self.index = 0

    def __call__(self):
        if not self.values:
            return None
        value = self.values[self.index] if self.index < len(self.values) else self.values[-1]
        self.index += 1
        return value


def expected(r):
    f = r.get("func")
    if f == "declares_external_supervisor":
        identity = r.get("identity")
        argv = r.get("argv")
        record = r.get("record")
        control_socket.identify_gateway = lambda home: identity
        gateway_status._get_process_hermes_home = lambda: HOME_PATH
        gateway_status.read_runtime_status = lambda path: record
        gw._capture_gateway_argv = lambda pid: argv
        return live.gateway_declares_external_supervisor(r.get("pid"), HOME_PATH)
    if f == "replacement_pid":
        live.poll_until = virtual_poll_until
        gateway_status.get_running_pid = Sequence(r.get("observed") or [])
        timeout = r.get("timeout")
        interval = r.get("poll_interval", 0.5)
        return live._wait_for_supervised_gateway_replacement(
            r.get("old_pid"), timeout, poll_interval=interval)
    if f == "start_mode":
        calls = {}

        def run_gateway(**kwargs):
            calls["run_gateway"] = kwargs

        def detached(home, *, force):
            calls["detached"] = {"home": home, "force": force}
            return int(r.get("detach_code", 0))

        gw.get_hermes_home = lambda: HOME_PATH
        gw.run_gateway = run_gateway
        gw_start.start_detached_replacement = detached
        args = types.SimpleNamespace(**(r.get("args") or {}))
        force = bool(r.get("force"))
        try:
            live.start_restart_replacement(args, force=force)
        except SystemExit as exc:
            return {"mode": "detach", "home": pathlib.PurePath(calls["detached"]["home"]).as_posix(),
                    "force": bool(calls["detached"]["force"]), "code": exc.code}
        return {"mode": "foreground", "force": bool(calls["run_gateway"]["force"]),
                "verbose": calls["run_gateway"]["verbose"]}
    if f == "verify_timeout":
        return live.SUPERVISED_REPLACEMENT_VERIFY_TIMEOUT
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
        print("MISMATCH %s case=%s\n  py=%r\n  c =%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_gatewaysup_735d69: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
