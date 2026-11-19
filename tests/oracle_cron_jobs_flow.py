#!/usr/bin/env python3
"""Oracle for the cron/jobs.py FLOW port (src/cron/cron_jobs_flow.c).

Drives the LIVE cron/jobs.py defs with SCRIPTED I/O: every module-level I/O
call, claimed sibling and inner-imported dependency is a recording stub fed
the same scripts the C harness feeds its seams. Compares the DECISION
SEQUENCE (ordered seam-call NAMES in "trace") plus the final return ("ret")
and mutated module globals ("state"), exactly like oracle_scheduler_flow.py.

The live defs are AST-extracted (module constants seeded, PEP 563 annotation
flag so def-exec never evaluates annotations) so the heavy quarry import
chain is never imported. Ported siblings (_ensure_cron_dir, _read_marker_fields,
_unlink_quiet, _append_telemetry_record) stay REAL on both sides; claimed
siblings (_current_cron_store, load_jobs, save_jobs, compute_next_run,
_ensure_croniter, _jobs_file_stamp, _peek_jobs_unlocked, _main_model_pin,
_normalize_job_optional_text, _is_named_profile_path, ensure_dirs) are seams.
poll_until is extracted REAL from agent/deadline.py (its time module is the
scripted clock double). Fail-closed: 0 cases => exit 1.
Summary line: 'ORACLE cron_jobs_flow: N cases, M mismatches'.

Deterministic subset (documented):
 - datetimes are ISO strings; .isoformat() is identity (_DT double).
 - paths are POSIX strings (PurePosixPath doubles); Path `/` and `.parent`
   are silent internal operations, mkdir/read_text are seams.
 - lock_fd is an int fd double: fileno() identity, close() the "close" seam.
 - json.dump is ONE "json_dump" seam (the file double's write is silent);
   json.dumps stays real but write text is not trace-compared (name-only).
 - _jobs_lock is jobs_lock.enter/exit; _with_job's callback is the "fn" seam.
 - BaseException-only kinds propagate through except-Exception sites.
 - str.format of CONTAINER values (dict repr) is excluded from fixtures.
"""
import ast
import copy
import errno
import json
import os
import sys
import types
import contextlib
import threading
import uuid
import re as _re
import shutil
import logging
from datetime import datetime, timedelta, UTC
from pathlib import Path, PurePosixPath
from contextvars import ContextVar
from dataclasses import dataclass, field
from typing import Optional, Dict, List, Any, Callable, Set, Tuple, Union, Collection
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
DEADLINE_SRC = str(QUARRY / "agent" / "deadline.py")

os.environ.setdefault("HOME", str(_HERE.parents[2]))
os.environ.setdefault("USERPROFILE", str(_HERE.parents[2]))
os.environ.setdefault("LOCALAPPDATA", str(_HERE.parents[2] / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_HERE.parents[1] / ".testbin" / "dbg_home"))

# C harness func name -> LIVE python name.
PY = {
    "self_removal_delivery_allowed": "self_removal_delivery_allowed",
    "acquire_flock": "_acquire_flock",
    "release_flock": "_release_flock",
    "ensure_cron_dir": "_ensure_cron_dir",
    "cron_schedule": "_cron_schedule",
    "parse_aware": "_parse_aware",
    "append_telemetry_record": "_append_telemetry_record",
    "record_timezone_migration_catchup": "_record_timezone_migration_catchup",
    "write_marker": "_write_marker",
    "read_marker_fields": "_read_marker_fields",
    "ticker_heartbeat_writer_alive": "ticker_heartbeat_writer_alive",
    "unmerged_disk_jobs": "_unmerged_disk_jobs",
    "unlink_quiet": "_unlink_quiet",
    "stage_jobs_payload": "_stage_jobs_payload",
    "with_job": "_with_job",
    "next_run_or_reject_past_oneshot": "_next_run_or_reject_past_oneshot",
    "apply_pin_update": "_apply_pin_update",
    "refresh_claim": "_refresh_claim",
}

# Module-global seams installed as plain recorders (object doubles seeded
# AFTER this loop so they are not overwritten).
IO_NAMES = {
    "load_jobs", "save_jobs", "_ensure_croniter", "croniter", "compute_next_run",
    "_is_named_profile_path", "ensure_dirs", "atomic_write_text",
    "_peek_jobs_unlocked", "_normalize_job_optional_text", "_ensure_aware",
}
# Seams whose scripted result needs a materialized double / tuple.
MAT_NAMES = {
    "_hermes_now", "_current_cron_store", "_jobs_file_stamp",
    "mkstemp_beside", "_main_model_pin",
}


def make_exc(spec):
    t = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    en = spec.get("errno")
    cls = {
        "OSError": OSError, "ValueError": ValueError, "TypeError": TypeError,
        "RuntimeError": RuntimeError, "KeyError": KeyError,
        "AttributeError": AttributeError, "FileNotFoundError": FileNotFoundError,
        "PermissionError": PermissionError, "ImportError": ImportError,
        "TimeoutError": TimeoutError, "SystemExit": SystemExit,
        "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    }.get(t)
    if cls is None:
        cls = type(t, (Exception,), {})
    e = cls(msg)
    if en and isinstance(e, OSError):
        e.errno = getattr(errno, en, None)
    return e


class _DT:
    """Datetime double: an ISO string whose .isoformat() is identity."""
    def __init__(self, s):
        self._s = s
    def isoformat(self):
        return self._s


class _PathDouble:
    def __init__(self, ctx, p):
        self._ctx = ctx
        self._p = PurePosixPath(str(p))
    def __truediv__(self, other):
        return _PathDouble(self._ctx, self._p / str(other))
    @property
    def parent(self):
        return _PathDouble(self._ctx, self._p.parent)
    def mkdir(self, parents=False, exist_ok=False):
        return self._ctx.recorder("mkdir")(str(self._p), parents=parents, exist_ok=exist_ok)
    def read_text(self, encoding=None):
        r = self._ctx.recorder("read_text")(str(self._p), encoding=encoding)
        if isinstance(r, str) and r.startswith("\ufeff"):
            r = r[1:]  # utf-8-sig strips a leading BOM
        return r
    def __str__(self):
        return str(self._p)
    def __fspath__(self):
        return str(self._p)


def _store_double(ctx, r):
    return types.SimpleNamespace(
        cron_dir=_PathDouble(ctx, r.get("cron_dir", "")),
        jobs_file=_PathDouble(ctx, r.get("jobs_file", "")),
        output_dir=_PathDouble(ctx, r.get("output_dir", "")))


class _FdDouble:
    def __init__(self, ctx, fd):
        self._ctx = ctx
        self._fd = fd
    def fileno(self):
        return self._fd
    def close(self):
        return self._ctx.recorder("close")(self._fd)


class _LockCtx:
    def __init__(self, ctx):
        self._ctx = ctx
    def __enter__(self):
        self._ctx.recorder("jobs_lock.enter")()
        return self
    def __exit__(self, *a):
        self._ctx.recorder("jobs_lock.exit")()
        return False


class _AppendFile:
    def __init__(self, ctx):
        self._ctx = ctx
    def __enter__(self):
        return self
    def write(self, text):
        return self._ctx.recorder("write")(text)
    def __exit__(self, *a):
        self._ctx.recorder("close")()
        return False


class _StageFile:
    def __init__(self, ctx, fd):
        self._ctx = ctx
        self._fd = fd
    def __enter__(self):
        return self
    def write(self, text):
        return None  # json.dump is seamed; chunked writes are not modeled
    def flush(self):
        return self._ctx.recorder("flush")()
    def fileno(self):
        return self._ctx.recorder("fileno")()
    def __exit__(self, *a):
        self._ctx.recorder("close")()
        return False


class _Clock:
    def __init__(self, ctx):
        self._ctx = ctx
    def monotonic(self):
        return self._ctx.recorder("time.monotonic")()
    def time(self):
        return self._ctx.recorder("time.time")()
    def sleep(self, s):
        return self._ctx.recorder("time.sleep")(s)


class _Logger:
    def __init__(self, ctx):
        self._ctx = ctx
    def __getattr__(self, level):
        return self._ctx.recorder("logger." + level)


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

    def materialize(self, name, ret):
        if name == "_hermes_now":
            return _DT(ret) if isinstance(ret, str) else ret
        if name == "_current_cron_store":
            return _store_double(self, ret) if isinstance(ret, dict) else ret
        if name in ("_jobs_file_stamp", "mkstemp_beside", "_main_model_pin"):
            return tuple(ret) if isinstance(ret, list) else ret
        return ret

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
                ret = result.get("ret")
            else:
                ret = result
            return ctx.materialize(name, ret)
        return call


CURRENT = {"rec": None}


def _stub_fn(name):
    def f(*a, **k):
        return CURRENT["rec"](name)(*a, **k)
    return f


def install_module_stubs():
    hcli = sys.modules.setdefault("hermes_cli", types.ModuleType("hermes_cli"))
    hcli.__path__ = []
    sub = types.ModuleType("hermes_cli._subprocess_compat")
    sub.pid_exists_stdlib = _stub_fn("pid_exists_stdlib")
    sys.modules["hermes_cli._subprocess_compat"] = sub
    hcli._subprocess_compat = sub
    return sub


SUBPROC_MOD = install_module_stubs()
_ANN_FLAG = __future__.annotations.compiler_flag


def extract():
    mod = types.ModuleType("cron.jobs")
    sys.modules["cron.jobs"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "cron.jobs",
        "contextlib": contextlib, "copy": copy, "json": json, "logging": logging,
        "shutil": shutil, "sys": sys, "threading": threading, "time": None,
        "os": os, "re": _re, "uuid": uuid,
        "datetime": datetime, "timedelta": timedelta, "UTC": UTC, "Path": Path,
        "ContextVar": ContextVar, "dataclass": dataclass, "field": field,
        "Optional": Optional, "Dict": Dict, "List": List, "Any": Any,
        "Callable": Callable, "Set": Set, "Tuple": Tuple, "Union": Union,
        "Collection": Collection,
        "fcntl": None, "msvcrt": None, "logger": None,
        # module-level imports the ported defs never call at runtime:
        "get_hermes_home": lambda: Path(os.environ["HERMES_HOME"]),
        "poll_until": None,
        "cron_env_setting": lambda *a, **k: "",
        "CLAIM_TTL_INACTIVITY_HEADROOM": 3.0,
        "FIRE_CLAIM_SKEW_SECONDS": 120,
        "FIRE_CLAIM_TTL_SECONDS": 900,
        "_validate_job_mode_invariants": lambda *a, **k: None,
        "validate_schedule_knobs": lambda *a, **k: None,
        "_normalize_catch_up": lambda v: v,
        "normalize_wrap_response": lambda v: v,
        "_main_model_pin": lambda: (None, None),
        "_normalize_base_url": lambda v: v,
        "_normalize_context_from": lambda v: v,
        "_normalize_failure_deliver": lambda v: v,
        "_normalize_job_optional_text": lambda v, **k: v,
        "jobs_time": types.SimpleNamespace(),
        "_claim_owner_is_dead": lambda *a, **k: False,
        "extra_fire": types.SimpleNamespace(),
        "jobs_error_text": types.SimpleNamespace(),
        "store_health": types.SimpleNamespace(),
        "_hermes_now": lambda: None,
        "get_timezone": lambda: None,
        "record_cron_missed": lambda *a, **k: None,
        "atomic_replace": lambda *a, **k: None,
        "atomic_write_text": lambda *a, **k: None,
        "fsync_directory": lambda *a, **k: None,
        "mkstemp_beside": lambda *a, **k: (0, ""),
        "croniter": None, "HAS_CRONITER": None,
    })
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # small record classes first (constants construct them)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in ("_CronStorePaths", "_SelfRemovalDelivery"):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                             flags=_ANN_FLAG), ns)
            except Exception:
                pass
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
    # poll_until (agent/deadline.py) — REAL, its `time` global is the double.
    dsrc = open(DEADLINE_SRC, encoding="utf-8").read()
    for node in ast.parse(dsrc).body:
        if isinstance(node, ast.FunctionDef) and node.name == "poll_until":
            exec(compile(ast.Module(body=[node], type_ignores=[]), DEADLINE_SRC, "exec",
                         flags=_ANN_FLAG), ns)
    missing = [v for v in PY.values() if v not in ns]
    if missing or ns.get("poll_until") is None:
        print("FATAL cron_jobs_flow: extraction missing names:",
              sorted(missing + (["poll_until"] if ns.get("poll_until") is None else [])))
        sys.exit(2)
    return ns


NS = extract()


def seed_case(ctx, inp):
    CURRENT["rec"] = ctx.recorder
    NS["time"] = _Clock(ctx)
    NS["logger"] = _Logger(ctx)
    NS["datetime"] = types.SimpleNamespace(fromisoformat=ctx.recorder("datetime.fromisoformat"))
    NS["json"] = types.SimpleNamespace(dumps=json.dumps, loads=json.loads,
                                       dump=ctx.recorder("json_dump"))
    plat = ctx.state.get("platform", "fcntl")
    if plat == "fcntl":
        NS["fcntl"] = types.SimpleNamespace(flock=ctx.recorder("fcntl.flock"),
                                            LOCK_EX=2, LOCK_NB=4, LOCK_UN=8)
        NS["msvcrt"] = None
    elif plat == "msvcrt":
        NS["fcntl"] = None
        NS["msvcrt"] = types.SimpleNamespace(locking=ctx.recorder("msvcrt.locking"),
                                             LK_LOCK=1, LK_UNLCK=0)
    else:
        NS["fcntl"] = None
        NS["msvcrt"] = None
    for nm in IO_NAMES:
        NS[nm] = ctx.recorder(nm)
    for nm in MAT_NAMES:
        NS[nm] = ctx.recorder(nm)
    # object doubles AFTER the generic recorder loops
    def _open(*a, **k):
        ctx.recorder("open")(*a, **k)  # may raise; records the seam
        return _AppendFile(ctx)
    NS["open"] = _open
    def _fdopen(fd, *a, **k):
        ctx.recorder("fdopen")(fd, *a, **k)
        return _StageFile(ctx, fd)
    NS["os"] = types.SimpleNamespace(fdopen=_fdopen, fsync=ctx.recorder("fsync"),
                                     unlink=ctx.recorder("os.unlink"))
    NS["_jobs_lock"] = lambda: _LockCtx(ctx)
    marker = ctx.state.get("self_removal_marker")
    mdouble = types.SimpleNamespace(job_id=marker.get("job_id", ""),
                                    removed=bool(marker.get("removed"))) if isinstance(marker, dict) else None
    NS["_self_removal_delivery"] = types.SimpleNamespace(get=lambda: mdouble)
    stamp = ctx.state.get("load_stamp")
    NS["_jobs_lock_state"] = types.SimpleNamespace(
        load_stamp=tuple(stamp) if isinstance(stamp, list) else None)
    NS["_timezone_migration_catchups"] = ctx.state.get("_timezone_migration_catchups", 0)
    NS["_timezone_migration_catchups_recent"] = ctx.state.setdefault("_timezone_migration_catchups_recent", [])
    SUBPROC_MOD.pid_exists_stdlib = ctx.recorder("pid_exists_stdlib")


def sync_state(ctx, func):
    if func == "record_timezone_migration_catchup":
        ctx.state["_timezone_migration_catchups"] = NS["_timezone_migration_catchups"]


def norm_ret(r):
    if isinstance(r, BaseException):
        return {"__raise__": type(r).__name__, "message": str(r)}
    if isinstance(r, tuple):
        return list(r)
    if isinstance(r, _DT):
        return r.isoformat()
    if isinstance(r, frozenset):
        return sorted(r)
    return r


def run_case(func, inp):
    ctx = CaseCtx(inp)
    seed_case(ctx, inp)
    out = {"ret": None, "trace": ctx.trace, "state": ctx.state}
    py = PY[func]
    try:
        if func == "self_removal_delivery_allowed":
            out["ret"] = NS[py](inp.get("job_id", ""))
        elif func == "acquire_flock":
            out["ret"] = NS[py](_FdDouble(ctx, inp.get("lock_fd", 0)), inp.get("timeout", 30.0))
        elif func == "release_flock":
            NS[py](_FdDouble(ctx, inp.get("lock_fd", 0)))
            out["ret"] = None
        elif func == "ensure_cron_dir":
            NS[py](_PathDouble(ctx, inp.get("cron_dir", "")))
            out["ret"] = None
        elif func == "cron_schedule":
            out["ret"] = NS[py](inp.get("expr", ""), inp.get("display", ""),
                                inp.get("missing_croniter", ""), inp.get("invalid_label", ""))
        elif func == "parse_aware":
            out["ret"] = NS[py](inp.get("value"), inp.get("zone"))
        elif func == "append_telemetry_record":
            recent = ctx.state.setdefault("recent", [])
            NS[py](inp.get("filename", "t.jsonl"), inp.get("entry") or {}, recent)
            out["ret"] = None
        elif func == "record_timezone_migration_catchup":
            NS[py](inp.get("job") or {}, _DT(inp.get("raw_next_run_at", "")),
                   _DT(inp.get("normalized_next_run_at", "")))
            out["ret"] = None
        elif func == "write_marker":
            NS[py](inp.get("name", ""), inp.get("text", ""), inp.get("tmp_prefix", ""))
            out["ret"] = None
        elif func == "read_marker_fields":
            out["ret"] = NS[py](inp.get("name", ""))
        elif func == "ticker_heartbeat_writer_alive":
            out["ret"] = NS[py]()
        elif func == "unmerged_disk_jobs":
            out["ret"] = NS[py](inp.get("jobs") or [], inp.get("removed_ids"))
        elif func == "unlink_quiet":
            NS[py](inp.get("path"))
            out["ret"] = None
        elif func == "stage_jobs_payload":
            out["ret"] = NS[py](_PathDouble(ctx, inp.get("jobs_file", "")), inp.get("jobs") or [])
        elif func == "with_job":
            def fn(jobs, i, job):
                return ctx.recorder("fn")(jobs, i, job)
            out["ret"] = NS[py](inp.get("job_id"), fn, inp.get("missing"))
        elif func == "next_run_or_reject_past_oneshot":
            out["ret"] = NS[py](inp.get("parsed_schedule") or {}, inp.get("label", ""),
                                inp.get("fallback_run_at"), inp.get("what", ""),
                                timezone=inp.get("timezone"))
        elif func == "apply_pin_update":
            updates = copy.deepcopy(inp.get("updates") or {})
            out["updates"] = updates  # mutated in place; echo even on raise
            NS[py](inp.get("job") or {}, updates)
            out["ret"] = None
        elif func == "refresh_claim":
            claim = copy.deepcopy(inp.get("claim"))
            jobs = copy.deepcopy(inp.get("jobs")) if "jobs" in inp else []
            if "claim" in inp:
                out["claim"] = claim  # mutated in place; echo even on raise
            if "jobs" in inp:
                out["jobs"] = jobs
            out["ret"] = NS[py](jobs, claim, inp.get("expected_owner", ""))
        else:
            raise KeyError(func)
    except BaseException as e:
        out["ret"] = {"__raise__": type(e).__name__, "message": str(e)}
    sync_state(ctx, func)
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
        for k in ("updates", "claim", "jobs"):
            if k in want:
                ok = ok and got.get(k) == want.get(k)
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(got, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE cron_jobs_flow: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())