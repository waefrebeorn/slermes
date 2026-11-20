#!/usr/bin/env python3
"""Oracle for the hermes_cli/projects_db.py worst-tier FLOW port
(src/cli/projects_db_flow.c).

Drives the LIVE projects_db.py defs with SCRIPTED seams: the claimed siblings
reached from the ported set (_normalize_path, list_projects), the external
collaborators (get_hermes_home, Path.resolve, open_db, add_column_if_missing,
time.time, conn.execute/executescript/close) are recorders fed the SAME
scripts the C harness feeds its seams. write_txn (sqlite_util) is STRUCTURAL
on both sides: its BEGIN IMMEDIATE / COMMIT / ROLLBACK conn.execute calls are
seams and the CM logic is transcribed (except Exception -> ROLLBACK, suppress
sqlite3.OperationalError on the ROLLBACK, re-raise; BaseException kinds skip
the ROLLBACK). open_db runs the initialize callback INSIDE the seam (the live
nested _initialize closure); an initialize raise closes the conn (a close
raise masks it). Compares the DECISION SEQUENCE (ordered structured seam
events) plus ret and state, with ordered canon (key order is part of the
contract).

The live defs are AST-extracted (Assign AND AnnAssign constants, the
ProjectFolder/Project dataclasses, all defs, and the nested _initialize/owns
closures for direct drives) so the heavy quarry import chain never runs.
Composed ported siblings stay REAL on both sides. Fail-closed: 0 cases, a
missing extraction, or an exhausted/missing seam script => nonzero exit /
RuntimeError. BaseException kinds (SystemExit / KeyboardInterrupt /
GeneratorExit) pass through every except-Exception / suppress handler (real
builtin classes are raised). Summary line:
'ORACLE projects_db_flow: N cases, M mismatches'.

Deterministic subset (documented in the port header): ASCII slug/name scope
(full White_Space strip, ASCII lower); ints or NON-integral floats in str()
positions (JSON numbers lose int/float identity); lexical POSIX paths
(PurePosixPath doubles, os double sep='/' normcase=posixpath.normcase);
sqlite3.Row doubles built as REAL Rows via SELECT ? AS <col> (row values
str/int/float/null — bool excluded, sqlite coerces to int; keys are SQL
identifiers; rows non-empty); cursor specs {fetchone?, fetchall?, rowcount?}
(missing key -> 'CursorDouble'-named AttributeError); project/folder doubles
are SimpleNamespace (missing attr -> double-named AttributeError) or real
dataclass instances for the to_dict/_load_project paths; params is a list or
null; only table-known exception kinds are scripted (no KeyError /
ExceptionGroup scripts — their str() rewrites the message).

Self-test: python tests/oracle_projects_db_flow.py --self-test
(drives every fixture case through the LIVE side only; flags script gaps and
AttributeError specs naming ported methods / seam stubs; cross-checks the
fixture func set against the PY map).
"""
import ast
import contextlib
import copy
import json
import math
import os
import posixpath
import re
import secrets
import sqlite3
import sys
import types
from dataclasses import dataclass, field
from pathlib import Path, PurePosixPath
from typing import Iterable, List, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parents[1]
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "hermes_cli" / "projects_db.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "projects_db.py")
FIXTURE = _REPO / "tests" / "fixtures" / "projects_db_flow.jsonl"

os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "pdb_home"))

# C harness func name == LIVE python name (identity map, kept explicit).
# The two to_dict methods drive under class-qualified names.
PY = {
    "projects_db_path": "projects_db_path",
    "_slugify": "_slugify",
    "_now": "_now",
    "connect": "connect",
    "_initialize": "_initialize",
    "connect_closing": "connect_closing",
    "ProjectFolder.to_dict": "to_dict",
    "Project.to_dict": "to_dict",
    "_load_project": "_load_project",
    "_primary_path_key": "_primary_path_key",
    "find_by_primary_path": "find_by_primary_path",
    "_execute_rowcount": "_execute_rowcount",
    "_upsert_meta_locked": "_upsert_meta_locked",
    "_get_meta": "_get_meta",
    "_clear_repos_locked": "_clear_repos_locked",
    "get_discovery_policy_key": "get_discovery_policy_key",
    "reconcile_discovered_repos_policy": "reconcile_discovered_repos_policy",
    "clear_discovered_repos": "clear_discovered_repos",
    "owns": "owns",
}

MISSING = object()  # script absent / exhausted -> fail closed

# Byte-pinned live constants (the generator's --check-const verifies the C
# side against the AST; the oracle fails closed on live drift).
EXPECT_OPTIONAL_PROJECT_COLUMNS = ("board_slug", "primary_path", "icon", "color")
EXPECT_OPTIONAL_ROW_FIELDS = ("description", "icon", "color", "board_slug", "primary_path")
EXPECT_ACTIVE_META_KEY = "active_id"
EXPECT_DISCOVERY_POLICY_META_KEY = "repo_discovery_policy"

TREE = ast.parse(open(SRC, encoding="utf-8").read())
_FUT = ast.ImportFrom(module="__future__", names=[ast.alias(name="annotations")], level=0)
ast.fix_missing_locations(_FUT)


def _exec_node(node, ns):
    m = ast.Module(body=[_FUT, node], type_ignores=[])
    ast.fix_missing_locations(m)
    exec(compile(m, SRC, "exec"), ns)


def extract():
    # dataclass KW_ONLY detection needs sys.modules[cls.__module__].__dict__
    mod = types.ModuleType("hermes_cli.projects_db")
    sys.modules["hermes_cli.projects_db"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "hermes_cli.projects_db",
        "os": os, "re": re, "secrets": secrets, "sqlite3": sqlite3,
        "contextlib": contextlib, "time": types.SimpleNamespace(time=lambda: 0.0),
        "dataclass": dataclass, "field": field,
        "Iterable": Iterable, "List": List, "Optional": Optional,
        "Path": PathDouble,
        # module-top imports the ported flows reach (per-case overrides below)
        "_add_column_if_missing": lambda *a, **k: None,
        "open_db": lambda *a, **k: None,
        "write_txn": lambda *a, **k: None,
        "get_hermes_home": lambda: PathDouble("."),
    })
    # 1. module constants (Assign AND AnnAssign; plain-Name targets only)
    for node in TREE.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if not isinstance(tgt, ast.Name):
                continue
            try:
                _exec_node(node, ns)
            except Exception:
                pass
    # 2. record classes BEFORE defs (annotations reference ProjectFolder)
    for node in TREE.body:
        if isinstance(node, ast.ClassDef):
            try:
                _exec_node(node, ns)
            except Exception:
                pass
    # 3. all function defs
    for node in TREE.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                _exec_node(node, ns)
            except Exception:
                pass
    # top-level defs only: _initialize/owns are nested closures extracted on
    # demand; to_dict is a class method (checked via the extracted classes).
    required = sorted(set(PY.values()) - {"_initialize", "owns", "to_dict"}) + [
        "SCHEMA_SQL", "_SLUG_RE", "_BRANCH_SAFE_RE", "_INITIALIZED_PATHS",
        "_OPTIONAL_PROJECT_COLUMNS", "_OPTIONAL_ROW_FIELDS",
        "_ACTIVE_META_KEY", "_DISCOVERY_POLICY_META_KEY",
        "Project", "ProjectFolder",
    ]
    missing = [n for n in required if n not in ns]
    if missing:
        print("FATAL projects_db_flow: extraction missing names:", sorted(missing))
        sys.exit(2)
    for cls_name in ("Project", "ProjectFolder"):
        if not callable(getattr(ns[cls_name], "to_dict", None)):
            print(f"FATAL projects_db_flow: {cls_name}.to_dict not extracted")
            sys.exit(2)
    if closure_node("connect", "_initialize") is None or closure_node("project_for_path", "owns") is None:
        print("FATAL projects_db_flow: nested closures not found")
        sys.exit(2)
    if ns["_OPTIONAL_PROJECT_COLUMNS"] != EXPECT_OPTIONAL_PROJECT_COLUMNS \
            or ns["_OPTIONAL_ROW_FIELDS"] != EXPECT_OPTIONAL_ROW_FIELDS \
            or ns["_ACTIVE_META_KEY"] != EXPECT_ACTIVE_META_KEY \
            or ns["_DISCOVERY_POLICY_META_KEY"] != EXPECT_DISCOVERY_POLICY_META_KEY:
        print("FATAL projects_db_flow: live constants drifted from the byte-pinned values")
        sys.exit(2)
    return ns


def closure_node(parent, name):
    for n in TREE.body:
        if isinstance(n, ast.FunctionDef) and n.name == parent:
            for s in n.body:
                if isinstance(s, ast.FunctionDef) and s.name == name:
                    return s
    return None


# ── doubles ──────────────────────────────────────────────────────────────────

CURRENT = {"ctx": None}


class PathDouble(PurePosixPath):
    """Lexical POSIX Path; resolve() is a seam (no filesystem)."""
    def resolve(self):
        return PathDouble(CURRENT["ctx"].rec("path.resolve", {"path": str(self)}))


_ROW_CONN = sqlite3.connect(":memory:")
_ROW_CONN.row_factory = sqlite3.Row


def make_row(d):
    """A REAL sqlite3.Row with the dict's columns/values (SELECT ? AS k).
    Fail-closed domain: non-empty object; identifier keys; str/int/float/null
    values (bool excluded — sqlite coerces to int)."""
    if not isinstance(d, dict):
        raise TypeError("row double must be an object")
    if not d:
        raise ValueError("row double must be non-empty")
    cols = list(d.keys())
    sql = "SELECT " + ", ".join("? AS " + c for c in cols)
    return _ROW_CONN.execute(sql, list(d.values())).fetchone()


class CursorDouble:
    """Scripted cursor: fetchone/fetchall/rowcount are INERT spec reads (no
    extra seam events); a missing key names the double."""
    def __init__(self, spec):
        if not isinstance(spec, dict):
            raise TypeError("cursor double must be an object")
        self.__dict__["_spec"] = spec

    def __getattr__(self, k):
        spec = self.__dict__["_spec"]
        if k in spec:
            return spec[k]
        raise AttributeError(f"'CursorDouble' object has no attribute '{k}'")

    def fetchone(self):
        spec = self.__dict__["_spec"]
        if "fetchone" not in spec:
            raise AttributeError("'CursorDouble' object has no attribute 'fetchone'")
        r = spec["fetchone"]
        return make_row(r) if r is not None else None

    def fetchall(self):
        spec = self.__dict__["_spec"]
        if "fetchall" not in spec:
            raise AttributeError("'CursorDouble' object has no attribute 'fetchall'")
        rows = spec["fetchall"]
        if not isinstance(rows, list):
            raise TypeError(f"'{type(rows).__name__}' object is not iterable")
        return [make_row(r) for r in rows]

    def __iter__(self):
        return iter(self.fetchall())


class ConnDouble:
    """Scripted connection: execute/executescript/close are seams. The conn
    token never rides in the seam args (single-conn domain)."""
    def __init__(self, token):
        self.token = token

    def execute(self, sql, params=()):
        spec = CURRENT["ctx"].rec("conn.execute", {"sql": sql, "params": list(params)})
        return CursorDouble(spec)

    def executescript(self, sql):
        return CURRENT["ctx"].rec("conn.executescript", {"sql": sql})

    def close(self):
        return CURRENT["ctx"].rec("conn.close", {})


def make_exc(spec):
    t = spec.get("type", "Exception")
    msg = spec.get("message", "")
    cls = {
        "Exception": Exception,
        "BaseException": BaseException,
        "SystemExit": SystemExit,
        "KeyboardInterrupt": KeyboardInterrupt,
        "GeneratorExit": GeneratorExit,
        "ValueError": ValueError,
        "TypeError": TypeError,
        "RuntimeError": RuntimeError,
        "KeyError": KeyError,
        "IndexError": IndexError,
        "AttributeError": AttributeError,
        "OSError": OSError,
        "IOError": OSError,
        "EnvironmentError": OSError,
        "WindowsError": OSError,
        "TimeoutError": TimeoutError,
        "FileNotFoundError": FileNotFoundError,
        "FileExistsError": FileExistsError,
        "PermissionError": PermissionError,
        "IsADirectoryError": IsADirectoryError,
        "NotADirectoryError": NotADirectoryError,
        "ImportError": ImportError,
        "ModuleNotFoundError": ModuleNotFoundError,
        "OverflowError": OverflowError,
        "ZeroDivisionError": ZeroDivisionError,
        "StopIteration": StopIteration,
        "StopAsyncIteration": StopAsyncIteration,
        "NotImplementedError": NotImplementedError,
        "RecursionError": RecursionError,
        "PythonFinalizationError": RuntimeError,
        "UnicodeError": UnicodeError,
        "UnicodeDecodeError": UnicodeDecodeError,
        "UnicodeEncodeError": UnicodeEncodeError,
        "UnicodeTranslateError": UnicodeTranslateError,
        "LookupError": LookupError,
        "NameError": NameError,
        "UnboundLocalError": UnboundLocalError,
        "AssertionError": AssertionError,
        "EOFError": EOFError,
        "MemoryError": MemoryError,
        "ReferenceError": ReferenceError,
        "SystemError": SystemError,
        "SyntaxError": SyntaxError,
        "IndentationError": IndentationError,
        "TabError": TabError,
        "BufferError": BufferError,
        "ArithmeticError": ArithmeticError,
        "FloatingPointError": FloatingPointError,
        "BlockingIOError": BlockingIOError,
        "ChildProcessError": ChildProcessError,
        "ConnectionError": ConnectionError,
        "BrokenPipeError": BrokenPipeError,
        "ConnectionAbortedError": ConnectionAbortedError,
        "ConnectionRefusedError": ConnectionRefusedError,
        "ConnectionResetError": ConnectionResetError,
        "InterruptedError": InterruptedError,
        "ProcessLookupError": ProcessLookupError,
        "Warning": Warning,
        "UserWarning": UserWarning,
        "DeprecationWarning": DeprecationWarning,
        "PendingDeprecationWarning": PendingDeprecationWarning,
        "RuntimeWarning": RuntimeWarning,
        "SyntaxWarning": SyntaxWarning,
        "FutureWarning": FutureWarning,
        "ImportWarning": ImportWarning,
        "UnicodeWarning": UnicodeWarning,
        "BytesWarning": BytesWarning,
        "ResourceWarning": ResourceWarning,
        "EncodingWarning": EncodingWarning,
        "ExceptionGroup": ExceptionGroup,
        "BaseExceptionGroup": BaseExceptionGroup,
        "Error": sqlite3.Error,
        "DatabaseError": sqlite3.DatabaseError,
        "DataError": sqlite3.DataError,
        "IntegrityError": sqlite3.IntegrityError,
        "InternalError": sqlite3.InternalError,
        "NotSupportedError": sqlite3.NotSupportedError,
        "OperationalError": sqlite3.OperationalError,
        "ProgrammingError": sqlite3.ProgrammingError,
        "InterfaceError": sqlite3.InterfaceError,
    }.get(t)
    if cls is KeyError:
        return KeyError(msg)
    if cls is not None:
        if cls in (ExceptionGroup, BaseExceptionGroup):
            return cls(msg, [Exception(msg)])
        return cls(msg)
    # unknown custom kind: a fresh Exception subclass (the fixture generator
    # only scripts table-known kinds; the C table fails closed on unknowns)
    return type(t, (Exception,), {})(msg)


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.io = inp.get("io", {}) or {}
        self.cursors = {}

    def script_next(self, name):
        if name not in self.io:
            return MISSING
        entry = self.io[name]
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return MISSING
            self.cursors[name] = idx + 1
            return entry[idx]
        return entry

    def rec(self, name, args):
        # deep-copy: the trace event is a SNAPSHOT of the args at call time
        # (the C seam copies args into the event before dispatch).
        ev = {"call": name}
        ev.update(copy.deepcopy(args))
        self.trace.append(ev)
        result = self.script_next(name)
        if result is MISSING:
            raise RuntimeError(f"no script for {name}")
        if isinstance(result, dict):
            raise_spec = result.get("raise")
            if isinstance(raise_spec, dict):
                raise make_exc(raise_spec)
            return result.get("ret")
        return result


def _proj_double(p):
    if not isinstance(p, dict):
        return p
    d = dict(p)
    if isinstance(d.get("folders"), list):
        d["folders"] = [types.SimpleNamespace(**f) if isinstance(f, dict) else f
                        for f in d["folders"]]
    return types.SimpleNamespace(**d)


def _inst_double(cls, fields, folders=False):
    """A real dataclass instance with ONLY the given fields set (skip
    __init__) so missing-attribute reads raise the class-named AttributeError."""
    if not isinstance(fields, dict):
        return fields
    obj = object.__new__(cls)
    for k, v in fields.items():
        if folders and k == "folders" and isinstance(v, list):
            v = [_inst_double(NS["ProjectFolder"], f) for f in v]
        setattr(obj, k, v)
    return obj


def _mat_path(v):
    if v is None:
        return None
    if isinstance(v, str):
        return PathDouble(v)
    return v


def make_write_txn():
    """STRUCTURAL write_txn (sqlite_util): BEGIN IMMEDIATE / COMMIT /
    ROLLBACK are conn.execute seams; except Exception -> ROLLBACK (suppress
    OperationalError on the ROLLBACK) + re-raise; BaseException skips it."""
    @contextlib.contextmanager
    def write_txn(conn):
        conn.execute("BEGIN IMMEDIATE")
        try:
            yield conn
        except Exception:
            try:
                conn.execute("ROLLBACK")
            except sqlite3.OperationalError:
                pass
            raise
        else:
            conn.execute("COMMIT")
    return write_txn


def open_db_stub(path, *, db_label, foreign_keys=False, initialize=None, **kw):
    ctx = CURRENT["ctx"]
    token = ctx.rec("open_db", {"path": str(path), "db_label": db_label,
                                "foreign_keys": foreign_keys,
                                "initialize": initialize is not None})
    conn = ConnDouble(token)
    if initialize is not None:
        try:
            initialize(conn)
        except BaseException:
            conn.close()
            raise
    return conn


def seed_case(ctx, inp):
    NS["get_hermes_home"] = lambda: PathDouble(ctx.rec("get_hermes_home", {}))
    NS["time"] = types.SimpleNamespace(time=lambda: ctx.rec("time.time", {}))
    NS["os"] = types.SimpleNamespace(
        sep="/",
        path=types.SimpleNamespace(normcase=posixpath.normcase,
                                   abspath=posixpath.abspath,
                                   expanduser=posixpath.expanduser,
                                   basename=posixpath.basename))
    NS["_normalize_path"] = lambda path: ctx.rec("normalize_path", {"path": path})
    NS["list_projects"] = lambda conn, *, include_archived=False: [
        _proj_double(p) for p in ctx.rec("list_projects", {"include_archived": include_archived})]
    NS["_add_column_if_missing"] = lambda conn, table, column, ddl: ctx.rec(
        "add_column_if_missing", {"table": table, "column": column, "ddl": ddl})
    NS["open_db"] = open_db_stub
    NS["write_txn"] = make_write_txn()
    st = inp.get("state") or {}
    NS["_INITIALIZED_PATHS"] = set(st.get("initialized_paths") or [])


def norm_ret(r):
    if isinstance(r, BaseException):
        return {"__raise__": type(r).__name__, "message": str(r)}
    if isinstance(r, ConnDouble):
        return r.token
    if isinstance(r, (NS["Project"], NS["ProjectFolder"])):
        return {k: norm_ret(v) for k, v in vars(r).items()}
    if isinstance(r, types.SimpleNamespace):
        return {k: norm_ret(v) for k, v in vars(r).items()}
    if isinstance(r, PurePosixPath):
        return r.as_posix()
    if isinstance(r, tuple):
        return [norm_ret(x) for x in r]
    if isinstance(r, list):
        return [norm_ret(x) for x in r]
    if isinstance(r, dict):
        return {k: norm_ret(v) for k, v in r.items()}
    if isinstance(r, float):
        if math.isfinite(r) and r.is_integer():
            return int(r)
        return r
    return r


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    seed_case(ctx, inp)
    out = {"ret": None, "trace": ctx.trace, "state": {}}
    conn = ConnDouble(inp.get("conn"))
    try:
        if func == "projects_db_path":
            r = NS["projects_db_path"]()
        elif func == "_slugify":
            r = NS["_slugify"](inp.get("name"))
        elif func == "_now":
            r = NS["_now"]()
        elif func == "connect":
            r = NS["connect"](db_path=_mat_path(inp.get("db_path")))
        elif func == "_initialize":
            node = closure_node("connect", "_initialize")
            if node is None:
                raise RuntimeError("nested _initialize closure not found")
            NS["resolved"] = inp.get("resolved", "")
            _exec_node(node, NS)
            r = NS["_initialize"](conn)
        elif func == "connect_closing":
            body = inp.get("body")
            with NS["connect_closing"](db_path=_mat_path(inp.get("db_path"))) as c:
                if isinstance(body, dict) and isinstance(body.get("raise"), dict):
                    raise make_exc(body["raise"])
            r = None
        elif func == "ProjectFolder.to_dict":
            r = NS["ProjectFolder"].to_dict(_inst_double(NS["ProjectFolder"], inp.get("folder")))
        elif func == "Project.to_dict":
            r = NS["Project"].to_dict(_inst_double(NS["Project"], inp.get("project"), folders=True))
        elif func == "_load_project":
            r = NS["_load_project"](conn, make_row(inp.get("row")))
        elif func == "_primary_path_key":
            r = NS["_primary_path_key"](inp.get("path"))
        elif func == "find_by_primary_path":
            r = NS["find_by_primary_path"](conn, inp.get("path"),
                                           include_archived=inp.get("include_archived", False))
        elif func == "_execute_rowcount":
            r = NS["_execute_rowcount"](conn, inp.get("sql"), inp.get("params"))
        elif func == "_upsert_meta_locked":
            r = NS["_upsert_meta_locked"](conn, inp.get("key"), inp.get("value"))
        elif func == "_get_meta":
            r = NS["_get_meta"](conn, inp.get("key"))
        elif func == "_clear_repos_locked":
            r = NS["_clear_repos_locked"](conn, inp.get("clear", False), inp.get("policy_key"))
        elif func == "get_discovery_policy_key":
            r = NS["get_discovery_policy_key"](conn)
        elif func == "reconcile_discovered_repos_policy":
            r = NS["reconcile_discovered_repos_policy"](
                conn, inp.get("policy_key"),
                preserve_unversioned=inp.get("preserve_unversioned", False))
        elif func == "clear_discovered_repos":
            r = NS["clear_discovered_repos"](conn, policy_key=inp.get("policy_key"))
        elif func == "owns":
            node = closure_node("project_for_path", "owns")
            if node is None:
                raise RuntimeError("nested owns closure not found")
            NS["target"] = inp.get("target", "")
            _exec_node(node, NS)
            r = NS["owns"](inp.get("folder", ""))
        else:
            raise KeyError(func)
        out["ret"] = norm_ret(r)
    except BaseException as e:
        out["ret"] = {"__raise__": type(e).__name__, "message": str(e)}
    out["state"] = {"initialized_paths": sorted(NS["_INITIALIZED_PATHS"])}
    return out


def canon(o):
    return json.dumps(o, ensure_ascii=False, sort_keys=False, default=str)


def self_test():
    """Drive every fixture through the LIVE side only (no C involved). Flag
    drive exceptions (script gaps / binding bugs) AND AttributeError ret specs
    whose message names a ported method or seam stub."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    if not FIXTURE.is_file():
        print("FATAL projects_db_flow: fixture missing:", FIXTURE)
        return 2
    funcs = set()
    n = 0
    bad = 0
    ported_names = set(PY.values()) | {
        "get_hermes_home", "normalize_path", "list_projects", "open_db",
        "write_txn", "add_column_if_missing", "_initialize", "owns",
        "resolve", "execute", "executescript", "close",
        "fetchone", "fetchall", "rowcount", "time",
    }
    # Carriers whose AttributeError reads are DOCUMENTED domain branches
    # (double-class missing keys / dataclass fields / raw scalar carriers),
    # not unbound-stub signals.
    expected_carriers = {
        "CursorDouble", "ConnDouble", "Project", "ProjectFolder",
        "types.SimpleNamespace", "int", "float", "str", "bool",
        "NoneType", "list", "dict", "PathDouble", "PurePosixPath",
    }
    attr_re = re.compile(r"^'([^']+)' object has no attribute '([^']+)'$")
    # split on "\n" ONLY: fixture strings may carry raw U+0085/2028/2029.
    for line in FIXTURE.read_text(encoding="utf-8").split("\n"):
        line = line.strip()
        if not line.startswith("{"):
            continue
        c = json.loads(line)
        func = c.get("func", "")
        funcs.add(func)
        n += 1
        if func not in PY:
            print(f"SELFTEST unknown func {func}")
            bad += 1
            continue
        try:
            out = run_case(func, c.get("in") or {})
        except BaseException as e:
            print(f"SELFTEST DRIVE-EXC {func} case {n}: {type(e).__name__}: {e}")
            bad += 1
            continue
        ret = out.get("ret")
        if isinstance(ret, dict) and ret.get("__raise__") == "AttributeError":
            msg = ret.get("message", "")
            m = attr_re.match(msg)
            carrier = m.group(1) if m else ""
            if carrier not in expected_carriers:
                for name in ported_names:
                    if f"'{name}'" in msg:
                        print(f"SELFTEST UNBOUND? {func} case {n}: {msg}")
                        bad += 1
                        break
        if isinstance(ret, dict) and ret.get("__raise__") == "NameError":
            msg = ret.get("message", "")
            for name in ported_names:
                if f"'{name}'" in msg:
                    print(f"SELFTEST UNSEEDED? {func} case {n}: {msg}")
                    bad += 1
                    break
    unknown = funcs - set(PY)
    untested = set(PY) - funcs
    if unknown:
        print("SELFTEST fixture funcs absent from PY map:", sorted(unknown))
        bad += 1
    if untested:
        print("SELFTEST PY funcs absent from fixtures:", sorted(untested))
        bad += 1
    print(f"SELFTEST projects_db_flow: {n} cases, {bad} problems, {len(funcs)} funcs")
    faulthandler.cancel_dump_traceback_later()
    return 0 if bad == 0 else 1


def main():
    if "--self-test" in sys.argv:
        return self_test()
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        cases += 1
        func = c.get("func", "")
        if func not in PY:
            mism += 1
            print(f"PYERR {func}: unknown func")
            continue
        try:
            want = run_case(func, c.get("in") or {})
        except Exception as e:
            mism += 1
            print(f"PYERR {func}: {type(e).__name__}: {e}")
            continue
        got = c.get("out") or {}
        ok = (canon(got.get("trace")) == canon(want.get("trace"))
              and canon(got.get("ret")) == canon(want.get("ret"))
              and canon(got.get("state")) == canon(want.get("state")))
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={canon(got)}")
            print(f"  PY ={canon(want)}")
    print(f"ORACLE projects_db_flow: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


NS = extract()

if __name__ == "__main__":
    sys.exit(main())