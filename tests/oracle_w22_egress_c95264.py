#!/usr/bin/env python3
"""Differential oracle for hermes_cli/subcommands/egress.py
(gate w22_egress_c95264).

argparse and proxy_cli registration are the CLI seam. The live
`build_egress_parser` body runs against a recording subparsers object and a
fake `hermes_cli.proxy_cli` module (injected through `__import__`), then the
`_dispatch_egress` closure it installs is called for real with fake args.
"""
import ast
import builtins
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "subcommands" / "egress.py"
GATE = "w22_egress_c95264"

_tree = ast.parse(SRC.read_text(encoding="utf-8"))
_NODE = None
for _n in ast.walk(_tree):
    if isinstance(_n, (ast.FunctionDef, ast.AsyncFunctionDef)) and _n.name == "build_egress_parser":
        _n.decorator_list = []
        _NODE = _n
        break
if _NODE is None:  # pragma: no cover
    raise SystemExit("build_egress_parser not found in %s" % SRC)

WANTED = {"spec", "dispatch"}


class Parser:
    def __init__(self, name, **kw):
        self.name = name
        self.help = kw.get("help")
        self.description = kw.get("description")
        self.defaults = {}
        self.help_prints = 0
        self.registered = None

    def set_defaults(self, **kw):
        self.defaults.update(kw)

    def print_help(self):
        self.help_prints += 1


class Subparsers:
    def __init__(self):
        self.parser = None

    def add_parser(self, name, **kw):
        self.parser = Parser(name, **kw)
        return self.parser


def build():
    holder = Subparsers()
    registered = {"count": 0}

    class ProxyCli:
        def register_cli(self, parser):
            registered["count"] += 1
            parser.registered = True

    def fake_import(name, *_a, **_k):
        return type("Mod", (), {"proxy_cli": ProxyCli()})

    g = {"__builtins__": dict(builtins.__dict__, __import__=fake_import)}
    exec(compile(ast.Module(body=[_NODE], type_ignores=[]), str(SRC), "exec"), g)
    g["build_egress_parser"](holder)
    return holder.parser, registered["count"]


def spec():
    parser, _ = build()
    return {"name": parser.name, "help": parser.help, "description": parser.description}


def dispatch(has_sub, has_func, func_is_self):
    parser, _ = build()
    inner = parser.defaults["func"]

    class Args:
        pass

    args = Args()
    if has_sub:
        args.egress_command = "list"
    if has_func:
        args.func = inner if func_is_self else (lambda _a: "called")
    result = inner(args)
    called = result == "called"
    return {"action": "call" if called else "help", "help": bool(parser.help_prints)}


def expected(r):
    f = r.get("func")
    if f == "spec":
        return spec()
    if f == "dispatch":
        return dispatch(bool(r.get("has_sub")), bool(r.get("has_func")),
                        bool(r.get("func_is_self")))
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
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
