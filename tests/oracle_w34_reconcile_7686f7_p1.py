#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/admin/_reconcile.py, store
identity and remedy half (gate w34_reconcile_7686f7_p1).

The live module is imported as a package module; the shared seam stubs cover
this host's blocked native extensions on the import chain.  Nothing under test
needs them -- every compared value is a pure function of the JSON inputs.

Ported symbols and their live entry points:

  json_file               _read_json_file(text)          (the file read is the seam:
                                                          the fixture carries the text)
  same_home               _same_hermes_home
  profile_label           _profile_label_for_home        (the platform-default-home
                                                          probe is a seam)
  repair_command          _repair_command
  owner_remedy            _owner_remedy
  owning_gateway_restart  _owning_gateway_restart        (the gateway.lock read is
                                                          stubbed to feed the fixture text)
  lagging_damaged         _LaggingStoreDamaged.__init__
"""
import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w34_reconcile_7686f7_p1"

_TMPHOME = tempfile.mkdtemp(prefix="w34-reconcile-home-")
os.environ["HOME"] = _TMPHOME
os.environ["USERPROFILE"] = _TMPHOME

sys.path.append(str(REPO / "tests"))
from hermes_seam_stubs import install as _install  # noqa: E402

_install()
if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.admin import _reconcile as live  # noqa: E402

WANTED = {
    "json_file", "same_home", "profile_label", "repair_command",
    "owner_remedy", "owning_gateway_restart", "lagging_damaged",
}

_REAL_READ_JSON = live._read_json_file
_LOCK_TEXT = [None]


def _read_json_from_text(text, *, bare_pid_ok=False):
    """The live reader over a fixture's text: same bytes, no store involved."""
    if text is None:
        return None
    fd, name = tempfile.mkstemp(suffix=".lock")
    try:
        with os.fdopen(fd, "wb") as fh:
            fh.write(str(text).encode("utf-8"))
        return _REAL_READ_JSON(Path(name), bare_pid_ok=bare_pid_ok)
    finally:
        try:
            os.unlink(name)
        except OSError:
            pass


def _read_stub(path, *, bare_pid_ok=False):  # noqa: ARG001 - path is the seam
    return _read_json_from_text(_LOCK_TEXT[0], bare_pid_ok=bare_pid_ok)


live._read_json_file = _read_stub


def json_file(r):
    return _read_json_from_text(r.get("raw"), bare_pid_ok=bool(r.get("bare_pid_ok")))


def same_home(r):
    return live._same_hermes_home(str(r.get("left")), str(r.get("right")))


def profile_label(r):
    return live._profile_label_for_home(str(r.get("path")))


def repair_command(r):
    return live._repair_command(Path(str(r.get("home"))))


def owner_remedy(r):
    label, command = live._owner_remedy(Path(str(r.get("home"))))
    return [label, command]


def owning_gateway_restart(r):
    _LOCK_TEXT[0] = r.get("lock_text")
    return live._owning_gateway_restart(Path(str(r.get("home"))))


def lagging_damaged(r):
    detail = r.get("detail")
    return str(live._LaggingStoreDamaged("" if detail is None else str(detail)))


def expected(r):
    f = r.get("func")
    if f == "json_file":
        return json_file(r)
    if f == "same_home":
        return same_home(r)
    if f == "profile_label":
        return profile_label(r)
    if f == "repair_command":
        return repair_command(r)
    if f == "owner_remedy":
        return owner_remedy(r)
    if f == "owning_gateway_restart":
        return owning_gateway_restart(r)
    if f == "lagging_damaged":
        return lagging_damaged(r)
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
    if cases == 0:
        print("ORACLE %s: FAIL-CLOSED, no cases" % GATE, file=sys.stderr)
        sys.exit(2)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
