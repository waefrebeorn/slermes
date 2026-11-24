#!/usr/bin/env python3
"""Differential oracle for hermes_cli/main_profile_recovery.py
(gate w21_mainprofil_8ddd4c).

The live module imports only argparse/contextlib/io at module level; its real
dependencies are reached lazily (`hermes_cli._parser.command_argv` and
`hermes_cli.subcommands.uninstall.build_uninstall_parser`), so both functions
run unmodified here -- the uninstall probe builds the genuine subparser, with
argparse's own abbreviation and mutual-exclusion rules.
"""
import importlib
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w21_mainprofil_8ddd4c"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

live = importlib.import_module("hermes_cli.main_profile_recovery")
command_argv = importlib.import_module("hermes_cli._parser").command_argv


def expected(r):
    argv = r.get("argv")
    argv = argv if isinstance(argv, list) else []
    f = r.get("func")
    if f == "cmd":
        return command_argv(argv)
    if f == "stale":
        return live.is_stale_profile_recovery_command(argv)
    return "UNKNOWN_FUNC"


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
        if not isinstance(r, dict) or r.get("func") not in ("cmd", "stale"):
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001 - reported as the harness shapes it
            py = {"status": "error", "type": type(exc).__name__, "message": str(exc)}
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
