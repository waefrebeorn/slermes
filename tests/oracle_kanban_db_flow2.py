#!/usr/bin/env python3
"""Oracle for the kanban_db UNCLAIMED-remainder FLOW port #2
(src/cli/kanban_db_flow2.c).

Drives the LIVE hermes_cli/kanban_db.py defs with SCRIPTED seams: the claimed
I/O siblings (get_task, _persist_scratch_completion_artifacts,
_insert_completion_attachment), the projects_db collaborators reached through
the in-function import (connect_closing enter/exit, get_project,
normalize_slug, Project), the pin ContextVar (set/reset) and the filesystem
probes (open/read/write/close/stat/unlink/rmdir) are recorders fed the SAME
scripts the C harness feeds its seams. The in-function
`from hermes_cli import projects_db` resolves through a per-case sys.modules
stub; an "import:hermes_cli.projects_db" script with import_raise installs
None (CPython halts the import with ModuleNotFoundError "import of
hermes_cli.projects_db halted; None in sys.modules") or raises via PEP 562
parent __getattr__. Compares the DECISION SEQUENCE (ordered structured seam
events) plus ret and state, with ordered canon (key order is part of the
contract).

The live defs are AST-extracted (Assign AND AnnAssign constants, record /
exception classes, all defs) so the heavy quarry import chain never runs.
The composed ported sibling (_project_from_source_task called by
_resolve_project_link) stays REAL on both sides. Fail-closed: 0 cases, a
missing extraction, or an exhausted/missing seam script => nonzero exit /
RuntimeError. BaseException kinds (SystemExit / KeyboardInterrupt /
GeneratorExit) pass through every except-Exception / suppress handler (real
builtin classes are raised). Summary line:
'ORACLE kanban_db_flow2: N cases, M mismatches'.

Deterministic subset (documented in the port header): ASCII slug grammars
(full White_Space strip, ASCII lower); ints or NON-integral floats (JSON
numbers lose int/float identity); lexical POSIX paths (PurePosixPath doubles,
fs probes seamed); SimpleNamespace Task/Project/stat doubles (missing attr =>
double-named AttributeError; get_task uses `is not None` identity); bytes as
canonical padded base64 doubles; KANBAN_ATTACHMENT_MAX_BYTES byte-pinned
default 26214400 with per-case "cap" injection seeded identically on both
sides; _stage metadata mutation modeled by fixture pre-state (the persist
seam records a pre-pop SNAPSHOT — rec() deep-copies args).

Self-test: python tests/oracle_kanban_db_flow2.py --self-test
(drives every fixture case through the LIVE side only; flags script gaps and
AttributeError specs naming ported methods; cross-checks the fixture func set
against the extraction map).
"""
import ast
import base64
import copy
import json
import math
import os
import sys
import types
from pathlib import Path, PurePosixPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parents[1]
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "hermes_cli" / "kanban_db.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "kanban_db.py")
FIXTURE = _REPO / "tests" / "fixtures" / "kanban_db_flow2.jsonl"

os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "kd2_home"))

# C harness func name == LIVE python name (identity map, kept explicit).
PY = {
    "pin_first_board_resolution": "pin_first_board_resolution",
    "_normalize_board_slug": "_normalize_board_slug",
    "_resolve_project_link": "_resolve_project_link",
    "_project_from_source_task": "_project_from_source_task",
    "_stage_completion_artifacts": "_stage_completion_artifacts",
    "_discard_staged_copies": "_discard_staged_copies",
    "_copy_capped": "_copy_capped",
}

MISSING = object()  # script absent / exhausted -> fail closed
DEFAULT_CAP = 25 * 1024 * 1024


def extract():
    import contextlib
    import contextvars
    import logging
    import re
    import secrets
    import sqlite3
    import subprocess
    import time
    from dataclasses import dataclass, field
    from typing import Any, Iterable, Optional

    ns = {
        "__name__": "hermes_cli.kanban_db",
        "os": os, "json": json, "sys": sys, "re": re, "time": time,
        "logging": logging, "contextlib": contextlib, "sqlite3": sqlite3,
        "secrets": secrets, "subprocess": subprocess,
        "ContextVar": contextvars.ContextVar, "Token": contextvars.Token,
        "dataclass": dataclass, "field": field,
        "Any": Any, "Iterable": Iterable, "Optional": Optional,
        "Path": PathDouble,
        "VALID_STATUSES": {"todo", "ready", "running", "blocked", "review",
                           "done", "archived", "triage"},
        "get_toolset_names": lambda: [],
        "unlink_files_older_than": lambda *a, **k: 0,
        "_worker_alive": lambda *a, **k: False,
        "write_txn": None,
    }
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # 1. module constants (Assign AND AnnAssign; plain-Name targets only)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if not isinstance(tgt, ast.Name):
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    # 2. record/exception classes BEFORE defs (annotations reference Task)
    for node in tree.body:
        if isinstance(node, ast.ClassDef):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    # 3. all function defs
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    required = list(PY.values()) + [
        "DEFAULT_BOARD", "_BOARD_SLUG_RE", "KANBAN_ATTACHMENT_MAX_BYTES",
        "ArtifactPreservationError", "_PIN_FIRST_BOARD_RESOLUTION", "Task",
    ]
    missing = [n for n in required if n not in ns]
    if missing:
        print("FATAL kanban_db_flow2: extraction missing names:", sorted(missing))
        sys.exit(2)
    if ns["KANBAN_ATTACHMENT_MAX_BYTES"] != DEFAULT_CAP:
        print("FATAL kanban_db_flow2: live cap drifted from the byte-pinned default")
        sys.exit(2)
    return ns


# ── doubles ──────────────────────────────────────────────────────────────────

CURRENT = {"ctx": None}


def _bytes_double(v):
    if isinstance(v, dict) and set(v.keys()) == {"__bytes__"}:
        return base64.b64decode(v["__bytes__"])
    raise ValueError("invalid canonical base64 bytes double")


class FileDouble:
    """Scripted file object: read/write/close are seams; __enter__ is inert."""
    def __init__(self, path, mode):
        self.path = path
        self.mode = mode

    def __enter__(self):
        return self

    def __exit__(self, et, ev, tb):
        CURRENT["ctx"].rec("close", {"path": self.path})
        return False

    def read(self, n):
        r = CURRENT["ctx"].rec("read", {"path": self.path, "size": n})
        return _bytes_double(r)

    def write(self, data):
        CURRENT["ctx"].rec("write", {"path": self.path,
                                     "data": {"__bytes__": base64.b64encode(data).decode("ascii")}})


class PathDouble(PurePosixPath):
    """Lexical POSIX Path; every filesystem probe is a seam (no fs)."""
    def expanduser(self):
        return PathDouble(CURRENT["ctx"].rec("expanduser", {"path": str(self)}))

    def stat(self):
        r = CURRENT["ctx"].rec("stat", {"path": str(self)})
        return types.SimpleNamespace(**(r if isinstance(r, dict) else {}))

    def unlink(self, missing_ok=False):
        return CURRENT["ctx"].rec("unlink", {"path": str(self), "missing_ok": missing_ok})

    def rmdir(self):
        return CURRENT["ctx"].rec("rmdir", {"path": str(self)})

    def open(self, mode="r", *a, **k):
        CURRENT["ctx"].rec("open", {"path": str(self), "mode": mode})
        return FileDouble(str(self), mode)


class CtxVarDouble:
    def __init__(self, ctx, prefix):
        self.ctx = ctx
        self.prefix = prefix

    def set(self, value):
        return self.ctx.rec(self.prefix + ".set", {"value": value})

    def reset(self, token):
        return self.ctx.rec(self.prefix + ".reset", {"token": token})


def make_exc(spec):
    import sqlite3
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
        "HallucinatedCardsError": NS["HallucinatedCardsError"],
        "EmptyCompletionError": NS["EmptyCompletionError"],
        "ArtifactPreservationError": NS["ArtifactPreservationError"],
        "LiveClaimError": NS["LiveClaimError"],
    }.get(t)
    if cls is KeyError:
        return KeyError(msg)
    if cls is not None:
        if cls in (ExceptionGroup, BaseExceptionGroup):
            return cls(msg, [Exception(msg)])
        if cls in (NS["HallucinatedCardsError"], NS["EmptyCompletionError"],
                   NS["LiveClaimError"]):
            # these customs REWRITE the message in __init__; a scripted seam
            # raise carries a verbatim type+message contract (the C sentinel),
            # so bypass the rewriting ctor while keeping the real class.
            e = cls.__new__(cls)
            BaseException.__init__(e, msg)
            return e
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
        self.state = {}

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
        # deep-copy: a seam arg (metadata) may be mutated AFTER the call by
        # the ported flow (pop); the trace event is a pre-mutation SNAPSHOT,
        # matching the C seam which copies args into the event.
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


def _ns_double(r):
    return types.SimpleNamespace(**r) if isinstance(r, dict) else r


def _make_pdb(ctx):
    stub = types.ModuleType("hermes_cli.projects_db")
    stub.__kd2_stub__ = True

    def normalize_slug(slug):
        return ctx.rec("normalize_slug", {"slug": slug})

    def Project(**kw):
        return _ns_double(ctx.rec("Project", kw))

    def get_project(conn, pid):
        return _ns_double(ctx.rec("get_project", {"conn": conn, "project_id": pid}))

    def connect_closing():
        class CM:
            def __enter__(self):
                return ctx.rec("connect_closing.enter", {})

            def __exit__(self, et, ev, tb):
                exc = {"type": et.__name__, "message": str(ev)} if et is not None else None
                ctx.rec("connect_closing.exit", {"exc": exc})
                return False
        return CM()

    stub.normalize_slug = normalize_slug
    stub.Project = Project
    stub.get_project = get_project
    stub.connect_closing = connect_closing
    return stub


def _install_imports(ctx, inp):
    """Per-case sys.modules gating for `from hermes_cli import projects_db`.
    A None entry HALTS the import (ModuleNotFoundError) before any finder
    runs; the stub parent carries empty __path__ so the editable-install
    meta-path finder never resolves the REAL submodule."""
    io = inp.get("io", {}) or {}
    parent = types.ModuleType("hermes_cli")
    parent.__path__ = []
    parent.__kd2_stub__ = True
    sys.modules["hermes_cli"] = parent
    full = "hermes_cli.projects_db"
    entry = io.get("import:" + full)
    gated = isinstance(entry, dict) and isinstance(entry.get("import_raise"), dict)
    if gated:
        spec = entry["import_raise"]
        if spec.get("type") == "ModuleNotFoundError":
            sys.modules[full] = None
        else:
            # a non-halt scripted import raise: PEP 562 module __getattr__
            # raises it at the from-import PARENT attribute resolution.
            stub = types.ModuleType(full)
            stub.__kd2_stub__ = True

            def parent_getattr(name, _spec=spec):
                if name == "projects_db":
                    raise make_exc(_spec)
                raise AttributeError(name)

            parent.__getattr__ = parent_getattr
            sys.modules[full] = stub
    else:
        stub = _make_pdb(ctx)
        sys.modules[full] = stub
        parent.projects_db = stub


def seed_case(ctx, inp):
    def get_task(conn, tid):
        return _ns_double(ctx.rec("get_task", {"task_id": tid}))

    def persist(conn, task_id, metadata):
        return ctx.rec("_persist_scratch_completion_artifacts",
                       {"task_id": task_id, "metadata": metadata})

    def insert(conn, task_id, *, filename, stored_path, size, created_at,
               uploaded_by="kanban_complete"):
        return ctx.rec("_insert_completion_attachment",
                       {"task_id": task_id, "filename": filename,
                        "stored_path": stored_path, "size": size,
                        "created_at": created_at, "uploaded_by": uploaded_by})

    NS["get_task"] = get_task
    NS["_persist_scratch_completion_artifacts"] = persist
    NS["_insert_completion_attachment"] = insert
    NS["_PIN_FIRST_BOARD_RESOLUTION"] = CtxVarDouble(ctx, "pin_first")
    # per-case cap injection (byte-pinned default; seeded identically in C)
    NS["KANBAN_ATTACHMENT_MAX_BYTES"] = inp.get("cap", DEFAULT_CAP)
    _install_imports(ctx, inp)


def norm_ret(r):
    if isinstance(r, BaseException):
        return {"__raise__": type(r).__name__, "message": str(r)}
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
    out = {"ret": None, "trace": ctx.trace, "state": ctx.state}
    py = PY[func]
    try:
        if func == "pin_first_board_resolution":
            body = inp.get("body")
            with NS[py]():
                if isinstance(body, dict) and isinstance(body.get("raise"), dict):
                    raise make_exc(body["raise"])
            r = None
        elif func == "_normalize_board_slug":
            r = NS[py](inp.get("slug"))
        elif func == "_resolve_project_link":
            r = NS[py](None, inp.get("project_id"), inp.get("project_source_task_id"),
                       inp.get("workspace_kind", "scratch"), inp.get("workspace_path"))
        elif func == "_project_from_source_task":
            pdb = sys.modules["hermes_cli.projects_db"]
            r = NS[py](None, pdb, inp.get("project_id", ""), inp.get("source_task_id", ""))
        elif func == "_stage_completion_artifacts":
            kw = {}
            if "uploaded_by" in inp:
                kw["uploaded_by"] = inp["uploaded_by"]
            r = NS[py](None, inp.get("task_id", ""), inp.get("metadata"),
                       inp.get("now", 0), **kw)
        elif func == "_discard_staged_copies":
            copies = inp.get("copies")
            if isinstance(copies, list):
                copies = [PathDouble(c) if isinstance(c, str) else c for c in copies]
            r = NS[py](copies, PathDouble(inp.get("attachment_dir", "")))
        elif func == "_copy_capped":
            r = NS[py](PathDouble(inp.get("src", "")), PathDouble(inp.get("dest", "")),
                       inp.get("artifact", ""))
        else:
            raise KeyError(func)
        out["ret"] = norm_ret(r)
    except BaseException as e:
        out["ret"] = {"__raise__": type(e).__name__, "message": str(e)}
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
        print("FATAL kanban_db_flow2: fixture missing:", FIXTURE)
        return 2
    funcs = set()
    n = 0
    bad = 0
    ported_names = set(PY.values()) | {
        "get_task", "_persist_scratch_completion_artifacts",
        "_insert_completion_attachment", "normalize_slug", "Project",
        "get_project", "connect_closing",
    }
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
            for name in ported_names:
                if f"'{name}'" in msg:
                    print(f"SELFTEST UNBOUND? {func} case {n}: {msg}")
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
    print(f"SELFTEST kanban_db_flow2: {n} cases, {bad} problems, {len(funcs)} funcs")
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
    print(f"ORACLE kanban_db_flow2: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


NS = extract()

if __name__ == "__main__":
    sys.exit(main())