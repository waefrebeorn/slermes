#!/usr/bin/env python3
"""Differential oracle for agent/verify/runner.py (gate w26_runner_fdd9a0).

The runner's I/O is the seam and is replaced by fixed inputs while the live code
still runs:

  * ``subprocess.run`` -- answers the ``docker compose ps`` probe (a return code
    and its stdout/stderr), or raises FileNotFoundError / TimeoutExpired;
  * ``_compose_live_state_reason`` -- a fixed reason, so the live ``run_verify``
    builds its refusal PhaseResult and ``VerifyResult.to_dict`` verbatim.

Phase/readiness results are constructed as the real dataclasses and serialized
by their own ``to_dict``.
"""
import json
import subprocess
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_runner_fdd9a0"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from agent.verify import runner as live  # noqa: E402

WANTED = {"tail", "phase_result", "readiness_result", "verify_ok",
          "compose_live_state_reason", "verify_refusal", "constants"}


def tail(req):
    if "limit" in req:
        return live._tail(req.get("text") or "", req.get("limit"))
    return live._tail(req.get("text") or "")


def phase_result(req):
    return live.PhaseResult(
        phase=req.get("phase", ""),
        command=req.get("command", ""),
        exit_code=req.get("exit_code"),
        duration=req.get("duration", 0.0),
        output_tail=req.get("output_tail", ""),
        timed_out=bool(req.get("timed_out", False)),
    ).to_dict()


def _readiness(raw):
    if not isinstance(raw, dict):
        return None
    return live.ReadinessResult(
        url=raw.get("url", ""),
        ready=bool(raw.get("ready", False)),
        status_code=raw.get("status_code"),
        duration=raw.get("duration", 0.0),
        error=raw.get("error"),
        output_tail=raw.get("output_tail", ""),
    )


def readiness_result(req):
    return _readiness(req).to_dict()


def verify_ok(req):
    phases = [live.PhaseResult(
        phase=p.get("phase", ""),
        command=p.get("command", ""),
        exit_code=p.get("exit_code"),
        duration=p.get("duration", 0.0),
        output_tail=p.get("output_tail", ""),
        timed_out=bool(p.get("timed_out", False)),
    ) for p in (req.get("phases") or []) if isinstance(p, dict)]
    result = live.VerifyResult(recipe_name="x", phases=phases,
                               readiness=_readiness(req.get("readiness")))
    return result.ok


def compose_live_state_reason(req):
    mode = req.get("mode", "ok")

    def fake_run(argv, **kwargs):
        if mode == "missing":
            raise FileNotFoundError(2, "No such file or directory", "docker")
        if mode == "timeout":
            raise subprocess.TimeoutExpired(argv, 15)
        return SimpleNamespace(returncode=req.get("returncode", 0),
                               stdout=req.get("stdout", ""),
                               stderr=req.get("stderr", ""))

    saved = live.subprocess.run
    live.subprocess.run = fake_run
    try:
        return live._compose_live_state_reason(Path("."))
    finally:
        live.subprocess.run = saved


def verify_refusal(req):
    reason = req.get("reason", "")
    build = req.get("build")
    recipe = SimpleNamespace(
        name=req.get("recipe", "verify"), kind="compose",
        build=list(build) if isinstance(build, list) else [],
        bootstrap=[], test=[], start=None, port=None, readiness_path="/",
    )
    saved = live._compose_live_state_reason
    live._compose_live_state_reason = lambda root: reason
    try:
        result = live.run_verify(Path("."), recipe, phases=["build"])
        return result.to_dict()
    finally:
        live._compose_live_state_reason = saved


def constants():
    return {"phase_timeout": float(live.DEFAULT_PHASE_TIMEOUT),
            "ready_timeout": float(live.DEFAULT_READY_TIMEOUT),
            "tail_chars": live._TAIL_CHARS,
            "phase_order": list(live.PHASE_ORDER)}


DISPATCH = {
    "tail": tail,
    "phase_result": phase_result,
    "readiness_result": readiness_result,
    "verify_ok": verify_ok,
    "compose_live_state_reason": compose_live_state_reason,
    "verify_refusal": verify_refusal,
    "constants": lambda req: constants(),
}


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
            py = DISPATCH[r["func"]](r)
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
