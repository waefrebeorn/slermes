#!/usr/bin/env python3
"""Oracle for the persistence/sqlite/conversation/sessions.py FLOW + leaf ports
(src/persistence/sqlite_sessions_flow.c).

Drives the LIVE SessionRepository methods and module leaves with SCRIPTED
seams: the target module's leaves + the whole SQLiteSessionRepository class are
AST-extracted from the quarry, the contract records/enums/errors are extracted
from their defining modules (persistence/conversation/sessions.py,
persistence/core/errors.py, persistence/core/identity.py,
persistence/core/conventions.py), and every live collaborator outside the
ported set (the _store.* verbs, refusals, stored_instant, _parse_model_config,
is_corrupt_model_config, session_unread, _own_profile_name, the read-ctx
connection) is replaced by recording stubs returning the SAME scripts the C
harness feeds its universal seam. Compares the ordered seam-call trace + the
return (or raise spec).

Input: stdin JSON lines from t_port_sqlite_sessions_flow:
    {"func": ..., "in": {...}, "out": {"ret": ..., "trace": [...]}}
Summary: ORACLE sqlite_sessions_flow: N cases, M mismatches. Exit 1 on any
mismatch, and fail-closed (exit 1) when zero cases were compared.

Modeling notes (identical on the C side):
- refusals(holder, sid) is a SEAMED sibling (_errors.py): a passthrough CM
  recording structural events refusals.enter {session_id} and refusals.exit
  {session_id, exc} (exc = the str-form raise spec leaving the with-body,
  AFTER the ported _title_refusals conversion; null on success). No script.
- holder() / holder._read_ctx() are silent state doubles (no trace events).
  conn.execute(sql, params) is a scripted seam whose ret is the fetchone()
  row (list) or null; the cursor double is internal.
- Store verbs project to {"args": [positionals after holder], "kwargs": {...}}
  with None-valued kwargs DROPPED on both sides. _parse_model_config /
  is_corrupt_model_config project to {"raw": cell}; stored_instant to
  {"value", "session_id", "field} with ret = the decoded epoch float or null
  (the .timestamp() projection; null reads UNTRUSTED_INSTANT for started_at,
  whose epoch floor is 0.0); session_unread to {"row": copy}.
- SessionId is NewType(str): identity. Records project to field-order dicts
  with enums as .value; tuples as lists.
- Script semantics: io[name] absent -> RuntimeError "no script for <name>";
  array -> per-call queue (exhaustion -> RuntimeError); {"__script__":true,
  "ret":X} -> constant container; other value -> constant. A raise spec
  {"__raise__":cls,"message":msg} is RECORDED then raised as a real exception
  (BaseException trio included). KeyError messages carry the RAW key in seam
  ret specs; str(KeyError(k))==repr(k) is applied at exc/ret projection.
- Fixture domain (ASCII): JSON text cells are canonical strict JSON (no
  leading zeros, NaN/Infinity, lone surrogates, raw controls, trailing commas;
  depth <= 8); int()/float() string arguments use ASCII whitespace, finite
  values; origin/model_config values are str/bool/null/int; counter int(x or 0)
  cells are null/bool/int/int-string; store-verb scripts return the documented
  shapes (dict configs, list rows, bool flags).
"""
import ast
import copy
import json
import math
import os
import sys
import types
from dataclasses import dataclass, field, fields as dc_fields
from enum import Enum
from typing import Any, Optional, Iterator, Mapping, Sequence, NewType
from contextlib import contextmanager
from datetime import datetime, timezone, UTC

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "persistence", "sqlite", "conversation", "sessions.py")
CONTRACT = os.path.join(QUARRY, "persistence", "conversation", "sessions.py")
ERRORS = os.path.join(QUARRY, "persistence", "core", "errors.py")
IDENTITY = os.path.join(QUARRY, "persistence", "core", "identity.py")
CONVENTIONS = os.path.join(QUARRY, "persistence", "core", "conventions.py")

import __future__
ANN_FLAG = __future__.annotations.compiler_flag

# The ported set (PoP targets). Everything else is seamed or silent.
FUNCS = [
    "_stripped", "_json_value", "_tool_pin", "_ids", "_removed", "_listed",
    "_optional_list", "_sql_limit", "_last_active", "_roster_session",
    "session_record", "_list_entry", "_recency_entry", "_title_refusals",
    "create", "set_title", "set_auto_title", "delete", "delete_if_empty",
    "backfill_session_owners", "get_title_source", "title_holder",
    "list_recent_sessions", "list_unlinked_telegram_sessions",
]
LEAF_FUNCS = {
    "_stripped", "_json_value", "_tool_pin", "_ids", "_removed", "_listed",
    "_optional_list", "_sql_limit", "_last_active", "_roster_session",
    "session_record", "_list_entry", "_recency_entry", "_title_refusals",
}
METHOD_FUNCS = set(FUNCS) - LEAF_FUNCS

# Seamed scripted names (fixture io keys == C seam names == recorder names).
IO_NAMES = [
    "create_session", "set_session_title", "set_auto_title", "delete_session",
    "delete_session_if_empty", "backfill_null_session_profiles",
    "_own_profile_name", "get_session_title_source",
    "list_recent_sessions_bounded", "list_unlinked_telegram_sessions_for_user",
    "session_unread", "_parse_model_config", "stored_instant",
    "is_corrupt_model_config", "conn.execute",
]
STRUCTURAL = ["refusals.enter", "refusals.exit"]

CUR = {"in": {}, "trace": [], "cursors": {}, "func": ""}
_MISSING = object()
NS = {}


def rec(ev):
    CUR["trace"].append(ev)


def build_exc(spec):
    """A scripted raise needs a REAL exception instance; the BaseException
    trio must be the real builtins so except-Exception sites propagate them."""
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("message", "")
    builtins = {
        "ValueError": ValueError, "TypeError": TypeError, "KeyError": KeyError,
        "AttributeError": AttributeError, "RuntimeError": RuntimeError,
        "Exception": Exception, "TimeoutError": TimeoutError,
        "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
        "GeneratorExit": GeneratorExit,
    }
    if kind in builtins:
        return builtins[kind](msg)
    if kind == "AuthorityRefusal":
        return NS["AuthorityRefusal"](msg)
    if kind == "RetiredSessionError":
        return NS["RetiredSessionError"]("scripted", msg)
    if kind == "TitleConflict":
        return NS["TitleConflict"](msg, "scripted")
    if kind == "CanonicalTitleLocked":
        return NS["CanonicalTitleLocked"]("scripted")
    if kind == "RecentBrowseTimeout":
        return NS["RecentBrowseTimeout"](msg)
    return type(kind, (Exception,), {})(msg)


def rec_seam(name, args):
    """Universal scripted seam: queue/constant/__script__ semantics; records
    {ev, args, ret} then raises a scripted spec or returns a deep copy."""
    io = CUR["in"].get("io") or {}
    script = io.get(name, _MISSING)
    if script is _MISSING:
        result = {"__raise__": "RuntimeError", "message": "no script for " + name}
    elif isinstance(script, list):
        cur = CUR["cursors"].get(name, 0)
        if cur >= len(script):
            result = {"__raise__": "RuntimeError",
                      "message": "script exhausted for " + name}
        else:
            result = script[cur]
            CUR["cursors"][name] = cur + 1
    elif isinstance(script, dict) and script.get("__script__"):
        result = script.get("ret")
    else:
        result = script
    rec({"ev": name, "args": copy.deepcopy(args), "ret": copy.deepcopy(result)})
    if isinstance(result, dict) and "__raise__" in result:
        raise build_exc(result)
    return copy.deepcopy(result)


def exc_spec(e):
    return {"__raise__": type(e).__name__, "message": str(e)}


class _RefusalsCM:
    """The seamed refusals() sibling: passthrough, structural enter/exit."""
    def __init__(self, sid):
        self.sid = sid

    def __enter__(self):
        rec({"ev": "refusals.enter", "args": {"session_id": self.sid}, "ret": None})
        return self

    def __exit__(self, et, e, tb):
        rec({"ev": "refusals.exit",
             "args": {"session_id": self.sid, "exc": None if e is None else exc_spec(e)},
             "ret": None})
        return False


def _refusals(holder=None, session_id=None):
    return _RefusalsCM(session_id)


class _InstantDouble:
    """stored_instant seam ret projection: the decoded instant's .timestamp()."""
    def __init__(self, epoch):
        self._epoch = epoch

    def timestamp(self):
        return self._epoch


def _stored_instant(value, *, session_id=None, field="timestamp"):
    r = rec_seam("stored_instant",
                 {"value": value, "session_id": session_id, "field": field})
    return None if r is None else _InstantDouble(r)


def _store_verb(name):
    def f(*a, **k):
        args = {"args": list(a[1:]),
                "kwargs": {kk: vv for kk, vv in k.items() if vv is not None}}
        return rec_seam(name, args)
    return f


def _parse_model_config(raw):
    return rec_seam("_parse_model_config", {"raw": raw})


def is_corrupt_model_config(raw):
    return rec_seam("is_corrupt_model_config", {"raw": raw})


def session_unread(row):
    return rec_seam("session_unread", {"row": row})


class _Cursor:
    def __init__(self, row):
        self._row = row

    def fetchone(self):
        return self._row


class _Conn:
    def execute(self, sql, params=()):
        row = rec_seam("conn.execute", {"sql": sql, "params": list(params)})
        return _Cursor(row)


class _ReadCtx:
    def __enter__(self):
        return _Conn()

    def __exit__(self, *a):
        return False


def _reg_mod(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def _src(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def _exec_node(node, src, path, ns):
    exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec",
                 flags=ANN_FLAG, dont_inherit=True), ns)


def _extract_class(path, classname, ns):
    src = _src(path)
    for node in ast.parse(src).body:
        if isinstance(node, ast.ClassDef) and node.name == classname:
            if node.decorator_list:
                lines = src.splitlines()
                seg = "\n".join(lines[node.decorator_list[0].lineno - 1:node.end_lineno])
                exec(compile(seg, path, "exec", flags=ANN_FLAG, dont_inherit=True), ns)
            else:
                _exec_node(node, src, path, ns)
            return
    raise RuntimeError("class %s not found in %s" % (classname, path))


def _extract_assign(path, name, ns):
    src = _src(path)
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == name for t in node.targets):
            _exec_node(node, src, path, ns)
            return
    raise RuntimeError("assign %s not found in %s" % (name, path))


def _extract_func(path, name, ns):
    src = _src(path)
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name == name:
            _exec_node(node, src, path, ns)
            return
    raise RuntimeError("func %s not found in %s" % (name, path))


def extract():
    global NS
    modname = "sqlite_sessions_flow_extracted"
    mod = _reg_mod(modname)
    ns = mod.__dict__
    ns["__name__"] = modname
    ns["__builtins__"] = __builtins__
    import re as _re
    ns["re"] = _re
    ns["json"] = json
    ns["types"] = types
    ns["contextmanager"] = contextmanager
    ns["dataclass"] = dataclass
    ns["field"] = field
    ns["Enum"] = Enum
    ns["Any"] = Any
    ns["Optional"] = Optional
    ns["Iterator"] = Iterator
    ns["Mapping"] = Mapping
    ns["Sequence"] = Sequence
    ns["NewType"] = NewType
    ns["datetime"] = datetime
    ns["timezone"] = timezone
    ns["UTC"] = UTC

    # Real error family + identity + instant floor from their defining modules.
    for nm in ("PersistenceError", "Conflict", "TransientStoreError",
               "AuthorityRefusal", "RetiredSessionError"):
        _extract_class(ERRORS, nm, ns)
    _extract_func(ERRORS, "_one_line", ns)
    _extract_assign(IDENTITY, "SessionId", ns)
    _extract_assign(CONVENTIONS, "_STORED_EPOCH_MIN", ns)
    _extract_assign(CONVENTIONS, "UNTRUSTED_INSTANT", ns)

    # Contract records / enums / errors / constants.
    for nm in ("TitleConflict", "CanonicalTitleLocked", "RecentBrowseTimeout",
               "SessionLineageKind", "TitleSource", "NewSession", "SessionRecord",
               "SessionWindow", "SessionCountQuery", "SessionListEntry",
               "SessionRecencyEntry", "RecentSessionEntry", "RecentBrowseQuery",
               "RosterSession", "SessionFacetEntry"):
        _extract_class(CONTRACT, nm, ns)
    for nm in ("END_REASON_SESSION_RESET", "RECENT_BROWSE_CANDIDATE_CAP",
               "RECENT_BROWSE_LINEAGE_CAP"):
        _extract_assign(CONTRACT, nm, ns)

    # Target module: the constants the ported set reads (NOT _LOCAL_CHAT /
    # _DESCENDANTS_SQL — their SQL helpers are outside the ported set).
    src = _src(SRC)
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id in (
                    "_MARKERS", "_MARKER_OF_KIND", "_TITLE_SOURCES", "_TITLE_HELD",
                    "_CANONICAL_TITLE_LOCKED", "_PROMPT_HASH") for t in node.targets):
            _exec_node(node, src, SRC, ns)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in LEAF_FUNCS:
            _exec_node(node, src, SRC, ns)
        elif isinstance(node, ast.ClassDef) and node.name == "SQLiteSessionRepository":
            _exec_node(node, src, SRC, ns)

    # Seamed collaborators (module globals the extracted bodies resolve).
    ns["refusals"] = _refusals
    ns["stored_instant"] = _stored_instant
    ns["_parse_model_config"] = _parse_model_config
    ns["is_corrupt_model_config"] = is_corrupt_model_config
    ns["session_unread"] = session_unread
    for nm in IO_NAMES:
        if nm in ("session_unread", "_parse_model_config", "stored_instant",
                  "is_corrupt_model_config", "conn.execute"):
            continue
        ns[nm] = _store_verb(nm)

    for nm in FUNCS + ["SQLiteSessionRepository", "_MARKERS", "_MARKER_OF_KIND",
                       "_TITLE_SOURCES", "_TITLE_HELD", "_CANONICAL_TITLE_LOCKED",
                       "_PROMPT_HASH", "UNTRUSTED_INSTANT", "SessionId"]:
        if nm not in ns and nm not in ("create", "set_title", "set_auto_title",
                                       "delete", "delete_if_empty",
                                       "backfill_session_owners", "get_title_source",
                                       "title_holder", "list_recent_sessions",
                                       "list_unlinked_telegram_sessions"):
            raise RuntimeError("seed missing: " + nm)
    for nm in METHOD_FUNCS:
        if not hasattr(ns["SQLiteSessionRepository"], nm):
            raise RuntimeError("method missing: " + nm)
    NS = ns
    return ns


def setup_case(inp):
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["cursors"] = {}
    holder = types.SimpleNamespace(_read_ctx=lambda: _ReadCtx(), db_path="state.db")
    store = types.SimpleNamespace(holder=lambda reacquire=True: holder,
                                  store_path="state.db")
    return NS["SQLiteSessionRepository"](store)


def rec_proj(r):
    d = {}
    for f in dc_fields(r):
        v = getattr(r, f.name)
        if isinstance(v, Enum):
            v = v.value
        d[f.name] = v
    return d


def entry_proj(e):
    return {"session": rec_proj(e.session), "preview": e.preview, "unread": e.unread,
            "last_active": e.last_active, "lineage_root_id": e.lineage_root_id,
            "lineage_ids": list(e.lineage_ids), "continuation_kind": e.continuation_kind,
            "compact": e.compact}


def recent_proj(e):
    return {f.name: getattr(e, f.name) for f in dc_fields(e)}


def facet_proj(e):
    return {"session": rec_proj(e.session), "last_active": e.last_active,
            "preview": e.preview}


def _materialize_spec(spec):
    kw = dict(spec)
    if "lineage_kind" in kw:
        kw["lineage_kind"] = NS["SessionLineageKind"](kw["lineage_kind"])
    return NS["NewSession"](**kw)


def _materialize_query(q):
    kw = dict(q)
    if "exclude_sources" in kw and kw["exclude_sources"] is not None:
        kw["exclude_sources"] = tuple(kw["exclude_sources"])
    return NS["RecentBrowseQuery"](**kw)


def drive(inp):
    func = CUR["func"]
    repo = setup_case(inp)
    try:
        if func == "_stripped":
            ret = NS["_stripped"](inp.get("value"))
        elif func == "_json_value":
            ret = NS["_json_value"](inp.get("text"))
        elif func == "_tool_pin":
            ret = NS["_tool_pin"](inp.get("text"))
        elif func == "_ids":
            ret = list(NS["_ids"](inp.get("values")))
        elif func == "_removed":
            ret = list(NS["_removed"](inp.get("values")))
        elif func == "_listed":
            ret = NS["_listed"](inp.get("values"))
        elif func == "_optional_list":
            ret = NS["_optional_list"](inp.get("values"))
        elif func == "_sql_limit":
            w = inp.get("window") or {}
            window = NS["SessionWindow"](limit=w.get("limit"), offset=w.get("offset", 0))
            ret = NS["_sql_limit"](window)
        elif func == "_last_active":
            ret = NS["_last_active"](inp.get("row") or {})
        elif func == "_roster_session":
            r = NS["_roster_session"](inp.get("fields"))
            ret = None if r is None else {f.name: getattr(r, f.name) for f in dc_fields(r)}
        elif func == "session_record":
            ret = rec_proj(NS["session_record"](inp.get("row") or {}))
        elif func == "_list_entry":
            e = NS["_list_entry"](repo._store.holder(), inp.get("row") or {},
                                  compact=inp.get("compact", False))
            ret = entry_proj(e)
        elif func == "_recency_entry":
            e = NS["_recency_entry"](inp.get("row") or {})
            ret = {"session": rec_proj(e.session), "last_active": e.last_active}
        elif func == "_title_refusals":
            body = inp.get("body") or {}
            with NS["_title_refusals"](inp.get("session_id")):
                if "raise" in body:
                    raise build_exc(body["raise"])
                ret = body.get("ret")
        elif func == "create":
            ret = repo.create(_materialize_spec(inp.get("spec") or {}))
        elif func == "set_title":
            ret = repo.set_title(inp.get("session_id"), inp.get("title"))
        elif func == "set_auto_title":
            ret = repo.set_auto_title(inp.get("session_id"), inp.get("title"),
                                      source=NS["TitleSource"](inp.get("source")))
        elif func == "delete":
            ret = list(repo.delete(inp.get("session_id"),
                                   expected_ids=inp.get("expected_ids"),
                                   expected_display=inp.get("expected_display"),
                                   refuse_guarded=inp.get("refuse_guarded", False),
                                   include_compression_chain=inp.get("include_compression_chain", False),
                                   media_root=inp.get("media_root")))
        elif func == "delete_if_empty":
            ret = list(repo.delete_if_empty(inp.get("session_id"),
                                            media_root=inp.get("media_root")))
        elif func == "backfill_session_owners":
            ret = repo.backfill_session_owners(stamp=inp.get("stamp"))
        elif func == "get_title_source":
            r = repo.get_title_source(inp.get("session_id"))
            ret = None if r is None else r.value
        elif func == "title_holder":
            ret = repo.title_holder(inp.get("title"), excluding=inp.get("excluding"))
        elif func == "list_recent_sessions":
            rows = repo.list_recent_sessions(_materialize_query(inp.get("query") or {}))
            ret = [recent_proj(e) for e in rows]
        elif func == "list_unlinked_telegram_sessions":
            rows = repo.list_unlinked_telegram_sessions(
                user_id=inp.get("user_id"), profile_name=inp.get("profile_name"),
                limit=inp.get("limit", 10))
            ret = [facet_proj(e) for e in rows]
        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:
        ret = exc_spec(e)
    return {"ret": ret, "trace": CUR["trace"]}


def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        f = float(x)
        if math.isfinite(f) and f == int(f) and abs(f) < 1e15:
            return ("num", float(int(f)))
        return ("num", f)
    if x is None:
        return ("null",)
    return ("str", x)


def main():
    extract()
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        CUR["func"] = func
        try:
            want = drive(inp)
        except BaseException as e:
            mism += 1
            print("PYERR %s: %r" % (func, e))
            continue
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE sqlite_sessions_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())