#!/usr/bin/env python3
"""Differential oracle for plugins/config_backends/remote/paths.py (gate w24_paths_616c0c).

The live module is imported from the quarry and every pure grammar helper is
called on the fixture values: ``encode``, ``decode``, ``validate``, ``covers``,
``covering_lock``, ``from_dotted`` and the §3.1 limits.  ``PathError`` text is
compared verbatim, so an off-by-one limit or a reworded refusal fails here.

Nothing opens a plane, a socket or a config file: the module is pure, and the
seam is that this oracle never needs the rest of the config plane.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_paths_616c0c"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.config_backends.remote import paths as live  # noqa: E402

WANTED = {"encode", "decode", "validate", "covers", "covering_lock", "from_dotted", "constants"}


def envelope(fn):
    try:
        return {"ok": True, "value": fn()}
    except Exception as exc:  # noqa: BLE001 - the refusal text is the compared value
        return {"ok": False, "error": str(exc)}


def encode(r):
    return envelope(lambda: live.encode(r.get("segments")))


def decode(r):
    def run():
        return list(live.decode(r.get("encoded")))
    return envelope(run)


def validate(r):
    segments = r.get("segments")
    if "encoded" in r:
        return envelope(lambda: live.validate(segments, r.get("encoded")) or True)
    return envelope(lambda: live.validate(segments) or True)


def covers(r):
    return live.covers(r.get("lock") or [], r.get("path") or [])


def covering_lock(r):
    locks = [(tuple(lock), level) for lock, level in r.get("locks") or []]
    hit = live.covering_lock(locks, r.get("path") or [])
    if hit is None:
        return None
    lock, level = hit
    return {"lock": list(lock), "level": level}


def from_dotted(r):
    return envelope(lambda: list(live.from_dotted(r.get("dotted"))))


def constants(r):
    return {
        "max_segments": live.MAX_SEGMENTS,
        "max_segment_bytes": live.MAX_SEGMENT_BYTES,
        "max_encoded_bytes": live.MAX_ENCODED_BYTES,
    }


def expected(r):
    f = r.get("func")
    if f == "encode":
        return encode(r)
    if f == "decode":
        return decode(r)
    if f == "validate":
        return validate(r)
    if f == "covers":
        return covers(r)
    if f == "covering_lock":
        return covering_lock(r)
    if f == "from_dotted":
        return from_dotted(r)
    if f == "constants":
        return constants(r)
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
