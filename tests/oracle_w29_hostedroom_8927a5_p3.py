#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/gateway/hosted_room_driver.py
(gate w29_hostedroom_8927a5_p3: task state machine, replays and task view).

The live verbs run through a scripted connection and a stub file handle, so
each verb's transaction body sees the fixture's rows and records every
statement and bind tuple.  That is the seam -- no file, no transaction, no
clock -- and what is compared is the decision: idempotent replay or conflict,
the stale-fence message, and the exact bind values the fenced statement takes.
"""
import dataclasses
import json
import sqlite3
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_hostedroom_8927a5_p3"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.gateway.hosted_rooms import DriverLease, HostedTaskIdentity, TaskAttempt  # noqa: E402
from persistence.sqlite.gateway import hosted_room_driver as live  # noqa: E402

WANTED = {
    "task_view", "settlement_params", "settlement_replay", "cancel_replay", "defer_result", "admit", "start",
    "cancel", "settle", "generation", "prune", "mark_indeterminate", "settle_stopping", "begin_cancel",
    "complete_cancel", "requeue_not_admitted",
}

ROOM_SELECT = "SELECT room_id, authority_gateway_id, authority_epoch, disbanded_at FROM hosted_rooms"
LEASE_SELECT = "SELECT * FROM hosted_room_driver_leases WHERE room_id=?"
TASK_SELECT = "SELECT * FROM hosted_room_driver_tasks WHERE room_id=? AND task_id=?"


class Row(dict):
    def __init__(self, values):
        super().__init__(values)
        self._v = list(values.values())

    def __getitem__(self, key):
        if isinstance(key, int):
            return self._v[key]
        return super().__getitem__(key)


class Cur:
    def __init__(self, rows, rowcount=None):
        self.rows = list(rows)
        self.rowcount = len(self.rows) if rowcount is None else rowcount

    def fetchone(self):
        return self.rows[0] if self.rows else None

    def fetchall(self):
        return list(self.rows)

    def __iter__(self):
        return iter(self.rows)


class Conn:
    def __init__(self, answers=None):
        self.answers = list(answers or [])
        self.sql = []

    def execute(self, sql, params=()):
        self.sql.append((sql, tuple(params)))
        for a in self.answers:
            if a["match"] in sql:
                then = a.get("then")
                resp = (then.pop(0) if len(then) > 1 else then[0]) if then else (a.get("rows") or [])
                if a.get("error"):
                    raise sqlite3.OperationalError(a["error"])
                rc = a.get("rowcount")
                if rc == "params":
                    rc = len(params) - 1
                return Cur(resp, rc)
        return Cur([], 1)


class Files:
    def __init__(self, conn):
        self.conn = conn

    def _hosted_write(self, placement, fn, *, immediate):
        return fn(self.conn)

    def _hosted_read(self, placement, fn):
        return fn(self.conn)


def repo(conn):
    r = live.SQLiteHostedRoomDriverRepository.__new__(live.SQLiteHostedRoomDriverRepository)
    f = Files(conn)
    r._files = f
    r._room_files = f
    r._bare_files = f
    r._read_only = False
    return r


def plain(obj):
    if dataclasses.is_dataclass(obj):
        return {k: plain(v) for k, v in dataclasses.asdict(obj).items()}
    if isinstance(obj, dict):
        return {k: plain(v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple)):
        return [plain(v) for v in obj]
    return obj


def mk_lease(d):
    d = d or {}
    return DriverLease(room_id=d.get("room_id"), gateway_id=d.get("gateway_id"),
                       authority_epoch=int(d.get("authority_epoch") or 0),
                       process_generation=d.get("process_generation"),
                       lease_generation=int(d.get("lease_generation") or 0),
                       expires_at=float(d.get("expires_at") or 0.0),
                       reclaimed=bool(d.get("reclaimed") or False))


def mk_identity(d):
    d = d or {}
    return HostedTaskIdentity(room_id=d.get("room_id"), task_id=d.get("task_id"),
                              thread_id=d.get("thread_id"), turn_id=d.get("turn_id"))


def mk_attempt(d, lease):
    d = d or {}
    return TaskAttempt(identity=mk_identity(d.get("identity")), lease=lease,
                       execution_generation=int(d.get("execution_generation") or 0),
                       cancel_generation=int(d.get("cancel_generation") or 0))


def flat(sql):
    return " ".join(sql.split())


def answers(fx, task_then=None, thread_row=None, blocker=False, next_id=None, running=False):
    task_row = fx.get("task_row")
    if task_then is not None:
        then = task_then
    elif "task_then" in fx:
        then = [[Row(x) for x in batch] for batch in fx["task_then"]]
    else:
        then = [[Row(task_row)] if task_row is not None else [],
                [Row(task_row)] if task_row is not None else []]
    has_pubs = fx.get("has_publications", True)
    return [
        {"match": ROOM_SELECT, "rows": [Row(fx["room"])] if fx.get("room") else []},
        {"match": LEASE_SELECT, "rows": [Row(fx["lease_row"])] if fx.get("lease_row") else []},
        {"match": TASK_SELECT, "then": then},
        {"match": "SELECT * FROM hosted_room_driver_tasks WHERE room_id=? AND thread_id=? AND turn_id=?",
         "rows": [Row(thread_row)] if thread_row else []},
        {"match": "SELECT task_id, status FROM hosted_room_driver_tasks",
         "rows": [Row({"task_id": "t0", "status": "running"})] if blocker else []},
        {"match": "SELECT task_id FROM hosted_room_driver_tasks WHERE room_id=? AND status='queued'",
         "rows": [Row({"task_id": next_id})] if next_id else []},
        {"match": "SELECT 1 FROM hosted_room_driver_tasks WHERE room_id=? AND status='running'",
         "rows": [Row({"1": 1})] if running else []},
        {"match": "SELECT 1 FROM sqlite_master WHERE type='table' AND name='hosted_room_policy_publications'",
         "rows": [Row({"1": 1})] if has_pubs else []},
        {"match": "SELECT t.task_id, t.terminal_at", "rows": [Row(r) for r in fx.get("rows") or []]},
        {"match": "INSERT INTO hosted_room_driver_tasks", "rowcount": fx.get("rowcount", 1)},
        {"match": "DELETE FROM hosted_room_driver_tasks", "rowcount": "params"},
        {"match": "UPDATE hosted_room_driver_tasks", "rowcount": fx.get("rowcount", 1)},
    ]


def task_write(conn):
    for sql, params in conn.sql:
        if flat(sql).startswith("UPDATE hosted_room_driver_tasks"):
            return list(params)
    return None


def expected(r):
    f = r["func"]
    if f == "task_view":
        try:
            return plain(live._task_from_row(Row(r["row"]), idempotent=bool(r.get("idempotent"))))
        except live.TaskConflictError as exc:
            return {"error": type(exc).__name__, "message": str(exc)}
    if f == "settlement_params":
        _, params = live._settlement(r["settlement_id"], r["status"], r.get("result"), float(r.get("now") or 0))
        return list(params)
    if f == "settlement_replay":
        replay, _ = live._settlement(r["settlement_id"], r["status"], r.get("result"), float(r.get("now") or 0))
        try:
            out = replay(Row(r["row"]))
        except live.TaskConflictError as exc:
            return {"outcome": "conflict", "error": type(exc).__name__, "message": str(exc)}
        if out is None:
            return {"outcome": "none"}
        return {"outcome": "idempotent", "task": plain(out)}
    if f == "cancel_replay":
        out = live._cancel_replay(r["cancel_id"], r.get("status", "cancelled"))(Row(r["row"]))
        if out is None:
            return {"outcome": "none"}
        return {"outcome": "idempotent", "task": plain(out)}
    if f == "defer_result":
        return live.compact_json({"reason": r["reason"], "retryable": True})
    if f == "admit":
        fx = r
        ident = mk_identity(fx["identity"])
        now = float(fx.get("now") or 0)
        conn = Conn(answers(fx, thread_row=fx.get("thread_row")))
        try:
            out = repo(conn).admit_task(ident, placement="profile", payload=dict(fx["payload"]), now=now)
        except Exception as exc:  # noqa: BLE001
            return {"outcome": "error", "error": type(exc).__name__, "message": str(exc)}
        params = None
        for sql, values in conn.sql:
            if flat(sql).startswith("INSERT INTO hosted_room_driver_tasks"):
                params = list(values)
                break
        if params is not None:
            return {"outcome": "insert", "params": params}
        return {"outcome": "replay", "task": plain(out)}
    if f == "start":
        fx = r
        lease = mk_lease(fx["lease"])
        conn = Conn(answers(fx, blocker=bool(fx.get("has_blocker")), next_id=fx.get("next_task_id")))
        try:
            out = repo(conn).start_task(mk_identity(fx["identity"]), lease, placement="profile",
                                        expected_cancel_generation=int(fx.get("expected_cancel_generation") or 0),
                                        now=float(fx.get("now") or 0))
        except Exception as exc:  # noqa: BLE001
            return {"outcome": "error", "error": type(exc).__name__, "message": str(exc)}
        return {"outcome": "started", "execution_generation": out.execution_generation, "params": task_write(conn)}
    if f == "cancel":
        fx = r
        conn = Conn(answers(fx))
        try:
            out = repo(conn).cancel_task(mk_identity(fx["identity"]), placement="profile",
                                         cancel_id=fx["cancel_id"],
                                         expected_cancel_generation=int(fx.get("expected_cancel_generation") or 0),
                                         now=float(fx.get("now") or 0))
        except Exception as exc:  # noqa: BLE001
            return {"outcome": "error", "error": type(exc).__name__, "message": str(exc)}
        params = task_write(conn)
        if params is None:
            return {"outcome": "replayed", "task": plain(out)}
        return {"outcome": "cancelled", "params": params}
    if f == "settle":
        fx = r
        lease = mk_lease(fx["lease"])
        conn = Conn(answers(fx))
        try:
            out = repo(conn).settle_task(mk_attempt(fx["attempt"], lease), placement="profile",
                                         settlement_id=fx["settlement_id"], status=fx["status"],
                                         result=fx.get("result"), now=float(fx.get("now") or 0),
                                         allow_expired_lease=bool(fx.get("allow_expired")))
        except Exception as exc:  # noqa: BLE001
            return {"outcome": "error", "error": type(exc).__name__, "message": str(exc)}
        params = task_write(conn)
        if params is None:
            return {"outcome": "replayed", "task": plain(out)}
        return {"outcome": "settled", "params": params}
    if f == "generation":
        fx = r
        name = fx["name"]
        lease = mk_lease(fx["lease"])
        ident = mk_identity(fx["identity"])
        conn = Conn(answers(fx))
        now = float(fx.get("now") or 0)
        eg = int(fx.get("execution_generation") or 0)
        cg = int(fx.get("cancel_generation") or 0)
        kw = dict(placement="profile", expected_execution_generation=eg, expected_cancel_generation=cg, now=now)
        handler = repo(conn)
        try:
            if name == "resolve":
                out = handler.resolve_indeterminate_task(ident, lease, settlement_id=fx["settlement_id"],
                                                         status=fx["status"], result=fx.get("result"), **kw)
            elif name == "resolve_cancel":
                out = handler.resolve_indeterminate_cancellation(ident, lease, cancel_id=fx["cancel_id"], **kw)
            elif name == "requeue":
                out = handler.requeue_indeterminate_task(ident, lease, **kw)
            elif name == "defer":
                out = handler.defer_indeterminate_task(ident, lease, reason=fx["reason"], **kw)
            elif name == "defer_stopping":
                out = handler.defer_unconfirmed_stop(ident, lease, reason=fx["reason"], **kw)
            elif name == "reopen_deferred":
                out = handler.reopen_deferred_task(ident, lease, **kw)
            elif name == "requeue_deferred":
                out = handler.requeue_deferred_task(ident, lease, **kw)
            else:
                raise AssertionError("unknown transition %r" % (name,))
        except Exception as exc:  # noqa: BLE001
            return {"outcome": "error", "error": type(exc).__name__, "message": str(exc)}
        params = task_write(conn)
        if params is None:
            return {"outcome": "replayed", "task": plain(out)}
        return {"outcome": "applied", "params": params}
    if f == "prune":
        fx = r
        conn = Conn(answers(fx))
        count = repo(conn).prune_published_terminal_tasks(
            placement="profile", room_id=fx.get("room_id", "r1"), now=float(fx.get("now") or 0),
            retention_seconds=float(fx.get("retention_seconds") or 0), retain=int(fx.get("retain") or 0))
        ids = []
        for sql, values in conn.sql:
            if flat(sql).startswith("DELETE FROM hosted_room_driver_tasks"):
                ids = list(values[1:])
                break
        return {"count": count, "ids": ids}
    if f == "mark_indeterminate":
        fx = r
        lease = mk_lease(fx["lease"])
        conn = Conn(answers(fx))
        try:
            repo(conn).mark_attempt_indeterminate(mk_attempt(fx["attempt"], lease), lease, placement="profile",
                                                  now=float(fx.get("now") or 0))
        except Exception as exc:  # noqa: BLE001
            return {"outcome": "error", "error": type(exc).__name__, "message": str(exc)}
        params = task_write(conn)
        if params is None:
            return {"outcome": "skipped"}
        return {"outcome": "marked", "params": params}
    if f == "settle_stopping":
        fx = r
        lease = mk_lease(fx["lease"])
        conn = Conn(answers(fx))
        try:
            out = repo(conn).settle_stopping_task(
                mk_identity(fx["identity"]), lease, placement="profile",
                expected_execution_generation=int(fx.get("expected_execution_generation") or 0),
                expected_cancel_generation=int(fx.get("expected_cancel_generation") or 0),
                settlement_id=fx["settlement_id"], status=fx["status"], result=fx.get("result"),
                now=float(fx.get("now") or 0))
        except Exception as exc:  # noqa: BLE001
            return {"outcome": "error", "error": type(exc).__name__, "message": str(exc)}
        params = task_write(conn)
        if params is None:
            return {"outcome": "replayed", "task": plain(out)}
        return {"outcome": "settled", "params": params}
    if f == "begin_cancel":
        fx = r
        conn = Conn(answers(fx))
        try:
            out = repo(conn).begin_task_cancel(mk_identity(fx["identity"]), placement="profile",
                                               cancel_id=fx["cancel_id"],
                                               expected_cancel_generation=int(fx.get("expected_cancel_generation") or 0),
                                               now=float(fx.get("now") or 0))
        except Exception as exc:  # noqa: BLE001
            return {"outcome": "error", "error": type(exc).__name__, "message": str(exc)}
        params = task_write(conn)
        if params is None:
            return {"outcome": "replayed", "task": plain(out)}
        return {"outcome": "stopping", "params": params}
    if f == "complete_cancel":
        fx = r
        conn = Conn(answers(fx))
        try:
            out = repo(conn).complete_task_cancel(mk_identity(fx["identity"]), placement="profile",
                                                  cancel_id=fx["cancel_id"],
                                                  expected_cancel_generation=int(fx.get("expected_cancel_generation") or 0),
                                                  now=float(fx.get("now") or 0))
        except Exception as exc:  # noqa: BLE001
            return {"outcome": "error", "error": type(exc).__name__, "message": str(exc)}
        params = task_write(conn)
        if params is None:
            return {"outcome": "replayed", "task": plain(out)}
        return {"outcome": "cancelled", "params": params}
    if f == "requeue_not_admitted":
        fx = r
        lease = mk_lease(fx["lease"])
        conn = Conn(answers(fx))
        try:
            out = repo(conn).requeue_not_admitted_task(mk_attempt(fx["attempt"], lease), placement="profile",
                                                       now=float(fx.get("now") or 0))
        except Exception as exc:  # noqa: BLE001
            return {"outcome": "error", "error": type(exc).__name__, "message": str(exc)}
        params = task_write(conn)
        if params is None:
            return {"outcome": "replayed", "task": plain(out)}
        return {"outcome": "requeued", "params": params}
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
