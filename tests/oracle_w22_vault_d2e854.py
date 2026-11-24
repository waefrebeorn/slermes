#!/usr/bin/env python3
"""Differential oracle for hermes_cli/subcommands/vault.py (gate w22_vault_d2e854).

`build_vault_parser` runs live against a real `argparse` subparsers object, so
the add_parser kwargs, the derived `prog`, the help row and argparse's own
duplicate-name failure are all the library's. The only seam is the
`hermes_cli.vault` import: `register_cli` and `vault_command` are fake module
members (the real ones drive a live vault backend), and the call count is read
back off the fake.
"""
import argparse
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "subcommands" / "vault.py"
GATE = "w22_vault_d2e854"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("vault_submodule_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["vault_submodule_live"] = live
_spec.loader.exec_module(live)

WANTED = {"spec", "mount"}


class FakeVaultModule(types.ModuleType):
    def __init__(self):
        super().__init__("hermes_cli.vault")
        self.calls = 0

        def register_cli(parser):
            self.calls += 1
            parser.vault_registered = True

        def vault_command(_args):
            return None

        self.register_cli = register_cli
        self.vault_command = vault_command


def mount(parent_prog, taken):
    parent = argparse.ArgumentParser(prog=parent_prog if isinstance(parent_prog, str) else "")
    subparsers = parent.add_subparsers()
    names = taken if isinstance(taken, list) else []
    for name in names:
        if isinstance(name, str):
            subparsers.add_parser(name)
    fake = FakeVaultModule()
    saved = sys.modules.get("hermes_cli.vault")
    sys.modules["hermes_cli.vault"] = fake
    try:
        live.build_vault_parser(subparsers)
    except Exception as exc:  # noqa: BLE001 - argparse raises ValueError here
        return {"ok": 0, "error_type": type(exc).__name__, "error": str(exc)}
    finally:
        if saved is None:
            sys.modules.pop("hermes_cli.vault", None)
        else:
            sys.modules["hermes_cli.vault"] = saved
    child = subparsers.choices["vault"]
    help_row = next((a.help for a in subparsers._choices_actions if a.dest == "vault"), None)
    func = child.get_default("func")
    return {"ok": 1, "prog": child.prog, "name": "vault", "help": help_row,
            "description": child.description,
            "defaults_func": getattr(func, "__name__", None),
            "register_calls": fake.calls}


def spec():
    got = mount("hermes", [])
    return {"name": got["name"], "help": got["help"], "description": got["description"],
            "defaults_func": got["defaults_func"], "register_calls": got["register_calls"]}


def expected(r):
    f = r.get("func")
    if f == "spec":
        return spec()
    if f == "mount":
        return mount(r.get("parent_prog"), r.get("taken"))
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
