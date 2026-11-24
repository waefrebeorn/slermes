#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/workers/_executions.py
(gate w20_executions_2cbbe5).

The live module is imported under its real dotted name against stubbed
`persistence.*` packages: `persistence.core.errors.AuthorityRefusal`, the
`authority._runtime` helpers (faithful copies of the upstream bodies, because the
real module pulls the whole contract package) and `workers._runtime_workers`
(lineage and worker-state probes). Each fixture line describes the rows the
statement stream would return, so `register_worker_execution` and
`adopt_worker_execution` run their real decision trees over a scripted store.
Compared value: the returned public row, or the refusal reason / raised error.
"""
import hashlib
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = list(attrs.pop("__path__", []))
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class AuthorityRefusal(Exception):
    def __init__(self, reason, **kw):
        super().__init__(reason)
        self.reason = reason
        self.kw = kw


class StoreUnavailable(Exception):
    pass


STATE = {"db": {}, "store": {}, "conn": None}


def _text(value):
    if not isinstance(value, str) or not value or len(value) > 1024:
        raise AuthorityRefusal("invalid_params")
    return value


def _epoch(conn, epoch):
    row = conn.execute("SELECT epoch FROM runtime_epoch WHERE singleton=1").fetchone()
    if type(epoch) is not int or row is None or row[0] != epoch:
        raise AuthorityRefusal("stale_epoch")


def _session(conn, session_id):
    row = conn.execute("SELECT * FROM sessions WHERE id=?", (session_id,)).fetchone()
    if row is None:
        raise AuthorityRefusal("not_found")
    return row


def _worker_public(row):
    result = dict(row)
    result.pop("adoption_digest")
    return result


def _worker_assignment(conn, execution_id, session_id, generation):
    row = conn.execute("SELECT * FROM worker_executions WHERE execution_id=?",
                       (execution_id,)).fetchone()
    if row is None:
        raise AuthorityRefusal("not_found")
    if row["session_id"] != session_id:
        raise AuthorityRefusal("permission_denied")
    if (type(generation) is not int or row["generation"] != generation
            or _session(conn, session_id)["runtime_generation"] != generation):
        raise AuthorityRefusal("stale_generation")
    return row


def _linked_worker_admission(conn, session_id, generation, owner_epoch, statuses):
    linked = STATE["db"].get("linked")
    rows = []
    if (isinstance(linked, dict) and linked.get("status") in statuses
            and linked.get("generation") == generation
            and linked.get("owner_epoch") == owner_epoch):
        rows.append(linked)
    if len(rows) > 1:
        raise AuthorityRefusal("admission_conflict")
    return rows[0] if rows else None


def _secret_digest(secret):
    _text(secret)
    return hashlib.sha256(secret.encode("utf-8")).hexdigest()


def _linked_placeholder(conn, session_id, generation, owner_epoch, statuses):
    return None


def runtime_lineage(conn, session_id):
    lineage = STATE["db"].get("lineage")
    return set(lineage) if isinstance(lineage, list) else {session_id}


def worker_states(conn, session_id):
    states = STATE["db"].get("worker_states")
    return set(states) if isinstance(states, list) else set()


_mk("persistence")
_mk("persistence.core")
_mk("persistence.core.errors", AuthorityRefusal=AuthorityRefusal, StoreUnavailable=StoreUnavailable)
_mk("persistence.sqlite")
_mk("persistence.sqlite.workers", __path__=[os.path.join(Q, "persistence", "sqlite", "workers")])
_mk("persistence.sqlite.authority")
_mk("persistence.sqlite.authority._runtime",
    _epoch=_epoch, _session=_session, _text=_text, _worker_public=_worker_public,
    _worker_assignment=_worker_assignment, _linked_worker_admission=_linked_worker_admission,
    _secret_digest=_secret_digest, _admission=_linked_placeholder)
_mk("persistence.sqlite.workers._runtime_workers",
    runtime_lineage=runtime_lineage, worker_states=worker_states,
    compact_terminal_receipts=lambda *a, **k: None)

NAME = "persistence.sqlite.workers._executions"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "persistence", "sqlite", "workers", "_executions.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


class Cursor:
    def __init__(self, rows=None, rowcount=1):
        self.rows = list(rows or [])
        self.rowcount = rowcount

    def fetchone(self):
        return self.rows[0] if self.rows else None

    def fetchall(self):
        return list(self.rows)


class Conn:
    """The scripted statement stream the fixture describes."""

    def __init__(self, db):
        self.db = db
        self.tables = {
            "worker_executions": {k: dict(v) for k, v in (db.get("executions") or {}).items()},
        }
        self.sessions = {k: dict(v) for k, v in (db.get("sessions") or {}).items()}

    def execute(self, sql, params=()):
        db = self.db
        text = " ".join(sql.split())
        if text.startswith("SELECT epoch FROM runtime_epoch"):
            epoch = db.get("epoch_row")
            return Cursor([(epoch,)] if epoch is not None else [])
        if text.startswith("SELECT * FROM sessions WHERE id=?"):
            row = self.sessions.get(params[0])
            return Cursor([row] if row is not None else [])
        if text.startswith("SELECT * FROM worker_executions WHERE execution_id=?"):
            row = self.tables["worker_executions"].get(params[0])
            return Cursor([dict(row)] if row is not None else [])
        if "FROM session_admissions WHERE target_session_id=? AND status IN" in text:
            active = db.get("admissions_active") or []
            return Cursor([(1,)] if params[0] in active else [])
        if "FROM session_admissions WHERE target_session_id=? AND status='unknown'" in text:
            unknown = db.get("admissions_unknown") or []
            return Cursor([(1,)] if params[0] in unknown else [])
        if text.startswith("INSERT INTO worker_executions"):
            execution_id, session_id, kind, owner_epoch, generation, digest = params
            row = {"execution_id": execution_id, "session_id": session_id, "kind": kind,
                   "owner_epoch": owner_epoch, "generation": generation,
                   "status": "registered", "adoption_digest": digest}
            if not db.get("insert_missing"):
                self.tables["worker_executions"][execution_id] = row
            return Cursor([], 1)
        if text.startswith("UPDATE session_admissions SET"):
            return Cursor([], db.get("admission_rowcount", 1))
        if text.startswith("UPDATE worker_executions SET owner_epoch=?,status='running'"):
            rowcount = db.get("execution_rowcount", 1)
            row = self.tables["worker_executions"].get(params[1])
            if row is not None and rowcount == 1:
                row["owner_epoch"] = params[0]
                row["status"] = "running"
            return Cursor([], rowcount)
        raise AssertionError("unscripted statement: %r params=%r" % (text, params))


class Holder:
    def _execute_write(self, write):
        return write(STATE["conn"])


def outcome(fn):
    try:
        return {"status": "ok", "value": fn()}
    except AuthorityRefusal as exc:
        return {"status": "refused", "reason": exc.reason}
    except Exception as exc:  # noqa: BLE001 - report the live failure verbatim
        return {"status": "raised", "type": type(exc).__name__, "message": str(exc)}


def expected(r):
    state = r.get("db") if isinstance(r.get("db"), dict) else {}
    STATE["db"] = state
    STATE["conn"] = Conn(state)
    args = dict(epoch=r.get("epoch"), execution_id=r.get("execution_id"),
                session_id=r.get("session_id"), generation=r.get("generation"),
                adoption_secret=r.get("adoption_secret"))
    f = r.get("func")
    if f == "register":
        args["kind"] = r.get("kind")
        args["require_idle"] = bool(r.get("require_idle"))
        return outcome(lambda: live.register_worker_execution(Holder(), **args))
    if f == "adopt":
        return outcome(lambda: live.adopt_worker_execution(Holder(), **args))
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s\n  py=%r\n  c =%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_executions_2cbbe5: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
