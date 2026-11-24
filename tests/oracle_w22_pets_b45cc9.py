#!/usr/bin/env python3
"""Differential oracle for hermes_cli/subcommands/pets.py (gate w22_pets_b45cc9).

`build_pets_parser` does two observable things: it hands argparse a declaration
(``name``/``help``/``description``) and it runs the guarded
``hermes_cli.pets.register_cli(parser)`` wiring.  argparse and the pets module
are the host seam, so they are scripted here:

  * a recording ``subparsers`` object captures the ``add_parser`` keywords;
  * a fake ``hermes_cli.pets`` module either registers or raises;
  * a handler on the module's logger (``hermes_cli.main``) captures the one
    formatted debug record.

The expected answer is the live declaration text plus whether the bridge ran and
what the record said.  Nothing about the strings is transcribed by hand.
"""
import importlib.util
import json
import logging
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "subcommands" / "pets.py"
GATE = "w22_pets_b45cc9"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("pets_subcommand_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["pets_subcommand_live"] = live
_spec.loader.exec_module(live)

WANTED = {"spec", "build"}
LOGGER_NAME = "hermes_cli.main"


class RecordingSubparsers:
    def __init__(self):
        self.seen = {}

    def add_parser(self, name, **kwargs):
        self.seen["name"] = name
        self.seen["help"] = kwargs.get("help")
        self.seen["description"] = kwargs.get("description")
        return types.SimpleNamespace(name=name)


class Capture(logging.Handler):
    def __init__(self):
        super().__init__(level=logging.DEBUG)
        self.messages = []

    def emit(self, record):
        self.messages.append(record.getMessage())


def run(mode, message):
    logger = logging.getLogger(LOGGER_NAME)
    old_level, old_prop = logger.level, logger.propagate
    cap = Capture()
    logger.setLevel(logging.DEBUG)
    logger.propagate = False
    logger.addHandler(cap)

    subs = RecordingSubparsers()
    state = {"registered": False}
    fake = types.ModuleType("hermes_cli.pets")

    if mode == "error":
        def register_cli(parser, _message=message):
            raise Exception(_message)
    else:
        def register_cli(parser):
            state["registered"] = True

    fake.register_cli = register_cli
    previous = sys.modules.get("hermes_cli.pets")
    sys.modules["hermes_cli.pets"] = fake
    try:
        live.build_pets_parser(subs)
    finally:
        if previous is None:
            sys.modules.pop("hermes_cli.pets", None)
        else:
            sys.modules["hermes_cli.pets"] = previous
        logger.removeHandler(cap)
        logger.setLevel(old_level)
        logger.propagate = old_prop

    return {
        "name": subs.seen.get("name"),
        "help": subs.seen.get("help"),
        "description": subs.seen.get("description"),
        "registered": state["registered"],
        "log": cap.messages[-1] if cap.messages else None,
    }


def expected(r):
    if r.get("func") == "spec":
        full = run("ok", "")
        return {k: full[k] for k in ("name", "help", "description")}
    mode = r.get("register")
    message = r["message"] if "message" in r else ""
    return run("error" if mode == "error" else "ok", message)


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
