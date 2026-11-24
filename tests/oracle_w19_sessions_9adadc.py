#!/usr/bin/env python3
"""Differential oracle for hermes_cli/subcommands/sessions.py (gate w19_sessions_9adadc).

This module is almost entirely a parser *specification*: the C port carries the argparse tree as
a static table. The oracle rebuilds the same tree with the live build_sessions_parser against a
real argparse.ArgumentParser and normalises both sides to the same JSON shape:

    {"dest", "parent": [arg...], "subs": {name: {"args": [arg...], "children": null}}}

    arg = {opts, dest, action, nargs, default, required, type, metavar, choices}
"""
import argparse
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
GATE = "w19_sessions_9adadc"
if Q not in sys.path:
    sys.path.insert(0, Q)

from hermes_cli.subcommands.sessions import build_sessions_parser  # noqa: E402


def norm_default(v):
    if v is None:
        return None
    if v is argparse.SUPPRESS:
        return "__SUPPRESS__"
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float, str)):
        return v
    return {"__type__": type(v).__name__}


def norm_type(t):
    if t is None:
        return None
    if t in (bool, int, float, str):
        return t.__name__
    return getattr(t, "__name__", str(t))


def arg_spec(a):
    return {
        "opts": list(a.option_strings),
        "dest": a.dest,
        "action": type(a).__name__,
        "nargs": a.nargs if isinstance(a.nargs, (int, str)) else None,
        "default": norm_default(a.default),
        "required": bool(a.required),
        "type": norm_type(a.type),
        "metavar": a.metavar if isinstance(a.metavar, str) else None,
        "choices": [str(x) for x in a.choices] if isinstance(a.choices, (list, tuple)) else None,
    }


def plain_args(parser):
    return [arg_spec(a) for a in parser._actions
            if not isinstance(a, argparse._SubParsersAction)]


def build():
    root = argparse.ArgumentParser(prog="hermes")
    subs = root.add_subparsers(dest="cmd")
    build_sessions_parser(subs, cmd_sessions=lambda *a, **k: None)
    outer = [a for a in root._actions if isinstance(a, argparse._SubParsersAction)][0]
    sessions_parser = outer.choices["sessions"]
    inner = [a for a in sessions_parser._actions if isinstance(a, argparse._SubParsersAction)][0]
    spec = {"dest": inner.dest, "parent": plain_args(sessions_parser), "subs": {}}
    for name, p in inner.choices.items():
        spec["subs"][name] = {"args": plain_args(p), "children": None}
    return spec


SPEC = build()


def run_case(r):
    f = r.get("func")
    sub = r.get("sub")
    if f == "spec":
        return SPEC
    if f == "parent_args":
        return SPEC["parent"]
    if f == "dest":
        return SPEC["dest"]
    if f == "sub_names":
        return list(SPEC["subs"])
    if f == "has_sub":
        return sub in SPEC["subs"]
    if f == "arg_count":
        return len(SPEC["subs"][sub]["args"]) if sub in SPEC["subs"] else None
    if f == "args":
        return SPEC["subs"][sub]["args"] if sub in SPEC["subs"] else None
    if f == "arg":
        flag = r.get("flag")
        if sub not in SPEC["subs"]:
            return None
        for a in SPEC["subs"][sub]["args"]:
            if flag in a["opts"]:
                return a
        return None
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
        f = r.get("func")
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
