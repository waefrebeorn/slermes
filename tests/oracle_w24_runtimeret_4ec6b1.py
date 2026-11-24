#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/authority/_runtime_retirement.py
(gate w24_runtimeret_4ec6b1).

The live module is imported as a package module (its relative imports need the
package context) and its pure reads are driven through seams that carry the rows
as values:

  * ``identity_key`` / ``mutation_receipt_key`` -- the durable keys the exact
    retry looks up, computed by the live functions after the live ``_text``
    input check (so the same three invalid_params cases are refused);
  * ``first_generation`` -- a fake holder/connection and a ``retired_in`` that
    answers from the fixture's retired-id list, so the real generation walk
    (including the pinned-identity branch) runs;
  * ``keep_passed_fences`` -- a fake connection that knows which sweep markers
    still exist, so the real per-generation verdict runs;
  * the durable key prefixes.
"""
import contextlib
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_runtimeret_4ec6b1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.authority.runtime import mutation_receipt_key  # noqa: E402
from persistence.core.errors import AuthorityRefusal  # noqa: E402
from persistence.sqlite.authority import _runtime_retirement as live  # noqa: E402
from persistence.sqlite.conversation._store import retirement as retirement_mod  # noqa: E402
from persistence.sqlite.conversation._store import terminal as terminal_mod  # noqa: E402

WANTED = {"identity_key", "receipt_key", "first_generation", "keep_passed_fences", "constants"}

RETIRED_PREFIX = retirement_mod.RETIRED_PREFIX


class FakeCursor:
    def __init__(self, rows):
        self.rows = list(rows)

    def fetchone(self):
        return self.rows[0] if self.rows else None


class FakeConn:
    """state_meta presence + sessions rows, answered from the fixture."""

    def __init__(self, identity_keys=(), rows=None, marked=()):
        self.identity_keys = set(identity_keys or ())
        self.rows = rows or {}
        self.marked = set(marked or ())
        self.unexpired = []

    def execute(self, sql, params=()):
        text = " ".join(str(sql).split())
        if text.startswith("SELECT 1 FROM state_meta"):
            key = params[0]
            return FakeCursor([(1,)] if key in (self.identity_keys | self.marked) else [])
        if text.startswith("SELECT title, source, archived"):
            row = self.rows.get(params[0])
            return FakeCursor([tuple(row)] if row is not None else [])
        if text.startswith("UPDATE state_meta SET value=json_remove"):
            self.unexpired.append(str(params[0])[len(RETIRED_PREFIX):])
            return FakeCursor([])
        raise AssertionError("unexpected SQL: %s" % text)


class FakeHolder:
    def __init__(self, conn):
        self.conn = conn

    def _read_ctx(self):
        return contextlib.nullcontext(self.conn)


def _texts(*values):
    """The source's own input check: `for value in (...): _text(value)`."""
    for value in values:
        live._text(value)


def identity_key(req):
    p, s, r = req.get("principal_id"), req.get("session_id"), req.get("request_id")
    try:
        _texts(p, s, r)
    except AuthorityRefusal:
        return {"error": "invalid_params"}
    return {"key": live.identity_key(p, s, r)}


def receipt_key(req):
    p, s, r = req.get("principal_id"), req.get("session_id"), req.get("request_id")
    try:
        _texts(p, s, r)
    except AuthorityRefusal:
        return {"error": "invalid_params"}
    key, digest = mutation_receipt_key(p, s, r, operation="model", payload={}, expected_revision=0)
    return {"key": key, "digest": digest}


def first_generation(req):
    generations = req.get("generations")
    if not isinstance(generations, list):
        return {"error": "TypeError"}
    start = req.get("start")
    start = 0 if start is None else start
    if isinstance(start, bool) or not isinstance(start, (int, float)):
        return {"error": "TypeError"}
    retired = set()
    for entry in req.get("retired") or []:
        try:
            retired.add(entry)
        except TypeError:
            continue
    pinned = req.get("pinned")
    pinned = tuple(pinned) if isinstance(pinned, list) and len(pinned) == 2 else None

    def derive(n):
        return generations[n]          # IndexError past the end, as a bounded walk

    retirement_mod.retired_in = lambda conn, sid: sid in retired
    conn = FakeConn(req.get("identity_keys") or [], req.get("rows") or {})
    holder = FakeHolder(conn)
    try:
        generation, sid, row = live.first_generation(holder, derive, pinned=pinned, start=start)
    except IndexError:
        return {"error": "IndexError"}
    return {"generation": generation, "sid": sid, "row": list(row) if row is not None else None}


def keep_passed_fences(req):
    generations = req.get("generations")
    if not isinstance(generations, list):
        return {"error": "TypeError"}
    marked = set(req.get("marked") or [])

    def derive(n):
        return generations[n]

    conn = FakeConn(marked=marked)
    try:
        ok = live.keep_passed_fences(conn, derive, req.get("session_id"))
    except IndexError:
        return {"error": "IndexError"}
    return {"ok": ok, "unexpired": conn.unexpired}


def expected(r):
    f = r.get("func")
    if f == "identity_key":
        return identity_key(r)
    if f == "receipt_key":
        return receipt_key(r)
    if f == "first_generation":
        return first_generation(r)
    if f == "keep_passed_fences":
        return keep_passed_fences(r)
    if f == "constants":
        probe = mutation_receipt_key("p", "s", "r", operation="model", payload={}, expected_revision=0)[0]
        return {"retired_prefix": RETIRED_PREFIX,
                "identity_prefix": terminal_mod.IDENTITY_PREFIX,
                "admission_prefix": terminal_mod.ADMISSION_PREFIX,
                "mutation_prefix": probe[:-64],
                "text_max_chars": 1024}
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
