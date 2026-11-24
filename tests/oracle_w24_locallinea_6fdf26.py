#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/gateway/_local_lineage.py
(gate w24_locallinea_6fdf26).

The live module is imported as a package module and called with a fake
connection: every SQL statement is answered from the case's values (the rows a
query would have returned), and every write is recorded instead of performed.
That is the seam -- no SQLite file, no transaction -- and it is the live routing
code that decides:

  * ``moved_receipts`` : the projection and the target_ids narrowing;
  * ``release_receipts`` : the (policy, legacy) key pairs it deletes;
  * ``hand_off_conversations`` : which receipts are handed, the segment dedupe
    that order-preserves the conversation, and what the release retires;
  * ``_segment_root`` : the head of the target's /new segment through the
    compressions behind it;
  * ``_title_follows_head`` : whether the name moves over a lower-provenance
    title, using the live ``_title_rank``;
  * ``_receipt`` and the module's three literals.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_locallinea_6fdf26"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.gateway import _local_lineage as live  # noqa: E402
from persistence.sqlite._schema import _title_rank  # noqa: E402

WANTED = {"constants", "receipt", "moved", "release_keys", "handoff",
          "segment_root", "title_moves", "title_rank"}


class Cursor:
    def __init__(self, row=None, rows=None):
        self._row = row
        self._rows = [] if rows is None else rows

    def fetchone(self):
        return self._row

    def fetchall(self):
        return self._rows


class FakeConn:
    """Answers the queries with values; records the writes."""

    def __init__(self, moved_rows=None, receipts=None, end_reasons=None, title_rows=None):
        self.moved_rows = moved_rows or []
        self.receipts = receipts or {}          # owner -> stored receipt text
        self.end_reasons = end_reasons or {}    # session id -> end_reason
        self.title_rows = title_rows or {}      # session id -> (title, title_source[, hidden])
        self.identity_moves = []                # (target, segments)
        self.deletes = []                       # list of [(policy, legacy), ...]
        self.title_updates = 0

    def execute(self, sql, params=()):
        s = " ".join(str(sql).split())
        if "FROM state_meta m JOIN" in s:
            return Cursor(rows=[tuple(r) for r in self.moved_rows])
        if "FROM state_meta WHERE key" in s:
            key = params[0]
            owner = key[len(live.POLICY_PREFIX):] if isinstance(key, str) else None
            if owner in self.receipts:
                return Cursor(row=(self.receipts[owner],))
            return Cursor(row=None)
        if "SELECT end_reason" in s:
            reason = self.end_reasons.get(params[0])
            return Cursor(row=None if reason is None else (reason,))
        if s.startswith("UPDATE sessions SET ("):
            self.identity_moves.append((params[0], json.loads(params[1])))
            return Cursor()
        if "SELECT title, title_source, hidden" in s or "SELECT title, title_source FROM" in s:
            row = self.title_rows.get(params[0])
            return Cursor(row=None if row is None else tuple(row))
        if "UPDATE sessions SET title" in s:
            self.title_updates += 1
            return Cursor()
        raise AssertionError("unexpected SQL %r" % (s,))

    def executemany(self, sql, seq):
        self.deletes.append([tuple(p) for p in seq])
        return Cursor()


def constants():
    return {
        "POLICY_PREFIX": live.POLICY_PREFIX,
        "LEGACY_PREFIX": live.LEGACY_PREFIX,
        "LANE_IDENTITY": live._LANE_IDENTITY,
    }


def receipt(r):
    return live._receipt(r.get("raw"))


def moved(r):
    rows = [tuple(x) for x in (r.get("rows") or []) if isinstance(x, list)]
    conn = FakeConn(moved_rows=rows)
    out = live.moved_receipts(conn, r.get("target_ids"))
    return {str(owner): list(pair) for owner, pair in out.items()}


def release_keys(r):
    conn = FakeConn()
    live.release_receipts(conn, list(r.get("owners") or []))
    return [list(p) for p in (conn.deletes[0] if conn.deletes else [])]


def handoff(r):
    raw = r.get("moved")
    moved_in = {owner: tuple(pair) for owner, pair in raw.items()} if isinstance(raw, dict) else {}
    conn = FakeConn(receipts=r.get("receipts") or {})
    handed = live.hand_off_conversations(conn, moved_in)
    segments = {owner: segs for owner, (_target, segs) in zip(handed.keys(), conn.identity_moves)}
    return {
        "handed": {owner: list(pair) for owner, pair in handed.items()},
        "segments": segments,
        "released": [list(p) for p in (conn.deletes[0] if conn.deletes else [])],
    }


def segment_root(r):
    conn = FakeConn(end_reasons=r.get("end_reasons") or {})
    return live._segment_root(conn, list(r.get("segments") or []), r.get("target"))


def title_moves(r):
    owner = r.get("owner")
    head = r.get("head")
    rows = {}
    if r.get("owner_row") is not None:
        rows[owner] = tuple(r.get("owner_row"))
    if r.get("head_row") is not None:
        rows[head] = tuple(r.get("head_row"))
    conn = FakeConn(title_rows=rows)
    live._title_follows_head(conn, owner, head)
    return conn.title_updates > 0


def expected(r):
    f = r.get("func")
    if f == "constants":
        return constants()
    if f == "receipt":
        return receipt(r)
    if f == "moved":
        return moved(r)
    if f == "release_keys":
        return release_keys(r)
    if f == "handoff":
        return handoff(r)
    if f == "segment_root":
        return segment_root(r)
    if f == "title_moves":
        return title_moves(r)
    if f == "title_rank":
        return _title_rank(r.get("source"))
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
