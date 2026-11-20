#!/usr/bin/env python3
"""Oracle for the cron/incidents.py FLOW2 port (src/cron/cron_incidents_flow2.c).

Drives the LIVE, UNCLAIMED cron/incidents.py defs with SCRIPTED I/O: every
module-level collaborator, claimed/ported sibling and inner-imported dependency
is a recording stub fed the same scripts the C harness feeds its seams. Compares
the DECISION SEQUENCE (ordered seam-call NAMES in "trace"), the final return
("ret") and the mutated module globals ("state").

Live defs are AST-extracted (module constants seeded, PEP 563 annotation flag)
so the heavy quarry import chain is never imported. Fail-closed: 0 cases => 1.
Summary: 'ORACLE cron_incidents_flow2: N cases, M mismatches'.

Deterministic subset (documented):
 - trace is NAME-ONLY; seam results are scripted by in.io (single object reused,
   bare list = per-call queue; {"raise": {...}} models a scripted exception).
 - the module global EXECUTIONS_FILE is injected as `in.module_file`; the
   _executions.EXECUTIONS_FILE attribute read is the "executions_file" seam.
 - Path values project to str(Path.as_posix) strings on both sides.
 - full builtin exception hierarchy: BaseException-only kinds propagate;
   scripted raises of every builtin class are materialised exactly.
"""
import ast
import copy
import errno
import json
import os
import sys
import types
import re as _re
import hashlib
import threading
from pathlib import Path
import __future__
from typing import Optional, Dict, Iterator, List, Any

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stdin, "reconfigure"):
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "cron" / "incidents.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "cron" / "incidents.py")

os.environ.setdefault("HOME", str(_HERE.parents[2]))
os.environ.setdefault("USERPROFILE", str(_HERE.parents[2]))
os.environ.setdefault("LOCALAPPDATA", str(_HERE.parents[2] / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_HERE.parents[1] / ".testbin" / "dbg_home"))

# C harness func -> LIVE python name
PY = {
    "db_path": "_db_path",
    "connect": "_connect",
    "initialize_schema": "_initialize_schema",
    "transaction": "_transaction",
    "set_incident_state": "set_incident_state",
    "ack_incident": "ack_incident",
    "close_incidents": "close_incidents_for_recovered_job",
    "state_filter": "_state_filter",
    "count_incidents": "count_incidents",
}

# ── full builtin exception hierarchy (name -> class) ────────────────────────
_EXC = {
    "BaseException": BaseException, "Exception": Exception,
    "GeneratorExit": GeneratorExit, "KeyboardInterrupt": KeyboardInterrupt,
    "SystemExit": SystemExit,
    "ArithmeticError": ArithmeticError, "AssertionError": AssertionError,
    "AttributeError": AttributeError, "BufferError": BufferError,
    "BytesWarning": BytesWarning, "DeprecationWarning": DeprecationWarning,
    "EOFError": EOFError, "EnvironmentError": OSError,
    "FloatingPointError": FloatingPointError, "FutureWarning": FutureWarning,
    "ImportError": ImportError, "ImportWarning": ImportWarning,
    "IndentationError": IndentationError, "IndexError": IndexError,
    "IOError": OSError, "KeyError": KeyError, "LookupError": LookupError,
    "MemoryError": MemoryError, "NameError": NameError,
    "NotImplementedError": NotImplementedError, "OSError": OSError,
    "OverflowError": OverflowError, "PendingDeprecationWarning": PendingDeprecationWarning,
    "ReferenceError": ReferenceError, "ResourceWarning": ResourceWarning,
    "RuntimeError": RuntimeError, "RuntimeWarning": RuntimeWarning,
    "StopIteration": StopIteration, "SyntaxError": SyntaxError,
    "SyntaxWarning": SyntaxWarning, "SystemError": SystemError,
    "TabError": TabError, "TimeoutError": TimeoutError,
    "TypeError": TypeError, "UnboundLocalError": UnboundLocalError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeError": UnicodeError, "UnicodeTranslateError": UnicodeTranslateError,
    "UnicodeWarning": UnicodeWarning, "UserWarning": UserWarning,
    "ValueError": ValueError, "Warning": Warning, "ZeroDivisionError": ZeroDivisionError,
    "BlockingIOError": BlockingIOError, "ChildProcessError": ChildProcessError,
    "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "FileExistsError": FileExistsError,
    "FileNotFoundError": FileNotFoundError, "InterruptedError": InterruptedError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
    "ModuleNotFoundError": ModuleNotFoundError, "RecursionError": RecursionError,
    "StopAsyncIteration": StopAsyncIteration, "EncodingWarning": EncodingWarning,
}
for _n in dir(builtins := __import__("builtins")):
    _o = getattr(builtins, _n)
    if isinstance(_o, type) and issubclass(_o, BaseException) and _n not in _EXC:
        _EXC[_n] = _o


def make_exc(spec):
    t = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    cls = _EXC.get(t) or type(t, (Exception,), {})
    try:
        if t in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
            e = cls.__new__(cls)
            Exception.__init__(e, msg)
        else:
            e = cls(msg) if msg else cls()
    except Exception:
        e = cls(msg)
    en = spec.get("errno")
    if en and isinstance(e, OSError):
        try:
            e.errno = getattr(errno, en, None)
        except Exception:
            pass
    return e


CUR = {"ctx": None}


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.io = inp.get("io", {}) or {}
        self.cursors = {}
        self.state = copy.deepcopy(inp.get("state", {}) or {})

    def script_next(self, name):
        if name not in self.io:           # fail-closed: missing script -> None
            return None
        entry = self.io[name]
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return None
            self.cursors[name] = idx + 1
            return entry[idx]
        return entry

    def recorder(self, name):
        ctx = self
        def call(*args, **kwargs):
            ctx.trace.append(name)
            result = ctx.script_next(name)
            if isinstance(result, dict):
                eff = result.get("state")
                if isinstance(eff, dict):
                    ctx.state.update(eff)
                rs = result.get("raise") or result.get("__raise__")
                if isinstance(rs, dict):
                    raise make_exc(rs)
                return result.get("ret")
            return result
        return call


# ── object doubles ──────────────────────────────────────────────────────────
class _PathDouble:
    """Path seam carrier. Construction is pure; .resolve() is a seam; / is pure."""
    def __init__(self, p):
        self.p = p if isinstance(p, str) else str(p)
    def resolve(self):
        v = CUR["ctx"].recorder("path.resolve")()
        return _PathDouble(v if isinstance(v, str) else self.p)
    def __truediv__(self, other):
        base = self.p
        if base == "/":
            return _PathDouble("/" + str(other))
        return _PathDouble(base.rstrip("/") + "/" + str(other))
    @property
    def parent(self):
        i = self.p.rfind("/")
        if i < 0:
            return _PathDouble(".")
        if i == 0:
            return _PathDouble("/")
        return _PathDouble(self.p[:i])
    def __str__(self):
        return self.p
    def __repr__(self):
        return self.p


class _ExecutionsDouble:
    """cron.executions module carrier: attribute read is a seam."""
    @property
    def EXECUTIONS_FILE(self):
        return CUR["ctx"].recorder("executions_file")()


class _NowDouble:
    def isoformat(self):
        return CUR["ctx"].recorder("hermes_now")()


def sqlname(sql):
    """Map a live conn.execute SQL literal to the C seam name (order matters)."""
    s = sql or ""
    if s.startswith("CREATE TABLE"):
        return "sql.create_table"
    if s.startswith("CREATE INDEX"):
        return "sql.index_job" if "job" in s else "sql.index_state"
    if s.startswith("SELECT COUNT(*)"):
        return "sql.count_filtered" if " WHERE " in s else "sql.count"
    if s.startswith("SELECT *"):
        return "sql.select_all"
    if "state='alerted'" in s:
        return "sql.update_alerted"
    if "state='closed'" in s:
        return "sql.update_closed"
    if "state='resolved'" in s:
        return "sql.update_resolved"
    if "SET state=?" in s:
        return "sql.update_state"
    if s.startswith("SELECT"):
        return "sql.select"
    if s.startswith("INSERT"):
        return "sql.insert"
    return "sql.execute"


class CursorDouble:
    """Result of conn.execute; fetchone/fetchall/rowcount read the one scripted result."""
    def __init__(self, r):
        self.r = r
    def fetchone(self):
        return self.r.get("fetchone") if isinstance(self.r, dict) else None
    def fetchall(self):
        return self.r.get("fetchall", []) if isinstance(self.r, dict) else []
    @property
    def rowcount(self):
        return self.r.get("rowcount") if isinstance(self.r, dict) else None


class ConnDouble:
    def __init__(self, ctx):
        self.ctx = ctx
    def execute(self, sql, params=None):
        return CursorDouble(self.ctx.recorder(sqlname(sql))())


class _SetTxCM:
    """_transaction() context manager for set/close/count flows (seamed sibling)."""
    def __init__(self, ctx):
        self.ctx = ctx
    def __enter__(self):
        self.ctx.recorder("tx.enter")()
        return ConnDouble(self.ctx)
    def __exit__(self, *a):
        self.ctx.recorder("tx.exit")()
        return False


class _TxCM:
    """Inner sqlite transaction(conn) CM for the dedicated _transaction flow."""
    def __init__(self, ctx, conn):
        self.ctx = ctx
        self.conn = conn
    def __enter__(self):
        return self.ctx.recorder("transaction.enter")()
    def __exit__(self, *a):
        self.ctx.recorder("transaction.exit")()
        return False


class _SilentCM:
    def __enter__(self):
        return None
    def __exit__(self, *a):
        return False


# ── sys.modules stubs for in-function imports ───────────────────────────────
def _install_import_stubs():
    cron_pkg = sys.modules.setdefault("cron", types.ModuleType("cron"))
    cron_pkg.__path__ = []
    jobs = types.ModuleType("cron.jobs")
    jobs._ensure_cron_dir = lambda *a, **k: None
    sys.modules["cron.jobs"] = jobs
    cron_pkg.jobs = jobs
    executions = types.ModuleType("cron.executions")
    executions.EXECUTIONS_FILE = None
    sys.modules["cron.executions"] = executions
    cron_pkg.executions = executions

    cli = sys.modules.setdefault("hermes_cli", types.ModuleType("hermes_cli"))
    cli.__path__ = []
    sq = types.ModuleType("hermes_cli.sqlite_util")
    sq.open_db = lambda *a, **k: None
    sq.add_column_if_missing = lambda *a, **k: None
    sq.transaction = lambda conn: _SilentCM()
    sys.modules["hermes_cli.sqlite_util"] = sq
    cli.sqlite_util = sq
    return cron_pkg, jobs, sq


CRON_PKG, CRON_JOBS, SQLITE = _install_import_stubs()
_ANN_FLAG = __future__.annotations.compiler_flag


def extract():
    mod = types.ModuleType("cron.incidents")
    sys.modules["cron.incidents"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "cron.incidents",
        "threading": threading, "re": _re, "hashlib": hashlib,
        "Optional": Optional, "Dict": Dict, "Iterator": Iterator,
        "List": List, "Any": Any,
        # top-level imports (resolved to seam doubles per case)
        "Path": _PathDouble,
        "get_hermes_home": lambda: _PathDouble(""),
        "_hermes_now": lambda: _NowDouble(),
        "_executions": _ExecutionsDouble(),
    })
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if not isinstance(tgt, ast.Name):
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                             flags=_ANN_FLAG), ns)
            except Exception:
                pass
    want = set(PY.values())
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in want:
            node = copy.copy(node)
            node.decorator_list = []          # strip @contextmanager etc.
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                             flags=_ANN_FLAG), ns)
            except Exception:
                pass
    missing = sorted(v for v in want if v not in ns)
    if missing:
        print("FATAL cron_incidents_flow2: extraction missing names:", missing)
        sys.exit(2)
    return ns


NS = extract()
REAL = {v: NS[v] for v in PY.values()}


def seed_case(ctx, inp, func):
    CUR["ctx"] = ctx
    for k, v in REAL.items():
        NS[k] = v
    NS["EXECUTIONS_FILE"] = inp.get("module_file")
    NS["_executions"] = _ExecutionsDouble()
    NS["get_hermes_home"] = lambda: _PathDouble(ctx.recorder("get_hermes_home")())
    NS["_hermes_now"] = lambda: _NowDouble()
    NS["_lock"] = _SilentCM()
    CRON_JOBS._ensure_cron_dir = ctx.recorder("ensure_cron_dir")
    SQLITE.open_db = ctx.recorder("open_db")
    SQLITE.add_column_if_missing = ctx.recorder("add_column_if_missing")
    SQLITE.transaction = lambda conn: _TxCM(ctx, conn)
    # ported siblings seamed for composed flows
    NS["_db_path"] = lambda: _PathDouble(ctx.recorder("_db_path")())
    NS["_connect"] = ctx.recorder("_connect")
    NS["set_incident_state"] = ctx.recorder("set_incident_state")
    if func == "transaction":
        NS["_transaction"] = REAL["_transaction"]   # drive the real generator
    else:
        NS["_transaction"] = lambda: _SetTxCM(ctx)


def norm_ret(r):
    if isinstance(r, BaseException):
        return {"__raise__": type(r).__name__, "message": str(r)}
    if isinstance(r, _PathDouble):
        return str(r)
    if isinstance(r, (tuple, list)):
        return [norm_ret(x) for x in r]
    if isinstance(r, dict):
        return {k: norm_ret(v) for k, v in r.items()}
    return r


def run_case(func, inp):
    ctx = CaseCtx(inp)
    seed_case(ctx, inp, func)
    out = {"ret": None, "trace": ctx.trace, "state": ctx.state}
    fn = REAL[PY[func]]
    try:
        if func == "db_path":
            out["ret"] = fn()
        elif func == "connect":
            out["ret"] = fn()
        elif func == "initialize_schema":
            fn(ConnDouble(ctx))              # live def takes `conn` and calls conn.execute
            out["ret"] = None
        elif func == "transaction":
            gen = fn()
            val = next(gen)
            try:
                gen.send(None)
            except StopIteration:
                pass
            out["ret"] = val
        elif func == "set_incident_state":
            out["ret"] = fn(inp.get("incident_id", ""), inp.get("inc_state"))
        elif func == "ack_incident":
            out["ret"] = fn(inp.get("incident_id", ""))
        elif func == "close_incidents":
            out["ret"] = fn(inp.get("job_id"))
        elif func == "state_filter":
            out["ret"] = fn(inp.get("inc_state"))
        elif func == "count_incidents":
            out["ret"] = fn(inp.get("inc_state"))
        else:
            raise KeyError(func)
    except BaseException as e:
        out["ret"] = {"__raise__": type(e).__name__, "message": str(e)}
    out["ret"] = norm_ret(out["ret"])
    return out


def main():
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
        ok = (got.get("trace") == want.get("trace")
              and got.get("ret") == want.get("ret")
              and got.get("state") == want.get("state"))
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(got, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE cron_incidents_flow2: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
