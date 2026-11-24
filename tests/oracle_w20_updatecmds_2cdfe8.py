#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_cmd_stale_survivors.py (w20_updatecmds_2cdfe8).

The live module is imported from the quarry against stubbed `hermes_cli`
siblings (update_cmd_fleet's drain/signal helpers and gateway's service-pid
probe), so the survivor sweep and the drain-first restart loop run for real
while no process is signalled. stdout is captured, because the operator lines
the loop prints are part of the behaviour under test.
"""
import contextlib
import importlib.util
import io
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


if "hermes_cli" not in sys.modules:
    _mk("hermes_cli")

STATE = {"identity": {}, "drain_raise": {}, "drain_ok": [], "service": [], "service_raises": False,
         "pending": []}


def _signalled_identity(pid):
    return STATE["identity"].get(str(pid))


def _drain_or_signal_gateway_for_update(pid, drain_budget, label):
    if str(pid) in STATE["drain_raise"]:
        raise RuntimeError(STATE["drain_raise"][str(pid)])
    return pid in STATE["drain_ok"]


def _restart_left_pending(pid, label, _third, born, supervised=False):
    STATE["pending"].append([pid, label, None, born, bool(supervised)])


def _get_service_pids(all_profiles=True):
    if STATE["service_raises"]:
        raise RuntimeError("service probe failed")
    return list(STATE["service"])


_mk("hermes_cli.update_cmd_fleet",
    _drain_or_signal_gateway_for_update=_drain_or_signal_gateway_for_update,
    _restart_left_pending=_restart_left_pending,
    _signalled_identity=_signalled_identity)
_mk("hermes_cli.gateway", _get_service_pids=_get_service_pids)

NAME = "w20_updatecmds_2cdfe8_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "hermes_cli", "update_cmd_stale_survivors.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


class Restart:
    def __init__(self, spec):
        spec = spec if isinstance(spec, dict) else {}
        self.killed_pids = set(spec.get("killed_pids") or [])
        stopped = spec.get("stopped_unmapped_pids")
        self.stopped_unmapped_pids = set(stopped) if isinstance(stopped, list) else None


def outcome(fn):
    try:
        return {"status": "ok", "value": fn()}
    except Exception as exc:  # noqa: BLE001 - report the live failure verbatim
        return {"status": "raised", "type": type(exc).__name__, "message": str(exc)}


def expected(r):
    f = r.get("func")
    if f == "stale_survivor_pids":
        already = r.get("already_signalled")
        return outcome(lambda: live.stale_fleet_survivor_pids(
            r.get("fleet"), set(already) if isinstance(already, list) else set()))
    if f == "signal":
        STATE["identity"] = r.get("identity") if isinstance(r.get("identity"), dict) else {}
        STATE["drain_raise"] = r.get("drain_raise") if isinstance(r.get("drain_raise"), dict) else {}
        STATE["drain_ok"] = r.get("drain_ok") if isinstance(r.get("drain_ok"), list) else []
        STATE["service"] = r.get("service_pids") if isinstance(r.get("service_pids"), list) else []
        STATE["service_raises"] = bool(r.get("service_raises"))
        STATE["pending"] = []
        restart = Restart(r.get("restart"))
        budget = r.get("drain_budget", 0)
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            result = outcome(lambda: live.signal_stale_fleet_survivors(
                r.get("fleet"), restart, budget))
        if result["status"] != "ok":
            return result
        if not buf.getvalue():
            # `if not pids: return []` -- the bare list, not the receipt dict
            return result
        manual = list(result["value"])
        # the live function returns the signalled list; the receipt carries the rest.
        # `manual` is the live line `if pid not in service_pids: manual.append(pid)`,
        # re-derived here from the service set the probe returned (the printed
        # "Stopped N ..." line independently checks its size).
        service = set() if STATE["service_raises"] else set(STATE["service"])
        printed = buf.getvalue().splitlines()
        stopped = restart.stopped_unmapped_pids
        return {"status": "ok", "value": {
            "signalled": manual,
            "manual": [pid for pid in manual if pid not in service],
            "killed": sorted(restart.killed_pids),
            "stopped": sorted(stopped) if isinstance(stopped, set) else None,
            "printed": printed,
            "pending": STATE["pending"],
        }}
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
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s\n  py=%r\n  c =%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_updatecmds_2cdfe8: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
