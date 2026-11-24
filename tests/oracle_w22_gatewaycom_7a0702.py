#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_command_errors.py
(gate w22_gatewaycom_7a0702).

The module is the exception -> guidance table `hermes gateway start/stop/
restart` prints. Nothing in it does I/O, so nothing is stubbed: the live
functions run against real exception instances built from the fixture.

  verb     -- live `_verb_for` over a `subprocess.CalledProcessError` whose
              `.cmd` is the fixture's cmd (a list or a string).
  explain  -- live `explain_service_failure` over the exception the fixture's
              `kind` names: SystemctlUnavailableError for "unavailable", a
              CalledProcessError carrying `returncode` for "cpe", and a plain
              ValueError for anything else (the unexplained path -> None).
"""
import importlib
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w22_gatewaycom_7a0702"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

live = importlib.import_module("hermes_cli.gateway_command_errors")

WANTED = {"verb", "explain"}


def cmd_of(r):
    """The fixture's cmd as the attribute the exception carries."""
    if "cmd" not in r:
        return None
    return r.get("cmd")


def rc_of(r):
    value = r.get("returncode")
    if value is None:
        return 0
    return int(value)


def expected(r):
    f = r.get("func")
    if f == "verb":
        return live._verb_for(live.subprocess.CalledProcessError(1, cmd_of(r)))
    if f == "explain":
        kind = r.get("kind")
        if kind == "unavailable":
            exc = live.SystemctlUnavailableError()
        elif kind == "cpe":
            exc = live.subprocess.CalledProcessError(rc_of(r), cmd_of(r))
        else:
            exc = ValueError(str(r.get("message") or ""))
        return live.explain_service_failure(exc)
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
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
