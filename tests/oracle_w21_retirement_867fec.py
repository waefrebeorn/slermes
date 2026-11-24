#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/housekeeping/_retirement.py
(gate w21_retirement_867fec).

The live `prune_retired_tombstones` runs unmodified against a real SQLite
`:memory:` database holding the fixture's `state_meta` rows, so the JSON1
predicates, GLOBs and DELETEs are the module's own SQL. Two seams:

* the clock -- `time.time()` is repointed at the fixture's `now`;
* `_reclaim_expired` is wrapped to record (prefix, cutoff) and then delegated
  to the live implementation.

`out` is the plan the C side computes: the keys the stamp UPDATE matches (the
statement's own WHERE, run as a SELECT so the predicate stays the module's), the
keys that disappeared, and the reclaim sweeps in call order.
"""
import importlib.util
import json
import sqlite3
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w21_retirement_867fec"
# module path inside the quarry (the module uses relative imports, so it is
# imported as part of its package rather than as a loose file)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))


live = importlib.import_module("persistence.sqlite.housekeeping._retirement")
terminal = importlib.import_module("persistence.sqlite.conversation._store.terminal")


class Recorder:
    """The open connection, plus the two facts the plan is made of."""

    def __init__(self, conn):
        self.conn = conn
        self.stamped = []
        self.reclaim = []

    def execute(self, sql, params=()):
        head = sql.lstrip()
        if head.upper().startswith("UPDATE STATE_META SET VALUE=JSON_SET"):
            sel = head.replace(
                "UPDATE state_meta SET value=json_set(value, '$.retired_at', ?) WHERE",
                "SELECT key FROM state_meta WHERE", 1)
            assert sel != head, "the stamp statement changed shape"
            self.stamped.extend(k for (k,) in self.conn.execute(sel, tuple(params)[1:]).fetchall())
        return self.conn.execute(sql, params)


def raw(value):
    """A string value is raw row text (so invalid JSON can be expressed); anything
    else is the row's value and is stored as its JSON text."""
    return value if isinstance(value, str) else json.dumps(value)


def expected(r):
    f = r.get("func")
    if f == "constants":
        return {"admission": live.ADMISSION_PREFIX, "result": live.RESULT_PREFIX,
                "worker": live.WORKER_PREFIX, "identity": terminal.IDENTITY_PREFIX,
                "retired": live.RETIRED_PREFIX, "route_fence": live.ROUTE_FENCE_PREFIX,
                "retry_horizon": live.RETRY_HORIZON_S}
    if f == "identity_key":
        return terminal.identity_key(r.get("principal"), r.get("target"), r.get("request"))
    if f != "prune":
        raise AssertionError("unknown func %r" % (f,))

    rows = r.get("rows") or []
    conn = sqlite3.connect(":memory:")
    conn.execute("CREATE TABLE state_meta (key TEXT, value TEXT)")
    inserted = []
    for row in rows:
        conn.execute("INSERT INTO state_meta (key, value) VALUES (?, ?)",
                     (row.get("key"), raw(row.get("value"))))
        inserted.append(row.get("key"))
    before = {k for (k,) in conn.execute("SELECT key FROM state_meta").fetchall()}

    rec = Recorder(conn)
    original_reclaim = live._reclaim_expired

    def reclaim(c, prefix, cutoff):
        rec.reclaim.append([prefix, cutoff])
        return original_reclaim(c, prefix, cutoff)

    live._reclaim_expired = reclaim
    # clock seam
    live.time = types.SimpleNamespace(time=lambda: r.get("now"))
    try:
        if "older_than" in r:
            live.prune_retired_tombstones(rec, r["older_than"])
        else:
            live.prune_retired_tombstones(rec)
    finally:
        live._reclaim_expired = original_reclaim
    after = {k for (k,) in conn.execute("SELECT key FROM state_meta").fetchall()}
    conn.close()
    return {"stamped": rec.stamped, "deleted": sorted(before - after), "reclaim": rec.reclaim}


def main():
    cases = mism = 0
    wanted = {"prune", "identity_key", "constants"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
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
