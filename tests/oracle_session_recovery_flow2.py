#!/usr/bin/env python3
"""Oracle for session_recovery_flow2.c — "hermes_cli/session_recovery.py remainder"
(live authority: C:/Users/eman5/hermes-agent/persistence/sqlite/admin/_recovery.py).

Drives the LIVE defs with SCRIPTED I/O. The 12 ported functions are AST-extracted
from the quarry. CLAIMED siblings (_table_columns, _source_fingerprint) and
every foreign collaborator (sqlite3 connection methods) are replaced by recording
stubs returning the SAME scripted results the C harness feeds its universal seam.
Internal composition (_state_meta_precheck -> _state_meta_result,
sanitize -> _reconcile, verify_structure -> _journal_mode) runs REAL on both sides.
Compares the ordered seam trace + return value + post-hoc mutated carrier with
ordered-dict comparison (insertion order is semantic). Per-case BaseException
catch; FULL builtin + sqlite3 exception hierarchy so scripted subclasses fold into
the exact `except sqlite3.DatabaseError` / `except BaseException` sites.

Input: stdin JSON lines from t_port_session_recovery_flow2:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "state":...}}
Summary: ORACLE session_recovery_flow2: N cases, M mismatches. Fail-closed (exit 2)
on extraction misses or zero cases.

Script semantics (identical on both sides, FAIL-CLOSED): a MISSING script returns a
RuntimeError raise spec; a seams[name] ARRAY is a per-call queue (last repeats,
empty -> None); a single value is a constant; a scripted null is None (NULL in C);
{"__raise__": cls, "msg": ...} scripts an exception. For fetchall seams
(integrity_check / foreign_key_check / fts_meta) a scripted null is an empty row
list; for fetchone seams (count_rows / journal_mode / reconcile_count /
schema_version) a scripted null is a None row.
"""
import ast
import builtins
import copy
import json
import os
import re
import sys
import types
from contextlib import contextmanager
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "sr2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "persistence", "sqlite", "admin", "_recovery.py")
SRC_TEXT = open(SRC, encoding="utf-8").read()
ANN_FLAG = __import__("__future__").annotations.compiler_flag

# func key -> live name (the 12 UNCLAIMED defs).
FUNCS = {
    "count_rows": "_count_rows",
    "quoted_columns": "_quoted_columns",
    "journal_mode": "_journal_mode",
    "compatible_columns": "_compatible_columns",
    "state_meta_result": "_state_meta_result",
    "state_meta_precheck": "_state_meta_precheck",
    "reconcile": "_reconcile",
    "immediate_transaction": "_immediate_transaction",
    "verify_fts_indexes": "_verify_fts_indexes",
    "verify_structure": "_verify_structure",
    "sanitize_session_model_config": "_sanitize_session_model_config",
    "recovery_report": "_recovery_report",
}
TOP_DEFS = list(FUNCS.values())
MUT_CARRIER = {
    "compatible_columns": "result",
    "verify_fts_indexes": "verification",
    "verify_structure": "verification",
    "recovery_report": "verification",
}

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "raised": []}


# ── sqlite3 stub family (normal str(); distinct classes for except identity) ──
class _SqlError(Exception):
    pass


class _SqlDatabaseError(_SqlError):
    pass


SQLITE_FAMILY = {}
for _n in ("Error", "DatabaseError", "DataError", "IntegrityError", "InternalError",
           "NotSupportedError", "OperationalError", "ProgrammingError"):
    _base = _SqlDatabaseError if _n == "DatabaseError" else (
        _SqlError if _n == "Error" else SQLITE_FAMILY.get("DatabaseError", _SqlDatabaseError))
    SQLITE_FAMILY[_n] = type(_n, (_base,), {})
# repair the subclass chain precisely (DatabaseError -> Error)
SQLITE_FAMILY["DatabaseError"] = type("DatabaseError", (SQLITE_FAMILY["Error"],), {})
for _n in ("DataError", "IntegrityError", "InternalError", "NotSupportedError",
           "OperationalError", "ProgrammingError"):
    SQLITE_FAMILY[_n] = type(_n, (SQLITE_FAMILY["DatabaseError"],), {})
SQLITE3 = types.SimpleNamespace(**SQLITE_FAMILY)


# ── exception construction ──
def build_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    if cls in SQLITE_FAMILY:
        return SQLITE_FAMILY[cls](msg)
    b = getattr(builtins, cls, None)
    if isinstance(b, type) and issubclass(b, BaseException):
        if cls in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
            e = b.__new__(b)
            Exception.__init__(e, msg)
            return e
        if cls in ("BaseExceptionGroup", "ExceptionGroup"):
            sub = SystemExit("sub") if cls == "BaseExceptionGroup" else Exception("sub")
            return b(msg, [sub])
        return b(msg)
    return type(cls, (Exception,), {})(msg)


def project_exc(e):
    for exc, spec in CUR["raised"]:
        if e is exc:
            return spec  # scripted raise round-trips bare (key order included)
    if isinstance(e, _SqlError):
        return {"__raise__": type(e).__name__, "msg": str(e)}
    return {"__raise__": type(e).__name__, "msg": str(e)}


def seam(name, args):
    args = json.loads(json.dumps({k: v for k, v in args.items() if v is not None}))
    scripts = CUR["scripts"]
    if name not in scripts:
        r = {"__raise__": "RuntimeError", "msg": "no script for seam: %s" % name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    r = json.loads(json.dumps(r))
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        e = build_exc(r)
        CUR["raised"].append((e, r))
        raise e
    return r


# ── normalization / canonical comparison ──
def norm(o):
    if o is None or isinstance(o, (bool, int, float, str)):
        return o
    if isinstance(o, (frozenset, set)):
        return sorted(norm(v) for v in o)
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    return str(o)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    o = _numnorm(o)
    if isinstance(o, dict):
        return ("obj", [(k, canon(v)) for k, v in o.items()])
    if isinstance(o, (list, tuple)):
        return ("arr", [canon(v) for v in o])
    if o is None:
        return ("null",)
    if isinstance(o, bool):
        return ("bool", o)
    if isinstance(o, (int, float)):
        return ("num", float(o))
    return ("str", o)


# ── SQL -> seam projection (identical to the C port's seam names/args) ──
FETCHALL = {"integrity_check", "foreign_key_check", "fts_meta"}


def map_sql(sql, params):
    if sql.startswith('SELECT COUNT(*) FROM "'):
        rest = sql[len('SELECT COUNT(*) FROM '):]
        end = rest.index('"', 1)
        table = rest[1:end]
        tail = rest[end + 1:].strip()
        if tail.startswith("WHERE "):
            return "reconcile_count", {"table": table, "where": tail[len("WHERE "):]}
        return "count_rows", {"table": table}
    if sql == "PRAGMA integrity_check":
        return "integrity_check", {}
    if sql == "PRAGMA foreign_key_check":
        return "foreign_key_check", {}
    if sql == "PRAGMA journal_mode":
        return "journal_mode", {}
    if sql == "SELECT version FROM schema_version LIMIT 1":
        return "schema_version", {}
    if sql == "SELECT key, value FROM state_meta WHERE key LIKE 'fts_%'":
        return "fts_meta", {}
    if sql == "BEGIN IMMEDIATE":
        return "begin", {}
    if sql == "COMMIT":
        return "commit", {}
    if sql == "ROLLBACK":
        return "rollback", {}
    for t in ("messages_fts", "messages_fts_trigram"):
        if sql.startswith('INSERT INTO "%s"' % t):
            return "fts_insert", {"table": t}
        if sql.startswith('SELECT 1 FROM "%s"' % t):
            return "fts_match", {"table": t}
    if " WHERE " in sql:
        mutation, where = sql.split(" WHERE ", 1)
        return "reconcile_mutate", {"mutation": mutation, "where": where}
    return "execute", {"sql": sql, "params": list(params)}


class Cursor:
    def __init__(self, rows):
        self.rows = rows

    def fetchone(self):
        return self.rows[0] if self.rows else None

    def fetchall(self):
        return self.rows


class Conn:
    def __init__(self, label=""):
        self.label = label

    def execute(self, sql, params=()):
        name, args = map_sql(sql, tuple(params))
        r = seam(name, args)
        if name in FETCHALL:
            rows = r if isinstance(r, list) else []
        else:
            rows = [r] if r is not None else []
        return Cursor(rows)


# ── extraction ──
def _exec_node(node, ns, filename):
    exec(compile(ast.Module(body=[node], type_ignores=[]), filename, "exec",
                 flags=ANN_FLAG), ns)


def extract_consts_into(ns, path, names, skip=()):
    tree = ast.parse(open(path, encoding="utf-8").read())
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
            if not any(t.id in names for t in node.targets):
                continue
            if any(t.id in skip for t in node.targets):
                continue
        elif isinstance(node, ast.AnnAssign):
            if not (node.value and isinstance(node.target, ast.Name)
                    and node.target.id in names):
                continue
        else:
            continue
        _exec_node(node, ns, path)


def extract():
    mod = types.ModuleType("session_recovery_flow2_extracted")
    sys.modules["session_recovery_flow2_extracted"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "session_recovery_flow2_extracted",
        "os": os, "sys": sys, "re": re, "json": json, "types": types,
        "sqlite3": SQLITE3,
        "contextmanager": contextmanager,
        "Any": object, "Optional": object, "Callable": object,
        "Iterator": object, "Dict": dict, "List": list, "Tuple": tuple,
    })
    # cross-module constants first
    extract_consts_into(ns, os.path.join(QUARRY, "persistence", "sqlite",
                        "_schema_ddl.py"), ["SCHEMA_VERSION", "FTS_STORAGE_VERSION"])
    extract_consts_into(ns, os.path.join(QUARRY, "persistence", "sqlite",
                        "conversation", "_store", "rules.py"), ["EPOCH_MIN", "EPOCH_MAX"])
    # module constants (skip the DDL-callback map — seed a plain dict instead)
    ns["_AUXILIARY_TABLE_SCHEMAS"] = {"delivery_obligations": None}

    def seed_consts():
        for node in ast.parse(SRC_TEXT).body:
            if isinstance(node, ast.Assign):
                if not all(isinstance(t, ast.Name) for t in node.targets):
                    continue
            elif isinstance(node, ast.AnnAssign):
                if not (node.value and isinstance(node.target, ast.Name)):
                    continue
            else:
                continue
            try:
                _exec_node(node, ns, SRC)
            except Exception:
                pass

    seed_consts()
    for node in ast.parse(SRC_TEXT).body:
        if isinstance(node, ast.FunctionDef) and node.name in TOP_DEFS:
            if node.name == "_immediate_transaction":
                # Keep the REAL @contextmanager version for composed callers
                # (_sanitize_session_model_config uses `with ...`), and add a
                # decorator-free clone to drive the raw generator directly.
                _exec_node(node, ns, SRC)
                gen = copy.deepcopy(node)
                gen.name = "_immediate_transaction_gen"
                gen.decorator_list = []
                _exec_node(gen, ns, SRC)
                continue
            _exec_node(node, ns, SRC)
    seed_consts()
    required = TOP_DEFS + ["_immediate_transaction_gen"]
    missing = [n for n in required if n not in ns]
    if missing:
        print("ORACLE session_recovery_flow2: FATAL missing defs: %s" % sorted(missing),
              file=sys.stderr)
        sys.exit(2)
    for k in ("_GENERATED_META_KEYS", "_INVENTORY_TABLES", "SCHEMA_VERSION",
              "FTS_STORAGE_VERSION"):
        if k not in ns:
            print("ORACLE session_recovery_flow2: FATAL missing constant: %s" % k,
                  file=sys.stderr)
            sys.exit(2)
    return ns


def install_stubs(ns):
    # CLAIMED siblings — seamed, never re-ported.
    ns["_table_columns"] = lambda conn, table: seam("table_columns", {"table": table})
    ns["_source_fingerprint"] = lambda path: seam("source_fingerprint", {"path": path})


# ── drive ──
def drive_immediate_transaction(ns, inp):
    g = ns["_immediate_transaction_gen"](Conn())
    next(g)  # fires BEGIN IMMEDIATE seam
    spec = inp.get("outcome")
    if isinstance(spec, dict) and "__raise__" in spec:
        e = build_exc(spec)
        CUR["raised"].append((e, spec))
        try:
            g.throw(e)
        except BaseException as caught:
            return project_exc(caught)
        return None
    try:
        next(g)  # normal exit -> COMMIT
    except StopIteration:
        return None
    except BaseException as caught:
        return project_exc(caught)
    return None


def drive(ns, func, inp):
    state = None
    try:
        if func == "count_rows":
            ret = ns["_count_rows"](Conn(), inp.get("table"))
        elif func == "quoted_columns":
            ret = ns["_quoted_columns"](inp.get("columns") or [])
        elif func == "journal_mode":
            ret = ns["_journal_mode"](Conn())
        elif func == "compatible_columns":
            carrier = copy.deepcopy(inp.get("result") or {})
            state = carrier
            ret = ns["_compatible_columns"](Conn(), Conn(), inp.get("table"), carrier)
        elif func == "state_meta_result":
            ret = ns["_state_meta_result"](inp.get("source_rows"), **(inp.get("extra") or {}))
        elif func == "state_meta_precheck":
            ret = ns["_state_meta_precheck"](Conn(), Conn(), inp.get("source_rows"),
                                             salvage=bool(inp.get("salvage", False)))
        elif func == "reconcile":
            ret = ns["_reconcile"](Conn(), inp.get("table"), inp.get("where"),
                                   inp.get("mutation"))
        elif func == "immediate_transaction":
            ret = drive_immediate_transaction(ns, inp)
        elif func == "verify_fts_indexes":
            v = copy.deepcopy(inp.get("verification") or {})
            state = v
            ret = ns["_verify_fts_indexes"](Conn(), v)
        elif func == "verify_structure":
            v = copy.deepcopy(inp.get("verification") or {})
            state = v
            ret = ns["_verify_structure"](Conn(), v)
        elif func == "sanitize_session_model_config":
            ret = ns["_sanitize_session_model_config"](Conn())
        elif func == "recovery_report":
            v = copy.deepcopy(inp.get("verification") or {})
            state = v
            ret = ns["_recovery_report"](
                inp.get("source"), inp.get("output"), inp.get("inspection") or {},
                inp.get("disk_space") or {}, v,
                on_source_change=inp.get("on_source_change", "healthy"),
                **(inp.get("fields") or {}))
        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:
        ret = project_exc(e)
    return ret, state


def main():
    ns = extract()
    install_stubs(ns)
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
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["raised"] = []
        if func not in FUNCS:
            mism += 1
            print("PYERR unknown func: %s" % func)
            continue
        ret, state = drive(ns, func, inp)
        want = {"ret": norm(ret), "trace": CUR["trace"], "state": norm(state)}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE session_recovery_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE session_recovery_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
