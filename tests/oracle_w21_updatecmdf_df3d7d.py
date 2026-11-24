#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_cmd_fleet_checkout.py
(gate w21_updatecmdf_df3d7d).

`checkout_contains` reaches exactly two things outside itself: the build
identity (`hermes_cli.build_info.get_code_identity`) and the git probe
(`hermes_cli.update_custody.run_git`, with `hermes_cli.update_cmd._m` supplying
PROJECT_ROOT). Those are the seams the oracle binds per case; the function body
itself is the live source.

Canonical `out` is 0/1. A probe recorded as {"error": ...} raises out of
run_git, and an identity that is not a dict raises AttributeError before the
try block -- the port reads both as the module's fail-closed False.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "update_cmd_fleet_checkout.py"
GATE = "w21_updatecmdf_df3d7d"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("update_cmd_fleet_checkout_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["update_cmd_fleet_checkout_live"] = live
_spec.loader.exec_module(live)

import hermes_cli.build_info as build_info  # noqa: E402
import hermes_cli.update_custody as custody  # noqa: E402   (git seam)
import hermes_cli.update_cmd as update_cmd  # noqa: E402   (PROJECT_ROOT seam)

WANTED = {"contains"}


class Result:
    def __init__(self, returncode):
        self.returncode = returncode
        self.stdout = ""
        self.stderr = ""


class Root:
    PROJECT_ROOT = QUARRY


def expected(r):
    if r.get("func") != "contains":
        raise AssertionError("unknown func %r" % (r.get("func"),))
    identity = r.get("identity")
    probe = r.get("probe")
    orig_identity = build_info.get_code_identity
    orig_git = custody.run_git
    orig_m = update_cmd._m
    build_info.get_code_identity = lambda: identity
    update_cmd._m = lambda: Root()

    def fake_git(_cmd, _args, **_kw):
        if not isinstance(probe, dict):
            raise RuntimeError("no probe recorded")
        if probe.get("error") is not None:
            raise RuntimeError(probe["error"])
        return Result(probe.get("returncode"))

    custody.run_git = fake_git
    try:
        return 1 if live.checkout_contains(r.get("sha") or "") else 0
    except AttributeError:
        return 0      # identity was not a dict: out of contract, fail-closed
    finally:
        build_info.get_code_identity = orig_identity
        custody.run_git = orig_git
        update_cmd._m = orig_m


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
