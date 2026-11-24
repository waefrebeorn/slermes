#!/usr/bin/env python3
"""Differential oracle for hermes_cli/session_reset_retirement.py
(gate w24_sessionres_41c66a).

The live module is imported and its two pure functions are called on the JSON
the harness read: ``retired_reset_policy`` over a config mapping, and
``format_notice`` over a path/mode pair.  ``reset_plugin_enabled`` is the seam
-- it scans this process's plugin manager and is not compared.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_sessionres_41c66a"
PYFILE = QUARRY / "hermes_cli/session_reset_retirement.py"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# The module imports only ``typing``; loading its file keeps hermes_cli's
# package __init__ (and the CLI it pulls in) out of the way.
_spec = importlib.util.spec_from_file_location("live_session_reset_retirement", PYFILE)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

WANTED = {"retired_reset_policy", "format_notice", "constants"}


def retired(r):
    policy = live.retired_reset_policy(r.get("config"))
    return None if policy is None else [policy[0], policy[1]]


def notice(r):
    return live.format_notice(r.get("path"), r.get("mode"))


def constants(r):
    return {
        "plugin_name": live.PLUGIN_NAME,
        "timed_modes": sorted(live._TIMED_MODES),
    }


def normalise(func, value):
    """Compare containers the way Python's == would, not by JSON identity."""
    if func == "constants" and isinstance(value, dict):
        modes = value.get("timed_modes")
        if isinstance(modes, list):
            value = dict(value, timed_modes=sorted(modes))
    return value


DISPATCH = {
    "retired_reset_policy": retired,
    "format_notice": notice,
    "constants": constants,
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = normalise(r["func"], r.get("out"))
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
