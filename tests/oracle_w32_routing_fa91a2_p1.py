#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/gateway/routing.py, part 1
(gate w32_routing_fa91a2_p1): the routing statements and the coercions that
build them.

The live ``SQLiteRoutingRepository`` is used with ``__new__`` and a scripted
store seam: the SQLite holder (writer connection, read pool, schema migration)
never opens a file.  What the verbs *decide* is what is compared:

  normalize_profile     persistence.gateway.routing.normalize_topic_profile_name
  route_namespace       persistence.gateway.routing.route_namespace
  upsert_plan           SQLiteRoutingRepository.upsert_routing_entry
  origin_query          SQLiteRoutingRepository.session_for_origin      (seam: holder read)
  never_active_query    SQLiteRoutingRepository.never_active_keyed_sessions (seam: holder read)
  peer_plan             SQLiteRoutingRepository.latest_session_for_peer (seam: holder read)
  record_peer_plan      SQLiteRoutingRepository.record_session_peer     (seam: holder write)
  enable_topic_plan     SQLiteRoutingRepository.enable_topic_mode       (seam: holder write)

Every seam input (the clock, the store's own profile, the rows a query would
return, the topic-table migration) is a fixture value; nothing here mutates a
database.
"""
import contextlib
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w32_routing_fa91a2_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))

from persistence.gateway import routing as contract  # noqa: E402
from persistence.sqlite.gateway import routing as live  # noqa: E402

WANTED = {
    "normalize_profile", "route_namespace", "upsert_plan", "origin_query",
    "never_active_query", "peer_plan", "record_peer_plan", "enable_topic_plan",
}

DEFAULT_NOW = 1700000000.0


# ── the seam ────────────────────────────────────────────────────────────────────
def _norm_params(params):
    if isinstance(params, dict):
        return {str(k): v for k, v in params.items()}
    if params is None:
        return []
    return list(params)


class _Cursor:
    def __init__(self, rows):
        self._rows = list(rows or [])
        self.rowcount = len(self._rows)

    def fetchone(self):
        return self._rows[0] if self._rows else None

    def fetchall(self):
        return list(self._rows)

    def __iter__(self):
        return iter(self._rows)


class _Seam:
    """A holder whose every read answers from a fixture script, in order."""

    def __init__(self, script=None):
        self._script = [list(r) for r in (script or [])]
        self.calls = []

    def _record(self, sql, params):
        self.calls.append({"sql": sql, "params": _norm_params(params)})

    def _take(self):
        return self._script.pop(0) if self._script else []

    def _read_all(self, sql, params=()):
        self._record(sql, params)
        return self._take()

    def _read_one(self, sql, params=()):
        self._record(sql, params)
        rows = self._take()
        return rows[0] if rows else None

    def _write_sql(self, sql, params=(), many=False):
        self._record(sql, params)

    def execute(self, sql, params=()):
        self._record(sql, params)
        return _Cursor(self._take())

    def executemany(self, sql, rows):
        self._record(sql, rows)
        return _Cursor([])

    def _execute_write(self, fn):
        return fn(self)

    @contextlib.contextmanager
    def _read_ctx(self):
        yield self


class _Sessions:
    def delete_many(self, *a, **k):
        raise AssertionError("delete_many is not part of this gate")


class _Store:
    def __init__(self, seam):
        self._seam = seam
        self.sessions = _Sessions()

    def holder(self):
        return self._seam


class _Clock:
    def __init__(self, now):
        self._now = now

    def time(self):
        return self._now


def now_of(req):
    value = req.get("now")
    return DEFAULT_NOW if value is None else value


def repo_for(req, script=None):
    seam = _Seam(script if script is not None else req.get("script") or [])
    live.time = _Clock(now_of(req))
    owner = req.get("owner")
    live._own_profile_name = lambda holder, _o=owner: _o
    live.apply_telegram_topic_migration = lambda holder: None
    repo = live.SQLiteRoutingRepository.__new__(live.SQLiteRoutingRepository)
    repo._store = _Store(seam)
    return repo, seam


# ── the ported verbs ───────────────────────────────────────────────────────────
def normalize_profile(req):
    return contract.normalize_topic_profile_name(req.get("value"))


def route_namespace(req):
    return contract.route_namespace(req.get("session_key"))


def upsert_plan(req):
    repo, seam = repo_for(req)
    repo.upsert_routing_entry(req.get("session_key"), req.get("entry_json"),
                              routing_scope=req.get("routing_scope", ""))
    return {"calls": seam.calls}


def origin_query(req):
    repo, seam = repo_for(req)
    repo.session_for_origin(platform=req.get("platform"), chat_id=req.get("chat_id"),
                            thread_id=req.get("thread_id"), user_id=req.get("user_id"))
    return {"calls": seam.calls}


def never_active_query(req):
    repo, seam = repo_for(req)
    try:
        repo.never_active_keyed_sessions(older_than_days=req.get("older_than_days"))
    except ValueError as exc:
        return {"error": str(exc)}
    return {"calls": seam.calls}


def peer_plan(req):
    repo, seam = repo_for(req, script=[[], []])
    repo.latest_session_for_peer(source=req.get("source"), user_id=req.get("user_id"),
                                 session_key=req.get("session_key"), chat_id=req.get("chat_id"),
                                 chat_type=req.get("chat_type"), thread_id=req.get("thread_id"))
    return {"calls": seam.calls}


def record_peer_plan(req):
    repo, seam = repo_for(req, script=[[]])
    repo.record_session_peer(
        req.get("session_id"), source=req.get("source", ""), user_id=req.get("user_id"),
        session_key=req.get("session_key"), chat_id=req.get("chat_id"),
        chat_type=req.get("chat_type"), thread_id=req.get("thread_id"),
        display_name=req.get("display_name"), origin_json=req.get("origin_json"),
        include_compression_ancestors=bool(req.get("include_compression_ancestors")),
        messaging_lane=False, transport_profile=req.get("transport_profile"))
    return {"calls": seam.calls}


def enable_topic_plan(req):
    repo, seam = repo_for(req)
    repo.enable_topic_mode(chat_id=req.get("chat_id"), user_id=req.get("user_id"),
                           profile_name=req.get("profile_name", "default"),
                           has_topics_enabled=req.get("has_topics_enabled"),
                           allows_users_to_create_topics=req.get("allows_users_to_create_topics"))
    return {"calls": seam.calls}


def expected(r):
    f = r.get("func")
    if f not in WANTED:
        raise AssertionError("unknown func %r" % (f,))
    return {
        "normalize_profile": normalize_profile,
        "route_namespace": route_namespace,
        "upsert_plan": upsert_plan,
        "origin_query": origin_query,
        "never_active_query": never_active_query,
        "peer_plan": peer_plan,
        "record_peer_plan": record_peer_plan,
        "enable_topic_plan": enable_topic_plan,
    }[f](r)


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
            print("MISMATCH %s case=%r live-error=%s: %r"
                  % (r.get("func"), r.get("case"), type(exc).__name__, exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%s\n  c =%s"
                  % (r.get("func"), r.get("case"),
                     json.dumps(py, sort_keys=True), json.dumps(got, sort_keys=True)),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
