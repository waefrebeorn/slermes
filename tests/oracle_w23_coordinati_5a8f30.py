#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/authority/coordination.py (gate
w23_coordinati_5a8f30).

The repository's decisions are made over rows a SQLite connection returned, so
the connection is the seam and the fixture supplies its answers:

  * ``FakeConn`` answers the module's statements from an in-memory table (the
    four lease statements get real semantics: DELETE frees the key, INSERT OR
    IGNORE loses to a live row) and records every statement with its
    parameters;
  * the store holder is a namespace whose ``_read_ctx_with_timeout`` /
    ``_execute_write`` hand that connection to the live verb;
  * ``time.time`` is the fixture's ``now`` where a verb reads the clock itself
    (``get_admitted_completion``).

Everything else -- the stale predicate, the age cap, the orphan filter, the
statement ladder, the finished-work selectors and the refused/None answers --
is the live code.  Both sides render statements with runs of whitespace
collapsed to one space, so the SQL contract is compared without formatting.

``ClaimLease`` returns ``_claim_lease_row``'s tuple; ``Sweep``/``Replay``
report the ids whose rows the module dropped (read off the recorded UPDATEs).
"""
import importlib.util
import json
import sys
import types
from contextlib import contextmanager
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w23_coordinati_5a8f30"
SITECOPY = "C:/Users/eman5/hermes-agent/sitecopy"

for p in (str(QUARRY), SITECOPY):
    if p not in sys.path:
        sys.path.insert(0, p)

import persistence.sqlite.authority.coordination as live  # noqa: E402
from persistence.authority import coordination as auth  # noqa: E402

WANTED = {"consts", "past_replay_age", "json_object", "claim_lease",
          "replay_pending", "admitted_completion", "sweep", "finished"}

DROP_PREFIX = "UPDATE async_delegations SET delivery_state='dropped'"


def norm_sql(sql):
    return " ".join(str(sql).split())


class Row:
    """sqlite3.Row: index by position and by name."""

    def __init__(self, *values, **named):
        if named:
            self._d = dict(named)
            self._v = list(named.values())
        else:
            self._d = {}
            self._v = list(values)

    def __getitem__(self, key):
        return self._d[key] if isinstance(key, str) else self._v[key]

    def __repr__(self):
        return "Row(%r)" % (self._v,)


class Cur:
    def __init__(self, rows=(), rowcount=0):
        self._rows = list(rows)
        self.rowcount = rowcount

    def fetchone(self):
        return self._rows[0] if self._rows else None

    def fetchall(self):
        return list(self._rows)


class FakeConn:
    """Records statements; answers the SELECTs from scripted rows."""

    def __init__(self, rows=(), lease=None):
        self.rows = list(rows)
        self.lease = lease            # [holder, expires_at] or None
        self.log = []

    def execute(self, sql, params=()):
        stmt = norm_sql(sql)
        self.log.append((stmt, list(params)))
        if stmt.startswith("SELECT holder, expires_at"):
            return Cur([Row(holder=self.lease[0], expires_at=self.lease[1])] if self.lease else [])
        if stmt.startswith("DELETE FROM"):
            if self.lease and self.lease[0] == params[1]:
                self.lease = None
            return Cur([], 1)
        if stmt.startswith("INSERT OR IGNORE"):
            if self.lease is None:
                self.lease = [params[1], params[3]]
            return Cur([], 0)
        if stmt.startswith("SELECT holder FROM"):
            return Cur([Row(holder=self.lease[0])] if self.lease else [])
        # every other statement is a SELECT answered once, or an UPDATE
        return Cur(self.rows)


class Holder:
    def __init__(self, conn):
        self.conn = conn

    def _execute_write(self, fn, patience_s=None):
        return fn(self.conn)

    def _read_ctx_with_timeout(self, patience=None):
        return _ctx(self.conn)

    def _read_ctx(self):
        return _ctx(self.conn)

    def _read_one(self, sql, params=()):
        return self.conn.execute(sql, params).fetchone()

    def _read_all(self, sql, params=()):
        return self.conn.execute(sql, params).fetchall()

    def _write_sql(self, sql, params=()):
        return self.conn.execute(sql, params).rowcount

    def _write_rowcount(self, sql, params=()):
        return self.conn.execute(sql, params).rowcount


@contextmanager
def _ctx(conn):
    yield conn


def repo_for(conn):
    store = types.SimpleNamespace(holder=lambda: Holder(conn))
    return live.SQLiteCoordinationRepository(store)


def statements(conn):
    return [s for s, _ in conn.log]


def params(conn):
    return [p for _, p in conn.log]


def dropped_ids(conn):
    return [p[1] for s, p in conn.log if s.startswith(DROP_PREFIX)]


def pending_dict(p):
    return {"delegation_id": str(p.delegation_id), "event": p.event,
            "completed_at": p.completed_at, "dispatched_at": p.dispatched_at}


def do_consts(_req):
    return {
        "replay_max_age": live.DELEGATION_REPLAY_MAX_AGE_SECONDS,
        "orphan_stale": live.DELEGATION_ORPHAN_STALE_SECONDS,
        "claim_stale": live.DELEGATION_CLAIM_STALE_SECONDS,
        "retention": live.DELEGATION_RETENTION_SECONDS,
        "failure_window": live.DELEGATION_FAILURE_WINDOW_SECONDS,
        "turn_lease_sweep_grace": live.TURN_LEASE_SWEEP_GRACE_SECONDS,
        "max_delivery_attempts": live.DELEGATION_MAX_DELIVERY_ATTEMPTS,
        "max_retained_terminal": live.DELEGATION_MAX_RETAINED_TERMINAL,
        "max_durable_pending": live.DELEGATION_MAX_DURABLE_PENDING,
        "completion_deliveries": list(live.COMPLETION_DELIVERIES),
    }


def do_claim(req):
    conn = FakeConn(lease=req.get("lease"))
    if req.get("mode", "lt") == "le":
        stale = lambda h, e: float(e) <= req["now"] or h in (req.get("dead") or [])  # noqa: E731
    else:
        stale = lambda h, e: e < req["now"] or h in (req.get("dead") or [])  # noqa: E731
    acquired, reclaimed = live._claim_lease_row(
        conn, req.get("table"), req.get("key_col"), req.get("key"), req.get("holder"),
        req.get("now"), req.get("expires_at"), stale)
    return {"acquired": acquired, "reclaimed": reclaimed,
            "sql": statements(conn), "params": params(conn)}


def do_replay(req):
    conn = FakeConn()
    rows = [(r[0], r[1], r[2], r[3]) for r in (req.get("rows") or [])]
    offered = live._replay_pending(conn, rows, req["now"])
    return {"offered": [pending_dict(p) for p in offered], "dropped": dropped_ids(conn),
            "sql": statements(conn), "params": params(conn)}


def do_admitted(req):
    row = req.get("row")
    conn = FakeConn(rows=[Row(*row)] if row is not None else [])
    saved = live.time
    live.time = types.SimpleNamespace(time=lambda: req["now"], monotonic=lambda: req["now"])
    try:
        got = repo_for(conn).get_admitted_completion(delegation_id=req.get("delegation_id"))
    finally:
        live.time = saved
    return {"result": pending_dict(got) if got is not None else None,
            "sql": statements(conn), "params": params(conn)}


def do_sweep(req):
    conn = FakeConn(rows=[Row(*r) for r in (req.get("rows") or [])])
    repo = repo_for(conn)
    offered = repo.sweep_orphaned_completions(
        now=req["now"],
        exclude_ids=frozenset(req.get("exclude_ids") or ()),
        dead_owners=frozenset(auth.RecordedOwner(pid=p, started_at=s)
                              for p, s in (req.get("dead_owners") or ())),
        lost_ids=frozenset(req.get("lost_ids") or ()),
    )
    return {"offered": [pending_dict(p) for p in offered], "dropped": dropped_ids(conn),
            "sql": statements(conn), "params": params(conn)}


def do_finished(req):
    conn = FakeConn(rows=[Row(*r) for r in (req.get("rows") or [])])
    rows = repo_for(conn).list_finished_delegations(
        origin_ui_session_id=req.get("origin_ui_session_id", ""),
        parent_session_id=req.get("parent_session_id", ""),
        limit=req.get("limit", 20), now=req["now"])
    out = []
    for r in rows:
        out.append({"delegation_id": str(r.delegation_id), "state": r.state,
                    "dispatched_at": r.dispatched_at, "completed_at": r.completed_at,
                    "task": r.task, "result": r.result})
    return {"rows": out, "sql": statements(conn), "params": params(conn)}


def expected(req):
    func = req.get("func")
    if func == "consts":
        return do_consts(req)
    if func == "past_replay_age":
        return live._past_replay_age(req["now"], req.get("completed_at"), req.get("dispatched_at"))
    if func == "json_object":
        return live._json_object(req.get("raw"))
    if func == "claim_lease":
        return do_claim(req)
    if func == "replay_pending":
        return do_replay(req)
    if func == "admitted_completion":
        return do_admitted(req)
    if func == "sweep":
        return do_sweep(req)
    if func == "finished":
        return do_finished(req)
    raise AssertionError("unknown func %r" % (func,))


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
