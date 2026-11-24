#!/usr/bin/env python3
"""Differential oracle for hermes_cli/subcommands/curator.py
(gate w22_curator_233719).

The live subcommand module is loaded by path and driven for real:

  spec   -- build_curator_parser is called against a real argparse tree through
            a recording proxy, so the name/aliases/help/description compared here
            are the ones argparse itself received.
  wiring -- the `except Exception` guard's outcome, decided by RAISING the real
            exception class named in the fixture (builtins lookup) and letting
            Python's own hierarchy answer, not a table.
  wiring + probe "broken_import" -- end to end: sys.modules["hermes_cli.curator"]
            is set to None, so `from hermes_cli.curator import register_cli`
            fails with the interpreter's own ImportError; the DEBUG record the
            module writes is captured and re-shaped. The fixture's exc fields are
            NOT consulted for this case.
"""
import argparse
import builtins
import importlib.util
import io
import json
import logging
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "subcommands" / "curator.py"
TARGET_MODULE = "hermes_cli.curator"
GATE = "w22_curator_233719"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("curator_subcommand_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["curator_subcommand_live"] = live
_spec.loader.exec_module(live)

build = live.build_curator_parser
WANTED = {"spec", "wiring"}
LOGGER = "hermes_cli.main"
PREFIX = "curator CLI wiring failed: "
_MISSING = object()


class RecordingSubparsers:
    """Delegates to the real subparsers and remembers what add_parser was given."""

    def __init__(self, real):
        self.real = real
        self.calls = []

    def add_parser(self, name, **kwargs):
        self.calls.append((name, kwargs))
        return self.real.add_parser(name, **kwargs)


def parser_spec():
    parser = argparse.ArgumentParser(prog="hermes")
    rec = RecordingSubparsers(parser.add_subparsers(dest="cmd"))
    build(rec)
    name, kwargs = rec.calls[0]
    return {"name": name,
            "aliases": list(kwargs.get("aliases") or []),
            "help": kwargs.get("help"),
            "description": kwargs.get("description")}


def outcome(exc_type, message):
    """Raise the real class and let Python's hierarchy decide where it lands."""
    cls = getattr(builtins, exc_type, Exception)
    try:
        raise cls(message)
    except Exception as exc:  # exactly the module's `except Exception`
        return {"wired": False,
                "log": {"logger": LOGGER, "level": "DEBUG",
                        "message": PREFIX + str(exc)}}
    except BaseException:  # noqa: BLE001 - escapes the module's guard
        return {"wired": False, "propagated": exc_type, "log": None}


def broken_import():
    saved = sys.modules.get(TARGET_MODULE, _MISSING)
    sys.modules[TARGET_MODULE] = None
    buf = io.StringIO()
    handler = logging.StreamHandler(buf)
    handler.setLevel(logging.DEBUG)
    handler.setFormatter(logging.Formatter("%(levelname)s %(name)s %(message)s"))
    logger = logging.getLogger(LOGGER)
    old_level, old_prop = logger.level, logger.propagate
    logger.addHandler(handler)
    logger.setLevel(logging.DEBUG)
    raised = None
    try:
        parser = argparse.ArgumentParser(prog="hermes")
        build(parser.add_subparsers(dest="cmd"))
    except BaseException as exc:  # noqa: BLE001 - would mean the guard failed
        raised = exc
    finally:
        logger.removeHandler(handler)
        logger.setLevel(old_level)
        logger.propagate = old_prop
        if saved is _MISSING:
            sys.modules.pop(TARGET_MODULE, None)
        else:
            sys.modules[TARGET_MODULE] = saved
    if raised is not None:
        return {"wired": False, "uncaught": type(raised).__name__}
    text = buf.getvalue()
    if PREFIX not in text:
        return {"wired": False, "log": None}
    message = text.split(PREFIX, 1)[1].strip()
    return {"wired": False,
            "log": {"logger": LOGGER, "level": "DEBUG", "message": PREFIX + message}}


def expected(r):
    f = r.get("func")
    if f == "spec":
        return parser_spec()
    if f == "wiring":
        if r.get("probe") == "broken_import":
            return broken_import()
        exc = r.get("exc")
        if not isinstance(exc, dict):
            return {"wired": True, "log": None}
        return outcome(exc.get("type", "Exception"), exc.get("message", ""))
    raise AssertionError("unknown func %r" % (f,))


def check_isolation():
    """The runner isolates HOME/SLERMES_HOME to one temp dir: assert it, don't assume."""
    home = os.environ.get("HOME")
    slermes = os.environ.get("SLERMES_HOME")
    same = bool(home and slermes and
                os.path.normcase(os.path.abspath(home)) ==
                os.path.normcase(os.path.abspath(slermes)))
    if not same or not os.path.isdir(home):
        print("HARNESS-ENV not isolated: HOME=%r SLERMES_HOME=%r" % (home, slermes),
              file=sys.stderr)
        sys.exit(2)


def main():
    check_isolation()
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
