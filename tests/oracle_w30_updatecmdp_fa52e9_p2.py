#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_cmd_posix_pause.py (gate
w30_updatecmdp_fa52e9_p2).

The live module is imported from the quarry and its pure value functions are
called on the fixture input, with every seam replaced by a fixture answer: the
control-socket ACK, the gateway argv, psutil's environment, the respawning
systemd unit, the ancestor test, the cgroup escape proof, the identity reader
(PID file + runtime lock) and the pause record's ``sync``.  Nothing is spawned
and no record is written: what is compared is exactly what those values decide.

  * ``_names`` / ``_describe``      - how a paused set is named;
  * ``_ask_to_drain``               - did the gateway take the drain;
  * ``_supervisor_markers``         - why a bare gateway is supervised;
  * ``_left_running``               - which runtimes must stay up;
  * ``_abort_unsettled``            - the --force refusal + token mutations;
  * ``_bare_replay_env``            - the replay environment cleanup;
  * ``_gateway_on_home``            - the adopted-pid rule.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# The login-shell proof environment (msys `bash -lc`) has no Windows home:
# HOME is a POSIX path and USERPROFILE is unset, so pathlib.Path.home() raises
# inside the quarry's import chain (tools.environments -> hermes_constants).
# Repair the environment, not the behaviour.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = os.environ.get("HOME") or os.getcwd()

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_updatecmdp_fa52e9_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import update_cmd_posix_pause as live  # noqa: E402
import hermes_cli.gateway as gw  # noqa: E402
import hermes_cli.update_cmd_fleet as fleet  # noqa: E402
import gateway.control_socket as control_socket  # noqa: E402
import gateway.status as gwstatus  # noqa: E402
import psutil  # noqa: E402

WANTED = {
    "names", "describe", "drain_ack", "supervisor_marker", "left_running",
    "abort_unsettled", "bare_replay_env", "gateway_on_home",
}

_MISSING = object()


class patchattr:
    """Set an attribute for the body, then put back what was there."""

    def __init__(self, obj, name, value):
        self.obj, self.name, self.value = obj, name, value

    def __enter__(self):
        self.old = getattr(self.obj, self.name, _MISSING)
        setattr(self.obj, self.name, self.value)
        return self.value

    def __exit__(self, *exc):
        if self.old is _MISSING:
            delattr(self.obj, self.name)
        else:
            setattr(self.obj, self.name, self.old)
        return False


def f_names(r):
    return live._names([dict(e) for e in r.get("entries") or [] if isinstance(e, dict)])


def f_describe(r):
    return live._describe(r.get("token") or {})


def f_drain_ack(r):
    ack = r.get("ack")

    def pause_gateway_for_update(home):
        if r.get("raises"):
            raise RuntimeError("no answer")
        return ack

    with patchattr(control_socket, "pause_gateway_for_update", pause_gateway_for_update):
        return bool(live._ask_to_drain({"home": r.get("home")}))


def f_supervisor_marker(r):
    argv = ["gateway", "run"]
    if r.get("argv_external"):
        argv.append("--external-supervisor")

    class FakeProcess:
        def __init__(self, pid):
            self.pid = pid

        def environ(self):
            if r.get("env_error"):
                raise OSError("unreadable environment")
            return dict(r.get("env") or {})

    with patchattr(gw, "_capture_gateway_argv", lambda pid: list(argv)), \
            patchattr(psutil, "Process", FakeProcess), \
            patchattr(live, "_respawning_service", lambda pid: r.get("respawning_unit")):
        return live._supervisor_markers(int(r.get("pid") or 4242))


def f_left_running(r):
    units = [dict(u) for u in r.get("units") or [] if isinstance(u, dict)]
    jobs = [dict(j) for j in r.get("jobs") or [] if isinstance(j, dict)]
    escape = {u.get("unit"): bool(u.get("escape_ok")) for u in units}
    ancestor = {int(j.get("pid") or 0): bool(j.get("ancestor")) for j in jobs}
    xpc = r.get("xpc")

    previous = os.environ.get("XPC_SERVICE_NAME", _MISSING)
    if xpc is None:
        os.environ.pop("XPC_SERVICE_NAME", None)
    else:
        os.environ["XPC_SERVICE_NAME"] = str(xpc)
    try:
        with patchattr(fleet, "_needs_sudo", lambda scope: bool(r.get("needs_sudo"))), \
                patchattr(fleet, "_sudo_noninteractive_ok", lambda probe: bool(r.get("sudo_ok"))), \
                patchattr(gw, "_is_pid_ancestor_of_current_process",
                          lambda pid: ancestor.get(int(pid), False)), \
                patchattr(live, "_escape_cgroup", lambda unit: escape.get(unit.get("unit"), False)):
            keep_units, keep_jobs, notices = live._left_running(units, jobs)
    finally:
        if previous is _MISSING:
            os.environ.pop("XPC_SERVICE_NAME", None)
        else:
            os.environ["XPC_SERVICE_NAME"] = previous
    return {
        "keep_units": [u.get("unit") for u in keep_units],
        "keep_jobs": [j.get("label") for j in keep_jobs],
        "notices": list(notices),
    }


def f_abort_unsettled(r):
    import hermes_cli.update_pause_record as pause_record

    token = json.loads(json.dumps(r.get("token") or {}))
    with patchattr(pause_record, "sync", lambda tok: None):
        message = live._abort_unsettled(token, r.get("why"))
    return {"message": message, "token": token}


def f_bare_replay_env(r):
    env = dict(r.get("env") or {})
    with patchattr(live, "_replay_env", lambda home: dict(env)):
        return live._bare_replay_env("home")


def f_gateway_on_home(r):
    def live_gateway_pid_for_home(home):
        if r.get("error"):
            raise OSError("unreadable identity files")
        return r.get("home_pid")

    exclude = {int(p) for p in r.get("exclude") or []}
    with patchattr(gwstatus, "live_gateway_pid_for_home", live_gateway_pid_for_home):
        return live._gateway_on_home("/home/x", exclude)


def expected(r):
    f = r.get("func")
    if f == "names":
        return f_names(r)
    if f == "describe":
        return f_describe(r)
    if f == "drain_ack":
        return f_drain_ack(r)
    if f == "supervisor_marker":
        return f_supervisor_marker(r)
    if f == "left_running":
        return f_left_running(r)
    if f == "abort_unsettled":
        return f_abort_unsettled(r)
    if f == "bare_replay_env":
        return f_bare_replay_env(r)
    if f == "gateway_on_home":
        return f_gateway_on_home(r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
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
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
