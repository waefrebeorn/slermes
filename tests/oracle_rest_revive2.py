#!/usr/bin/env python3
"""oracle_rest_revive2.py — differential oracle for the rest group-2 revival.

Reads the harness JSONL on stdin ({"func","in","out"}), AST-extracts the LIVE
quarry defs (cron/job_args.py, tools/cronjob_tools.py, hermes_cli/cron.py,
utils/env.py, cron/scheduler_delivery.py), seeds module constants (Assign +
AnnAssign), drives each live function under the SAME scripted seam results
the C harness consumed, and compares ordered trace + ret + post-state.
BaseException is caught per case (SystemExit included); missing/exhausted
scripts fail closed on both sides.

Seam conventions (identical on both sides, documented in the gap files):
- In-function imports are always resolvable; a scripted raise on the imported
  callable models a CALL failure (import-failure paths are out of scope).
- Seam args drop None-valued keys on both sides; kwarg order is preserved.
- Integral JSON numbers model Python ints, fractional ones floats; JSON
  arrays model lists (tuples where the live code returns tuples).
- Path doubles normalize str() to as_posix(); lexical ops follow HOST pathlib
  semantics (CPython 3.14 transcribed; #ifdef _WIN32 on the C side).
- owner/future identity rides fixed ids "owner:1"/"future:1"; the dispatch
  runner closure rides "runner:1".
- Exception instances in logger args serialize as {"__exc__", "message"}.

Gate:
  bash tests/run_one_oracle.sh rest_revive2 oracle_rest_revive2.py "" "lib/libjson/json.o"
"""
import ast
import builtins
import copy
import __future__
import json
import math
import os
import re
import sys
import time as _time_mod
import types
import threading
import contextlib
import logging
import datetime as _dtm
from concurrent.futures import Future
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(REPO / ".testbin" / "dbg_home_r2"))

FUT = __future__.annotations.compiler_flag
MISSING = object()
IS_WIN = os.name == "nt"


def norm(x):
    """Integral floats -> ints; tuples/sets -> lists (sets sorted); Path -> posix."""
    if isinstance(x, float):
        if math.isfinite(x) and x == int(x) and abs(x) < 1e16:
            return int(x)
        return x
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    if isinstance(x, (set, frozenset)):
        return sorted(norm(v) for v in x)
    if isinstance(x, Path):
        return x.as_posix()
    if isinstance(x, PathDoubleBase):
        return x.posix
    return x


class Raise:
    def __init__(self, spec):
        ty = spec.get("type", "Exception")
        msg = spec.get("message", "")
        self.spec = {"__raise__": ty, "message": msg}
        cls = getattr(builtins, ty, None)
        if not (isinstance(cls, type) and issubclass(cls, BaseException)):
            cls = type(ty, (Exception,), {})
        self.exc = cls(msg)


class Script:
    def __init__(self, io_map):
        self.io = io_map or {}
        self.cursors = {}

    def next(self, name):
        entry = self.io.get(name, MISSING)
        if entry is MISSING:
            # RETURN a Raise (not raise): make_rec must record the fail-closed
            # event before propagating, exactly like the C dispatcher+seam.
            return Raise({"type": "RuntimeError",
                          "message": f"no script for {name}"})
        if isinstance(entry, list):
            i = self.cursors.get(name, 0)
            if i >= len(entry):
                return Raise({"type": "RuntimeError",
                              "message": f"script exhausted for {name}"})
            self.cursors[name] = i + 1
            entry = entry[i]
        if isinstance(entry, dict) and entry.get("__script__"):
            if "raise" in entry:
                return Raise(entry["raise"])
            return copy.deepcopy(entry.get("ret"))
        return copy.deepcopy(entry)


# ── AST extraction ──────────────────────────────────────────────────────────

def compile_node(node, filename):
    mod = ast.Module(body=[node], type_ignores=[])
    ast.fix_missing_locations(mod)
    return compile(mod, filename, "exec", flags=FUT, dont_inherit=True)


def load_ns(relpath, seeds, want_funcs=(), strict_consts=False):
    src = (QUARRY / relpath).read_text(encoding="utf-8")
    tree = ast.parse(src)
    modname = "quarry_r2_" + re.sub(r"\W", "_", relpath)
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update(seeds)
    ns["__name__"] = modname
    consts = [n for n in tree.body
              if isinstance(n, (ast.Assign, ast.AnnAssign))
              and n.value is not None
              and all(isinstance(t, ast.Name) for t in
                      (n.targets if isinstance(n, ast.Assign) else [n.target]))]

    def seed_consts(strict):
        for n in consts:
            try:
                exec(compile_node(n, relpath), ns)
            except Exception:
                if strict:
                    raise

    seed_consts(False)
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in want_funcs:
            n2 = copy.copy(n)
            n2.decorator_list = []
            exec(compile_node(n2, relpath), ns)
    seed_consts(strict_consts)
    for name in want_funcs:
        assert name in ns, f"seed failure: {relpath}:{name}"
    return ns


class Colors:
    RESET = "\033[0m"
    BOLD = "\033[1m"
    DIM = "\033[2m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"
    MAGENTA = "\033[35m"
    CYAN = "\033[36m"


def base_seeds(modname):
    logging_double = types.SimpleNamespace(
        getLogger=lambda name: types.SimpleNamespace(
            warning=lambda *a, **k: None, debug=lambda *a, **k: None,
            info=lambda *a, **k: None, error=lambda *a, **k: None))
    return {
        "__name__": modname,
        "Any": object, "Callable": object, "Optional": object, "Dict": object,
        "List": object, "Iterable": object, "Union": object, "Tuple": object,
        "Literal": object, "Mapping": object,
        "contextlib": contextlib,
        "logging": logging_double,
        "threading": threading,
        "time": types.SimpleNamespace(monotonic=_time_mod.monotonic,
                                      time=_time_mod.time),
        "Future": Future,
        "copy": copy,
        "math": math, "re": re, "json": json,
        "sys": sys,
        "os": types.SimpleNamespace(environ={}),
        "datetime": _dtm.datetime, "timedelta": _dtm.timedelta,
        "timezone": _dtm.timezone, "UTC": _dtm.UTC,
        "Path": Path,
        "Colors": Colors,
        "color": lambda text, *codes: text,
    }


def pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


# ── module namespaces (built once) ──────────────────────────────────────────

NS_JA = load_ns(
    "cron/job_args.py", base_seeds("quarry_r2_ja"),
    want_funcs=("_mode_guidance_notes", "_split_monitor_arg",
                "_resolve_cron_context_deliver", "_gateway_liveness_notice",
                "_clean_str_list"))

NS_CT = load_ns(
    "tools/cronjob_tools.py", base_seeds("quarry_r2_ct"),
    want_funcs=("_run_claimed_job", "_try_dispatch_background_run",
                "_forward_relay_fronted_run", "_latest_job_output_excerpt",
                "_manual_run_delivery_note", "_cronjob_handler",
                "_claim_for_manual_run", "_background_session_key",
                "_run_heartbeat", "_dumps", "_reap_stale_executions"))

NS_HC = load_ns(
    "hermes_cli/cron.py", base_seeds("quarry_r2_hc"),
    want_funcs=("_cron_doctor_issues_for_job", "_dispatch_display",
                "_script_health_issue", "_format_lateness",
                "_next_run_overdue_issue", "_scripts_dir_for_cron",
                "_short_reason", "_delivery_fix_hint", "_missed_fire_issue",
                "_unverified_targets", "_dispatch_kind_label",
                "_next_run_overdue_seconds"))

NS_UE = load_ns("utils/env.py", base_seeds("quarry_r2_ue"),
                want_funcs=("is_truthy_value",))
NS_SD = load_ns("cron/scheduler_delivery.py", base_seeds("quarry_r2_sd"),
                want_funcs=("_normalize_deliver_value",))

# cronjob_tools imports these at module top; the extracted defs read them as
# globals. Real siblings where pure, recorders installed per case otherwise.
NS_CT["_split_monitor_arg"] = NS_JA["_split_monitor_arg"]
NS_CT["_clean_str_list"] = NS_JA["_clean_str_list"]
# _run_heartbeat is @contextlib.contextmanager live; load_ns strips decorators,
# so restore the wrapper (a bare generator breaks `with _run_heartbeat(...)`).
NS_CT["_run_heartbeat"] = contextlib.contextmanager(NS_CT["_run_heartbeat"])
assert "_CRON_RUN_HEARTBEAT_INTERVAL" in NS_CT, "heartbeat interval constant missing"
assert "_CRON_RUN_HEARTBEAT_CEILING" in NS_CT, "heartbeat ceiling constant missing"

REAL_JA = {k: NS_JA[k] for k in
           ("_mode_guidance_notes", "_split_monitor_arg",
            "_resolve_cron_context_deliver", "_gateway_liveness_notice",
            "_clean_str_list")}
REAL_CT = {k: NS_CT[k] for k in
           ("_run_claimed_job", "_try_dispatch_background_run",
            "_forward_relay_fronted_run", "_latest_job_output_excerpt",
            "_manual_run_delivery_note", "_cronjob_handler",
            "_claim_for_manual_run", "_background_session_key",
            "_run_heartbeat", "_dumps", "_reap_stale_executions",
            "_split_monitor_arg", "_clean_str_list")}
REAL_HC = {k: NS_HC[k] for k in
           ("_cron_doctor_issues_for_job", "_dispatch_display",
            "_script_health_issue", "_format_lateness",
            "_next_run_overdue_issue", "_scripts_dir_for_cron",
            "_short_reason", "_delivery_fix_hint", "_missed_fire_issue",
            "_unverified_targets", "_dispatch_kind_label",
            "_next_run_overdue_seconds")}
SILENT_LOG = base_seeds("x")["logging"].getLogger("silent")
REAL_TIME = types.SimpleNamespace(monotonic=_time_mod.monotonic,
                                  time=_time_mod.time)

# ── lexical Path model (mirrors the C r2_path_* helpers; host semantics) ───


def pnorm(s):
    s = str(s)
    rooted = s.startswith("/")
    unc = s.startswith("//")
    parts = [p for p in s.split("/") if p and p != "."]
    if unc:
        return "//" + "/".join(parts)
    if rooted:
        return "/" + "/".join(parts)
    return "/".join(parts) if parts else "."


def p_is_abs(n):
    if IS_WIN:
        if n.startswith("//"):
            return True
        return (len(n) >= 3 and n[0].isascii() and n[0].isalpha()
                and n[1] == ":" and n[2] == "/")
    return n.startswith("/")


def p_parent(n):
    if n == "/":
        return "/"
    if n.startswith("//"):
        return n
    if "/" not in n:
        return "."
    head = n.rsplit("/", 1)[0]
    if head == "":
        return "/"
    if len(head) == 2 and head[1] == ":" and head[0].isascii() and head[0].isalpha():
        return head + "/"
    return head


def p_join(a, b):
    b = str(b)
    if b == "":
        return pnorm(a)
    bn = pnorm(b)
    if p_is_abs(bn) or bn.startswith("/"):
        return bn
    an = pnorm(a)
    if an == ".":          # Path(".") / "x" == Path("x")
        return bn
    return an + ("" if an.endswith("/") else "/") + bn


def p_parts(n):
    if n.startswith("//"):
        rest = n[2:]
        seg = rest.split("/")
        root = "//" + "/".join(seg[:2]) if len(seg) >= 2 else n
        return [root] + [p for p in seg[2:] if p]
    if n.startswith("/"):
        return ["/"] + [p for p in n[1:].split("/") if p]
    return [p for p in n.split("/") if p]


def p_key(n):
    return tuple(p.lower() for p in p_parts(n)) if IS_WIN else tuple(p_parts(n))


def p_lt(a, b):
    return p_key(a) < p_key(b)


def p_relative_ok(p, base):
    kp, kb = p_key(p), p_key(base)
    return len(kb) <= len(kp) and kp[:len(kb)] == kb


class PathDoubleBase:
    """Marker base so norm() can stringify doubles."""
    posix = ""


def make_path_double(rec):
    class PathDouble(PathDoubleBase):
        def __init__(self, raw=""):
            if isinstance(raw, os.PathLike):
                raw = os.fspath(raw)
            self.posix = pnorm(raw)

        def __str__(self):
            return self.posix

        def __fspath__(self):
            return self.posix

        def __repr__(self):
            return f"PathDouble({self.posix!r})"

        @property
        def parent(self):
            return PathDouble(p_parent(self.posix))

        def __truediv__(self, other):
            if not isinstance(other, (str, os.PathLike)):
                cls = "WindowsPath" if IS_WIN else "PosixPath"
                raise TypeError("unsupported operand type(s) for /: "
                                f"'{cls}' and '{type(other).__name__}'")
            o = os.fspath(other) if isinstance(other, os.PathLike) else other
            return PathDouble(p_join(self.posix, o))

        def is_absolute(self):
            return p_is_abs(self.posix)

        def relative_to(self, base):
            if not p_relative_ok(self.posix, str(base)):
                raise ValueError(f"{self.posix!r} is not in the subpath of {str(base)!r}")
            kp, kb = p_parts(self.posix), p_parts(str(base))
            return PathDouble("/".join(kp[len(kb):]) or ".")

        def resolve(self):
            r = rec("path_resolve", lambda a, k: {"path": self.posix})()
            return PathDouble(r)

        def expanduser(self):
            r = rec("path_expanduser", lambda a, k: {"path": self.posix})()
            return PathDouble(r)

        def exists(self):
            return rec("path_exists", lambda a, k: {"path": self.posix})()

        def is_file(self):
            return rec("path_is_file", lambda a, k: {"path": self.posix})()

        def glob(self, pattern):
            r = rec("glob", lambda a, k: {"dir": self.posix, "pattern": a[0]})(pattern)
            return [PathDouble(p) for p in r]

        def read_text(self, encoding=None, errors=None):
            return rec("read_text",
                       lambda a, k: {"path": self.posix,
                                     "encoding": k.get("encoding"),
                                     "errors": k.get("errors")})(
                           encoding=encoding, errors=errors)

        def __lt__(self, other):
            return p_lt(self.posix, other.posix)

        def __eq__(self, other):
            return isinstance(other, PathDouble) and p_key(self.posix) == p_key(other.posix)

        def __hash__(self):
            return hash(p_key(self.posix))

    return PathDouble


def checked_path_ctor(PD):
    """Path() constructor double: pathlib type-check BEFORE construction."""
    def ctor(raw=""):
        if not isinstance(raw, (str, os.PathLike)):
            raise TypeError(
                "argument should be a str or an os.PathLike object where "
                f"__fspath__ returns a str, not '{type(raw).__name__}'")
        return PD(raw)
    return ctor


# ── recording seam factory ──────────────────────────────────────────────────

def exc_json(x):
    if isinstance(x, BaseException):
        return {"__exc__": type(x).__name__, "message": str(x)}
    return norm(x)


def make_rec(script, trace):
    def rec(name, argb, make=None):
        def call(*a, **k):
            r = script.next(name)
            args = {kk: vv for kk, vv in argb(a, k).items() if vv is not None}
            ev = {"ev": name, "args": args, "ret": None}
            if isinstance(r, Raise):
                ev["ret"] = r.spec
                trace.append(ev)
                raise r.exc
            ev["ret"] = norm(r)
            trace.append(ev)
            return make(r) if make else r
        return call
    return rec


def kw_argb(k):
    return {kk: norm(vv) for kk, vv in k.items() if vv is not None}


def logger_double(rec):
    def meth(level):
        def log(msg, *args):
            return rec("logger." + level,
                       lambda a, k: {"msg": a[0],
                                     "args": [exc_json(x) for x in a[1:]]})(msg, *args)
        return log
    return types.SimpleNamespace(error=meth("error"), warning=meth("warning"),
                                 info=meth("info"), debug=meth("debug"))


def ns_obj(r):
    return types.SimpleNamespace(**r) if isinstance(r, dict) else r


def minus_io(work):
    work.pop("io", None)
    return work


# ── per-case sys.modules installation ───────────────────────────────────────

STUB_NAMES = (
    "cron.jobs", "cron.scheduler", "cron.executions", "cron.scheduler_preflight",
    "cron.jobs_time", "hermes_constants", "gateway.run", "gateway.session_context",
    "utils", "tools.approval_context", "tools.async_delegation",
    "tools.environments.base", "agent.secret_scope", "httpx", "hermes_cli.cron",
    "hermes_time",
)


def install_module(name, attrs=None, getattr_fn=None):
    mod = types.ModuleType(name)
    for k, v in (attrs or {}).items():
        setattr(mod, k, v)
    if getattr_fn is not None:
        mod.__getattr__ = getattr_fn
    sys.modules[name] = mod
    parent, _, child = name.rpartition(".")
    if parent:
        setattr(pkg(parent), child, mod)
    return mod


def iso_make(r):
    if isinstance(r, str):
        return _dtm.datetime.fromisoformat(r)
    return r

def drop_none(d):
    return {k: v for k, v in d.items() if v is not None}


def cron_dir_getattr(rec, PD):
    def ga(name):
        if name != "CRON_DIR":
            raise AttributeError(name)
        return rec("cron_dir", lambda a, k: {},
                   make=lambda r: PD(r) if isinstance(r, str) else r)()
    return ga


def restore_real():
    for k, v in REAL_JA.items():
        NS_JA[k] = v
    for k, v in REAL_CT.items():
        NS_CT[k] = v
    for k, v in REAL_HC.items():
        NS_HC[k] = v
    NS_CT["logger"] = SILENT_LOG
    NS_HC["logger"] = SILENT_LOG
    NS_CT["time"] = REAL_TIME
    NS_CT["cronjob"] = lambda **k: None
    NS_CT["_relay_fronted_delivery_platforms"] = lambda job: set()
    NS_CT["_api_server_base_url"] = lambda: ""
    NS_CT["_get_max_async_children"] = lambda: 3
    for nm in ("claim_job_for_fire", "get_job", "is_job_runnable", "mark_job_run"):
        NS_CT[nm] = lambda *a, **k: None
    NS_HC["Path"] = Path
    NS_HC["color"] = lambda text, *codes: text


# ── per-function drivers ────────────────────────────────────────────────────

def d_split_monitor_arg(work, trace, script):
    return NS_JA["_split_monitor_arg"](
        work.get("monitor"), work.get("monitor_script"), work.get("monitor_url"))


def d_mode_guidance_notes(work, trace, script):
    return NS_JA["_mode_guidance_notes"](work.get("job"), work.get("user_deliver"))


def d_manual_run_delivery_note(work, trace, script):
    return NS_CT["_manual_run_delivery_note"](work.get("deliver"), work.get("refreshed"))


def d_gateway_liveness_notice(work, trace, script):
    rec = make_rec(script, trace)
    install_module("hermes_cli.cron", {
        "_builtin_gateway_liveness": rec("builtin_gateway_liveness", lambda a, k: {})})
    return NS_JA["_gateway_liveness_notice"](work.get("plural", False))


def d_resolve_cron_context_deliver(work, trace, script):
    rec = make_rec(script, trace)
    install_module("gateway.session_context", {
        "get_session_env": rec("get_session_env",
                               lambda a, k: {"name": a[0],
                                             "default": a[1] if len(a) > 1 else ""})})
    install_module("utils", {"is_truthy_value": NS_UE["is_truthy_value"]})
    return NS_JA["_resolve_cron_context_deliver"](work.get("deliver"))


def d_forward_relay_fronted_run(work, trace, script):
    rec = make_rec(script, trace)
    NS_CT["_relay_fronted_delivery_platforms"] = rec(
        "relay_fronted_delivery_platforms", lambda a, k: {"job": a[0]},
        make=lambda r: set(r) if isinstance(r, list) else r)
    NS_CT["_api_server_base_url"] = rec("api_server_base_url", lambda a, k: {})
    install_module("agent.secret_scope", {
        "get_secret": rec("get_secret",
                          lambda a, k: {"name": a[0],
                                        "default": a[1] if len(a) > 1 else None})})
    install_module("httpx", {
        "post": rec("httpx_post",
                    lambda a, k: {"url": a[0], "headers": k.get("headers"),
                                  "json": k.get("json"), "timeout": k.get("timeout")},
                    make=ns_obj)})
    return NS_CT["_forward_relay_fronted_run"](
        work.get("job"), extra_prompt=work.get("extra_prompt"))


def d_latest_job_output_excerpt(work, trace, script):
    rec = make_rec(script, trace)
    PD = make_path_double(rec)
    install_module("cron.jobs", {
        "get_cron_output_dir": rec("get_cron_output_dir", lambda a, k: {},
                                   make=lambda r: PD(r) if isinstance(r, str) else r)})
    return NS_CT["_latest_job_output_excerpt"](
        work.get("job_id"), work.get("max_chars", 2000))


def d__cronjob_handler(work, trace, script):
    rec = make_rec(script, trace)
    NS_CT["cronjob"] = rec("cronjob", lambda a, k: kw_argb(k))
    return NS_CT["_cronjob_handler"](work.get("args"), **(work.get("kw") or {}))


def d_run_claimed_job(work, trace, script):
    rec = make_rec(script, trace)
    NS_CT["logger"] = logger_double(rec)
    NS_CT["get_job"] = rec("get_job", lambda a, k: {"job_id": a[0]})
    NS_CT["mark_job_run"] = rec(
        "mark_job_run",
        lambda a, k: {"job_id": a[0], "success": a[1],
                      "error": a[2] if len(a) > 2 else None,
                      "expected_fire_owner": k.get("expected_fire_owner")})
    install_module("cron.scheduler", {
        "try_register_running_job": rec(
            "try_register_running_job",
            lambda a, k: {"job_id": a[0], "owner": "owner:1", "future": "future:1"}),
        "release_running_job": rec(
            "release_running_job", lambda a, k: {"job_id": a[0], "owner": "owner:1"}),
        "run_one_job": rec(
            "run_one_job",
            lambda a, k: {"job": a[0], "adapters": k.get("adapters"),
                          "loop": k.get("loop"), "extra_prompt": k.get("extra_prompt")}),
    })
    install_module("cron.executions", {
        "get_execution": rec("get_execution", lambda a, k: {"execution_id": a[0]})})
    install_module("hermes_constants", {
        "get_hermes_home": rec("get_hermes_home", lambda a, k: {}),
        "profile_name_for_home": rec("profile_name_for_home",
                                     lambda a, k: {"home": a[0]}),
    })
    install_module("cron.scheduler_preflight", {
        "shared_route_adapters_for_current_home": rec(
            "shared_route_adapters", lambda a, k: {"adapters": a[0]})})
    install_module("tools.environments.base", {
        "get_activity_callback": rec("get_activity_callback", lambda a, k: {})})

    def build_runner(r):
        if not isinstance(r, dict):
            return r
        ns = types.SimpleNamespace(adapters=r.get("adapters"),
                                   _gateway_loop=r.get("gateway_loop"))
        if r.get("has_adapters_for_profile"):
            ns._adapters_for_profile = rec("adapters_for_profile",
                                           lambda a, k: {"profile": a[0]})
        if r.get("has_is_shared_bot_satellite"):
            ns._is_shared_bot_satellite = rec("is_shared_bot_satellite",
                                              lambda a, k: {"profile": a[0]})
        return ns

    rm = work.get("runner")
    if isinstance(rm, dict) and rm.get("ref_callable"):
        install_module("gateway.run", {
            "_gateway_runner_ref": rec("runner_ref_call", lambda a, k: {},
                                       make=build_runner)})
    elif isinstance(rm, dict):
        install_module("gateway.run", {"_gateway_runner_ref": object()})
    else:
        sys.modules.pop("gateway.run", None)
    return NS_CT["_run_claimed_job"](work.get("job"),
                                     extra_prompt=work.get("extra_prompt"))


def d_try_dispatch_background_run(work, trace, script):
    rec = make_rec(script, trace)
    NS_CT["logger"] = logger_double(rec)
    NS_CT["_reap_stale_executions"] = rec("reap_stale_executions",
                                          lambda a, k: {"job_name": a[0]})
    NS_CT["_run_claimed_job"] = rec(
        "run_claimed_job",
        lambda a, k: {"job": a[0], "extra_prompt": k.get("extra_prompt")})
    NS_CT["_get_max_async_children"] = rec("get_max_async_children", lambda a, k: {})
    NS_CT["time"] = types.SimpleNamespace(
        time=rec("time.time", lambda a, k: {}), monotonic=_time_mod.monotonic)
    NS_CT["claim_job_for_fire"] = rec(
        "claim_job_for_fire",
        lambda a, k: {"job_id": a[0], "manual": k.get("manual"),
                      "return_job": k.get("return_job")})
    NS_CT["get_job"] = rec("get_job", lambda a, k: {"job_id": a[0]})
    NS_CT["is_job_runnable"] = rec("is_job_runnable", lambda a, k: {"job": a[0]})
    NS_CT["mark_job_run"] = rec(
        "mark_job_run",
        lambda a, k: {"job_id": a[0], "success": a[1],
                      "error": a[2] if len(a) > 2 else None})
    install_module("gateway.session_context", {
        "async_delivery_supported": rec("async_delivery_supported", lambda a, k: {}),
        "get_session_env": rec("get_session_env",
                               lambda a, k: {"name": a[0],
                                             "default": a[1] if len(a) > 1 else ""}),
    })
    install_module("tools.approval_context", {
        "get_current_session_key": rec("get_current_session_key",
                                       lambda a, k: {"default": k.get("default")})})
    install_module("cron.scheduler", {
        "is_job_running": rec("is_job_running", lambda a, k: {"job_id": a[0]}),
        "_normalize_deliver_value": NS_SD["_normalize_deliver_value"],
    })
    install_module("tools.async_delegation", {
        "_current_origin_session_id": rec("current_origin_session_id", lambda a, k: {}),
        "dispatch_async_delegation": rec(
            "dispatch_async_delegation",
            lambda a, k: {kk: ("runner:1" if kk == "runner" else vv)
                          for kk, vv in k.items()}),
    })
    return NS_CT["_try_dispatch_background_run"](
        work.get("job"), session_id=work.get("session_id"),
        extra_prompt=work.get("extra_prompt"))


def d_cron_doctor_issues_for_job(work, trace, script):
    rec = make_rec(script, trace)
    PD = make_path_double(rec)
    NS_HC["Path"] = checked_path_ctor(PD)
    install_module("cron.jobs_time", {
        "zone_for_job": rec("zone_for_job", lambda a, k: {"job": a[0]}),
        "normalize_grace_minutes": rec("normalize_grace_minutes",
                                       lambda a, k: {"value": a[0]}),
    })
    install_module("cron.jobs",
                   {"_parse_aware": rec("parse_aware", lambda a, k: {"value": a[0]},
                                        make=iso_make)},
                   getattr_fn=cron_dir_getattr(rec, PD))
    install_module("hermes_time", {"now": rec("now", lambda a, k: {}, make=iso_make)})
    return NS_HC["_cron_doctor_issues_for_job"](work.get("job"))


def d_dispatch_display(work, trace, script):
    rec = make_rec(script, trace)
    NS_HC["color"] = rec("color", lambda a, k: {"text": a[0], "codes": list(a[1:])})
    NS_HC["Colors"] = Colors
    return NS_HC["_dispatch_display"](work.get("dispatch"))


def d_script_health_issue(work, trace, script):
    rec = make_rec(script, trace)
    PD = make_path_double(rec)
    NS_HC["Path"] = checked_path_ctor(PD)
    install_module("cron.jobs", {}, getattr_fn=cron_dir_getattr(rec, PD))
    return NS_HC["_script_health_issue"](work.get("script"))


def d_format_lateness(work, trace, script):
    return NS_HC["_format_lateness"](work.get("seconds"))


def d_next_run_overdue_issue(work, trace, script):
    rec = make_rec(script, trace)
    install_module("cron.jobs",
                   {"_parse_aware": rec("parse_aware", lambda a, k: {"value": a[0]},
                                        make=iso_make)})
    install_module("hermes_time", {"now": rec("now", lambda a, k: {}, make=iso_make)})
    return NS_HC["_next_run_overdue_issue"](work.get("next_run"))


def d_scripts_dir_for_cron(work, trace, script):
    rec = make_rec(script, trace)
    PD = make_path_double(rec)
    install_module("cron.jobs", {}, getattr_fn=cron_dir_getattr(rec, PD))
    return NS_HC["_scripts_dir_for_cron"]()


DRIVERS = {
    "run_claimed_job": d_run_claimed_job,
    "try_dispatch_background_run": d_try_dispatch_background_run,
    "mode_guidance_notes": d_mode_guidance_notes,
    "resolve_cron_context_deliver": d_resolve_cron_context_deliver,
    "forward_relay_fronted_run": d_forward_relay_fronted_run,
    "latest_job_output_excerpt": d_latest_job_output_excerpt,
    "manual_run_delivery_note": d_manual_run_delivery_note,
    "_cronjob_handler": d__cronjob_handler,
    "split_monitor_arg": d_split_monitor_arg,
    "gateway_liveness_notice": d_gateway_liveness_notice,
    "cron_doctor_issues_for_job": d_cron_doctor_issues_for_job,
    "dispatch_display": d_dispatch_display,
    "script_health_issue": d_script_health_issue,
    "format_lateness": d_format_lateness,
    "next_run_overdue_issue": d_next_run_overdue_issue,
    "scripts_dir_for_cron": d_scripts_dir_for_cron,
}


def run_case(driver, inp):
    trace = []
    work = copy.deepcopy(inp)
    script = Script(work.get("io"))
    saved = {n: sys.modules.get(n, MISSING) for n in STUB_NAMES}
    for n in STUB_NAMES:
        sys.modules.pop(n, None)
    restore_real()
    try:
        ret = driver(work, trace, script)
        minus_io(work)
        return {"ret": norm(ret), "trace": norm(trace), "state": norm(work)}
    except BaseException as e:   # SystemExit included — fail closed per case
        spec = {"__raise__": type(e).__name__, "message": str(e)}
        minus_io(work)
        return {"ret": spec, "trace": norm(trace), "state": norm(work)}
    finally:
        for n, m in saved.items():
            if m is MISSING:
                sys.modules.pop(n, None)
            else:
                sys.modules[n] = m


def main():
    n = 0
    mismatches = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception as e:
            mismatches += 1
            print(f"BAD JSON from harness: {e}")
            continue
        n += 1
        func = rec.get("func", "")
        inp = rec.get("in") or {}
        got = rec.get("out")
        driver = DRIVERS.get(func)
        if driver is None:
            mismatches += 1
            print(f"UNKNOWN func {func!r}")
            continue
        if not isinstance(got, dict) or "error" in got:
            mismatches += 1
            print(f"HARNESS ERROR {func}: {got}")
            continue
        try:
            want = run_case(driver, inp)
        except BaseException as e:
            mismatches += 1
            print(f"ORACLE DRIVER FAILURE {func}: {type(e).__name__}: {e}")
            continue
        if got != want:
            mismatches += 1
            if shown < 8:
                shown += 1
                print(f"MISMATCH {func} case#{n}")
                for key in ("ret", "trace", "state"):
                    if got.get(key) != want.get(key):
                        print(f"  {key} C  : {json.dumps(got.get(key), ensure_ascii=False)[:600]}")
                        print(f"  {key} PY : {json.dumps(want.get(key), ensure_ascii=False)[:600]}")
    print(f"ORACLE rest_revive2: {n} cases, {mismatches} mismatches")
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()
