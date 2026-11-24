#!/usr/bin/env python3
"""Differential oracle for hermes_cli/tool_availability_notices.py (gate w25_toolavaila_80adee).

The live module is loaded straight from the quarry by path: its own imports are
stdlib only, and the two seams it touches (``check_tool_availability`` and
``toolsets.resolve_toolset``) are inputs here -- the ``unavailable`` rows and a
name->tools map.  Three pure functions are compared, plus the literal tables:

  * ``filter_to_enabled_toolsets`` over a configured selection and its expansion;
  * ``tool_availability_warning_lines`` over the rows and the terminal state;
  * ``_terminal_line`` over a backend and a reason.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_toolavaila_80adee"
MODULE = QUARRY / "hermes_cli" / "tool_availability_notices.py"


def load_live():
    spec = importlib.util.spec_from_file_location("live_tool_availability_notices", MODULE)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


live = load_live()

WANTED = {"filter_enabled", "warning_lines", "terminal_line", "constants"}


def filter_enabled(r):
    unavailable = r.get("unavailable")
    resolve_map = r.get("resolve")

    def resolve(name):
        # A name the registry does not know raises live; the caller swallows it.
        if not isinstance(resolve_map, dict):
            raise KeyError(name)
        return resolve_map[name]

    return live.filter_to_enabled_toolsets(
        unavailable if isinstance(unavailable, list) else [], r.get("enabled"), resolve)


def warning_lines(r):
    unavailable = r.get("unavailable")
    return live.tool_availability_warning_lines(
        unavailable if isinstance(unavailable, list) else [],
        terminal_reason=r.get("terminal_reason"),
        terminal_backend=r.get("terminal_backend", "local"))


def terminal_line(r):
    return live._terminal_line(r.get("backend", "local"), r.get("reason"))


def constants():
    notices = live._MULTI_PROVIDER_NOTICES
    return {
        "generic_footer": live._GENERIC_FOOTER,
        "multi_provider_keys": sorted(notices),
        "web_notice": notices["web"],
    }


def expected(r):
    f = r.get("func")
    if f == "filter_enabled":
        return filter_enabled(r)
    if f == "warning_lines":
        return warning_lines(r)
    if f == "terminal_line":
        return terminal_line(r)
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
