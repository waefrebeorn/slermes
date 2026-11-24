#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/gateway/routing.py, part 2
(gate w32_routing_fa91a2_p2): the index reads, the orphan/adoption decisions,
the topic-binding guard and the prune outcome.

The live ``SQLiteRoutingRepository`` runs against a scripted store seam: no
connection, no schema, no delete.  The rows every query would return, the
store's own profile and the clock are fixture values, so what is compared is
exactly what the values decide:

  routing_entries_for_sessions  _routing_entries_for_sessions
  routing_entry_for_session     routing_entry_for_session
  session_for_origin_pick       session_for_origin            (seam: holder read)
  orphaned_sessions             orphaned_sessions             (seam: holder reads)
  adopt_gate                    adopt_orphan                  (seam: holder reads)
  bind_guard                    bind_topic                    (seam: holder + migration)
  replace_entries               replace_routing_entries       (seam: _execute_write)
  prune_outcome                 prune_never_active            (seam: lineage + delete)

Reads are the seam: they are fed as ``rows``/``existing``/``current``.  What the
verbs then *write* is what the C side has to reproduce byte for byte, so each
guard reports the statements it generated.
"""
import contextlib
import dataclasses
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w32_routing_fa91a2_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))

from persistence.sqlite.gateway import routing as live  # noqa: E402

WANTED = {
    "routing_entries_for_sessions", "routing_entry_for_session", "session_for_origin_pick",
    "orphaned_sessions", "adopt_gate", "bind_guard", "replace_entries", "prune_outcome",
}

DEFAULT_NOW = 1700000000.0


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


class _Outcome:
    def __init__(self, deleted, removed_ids, skipped_guarded):
        self.deleted = deleted
        self.removed_ids = tuple(removed_ids or ())
        self.skipped_guarded = tuple(skipped_guarded or ())


class _Sessions:
    def __init__(self, spec):
        self._spec = spec or {}
        self.calls = []

    def delete_many(self, ids, skip_guarded=False, media_root=None):
        self.calls.append({"ids": [str(i) for i in ids], "skip_guarded": bool(skip_guarded)})
        return _Outcome(self._spec.get("deleted", 0), self._spec.get("removed_ids"),
                        self._spec.get("skipped_guarded"))


class _Store:
    def __init__(self, seam, delete_spec=None):
        self._seam = seam
        self.sessions = _Sessions(delete_spec)

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


def repo_for(req, script=None, delete_spec=None):
    seam = _Seam(script if script is not None else req.get("script") or [])
    live.time = _Clock(now_of(req))
    owner = req.get("owner")
    live._own_profile_name = lambda holder, _o=owner: _o
    live.owned_lineage_ids = lambda conn, sid, _l=req.get("lineage") or {}: list(
        _l.get(str(sid), [str(sid)]))
    live.apply_telegram_topic_migration = lambda holder: None
    repo = live.SQLiteRoutingRepository.__new__(live.SQLiteRoutingRepository)
    repo._store = _Store(seam, delete_spec)
    return repo, seam


def writes_of(calls):
    return [c for c in calls if not c["sql"].lstrip().startswith("SELECT")]


# ── the ported verbs ───────────────────────────────────────────────────────────
def routing_entries_for_sessions(req):
    repo, _ = repo_for(req, script=[req.get("rows") or []])
    found = repo._routing_entries_for_sessions({str(s) for s in req.get("session_ids") or []})
    return [list(triple) for triple in found]


def routing_entry_for_session(req):
    repo, _ = repo_for(req, script=[req.get("rows") or []])
    return repo.routing_entry_for_session(str(req.get("session_id")))


def session_for_origin_pick(req):
    repo, _ = repo_for(req, script=[req.get("rows") or []])
    return repo.session_for_origin(platform=req.get("platform"), chat_id=req.get("chat_id"),
                                   thread_id=req.get("thread_id"), user_id=req.get("user_id"))


def orphaned_sessions(req):
    orphans = req.get("orphans") or []
    donors = req.get("donors") or {}
    script = [orphans] + [donors.get(str(o.get("id"))) or [] for o in orphans]
    repo, _ = repo_for(req, script=script)
    reports = repo.orphaned_sessions(max_gap_s=req.get("max_gap_s"))
    return [dataclasses.asdict(report) for report in reports]


def adopt_gate(req):
    repo, seam = repo_for(req, script=[[req["donor"]] if req.get("donor") else [],
                                       [req["orphan"]] if req.get("orphan") else []])
    result = repo.adopt_orphan(req.get("orphan_id"), req.get("donor_id"))
    return {"result": bool(result), "writes": writes_of(seam.calls)}


def bind_guard(req):
    script = []
    if req.get("expected_session_id") is not None:
        script.append([req["current"]] if req.get("current") else [])
    script.append([req["existing"]] if req.get("existing") else [])
    repo, seam = repo_for(req, script=script)
    try:
        result = repo.bind_topic(
            chat_id=req.get("chat_id"), thread_id=req.get("thread_id"),
            user_id=req.get("user_id"), session_key=req.get("session_key"),
            session_id=req.get("session_id"), managed_mode=req.get("managed_mode", "auto"),
            profile_name=req.get("profile_name", "default"),
            expected_session_id=req.get("expected_session_id"),
            move_session=bool(req.get("move_session")))
    except live.TopicAlreadyLinked:
        return {"error": "TopicAlreadyLinked", "writes": writes_of(seam.calls)}
    return {"result": bool(result), "writes": writes_of(seam.calls)}


def replace_entries(req):
    # the DELETE consumes the first script slot; the kept SELECT reads the second
    repo, seam = repo_for(req, script=[[], [[k] for k in req.get("kept") or []]])
    rejected = repo.replace_routing_entries(req.get("entries") or {},
                                            routing_scope=req.get("routing_scope", ""))
    return {"statements": writes_of(seam.calls), "rejected": sorted(rejected)}


def prune_outcome(req):
    repo, _ = repo_for(req, script=[req.get("rows") or [], req.get("routing_rows") or []],
                       delete_spec=req.get("delete") or {})
    outcome = repo.prune_never_active(older_than_days=7.0, media_root="/tmp")
    view = dataclasses.asdict(outcome)
    view["deleted_ids"] = list(view["deleted_ids"])
    view["removed_ids"] = list(view["removed_ids"])
    return view


def expected(r):
    f = r.get("func")
    if f not in WANTED:
        raise AssertionError("unknown func %r" % (f,))
    return {
        "routing_entries_for_sessions": routing_entries_for_sessions,
        "routing_entry_for_session": routing_entry_for_session,
        "session_for_origin_pick": session_for_origin_pick,
        "orphaned_sessions": orphaned_sessions,
        "adopt_gate": adopt_gate,
        "bind_guard": bind_guard,
        "replace_entries": replace_entries,
        "prune_outcome": prune_outcome,
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
                     json.dumps(py, sort_keys=True)[:1500], json.dumps(got, sort_keys=True)[:1500]),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
