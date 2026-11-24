#!/usr/bin/env python3
"""Differential oracle for hermes_cli/subcommands/migrate.py (gate
w23_migrate_73e740).

``build_migrate_parser`` only declares an argparse parser, so the live oracle
runs it for real against a real ``add_subparsers`` and then reads the parser
tree back: each subcommand's help/description, every declared option (in
declaration order) with its dest, option strings, action class, default, nargs,
metavar and help, the subcommand menu, and the handler defaults.

Two seams:

  * ``hermes_cli.migrate`` is the implementation module the parser imports its
    three handler callables from (it opens config, installs plugins, rewrites
    files). The parser only needs the objects ``set_defaults`` names, so it is a
    stub of the three functions;
  * everything else -- including the agent-importers ``IMPORT_SOURCES``
    catalogue that decides which agent subcommands exist and what they are
    called -- is the live module.
"""
import argparse
import importlib
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w23_migrate_73e740"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))


def _named(name):
    def handler(*_args, **_kwargs):
        return None

    handler.__name__ = name
    return handler


_IMPL = types.ModuleType("hermes_cli.migrate")
_IMPL.cmd_migrate = _named("cmd_migrate")
_IMPL.cmd_migrate_xai = _named("cmd_migrate_xai")
_IMPL.cmd_migrate_agent = _named("cmd_migrate_agent")
sys.modules["hermes_cli.migrate"] = _IMPL

live = importlib.import_module("hermes_cli.subcommands.migrate")

_ROOT = argparse.ArgumentParser(prog="hermes")
_ROOT_SUBS = _ROOT.add_subparsers(dest="command")
live.build_migrate_parser(_ROOT_SUBS)

MIGRATE = _ROOT_SUBS.choices["migrate"]

WANTED = {"surface", "subcommand_names"}

ACTION_NAMES = {
    argparse._StoreTrueAction: "store_true",
    argparse._StoreAction: "store",
    argparse._AppendAction: "append",
    argparse._SubParsersAction: "subparsers",
}


def _norm(value):
    return getattr(value, "__name__", value) if callable(value) else value


def _option(action):
    return {
        "dest": action.dest,
        "option_strings": list(action.option_strings),
        "action": ACTION_NAMES.get(type(action), type(action).__name__),
        "default": _norm(action.default),
        "nargs": action.nargs,
        "metavar": action.metavar,
        "help": action.help,
    }


def _subparsers(parser):
    for action in parser._actions:
        if isinstance(action, argparse._SubParsersAction):
            return action
    return None


def _menu(action):
    help_by_dest = {meta.dest: meta.help for meta in action._choices_actions}
    return [{"name": name, "help": help_by_dest.get(name)} for name in action.choices]


def surface(req):
    raw = req.get("path")
    if raw is None:
        names = []
    elif isinstance(raw, list) and all(isinstance(name, str) for name in raw):
        names = raw
    else:
        return {"error": "unknown_command"}

    parser = MIGRATE
    help_text = next((meta.help for meta in _ROOT_SUBS._choices_actions
                      if meta.dest == "migrate"), None)
    action = _subparsers(parser)
    for name in names:
        if action is None or name not in action.choices:
            return {"error": "unknown_command"}
        help_text = next((meta.help for meta in action._choices_actions
                          if meta.dest == name), None)
        parser = action.choices[name]
        action = _subparsers(parser)

    options = [_option(a) for a in parser._actions
               if not isinstance(a, (argparse._HelpAction, argparse._SubParsersAction))]
    return {
        "help": help_text,
        "description": parser.description,
        "options": options,
        "subcommands": _menu(action) if action is not None else None,
        "defaults": {key: _norm(value) for key, value in parser._defaults.items()},
    }


def subcommand_names(_req):
    return list(_subparsers(MIGRATE).choices)


def expected(r):
    f = r.get("func")
    if f == "surface":
        return surface(r)
    if f == "subcommand_names":
        return subcommand_names(r)
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
            print("MISMATCH", r.get("func"), "case=", r.get("case"),
                  "live-error=", repr(exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH", r.get("func"), "case=", r.get("case"),
                  "py=", repr(py)[:600], "c=", repr(got)[:600], file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
