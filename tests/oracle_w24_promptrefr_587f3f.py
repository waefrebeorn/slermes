#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/prompt_refresh.py
(gate w24_promptrefr_587f3f).

The live module is imported whole and its three public functions are called
against a *recording* holder instead of a SQLite one: ``_execute_write`` runs
the callback with a fake connection whose SELECT returns the fixture's MAX(...)
row and whose INSERT records its parameters, and ``_read_one`` returns the
fixture row.  Nothing is opened, so the SQL text, the key predicate and the
``int(row[0] or 0)`` collapses under test are the upstream ones.

Exceptions are the error seam: the C port has no raise path, so a live
TypeError/ValueError/AttributeError is compared as null.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_promptrefr_587f3f"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.conversation._store import prompt_refresh as live  # noqa: E402

WANTED = {"refresh_key", "next_epoch", "epoch_from_row", "count_from_row", "constants"}

SEAM_ERRORS = (TypeError, ValueError, AttributeError, KeyError, IndexError)


class _Cursor:
    def __init__(self, row):
        self._row = row

    def fetchone(self):
        return self._row


class _Conn:
    """A connection whose SELECT yields ``row``; INSERTs are recorded."""

    def __init__(self, row, writes):
        self._row = row
        self._writes = writes

    def execute(self, sql, params=()):
        if sql.lstrip().upper().startswith("SELECT"):
            return _Cursor(self._row)
        self._writes.append((sql, params))
        return _Cursor(None)


class RecHolder:
    """Holder seam: records the write callback's effect, serves the fixture row."""

    def __init__(self, row):
        self.row = row
        self.writes = []
        self.reads = []

    def _read_one(self, sql, params=()):
        self.reads.append((sql, params))
        return self.row

    def _execute_write(self, fn):
        return fn(_Conn(self.row, self.writes))


def refresh_key(req):
    holder = RecHolder([0])
    live.request_prompt_refresh(holder, req.get("session_id"))
    return holder.writes[0][1][0]          # the state_meta key it UPSERTs


def next_epoch(req):
    holder = RecHolder([req.get("current")])
    return live.request_prompt_refresh(holder, None)


def epoch_from_row(req):
    return live.prompt_refresh_epoch(RecHolder(req.get("row")), "sess-x")


def count_from_row(req):
    return live.count_sessions_with_stored_prompt(RecHolder(req.get("row")))


def constants():
    return {"profile_key": live.PROFILE_KEY, "session_key_prefix": live.SESSION_KEY_PREFIX}


def expected(r):
    f = r.get("func")
    if f == "refresh_key":
        return refresh_key(r)
    if f == "next_epoch":
        return next_epoch(r)
    if f == "epoch_from_row":
        return epoch_from_row(r)
    if f == "count_from_row":
        return count_from_row(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = seams = 0
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
        except SEAM_ERRORS as exc:
            seams += 1
            py = None                       # the C port's raise seam
            if r.get("out") is not None:
                mism += 1
                print("MISMATCH %s case=%r live=%r but c=%r (expected the null seam)"
                      % (r.get("func"), r.get("case"), exc, r.get("out")), file=sys.stderr)
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
