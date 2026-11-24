#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/gateway/hosted_room_driver.py
(gate w29_hostedroom_8927a5_p2: fenced lease lifecycle and row predicates).

The live verbs are driven through a scripted connection and a stub file handle:
``_hosted_write`` runs the verb's transaction body against the fixture's rows
and records every statement and bind tuple.  That is the seam -- no file is
opened, no transaction is begun, no clock is read beyond the fixture's ``now``
-- and what is compared is the decision (outcome, message, bind values) the
live code makes from those rows.
"""
import json
import sqlite3
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_hostedroom_8927a5_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.gateway.hosted_rooms import DriverLease, HostedTaskIdentity  # noqa: E402
from persistence.sqlite.gateway import hosted_room_driver as live  # noqa: E402

WANTED = {
    "run_fence", "lease_from_row", "lease_row_matches", "task_identity", "load_task", "active_room",
    "room_authority", "lease_gate", "generations_match", "cancel_generation_guard", "acquire", "renew", "release",
    "recover",
}

ROOM_SELECT = "SELECT room_id, authority_gateway_id, authority_epoch, disbanded_at FROM hosted_rooms"
LEASE_SELECT = "SELECT * FROM hosted_room_driver_leases WHERE room_id=?"


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
                return Cur(resp, a.get("rowcount"))
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


def room_answers(room, no_table=False):
    if no_table:
        return [{"match": ROOM_SELECT, "error": "no such table: hosted_rooms"}]
    return [{"match": ROOM_SELECT, "rows": [Row(room)] if room is not None else []}]


def flat(sql):
    return " ".join(sql.split())


def lease_stats(conn):
    """The first lease statement that wrote something: (kind, params)."""
    for sql, params in conn.sql:
        s = flat(sql)
        if s.startswith("INSERT INTO hosted_room_driver_leases"):
            return "insert", list(params)
        if s.startswith("UPDATE hosted_room_driver_leases SET gateway_id="):
            return "reclaim", list(params)
        if s.startswith("UPDATE hosted_room_driver_leases SET expires_at=?, updated_at=?"):
            return "renew", list(params)
        if s.startswith("UPDATE hosted_room_driver_leases SET expires_at=?, updated_at=?, released_at=?"):
            return "release", list(params)
    return None, None


def expected(r):
    f = r["func"]
    if f == "run_fence":
        return list(live._run_fence(mk_lease(r.get("lease"))))
    if f == "lease_from_row":
        import dataclasses

        return dataclasses.asdict(live._lease_from_row(Row(r["row"]), reclaimed=bool(r.get("reclaimed"))))
    if f == "lease_row_matches":
        row = Row(r["row"]) if r.get("row") is not None else None
        return live._lease_row_matches(row, mk_lease(r.get("lease")))
    if f == "task_identity":
        import dataclasses

        return dataclasses.asdict(live._task_identity_from_row(Row(r["row"])))
    if f == "load_task":
        conn = Conn([{"match": "SELECT * FROM hosted_room_driver_tasks WHERE room_id=? AND task_id=?",
                      "rows": [Row(r["row"])] if r.get("row") is not None else []}])
        try:
            live._load_task(conn, mk_identity(r.get("identity")), required=bool(r.get("required", True)))
        except live.TaskConflictError as exc:
            return {"error": type(exc).__name__, "message": str(exc)}
        return None
    if f == "active_room":
        conn = Conn(room_answers(r.get("room"), bool(r.get("no_table"))))
        try:
            live._load_active_room(conn, (r.get("room") or {}).get("room_id", "r1"))
        except live.RoomUnavailableError as exc:
            return {"error": type(exc).__name__, "message": str(exc)}
        return None
    if f == "room_authority":
        conn = Conn(room_answers(r.get("room")))
        try:
            live._require_room_authority(conn, (r.get("room") or {}).get("room_id", "r1"),
                                         r.get("gateway_id"), int(r.get("epoch") or 0))
        except live.DriverStateError as exc:
            return {"error": type(exc).__name__, "message": str(exc)}
        return None
    if f == "lease_gate":
        answers = room_answers(r.get("room")) + [
            {"match": LEASE_SELECT, "rows": [Row(r["lease_row"])] if r.get("lease_row") is not None else []}]
        conn = Conn(answers)
        try:
            live._require_active_lease(conn, mk_lease(r.get("lease")), now=float(r.get("now") or 0),
                                       allow_expired=bool(r.get("allow_expired")))
        except live.DriverStateError as exc:
            return {"error": type(exc).__name__, "message": str(exc)}
        return None
    if f == "generations_match":
        return live._generations_match(Row(r["row"]), r.get("status"), int(r.get("execution_generation") or 0),
                                       int(r.get("cancel_generation") or 0))
    if f == "cancel_generation_guard":
        try:
            live._require_cancel_generation(Row(r["row"]), int(r.get("expected") or 0))
        except live.StaleTaskError as exc:
            return {"error": type(exc).__name__, "message": str(exc)}
        return None
    if f == "acquire":
        row = r.get("lease_row")
        now = float(r.get("now") or 0)
        expires_at = now + float(r.get("ttl_seconds") or 0)
        reload = None
        if row is None:
            reload = {"room_id": r["room_id"], "gateway_id": r["gateway_id"],
                      "authority_epoch": int(r.get("authority_epoch") or 0),
                      "process_generation": r["process_generation"], "lease_generation": 1,
                      "expires_at": expires_at, "acquired_at": now, "updated_at": now, "released_at": None}
        else:
            renew = (row.get("gateway_id") == r["gateway_id"]
                     and int(row.get("authority_epoch") or 0) == int(r.get("authority_epoch") or 0)
                     and row.get("process_generation") == r["process_generation"]
                     and row.get("released_at") is None and float(row.get("expires_at") or 0) > now)
            if renew:
                reload = dict(row, expires_at=max(float(row.get("expires_at") or 0), expires_at), updated_at=now)
            else:
                reload = dict(row, gateway_id=r["gateway_id"], authority_epoch=int(r.get("authority_epoch") or 0),
                              process_generation=r["process_generation"],
                              lease_generation=int(row.get("lease_generation") or 0) + 1, expires_at=expires_at,
                              acquired_at=now, updated_at=now, released_at=None)
        conn = Conn(room_answers(r.get("room")) + [
            {"match": LEASE_SELECT, "then": [[Row(row)] if row is not None else [], [Row(reload)]]},
            {"match": "INSERT INTO hosted_room_driver_leases", "rowcount": r.get("rowcount", 1)},
            {"match": "UPDATE hosted_room_driver_leases", "rowcount": r.get("rowcount", 1)}])
        try:
            repo(conn).acquire_lease(placement="profile", room_id=r["room_id"], gateway_id=r["gateway_id"],
                                     authority_epoch=int(r.get("authority_epoch") or 0),
                                     process_generation=r["process_generation"],
                                     ttl_seconds=float(r.get("ttl_seconds") or 0), now=now)
        except Exception as exc:  # noqa: BLE001
            outcome = "held" if isinstance(exc, live.LeaseHeldError) else "error"
            return {"outcome": outcome, "error": type(exc).__name__, "message": str(exc), "params": None}
        kind, params = lease_stats(conn)
        return {"outcome": kind, "error": None, "message": None, "params": params}
    if f == "renew":
        conn = Conn(room_answers(r.get("room")) + [
            {"match": LEASE_SELECT, "rows": [Row(r["lease_row"])] if r.get("lease_row") is not None else []},
            {"match": "UPDATE hosted_room_driver_leases", "rowcount": r.get("rowcount", 1)}])
        try:
            repo(conn).renew_lease(mk_lease(r.get("lease")), placement="profile",
                                   ttl_seconds=float(r.get("ttl_seconds") or 0), now=float(r.get("now") or 0))
        except Exception as exc:  # noqa: BLE001
            return {"outcome": "error", "error": type(exc).__name__, "message": str(exc), "params": None}
        kind, params = lease_stats(conn)
        return {"outcome": "renewed", "error": None, "message": None, "params": params}
    if f == "release":
        conn = Conn(room_answers(r.get("room")) + [
            {"match": LEASE_SELECT, "rows": [Row(r["lease_row"])] if r.get("lease_row") is not None else []},
            {"match": "SELECT 1 FROM hosted_room_driver_tasks WHERE room_id=? AND status='running'",
             "rows": [Row({"1": 1})] if r.get("has_running") else []},
            {"match": "UPDATE hosted_room_driver_leases", "rowcount": r.get("rowcount", 1)}])
        try:
            result = repo(conn).release_lease(mk_lease(r.get("lease")), placement="profile",
                                              now=float(r.get("now") or 0))
        except Exception as exc:  # noqa: BLE001
            return {"outcome": "error", "error": type(exc).__name__, "message": str(exc), "params": None}
        kind, params = lease_stats(conn)
        return {"outcome": "idempotent" if result.idempotent else "released", "error": None, "message": None,
                "params": params}
    if f == "recover":
        import dataclasses

        r0 = r
        stale = [Row(x) for x in r0.get("stale_rows") or []]
        queued = [Row(x) for x in r0.get("queued_rows") or []]
        indeterminate = [Row(x) for x in r0.get("indeterminate_rows") or []]
        conn = Conn(room_answers(r0.get("room")) + [
            {"match": LEASE_SELECT, "rows": [Row(r0["lease_row"])] if r0.get("lease_row") else []},
            {"match": "SELECT * FROM hosted_room_driver_tasks WHERE room_id=? AND status='running' AND NOT",
             "rows": stale},
            {"match": "UPDATE hosted_room_driver_tasks", "rowcount": 1},
            {"match": "AND status=? ORDER BY", "then": [queued, indeterminate]}])
        try:
            out = repo(conn).recover_room(mk_lease(r0.get("lease")), placement="profile",
                                          now=float(r0.get("now") or 0))
        except Exception as exc:  # noqa: BLE001
            return {"outcome": "error", "error": type(exc).__name__, "message": str(exc)}
        params = None
        for sql, values in conn.sql:
            if flat(sql).startswith("UPDATE hosted_room_driver_tasks"):
                params = list(values)
                break
        return {"outcome": "recovered", "update": params is not None, "params": params,
                "stale_ids": [row.get("task_id") for row in stale],
                "queued": [dataclasses.asdict(i) for i in out.queued],
                "indeterminate": [dataclasses.asdict(i) for i in out.indeterminate]}
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
