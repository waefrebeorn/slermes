#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/media.py
(gate w22_media_6187d8).

The sqlite handle is the seam, so both entry points run live against a recording
connection: `execute` keeps the (key, value) pairs `retire_media` INSERTs and
`executemany` keeps the batched spillover pairs, and `fetchall` hands
`retire_spillover` the fixture's message rows. The key derivation, the hosted
document rule and the archive-pointer scan are the live module's own code.

  prefix / spillover_prefix -- the module literals.
  retire_media              -- [{"key", "value"}] in the order the INSERTs run.
                               None when the live call raises (the C's NULL).
  spillover_records         -- {key: value}, a dict because the source builds the
                               batch from a set (order is not part of the
                               contract) and compares it per row.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "persistence" / "sqlite" / "conversation" / "_store" / "media.py"
GATE = "w22_media_6187d8"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("media_store_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["media_store_live"] = live
_spec.loader.exec_module(live)

WANTED = {"prefix", "spillover_prefix", "retire_media", "spillover_records"}


class FakeConn:
    """The sqlite seam: records INSERTs, replays the fixture's SELECT rows."""

    def __init__(self, rows=None):
        self.inserted = []
        self.many = []
        self.rows = list(rows or [])

    def execute(self, sql, params=None):
        if not str(sql).lstrip().upper().startswith("SELECT") and params is not None:
            self.inserted.append((params[0], params[1]))
        return self

    def executemany(self, _sql, seq):
        self.many.extend(list(seq))

    def fetchall(self):
        return list(self.rows)


def py_retire_media(payload, request_id, media_root):
    conn = FakeConn()
    try:
        live.retire_media(conn, payload, request_id, media_root=media_root)
    except Exception:  # KeyError / TypeError / AttributeError -> the C's NULL
        return None
    return [{"key": key, "value": json.loads(value)} for key, value in conn.inserted]


def py_spillover_records(rows):
    conn = FakeConn([(row.get("content"), row.get("api_content")) for row in rows])
    try:
        live.retire_spillover(conn, ["s1"])
    except Exception:  # noqa: BLE001
        return None
    # the batch is built from a set, so the C sorts; answer the same sorted rows
    pairs = {key: json.loads(value) for key, value in conn.many}
    return [{"key": key, "value": pairs[key]} for key in sorted(pairs)]


def expected(r):
    f = r.get("func")
    if f == "prefix":
        return live.PREFIX
    if f == "spillover_prefix":
        return live.SPILLOVER_PREFIX
    if f == "retire_media":
        return py_retire_media(r.get("payload"), r.get("request_id") or "", r.get("media_root"))
    if f == "spillover_records":
        return py_spillover_records(r.get("rows") or [])
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
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
