#!/usr/bin/env python3
"""Differential oracle for gateway/relay/admitted_authors.py (gate w25_admittedau_967705).

The live module is imported and its four pure decisions are called for real; the
process-level record (``_admitted``, ``_MAX``) is loaded from the request and read
back after the call, and the Discord ``source``/``event`` objects are stand-ins
carrying exactly the attributes the module reads.  No connector, socket or
command registry is touched: ``resolve_command`` is reached through the live
module, and the fixtures only use the plain ``/approve`` / ``/deny`` names.
"""
import json
import sys
import tempfile
from collections import OrderedDict
from pathlib import Path

# Env seam: the live package may resolve Path.home() at import time and ntpath
# only knows USERPROFILE; give it one when the caller's shell dropped it.
import os
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = os.environ.get("HOME") or tempfile.gettempdir()

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_admittedau_967705"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from gateway.relay import admitted_authors as live  # noqa: E402

WANTED = {"relayed_discord", "note_delivered", "answer_admitted",
          "interaction_admitted", "constants"}


class Src:
    """A message source: only the attributes the module reads."""

    def __init__(self, data):
        if isinstance(data, dict):
            self.__dict__.update(data)


class Ev:
    """A message event: ``source`` plus the two accessors the module calls."""

    def __init__(self, source, sender, command):
        self.source = source
        self._sender = sender
        self._command = command

    def sender_id(self):
        return self._sender

    def get_command(self):
        return self._command


def _source(r):
    """The fixture's source object: the platform's *value* stands in for the
    ``gateway.config.Platform`` member a real source carries (Platform is a plain
    Enum, so its members are only equal to themselves)."""
    raw = r.get("source")
    if not isinstance(raw, dict):
        return None
    data = dict(raw)
    if isinstance(data.get("platform"), str):
        from gateway.config import Platform
        try:
            data["platform"] = Platform(data["platform"])
        except ValueError:
            pass
    return Src(data)


def _load(r):
    admitted = r.get("admitted")
    live._admitted = OrderedDict()
    for key in admitted if isinstance(admitted, list) else []:
        live._admitted[key] = None
    cap = r.get("max")
    live._MAX = cap if isinstance(cap, int) and not isinstance(cap, bool) else 4096


def relayed_discord(r):
    return live._relayed_discord(_source(r))


def note_delivered(r):
    _load(r)
    live.note_delivered(_source(r))
    return {"admitted": list(live._admitted.keys())}


def answer_admitted(r):
    _load(r)
    return live.answer_admitted(Ev(_source(r), r.get("sender"), r.get("command")))


def interaction_admitted(r):
    _load(r)
    admins = r.get("admin_ids")
    return live.interaction_admitted(
        Ev(_source(r), r.get("sender"), r.get("command")),
        admins if isinstance(admins, list) else ())


def constants():
    _load({"admitted": []})
    return {"max": live._MAX, "unadmitted_commands": sorted(live._UNADMITTED_COMMANDS)}


def expected(r):
    f = r.get("func")
    if f == "relayed_discord":
        return relayed_discord(r)
    if f == "note_delivered":
        return note_delivered(r)
    if f == "answer_admitted":
        return answer_admitted(r)
    if f == "interaction_admitted":
        return interaction_admitted(r)
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
