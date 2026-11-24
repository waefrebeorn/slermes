#!/usr/bin/env python3
"""Differential oracle for hermes_cli/subcommands/secrets.py
(gate w21_secrets_95fd41).

The live parser builder runs against a real argparse tree. Both backend modules
it imports lazily (`hermes_cli.secrets_cli`, `hermes_cli.onepassword_secrets_cli`)
are stubbed for a host reason: importing them loads cryptography's native
extension, which this host refuses with
`ImportError: DLL load failed: Access is denied` (the `_rust` module). Their
register_cli only installs per-backend flags + a handler that is outside this
module, so the stub records the handler default and returns the fixture's exit
code; `_dispatch_secrets` itself is the real closure.
"""
import argparse
import importlib.util
import io
import json
import sys
import types
from contextlib import redirect_stdout
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "subcommands" / "secrets.py"
GATE = "w21_secrets_95fd41"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import hermes_cli  # noqa: E402

RC = {"value": 0}


def _register_cli(parser):
    parser.set_defaults(func=lambda args: RC["value"])


def _stub_backends():
    for name in ("hermes_cli.secrets_cli", "hermes_cli.onepassword_secrets_cli"):
        mod = types.ModuleType(name)
        mod.register_cli = _register_cli
        sys.modules[name] = mod
        setattr(hermes_cli, name.rsplit(".", 1)[1], mod)


def load():
    spec = importlib.util.spec_from_file_location("secrets_live", SRC)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["secrets_live"] = mod
    spec.loader.exec_module(mod)
    return mod


_stub_backends()
LIVE = load()


def build():
    root = argparse.ArgumentParser(prog="hermes")
    root_sub = root.add_subparsers()
    LIVE.build_secrets_parser(root_sub)
    outer = [a for a in root._actions if isinstance(a, argparse._SubParsersAction)][0]
    command = list(outer.choices.values())[0]
    action = [a for a in command._actions if isinstance(a, argparse._SubParsersAction)][0]
    return command, action


def dump_parser():
    command, action = build()
    helps = {c.dest: c.help for c in action._choices_actions}
    choices = [c.dest for c in action._choices_actions]
    aliases, seen = {}, {}
    for name, child in action.choices.items():
        if id(child) not in seen:
            seen[id(child)] = name
        else:
            aliases[name] = seen[id(child)]
    descriptions = {name: child.description for name, child in action.choices.items()
                    if name in choices}
    func = command._defaults.get("func")
    return {
        "command": command.prog.split()[-1] if command.prog else "secrets",
        "help": _OUTER_HELP[0],
        "description": command.description,
        "dest": action.dest,
        "choices": choices,
        "aliases": aliases,
        "helps": helps,
        "descriptions": descriptions,
        "func": func.__name__ if callable(func) else None,
    }


_OUTER_HELP = [None]


def _capture_outer_help():
    root = argparse.ArgumentParser(prog="hermes")
    root_sub = root.add_subparsers()
    LIVE.build_secrets_parser(root_sub)
    for act in root._actions:
        if isinstance(act, argparse._SubParsersAction):
            for choice in act._choices_actions:
                _OUTER_HELP[0] = choice.help
                return


def expected(r):
    if r.get("func") == "parser":
        _capture_outer_help()
        return dump_parser()
    command, action = build()
    sub = r.get("sub")
    RC["value"] = int(r.get("handler_rc") or 0)
    dispatch = command._defaults["func"]
    if sub is None:
        args = argparse.Namespace(secrets_command=None)
    else:
        args = command.parse_args([sub])
    buf = io.StringIO()
    with redirect_stdout(buf):
        rc = dispatch(args)
    return {"rc": rc, "printed_help": buf.getvalue() != ""}


def main():
    cases = mism = 0
    wanted = {"parser", "dispatch"}
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
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
