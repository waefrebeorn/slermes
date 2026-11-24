#!/usr/bin/env python3
"""Differential oracle for gateway/session_messaging_picks.py
(gate w26_sessionmes_de1f3f).

The live module is imported and its admission values are called on a stand-in
live session: a SimpleNamespace whose ``source.platform`` is the real
``gateway.config.Platform`` member named by the request.  That is the seam --
the authority, the runner, the runtime row and the receipt write never run;
what is compared is the gate that decides whether they are reached at all.
"""
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_sessionmes_de1f3f"

os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from gateway.config import Platform  # noqa: E402
from gateway import session_messaging_picks as live  # noqa: E402

WANTED = {"is_chat_pick", "platform_key", "constants"}


def stand_in(req):
    """A live session whose only consulted field is source.platform."""
    if not req.get("live", True):
        return None
    return SimpleNamespace(source=SimpleNamespace(platform=Platform(req.get("platform"))))


def is_chat_pick(req):
    return live.is_chat_pick(stand_in(req), req.get("operation"))


def platform_key(req):
    return live._platform_key(stand_in(req))


def constants():
    return sorted(live.CHAT_PICKS)


def expected(r):
    f = r.get("func")
    if f == "is_chat_pick":
        return is_chat_pick(r)
    if f == "platform_key":
        return platform_key(r)
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
