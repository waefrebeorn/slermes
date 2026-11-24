#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/compression.py
(gate w31_compressio_220b6d_p2).

Statement builders are compared byte-for-byte against the live module constants;
the guarded writes are compared as {sql, params} captured from a fake holder's
_write_sql; the column coercions are compared against the live setters with the
clock pinned.  Every connection and the clock stay seams.
"""
import contextlib
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_compressio_220b6d_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite._schema_ddl import _RESET_CHILD_SQL, _sql_json_extract  # noqa: E402
from persistence.sqlite.conversation._store import compression as live  # noqa: E402
from persistence.sqlite.conversation._store import sql_common as SC  # noqa: E402

WANTED = {
    "chain_step_sql", "unset_ancestor_sql", "unstamped_edge_sql", "record_cooldown",
    "clear_cooldown", "finalize_orphans", "read_number", "streak_write",
    "deadline_write", "overload_bump", "parent_gate",
}

FRAGMENTS = {
    "jx_branched": _sql_json_extract("child.model_config", "$._branched_from"),
    "jx_delegate": _sql_json_extract("child.model_config", "$._delegate_from"),
    "reset_child": _RESET_CHILD_SQL.format(a="child"),
    "last_active": SC._sql_session_last_active("child"),
    "unset_sql": live._UNSET_CREATED_SQL.format(col="s.created_source"),
    "unset_parent": live._UNSET_CREATED_SQL.format(col="parent.created_source"),
    "unset_child": live._UNSET_CREATED_SQL.format(col="child.created_source"),
    "non_continuation": SC._non_continuation_child_sql("child.", "parent.id"),
}

STREAK_SETTERS = {
    "compression_fallback_streak": live.set_compression_fallback_streak,
    "compression_ineffective_count": live.set_compression_ineffective_count,
    "compression_overload_streak": live.set_compression_overload_streak,
}


class Cur:
    def __init__(self, rows=None, rc=0):
        self.rows = rows or []
        self.rowcount = rc

    def fetchone(self):
        return self.rows[0] if self.rows else None

    def fetchall(self):
        return list(self.rows)


class WriteHolder:
    """Captures the writes the store would send; answers reads from a cell."""

    def __init__(self, cell=None, bump_row=None):
        self.calls = []
        self.cell = cell
        self.bump_row = bump_row

    def _write_sql(self, sql, params=()):
        self.calls.append((sql, list(params)))

    def _write_rowcount(self, sql, params=()):
        self.calls.append((sql, list(params)))
        return 0

    def _read_one(self, sql, params=()):
        return self.cell

    def _execute_write(self, fn, patience_s=None):
        return fn(self)

    def execute(self, sql, params=()):
        return Cur([self.bump_row] if self.bump_row is not None else [])


@contextlib.contextmanager
def clock(now):
    original = live.time
    live.time = types.SimpleNamespace(time=lambda: float(now))
    try:
        yield
    finally:
        live.time = original


def as_write(calls):
    if not calls:
        return None
    sql, params = calls[0]
    return {"sql": sql, "params": params}


def expected(r):
    f = r.get("func")
    if f == "chain_step_sql":
        for key, value in FRAGMENTS.items():
            if key in r and r[key] != value:
                raise AssertionError("fragment %s drifted from the live module" % key)
        return live._CHAIN_STEP_SQL
    if f == "unset_ancestor_sql":
        if r.get("unset_sql") != FRAGMENTS["unset_sql"]:
            raise AssertionError("unset fragment drifted")
        return live._UNSET_ANCESTOR_SQL
    if f == "unstamped_edge_sql":
        if (r.get("unset_parent") != FRAGMENTS["unset_parent"]
                or r.get("unset_child") != FRAGMENTS["unset_child"]
                or r.get("non_continuation") != FRAGMENTS["non_continuation"]):
            raise AssertionError("unstamped fragments drifted")
        return live._UNSTAMPED_EDGE_CHILD_SQL
    if f == "record_cooldown":
        holder = WriteHolder()
        live.record_compression_failure_cooldown(
            holder, r.get("session_id", ""), r.get("cooldown_until"), r.get("error"))
        return as_write(holder.calls)
    if f == "clear_cooldown":
        holder = WriteHolder()
        live.clear_compression_failure_cooldown(holder, r.get("session_id", ""))
        return as_write(holder.calls)
    if f == "finalize_orphans":
        holder = WriteHolder()
        with clock(r.get("now", 0.0)):
            live.finalize_orphaned_compression_sessions(holder)
        return as_write(holder.calls)
    if f == "read_number":
        holder = WriteHolder(cell=(r.get("value"),))
        cast = float if r.get("kind") == "float" else int
        return live._read_session_number(holder, "probe_column", "s1", cast, r.get("zero"))
    if f == "streak_write":
        setter = STREAK_SETTERS[r.get("column")]
        holder = WriteHolder()
        setter(holder, r.get("session_id", ""), r.get("streak"))
        return as_write(holder.calls)
    if f == "deadline_write":
        holder = WriteHolder()
        live.set_compression_recovery_deadline(holder, r.get("session_id", ""), r.get("deadline"))
        return as_write(holder.calls)
    if f == "overload_bump":
        holder = WriteHolder(bump_row=r.get("row"))
        return live.increment_compression_overload_streak(holder, r.get("session_id", ""))
    if f == "parent_gate":
        automatic = live.is_automatic_end_reason(r.get("end_reason"))
        if automatic != bool(r.get("automatic_reason")):
            raise AssertionError("automatic_reason fixture drift for %r" % (r.get("end_reason"),))
        if not r.get("parent_exists"):
            return "missing"
        if not r.get("ended_at_set"):
            return "proceed"
        return "clear_stamp" if automatic else "already_ended"
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
    sys.exit(1 if mism or not cases else 0)


if __name__ == "__main__":
    main()
