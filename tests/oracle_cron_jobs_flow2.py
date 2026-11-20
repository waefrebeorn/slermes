#!/usr/bin/env python3
"""Oracle for the cron/jobs.py FLOW2 port (src/cron/cron_jobs_flow2.py).

Drives the LIVE, UNCLAIMED cron/jobs.py defs with SCRIPTED I/O: every
module-level call, claimed sibling and inner-imported dependency is a recording
stub fed the same scripts the C harness feeds its seams. Compares the DECISION
SEQUENCE (ordered seam-call NAMES in "trace") plus the final return ("ret"),
mutated out-params ("updates"/"updated"/"job") and module globals ("state").

Live defs are AST-extracted (module constants seeded, PEP 563 annotation flag)
so the heavy quarry import chain is never imported. Fail-closed: 0 cases => 1.
Summary: 'ORACLE cron_jobs_flow2: N cases, M mismatches'.

Deterministic subset (documented):
 - seam traces are NAME-ONLY; seam results are scripted by in.io.
 - out-params are dicts mirrored from the fixture and echoed in full.
 - the full builtin exception hierarchy is modelled: BaseException-only kinds
   propagate through `except Exception`; OSError family through suppress.
"""
import ast
import copy
import errno
import json
import os
import sys
import types
import decimal
import fractions
from pathlib import Path
from datetime import datetime, timedelta, UTC
from collections import OrderedDict
import contextlib
import threading
import logging
import re as _re
import uuid
import shutil
import io as _io
from contextvars import ContextVar
from dataclasses import dataclass, field
from typing import (Optional, Dict, List, Any, Callable, Set, Tuple, Union,
                    Collection, Iterable, Sequence, Mapping)
import __future__

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stdin, "reconfigure"):
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "cron" / "jobs.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "cron" / "jobs.py")

os.environ.setdefault("HOME", str(_HERE.parents[2]))
os.environ.setdefault("USERPROFILE", str(_HERE.parents[2]))
os.environ.setdefault("HERMES_HOME", str(_HERE.parents[1] / ".testbin" / "dbg_home"))

# C harness func -> LIVE python name
PY = {
    "normalize_job_updates": "_normalize_job_updates",
    "rederive_repeat": "_rederive_repeat_for_schedule_change",
    "apply_schedule_update": "_apply_schedule_update",
    "fill_missing_next_run": "_fill_missing_next_run",
    "cron_config_number": "_cron_config_number",
    "record_run_outcome": "_record_run_outcome",
    "advance_after_run": "_advance_after_run",
    "under_fire_fence": "_under_fire_fence",
}

# Full builtin exception hierarchy (name -> class). BaseException-only kinds
# are handled by Python semantics automatically; we only need the class.
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
    cls = _EXC.get(t)
    if cls is None:
        cls = type(t, (Exception,), {})
    try:
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


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.io = inp.get("io", {}) or {}
        self.cursors = {}
        self.state = copy.deepcopy(inp.get("state", {}) or {})

    def script_next(self, name):
        entry = self.io.get(name)
        if entry is None:
            return None
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
                raise_spec = result.get("raise")
                if isinstance(raise_spec, dict):
                    raise make_exc(raise_spec)
                return result.get("ret")
            return result
        return call


class _FireLockCtx:
    """Context-manager double for _fire_job_lock(job_id)."""
    def __init__(self, ctx):
        self.ctx = ctx
    def __enter__(self):
        return self.ctx.recorder("fire_lock.enter")()
    def __exit__(self, *a):
        self.ctx.recorder("fire_lock.exit")()
        return False


CURRENT = {"rec": None}


def _stub_fn(name):
    def f(*a, **k):
        return CURRENT["rec"](name)(*a, **k)
    return f


def _install_config_stub():
    cfg = sys.modules.setdefault("hermes_cli", types.ModuleType("hermes_cli"))
    cfg.__path__ = []
    sub = types.ModuleType("hermes_cli.config")
    sub.load_config = _stub_fn("load_config")
    sys.modules["hermes_cli.config"] = sub
    cfg.config = sub
    return sub


CONFIG_MOD = _install_config_stub()
_ANN_FLAG = __future__.annotations.compiler_flag

# Normalizer fan-out order, pinned to _UPDATE_FIELD_NORMALIZERS in jobs.py.
NORM_KEYS = [
    "enabled_toolsets", "workdir", "monitor_script", "monitor_url",
    "interpreter", "reasoning_effort", "timezone", "grace_minutes",
    "catch_up", "wrap_response",
]


def extract():
    mod = types.ModuleType("cron.jobs")
    sys.modules["cron.jobs"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "cron.jobs",
        "contextlib": contextlib, "copy": copy, "json": json, "logging": logging,
        "shutil": shutil, "sys": sys, "time": None, "os": os, "re": _re,
        "uuid": uuid, "decimal": decimal, "fractions": fractions, "io": _io,
        "datetime": datetime, "timedelta": timedelta, "UTC": UTC, "Path": Path,
        "OrderedDict": OrderedDict, "ContextVar": ContextVar,
        "dataclass": dataclass, "field": field,
        "Optional": Optional, "Dict": Dict, "List": List, "Any": Any,
        "Callable": Callable, "Set": Set, "Tuple": Tuple, "Union": Union,
        "Collection": Collection, "Iterable": Iterable, "Sequence": Sequence,
        "Mapping": Mapping,
        "fcntl": None, "msvcrt": None, "logger": None,
        "get_hermes_home": lambda: Path(os.environ["HERMES_HOME"]),
        "cron_env_setting": lambda *a, **k: "",
        "ONESHOT_GRACE_SECONDS": 120,
        "STALE_CRON_MATCH": "match",
        "STALE_CRON_TIMEZONE_MIGRATION": "timezone_migration",
        "STALE_CRON_EXPR_EDIT": "expr_edit",
        "jobs_time": types.SimpleNamespace(),
        "jobs_error_text": types.SimpleNamespace(),
        "_normalize_catch_up": lambda v: v,
        "normalize_wrap_response": lambda v: v,
        "_UPDATE_FIELD_NORMALIZERS": {},
        "normalize_repeat_value": lambda v: v,
        "parse_schedule": lambda v: v,
        "_next_run_or_reject_past_oneshot": lambda *a, **k: None,
        "compute_next_run": lambda *a, **k: None,
        "load_config": lambda: {},
        "_complete_job_record": lambda *a, **k: None,
        "_fire_job_lock": lambda jid: _FireLockCtx(CaseCtx({})),
        "croniter": None, "HAS_CRONITER": None,
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
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                             flags=_ANN_FLAG), ns)
            except Exception:
                pass
    missing = [v for v in PY.values() if v not in ns]
    if missing:
        print("FATAL cron_jobs_flow2: extraction missing names:", sorted(missing))
        sys.exit(2)
    return ns


NS = extract()


def seed_case(ctx, inp):
    CURRENT["rec"] = ctx.recorder
    rec = ctx.recorder
    NS["_UPDATE_FIELD_NORMALIZERS"] = {k: rec("norm_" + k) for k in NORM_KEYS}
    NS["normalize_repeat_value"] = rec("normalize_repeat_value")
    NS["parse_schedule"] = rec("parse_schedule")
    NS["_next_run_or_reject_past_oneshot"] = rec("_next_run_or_reject_past_oneshot")
    NS["compute_next_run"] = rec("compute_next_run")
    NS["load_config"] = rec("load_config")
    CONFIG_MOD.load_config = rec("load_config")
    NS["jobs_error_text"] = types.SimpleNamespace(
        sanitize_persisted_error=rec("sanitize_persisted_error"))
    NS["_complete_job_record"] = rec("_complete_job_record")
    NS["logger"] = types.SimpleNamespace(error=rec("logger.error"))
    NS["_fire_job_lock"] = lambda jid: _FireLockCtx(ctx)


def norm_ret(r):
    if isinstance(r, BaseException):
        return {"__raise__": type(r).__name__, "message": str(r)}
    if isinstance(r, tuple):
        return list(r)
    return r


def run_case(func, inp):
    ctx = CaseCtx(inp)
    seed_case(ctx, inp)
    out = {"ret": None, "trace": ctx.trace, "state": ctx.state}
    py = PY[func]
    try:
        if func in ("normalize_job_updates", "rederive_repeat"):
            updates = copy.deepcopy(inp.get("updates") or {})
            out["updates"] = updates
            NS[py](inp.get("job") or {}, updates)
        elif func == "apply_schedule_update":
            updated = copy.deepcopy(inp.get("updated") or {})
            updates = copy.deepcopy(inp.get("updates") or {})
            out["updated"] = updated
            out["updates"] = updates
            NS[py](updated, updates, inp.get("job_id", ""))
        elif func == "fill_missing_next_run":
            updated = copy.deepcopy(inp.get("updated") or {})
            out["updated"] = updated
            NS[py](updated)
        elif func == "cron_config_number":
            out["ret"] = NS[py](inp.get("key", ""), inp.get("default"),
                                ctx.recorder("cast"))
        elif func == "record_run_outcome":
            job = copy.deepcopy(inp.get("job") or {})
            out["job"] = job
            NS[py](job, bool(inp.get("success", False)), inp.get("error"),
                   inp.get("delivery_error"), inp.get("status"), inp.get("now"))
        elif func == "advance_after_run":
            job = copy.deepcopy(inp.get("job") or {})
            out["job"] = job
            NS[py](job, inp.get("now"), ladder_rung=bool(inp.get("ladder_rung", False)))
        elif func == "under_fire_fence":
            def fn():
                return ctx.recorder("fn")()
            out["ret"] = NS[py](inp.get("job_id", ""), fn)
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
        for k in ("updates", "updated", "job"):
            if k in want:
                ok = ok and got.get(k) == want.get(k)
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(got, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE cron_jobs_flow2: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
