#!/usr/bin/env python3
"""Differential oracle for hermes_cli/subcommands/pause.py (gate w24_pause_6e1faf).

The live ``cmd_pause`` / ``cmd_resume`` are called with a fake ``agent.estop``
module installed in ``sys.modules`` -- engage()/get_state()/is_engaged()/
disengage()/sentinel_path() are the module's own seams, and what is compared is
everything the command prints plus its return code.  ``build_pause_parser`` is
driven on a throwaway argparse parser, so the help, description and --reason
default are the strings the shipped CLI really installs.

No sentinel is written and no process state is read: the estop module never
exists on disk for this oracle.
"""
import argparse
import contextlib
import io
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_pause_6e1faf"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli.subcommands import pause as live  # noqa: E402

WANTED = {"pause_output", "resume_output", "parser_spec"}


def fake_estop(engaged, state, path, disengaged):
    pkg = types.ModuleType("agent")
    pkg.__path__ = []
    est = types.ModuleType("agent.estop")
    est.engage = lambda reason=None: path
    est.get_state = lambda: state
    est.is_engaged = lambda: engaged
    est.disengage = lambda: disengaged
    est.sentinel_path = lambda: path
    pkg.estop = est
    return {"agent": pkg, "agent.estop": est}


@contextlib.contextmanager
def estop(engaged=False, state=None, path="", disengaged=False):
    saved = {k: sys.modules.get(k) for k in ("agent", "agent.estop")}
    sys.modules.update(fake_estop(engaged, state, path, disengaged))
    try:
        yield
    finally:
        for k, v in saved.items():
            if v is None:
                sys.modules.pop(k, None)
            else:
                sys.modules[k] = v


def pause_output(r):
    buf = io.StringIO()
    with estop(engaged=bool(r.get("engaged")), state=r.get("state"),
               path=r.get("path", "")):
        with contextlib.redirect_stdout(buf):
            rc = live.cmd_pause(argparse.Namespace(reason=r.get("reason")))
    return {"text": buf.getvalue(), "rc": rc}


def resume_output(r):
    buf = io.StringIO()
    with estop(disengaged=bool(r.get("disengaged")), path=r.get("path", "")):
        with contextlib.redirect_stdout(buf):
            rc = live.cmd_resume(argparse.Namespace())
    return {"text": buf.getvalue(), "rc": rc}


def parser_spec(r):
    subs = argparse.ArgumentParser().add_subparsers()
    live.build_pause_parser(subs)
    which = r.get("which") or "pause"
    field = r.get("field") or "all"
    helps = {pa.dest: pa.help for pa in subs._choices_actions}
    parser = subs.choices[which]
    reason = next((a.default for a in parser._actions if "--reason" in a.option_strings), None)
    spec = {"help": helps.get(which), "description": parser.description,
            "reason_default": reason}
    if field == "all":
        return spec
    return {"value": spec[field]}


def expected(r):
    f = r.get("func")
    if f == "pause_output":
        return pause_output(r)
    if f == "resume_output":
        return resume_output(r)
    if f == "parser_spec":
        return parser_spec(r)
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
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
