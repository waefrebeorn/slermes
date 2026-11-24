#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/authority/_binding_api.py
(w20_bindingapi_eaedd9).

The live module is imported against stubbed authority packages, and every case
runs ``bind_api`` against a real in-memory SQLite database seeded from the
fixture: the SQL, the conflict clauses and the transaction rollback are the
real ones. ``_epoch`` is the upstream fence check, ``_json`` the canonical
encoder, ``keep_passed_fences`` and the admission gate are scripted seams.
"""
import contextlib
import importlib.util
import json
import os
import sqlite3
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class AuthorityRefusal(Exception):
    def __init__(self, reason, **kw):
        super().__init__(reason)
        self.reason = reason
        self.kw = kw


class Gate:
    def __init__(self, spec):
        self.spec = spec

    def refusal(self):
        return AuthorityRefusal(self.spec.get("reason", "permission_denied"))


def _json(value):
    if not isinstance(value, dict):
        raise AuthorityRefusal("invalid_params")
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


for pkg in ("persistence", "persistence.core", "persistence.authority",
            "persistence.sqlite", "persistence.sqlite.authority",
            "persistence.sqlite.conversation", "persistence.sqlite.conversation._store"):
    if pkg not in sys.modules:
        _mk(pkg)

EPOCHS = []
_mk("persistence.authority.bindings", api_generation_id=lambda sid: sid)
_mk("persistence.authority.admissions",
    raise_gate=lambda gate: (_ for _ in ()).throw(gate.refusal()) if gate is not None else None)
_mk("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)


def _epoch(conn, epoch):
    EPOCHS.append(epoch)
    row = conn.execute("SELECT epoch FROM runtime_epoch WHERE singleton=1").fetchone()
    if type(epoch) is not int or row is None or row[0] != epoch:
        raise AuthorityRefusal("stale_epoch")


_mk("persistence.sqlite.authority._runtime", _epoch=_epoch, _json=_json)
_mk("persistence.sqlite.authority._runtime_retirement",
    keep_passed_fences=lambda conn, derive, sid: bool(CURRENT.get("fence_ok", True)))
_mk("persistence.sqlite.conversation._store.local",
    API_BINDING_PREFIX="gateway.api.binding.v1.",
    API_DECLARED_PREFIX="gateway.api.conversation.v1.",
    RETIRED_PREFIX="gateway.retired_session.v1.")
_mk("persistence.sqlite.conversation._store.retirement",
    RETIRED_PREFIX="gateway.retired_session.v1.")

NAME = "persistence.sqlite.authority._binding_api_w20"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "persistence", "sqlite", "authority", "_binding_api.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

CURRENT = {}

SCHEMA = """
CREATE TABLE state_meta(key TEXT PRIMARY KEY, value TEXT);
CREATE TABLE sessions(id TEXT PRIMARY KEY, source TEXT, title TEXT, hidden INTEGER,
                      started_at INTEGER, session_key TEXT, chat_id TEXT, user_id TEXT,
                      chat_type TEXT, origin_json TEXT);
CREATE TABLE gateway_routing(scope TEXT, session_key TEXT, entry_json TEXT, updated_at INTEGER,
                             PRIMARY KEY(scope, session_key));
CREATE TABLE runtime_epoch(singleton INTEGER PRIMARY KEY, epoch INTEGER);
"""


class Holder:
    def __init__(self, conn):
        self.conn = conn

    def _execute_write(self, write):
        try:
            write(self.conn)
            self.conn.commit()
        except BaseException:
            self.conn.rollback()
            raise

    @contextlib.contextmanager
    def _read_ctx(self):
        yield self.conn


def build(request):
    conn = sqlite3.connect(":memory:")
    conn.row_factory = sqlite3.Row
    conn.executescript(SCHEMA)
    if "runtime_epoch" in request:
        if request["runtime_epoch"] is not None:
            conn.execute("INSERT INTO runtime_epoch VALUES(1,?)", (request["runtime_epoch"],))
    else:
        conn.execute("INSERT INTO runtime_epoch VALUES(1,?)", (request.get("epoch"),))
    for key, value in (request.get("state_meta") or {}).items():
        conn.execute("INSERT INTO state_meta VALUES(?,?)", (key, value))
    for row in request.get("sessions") or []:
        conn.execute(
            "INSERT INTO sessions(id, source, title, hidden, started_at, session_key, chat_id,"
            " user_id, chat_type, origin_json) VALUES(?,?,?,?,?,?,?,?,?,?)",
            (row.get("id"), row.get("source"), row.get("title"), row.get("hidden"),
             row.get("started_at"), row.get("session_key"), row.get("chat_id"),
             row.get("user_id"), row.get("chat_type"),
             None if row.get("origin") is None else json.dumps(row["origin"])))
    for row in request.get("routing") or []:
        conn.execute("INSERT INTO gateway_routing VALUES('',?,?,?)",
                     (row.get("session_key"), json.dumps(row.get("entry") or {}),
                      row.get("updated_at")))
    conn.commit()
    return conn


def snapshot(conn):
    meta = {r["key"]: r["value"]
            for r in conn.execute("SELECT key, value FROM state_meta ORDER BY key")}
    sessions = []
    for r in conn.execute("SELECT * FROM sessions ORDER BY id"):
        origin = r["origin_json"]
        sessions.append({
            "id": r["id"], "source": r["source"], "title": r["title"],
            "hidden": r["hidden"], "started_at": r["started_at"],
            "session_key": r["session_key"], "chat_id": r["chat_id"],
            "user_id": r["user_id"], "chat_type": r["chat_type"],
            "origin": json.loads(origin) if origin is not None else None,
        })
    routing = []
    for r in conn.execute("SELECT session_key, entry_json, updated_at FROM gateway_routing"
                          " WHERE scope='' ORDER BY session_key"):
        routing.append({"session_key": r["session_key"],
                        "updated_at": r["updated_at"],
                        "entry": json.loads(r["entry_json"])})
    return {"epoch": EPOCHS[-1] if EPOCHS else None, "state_meta": meta,
            "sessions": sessions, "routing": routing}


def plain(v):
    if isinstance(v, dict):
        return {k: plain(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [plain(x) for x in v]
    return v


def expected(r):
    f = r.get("func")
    if f == "bind":
        CURRENT.clear()
        CURRENT.update(r)
        del EPOCHS[:]
        conn = build(r)
        holder = Holder(conn)
        gate = None if r.get("gate") is None else Gate(r["gate"])
        try:
            live.bind_api(
                holder,
                epoch=r.get("epoch"),
                session_id=r.get("session_id", ""),
                route=r.get("route", ""),
                storage_source=r.get("storage_source", ""),
                title=r.get("title", ""),
                declared_key=r.get("declared_key"),
                caller_key=r.get("caller_key"),
                source=r.get("source"),
                entry=r.get("entry"),
                receipt=r.get("receipt") or {},
                now=r.get("now"),
                gate=gate,
                walked_from=r.get("walked_from"),
            )
            return {"status": "ok", "value": snapshot(conn)}
        except AuthorityRefusal as exc:
            return {"status": "refused", "reason": exc.reason}
        except Exception as exc:  # noqa: BLE001 - reported verbatim, like the C side
            return {"status": "raised", "type": type(exc).__name__, "message": str(exc)}
    if f == "receipt":
        del EPOCHS[:]
        conn = build({"state_meta": r.get("state_meta") or {}, "epoch": None})
        return live.api_binding_receipt(Holder(conn), r.get("session_id", ""))
    if f == "declared_session":
        conn = build({"state_meta": r.get("state_meta") or {}, "epoch": None})
        return live.declared_session(Holder(conn), r.get("key", ""))
    if f == "heal_replacement":
        conn = build({"state_meta": r.get("state_meta") or {}, "epoch": None})
        return bool(live._heal_replacement(conn, r.get("caller", ""), r.get("holder", "")))
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
        exp = plain(expected(r))
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
print("ORACLE w20_bindingapi_eaedd9: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
