#!/usr/bin/env python3
"""Differential oracle for hermes_cli/subcommands/connectors.py
(gate w21_connectors_0e0582).

build_connectors_parser is executed against a real argparse parser tree, so the
live module builds the surface; `hermes_cli.connectors_cmd` (the command body,
which talks to Nous Portal) is stubbed. The parser is then dumped to the same
canonical shape the C port carries: every subcommand, its aliases, its help and
every argument's dest/options/nargs/required/action/default/help.
"""
import argparse
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def cmd_connectors(args):  # the real body lives in hermes_cli.connectors_cmd
    return None


for _name, _path in (("hermes_cli", Q + "/hermes_cli"),
                     ("hermes_cli.subcommands", Q + "/hermes_cli/subcommands")):
    if _name not in sys.modules:
        _pkg = types.ModuleType(_name)
        _pkg.__path__ = [_path]
        sys.modules[_name] = _pkg
_cmd = types.ModuleType("hermes_cli.connectors_cmd")
_cmd.__path__ = []
_cmd.cmd_connectors = cmd_connectors
sys.modules["hermes_cli.connectors_cmd"] = _cmd

SRC = Q + "/hermes_cli/subcommands/connectors.py"
_spec = importlib.util.spec_from_file_location("w21_conn_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["w21_conn_live"] = live
_spec.loader.exec_module(live)


def nargs_of(action):
    if isinstance(action.nargs, str):
        return action.nargs
    if action.nargs == 0:
        return "0"
    if action.nargs is None:
        return None
    return str(action.nargs)


def dump_args(parser):
    out = []
    for action in parser._actions:
        if isinstance(action, argparse._HelpAction):
            continue
        out.append({"dest": action.dest,
                    "options": list(action.option_strings),
                    "nargs": nargs_of(action),
                    "required": bool(action.required),
                    "action": type(action).__name__,
                    "default": action.default,
                    "help": action.help})
    return out


def build():
    """The live parser tree, plus (subparsers action, top-level help)."""
    parser = argparse.ArgumentParser(prog="hermes")
    sub = parser.add_subparsers(dest="top")
    live.build_connectors_parser(sub)
    child = sub._name_parser_map["connectors"]
    top_help = {ca.dest: ca.help for ca in sub._choices_actions}.get("connectors")
    acts = [a for a in child._actions if isinstance(a, argparse._SubParsersAction)][0]
    return child, acts, top_help


def dump_spec():
    child, acts, top_help = build()
    helps = {ca.dest: ca.help for ca in acts._choices_actions}
    order, seen = [], {}
    for name, p in acts._name_parser_map.items():
        if id(p) in seen:
            continue
        seen[id(p)] = name
        order.append((name, p))
    subs = []
    for name, p in order:
        aliases = [k for k, v in acts._name_parser_map.items() if v is p and k != name]
        subs.append({"name": name, "aliases": aliases, "help": helps.get(name),
                     "arguments": dump_args(p)})
    func = child._defaults.get("func")
    return {"name": "connectors",
            "help": top_help,
            "description": child.description,
            "action_dest": acts.dest,
            "func": func.__name__ if func is not None else None,
            "subcommands": subs}


def dump_lookup(token):
    child, acts, _top = build()
    if not isinstance(token, str) or token not in acts._name_parser_map:
        return None
    parser = acts._name_parser_map[token]
    name = next(k for k, v in acts._name_parser_map.items() if v is parser)
    aliases = [k for k, v in acts._name_parser_map.items() if v is parser and k != name]
    required, flags, positionals = [], [], []
    for action in parser._actions:
        if isinstance(action, argparse._HelpAction):
            continue
        if action.option_strings:
            flags.extend(action.option_strings)
            if action.required:
                required.append(action.option_strings[0])
        else:
            positionals.append(action.dest)
            if action.required:
                required.append(action.dest)
    return {"name": name, "aliases": aliases, "required": required,
            "flags": flags, "positionals": positionals}


def expected(r):
    f = r.get("func")
    if f == "env_probe":
        sle = os.environ.get("SLERMES_HOME") or ""
        home = os.environ.get("HOME") or ""
        return {"sler": bool(sle), "home": bool(home), "same": bool(sle) and sle == home}
    if f == "spec":
        return dump_spec()
    if f == "lookup":
        return dump_lookup(r.get("token"))
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"spec", "lookup", "env_probe"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
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
    print("ORACLE w21_connectors_0e0582: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
