#!/usr/bin/env python3
"""Differential oracle for hermes_cli/subcommands/fallback.py
(gate w21_fallback_689de3).

`build_fallback_parser(subparsers)` only ever *calls* argparse: `add_parser`,
`add_subparsers`, `add_argument`, `set_defaults`. The oracle hands it a
recording stand-in for that plumbing (the CLI seam) and runs the live function,
so every help string, alias and flag order in the expected tree came out of the
real source. The real `hermes_cli.fallback_cmd.cmd_fallback` is what
`set_defaults` receives, and its `__name__` is what the tree records.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "subcommands" / "fallback.py"
GATE = "w21_fallback_689de3"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("subcommands_fallback_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["subcommands_fallback_live"] = live
_spec.loader.exec_module(live)

WANTED = {"spec", "subcommand", "default_func"}


class Recorder:
    """The argparse seam: records the tree build_fallback_parser attaches."""

    def __init__(self, name=None, **kw):
        self.name = name
        self.kw = kw
        self.choices = []
        self.args = []
        self.sub_dest = None
        self.defaults = {}

    def add_parser(self, name, **kw):
        child = Recorder(name, **kw)
        self.choices.append(child)
        return child

    def add_subparsers(self, **kw):
        self.sub_dest = kw.get("dest")
        return self

    def add_argument(self, *flags, **kw):
        entry = {"flags": list(flags)}
        entry.update(kw)
        self.args.append(entry)

    def set_defaults(self, **kw):
        for k, v in kw.items():
            self.defaults[k] = getattr(v, "__name__", v)


def build():
    outer = Recorder()                      # the parent CLI's subparsers
    live.build_fallback_parser(outer)
    return outer.choices[0]                 # the `fallback` parser it attached


def to_dict(rec):
    d = {
        "prog": rec.name,
        "help": rec.kw.get("help"),
        "description": rec.kw.get("description"),
        "defaults": dict(rec.defaults),
    }
    if rec.sub_dest is None and not rec.choices:
        d["subparsers"] = None
    else:
        d["subparsers"] = {
            "dest": rec.sub_dest,
            "choices": [
                {"name": c.name, "aliases": c.kw.get("aliases") or [],
                 "help": c.kw.get("help"), "arguments": c.args}
                for c in rec.choices
            ],
        }
    return d


def find(root, name):
    if not isinstance(name, str) or not name:
        return None
    for c in root.choices:
        if name == c.name or name in (c.kw.get("aliases") or []):
            return {"name": c.name, "aliases": c.kw.get("aliases") or [],
                    "help": c.kw.get("help"), "arguments": c.args}
    return None


def expected(r):
    f = r.get("func")
    root = build()
    if f == "spec":
        return to_dict(root)
    if f == "subcommand":
        return find(root, r.get("name"))
    if f == "default_func":
        return root.defaults.get("func")
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
