#!/usr/bin/env python3
"""Differential oracle for hermes_cli/subcommands/webapp.py
(gate w22_webapp_0bcaf2).

`build_webapp_parser` is pure argparse wiring, so the live parser is built
here (with a throwaway handler) and then introspected for the contract:

  spec   -- every registered action's flags/dest/action/default/help, the
            mutually-exclusive group, and the parser description.
  parse  -- the live parser's own verdict on argv: the resulting namespace, or
            the SystemExit + the last stderr line argparse printed (prefix
            stripped). A help request exits 0 and is reported as "help".
"""
import argparse
import contextlib
import importlib.util
import io
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "subcommands" / "webapp.py"
GATE = "w22_webapp_0bcaf2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("webapp_subcommand_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["webapp_subcommand_live"] = live
_spec.loader.exec_module(live)

WANTED = {"spec", "parse"}


def handler(_args):
    return None


def build():
    parent = argparse.ArgumentParser(prog="hermes")
    subs = parent.add_subparsers(dest="command")
    live.build_webapp_parser(subs, cmd_webapp=handler)
    return subs.choices["webapp"]


PARSER = build()


def spec():
    options = []
    for a in PARSER._actions:
        if isinstance(a, argparse._HelpAction):
            continue
        options.append({
            "flags": list(a.option_strings),
            "dest": a.dest,
            "action": "store_true" if isinstance(a, argparse._StoreTrueAction) else type(a).__name__,
            "default": a.default,
            "help": a.help,
        })
    groups = [[g.option_strings[0] for g in grp._group_actions]
              for grp in PARSER._mutually_exclusive_groups]
    return {
        "prog_suffix": PARSER.prog.split()[-1],
        "description": PARSER.description,
        "options": options,
        "exclusive": groups,
        "func": "cmd_webapp",
    }


def parse(argv):
    out, err = io.StringIO(), io.StringIO()
    try:
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            ns = PARSER.parse_args([str(a) for a in argv])
    except SystemExit as exc:
        code = 0 if exc.code is None else exc.code
        if code == 0:
            return {"ok": False, "error": "help", "exit": 0}
        lines = [l for l in err.getvalue().splitlines() if l.strip()]
        msg = lines[-1] if lines else ""
        marker = "error: "
        if marker in msg:
            msg = msg.split(marker, 1)[1]
        return {"ok": False, "error": msg, "exit": code}
    values = {k: v for k, v in vars(ns).items() if k != "func"}
    return {"ok": True, "values": values, "func": callable(getattr(ns, "func", None))}


def expected(r):
    f = r.get("func")
    if f == "spec":
        return spec()
    if f == "parse":
        flags = r.get("flags")
        if not isinstance(flags, list):
            flags = []
        return parse(flags)
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
