#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/compression.py
(gate w31_compressio_220b6d_p1).

The live module is imported as a package module.  Every sqlite seam it reaches
is faked: the chatty StoreHolder reads answer from a {id: row} map, the chain
step answers from a {id: child} map, and the write transaction records the
stamps it would make.  What is compared is exactly the decision those reads
produce: chain walks, lineage and rotation roots, lease keys, fork and
continuation predicates, cooldown expiry and the sessions row a compression
child is published with.
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
GATE = "w31_compressio_220b6d_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.conversation._store import compression as live  # noqa: E402
from persistence.sqlite.conversation._store.messages import (  # noqa: E402
    _is_explicit_fork_child_row,
)
from session_rules.surfaces import created_surface  # noqa: E402

WANTED = {
    "cooldown_row", "walk_chain", "chain_tip", "unset_created", "fork_child",
    "compression_child", "lineage", "tool_root", "lease_key", "cooldown_active",
    "stamp_ancestors", "child_row",
}

LEASE_SQL = "SELECT id, parent_session_id, source, model_config, end_reason FROM sessions WHERE id = ?"


class Cur:
    def __init__(self, rows=None, rc=0):
        self.rows = rows or []
        self.rowcount = rc

    def fetchone(self):
        return self.rows[0] if self.rows else None

    def fetchall(self):
        return list(self.rows)


class StepConn:
    """Answers ``_CHAIN_STEP_SQL``: step(id) -> {"id": child}."""

    def __init__(self, steps):
        self.steps = steps

    def execute(self, sql, params=()):
        if sql == live._CHAIN_STEP_SQL:
            child = self.steps.get(params[0])
            return Cur([{"id": child}] if child else [])
        return Cur([])


class StepHolder:
    def __init__(self, steps):
        self.steps = steps

    def _read_ctx(self):
        steps = self.steps

        @contextlib.contextmanager
        def cm():
            yield StepConn(steps)

        return cm()


class StoreHolder:
    """get_session / _read_one / _read_all answers from maps."""

    def __init__(self, sessions=None, children=None, cooldown=None, row=None):
        self.sessions = sessions or {}
        self.children = children or {}
        self.cooldown = cooldown
        self.row = row

    def flush_for_read(self):
        pass

    def _read_one(self, sql, params=()):
        if sql.startswith("SELECT s.*"):
            row = self.sessions.get(params[0])
            return dict(row) if row else None
        if sql == live._COOLDOWN_ROW_SQL:
            return self.cooldown
        if sql.startswith("SELECT ") and " FROM sessions WHERE id = ?" in sql:
            return self.row
        return None

    def _read_all(self, sql, params=()):
        if "SELECT * FROM sessions" in sql and "parent_session_id = ?" in sql:
            return [dict(r) for r in self.children.get(params[0], [])]
        return []


class LeaseConn:
    def __init__(self, sessions):
        self.sessions = sessions

    def execute(self, sql, params=()):
        if sql.startswith("SELECT id, parent_session_id, source, model_config, end_reason"):
            row = self.sessions.get(params[0])
            return Cur([dict(row)] if row else [])
        return Cur([])


class StampConn:
    """The stamp walk: created rows, the unset-ancestor probe, chain steps, UPDATEs."""

    def __init__(self, created, steps, unset_ancestor):
        self.created = created
        self.steps = steps
        self.unset_ancestor = unset_ancestor
        self.stamps = []

    def execute(self, sql, params=()):
        if sql == live._CREATED_ROW_SQL:
            row = self.created.get(params[0])
            return Cur([dict(row)] if row else [])
        if sql.strip().startswith("WITH RECURSIVE"):
            return Cur([(1,)] if self.unset_ancestor else [])
        if sql == live._CHAIN_STEP_SQL:
            child = self.steps.get(params[0])
            return Cur([{"id": child}] if child else [])
        if sql.startswith("UPDATE sessions SET created_source"):
            self.stamps.append(params[1])
            return Cur([], 1)
        return Cur([], 0)


class PublishConn:
    def __init__(self):
        self.calls = []

    def execute(self, sql, params=()):
        if sql.startswith("INSERT INTO sessions"):
            self.calls.append((sql, tuple(params)))
        return Cur([], 1)


class PublishHolder:
    def __init__(self, prompt_hash):
        self.prompt_hash = prompt_hash

    def _store_system_prompt(self, conn, prompt):
        return self.prompt_hash


@contextlib.contextmanager
def clock(now):
    original = live.time
    live.time = types.SimpleNamespace(time=lambda: float(now))
    try:
        yield
    finally:
        live.time = original


def walk(session_id, steps):
    def step(current):
        child = steps.get(current)
        return {"id": child} if child else None

    return live._walk_compression_chain(session_id, step)


def stamp_ancestors(r):
    conn = StampConn(r.get("created") or {}, r.get("steps") or {},
                     bool(r.get("unset_ancestor")))
    live.stamp_compression_ancestors(conn, r.get("session_id", ""))
    return conn.stamps


CHILD_COLUMNS = (
    "id", "source", "created_source", "model", "model_config", "system_prompt_hash",
    "tool_names", "parent_session_id", "cwd", "git_branch", "git_repo_root",
    "profile_name", "user_id", "session_key", "chat_id", "chat_type", "thread_id",
    "display_name", "origin_json", "pinned", "started_at", "archived", "auto_archived",
)


def child_row(r):
    parent = r.get("parent") or {}
    conn = PublishConn()
    holder = PublishHolder(r.get("prompt_hash"))
    with clock(r.get("now", 0.0)):
        live._publish_child_session_row(
            holder, conn, parent,
            parent_session_id=r.get("parent_session_id"),
            child_session_id=r.get("child_id"),
            source=r.get("source"),
            model=r.get("model"),
            model_config=r.get("model_config"),
            system_prompt=r.get("prompt_hash"),
            cwd=r.get("cwd"),
            profile_name=r.get("profile_name"))
    _, params = conn.calls[0]
    out = dict(zip(CHILD_COLUMNS, params))
    if isinstance(out.get("model_config"), str):
        out["model_config"] = json.loads(out["model_config"])
    # The three surfaces are seam answers; check the fixture agrees with the rule.
    want_p = created_surface(parent.get("created_source"))
    want_s = created_surface(r.get("source"))
    assert r.get("surface_parent") == want_p, "surface_parent fixture drift"
    assert r.get("surface_source") == want_s, "surface_source fixture drift"
    return out


def expected(r):
    f = r.get("func")
    if f == "cooldown_row":
        return live._cooldown_row(bool(r.get("session_exists")),
                                  r.get("cooldown_until"), r.get("error"))
    if f == "walk_chain":
        return walk(r.get("session_id", ""), r.get("steps") or {})
    if f == "chain_tip":
        return live.get_compression_tip(StepHolder(r.get("steps") or {}),
                                        r.get("session_id", ""))
    if f == "unset_created":
        return r.get("value") in live._UNSET_CREATED_SOURCES
    if f == "fork_child":
        return _is_explicit_fork_child_row(
            None, r.get("session") or {}, include_reset=bool(r.get("include_reset")))
    if f == "compression_child":
        return live._is_compression_child_row(StoreHolder(r.get("sessions") or {}),
                                              r.get("child") or {})
    if f == "lineage":
        holder = StoreHolder(r.get("sessions") or {}, r.get("children") or {})
        return live.get_compression_lineage(holder, r.get("session_id", ""))
    if f == "tool_root":
        return live.get_tool_rotation_root(StoreHolder(r.get("sessions") or {}),
                                           r.get("session_id", ""))
    if f == "lease_key":
        sessions = r.get("sessions") or {}
        holder = StoreHolder(sessions)
        return live._session_turn_lease_key_on_conn(
            holder, LeaseConn(sessions), r.get("session_id", ""))
    if f == "cooldown_active":
        holder = StoreHolder(cooldown=r.get("row"))
        with clock(r.get("now", 0.0)):
            return live.get_compression_failure_cooldown(holder, r.get("session_id", ""))
    if f == "stamp_ancestors":
        return stamp_ancestors(r)
    if f == "child_row":
        return child_row(r)
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
