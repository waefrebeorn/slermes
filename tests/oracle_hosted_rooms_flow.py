#!/usr/bin/env python3
"""Oracle for the gateway/hosted_rooms.py room-lifecycle FLOW port
(src/gateway/hosted_rooms_flow.c).

Drives the LIVE decision chains with SCRIPTED seams: the store verbs, the wall
clock (common.clock -> time.time), get_hermes_home, sqlite conn.execute and the
claimed _prune_disbanded_rooms_locked helper are recording stubs fed the same
scripts the C harness feeds its universal seam. Compares the DECISION SEQUENCE
(ordered seam events {"name","args","ret"} in "trace") plus the final return /
raised exception ("ret").

The live defs are AST-extracted from gateway/hosted_rooms.py (module constants
seeded Assign AND AnnAssign, plain-Name targets only, re-seeded AFTER the defs
so _LEGACY_ACTOR_JSON / the partial aliases bind). Support leaves are the REAL
hosted_rooms_common functions (identifier/bounded_int/canonical_json/
compact_json/utf8_len/clock/table_columns) and the REAL persistence exception
classes + record dataclasses, AST-extracted from their modules. Every exec
namespace is a registered sys.modules module (py3.14 dataclasses _is_type needs
cls.__module__ resolvable); annotations compile as strings (PEP 563 flag).

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE hosted_rooms_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)};
  KeyError carries str(exc) == repr(key). No ported flow has an except site,
  so scripted BaseException kinds propagate verbatim.
- conn.execute scripts are ROWS lists (fetchone = rows[0] or None); the cursor
  double records only the execute event (fetchone/fetchall/iteration are pure
  projections on both sides).
- placement travels as its Literal value string; owner as its mapping;
  dataclass results project via asdict; tuples as lists; frozenset as a
  sorted-unique list (canon order == the C sort-by-JSON-serialization).
- None-valued seam arg keys are dropped on both sides.
- Integral floats normalize to int on both sides; bounded_int positions carry
  int literals only (libjson number identity); str()/repr() and canonical-JSON
  corpus is ASCII (container repr included). default_db_path compares
  Path.as_posix() over drive-less posix paths.
"""
import ast
import json
import os
import re
import sys
import types
from dataclasses import dataclass, field, fields, replace, asdict, is_dataclass
from functools import partial
from pathlib import Path
from typing import Any, Mapping
import __future__ as _fut

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stdin, "reconfigure"):
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
HR_SRC = os.path.join(Q, "gateway", "hosted_rooms.py")
COMMON_SRC = os.path.join(Q, "gateway", "hosted_rooms_common.py")
PERSIST_SRC = os.path.join(Q, "persistence", "gateway", "hosted_rooms.py")
ERRORS_SRC = os.path.join(Q, "persistence", "core", "errors.py")

_FLAGS = _fut.annotations.compiler_flag

# The 18 ported flows + the unclaimed helpers they compose (run real on both
# sides). _prune_disbanded_rooms_locked is claimed elsewhere -> recorder seam.
WANTED = {
    "create_room", "list_rooms", "room_has_peer_obligations",
    "retire_room_link_records", "due_room_grant_revokes",
    "settle_room_grant_revoke", "drop_remote_run_receipt",
    "_typed", "_grant_claims", "_reservation_claims", "_reservation_grant",
    "default_db_path", "_room_row", "_is_retired", "_require_authority",
    "_migrate_legacy_columns", "_prepare_event", "_insert_event",
    "_actor_id", "_bounded_limit", "_non_negative", "_validate_members",
    "_claim_values", "_system_actor_json", "_gateway_event_bytes",
}
FUNCS = [
    "create_room", "list_rooms", "room_has_peer_obligations",
    "retire_room_link_records", "due_room_grant_revokes",
    "settle_room_grant_revoke", "drop_remote_run_receipt",
    "_typed", "_grant_claims", "_reservation_claims", "_reservation_grant",
    "default_db_path", "_room_row", "_is_retired", "_require_authority",
    "_migrate_legacy_columns", "_prepare_event", "_insert_event",
]
SEAM_NAMES = {
    "hosted_rooms.create_room", "hosted_rooms.list_rooms",
    "hosted_room_peers.room_has_peer_obligations",
    "hosted_room_peers.retire_room_link_records",
    "hosted_room_peers.due_room_grant_revokes",
    "hosted_room_peers.settle_room_grant_revoke",
    "hosted_room_peers.drop_remote_run_receipt",
    "get_hermes_home", "clock", "conn.execute", "_prune_disbanded_rooms_locked",
}


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    import textwrap
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def seed_constants(src, ns, path):
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, path)
            except Exception:
                pass


def class_nodes(src, names):
    return [n for n in ast.parse(src).body
            if isinstance(n, ast.ClassDef) and n.name in names]


def func_nodes(src, names):
    return [n for n in ast.parse(src).body
            if isinstance(n, ast.FunctionDef) and n.name in names]


def extract():
    mod = new_module("hrf_oracle_main")
    ns = mod.__dict__
    import hashlib
    import sqlite3
    import time
    ns.update({
        "json": json, "re": re, "os": os, "hashlib": hashlib, "sqlite3": sqlite3,
        "time": time, "partial": partial, "Path": Path, "Any": Any, "Mapping": Mapping,
        "dataclass": dataclass, "field": field, "dataclass_field": field,
        "fields": fields, "replace": replace, "asdict": asdict,
        "TYPE_CHECKING": False,
    })
    # real core error bases
    esrc = open(ERRORS_SRC, encoding="utf-8").read()
    for node in class_nodes(esrc, ["PersistenceError", "NotFound", "Conflict"]):
        exec_node(esrc, node, ns, ERRORS_SRC)
    # real hosted-room exceptions + record dataclasses
    psrc = open(PERSIST_SRC, encoding="utf-8").read()
    for node in class_nodes(psrc, [
            "_OneMessage", "HostedRoomError", "RoomNotFoundError",
            "RoomHistoryExpiredError", "AuthorityConflictError",
            "RoomGrantClaims", "RemoteRunIdentity", "RemoteRunReceipt",
            "RoomLinkRecord", "HostedOwnerClaim"]):
        exec_node(psrc, node, ns, PERSIST_SRC)
    # real common leaves (exec into the SAME ns so clock reads ns["time"])
    csrc = open(COMMON_SRC, encoding="utf-8").read()
    seed_constants(csrc, ns, COMMON_SRC)
    for node in func_nodes(csrc, ["identifier", "bounded_int", "canonical_json",
                                  "compact_json", "utf8_len", "clock", "table_columns"]):
        exec_node(csrc, node, ns, COMMON_SRC)
    ns["_now"] = ns["clock"]
    # hosted_rooms: constants, defs, constants again (partial aliases bind late)
    hsrc = open(HR_SRC, encoding="utf-8").read()
    seed_constants(hsrc, ns, HR_SRC)
    for node in func_nodes(hsrc, WANTED):
        exec_node(hsrc, node, ns, HR_SRC)
    seed_constants(hsrc, ns, HR_SRC)
    required = sorted(WANTED | {"HostedRoomError", "RoomNotFoundError",
                                "RoomHistoryExpiredError", "AuthorityConflictError",
                                "RoomGrantClaims", "RemoteRunIdentity", "_now",
                                "_room_id", "_actor_json", "_LEGACY_COLUMNS",
                                "_EVENT_BYTES_BACKFILL", "_INSERT_EVENT",
                                "_SUM_EVENT_BYTES", "_LEGACY_ACTOR_JSON"})
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_hosted_rooms_flow: missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()

# ── per-case script backend (mirrors the C hrf_call exactly) ──
CUR = {"ctx": None}
_MISSING = object()


def canon(v):
    def conv(x):
        if is_dataclass(x) and not isinstance(x, type):
            return conv(asdict(x))
        if isinstance(x, Path):
            return x.as_posix()
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (frozenset, set)):
            return sorted((conv(i) for i in x), key=lambda z: json.dumps(z, sort_keys=True))
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    if kind == "KeyError":
        return KeyError(msg)  # str(KeyError(k)) == repr(k)
    cls = {"TypeError": TypeError, "ValueError": ValueError, "RuntimeError": RuntimeError,
           "AttributeError": AttributeError, "IndexError": IndexError,
           "OverflowError": OverflowError, "SystemExit": SystemExit,
           "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
           "HostedRoomError": NS["HostedRoomError"],
           "RoomNotFoundError": NS["RoomNotFoundError"],
           "RoomHistoryExpiredError": NS["RoomHistoryExpiredError"],
           "AuthorityConflictError": NS["AuthorityConflictError"]}.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


def drop_none(d):
    return {k: v for k, v in d.items() if v is not None}


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": args, "ret": ret})
        return ret


def rec(ctx, name):
    def f(**k):
        return ctx.call(name, drop_none(dict(k)))
    return f


class CursorDouble:
    def __init__(self, rows):
        self.rows = rows

    def fetchone(self):
        return next(iter(self.rows), None)

    def fetchall(self):
        return list(self.rows)

    def __iter__(self):
        return iter(self.rows)


class ConnDouble:
    def __init__(self, ctx):
        self.ctx = ctx

    def execute(self, sql, params=None):
        args = {"sql": sql}
        if params is not None:
            args["params"] = list(params)
        return CursorDouble(self.ctx.call("conn.execute", args))


def make_store(ctx):
    return types.SimpleNamespace(
        hosted_rooms=types.SimpleNamespace(
            create_room=rec(ctx, "hosted_rooms.create_room"),
            list_rooms=rec(ctx, "hosted_rooms.list_rooms"),
        ),
        hosted_room_peers=types.SimpleNamespace(
            room_has_peer_obligations=rec(ctx, "hosted_room_peers.room_has_peer_obligations"),
            retire_room_link_records=rec(ctx, "hosted_room_peers.retire_room_link_records"),
            due_room_grant_revokes=rec(ctx, "hosted_room_peers.due_room_grant_revokes"),
            settle_room_grant_revoke=rec(ctx, "hosted_room_peers.settle_room_grant_revoke"),
            drop_remote_run_receipt=rec(ctx, "hosted_room_peers.drop_remote_run_receipt"),
        ),
    )


def install_case(inp):
    ctx = Ctx(inp.get("io"))
    CUR["ctx"] = ctx
    NS["time"] = types.SimpleNamespace(time=lambda: ctx.call("clock", {}))

    def prune(conn, *, now, max_gateway_event_bytes=None):
        return ctx.call("_prune_disbanded_rooms_locked",
                        drop_none({"now": now, "max_gateway_event_bytes": max_gateway_event_bytes}))
    NS["_prune_disbanded_rooms_locked"] = prune
    return ctx


_hc = types.ModuleType("hermes_constants")
_hc.get_hermes_home = lambda: CUR["ctx"].call("get_hermes_home", {})
sys.modules["hermes_constants"] = _hc


def drive(func, inp):
    ctx = CUR["ctx"]
    store = make_store(ctx)
    conn = ConnDouble(ctx)
    if func == "create_room":
        return NS["create_room"](store, placement=inp["placement"], room_id=inp["room_id"],
                                 name=inp["name"], members=inp["members"],
                                 authority_gateway_id=inp["authority_gateway_id"],
                                 now=inp.get("now"), owner=inp.get("owner"))
    if func == "list_rooms":
        return NS["list_rooms"](store, placement=inp["placement"],
                                include_disbanded=inp.get("include_disbanded", False),
                                limit=inp.get("limit", 500), offset=inp.get("offset", 0))
    if func == "room_has_peer_obligations":
        return NS["room_has_peer_obligations"](store, placement=inp["placement"],
                                               room_id=inp["room_id"], now=inp.get("now"))
    if func == "retire_room_link_records":
        return NS["retire_room_link_records"](store, placement=inp["placement"],
                                              room_id=inp["room_id"],
                                              owe_routes=inp.get("owe_routes", False),
                                              now=inp.get("now"),
                                              revoked_digests=inp.get("revoked_digests"))
    if func == "due_room_grant_revokes":
        return NS["due_room_grant_revokes"](store, placement=inp["placement"],
                                            limit=inp["limit"], now=inp.get("now"))
    if func == "settle_room_grant_revoke":
        return NS["settle_room_grant_revoke"](store, placement=inp["placement"],
                                              room_id=inp["room_id"], member_id=inp["member_id"],
                                              grant_digest=inp["grant_digest"],
                                              retry_at=inp.get("retry_at"))
    if func == "drop_remote_run_receipt":
        return NS["drop_remote_run_receipt"](store, placement=inp["placement"],
                                             record=inp["record"], run_id=inp["run_id"])
    if func == "_typed":
        return NS["_typed"](NS[inp["type"]], inp["record"])
    if func == "_grant_claims":
        return NS["_grant_claims"](inp["claims"])
    if func == "_reservation_claims":
        return NS["_reservation_claims"](inp["claims"])
    if func == "_reservation_grant":
        return NS["_reservation_grant"](inp["claims"])
    if func == "default_db_path":
        return NS["default_db_path"](inp.get("home"))
    if func == "_room_row":
        return NS["_room_row"](conn, inp["sql"], tuple(inp["params"]), inp["room_id"])
    if func == "_is_retired":
        return NS["_is_retired"](conn, inp["room_id"])
    if func == "_require_authority":
        return NS["_require_authority"](inp["room"], inp["gateway_id"], inp["epoch"], inp["message"])
    if func == "_migrate_legacy_columns":
        return NS["_migrate_legacy_columns"](conn)
    if func == "_prepare_event":
        return NS["_prepare_event"](conn, inp["room"], inp["event_id"], inp["kind"],
                                    inp["actor_json"], inp["payload_json"],
                                    allow_control=inp.get("allow_control", False))
    if func == "_insert_event":
        return NS["_insert_event"](conn, inp["room"], inp["room_id"], inp["seq"],
                                   inp["event_id"], inp["kind"], inp["actor_json"],
                                   inp["epoch"], inp["payload_json"], inp["now"],
                                   allow_control=inp.get("allow_control", False))
    raise ValueError(f"unknown func: {func}")


def run_case(func, inp):
    ctx = install_case(inp)
    args = inp.get("args") or {}
    try:
        ret = drive(func, args)
    except BaseException as exc:  # SystemExit included; no ported flow catches
        return {"ret": proj_exc(exc), "trace": ctx.trace}
    return {"ret": canon(ret), "trace": ctx.trace}


# ── three-way seam-name cross-check (C call sites / fixture io / oracle stubs) ──
def cross_check(fixture_path):
    csrc = open(os.path.join(_REPO, "src", "gateway", "hosted_rooms_flow.c"), encoding="utf-8").read()
    c_names = set(re.findall(r'hrf_call\(c,\s*"([^"]+)"', csrc))
    if c_names != SEAM_NAMES:
        print(f"FATAL oracle_hosted_rooms_flow: C seams {sorted(c_names)} != oracle {sorted(SEAM_NAMES)}")
        sys.exit(2)
    hsrc = open(os.path.join(_REPO, "tests", "t_port_hosted_rooms_flow.c"), encoding="utf-8").read()
    h_funcs = set(re.findall(r'\{\s*"([^"]+)",\s*hrf_', hsrc))
    if h_funcs != set(FUNCS):
        print(f"FATAL oracle_hosted_rooms_flow: harness funcs {sorted(h_funcs)} != {sorted(FUNCS)}")
        sys.exit(2)
    io_names = set()
    if os.path.exists(fixture_path):
        with open(fixture_path, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    rec = json.loads(line)
                except Exception:
                    continue
                io_names |= set((rec.get("in") or {}).get("io") or {})
        extra = io_names - SEAM_NAMES
        if extra:
            print(f"FATAL oracle_hosted_rooms_flow: fixture io keys without a seam: {sorted(extra)}")
            sys.exit(2)


def main():
    fixture = os.path.join(_REPO, "tests", "fixtures", "hosted_rooms_flow.jsonl")
    cross_check(fixture)
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec.get("func", "")
        inp = rec.get("in") or {}
        got = rec.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print(f"MISMATCH {func}: harness emitted no result: {got}")
            continue
        want = run_case(func, inp)
        g = canon(got)
        if g != canon(want):
            m += 1
            if shown < 8:
                print(f"MISMATCH {func}")
                print(f"  C : {json.dumps(g, ensure_ascii=False)[:600]}")
                print(f"  PY: {json.dumps(want, ensure_ascii=False)[:600]}")
            shown += 1
    print(f"ORACLE hosted_rooms_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


if __name__ == "__main__":
    main()