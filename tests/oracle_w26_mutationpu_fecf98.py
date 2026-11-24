#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/authority/_mutation_publish.py
(gate w26_mutationpu_fecf98).

The live module is imported inside its package (relative imports need it) and
its two value decisions are called directly: the profile a gateway session key
encodes, and the frozen policy a branch child inherits.  The connection, the
prepared snapshot's rows, the lineage tables, the archive/publish writers and
the receipt write are seams; nothing here opens a database.  A refusal is
reported as ``{"__refusal": <wire code>}`` -- the same stand-in the C port
returns instead of raising.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_mutationpu_fecf98"

os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.core.errors import AuthorityRefusal  # noqa: E402
from persistence.sqlite.authority import _mutation_publish as live  # noqa: E402

WANTED = {"profile_from_session_key", "child_policy", "constants"}


def profile_from_session_key(req):
    return live._profile_from_session_key(req.get("session_key"))


def child_policy(req):
    try:
        return live._child_policy(req.get("policy"), req.get("child"), req.get("prepared"))
    except AuthorityRefusal as exc:
        return {"__refusal": exc.reason}


def constants():
    return {"title_source_user": live.TITLE_SOURCE_USER,
            "title_source_derived": live.TITLE_SOURCE_DERIVED}


def expected(r):
    f = r.get("func")
    if f == "profile_from_session_key":
        return profile_from_session_key(r)
    if f == "child_policy":
        return child_policy(r)
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
