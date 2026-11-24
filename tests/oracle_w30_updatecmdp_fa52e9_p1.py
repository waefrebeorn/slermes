#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_cmd_posix_pause.py (gate
w30_updatecmdp_fa52e9_p1).

The live module is imported from the quarry and its pure value functions are
called on the fixture input with every process/systemd seam replaced by a
fixture answer (a start time, a liveness verdict, a `systemctl show` state, a
sudo probe result).  Nothing is spawned, no /proc entry is read: what is
compared is exactly what those values decide.

  * ``_empty_token``            - the pause token's shape;
  * ``gateways_paused``         - the pause predicate over that token;
  * ``_inside``                 - cgroup containment;
  * ``_scope_cmd`` / ``_manage_cmd`` - the systemctl / sudo prefixes;
  * ``unit_stop_pending``       - has the unit's stop settled;
  * ``_alive``                  - liveness by pid + native start time;
  * ``is_left_pending``         - the very process left running;
  * ``adopt_left_pending``      - which recorded pairs a fresh process restores.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# The login-shell proof environment (msys `bash -lc`) has no Windows home:
# HOME is a POSIX path and USERPROFILE is unset, so pathlib.Path.home() raises
# inside the quarry's import chain.  Repair the environment, not the behaviour.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = os.environ.get("HOME") or os.getcwd()

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_updatecmdp_fa52e9_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import update_cmd_posix_pause as live  # noqa: E402
import hermes_cli.update_cmd_fleet as fleet  # noqa: E402
import hermes_cli.update_cmd_fleet_unit_budget as budget  # noqa: E402
import gateway.status as gwstatus  # noqa: E402

WANTED = {
    "empty_token", "gateways_paused", "inside", "scope_cmd", "manage_cmd",
    "unit_stop_pending", "alive", "is_left_pending", "adopt_left_pending",
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


def f_empty_token(r):
    return live._empty_token()


def f_gateways_paused(r):
    with patchattr(live, "_RUN", r.get("token")):
        return bool(live.gateways_paused())


def f_inside(r):
    return bool(live._inside(r.get("cgroup"), r.get("unit_cgroup")))


def f_scope_cmd(r):
    return live._scope_cmd({"scope": r.get("scope")})


def f_manage_cmd(r):
    with patchattr(fleet, "_needs_sudo", lambda scope: bool(r.get("needs_sudo"))), \
            patchattr(fleet, "_sudo_noninteractive_ok", lambda probe: bool(r.get("sudo_ok"))):
        return live._manage_cmd(r.get("scope"))


def f_unit_stop_pending(r):
    state = r.get("state")
    state = dict(state) if isinstance(state, dict) else {}
    with patchattr(budget, "unit_job_state", lambda scope_cmd, name: dict(state)):
        return bool(live.unit_stop_pending({"kind": r.get("kind"), "scope": r.get("scope"),
                                            "unit": r.get("unit")}))


def f_alive(r):
    verdict = r.get("pid_exists")

    def pid_exists(pid):
        if verdict == "error":
            raise OSError("unreadable")
        return verdict == "yes"

    with patchattr(gwstatus, "_pid_exists", pid_exists), \
            patchattr(gwstatus, "get_process_start_time", lambda pid: r.get("start_time")):
        return bool(live._alive(int(r.get("pid") or 0), r.get("ct")))


def f_is_left_pending(r):
    pairs = {}
    for pair in r.get("left_pending") or []:
        pairs[int(pair[0])] = pair[1]
    with patchattr(live, "_LEFT_PENDING", pairs), \
            patchattr(gwstatus, "get_process_start_time", lambda pid: r.get("ct")):
        return bool(live.is_left_pending(int(r.get("pid") or 0)))


def f_adopt_left_pending(r):
    starts = {str(k): v for k, v in (r.get("start_times") or {}).items()}
    with patchattr(live, "_LEFT_PENDING", {}), \
            patchattr(gwstatus, "get_process_start_time", lambda pid: starts.get(str(pid))):
        live.adopt_left_pending({"left_pending": r.get("pairs") or []})
        return [[int(pid), ct] for pid, ct in sorted(live._LEFT_PENDING.items())]


def expected(r):
    f = r.get("func")
    if f == "empty_token":
        return f_empty_token(r)
    if f == "gateways_paused":
        return f_gateways_paused(r)
    if f == "inside":
        return f_inside(r)
    if f == "scope_cmd":
        return f_scope_cmd(r)
    if f == "manage_cmd":
        return f_manage_cmd(r)
    if f == "unit_stop_pending":
        return f_unit_stop_pending(r)
    if f == "alive":
        return f_alive(r)
    if f == "is_left_pending":
        return f_is_left_pending(r)
    if f == "adopt_left_pending":
        return f_adopt_left_pending(r)
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
