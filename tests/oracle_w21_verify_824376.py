#!/usr/bin/env python3
"""Differential oracle for hermes_cli/subcommands/verify.py
(gate w21_verify_824376).

The live `build_verify_parser` is called with a real argparse subparsers
action, so the oracle compares against argparse's own objects:

* `spec`  -- the subparser argparse built: name/help/description, each action's
  dest, option strings, nargs, default, type, action class, choices and help,
  plus the handler default it binds;
* `parse` -- `vars()` of the namespace argparse returns for the fixture argv
  (the `cmd` key and the injected callback are dropped).

argparse exits 2 on a rejected argv; that exit is reported as the harness's
{"status":"error","code":2}.
"""
import argparse
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = Path("C:/Users/eman5/hermes-agent")
SRC = QUARRY / "hermes_cli" / "subcommands" / "verify.py"
GATE = "w21_verify_824376"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("verify_subcommand_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["verify_subcommand_live"] = live
_spec.loader.exec_module(live)

CMD_VERIFY = lambda namespace: None  # noqa: E731 - the injected handler


def build():
    parser = argparse.ArgumentParser(prog="hermes")
    subparsers = parser.add_subparsers(dest="cmd")
    live.build_verify_parser(subparsers, cmd_verify=CMD_VERIFY)
    help_text = None
    for choice in subparsers._choices_actions:
        if choice.dest == "verify":
            help_text = choice.help
    return parser, subparsers.choices["verify"], help_text


_PARSER, _VERIFY, _VERIFY_HELP = build()

_ACTIONS = {"_StoreTrueAction": "store_true", "_AppendAction": "append", "_StoreAction": "store"}
_TYPES = {int: "int", float: "float"}


def spec():
    defaults = {"func": "cmd_verify"} if _VERIFY._defaults.get("func") is CMD_VERIFY else {}
    args = []
    for action in _VERIFY._actions:
        if action.dest in ("help", "cmd"):
            continue
        args.append({
            "name": action.dest,
            "flags": list(action.option_strings) or [action.dest],
            "nargs": action.nargs,
            "default": action.default,
            "type": _TYPES.get(action.type),
            "action": _ACTIONS.get(type(action).__name__),
            "choices": list(action.choices) if action.choices is not None else None,
            "help": action.help,
        })
    return {"name": _VERIFY.prog.split()[-1],
            "help": _VERIFY_HELP,
            "description": _VERIFY.description,
            "arguments": args,
            "defaults": defaults}


def parse(argv):
    try:
        namespace = _PARSER.parse_args(["verify"] + list(argv))
    except SystemExit as exc:
        return {"status": "error", "code": int(exc.code or 0)}
    values = {k: v for k, v in vars(namespace).items() if k not in ("cmd", "func")}
    return {"status": "ok", "value": values}


def expected(r):
    if r.get("func") == "spec":
        return spec()
    if r.get("func") == "parse":
        return parse(r.get("argv") or [])
    raise AssertionError("unknown func %r" % (r.get("func"),))


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
        if not isinstance(r, dict) or r.get("func") not in ("spec", "parse"):
            continue
        cases += 1
        try:
            py = expected(r)
        except BaseException as exc:  # noqa: BLE001 - reported, never fatal
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
