#!/usr/bin/env python3
"""Differential oracle for hermes_cli/subcommands/worktree.py (w20_worktree_959719).

The live ``build_worktree_parser`` is run against a real ``argparse`` subparsers
action, so the compared spec is argparse's own view of the attached parser
(subcommand names, aliases, help text, options, dests, types, defaults).  The
dispatch aliasing (``ls``/``audit`` -> ``list``) is compared through the live
``_dispatch_worktree`` with ``hermes_cli.worktree_cmd`` stubbed.
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


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


if "hermes_cli" not in sys.modules:
    _mk("hermes_cli")
_mk("hermes_cli.subcommands")

sp = importlib.util.spec_from_file_location(
    "w20_worktree_959719_live", os.path.join(Q, "hermes_cli", "subcommands", "worktree.py"))
live = importlib.util.module_from_spec(sp)
sys.modules["w20_worktree_959719_live"] = live
sp.loader.exec_module(live)

CALLS = []
_mk("hermes_cli.worktree_cmd",
    cmd_worktree=lambda args: (CALLS.append(args), args.worktree_action)[1])


def build():
    root = argparse.ArgumentParser(prog="hermes", add_help=False)
    sub = root.add_subparsers(dest="command")
    live.build_worktree_parser(sub)
    return root, sub


def action_spec(a):
    name = type(a).__name__
    if name.startswith("_"):
        name = name[1:]
    if name.endswith("Action"):
        name = name[:-6]
    name = "".join(("_" + c.lower()) if c.isupper() else c for c in name).lstrip("_")
    return {
        "flags": list(a.option_strings),
        "dest": a.dest,
        "action": name,
        "nargs": a.nargs,
        "default": a.default,
        "type": a.type.__name__ if a.type is not None else None,
        "metavar": a.metavar,
        "help": a.help,
    }


def spec():
    root, sub = build()
    # The single subparsers action attached by build_worktree_parser.
    sa = [a for a in root._actions if isinstance(a, argparse._SubParsersAction)][-1]
    worktree = sa.choices["worktree"]
    inner = [a for a in worktree._actions if isinstance(a, argparse._SubParsersAction)][0]
    subcommands, seen = [], set()
    for name, parser in inner.choices.items():
        if id(parser) in seen:
            subcommands[-1]["aliases"].append(name)
            continue
        seen.add(id(parser))
        subcommands.append({
            "name": name,
            "aliases": [],
            "help": parser.description if parser.description is not None else
                    (inner._choices_actions and None),
            "options": [action_spec(a) for a in parser._actions
                        if not isinstance(a, argparse._HelpAction)],
        })
        # add_parser(help=...) lands on the pseudo-action, not on parser.description.
        for ca in inner._choices_actions:
            if ca.dest == name:
                subcommands[-1]["help"] = ca.help
    return {
        "help": worktree.description and None,
        "root_help": _help_of(sa),
        "root_description": worktree.description,
        "dest": inner.dest,
        "subcommands": subcommands,
    }


def _help_of(sa):
    for ca in sa._choices_actions:
        if ca.dest == "worktree":
            return ca.help
    return None


def dispatch(action, extra=None):
    root, _sub = build()
    argv = ["worktree", action] + (extra or [])
    args = root.parse_args(argv)
    return args.func(args)


def expected(r):
    f = r.get("func")
    if f == "spec":
        s = spec()
        s.pop("help", None)
        return s
    if f in ("dispatch", "canonical_action"):
        action = r.get("action")
        if not isinstance(action, str):
            # The harness reads the field as text; a non-string never reaches argparse.
            return ""
        if action in ("ls", "audit", "list", "prune", "restore"):
            return dispatch(action, r.get("extra"))
        # argparse rejects anything else before _dispatch_worktree runs.
        return "list" if action in ("ls", "audit") else action
    raise AssertionError("unknown func %r" % (f,))


if __name__ == "__main__" and "--dump" in sys.argv:
    print(json.dumps(spec(), indent=1))
    sys.exit(0)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_worktree_959719: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
