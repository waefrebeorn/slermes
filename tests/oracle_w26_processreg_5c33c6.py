#!/usr/bin/env python3
"""Differential oracle for tools/process_registry_output_log.py (gate w26_processreg_5c33c6).

The live module is imported as-is and only its value decisions are compared.
Every file-system seam is turned into a fixed input: ``output_dir`` is patched
to the request's directory before ``adoptable_log`` runs, and the profile home
is handed to ``output_dir`` through ``get_hermes_home``.  No worker, no
directory creation, no glob sweep, no descriptor, no tail loop starts.
"""
import json
import pathlib
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_processreg_5c33c6"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tools import process_registry_output_log as live  # noqa: E402

WANTED = {"output_dir", "adoptable_log", "exit_path", "record_exit_command", "constants"}

# The module is written for the POSIX path flavour (``record_exit_command`` even
# calls ``as_posix()``); the gate runs on a Windows host, so the live module's
# ``Path`` global is pinned to PurePosixPath.  That is the seam: the values are
# the upstream's own, computed with the path class the upstream assumes.
POSIX = pathlib.PurePosixPath
live.Path = POSIX


def output_dir(req):
    home = req.get("hermes_home")
    live.get_hermes_home = lambda: POSIX(home)
    return str(live.output_dir())


def adoptable_log(req):
    live.output_dir = lambda: POSIX(req.get("output_dir") or "")
    return live.adoptable_log(req.get("path"))


def exit_path(req):
    return str(live._exit_path(req.get("log")))


def record_exit_command(req):
    return live.record_exit_command(POSIX(req.get("log")), req.get("command"))


def constants():
    return {"tail_seconds": float(live._TAIL_SECONDS),
            "final_drain_max_bytes": float(live._FINAL_DRAIN_MAX_BYTES)}


def expected(r):
    f = r.get("func")
    if f == "output_dir":
        return output_dir(r)
    if f == "adoptable_log":
        return adoptable_log(r)
    if f == "exit_path":
        return exit_path(r)
    if f == "record_exit_command":
        return record_exit_command(r)
    if f == "constants":
        return constants()
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
