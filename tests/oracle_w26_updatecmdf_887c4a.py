#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_cmd_fleet_unit_budget.py
(gate w26_updatecmdf_887c4a).

``update_cmd_fleet._systemctl`` is the seam the live functions late-import; it
is replaced per case by a stub that returns the fixture's ``show``/``rc`` or
raises the fixture's FileNotFoundError / TimeoutExpired.  No systemctl runs.
"""
import json
import os
import subprocess
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_updatecmdf_887c4a"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
os.environ.setdefault("USERPROFILE", str(QUARRY.parent))
os.environ.setdefault("HOME", str(QUARRY.parent))

import hermes_cli.update_cmd_fleet as fleet  # noqa: E402
import hermes_cli.update_cmd_fleet_unit_budget as live  # noqa: E402

WANTED = {"restart_timeout", "unit_job_budget", "unit_job_state", "settled",
          "service_restart_sec", "constants"}


class Show:
    """The CompletedProcess shape the live code reads: returncode + stdout."""

    def __init__(self, rc, out):
        self.returncode = rc
        self.stdout = out
        self.stderr = ""


def install(req):
    mode = req.get("raise") or ""
    rc = int(req.get("rc") or 0)
    out = req.get("show") if isinstance(req.get("show"), str) else ""

    def fake(argv, timeout=None, **kw):
        if mode == "missing":
            raise FileNotFoundError(2, "No such file or directory")
        if mode == "timeout":
            raise subprocess.TimeoutExpired(argv, timeout if timeout else 1)
        return Show(rc, out)

    fleet._systemctl = fake


def expected(req):
    f = req.get("func")
    install(req)
    if f == "restart_timeout":
        return live.systemd_restart_timeout([], "hermes-gateway.service",
                                            start_only=bool(req.get("start_only")))
    if f == "unit_job_budget":
        return live.unit_job_budget([], "hermes-gateway.service", req.get("verb") or "")
    if f == "unit_job_state":
        return live.unit_job_state([], "hermes-gateway.service")
    if f == "service_restart_sec":
        default = req.get("default")
        return live.service_restart_sec([], "hermes-gateway.service",
                                        float(default) if default is not None else 0.0)
    if f == "settled":
        state = req.get("state")
        return bool(live._settled(state if isinstance(state, dict) else {}))
    if f == "constants":
        return {
            "systemctl_restart_timeout_max": live._SYSTEMCTL_RESTART_TIMEOUT_MAX,
            "unit_job_hard_cap_s": float(live._UNIT_JOB_HARD_CAP_S),
            "unit_job_still_running": live.UNIT_JOB_STILL_RUNNING,
            "transitional_states": list(live._TRANSITIONAL_STATES),
        }
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
