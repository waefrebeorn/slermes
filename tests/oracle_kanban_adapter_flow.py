#!/usr/bin/env python3
"""Oracle for the kanban adapter UNCLAIMED-remainder FLOW port
(src/cli/kanban_adapter_flow.c).

MODULE RESOLUTION: the task names "hermes_cli/kanban_adapter.py", which does
not exist in the quarry; the legacy adapter object (src/tools/kanban_adapter.c)
declares hermes_cli/kanban_db.py as its semantic source of truth, so this oracle
drives the LIVE unclaimed kanban_db.py defs (PoP grep verified: 39 unclaimed,
33 ported here).

Drives the LIVE defs with SCRIPTED seams: conn.execute is a recording stub
returning scripted cursor specs (rows / fetch_raise), cursor iteration records
one "iter" event + one "next" per yielded row, write_txn is a context-manager
double recording enter/exit, and the claimed I/O siblings (_normalize_board_slug,
get_current_board, read_board_metadata, _claimer_id, kanban_home, board_dir,
_fire_kanban_lifecycle_hook, _end_run, _synthesize_ended_run,
_verify_created_cards, _append_event, _scan_prose_for_phantom_ids,
subprocess.run) plus environment probes (os.environ.get, ContextVar.get,
Path.expanduser) are recorders fed the SAME scripts the C harness feeds its
seams. In-function imports (hermes_cli.profiles / agent.delegation_context /
hermes_cli.projects_db) resolve through per-case sys.modules stubs; an
"import:<module>" script with import_raise installs None (CPython halts the
import with ModuleNotFoundError "import of <module> halted; None in
sys.modules"). Compares the DECISION SEQUENCE (ordered structured seam events,
SQL verbatim) plus ret and state, with ordered canon (key order is part of the
contract).

The live defs are AST-extracted (Assign AND AnnAssign constants, record /
exception classes, all defs) so the heavy quarry import chain never runs.
Composed ported siblings (_explicit_board_slug, _explicit_board_intent_pinned,
_first_line) stay REAL on both sides. Fail-closed: 0 cases, a missing
extraction, or an exhausted/missing seam script => nonzero exit / RuntimeError.
BaseException kinds (SystemExit / KeyboardInterrupt / GeneratorExit) pass
through every except-Exception handler (real builtin classes are raised).
Summary line: 'ORACLE kanban_adapter_flow: N cases, M mismatches'.

Deterministic subset (documented in the port header): ASCII int/strip/splitlines
grammars + common Unicode whitespace/line separators; bytes travel as canonical
padded base64 doubles; strict json.loads grammar (no NaN/Infinity/lone
surrogates/>2^53 ints); lexical POSIX paths (PurePosixPath, expanduser seamed);
sqlite3.Row dict doubles; SimpleNamespace task/result doubles; JSON numbers
lose int/float identity (integral floats canon to ints; _json_or_null fixtures
carry ints or non-integral floats).

Self-test: python tests/oracle_kanban_adapter_flow.py --self-test
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
FIXTURE = _REPO / "tests" / "fixtures" / "kanban_adapter_flow.jsonl"

os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "ka_home"))

# C harness func name == LIVE python name (identity map, kept explicit).
PY = {
    "_lossy_text": "_lossy_text",
    "_row_get": "_row_get",
    "_json_or": "_json_or",
    "_json_dict": "_json_dict",
    "_env_int": "_env_int",
    "_first_line": "_first_line",
    "_opt_int": "_opt_int",
    "_json_or_null": "_json_or_null",
    "_substantive_text": "_substantive_text",
    "_nonblank_str": "_nonblank_str",
    "_cleaned_artifact_paths": "_cleaned_artifact_paths",
    "_retention_seconds": "_retention_seconds",
    "_slug_or_default": "_slug_or_default",
    "_require_slug": "_require_slug",
    "_host_prefix": "_host_prefix",
    "_board_meta_for": "_board_meta_for",
    "_hook_profile_name": "_hook_profile_name",
    "_fire_task_hook": "_fire_task_hook",
    "_explicit_board_slug": "_explicit_board_slug",
    "_explicit_board_intent_pinned": "_explicit_board_intent_pinned",
    "_board_path": "_board_path",
    "_link": "_link",
    "_linked_ids": "_linked_ids",
    "_require_task": "_require_task",
    "_task_rows": "_task_rows",
    "_latest_event": "_latest_event",
    "_counts_by_assignee": "_counts_by_assignee",
    "current_run_started_ats": "current_run_started_ats",
    "_end_or_synthesize_run": "_end_or_synthesize_run",
    "_gate_created_cards": "_gate_created_cards",
    "_flag_phantom_prose_refs": "_flag_phantom_prose_refs",
    "_git_out": "_git_out",
    "_project_branch_name": "_project_branch_name",
}

# In-function import channels: module -> (parent package, attr, seam name).
IMPORT_CHANNELS = {
    "hermes_cli.profiles": ("hermes_cli", "get_active_profile_name", "get_active_profile_name"),
    "agent.delegation_context": ("agent", "explicit_board_intent_is_pinned", "explicit_board_intent_is_pinned"),
    "hermes_cli.projects_db": ("hermes_cli", "branch_name_for", "branch_name_for"),
}

MISSING = object()  # script absent / exhausted -> fail closed


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
        "DEFAULT_BOARD", "_UNSET", "Task", "HallucinatedCardsError",
        "_CURRENT_BOARD_OVERRIDE", "_PIN_FIRST_BOARD_RESOLUTION",
    ]
    missing = [n for n in required if n not in ns]
    if missing:
        print("FATAL kanban_adapter_flow: extraction missing names:", sorted(missing))
        sys.exit(2)
    return ns


# ── doubles ──────────────────────────────────────────────────────────────────

class PathDouble(PurePosixPath):
    """Lexical POSIX Path; .expanduser() is the expanduser seam (no fs)."""
    def expanduser(self):
        return PathDouble(CURRENT["ctx"].rec("expanduser", {"path": str(self)}))


class EnvironDouble:
    def __init__(self, ctx):
        self.ctx = ctx

    def get(self, name, default=None):
        return self.ctx.rec("environ.get", {"name": name, "default": default})


class CtxVarDouble:
    def __init__(self, ctx, seam):
        self.ctx = ctx
        self.seam = seam

    def get(self, *a):
        return self.ctx.rec(self.seam, {})


class _Cursor:
    def __init__(self, ctx, spec):
        self.ctx = ctx
        spec = spec if isinstance(spec, dict) else {}
        self.rows = list(spec.get("rows") or [])
        self.idx = 0
        self.fetch_raise = spec.get("fetch_raise")

    def _fetch_pre(self, name):
        self.ctx.trace.append({"call": name})
        if isinstance(self.fetch_raise, dict):
            raise make_exc(self.fetch_raise)

    def fetchone(self):
        self._fetch_pre("fetchone")
        if self.idx >= len(self.rows):
            return None
        r = self.rows[self.idx]
        self.idx += 1
        return r

    def fetchall(self):
        self._fetch_pre("fetchall")
        rs = self.rows[self.idx:]
        self.idx = len(self.rows)
        return rs

    def __iter__(self):
        self.ctx.trace.append({"call": "iter"})
        if isinstance(self.fetch_raise, dict):
            raise make_exc(self.fetch_raise)
        return self

    def __next__(self):
        # the final StopIteration __next__ is UNTRACED on both sides
        if self.idx >= len(self.rows):
            raise StopIteration
        self.ctx.trace.append({"call": "next"})
        r = self.rows[self.idx]
        self.idx += 1
        return r


class _Conn:
    def __init__(self, ctx):
        self.ctx = ctx

    def execute(self, sql, params=()):
        ctx = self.ctx
        ctx.trace.append({"call": "execute", "sql": sql, "params": list(params)})
        result = ctx.script_next("execute")
        if result is MISSING:
            raise RuntimeError("no script for execute")
        spec = None
        if isinstance(result, dict):
            raise_spec = result.get("raise")
            if isinstance(raise_spec, dict):
                raise make_exc(raise_spec)
            spec = result.get("ret")
        else:
            spec = result
        return _Cursor(ctx, spec)


class _WriteTxnCM:
    def __init__(self, ctx, conn, allow_nested):
        self.ctx = ctx
        self.conn = conn
        self.allow_nested = allow_nested

    def __enter__(self):
        self.ctx.rec("write_txn.enter", {"allow_nested": self.allow_nested})
        return self.conn

    def __exit__(self, et, ev, tb):
        exc = {"type": et.__name__, "message": str(ev)} if et is not None else None
        self.ctx.rec("write_txn.exit", {"exc": exc})
        return False


CURRENT = {"ctx": None}


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
        "TimeoutError": TimeoutError,
        "FileNotFoundError": FileNotFoundError,
        "FileExistsError": FileExistsError,
        "PermissionError": PermissionError,
        "ImportError": ImportError,
        "ModuleNotFoundError": ModuleNotFoundError,
        "OverflowError": OverflowError,
        "ZeroDivisionError": ZeroDivisionError,
        "StopIteration": StopIteration,
        "NotImplementedError": NotImplementedError,
        "RecursionError": RecursionError,
        "UnicodeError": UnicodeError,
        "UnicodeDecodeError": UnicodeDecodeError,
        "LookupError": LookupError,
        "NameError": NameError,
        "AssertionError": AssertionError,
        "EOFError": EOFError,
        "MemoryError": MemoryError,
        "ReferenceError": ReferenceError,
        "SystemError": SystemError,
        "SyntaxError": SyntaxError,
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
        "IsADirectoryError": IsADirectoryError,
        "NotADirectoryError": NotADirectoryError,
        "ProcessLookupError": ProcessLookupError,
        "UnboundLocalError": UnboundLocalError,
        "IndentationError": IndentationError,
        "TabError": TabError,
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
        "OperationalError": sqlite3.OperationalError,
        "IntegrityError": sqlite3.IntegrityError,
        "DatabaseError": sqlite3.DatabaseError,
        "HallucinatedCardsError": NS["HallucinatedCardsError"],
        "EmptyCompletionError": NS["EmptyCompletionError"],
        "ArtifactPreservationError": NS["ArtifactPreservationError"],
    }.get(t)
    if cls is KeyError:
        return KeyError(msg)
    if cls is not None:
        if cls in (ExceptionGroup, BaseExceptionGroup):
            return cls(msg, [Exception(msg)])
        return cls(msg)
    # unknown custom kind: a fresh Exception subclass (never catchable by the
    # ported except-Exception handlers unless it IS an Exception subclass)
    return type(t, (Exception,), {})(msg)


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.io = inp.get("io", {}) or {}
        self.cursors = {}
        self.state = {}

    def script_next(self, name):
        entry = self.io.get(name)
        if entry is None:
            return MISSING
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return MISSING
            self.cursors[name] = idx + 1
            return entry[idx]
        return entry

    def rec(self, name, args):
        ev = {"call": name}
        ev.update(args)
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


def _install_imports(ctx, inp):
    """Per-case sys.modules gating for the in-function imports. A None entry
    HALTS the import (ModuleNotFoundError) before any finder runs; the stub
    parent packages carry empty __path__ so the editable-install meta-path
    finder never resolves the REAL submodule."""
    io = inp.get("io", {}) or {}
    for parent in ("hermes_cli", "agent"):
        mod = types.ModuleType(parent)
        mod.__path__ = []
        mod.__ka_stub__ = True
        sys.modules[parent] = mod
    for full, (parent, attr, seam) in IMPORT_CHANNELS.items():
        entry = io.get("import:" + full)
        gated = isinstance(entry, dict) and isinstance(entry.get("import_raise"), dict)
        pmod = sys.modules[parent]
        leaf = full.rsplit(".", 1)[1]
        if gated:
            spec = entry["import_raise"]
            if spec.get("type") == "ModuleNotFoundError":
                # None entry HALTS the import before any finder runs; the
                # parent attr must be absent so `from pkg import leaf` falls
                # through to the dotted import (and its halt message).
                sys.modules[full] = None
                if hasattr(pmod, attr):
                    delattr(pmod, attr)
                if hasattr(pmod, leaf):
                    delattr(pmod, leaf)
            else:
                # a non-halt scripted import raise: PEP 562 module __getattr__
                # raises it at the from-import attribute resolution.
                stub = types.ModuleType(full)
                stub.__ka_stub__ = True

                def mod_getattr(name, _spec=spec):
                    raise make_exc(_spec)

                stub.__getattr__ = mod_getattr
                sys.modules[full] = stub
                if full == "hermes_cli.projects_db":
                    # `from hermes_cli import projects_db` resolves via the
                    # PARENT getattr — raise there (a leaf __getattr__ would
                    # fire too late, inside the try).
                    def parent_getattr(name, _spec=spec, _leaf=leaf):
                        if name == _leaf:
                            raise make_exc(_spec)
                        raise AttributeError(name)

                    pmod.__getattr__ = parent_getattr
                    # pmod is a FRESH stub this case (no leaf attr); do NOT
                    # hasattr() it — that would trigger parent_getattr's raise.
        else:
            stub = types.ModuleType(full)
            stub.__ka_stub__ = True

            def recorder(*a, _seam=seam, **k):
                args = {}
                if _seam == "branch_name_for":
                    args = {"project_obj": a[0] if len(a) > 0 else k.get("project_obj"),
                            "task_id": a[1] if len(a) > 1 else k.get("task_id"),
                            "title": k.get("title")}
                return ctx.rec(_seam, args)

            setattr(stub, attr, recorder)
            sys.modules[full] = stub
            setattr(pmod, attr, recorder)
            leaf = full.rsplit(".", 1)[1]
            setattr(pmod, leaf, stub)


def seed_case(ctx, inp):
    conn = _Conn(ctx)
    unset = NS["_UNSET"]
    NS["write_txn"] = lambda c, allow_nested=False: _WriteTxnCM(ctx, c, allow_nested)
    NS["os"] = types.SimpleNamespace(environ=EnvironDouble(ctx))
    NS["_CURRENT_BOARD_OVERRIDE"] = CtxVarDouble(ctx, "current_board_override.get")
    NS["_PIN_FIRST_BOARD_RESOLUTION"] = CtxVarDouble(ctx, "pin_first.get")

    def normalize_slug(slug):
        return ctx.rec("_normalize_board_slug", {"slug": slug})

    def get_current_board():
        return ctx.rec("get_current_board", {})

    def read_board_metadata(board):
        return ctx.rec("read_board_metadata", {"board": board})

    def claimer_id():
        return ctx.rec("_claimer_id", {})

    def kanban_home():
        return PathDouble(ctx.rec("kanban_home", {}))

    def board_dir(board=None):
        return PathDouble(ctx.rec("board_dir", {"slug": board}))

    def fire_hook(event, task_id, **fields):
        return ctx.rec("_fire_kanban_lifecycle_hook",
                       {"event": event, "task_id": task_id, **fields})

    def end_run(c, task_id, *, outcome, summary=None, error=None, metadata=None, status=None):
        return ctx.rec("_end_run", {
            "task_id": task_id, "outcome": outcome, "status": status,
            "summary": summary, "error": error, "metadata": metadata})

    def synthesize(c, task_id, *, outcome, summary=None, error=None, metadata=None, profile=unset):
        return ctx.rec("_synthesize_ended_run", {
            "task_id": task_id, "outcome": outcome, "summary": summary,
            "error": error, "metadata": metadata,
            "profile": None if profile is unset else profile})

    def verify_cards(c, completing_task_id, claimed_ids):
        return ctx.rec("_verify_created_cards", {
            "task_id": completing_task_id, "created_cards": list(claimed_ids)})

    def append_event(c, task_id, kind, payload=None, *, run_id=None):
        return ctx.rec("_append_event", {
            "task_id": task_id, "kind": kind, "payload": payload, "run_id": run_id})

    def scan_prose(c, text):
        return ctx.rec("_scan_prose_for_phantom_ids", {"text": text})

    def subprocess_run(args, **kwargs):
        ret = ctx.rec("subprocess.run", {"args": list(args), **kwargs})
        return types.SimpleNamespace(**(ret if isinstance(ret, dict) else {}))

    NS["_normalize_board_slug"] = normalize_slug
    NS["get_current_board"] = get_current_board
    NS["read_board_metadata"] = read_board_metadata
    NS["_claimer_id"] = claimer_id
    NS["kanban_home"] = kanban_home
    NS["board_dir"] = board_dir
    NS["_fire_kanban_lifecycle_hook"] = fire_hook
    NS["_end_run"] = end_run
    NS["_synthesize_ended_run"] = synthesize
    NS["_verify_created_cards"] = verify_cards
    NS["_append_event"] = append_event
    NS["_scan_prose_for_phantom_ids"] = scan_prose
    NS["subprocess"] = types.SimpleNamespace(run=subprocess_run)
    _install_imports(ctx, inp)
    return conn


def norm_ret(r):
    if isinstance(r, BaseException):
        return {"__raise__": type(r).__name__, "message": str(r)}
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


def _bytes_double(v):
    if isinstance(v, dict) and set(v.keys()) == {"__bytes__"}:
        return base64.b64decode(v["__bytes__"])
    return v


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    conn = seed_case(ctx, inp)
    out = {"ret": None, "trace": ctx.trace, "state": ctx.state}
    py = PY[func]
    tid = inp.get("task_id", "")
    try:
        if func == "_lossy_text":
            r = NS[py](_bytes_double(inp.get("value")))
        elif func == "_row_get":
            if "default" in inp:
                r = NS[py](inp.get("row"), inp.get("col", ""), inp["default"])
            else:
                r = NS[py](inp.get("row"), inp.get("col", ""))
        elif func == "_json_or":
            if "default" in inp:
                r = NS[py](inp.get("value"), inp["default"])
            else:
                r = NS[py](inp.get("value"))
        elif func == "_json_dict":
            r = NS[py](inp.get("value"))
        elif func == "_env_int":
            kw = {}
            if "minimum" in inp:
                kw["minimum"] = inp["minimum"]
            r = NS[py](inp.get("name", ""), inp.get("default"), **kw)
        elif func == "_first_line":
            r = NS[py](inp.get("text"), inp.get("limit", 0))
        elif func == "_opt_int":
            r = NS[py](inp.get("value"))
        elif func == "_json_or_null":
            r = NS[py](inp.get("obj"))
        elif func == "_substantive_text":
            r = NS[py](inp.get("value"))
        elif func == "_nonblank_str":
            r = NS[py](inp.get("value"))
        elif func == "_cleaned_artifact_paths":
            r = NS[py](inp.get("metadata"))
        elif func == "_retention_seconds":
            r = NS[py](inp.get("older_than_seconds"))
        elif func == "_slug_or_default":
            r = NS[py](inp.get("board"))
        elif func == "_require_slug":
            r = NS[py](inp.get("slug"))
        elif func == "_host_prefix":
            r = NS[py]()
        elif func == "_board_meta_for":
            r = NS[py](inp.get("board"))
        elif func == "_hook_profile_name":
            r = NS[py]()
        elif func == "_fire_task_hook":
            task = inp.get("task")
            if isinstance(task, dict):
                task = types.SimpleNamespace(**task)
            r = NS[py](inp.get("event", ""), task, tid, inp.get("run_id"),
                       **(inp.get("fields") or {}))
        elif func == "_explicit_board_slug":
            r = NS[py](inp.get("board"))
        elif func == "_explicit_board_intent_pinned":
            r = NS[py]()
        elif func == "_board_path":
            r = NS[py](inp.get("env_var"), inp.get("board"),
                       tuple(inp.get("default_parts") or []), inp.get("leaf", ""))
        elif func == "_link":
            r = NS[py](conn, inp.get("parent_id", ""), inp.get("child_id", ""))
        elif func == "_linked_ids":
            r = NS[py](conn, inp.get("want", ""), inp.get("where", ""), tid)
        elif func == "_require_task":
            r = NS[py](conn, tid)
        elif func == "_task_rows":
            r = NS[py](conn, inp.get("table", ""), tid, inp.get("order", ""))
        elif func == "_latest_event":
            r = NS[py](conn, tid, inp.get("kind", ""), run_id=inp.get("run_id"))
        elif func == "_counts_by_assignee":
            r = NS[py](conn)
        elif func == "current_run_started_ats":
            r = NS[py](conn, inp.get("task_ids") or [])
        elif func == "_end_or_synthesize_run":
            kw = {}
            if "profile" in inp:
                kw["profile"] = inp["profile"]
            r = NS[py](conn, tid, outcome=inp.get("outcome", ""), status=inp.get("status", ""),
                       summary=inp.get("summary"), metadata=inp.get("metadata"),
                       synthesize=inp.get("synthesize", False), **kw)
        elif func == "_gate_created_cards":
            r = NS[py](conn, tid, inp.get("created_cards"), inp.get("preview_text"))
        elif func == "_flag_phantom_prose_refs":
            r = NS[py](conn, tid, inp.get("run_id"), inp.get("summary"),
                       inp.get("result"), inp.get("verified_cards") or [])
        elif func == "_git_out":
            r = NS[py](PathDouble(inp.get("cwd", "")), *(inp.get("args") or []),
                       timeout=inp.get("timeout", 30))
        elif func == "_project_branch_name":
            r = NS[py](inp.get("project_obj"), tid, inp.get("title"))
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
    whose message names a ported method or seam stub (a uniform unbound-method
    family reads as a confident wrong ret, not a drive failure)."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    if not FIXTURE.is_file():
        print("FATAL kanban_adapter_flow: fixture missing:", FIXTURE)
        return 2
    funcs = set()
    n = 0
    bad = 0
    ported_names = set(PY.values()) | {
        "_normalize_board_slug", "get_current_board", "read_board_metadata",
        "_claimer_id", "kanban_home", "board_dir", "_fire_kanban_lifecycle_hook",
        "_end_run", "_synthesize_ended_run", "_verify_created_cards",
        "_append_event", "_scan_prose_for_phantom_ids", "get_active_profile_name",
        "explicit_board_intent_is_pinned", "branch_name_for",
    }
    # split on "\n" ONLY: fixture strings carry raw U+0085/2028/2029 (valid
    # JSON, ensure_ascii=False) which splitlines() would treat as boundaries.
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
    print(f"SELFTEST kanban_adapter_flow: {n} cases, {bad} problems, {len(funcs)} funcs")
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
    print(f"ORACLE kanban_adapter_flow: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


NS = extract()

if __name__ == "__main__":
    sys.exit(main())