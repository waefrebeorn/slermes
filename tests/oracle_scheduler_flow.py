#!/usr/bin/env python3
"""Oracle for the cron/scheduler.py FLOW-core port (src/cron/port_scheduler_flow.c).

Drives the LIVE cron/scheduler.py defs with SCRIPTED I/O: every module-level I/O
call and inner-imported dependency is a recording stub fed the same scripts the C
harness feeds its seams. Compares the DECISION SEQUENCE (ordered seam-call names in
"trace") plus the final return ("ret") and mutated module globals ("state").

The live defs are AST-extracted (module constants seeded) so the heavy quarry import
chain is never imported; inner imports are satisfied with sys.modules stubs whose
functions delegate to the current case's recorder. Pure internal helpers stay REAL
(bound from the extraction) so nested decision logic is exercised, exactly matching
the C port which calls its own ported siblings. Fail-closed: 0 cases => exit 1.
Summary line: 'ORACLE scheduler_flow: N cases, M mismatches'.

Deterministic subset (documented): ISO timestamps are timezone-aware; errnos are
carried as symbolic names mapped through the live `errno` module; logger calls are
recorded as logger.<level> tokens on both sides.
"""
import ast
import copy
import errno
import json
import os
import sys
import types
from datetime import datetime, timezone, UTC
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "cron" / "scheduler.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "cron" / "scheduler.py")
JOBS_SRC = str(QUARRY / "cron" / "jobs.py")

# C harness func name -> LIVE python name (module-level unless noted).
PY = {
    "job_route_pinned": "_job_route_pinned",
    "normalize_newlines": "_normalize_newlines",
    "cron_failure_marker_error": "_cron_failure_marker_error",
    "apply_agent_failure_marker": "_apply_agent_failure_marker",
    "is_lock_contention_errno": "_is_lock_contention_errno",
    "is_fd_exhaustion_text": "_is_fd_exhaustion_text",
    "is_fd_exhaustion": "_is_fd_exhaustion",
    "stale_code_yield_labels": "stale_code_yield_labels",
    "inflight_key": "_inflight_key",
    "is_job_running": "is_job_running",
    "job_interval_minutes": "_job_interval_minutes",
    "failure_repeat_alert_hours": "_failure_repeat_alert_hours",
    "repeat_alert_withheld": "_repeat_alert_withheld",
    "failure_streak_nudge": "_failure_streak_nudge",
    "inflight_min_allowance_minutes": "_inflight_min_allowance_minutes",
    "cron_cleanup_timeout_seconds": "_cron_cleanup_timeout_seconds",
    "resolve_job_workdir": "_resolve_job_workdir",
    "resolve_job_reasoning_config": "_resolve_job_reasoning_config",
    "acquire_tick_lock": "_acquire_tick_lock",
    "release_tick_lock": "_release_tick_lock",
    "maybe_reap_dead_owners": "_maybe_reap_dead_owners",
    "sweep_stale_inflight_for_tick": "_sweep_stale_inflight_for_tick",
    "resolve_max_parallel_workers": "_resolve_max_parallel_workers",
    "sweep_mcp_orphans": "_sweep_mcp_orphans",
    "sweep_mcp_orphans_when_all_done": "_sweep_mcp_orphans_when_all_done",
    "process_due_job": "_process_due_job",
    "record_fire_ownership_lost": "_record_fire_ownership_lost",
    "fire_ownership_lost": "_FireOwnership.lost",
    "mark_incident_alerted": "_mark_incident_alerted",
    "resolve_incidents_for_recovered_job": "_resolve_incidents_for_recovered_job",
    "upsert_incident_for_failure": "_upsert_incident_for_failure",
    "record_stale_release": "_record_stale_release",
    "discard_parallel_pools": "discard_parallel_pools",
    "cron_run_scope_enter": "_CronRunScope.enter",
    "cron_run_scope_exit": "_CronRunScope.exit",
    "run_with_fire_claim_heartbeat": "_run_with_fire_claim_heartbeat",
    "compose_run_delivery": "_compose_run_delivery",
    "deliver_crash_failure": "_deliver_crash_failure",
}

# scheduler.py defs that the C port calls as SEAMS (I/O siblings) -> recorder, not real.
SEAM_DEFS = {
    "_get_hermes_home", "sweep_stale_inflight", "run_one_job",
    "_record_forced_release", "_summarize_cron_failure_for_delivery",
    "_interpreter_shutting_down",
}

# External I/O names recorded as seams (module globals or inner imports).
IO_NAMES = {
    "open", "close", "hermes_home_key", "cron_env_setting", "load_config",
    "parse_schedule", "_cron_interval_minutes", "parse_reasoning_effort",
    "resolve_reasoning_config", "_cron_config_number", "_hermes_now",
    "claim_job_for_fire", "note_unwritable", "settle_unstarted_execution",
    "finish_execution", "heartbeat_fire_claim", "is_retry_run", "mark_job_run",
    "finish_lost_execution", "self_removal_delivery_allowed", "_start_heartbeat_thread",
    "upsert_incident", "get_incident", "set_incident_state",
    "close_incidents_for_recovered_job", "recover_interrupted_executions", "load_jobs",
    "_kill_orphaned_mcp_children", "_delivery_lane_value", "_normalize_deliver_value",
    "_deliver_result", "_resolve_delivery_targets", "_classify_delivery_outcome",
    "hold_notice", "blocked_config_notice", "generic_failure_notice",
    "uuid_hex6", "run",
}


def make_exc(spec):
    t = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    en = spec.get("errno")
    if t == "OSError":
        e = OSError(msg)
        if en:
            e.errno = getattr(errno, en, None)
    elif t == "ValueError":
        e = ValueError(msg)
    elif t == "TypeError":
        e = TypeError(msg)
    elif t == "TimeoutError":
        e = TimeoutError(msg)
    elif t == "RuntimeError":
        e = RuntimeError(msg)
    else:
        e = Exception(msg)
    return e


class _LockFile:
    def __init__(self, ctx, fd):
        self._ctx = ctx
        self._fd = fd
    def fileno(self):
        return self._fd
    def close(self):
        return self._ctx.recorder("close")(self._fd)


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


class _Path:
    """Path stub: is_dir records a seam; str() yields the raw path."""
    def __init__(self, ctx, p):
        self._ctx = ctx
        self._p = str(p)
    def is_dir(self):
        return self._ctx.recorder("is_dir")(self._p)
    def __str__(self):
        return self._p
    def __fspath__(self):
        return self._p


class _Event:
    def __init__(self, ctx, name):
        self._ctx = ctx
        self._name = name
        self._set = False
    def is_set(self):
        return self._set
    def set(self):
        self._set = True
        self._ctx.recorder(self._name + ".set")()
    def wait(self, timeout=None):
        return self._ctx.recorder(self._name + ".wait")(timeout)


class _Thread:
    def __init__(self, ctx):
        self._ctx = ctx
    def join(self, timeout=None):
        self._ctx.recorder("thread.join")(timeout)


class _Pool:
    def __init__(self, ctx, key):
        self._ctx = ctx
        self._key = key
    def shutdown(self, wait=True, cancel_futures=False):
        self._ctx.recorder("pool.shutdown")(self._key, wait, cancel_futures)


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
                ret = result.get("ret")
            else:
                ret = result
            if name == "_hermes_now" and isinstance(ret, str):
                return datetime.fromisoformat(ret)
            return ret
        return call


CURRENT = {"rec": None}


def _stub_fn(name):
    def f(*a, **k):
        return CURRENT["rec"](name)(*a, **k)
    return f


def install_module_stubs():
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m
    cron = sys.modules.setdefault("cron", types.ModuleType("cron"))
    cron.__path__ = []
    mod("cron.executions", recover_interrupted_executions=_stub_fn("recover_interrupted_executions"),
        settle_unstarted_execution=_stub_fn("settle_unstarted_execution"),
        finish_execution=_stub_fn("finish_execution"),
        create_execution=_stub_fn("create_execution"))
    # cron.jobs: pure helpers stay REAL (bound later), I/O names are recorders.
    jobs = mod("cron.jobs", load_jobs=_stub_fn("load_jobs"),
               parse_schedule=_stub_fn("parse_schedule"),
               _cron_interval_minutes=_stub_fn("_cron_interval_minutes"),
               _cron_config_number=_stub_fn("_cron_config_number"),
               claim_job_for_fire=_stub_fn("claim_job_for_fire"),
               mark_job_run=_stub_fn("mark_job_run"),
               save_job_output=_stub_fn("save_job_output"),
               self_removal_delivery_allowed=_stub_fn("self_removal_delivery_allowed"),
               heartbeat_fire_claim=_stub_fn("heartbeat_fire_claim"),
               fire_claim_fence=_stub_fn("fire_claim_fence"),
               clear_run_claim=_stub_fn("clear_run_claim"))
    mod("cron.incidents", upsert_incident=_stub_fn("upsert_incident"),
        get_incident=_stub_fn("get_incident"),
        set_incident_state=_stub_fn("set_incident_state"),
        close_incidents_for_recovered_job=_stub_fn("close_incidents_for_recovered_job"))
    mod("cron.unreachable_retry", is_retry_run=_stub_fn("is_retry_run"))
    mod("cron.scheduler_failure_copy", blocked_config_notice=_stub_fn("blocked_config_notice"),
        generic_failure_notice=_stub_fn("generic_failure_notice"))
    mod("cron.quota_hold", hold_notice=_stub_fn("hold_notice"))
    sh = mod("cron.store_health", note_unwritable=_stub_fn("note_unwritable"))
    sh.UNWRITABLE_ERRNOS = frozenset({errno.EROFS, errno.EACCES, errno.EPERM, errno.ENOSPC, errno.EDQUOT})
    mod("cron.scheduler_script", _start_heartbeat_thread=_stub_fn("_start_heartbeat_thread"))
    tools = sys.modules.setdefault("tools", types.ModuleType("tools"))
    tools.__path__ = []
    mod("tools.mcp_tool_lifecycle", _kill_orphaned_mcp_children=_stub_fn("_kill_orphaned_mcp_children"))
    hc = mod("hermes_constants", parse_reasoning_effort=_stub_fn("parse_reasoning_effort"),
             resolve_reasoning_config=_stub_fn("resolve_reasoning_config"),
             hermes_home_key=_stub_fn("hermes_home_key"))
    return jobs, hc


JOBS_MOD, HC_MOD = install_module_stubs()


def extract():
    import re as _re
    import contextlib as _contextlib
    import threading as _threading
    import concurrent.futures as _futures
    ns = {
        "__name__": "cron.scheduler",
        "re": _re, "os": os, "json": json, "sys": sys, "errno": errno,
        "contextlib": _contextlib, "threading": _threading,
        "concurrent": types.SimpleNamespace(futures=_futures),
        "datetime": datetime, "timezone": timezone, "UTC": UTC, "Path": Path,
        "Optional": object, "Any": object,
        "logger": None, "time": None,
        "fcntl": None, "msvcrt": None,
    }
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if not isinstance(tgt, ast.Name):
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    # class methods needed as targets / helpers
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in ("_FireOwnership", "_CronRunScope", "_CombinedCancelEvent"):
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef):
                    sub.decorator_list = []
                    try:
                        exec(compile(ast.Module(body=[sub], type_ignores=[]), SRC, "exec"), ns)
                    except Exception:
                        pass
    # pure helpers from cron/jobs.py used by _repeat_alert_withheld
    jsrc = open(JOBS_SRC, encoding="utf-8").read()
    jtree = ast.parse(jsrc)
    for node in jtree.body:
        if isinstance(node, ast.FunctionDef) and node.name in ("_elapsed_seconds", "_ensure_aware", "_hermes_now"):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), JOBS_SRC, "exec"), ns)
            except Exception:
                pass
    # bind the real pure jobs helpers onto the cron.jobs stub module
    for nm in ("_elapsed_seconds", "_ensure_aware"):
        if nm in ns:
            setattr(JOBS_MOD, nm, ns[nm])
    missing = [v for v in PY.values() if "." not in v and v not in ns]
    if missing:
        print("FATAL scheduler_flow: extraction missing names:", sorted(missing))
        sys.exit(2)
    return ns


NS = extract()


def seed_case(ctx, inp):
    NS["time"] = _Clock(ctx)
    NS["logger"] = _Logger(ctx)
    NS["Path"] = lambda p: _Path(ctx, p)
    CURRENT["rec"] = ctx.recorder
    # platform modules for the tick lock
    plat = inp.get("platform") or ctx.state.get("platform", "fcntl")
    if plat == "fcntl":
        NS["fcntl"] = types.SimpleNamespace(
            flock=ctx.recorder("fcntl.flock"), LOCK_EX=2, LOCK_NB=4, LOCK_UN=8)
        NS["msvcrt"] = None
    elif plat == "msvcrt":
        NS["fcntl"] = None
        NS["msvcrt"] = types.SimpleNamespace(
            locking=ctx.recorder("msvcrt.locking"), LK_NBLCK=1, LK_UNLCK=0)
    else:
        NS["fcntl"] = None
        NS["msvcrt"] = None
    NS["open"] = lambda *a, **k: _LockFile(ctx, ctx.recorder("open")(*a, **k))
    # module globals mirroring state
    NS["_running_job_ids"] = {tuple(k) for k in ctx.state.get("_running_job_ids", [])}
    NS["_running_fire_owners"] = {tuple(k): {} for k in ctx.state.get("_running_fire_owners", [])}
    NS["_last_dead_owner_reap_at"] = copy.deepcopy(ctx.state.get("_last_dead_owner_reap_at", {}))
    pool_markers = ctx.state.get("_parallel_pools", {})
    NS["_parallel_pools"] = {k: _Pool(ctx, k) for k in pool_markers}
    NS["_parallel_pool_max_workers"] = copy.deepcopy(ctx.state.get("_parallel_pool_max_workers", {}))
    # external I/O recorders
    for nm in IO_NAMES:
        NS[nm] = ctx.recorder(nm)
    for nm in SEAM_DEFS:
        NS[nm] = ctx.recorder(nm)
    NS["hermes_home_key"] = ctx.recorder("hermes_home_key")
    HC_MOD.hermes_home_key = ctx.recorder("hermes_home_key")
    NS["_get_hermes_home"] = ctx.recorder("_get_hermes_home")
    # store_health module global
    NS["store_health"] = sys.modules["cron.store_health"]
    NS["load_config"] = ctx.recorder("load_config")
    NS["cron_env_setting"] = ctx.recorder("cron_env_setting")


def sync_state(ctx, func):
    # Only mirror back the globals a given flow actually mutates, so read-only
    # cases echo the input state verbatim (matching the C harness io.state).
    if func == "maybe_reap_dead_owners":
        ctx.state["_last_dead_owner_reap_at"] = NS.get("_last_dead_owner_reap_at", {})
    elif func == "discard_parallel_pools":
        if "_parallel_pools" in ctx.state:
            markers = ctx.inp.get("state", {}).get("_parallel_pools", {})
            ctx.state["_parallel_pools"] = {k: markers.get(k, 1) for k in NS.get("_parallel_pools", {})}
        if "_parallel_pool_max_workers" in ctx.state:
            ctx.state["_parallel_pool_max_workers"] = NS.get("_parallel_pool_max_workers", {})


def norm_ret(r):
    if isinstance(r, BaseException):
        return {"__raise__": type(r).__name__, "message": str(r)}
    if isinstance(r, tuple):
        return list(r)
    if isinstance(r, frozenset):
        return sorted(r)
    return r


def entry_proj(e):
    if e is None:
        return None
    return {"id": getattr(e, "id", ""), "last_status": getattr(e, "last_status", None)}


def run_case(func, inp):
    ctx = CaseCtx(inp)
    seed_case(ctx, inp)
    out = {"ret": None, "trace": ctx.trace, "state": ctx.state}
    py = PY[func]
    job = inp.get("job") or {}

    if func == "job_route_pinned":
        out["ret"] = NS[py](job)
    elif func == "normalize_newlines":
        out["ret"] = NS[py](inp.get("text", ""))
    elif func == "cron_failure_marker_error":
        out["ret"] = NS[py](inp.get("text", ""))
    elif func == "apply_agent_failure_marker":
        out["ret"] = list(NS[py](job, inp.get("success", False), inp.get("error"), inp.get("final_response", "")))
    elif func == "is_lock_contention_errno":
        en = inp.get("errno", "")
        e = OSError("x")
        e.errno = getattr(errno, en, None) if en else None
        out["ret"] = NS[py](e)
    elif func == "is_fd_exhaustion_text":
        out["ret"] = NS[py](inp.get("text", ""))
    elif func == "is_fd_exhaustion":
        out["ret"] = NS[py](make_exc(inp.get("exc") or {}))
    elif func == "stale_code_yield_labels":
        r = NS[py](inp.get("recorded_error", ""))
        out["ret"] = list(r) if r is not None else None
    elif func == "inflight_key":
        home = inp.get("home")
        r = NS[py](inp.get("job_id", ""), home)
        out["ret"] = list(r)
    elif func == "is_job_running":
        out["ret"] = NS[py](inp.get("job_id", ""), inp.get("home"))
    elif func == "job_interval_minutes":
        out["ret"] = NS[py](job)
    elif func == "failure_repeat_alert_hours":
        out["ret"] = NS[py]()
    elif func == "repeat_alert_withheld":
        out["ret"] = NS[py](inp.get("incident") or {})
    elif func == "failure_streak_nudge":
        out["ret"] = NS[py](job)
    elif func == "inflight_min_allowance_minutes":
        out["ret"] = NS[py]()
    elif func == "cron_cleanup_timeout_seconds":
        out["ret"] = NS[py]()
    elif func == "resolve_job_workdir":
        out["ret"] = NS[py](job, inp.get("job_id", ""))
    elif func == "resolve_job_reasoning_config":
        out["ret"] = NS[py](job, inp.get("cfg") or {}, inp.get("model", ""))
    elif func == "acquire_tick_lock":
        try:
            r = NS[py](inp.get("lock_file", ""))
            out["ret"] = r.fileno() if isinstance(r, _LockFile) else r
        except OSError as e:
            sent = {"__raise__": "OSError", "message": str(e)}
            if e.errno is not None:
                sent["errno"] = errno.errorcode.get(e.errno, "")
            out["ret"] = sent
    elif func == "release_tick_lock":
        fd = inp.get("lock_fd")
        class _FD:
            def fileno(self):
                return fd
            def close(self):
                return ctx.recorder("close")(fd)
        try:
            NS[py](_FD())
            out["ret"] = None
        except OSError as e:
            out["ret"] = {"__raise__": "OSError", "message": str(e)}
    elif func == "maybe_reap_dead_owners":
        NS[py]()
        out["ret"] = None
    elif func == "sweep_stale_inflight_for_tick":
        NS[py](inp.get("due_jobs") or [])
        out["ret"] = None
    elif func == "resolve_max_parallel_workers":
        out["ret"] = NS[py]()
    elif func == "sweep_mcp_orphans":
        NS[py]()
        out["ret"] = None
    elif func == "sweep_mcp_orphans_when_all_done":
        futures = []
        for f in inp.get("futures") or []:
            class _F:
                def __init__(self, exc):
                    self._exc = exc
                def add_done_callback(self, cb):
                    ctx.recorder("future.add_done_callback")()
                    self._cb = cb
                def exception(self):
                    return ctx.recorder("future.exception")()
            fo = _F(f.get("exception"))
            futures.append(fo)
        NS[py](futures)
        for fo in futures:
            cb = getattr(fo, "_cb", None)
            if cb:
                cb(fo)
        out["ret"] = None
    elif func == "process_due_job":
        try:
            r = NS[py](job, None, None, False)
            out["ret"] = r
        except BaseException as e:
            out["ret"] = {"__raise__": type(e).__name__, "message": str(e)}
    elif func == "record_fire_ownership_lost":
        fo = inp.get("fire_owner")
        NS[py](job, fo if fo else None, inp.get("execution_id", ""))
        out["ret"] = None
    elif func == "fire_ownership_lost":
        own = inp.get("ownership") or {}
        fence = NS["_FireOwnership"](job)
        fence.owner = own.get("owner")
        fence.transport_cancel = _Event(ctx, "transport_cancel") if own.get("transport_cancel") is not None else None
        if fence.transport_cancel is not None:
            fence.transport_cancel._set = bool(own["transport_cancel"].get("set"))
        fence.claim_lost = _Event(ctx, "claim_lost") if own.get("claim_lost") is not None else None
        if fence.claim_lost is not None:
            fence.claim_lost._set = bool(own["claim_lost"].get("set"))
        out["ret"] = fence.lost()
    elif func == "mark_incident_alerted":
        NS[py](inp.get("incident_id") or None)
        out["ret"] = None
    elif func == "resolve_incidents_for_recovered_job":
        NS[py](job)
        out["ret"] = None
    elif func == "upsert_incident_for_failure":
        r = NS[py](job, inp.get("error", ""), output_file=inp.get("output_file"))
        out["ret"] = list(r)
    elif func == "record_stale_release":
        NS[py](job, inp.get("job_id", ""), inp.get("age", 0), inp.get("allowance", 0),
               None, inp.get("reason", ""))
        out["ret"] = None
    elif func == "discard_parallel_pools":
        NS[py](inp.get("home_keys") or [])
        out["ret"] = None
    elif func in ("cron_run_scope_enter", "cron_run_scope_exit"):
        scope = copy.deepcopy(inp.get("scope") or {})
        obj = types.SimpleNamespace(**scope)
        obj._cron_session_var = types.SimpleNamespace(
            set=ctx.recorder("cron_session_var.set"), reset=ctx.recorder("cron_session_var.reset"))
        obj._var_map = {n: types.SimpleNamespace(set=ctx.recorder("var_map.set"))
                        for n in ("HERMES_CRON_AUTO_DELIVER_PLATFORM",
                                  "HERMES_CRON_AUTO_DELIVER_CHAT_ID",
                                  "HERMES_CRON_AUTO_DELIVER_THREAD_ID")}
        NS["enter_non_dispatcher_owned_context"] = ctx.recorder("enter_non_dispatcher_owned_context")
        NS["exit_non_dispatcher_owned_context"] = ctx.recorder("exit_non_dispatcher_owned_context")
        NS["bind_run"] = ctx.recorder("bind_run")
        NS["unbind_run"] = ctx.recorder("unbind_run")
        NS["clear_session_cwd"] = ctx.recorder("clear_session_cwd")
        NS["clear_session_vars"] = ctx.recorder("clear_session_vars")
        if func == "cron_run_scope_enter":
            obj.enter()
        else:
            obj.exit()
        out["ret"] = None
        out["scope"] = {k: getattr(obj, k) for k in scope}
    elif func == "run_with_fire_claim_heartbeat":
        def run_cb(lost):
            return ctx.recorder("run")(lost)
        NS["threading"] = types.SimpleNamespace(Event=lambda: _Event(ctx, "stop"))
        def start_thread(*a, **k):
            r = ctx.recorder("_start_heartbeat_thread")(*a, **k)
            return _Thread(ctx) if r else None
        NS["_start_heartbeat_thread"] = start_thread
        r = NS[py](job, run_cb)
        out["ret"] = r
    elif func == "compose_run_delivery":
        r = NS[py](job, success=inp.get("success", False), error=inp.get("error"),
                   final_response=inp.get("final_response", ""), output_file=inp.get("output_file"),
                   agent_declared=inp.get("agent_declared", False))
        out["ret"] = list(r)
    elif func == "deliver_crash_failure":
        r = NS[py](job, inp.get("err_text", ""), adapters=None, loop=None)
        out["ret"] = list(r)
    else:
        raise KeyError(func)

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
        if "scope" in want:
            ok = ok and got.get("scope") == want.get("scope")
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(got, sort_keys=True)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE scheduler_flow: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())