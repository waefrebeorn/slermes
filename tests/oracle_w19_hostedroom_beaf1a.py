#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/gateway/hosted_room_replicas.py (gate w19_hostedroom_beaf1a)."""
import ast
import importlib.util
import json
import sqlite3
import sys
import types
from contextlib import contextmanager
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
Q = REPO.parent / "hermes-agent"
SRC = Q / "persistence" / "sqlite" / "gateway" / "hosted_room_replicas.py"
RULES = Q / "session_rules" / "hosted_rooms.py"
GATE = "w19_hostedroom_beaf1a"
FUNCS = {"schema_ready", "schema_sql", "control_event", "event_bytes", "store_values", "row_state",
         "ingest_plan", "promote_plan", "demote_plan"}
SHAPE = ("error", "result", "control_row", "room_row", "update_params", "rows")


def stub_module(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    return mod


def err_class(name, base=Exception):
    return type(name, (base,), {})


class Row(dict):
    def __getitem__(self, key):
        if isinstance(key, int):
            return list(self.values())[key]
        return dict.__getitem__(self, key)


class Cur:
    def __init__(self, rows):
        self.rows = rows

    def fetchone(self):
        return self.rows[0] if self.rows else None

    def fetchall(self):
        return list(self.rows)

    def __iter__(self):
        return iter(self.rows)


class FakeConn:
    def __init__(self, c):
        self.c = c
        self.calls = []

    def execute(self, sql, params=None):
        self.calls.append((sql, params))
        s = " ".join(sql.split())
        c = self.c
        if "sqlite_master" in s:
            return Cur([])
        if s.startswith("SELECT authority_gateway_id, authority_epoch, last_seq, latest_seq, event_bytes FROM hosted_room_replicas"):
            return Cur([Row(c["stored"])] if c.get("found") else [])
        if s.startswith("SELECT COUNT(*) FROM hosted_room_replicas"):
            return Cur([(c.get("room_count", 0),)])
        if s.startswith("SELECT 1 FROM hosted_rooms WHERE room_id=?"):
            return Cur([(1,)] if c.get("room_exists") else [])
        if s.startswith("SELECT 1 FROM hosted_room_retired_ids"):
            return Cur([(1,)] if c.get("retired_exists") else [])
        if s.startswith("SELECT * FROM hosted_room_replicas"):
            return Cur([Row(c["stored"])] if c.get("found") else [])
        if s.startswith("SELECT authority_gateway_id, authority_epoch, next_seq FROM hosted_rooms"):
            return Cur([Row(c["room"])] if c.get("found") else [])
        return Cur([])


def load_live():
    e = err_class("HostedRoomError")
    rerr = err_class("ReplicaError")
    stub_module("persistence", JsonValue=object)
    stub_module("persistence.gateway")
    stub_module("persistence.gateway.hosted_rooms",
                HostedRoomError=e, ReplicaError=rerr,
                ReplicaGapError=err_class("ReplicaGapError", rerr),
                ReplicaEpochRegressionError=err_class("ReplicaEpochRegressionError", rerr),
                ReplicaPage=object, HostedPlacement=object,
                RoomConflictError=err_class("RoomConflictError", e),
                AuthorityHistoryError=err_class("AuthorityHistoryError"))
    stub_module("session_rules")
    spec = importlib.util.spec_from_file_location("session_rules.hosted_rooms", str(RULES))
    rules = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = rules
    spec.loader.exec_module(rules)

    import persistence.gateway.hosted_rooms as pg
    g = {
        "__name__": "hrr_live", "sqlite3": sqlite3, "Mapping": dict, "Path": Path, "Any": object,
        "JsonValue": object, "HostedPlacement": object, "ReplicaPage": object,
        "ReplicaError": pg.ReplicaError, "ReplicaGapError": pg.ReplicaGapError,
        "ReplicaEpochRegressionError": pg.ReplicaEpochRegressionError,
        "RoomConflictError": pg.RoomConflictError,
        "MAX_REPLICA_EVENT_BYTES": rules.MAX_REPLICA_EVENT_BYTES,
        "MAX_REPLICA_ROOMS": rules.MAX_REPLICA_ROOMS,
        "utf8_len": rules.utf8_len, "clock": rules.clock,
        "_actor_json": rules._actor_json, "_control_event": rules._control_event,
        "_event_bytes": rules._event_bytes, "_payload_json": rules._payload_json,
        "SQLiteStore": object, "register_repository": lambda *a, **k: None,
        "table_exists": _table_exists,
        "ROOMS_STEP": None, "SchemaStep": object,
    }
    g["translated"] = _translated
    g["HostedFiles"] = HostedFiles
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
        elif isinstance(node, ast.FunctionDef):
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
        elif isinstance(node, ast.ClassDef) and node.name == "SQLiteHostedRoomReplicaRepository":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
    return g


@contextmanager
def _translated():
    yield None


_TABLES = {}


def _table_exists(conn, name):
    return bool(_TABLES.get(name))


class HostedFiles:
    def __init__(self, store, steps):
        self.store = store
        self.steps = steps

    def _hosted_write(self, placement, fn, immediate=True):
        return fn(self.store.conn)

    def _hosted_read(self, placement, fn):
        return fn(self.store.conn)


class Store:
    def __init__(self, conn):
        self.conn = conn


G = load_live()


class Page:
    def __init__(self, d):
        self.events = d.get("events") or []
        self.authority_gateway_id = d.get("gateway_id")
        self.authority_epoch = d.get("page_epoch")
        self.latest_seq = d.get("page_latest_seq")


def repo_for(conn):
    repo = G["SQLiteHostedRoomReplicaRepository"](Store(conn))
    return repo


def shape(error=None, result=None, control=None, room=None, update=None, rows=None):
    return {"error": error, "result": result, "control_row": control,
            "room_row": room, "update_params": update, "rows": rows if rows is not None else []}


def err_text(exc):
    return "%s: %s" % (type(exc).__name__, exc)


def do_ingest(r):
    stored = {"authority_gateway_id": r.get("gateway_id"), "authority_epoch": r.get("stored_epoch", 0),
              "last_seq": r.get("stored_last_seq", 0), "latest_seq": r.get("stored_last_seq", 0),
              "event_bytes": r.get("stored_bytes", 0), "room_id": r.get("room_id"),
              "name": r.get("room_name"), "members_json": r.get("members_json", "{}"),
              "created_at": r.get("now", 0.0), "updated_at": r.get("now", 0.0)}
    conn = FakeConn({"found": r.get("found", False), "room_count": r.get("room_count", 0), "stored": stored})
    try:
        out = repo_for(conn).ingest_page(placement=object(), room_id=r.get("room_id"),
                                         room_name=r.get("room_name"),
                                         members_json=r.get("members_json", "{}"),
                                         page=Page(r), now=r.get("now"))
    except BaseException as exc:
        return shape(error=err_text(exc))
    rows = [list(p) for (sql, p) in conn.calls
            if p and "INSERT INTO hosted_room_replica_events" in " ".join(sql.split())]
    return shape(result=out, rows=rows)


def do_promote(r):
    stored = {"room_id": r.get("room_id"), "name": r.get("name"),
              "members_json": r.get("members_json", "{}"),
              "authority_gateway_id": r.get("authority_gateway_id"),
              "authority_epoch": r.get("authority_epoch", 0), "last_seq": r.get("last_seq", 0),
              "latest_seq": r.get("last_seq", 0), "event_bytes": r.get("event_bytes", 0),
              "created_at": r.get("now", 0.0), "updated_at": r.get("now", 0.0)}
    conn = FakeConn({"found": r.get("found", False), "stored": stored,
                     "room_exists": r.get("room_exists", False),
                     "retired_exists": r.get("retired_exists", False)})
    try:
        out = repo_for(conn).promote_replica(placement=object(), room_id=r.get("room_id"),
                                             local_gateway_id=r.get("local_gateway_id"),
                                             reason=r.get("reason", "authority-unreachable"),
                                             now=r.get("now"))
    except BaseException as exc:
        return shape(error=err_text(exc))
    control = room = None
    for sql, p in conn.calls:
        s = " ".join(sql.split())
        if not p:
            continue
        if s.startswith("INSERT INTO hosted_room_events") and len(p) == 8:
            control = list(p)
        if s.startswith("INSERT INTO hosted_rooms"):
            room = list(p)
    return shape(result=out, control=control, room=room)


def do_demote(r):
    room = {"authority_gateway_id": r.get("current_gateway"),
            "authority_epoch": r.get("current_epoch", 0), "next_seq": r.get("next_seq", 1)}
    conn = FakeConn({"found": r.get("found", False), "room": room})
    try:
        out = repo_for(conn).demote_room(placement=object(), room_id=r.get("room_id"),
                                         observed_gateway_id=r.get("observed_gateway"),
                                         observed_epoch=r.get("observed_epoch"),
                                         local_gateway_id=r.get("local_gateway_id"),
                                         now=r.get("now"))
    except BaseException as exc:
        return shape(error=err_text(exc))
    control = update = None
    for sql, p in conn.calls:
        s = " ".join(sql.split())
        if not p:
            continue
        if s.startswith("INSERT INTO hosted_room_events") and len(p) == 8:
            control = list(p)
        if s.startswith("UPDATE hosted_rooms"):
            update = list(p)
    return shape(result=out, control=control, update=update)


def run_case(r):
    f = r.get("func")
    if f == "schema_ready":
        _TABLES.clear()
        _TABLES["hosted_room_replicas"] = bool(r.get("replicas"))
        _TABLES["hosted_room_replica_events"] = bool(r.get("replica_events"))
        return G["_replica_schema_ready"](None, None)
    if f == "schema_sql":
        conn = FakeConn({})
        G["_initialize_replica_schema"](conn)
        sqls = [s for (s, p) in conn.calls]
        return {"replicas": sqls[0], "events": sqls[1]}
    if f == "control_event":
        ev = G["_control_event"](r.get("kind"), int(r.get("epoch")), dict(r.get("payload") or {}))
        return {"event_id": ev[0], "kind": ev[1], "actor_json": ev[2], "payload_json": ev[3]}
    if f == "event_bytes":
        return G["_event_bytes"](dict(r.get("event") or {}))
    if f == "store_values":
        stored = {"authority_gateway_id": r.get("gateway_id"),
                  "authority_epoch": int(r.get("epoch")),
                  "last_seq": int(r.get("new_last")), "latest_seq": int(r.get("new_last")),
                  "event_bytes": 0, "room_id": "room-1", "name": "", "members_json": "{}"}
        conn = FakeConn({"stored": stored})
        G["_store_replica"](conn, is_new=bool(r.get("is_new")), room_id="room-1",
                            room_name=r.get("room_name"), members_json=r.get("members_json"),
                            authority={"gateway_id": r.get("gateway_id"), "epoch": int(r.get("epoch"))},
                            new_last=int(r.get("new_last")), latest_seq=int(r.get("latest_seq")),
                            added_bytes=int(r.get("added_bytes", 0)), now=float(r.get("now") or 0))
        params = [p for (sql, p) in conn.calls if p][-1]
        values = list(params[1:7]) if r.get("is_new") else list(params[0:6])
        keys = ["name", "members_json", "authority_gateway_id", "authority_epoch", "last_seq", "latest_seq"]
        return dict(zip(keys, values))
    if f == "row_state":
        stored = {"authority_gateway_id": r.get("gateway_id", "g"),
                  "authority_epoch": int(r.get("stored_epoch", 0)),
                  "last_seq": int(r.get("stored_last_seq", 0)),
                  "latest_seq": int(r.get("stored_last_seq", 0)),
                  "event_bytes": int(r.get("stored_bytes", 0))}
        conn = FakeConn({"found": bool(r.get("found")), "stored": stored,
                         "room_count": int(r.get("room_count", 0))})
        try:
            row, epoch, last_seq, size = G["_replica_row_state"](conn, r.get("room_id", "room-1"))
        except BaseException as exc:
            return {"error": err_text(exc), "found": False, "stored_epoch": 0,
                    "last_seq": 0, "event_bytes": 0}
        return {"error": None, "found": row is not None, "stored_epoch": epoch,
                "last_seq": last_seq, "event_bytes": size}
    if f == "ingest_plan":
        return do_ingest(r)
    if f == "promote_plan":
        return do_promote(r)
    if f == "demote_plan":
        return do_demote(r)
    return None


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
        f = r.get("func")
        if f not in FUNCS:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as exc:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
