#!/usr/bin/env python3
"""Oracle for the cron/scheduler.py UNCLAIMED-remainder FLOW port
(src/agent/scheduler_flow2.c).

Drives the LIVE cron/scheduler.py functions with SCRIPTED I/O: every collaborator
(the clock, config/env reads, the jobs/executions ledger, croniter, the
fallback-chain helpers, the secret-scope/contextvar collaborators, the
delivery-target resolver, logging, and every CLAIMED sibling) is a recording
stub fed the same scripts the C harness feeds its universal seam. The UNCLAIMED
siblings composed for real (_wedged_inflight_keys / _blocked_config_result) run
live. Compares the ordered seam call sequence ("trace"), the final return
("ret"), and the mutated module-global state ("state").

The live defs are AST-extracted (module constants seeded; Assign AND AnnAssign;
annotations kept lazy via __future__.annotations). The heavy quarry import chain
is never imported; in-function imports are satisfied with sys.modules stubs whose
functions delegate to the current case's recorder. Fail-closed: missing defs or
0 cases => exit 2. Summary: 'ORACLE scheduler_flow2: N cases, M mismatches'.

Representation contract (shared with the C port header):
- In-flight KEY = (home_key, job_id) tuple <-> JSON 2-array [home_key, job_id].
- Sets (_running_job_ids / _scope_isolated_job_ids) are INSERTION-ordered doubles
  (live set iteration order is hash-randomized and NOT part of the contract);
  sorted/len/membership projections are order-insensitive.
- Key-keyed dicts (_running_since / _running_allowance_s / _running_worker_pids /
  _running_futures / _running_fire_owners) are OrderedDicts keyed by tuple,
  materialized from / projected to arrays of {"key":[h,j], <field>:value}.
- Paths are POSIX strings (PurePosixPath); datetimes are aware ISO-8601 strings.
- future spec: null / {"__pending__":true} / {"id":str}; pool spec:
  {"has_credentials","entries"}; agent spec: {"id","has_activity"}.
- logger calls ARE traced ("logger.<level>", {msg,args,exc_info?}); BaseException
  args project to str(exc), Path args to str.
- Integral floats == ints; str()/type-name fixtures stay integral-int or
  non-integral; int()/fromisoformat invalid tokens stay printable ASCII.
"""
import ast
import contextlib
import json
import math
import os
import re
import sys
import textwrap
import types
import uuid
import errno
import logging
import threading
import contextvars
import concurrent.futures
from collections import OrderedDict
from dataclasses import dataclass, field
from datetime import datetime, timezone, UTC
from pathlib import Path, PurePosixPath, PurePath
from typing import Any, Optional, Callable, Dict, List, Union

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

# Scrubbed-env safety: derive the quarry from this file (tests/ -> repo -> home).
_REPO = Path(__file__).resolve().parent.parent
_HOME = _REPO.parent
os.environ.setdefault("HOME", str(_HOME))
os.environ.setdefault("USERPROFILE", str(_HOME))
os.environ.setdefault("LOCALAPPDATA", str(_HOME / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
Q = os.path.join(str(_HOME), "hermes-agent")
SRC = os.path.join(Q, "cron", "scheduler.py")

TARGETS = [
    "_job_fallback_chain", "_current_gateway_code_sha", "_remember_inflight_home",
    "_inflight_home_path", "get_running_job_details", "_wedged_inflight_keys",
    "get_wedged_job_ids", "get_restart_wait_cron_counts", "_record_external_cron_worker",
    "_cron_interval_minutes", "_latest_executions_for_releasable_claims",
    "_row_belongs_to_claim", "_reclaim_fds_best_effort", "_load_prefill_messages",
    "_preflight_or_block", "_blocked_config_result", "_load_credential_pool",
    "_init_cron_mcp_tools", "_raise_inactivity_timeout", "_run_doc_header",
    "_reload_dotenv_and_publish_delivery_target", "_install_fire_secret_scope",
    "_reset_fire_secret_scope", "_start_owned_run", "_maybe_run_worktree_maintenance",
]
# Claimed siblings DEFINED in scheduler.py and seamed by name (extractable).
SEAMED_SIBLINGS = [
    "_job_route_pinned", "_inflight_key", "_inflight_min_allowance_minutes",
    "_job_interval_minutes", "_get_hermes_home",
]
# Claimed siblings IMPORTED into scheduler.py (bare globals; seeded as
# placeholders, installed as recorders per case — not extractable defs).
IMPORTED_SEAMS = [
    "_hermes_now", "_cron_preflight_enabled", "_preflight_job_config",
    "_resolve_delivery_target",
]
# Module-top bare globals (imported names) seamed as recorders.
BARE_GLOBALS = [
    "hermes_home_key", "get_fallback_chain", "scoped_fallback_chain",
    "cron_env_setting", "request_hard_interrupt", "get_hermes_home",
]

import __future__ as _fut
_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_text(text, ns, path):
    exec(compile(textwrap.dedent(text), path, "exec", flags=_FLAGS, dont_inherit=True), ns)


# Byte-pinned constants imported at the bottom of cron/scheduler.py from the
# split modules (cron.scheduler_prompt / cron.scheduler_preflight).
SPLIT_CONSTS = {
    "_PROMPT_HEADING": "## Prompt\n\n",
    "_PROMPT_SEPARATOR": "\n\n",
    "BLOCKED_CONFIG_MARKER": "[blocked_config]",
    "BLOCKED_CONFIG_SILENT_MARKER": "[blocked_config:silent]",
}


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {
        "__name__": "sc2_oracle_sched",
        "re": re, "json": json, "os": os, "sys": sys, "types": types,
        "contextlib": contextlib, "math": math, "logging": logging,
        "threading": threading, "contextvars": contextvars, "uuid": uuid,
        "errno": errno, "concurrent": types.SimpleNamespace(futures=concurrent.futures),
        "dataclass": dataclass, "field": field,
        "Path": Path, "PurePosixPath": PurePosixPath,
        "datetime": datetime, "timezone": timezone, "UTC": UTC,
        "Any": Any, "Optional": Optional, "Callable": Callable,
        "Dict": Dict, "List": List, "Union": Union,
        "SimpleNamespace": types.SimpleNamespace,
        "OrderedDict": OrderedDict,
    }
    sys.modules.setdefault("sc2_oracle_sched", types.ModuleType("sc2_oracle_sched"))
    tree = ast.parse(src)

    def seed_consts():
        for node in tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
                if not all(isinstance(t, ast.Name) for t in tgts):
                    continue
                with contextlib.suppress(Exception):
                    exec_text(seg(src, node), ns, SRC)

    seed_consts()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            with contextlib.suppress(Exception):
                exec_text(seg(src, node), ns, SRC)
    seed_consts()
    ns.update(SPLIT_CONSTS)
    # Imported bare globals / claimed siblings: placeholder until install_globals.
    for nm in IMPORTED_SEAMS + BARE_GLOBALS:
        ns.setdefault(nm, lambda *a, **k: None)
    missing = [t for t in TARGETS if t not in ns]
    missing += [s for s in SEAMED_SIBLINGS if s not in ns]
    for cname in ("SILENT_MARKER", "_INFLIGHT_MIN_ALLOWANCE_MINUTES", "_FUTURE_PENDING",
                  "_WORKTREE_MAINTENANCE_INTERVAL_SECONDS", "_running_lock",
                  "_worktree_maintenance_lock", "_cron_interval_cache"):
        if cname not in ns:
            missing.append(cname)
    if missing:
        print(f"FATAL oracle_scheduler_flow2: missing defs: {missing}")
        sys.exit(2)
    return ns


NS = extract()
PRISTINE = dict(NS)


# ── exception materialization ─────────────────────────────────────────────
EXC_CLASSES = {
    "OSError": OSError, "PermissionError": PermissionError,
    "FileNotFoundError": FileNotFoundError, "FileExistsError": FileExistsError,
    "BlockingIOError": BlockingIOError, "TimeoutError": TimeoutError,
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "Exception": Exception,
    "IndexError": IndexError, "StopIteration": StopIteration,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "OverflowError": OverflowError, "AssertionError": AssertionError,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    en = spec.get("errno")
    if en is not None and issubclass(cls, OSError):
        return cls(en, msg)
    return cls(msg)


def proj_exc(exc):
    d = {"__raise__": type(exc).__name__}
    if isinstance(exc, OSError) and exc.errno is not None:
        d["message"] = exc.strerror if exc.strerror is not None else str(exc)
        d["errno"] = exc.errno
    else:
        d["message"] = str(exc)
    return d


# ── per-case script backend (fail-closed: presence test, not .get) ─────────
class Ctx:
    def __init__(self, inp):
        self.io = inp.get("io") or {}
        self.cursors = {}
        self.trace = []

    def call(self, name, args):
        self.trace.append({"name": name, "args": args})
        if name not in self.io:
            raise RuntimeError(f"no script for {name}")
        entry = self.io[name]
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                raise RuntimeError(f"script exhausted for {name}")
            self.cursors[name] = idx + 1
            entry = entry[idx]
        if isinstance(entry, dict) and entry.get("__script__"):
            if isinstance(entry.get("raise"), dict):
                raise make_exc(entry["raise"])
            return entry.get("ret")
        return entry


def canon(v):
    if isinstance(v, float):
        if math.isnan(v):
            return {"__float__": "nan"}
        if math.isinf(v):
            return {"__float__": "-inf" if v < 0 else "inf"}
        if v == int(v) and abs(v) < 1e16:
            return int(v)
        return v
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, PurePath):
        return str(v)
    if hasattr(v, "__fspath__"):
        return str(v)
    if isinstance(v, datetime):
        return v.isoformat()
    if isinstance(v, dict):
        return {k: canon(x) for k, x in v.items()}
    if isinstance(v, (list, tuple, set, frozenset)):
        return [canon(x) for x in v]
    return v


# ── state doubles ─────────────────────────────────────────────────────────
class KeySet:
    """Insertion-ordered set of (home_key, job_id) tuples; set ops -> frozenset."""
    def __init__(self, keys=()):
        self._d = OrderedDict()
        for k in keys:
            self._d[tuple(k)] = None

    def add(self, k):
        self._d[tuple(k)] = None

    def discard(self, k):
        self._d.pop(tuple(k), None)

    def __contains__(self, k):
        return tuple(k) in self._d

    def __iter__(self):
        return iter(self._d)

    def __len__(self):
        return len(self._d)

    def __or__(self, other):
        return frozenset(self._d) | frozenset(other)

    def __and__(self, other):
        return frozenset(self._d) & frozenset(other)

    def __sub__(self, other):
        return frozenset(self._d) - frozenset(other)

    def proj(self):
        return [list(k) for k in self._d]


class FutureDouble:
    def __init__(self, fid, ctx):
        self.id = fid
        self.ctx = ctx

    def done(self):
        return self.ctx.call("future.done", {"id": self.id})


class CroniterDouble:
    def __init__(self, ctx):
        self.ctx = ctx

    def get_next(self, typ):
        return datetime.fromisoformat(self.ctx.call("croniter.get_next", {}))


class PoolDouble:
    def __init__(self, spec):
        self.spec = spec or {}

    def has_credentials(self):
        return self.spec.get("has_credentials", False)

    def entries(self):
        return self.spec.get("entries", [])


class VarDouble:
    def __init__(self, name, ctx):
        self.name = name
        self.ctx = ctx

    def set(self, value):
        return self.ctx.call("var_map.set", {"name": self.name, "value": value})


class SilentLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class ThreadDouble:
    def __init__(self, ctx, target=None, args=(), name=None, daemon=None):
        self.ctx = ctx
        self.target = target
        self.args = args
        self.name = name
        self.daemon = daemon

    def start(self):
        self.ctx.call("thread.start", {"name": self.name, "daemon": self.daemon})
        try:
            if self.target is not None:
                self.target(*self.args)
        except BaseException:
            pass  # a daemon-thread exception is not observable in the caller


class RecLogger:
    def __init__(self, ctx):
        self.ctx = ctx

    def __getattr__(self, level):
        def log(msg, *args, exc_info=None, **kw):
            proj = []
            for a in args:
                if isinstance(a, BaseException):
                    proj.append(str(a))
                elif isinstance(a, PurePath) or hasattr(a, "__fspath__"):
                    proj.append(str(a))
                else:
                    proj.append(a)
            d = {"msg": msg, "args": proj}
            if exc_info is not None:
                d["exc_info"] = exc_info
            return self.ctx.call("logger." + level, d)
        return log


def _pstr(v):
    return None if v is None else str(v)


def mat_future(spec, ctx):
    if spec is None:
        return None
    if isinstance(spec, dict) and spec.get("__pending__"):
        return NS["_FUTURE_PENDING"]
    if isinstance(spec, dict):
        return FutureDouble(spec.get("id"), ctx)
    return spec


def proj_future(f):
    if f is None:
        return None
    if f is NS["_FUTURE_PENDING"]:
        return {"__pending__": True}
    if isinstance(f, FutureDouble):
        return {"id": f.id}
    return f


# state key -> (materializer, projector)
def mat_state_key(key, val, ctx):
    if key in ("_running_job_ids", "_scope_isolated_job_ids"):
        return KeySet(val or [])
    if key == "_running_since":
        return OrderedDict((tuple(e["key"]), e["started"]) for e in (val or []))
    if key == "_running_allowance_s":
        return OrderedDict((tuple(e["key"]), e["allowance"]) for e in (val or []))
    if key == "_running_worker_pids":
        return OrderedDict((tuple(e["key"]), e["pid"]) for e in (val or []))
    if key == "_running_futures":
        return OrderedDict((tuple(e["key"]), mat_future(e.get("future"), ctx)) for e in (val or []))
    if key == "_running_fire_owners":
        return OrderedDict((tuple(e["key"]), e.get("owners", {})) for e in (val or []))
    if key == "_inflight_home_paths":
        return {k: (PurePosixPath(v) if v is not None else None) for k, v in (val or {}).items()}
    if key == "_cron_interval_cache":
        return dict(val or {})
    if key == "_last_worktree_maintenance_at":
        return None if val is None else dict(val)
    return val


def proj_state_key(key, v):
    if isinstance(v, KeySet):
        return v.proj()
    if key == "_running_since":
        return [{"key": list(k), "started": val} for k, val in v.items()]
    if key == "_running_allowance_s":
        return [{"key": list(k), "allowance": val} for k, val in v.items()]
    if key == "_running_worker_pids":
        return [{"key": list(k), "pid": val} for k, val in v.items()]
    if key == "_running_futures":
        return [{"key": list(k), "future": proj_future(val)} for k, val in v.items()]
    if key == "_running_fire_owners":
        return [{"key": list(k), "owners": val} for k, val in v.items()]
    if key == "_inflight_home_paths":
        return {k: (str(p) if p is not None else None) for k, p in v.items()}
    if key == "_last_worktree_maintenance_at":
        return None if v is None else dict(v)
    if isinstance(v, OrderedDict):
        return dict(v)
    return v


# ── sys.modules stub tree (in-function imports) ───────────────────────────
def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    return m


def install_stub_modules(ctx):
    """Build the package tree for every in-function import; return saved keys."""
    mods = {}

    def reg(name, mod):
        mods[name] = mod

    def leaf(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        reg(name, m)
        return m

    # packages
    for p in ("cron", "hermes_cli", "agent", "tools", "gateway", "tui_gateway"):
        reg(p, _pkg(p))
    reg("hermes_constants", types.ModuleType("hermes_constants"))
    reg("gc", types.ModuleType("gc"))

    cs = leaf("gateway.code_skew",
              current_code_sha=lambda: ctx.call("current_code_sha", {}))
    mods["gateway"].code_skew = cs

    def _croniter(expr, base):
        ctx.call("croniter", {"expr": expr, "base": base.isoformat()})
        return CroniterDouble(ctx)

    cj = leaf("cron.jobs",
              load_jobs=lambda: ctx.call("load_jobs", {}),
              _ensure_croniter=lambda: ctx.call("_ensure_croniter", {}),
              croniter=_croniter,
              _ensure_aware=lambda dt: datetime.fromisoformat(
                  ctx.call("_ensure_aware", {"dt": dt.isoformat()})),
              clear_preflight_alerted=lambda job_id: ctx.call(
                  "clear_preflight_alerted", {"job_id": job_id}),
              mark_preflight_alerted=lambda job_id: ctx.call(
                  "mark_preflight_alerted", {"job_id": job_id}))
    mods["cron"].jobs = cj
    ce = leaf("cron.executions",
              latest_executions=lambda job_ids: ctx.call(
                  "latest_executions", {"job_ids": list(job_ids)}),
              get_execution=lambda eid: ctx.call("get_execution", {"execution_id": eid}),
              mark_execution_running=lambda eid: ctx.call(
                  "mark_execution_running", {"execution_id": eid}))
    mods["cron"].executions = ce
    csp = leaf("cron.scheduler_provider",
               routed_profile_fire=lambda: ctx.call("routed_profile_fire", {}))
    mods["cron"].scheduler_provider = csp
    csw = leaf("cron.scheduler_worktree_maintenance",
               run_sweep=lambda repos, *, interval_seconds: ctx.call(
                   "run_sweep", {"repos": getattr(repos, "__name__", repos),
                                 "interval_seconds": interval_seconds}))
    mods["cron"].scheduler_worktree_maintenance = csw

    rl = leaf("hermes_cli.resource_limits",
              apply_nofile_soft_limit=lambda limit: ctx.call(
                  "apply_nofile_soft_limit", {"limit": limit}))
    mods["hermes_cli"].resource_limits = rl
    el = leaf("hermes_cli.env_loader",
              load_hermes_dotenv=lambda *, hermes_home: ctx.call(
                  "load_hermes_dotenv", {"hermes_home": _pstr(hermes_home)}),
              reset_secret_source_cache=lambda home: ctx.call(
                  "reset_secret_source_cache", {"hermes_home": _pstr(home)}),
              hydrate_profile_secret_sources=lambda home: ctx.call(
                  "hydrate_profile_secret_sources", {"home": _pstr(home)}))
    mods["hermes_cli"].env_loader = el

    cp = leaf("agent.custom_entry_pool",
              load_route_pool=lambda provider, base_url, requested_provider: (
                  lambda r: None if r is None else PoolDouble(r))(
                  ctx.call("load_route_pool", {"provider": provider,
                                               "base_url": base_url,
                                               "requested_provider": requested_provider})))
    mods["agent"].custom_entry_pool = cp
    ss = leaf("agent.secret_scope",
              set_secret_scope=lambda scope, *, profile_home: ctx.call(
                  "set_secret_scope", {"scope": scope, "profile_home": profile_home}),
              set_multiplex_context=lambda value: ctx.call(
                  "set_multiplex_context", {"value": value}),
              reset_multiplex_context=lambda token: ctx.call(
                  "reset_multiplex_context", {"token": token}),
              reset_secret_scope=lambda token: ctx.call(
                  "reset_secret_scope", {"token": token}))
    mods["agent"].secret_scope = ss

    md = leaf("tools.mcp_tool_discovery",
              discover_mcp_tools=lambda: ctx.call("discover_mcp_tools", {}))
    mods["tools"].mcp_tool_discovery = md

    sc = leaf("gateway.session_context",
              _VAR_MAP={n: VarDouble(n, ctx) for n in (
                  "HERMES_CRON_AUTO_DELIVER_PLATFORM",
                  "HERMES_CRON_AUTO_DELIVER_CHAT_ID",
                  "HERMES_CRON_AUTO_DELIVER_THREAD_ID")})
    mods["gateway"].session_context = sc

    lp = leaf("tui_gateway.launch_profile_policy",
              owner_secret_scope=lambda home: ctx.call(
                  "owner_secret_scope", {"home": _pstr(home)}))
    mods["tui_gateway"].launch_profile_policy = lp

    mods["hermes_constants"].get_hermes_home = lambda: PurePosixPath(
        ctx.call("get_hermes_home", {}))
    mods["hermes_constants"].hermes_home_key = lambda home: ctx.call(
        "hermes_home_key", {"home": _pstr(home)})
    mods["gc"].collect = lambda: ctx.call("gc.collect", {})

    saved = {}
    for name, mod in mods.items():
        saved[name] = sys.modules.get(name)
        sys.modules[name] = mod
    return saved


def restore_modules(saved):
    for name, old in saved.items():
        if old is None:
            sys.modules.pop(name, None)
        else:
            sys.modules[name] = old


def install_globals(ctx):
    """ns-level recorders for bare globals + claimed siblings + clock/os/threading."""
    NS["logger"] = RecLogger(ctx)
    NS["Path"] = PurePosixPath  # POSIX path model (C ports paths as POSIX strings)
    NS["time"] = types.SimpleNamespace(
        time=lambda: ctx.call("time.time", {}),
        monotonic=lambda: ctx.call("time.monotonic", {}))
    NS["os"] = types.SimpleNamespace(
        environ=types.SimpleNamespace(
            get=lambda key: ctx.call("os.environ.get", {"key": key})))
    NS["threading"] = types.SimpleNamespace(
        Lock=lambda: SilentLock(),
        Thread=lambda target=None, args=(), name=None, daemon=None: ThreadDouble(
            ctx, target=target, args=args, name=name, daemon=daemon),
        Event=lambda: types.SimpleNamespace(set=lambda: None, wait=lambda t: True))
    NS["hermes_home_key"] = lambda home: ctx.call("hermes_home_key", {"home": _pstr(home)})
    NS["get_hermes_home"] = lambda: PurePosixPath(ctx.call("get_hermes_home", {}))
    NS["get_fallback_chain"] = lambda cfg: ctx.call("get_fallback_chain", {"cfg": cfg})

    def scoped(inherited, declared, *, pinned, owner):
        return ctx.call("scoped_fallback_chain",
                        {"inherited": inherited, "declared": declared,
                         "pinned": pinned, "owner": owner})
    NS["scoped_fallback_chain"] = scoped
    NS["cron_env_setting"] = lambda name: ctx.call("cron_env_setting", {"name": name})
    NS["_hermes_now"] = lambda: datetime.fromisoformat(ctx.call("_hermes_now", {}))
    NS["request_hard_interrupt"] = lambda agent, reason, *, tool_reason: ctx.call(
        "request_hard_interrupt",
        {"agent": getattr(agent, "id", agent), "reason": reason, "tool_reason": tool_reason})
    NS["_get_hermes_home"] = lambda: PurePosixPath(ctx.call("_get_hermes_home", {}))
    NS["_job_route_pinned"] = lambda job: ctx.call("_job_route_pinned", {"job": job})

    def inflight_key(job_id, home=None):
        return tuple(ctx.call("_inflight_key", {"job_id": job_id, "home": _pstr(home)}))
    NS["_inflight_key"] = inflight_key
    NS["_inflight_min_allowance_minutes"] = lambda: ctx.call(
        "_inflight_min_allowance_minutes", {})
    NS["_job_interval_minutes"] = lambda job: ctx.call("_job_interval_minutes", {"job": job})
    NS["_cron_preflight_enabled"] = lambda cfg: ctx.call("_cron_preflight_enabled", {"cfg": cfg})
    NS["_preflight_job_config"] = lambda job, cfg: ctx.call(
        "_preflight_job_config", {"job": job, "cfg": cfg})
    NS["_resolve_delivery_target"] = lambda job: ctx.call("_resolve_delivery_target", {"job": job})
    # _start_owned_run bare globals (module-top imports from cron.executions /
    # cron.execution_identity).
    NS["get_execution"] = lambda eid: ctx.call("get_execution", {"execution_id": eid})
    NS["mark_execution_running"] = lambda eid: ctx.call(
        "mark_execution_running", {"execution_id": eid})
    NS["enter_cron_execution"] = lambda job, eid, record: ctx.call(
        "enter_cron_execution", {"job": job, "execution_id": eid, "record": record})


class AgentDouble:
    def __init__(self, spec, ctx):
        self.id = spec.get("id")
        self.ctx = ctx
        if spec.get("has_activity"):
            self.get_activity_summary = lambda: ctx.call("agent.get_activity_summary", {})


def _posix_join(a, b):
    if b.startswith("/"):
        return b
    if not a:
        return b
    return a + ("" if a.endswith("/") else "/") + b


class FakePath:
    """POSIX Path double for _load_prefill_messages: expanduser/exists are seams,
    is_absolute is lexical, / joins lexically (no real filesystem access)."""
    def __init__(self, s, ctx):
        self.s = str(s)
        self.ctx = ctx

    def __str__(self):
        return self.s

    def __fspath__(self):
        return self.s

    def __repr__(self):
        return self.s

    def expanduser(self):
        return FakePath(self.ctx.call("path.expanduser", {"path": self.s}), self.ctx)

    def is_absolute(self):
        return self.s.startswith("/")

    def exists(self):
        return self.ctx.call("path.exists", {"path": self.s})

    def __truediv__(self, other):
        return FakePath(_posix_join(self.s, str(other)), self.ctx)


class FileDouble:
    def __init__(self, path, encoding, ctx):
        self.path = path
        self.encoding = encoding
        self.ctx = ctx

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class NowDouble:
    """datetime stand-in for the in-function `from datetime import datetime`."""
    @classmethod
    def now(cls):
        return datetime.fromisoformat(CUR_CTX.call("datetime.now", {}))

    fromisoformat = staticmethod(datetime.fromisoformat)


CUR_CTX = None


def run_case(func, inp):
    global CUR_CTX
    ctx = Ctx(inp)
    CUR_CTX = ctx
    NS.clear()
    NS.update(PRISTINE)
    install_globals(ctx)
    saved = install_stub_modules(ctx)
    # import-failure gates (None halts the import before any finder runs)
    gated = inp.get("import_fail") or []
    gated_saved = {}
    for mod in gated:
        gated_saved[mod] = sys.modules.get(mod)
        sys.modules[mod] = None
    # datetime stub for _cron_interval_minutes's in-function import
    dt_saved = sys.modules.get("datetime")
    if func == "_cron_interval_minutes":
        dtmod = types.ModuleType("datetime")
        dtmod.datetime = NowDouble
        dtmod.timezone = timezone
        dtmod.UTC = UTC
        sys.modules["datetime"] = dtmod
    # materialize state globals (only fixture-present keys)
    state_in = inp.get("state") or {}
    state_keys = list(state_in.keys())
    for k in state_keys:
        NS[k] = mat_state_key(k, state_in[k], ctx)
    out = {}
    ret = None
    try:
        if func == "_job_fallback_chain":
            ret = NS["_job_fallback_chain"](inp.get("job"), inp.get("cfg"))
        elif func == "_current_gateway_code_sha":
            ret = NS["_current_gateway_code_sha"]()
        elif func == "_remember_inflight_home":
            r = NS["_remember_inflight_home"](PurePosixPath(inp["home"]))
            ret = str(r)
        elif func == "_inflight_home_path":
            r = NS["_inflight_home_path"](inp["home_key"])
            ret = str(r)
        elif func == "get_running_job_details":
            ret = NS["get_running_job_details"]()
        elif func == "_wedged_inflight_keys":
            ret = sorted(list(k) for k in NS["_wedged_inflight_keys"]())
        elif func == "get_wedged_job_ids":
            ret = sorted(NS["get_wedged_job_ids"]())
        elif func == "get_restart_wait_cron_counts":
            ret = NS["get_restart_wait_cron_counts"]()
        elif func == "_record_external_cron_worker":
            NS["_record_external_cron_worker"](
                inp["job_id"], inp.get("pid"),
                scope_isolated=inp.get("scope_isolated", False))
            ret = None
        elif func == "_cron_interval_minutes":
            ret = NS["_cron_interval_minutes"](inp["expr"])
        elif func == "_latest_executions_for_releasable_claims":
            ret = NS["_latest_executions_for_releasable_claims"]()
        elif func == "_row_belongs_to_claim":
            ret = NS["_row_belongs_to_claim"](inp.get("row"), inp.get("claim_started"))
        elif func == "_reclaim_fds_best_effort":
            NS["_reclaim_fds_best_effort"]()
            ret = None
        elif func == "_load_prefill_messages":
            NS["Path"] = lambda s: FakePath(s, ctx)
            NS["_get_hermes_home"] = lambda: FakePath(ctx.call("_get_hermes_home", {}), ctx)
            NS["open"] = lambda path, mode="r", encoding=None: FileDouble(str(path), encoding, ctx)
            NS["json"] = types.SimpleNamespace(
                load=lambda fp: ctx.call("prefill.load",
                                         {"path": fp.path, "encoding": fp.encoding}))
            ret = NS["_load_prefill_messages"](inp.get("cfg"), inp["job_id"])
        elif func == "_preflight_or_block":
            ret = NS["_preflight_or_block"](inp.get("job"), inp["job_id"],
                                            inp["job_name"], inp.get("cfg"))
        elif func == "_blocked_config_result":
            r = NS["_blocked_config_result"](inp["job_id"], inp["job_name"], inp["_pf_reason"])
            ret = list(r)
        elif func == "_load_credential_pool":
            pool = NS["_load_credential_pool"](inp.get("runtime"), inp["job_id"])
            ret = None if pool is None else pool.spec
        elif func == "_init_cron_mcp_tools":
            NS["_init_cron_mcp_tools"](inp["job_id"])
            ret = None
        elif func == "_raise_inactivity_timeout":
            agent = AgentDouble(inp.get("agent") or {}, ctx)
            NS["_raise_inactivity_timeout"](agent, inp["job_name"], inp.get("limit_s"))
            ret = None
        elif func == "_run_doc_header":
            ret = NS["_run_doc_header"](inp.get("job"), inp["title"], inp["job_id"],
                                        inp["prompt"], prompt_stamp=inp.get("prompt_stamp", ""))
        elif func == "_reload_dotenv_and_publish_delivery_target":
            NS["_reload_dotenv_and_publish_delivery_target"](inp.get("job"))
            ret = None
        elif func == "_install_fire_secret_scope":
            r = NS["_install_fire_secret_scope"]()
            ret = list(r)
        elif func == "_reset_fire_secret_scope":
            NS["_reset_fire_secret_scope"](inp.get("tokens"))
            ret = None
        elif func == "_start_owned_run":
            ret = NS["_start_owned_run"](inp.get("job"), inp["execution_id"])
        elif func == "_maybe_run_worktree_maintenance":
            NS["_maybe_run_worktree_maintenance"]()
            ret = None
        else:
            out["error"] = "unknown func"
            return out
    except BaseException as exc:  # noqa: BLE001 - raise sentinel parity (incl. trio)
        ret = proj_exc(exc)
    finally:
        restore_modules(saved)
        for mod, old in gated_saved.items():
            if old is None:
                sys.modules.pop(mod, None)
            else:
                sys.modules[mod] = old
        if func == "_cron_interval_minutes":
            if dt_saved is None:
                sys.modules.pop("datetime", None)
            else:
                sys.modules["datetime"] = dt_saved
    out["ret"] = ret
    out["trace"] = ctx.trace
    out["state"] = {k: proj_state_key(k, NS[k]) for k in state_keys if k in NS}
    return out


def deep_eq(a, b):
    return canon(a) == canon(b)


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or not line.startswith("{"):
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = rec.get("func")
        inp = rec.get("in", {}) or {}
        c_out = rec.get("out")
        cases += 1
        try:
            py_out = run_case(func, inp)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print(f"PYERR {func}: {type(exc).__name__}: {exc}")
            continue
        if c_out is None:
            mism += 1
            print(f"MISMATCH {func}: missing C out")
            continue
        bad = []
        for key, want in py_out.items():
            got = c_out.get(key)
            if not deep_eq(got, want):
                bad.append(key)
        if bad:
            mism += 1
            print(f"MISMATCH {func} keys={bad}")
            for key in bad:
                print(f"  C.{key}={json.dumps(canon(c_out.get(key)), default=str)[:600]}")
                print(f"  PY.{key}={json.dumps(canon(py_out.get(key)), default=str)[:600]}")
    if cases == 0:
        print("FATAL oracle_scheduler_flow2: 0 cases")
        print("ORACLE scheduler_flow2: 0 cases, 0 mismatches")
        return 2
    print(f"ORACLE scheduler_flow2: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
