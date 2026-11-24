#!/usr/bin/env python3
"""Differential oracle for hermes_cli/subcommands/clipboard.py
(gate w22_clipboard_1504a0).

`build_clipboard_parser` mutates an argparse tree and touches no device, gateway
or session store, so the tree is the behaviour: the live function is handed a
*recording* subparsers object and every add_parser / add_subparsers /
add_argument / set_defaults call is rendered to the same JSON the C side builds.
`_shared.add_json_flag` is real (its whole body is one add_argument call).

`_cmd_clipboard` is a lazy import that forwards the parsed namespace. The
clipboard backend module is the device seam, so it is stubbed with a recorder
that returns what it was handed; nothing about the real clipboard is reached.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "subcommands" / "clipboard.py"
GATE = "w22_clipboard_1504a0"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("clipboard_subcommand_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["clipboard_subcommand_live"] = live
_spec.loader.exec_module(live)

WANTED = {"parser", "cmd"}


class RecParser:
    def __init__(self, name, **kwargs):
        self.name = name
        self.kwargs = dict(kwargs)
        self.arguments = []
        self.subparsers = None
        self.actions = None
        self.defaults = None

    def add_argument(self, *names, **kwargs):
        rec = {"names": [str(n) for n in names]}
        for k in ("required", "metavar", "action", "help"):
            if k in kwargs:
                rec[k] = kwargs[k]
        self.arguments.append(rec)

    def add_subparsers(self, **kwargs):
        self.subparsers = {k: kwargs[k] for k in ("dest", "required") if k in kwargs}
        self.actions = RecActions()
        return self.actions

    def set_defaults(self, **kwargs):
        self.defaults = {k: getattr(v, "__name__", str(v)) for k, v in kwargs.items()}


class RecActions:
    def __init__(self):
        self.children = []

    def add_parser(self, name, **kwargs):
        child = RecParser(name, **kwargs)
        self.children.append(child)
        return child


def render(parser):
    out = {"name": parser.name, "arguments": parser.arguments}
    for key in ("help", "description"):
        if key in parser.kwargs:
            out[key] = parser.kwargs[key]
    if parser.subparsers is not None:
        out["subparsers"] = parser.subparsers
    if parser.defaults is not None:
        out["defaults"] = parser.defaults
    out["children"] = [render(c) for c in (parser.actions.children if parser.actions else [])]
    return out


def parser_tree():
    root = RecActions()
    live.build_clipboard_parser(root)
    return {"parsers": [render(c) for c in root.children]}


def cmd_case(args):
    # `_cmd_clipboard` forwards the namespace verbatim; whatever the caller
    # handed in is what the backend receives (no coercion happens in the seam).
    namespace = args
    holder = {}

    def fake_cmd(received):
        holder["args"] = received
        return {"delegated_to": "hermes_cli.clipboard_cmd.cmd_clipboard",
                "args": received,
                "same": received is namespace}

    stub = types.ModuleType("hermes_cli.clipboard_cmd")
    stub.cmd_clipboard = fake_cmd
    prev = sys.modules.get("hermes_cli.clipboard_cmd")
    sys.modules["hermes_cli.clipboard_cmd"] = stub
    try:
        return live._cmd_clipboard(namespace)
    finally:
        if prev is None:
            sys.modules.pop("hermes_cli.clipboard_cmd", None)
        else:
            sys.modules["hermes_cli.clipboard_cmd"] = prev


def expected(r):
    f = r.get("func")
    if f == "parser":
        return parser_tree()
    if f == "cmd":
        return cmd_case(r.get("args"))
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
