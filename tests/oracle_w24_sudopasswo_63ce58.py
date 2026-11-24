#!/usr/bin/env python3
"""Differential oracle for tools/environments/sudo_password_note.py
(gate w24_sudopasswo_63ce58).

The live module is imported and driven for real: ``backend_label`` is called on
a synthetic environment class built from the wire descriptor, and
``note_sudo_password_sent`` is called for real with a logging handler attached,
so what is compared is the exact record the module would emit -- including the
process-wide once-per-class set, which the oracle clears before every case.
The logging call and ``_noted_backend_types`` are the seams the C side leaves
to its caller.

Pinned shapes:
  * ``sudo_stdin`` that is neither None nor a str is ``"ERROR:attribute"``
    (upstream raises AttributeError on ``.strip``);
  * a non-object env is its Python type name, exactly what
    ``type(env).__name__`` yields for it.
"""
import json
import logging
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_sudopasswo_63ce58"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tools.environments import sudo_password_note as live  # noqa: E402

WANTED = {"label", "note", "constants"}


def make_env(req):
    desc = req.get("env")
    if desc is None or not isinstance(desc, dict):
        return desc
    attrs = {}
    if "backend_name" in desc:
        attrs["_hermes_backend_name"] = desc["backend_name"]
    name = desc.get("class_name")
    if not isinstance(name, str):
        name = "dict"
    env = type(name, (), attrs)()
    if "is_local" in desc:
        env.is_local = desc["is_local"]
    return env


def label(req):
    return live.backend_label(make_env(req))


class _Capture(logging.Handler):
    def __init__(self):
        super().__init__(level=logging.WARNING)
        self.records = []

    def emit(self, record):
        self.records.append(record)


def note(req):
    stdin = req.get("sudo_stdin")
    if stdin is not None and not isinstance(stdin, str):
        return "ERROR:attribute"
    live._reset_noted_backends()
    capture = _Capture()
    old_level, old_prop = live.logger.level, live.logger.propagate
    live.logger.addHandler(capture)
    live.logger.setLevel(logging.WARNING)
    live.logger.propagate = False
    try:
        live.note_sudo_password_sent(make_env(req), stdin)
    finally:
        live.logger.removeHandler(capture)
        live.logger.setLevel(old_level)
        live.logger.propagate = old_prop
        live._reset_noted_backends()
    if not capture.records:
        return {"logged": False, "message": None}
    return {"logged": True, "message": capture.records[0].getMessage()}


def constants():
    return {"camel_boundary": live._CAMEL_BOUNDARY.pattern}


def expected(r):
    f = r.get("func")
    if f == "label":
        return label(r)
    if f == "note":
        return note(r)
    if f == "constants":
        return constants()
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
