#!/usr/bin/env python3
"""Oracle for the tui_gateway/methods_config.py FLOW port
(src/gateway/tui_methods_config_flow.c).

Drives the LIVE methods_config bodies with SCRIPTED seams: every server global
(_err / _ok / _load_cfg / _display_cfg / _sessions / _hermes_home / _profile_db /
logger / the agent_factory + methods_projects + session_workdir + model_switch
helpers), every in-function import callable, and every callback arg is a
recording stub fed the same scripts the C harness feeds its universal seam.
Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace"), the final return ("ret") and the mutated state mirror ("state":
the readiness inflight pairs).

Live defs are AST-extracted from methods_config.py (module Assign/AnnAssign
constants seeded, plain-Name targets only, the heavy pool/registry machinery
skipped; UNDECORATED FunctionDefs exec'd with the future-annotations flag into
a registered sys.modules module -- py3.14). Decorated `def _` handlers are
extracted BARE (decorators stripped) under the _h_* names; the registry
decorators are driven separately through _projects_handler. The
_CONFIG_GETTERS table is exec'd AFTER the getters so the lambdas compose live.
The heavy quarry import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2; a missing/exhausted seam script
is a recorded RuntimeError raise on BOTH sides. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE tui_methods_config_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
  BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
  CancelledError) pass through every except-Exception site.
- `is False` / `is True` identity gates match ONLY the bool (0/1/null differ).
- Record doubles (wait_for_record scripts) are SimpleNamespaces with a
  failure_fields() method; canon projects them back to the script dict
  (failure_fields stripped). session dicts stay raw but their "agent" field
  becomes an attribute double.
- Readiness inflight: state.inflight is an ORDERED list of [key, future_id]
  pairs (key = [probe_key, profile]); models dict[tuple, Future] identity by
  id equality. _readiness_lock is silent (single-threaded).
- pool.submit is scripted; the submit double runs run_probe SYNCHRONOUSLY
  right after the pool_submit event (probe outcome stored on the future;
  future.result re-scripts it). add_done_callback does NOT invoke the callback
  (_readiness_cleared has its own cases). future.exception() scripts use the
  {"__raise__", "message"} VALUE shape, materialized as real exception
  instances and projected back by canon.
- _hermes_home is driven as PurePosixPath(args.hermes_home) so str() stays
  POSIX and `/` joins append a component. profile_db / connect_closing seams
  return the YIELDED value wrapped in a silent CM (null == yielded None).
- Tuples project to lists; sets to sorted lists; integral floats to ints;
  Paths to POSIX strings. Fixture numbers are ints except wait_seconds and
  int()-coercion inputs; unpack seams script 2-lists or null/int (never str).
  ASCII corpus only (strip ws set includes \\x1c-\\x1f).
- Seam args drop None-valued keys on both sides (absent-vs-null parity).
"""
import ast
import asyncio
import copy
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import Path, PurePosixPath

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "tui_gateway", "methods_config.py")

_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


HANDLERS = {
    "projects.discover_repos": "_h_projects_discover_repos",
    "projects.record_repos": "_h_projects_record_repos",
    "projects.tree": "_h_projects_tree",
    "projects.project_sessions": "_h_projects_project_sessions",
    "config.get": "_h_config_get",
    "setup.status": "_h_setup_status",
    "setup.runtime_check": "_h_setup_runtime_check",
    "diagnostics.share_nous": "_h_share_nous",
}

PORTED = [
    "_projects_handler", "_reconcile_repo_discovery", "_stamped_project_tree",
    "_display_raw", "_display_word", "_cfg_get_provider", "_cfg_get_project",
    "_cfg_get_personality", "_cfg_get_reasoning", "_cfg_get_fast",
    "_cfg_get_thinking_mode", "_cfg_get_mtime", "_readiness_cleared",
    "_readiness_share", "_readiness_check", "_redact_client_text",
    "_safe_client_label",
] + list(HANDLERS.values())

READINESS_FUNCS = {"_readiness_cleared", "_readiness_share", "_readiness_check",
                   "_h_setup_status", "_h_setup_runtime_check"}


# -- methods_config.py namespace (live defs + seeded constants) --
def extract():
    mod = new_module("tmc_oracle_methods_config")
    ns = mod.__dict__
    import concurrent.futures
    import threading
    ns.update({"concurrent": concurrent, "threading": threading, "sys": sys,
               "os": os, "json": json, "types": types, "copy": copy,
               "Path": Path, "PurePosixPath": PurePosixPath})
    # module-level machinery doubles (the real pool/atexit are never driven)
    ns["atexit"] = types.SimpleNamespace(register=lambda fn: None)
    ns["env_int"] = lambda name, default: default
    ns["HandlerRegistry"] = lambda: types.SimpleNamespace(
        method=lambda name: (lambda fn: fn), profile_scoped=lambda fn: fn)
    ns["bind_module"] = lambda *a, **k: None
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    skip = {"_registry", "method", "_profile_scoped", "_readiness_pool",
            "_readiness_inflight", "_CONFIG_GETTERS"}
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            if any(t.id in skip for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and not node.decorator_list:
            exec_node(src, node, ns, SRC)
    # decorated `def _` handlers: strip decorators, rename, exec the bare body
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_" and node.decorator_list:
            rpc = None
            for d in node.decorator_list:
                if isinstance(d, ast.Call) and d.args and isinstance(d.args[0], ast.Constant):
                    rpc = d.args[0].value
                    break
            if rpc not in HANDLERS:
                print(f"FATAL oracle_tui_methods_config_flow: unknown handler {rpc}")
                sys.exit(2)
            name = HANDLERS[rpc]
            text = "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])
            text = text.replace("def _(", f"def {name}(", 1)
            exec(compile(text, SRC, "exec", flags=_FLAGS, dont_inherit=True), ns)
    # the getter table AFTER the getters (lambdas capture globals lazily)
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "_CONFIG_GETTERS" for t in node.targets):
            exec_node(src, node, ns, SRC)
    # external constants (hermes_constants / server globals)
    ns["DEFAULT_INDICATOR_STYLE"] = "kaomoji"
    ns["INDICATOR_STYLES"] = ("ascii", "emoji", "kaomoji", "unicode")
    ns["_DETAIL_MODES"] = frozenset({"hidden", "collapsed", "expanded"})
    required = PORTED + ["_CONFIG_GETTERS", "_CONFIG_GET_ERR", "_THINKING_MODES",
                         "_READINESS_SHARE_WAIT_SECONDS", "_READINESS_JOIN_GRACE_SECONDS",
                         "_READINESS_STATUS_SHARE_WAIT_SECONDS", "_READINESS_IN_PROGRESS_ERR"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_tui_methods_config_flow: methods_config missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()


# -- exception materialization / projection --
EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "PermissionError": PermissionError, "OSError": OSError, "IndexError": IndexError,
    "TimeoutError": TimeoutError, "Exception": Exception, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "CancelledError": asyncio.CancelledError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    if kind == "KeyError":
        # str(KeyError(msg)) is repr(msg); the script contract is
        # message == str(exc), so materialize a KeyError whose str IS msg
        cls = type("KeyError", (KeyError,),
                   {"__str__": lambda self: self.args[0] if self.args else ""})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# -- canonical projection (compare parsed JSON, never raw strings) --
class SilentCM:
    """Silent context-manager double carrying its script marker."""
    def __init__(self, marker):
        self.__cm_marker__ = marker

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class YieldCM:
    """CM double whose __enter__ yields the script value (profile_db /
    connect_closing model the YIELDED handle; null == yielded None)."""
    def __init__(self, value):
        self.value = value

    def __enter__(self):
        return self.value

    def __exit__(self, *a):
        return False


def canon(v):
    def conv(x):
        if isinstance(x, BaseException):
            return proj_exc(x)
        if isinstance(x, SilentCM):
            return conv(x.__cm_marker__)
        if isinstance(x, types.SimpleNamespace):
            # record doubles carry a failure_fields method -- project back to
            # the script dict (the method is not data)
            return conv({k: val for k, val in vars(x).items()
                         if k != "failure_fields"})
        if isinstance(x, Path):
            return x.as_posix()
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# -- per-case script backend (mirrors the C tmc_call exactly) --
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        args = {k: v for k, v in (args or {}).items() if v is not None}
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        # C returns a fresh copy per call; live flows mutate seam results in
        # place, so the script entry must stay pristine.
        return copy.deepcopy(ret) if isinstance(ret, (dict, list)) else ret


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


# -- readiness doubles --
class FutureDouble:
    def __init__(self, ctx, marker, run_probe=None):
        self.ctx = ctx
        self.id = (marker.get("id") if isinstance(marker, dict) else None) or ""
        self._run_probe = run_probe
        self._probe_exc = None

    def add_done_callback(self, cb):
        # the callback is NOT invoked (modeling: _readiness_cleared own cases)
        return self.ctx.call("add_done_callback", {"id": self.id})

    def result(self, timeout=None):
        return self.ctx.call("future_result", {"id": self.id, "timeout": timeout})

    def cancelled(self):
        return self.ctx.call("future_cancelled", {"id": self.id})

    def exception(self):
        raw = self.ctx.call("future_exception", {"id": self.id})
        if isinstance(raw, dict) and "__raise__" in raw:
            return make_exc({"type": raw["__raise__"], "message": raw.get("message", "")})
        return raw


class PoolDouble:
    def __init__(self, ctx):
        self.ctx = ctx

    def submit(self, run_probe):
        raw = self.ctx.call("pool_submit", {})
        if not isinstance(raw, dict):
            return raw  # null/non-dict marker -> live AttributeError at use
        fut = FutureDouble(self.ctx, raw, run_probe)
        # SINGLE-THREADED MODEL: run the probe synchronously; the outcome is
        # stored on the future (future.result re-scripts it).
        try:
            run_probe()
        except BaseException as e:
            fut._probe_exc = e
        return fut


def record_build(raw, ctx):
    if not isinstance(raw, dict):
        return raw
    ns = types.SimpleNamespace(**raw)
    ns.failure_fields = rec(ctx, "failure_fields", lambda: {})
    return ns


def session_build(raw):
    if not isinstance(raw, dict):
        return raw
    out = dict(raw)
    if isinstance(out.get("agent"), dict):
        out["agent"] = types.SimpleNamespace(**out["agent"])
    return out


def cm_build(raw):
    return SilentCM(raw) if isinstance(raw, dict) else raw


def method_rec(ctx):
    def f(name):
        ctx.call("method", {"name": name})
        return lambda fn: fn
    return f


def profile_scoped_rec(ctx):
    def f(h):
        ctx.call("profile_scoped", {})
        return h
    return f

# -- in-function import stub modules (registered once, rebound per case) --
def stub_pkg(name):
    if name not in sys.modules:
        new_module(name)
    return sys.modules[name]


def stub_mod(full):
    parts = full.split(".")
    for i in range(1, len(parts)):
        stub_pkg(".".join(parts[:i]))
    mod = new_module(full) if full not in sys.modules else sys.modules[full]
    parent = sys.modules.get(".".join(parts[:-1]))
    if parent is not None:
        setattr(parent, parts[-1], mod)
    return mod


MODELS_MOD = stub_mod("hermes_cli.models")
PERSONALITY_MOD = stub_mod("hermes_cli.personality")
VOICE_MOD = stub_mod("agent.voice_turn_route")
CONST_MOD = stub_mod("hermes_constants")
CFGBACK_MOD = stub_mod("hermes_cli.config_backend")
PROFILES_MOD = stub_mod("hermes_cli.profiles")
ANON_MOD = stub_mod("hermes_cli.anon_auth")
MAIN_MOD = stub_mod("hermes_cli.main")
BOOTSTRAP_MOD = stub_mod("hermes_cli.free_tier_bootstrap")
RTPROV_MOD = stub_mod("hermes_cli.runtime_provider")
RTREADY_MOD = stub_mod("hermes_cli.runtime_readiness")
REDACT_MOD = stub_mod("hermes_cli.debug_redaction")
DEBUG_MOD = stub_mod("hermes_cli.debug")
UPLOAD_MOD = stub_mod("hermes_cli.diagnostics_upload")
PDB_MOD = stub_mod("hermes_cli.projects_db")
PTREE_MOD = stub_mod("tui_gateway.project_tree")
CONST_MOD.DEFAULT_INDICATOR_STYLE = "kaomoji"
CONST_MOD.INDICATOR_STYLES = ("ascii", "emoji", "kaomoji", "unicode")

# -- real defs restored before every case (seam slots are per-case) --
REAL = {nm: NS[nm] for nm in PORTED}


class InflightDict(dict):
    """dict[tuple, Future] double that records whether the flow touched it --
    the C port creates state.inflight lazily on the first lookup."""
    def __init__(self, *a, **k):
        super().__init__(*a, **k)
        self.touched = False

    def get(self, *a, **k):
        self.touched = True
        return super().get(*a, **k)

    def __setitem__(self, k, v):
        self.touched = True
        super().__setitem__(k, v)

    def pop(self, *a, **k):
        self.touched = True
        return super().pop(*a, **k)


def install_stubs(ctx, args, inflight):
    NS["_err"] = rec(ctx, "err",
                     lambda rid, code, msg, data=None:
                     {"rid": rid, "code": code, "message": msg, "data": data})
    NS["_ok"] = rec(ctx, "ok", lambda rid, result: {"rid": rid, "result": result})
    NS["_load_cfg"] = rec(ctx, "load_cfg", lambda: {})
    NS["_display_cfg"] = rec(ctx, "display_cfg", lambda: {})
    NS["_resolve_model"] = rec(ctx, "resolve_model", lambda: {})
    NS["_completion_cwd"] = rec(ctx, "completion_cwd", lambda params: {"params": params})
    NS["git_probe"] = types.SimpleNamespace(
        branch=rec(ctx, "git_branch", lambda cwd: {"cwd": cwd}))
    NS["_load_service_tier"] = rec(ctx, "load_service_tier", lambda: {})
    NS["_compute_mcp_rev"] = rec(ctx, "compute_mcp_rev", lambda: {})
    NS["_sessions"] = types.SimpleNamespace(
        get=rec(ctx, "sessions_get", lambda session_id: {"session_id": session_id},
                build=session_build))
    NS["_hermes_home"] = PurePosixPath(args.get("hermes_home", "/h/launch"))
    NS["_profile_db"] = rec(ctx, "profile_db", lambda params: {"params": params},
                            build=lambda raw: YieldCM(raw))
    NS["_repo_discovery_policy"] = rec(ctx, "repo_discovery_policy",
                                       lambda raw=None: {"raw": raw})
    NS["_repo_discovery_policy_key"] = rec(ctx, "repo_discovery_policy_key",
                                           lambda policy: {"policy": policy})
    NS["_repo_discovery_policy_is_default"] = rec(
        ctx, "repo_discovery_policy_is_default", lambda policy: {"policy": policy})
    NS["_scan_discovered_repos_remote"] = rec(
        ctx, "scan_discovered_repos_remote", lambda conn, policy: {"conn": conn, "policy": policy})
    NS["_discover_repos_payload"] = rec(
        ctx, "discover_repos_payload",
        lambda db, conn=None, include_cached=None:
        {"db": db, "conn": conn, "include_cached": include_cached})
    NS["_build_project_tree"] = rec(ctx, "build_project_tree",
                                    lambda db, **kwargs: {"db": db, **kwargs})
    NS["_response_profile_name"] = rec(ctx, "response_profile_name",
                                       lambda profile=None: {"profile": profile})
    NS["_profile_home"] = rec(ctx, "profile_home", lambda profile: {"profile": profile})
    NS["_session_profile_runtime_scope"] = rec(
        ctx, "session_profile_runtime_scope", lambda session: {"session": session},
        build=cm_build)
    NS["_resolve_startup_runtime"] = rec(ctx, "resolve_startup_runtime", lambda: {})
    NS["_resolve_agent_model_runtime"] = rec(
        ctx, "resolve_agent_model_runtime",
        lambda model_override, provider_override: {})
    NS["_load_busy_input_mode"] = rec(ctx, "load_busy_input_mode", lambda: {})
    NS["_load_approval_mode"] = rec(ctx, "load_approval_mode", lambda: {})
    NS["_coerce_statusbar"] = rec(ctx, "coerce_statusbar", lambda raw: {"raw": raw})
    NS["_load_tool_progress_mode"] = rec(ctx, "load_tool_progress_mode", lambda: {})
    NS["_display_mouse_tracking"] = rec(ctx, "display_mouse_tracking",
                                        lambda display: {"display": display})
    NS["logger"] = types.SimpleNamespace(
        debug=rec(ctx, "logger_debug", lambda fmt, *a: {"msg": fmt % a}),
        warning=rec(ctx, "logger_warning", lambda fmt, *a: {"msg": fmt % a}))
    NS["_readiness_pool"] = PoolDouble(ctx)
    NS["_readiness_inflight"] = inflight
    NS["method"] = method_rec(ctx)
    NS["_profile_scoped"] = profile_scoped_rec(ctx)
    MODELS_MOD.list_available_providers = rec(ctx, "list_available_providers", lambda: {})
    MODELS_MOD.normalize_provider = rec(ctx, "normalize_provider",
                                        lambda provider: {"provider": provider})
    PERSONALITY_MOD.active_personality_name = rec(
        ctx, "active_personality_name", lambda cfg: {"cfg": cfg})
    VOICE_MOD.session_runtime_view = rec(
        ctx, "session_runtime_view", lambda agent: {"agent": agent},
        build=lambda raw: types.SimpleNamespace(**raw) if isinstance(raw, dict) else raw)
    CONST_MOD.parse_reasoning_effort = rec(ctx, "parse_reasoning_effort",
                                           lambda raw: {"raw": raw})
    CONST_MOD.display_hermes_home = rec(ctx, "display_hermes_home", lambda: {})
    NS["_display_hermes_home"] = CONST_MOD.display_hermes_home
    CFGBACK_MOD.config_version_or_none = rec(
        ctx, "config_version_or_none",
        lambda path: {"path": path.as_posix() if isinstance(path, Path) else path})
    CFGBACK_MOD.version_token = rec(ctx, "version_token", lambda version: {"version": version})
    PROFILES_MOD.profile_exists = rec(ctx, "profile_exists",
                                      lambda profile: {"profile": profile})
    ANON_MOD.free_tier_route = rec(ctx, "free_tier_route", lambda: {})
    ANON_MOD.route_is_welcome_host = rec(ctx, "route_is_welcome_host",
                                         lambda base_url: {"base_url": base_url})
    MAIN_MOD._has_any_provider_configured = rec(
        ctx, "has_any_provider_configured",
        lambda strict_profile_scope: {"strict_profile_scope": strict_profile_scope})
    BOOTSTRAP_MOD.wait_for_record = rec(ctx, "wait_for_record", lambda: {},
                                        build=lambda raw: record_build(raw, ctx))
    RTPROV_MOD.resolve_runtime_provider = rec(
        ctx, "resolve_runtime_provider",
        lambda requested, target_model: {"requested": requested, "target_model": target_model})
    RTREADY_MOD.runtime_has_credentials = rec(
        ctx, "runtime_has_credentials", lambda runtime: {"runtime": runtime})
    RTREADY_MOD.missing_credentials_suffix = rec(
        ctx, "missing_credentials_suffix", lambda provider: {"provider": provider})
    RTREADY_MOD.readiness_failure = rec(
        ctx, "readiness_failure",
        lambda e: {"type": type(e).__name__, "message": str(e)})
    REDACT_MOD.redact_debug_support_text = rec(
        ctx, "redact_debug_support_text",
        lambda value, max_chars=None:
        {"text": value if isinstance(value, str) else str(value), "max_chars": max_chars})
    DEBUG_MOD.collect_share_bundle = rec(
        ctx, "collect_share_bundle",
        lambda log_lines, redact: {"log_lines": log_lines, "redact": redact})
    DEBUG_MOD.build_nous_bundle = rec(
        ctx, "build_nous_bundle", lambda bundle, redact: {"bundle": bundle, "redact": redact})
    UPLOAD_MOD.share_to_nous = rec(ctx, "share_to_nous", lambda bundle: {"bundle": bundle})
    PDB_MOD.connect_closing = rec(ctx, "connect_closing", lambda: {},
                                  build=lambda raw: YieldCM(raw))
    PDB_MOD.reconcile_discovered_repos_policy = rec(
        ctx, "reconcile_discovered_repos_policy",
        lambda conn, policy_key, preserve_unversioned:
        {"conn": conn, "policy_key": policy_key, "preserve_unversioned": preserve_unversioned})
    PDB_MOD.record_discovered_repos = rec(
        ctx, "record_discovered_repos",
        lambda conn, pairs, replace, policy_key:
        {"conn": conn, "pairs": pairs, "replace": replace, "policy_key": policy_key})
    PDB_MOD.clear_discovered_repos = rec(
        ctx, "clear_discovered_repos",
        lambda conn, policy_key: {"conn": conn, "policy_key": policy_key})
    PTREE_MOD.stamp_profile = rec(ctx, "stamp_profile",
                                  lambda projects, profile: {"projects": projects, "profile": profile})


def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    state_in = copy.deepcopy(inp.get("state") or {})
    for nm, real in REAL.items():
        NS[nm] = real
    # inflight pairs -> live dict[tuple, FutureDouble] (identity by id)
    inflight = InflightDict()
    for pair in state_in.get("inflight") or []:
        key, fid = tuple(pair[0]), pair[1]
        inflight[key] = FutureDouble(ctx, {"id": fid}, None) if fid is not None else None
    install_stubs(ctx, args, inflight)
    try:
        if func == "_display_raw":
            ret = NS[func]()
        elif func == "_display_word":
            ret = NS[func](args.get("key"), args.get("default"), set(args.get("allowed") or []))
        elif func == "_safe_client_label":
            ret = NS[func](args.get("label"))
        elif func == "_redact_client_text":
            ret = NS[func](args.get("text"), args.get("cap"))
        elif func in ("_cfg_get_provider", "_cfg_get_project", "_cfg_get_personality",
                      "_cfg_get_reasoning", "_cfg_get_fast", "_cfg_get_thinking_mode",
                      "_cfg_get_mtime"):
            ret = NS[func](args.get("params"))
        elif func == "_config_getter":
            ret = NS["_CONFIG_GETTERS"][args["key"]](args.get("params"))
        elif func == "_h_config_get":
            ret = NS[func](args.get("rid"), args.get("params"))
        elif func == "_reconcile_repo_discovery":
            pdb = types.SimpleNamespace(
                reconcile_discovered_repos_policy=rec(
                    ctx, "reconcile_discovered_repos_policy",
                    lambda conn, policy_key, preserve_unversioned:
                    {"conn": conn, "policy_key": policy_key,
                     "preserve_unversioned": preserve_unversioned}))
            ret = NS[func](pdb, args.get("conn"), args.get("policy"), args.get("policy_key"))
        elif func == "_stamped_project_tree":
            ret = NS[func](args.get("db"), args.get("params"),
                           preview_limit=args.get("preview_limit", 3),
                           hydrate=args.get("hydrate", False),
                           session_limit=args.get("session_limit", 2000),
                           include_discovered=args.get("include_discovered", True))
        elif func in ("_h_projects_discover_repos", "_h_projects_record_repos",
                      "_h_projects_tree", "_h_projects_project_sessions",
                      "_h_setup_status", "_h_setup_runtime_check", "_h_share_nous"):
            ret = NS[func](args.get("rid"), args.get("params"))
        elif func == "_readiness_cleared":
            key = tuple(args["key"])
            fid = args.get("future_id")
            held = inflight.get(key)
            future = held if (isinstance(held, FutureDouble) and held.id == fid) \
                else FutureDouble(ctx, {"id": fid}, None)
            ret = NS[func](key)(future)
        elif func == "_readiness_share":
            ret = NS[func](args.get("rid"), tuple(args["key"]),
                           rec(ctx, "run_probe", lambda: {}),
                           args.get("wait_seconds", 4.0))
        elif func == "_readiness_check":
            probe = rec(ctx, "probe",
                        lambda profile, scoped: {"profile": profile, "scoped": scoped})
            ret = NS[func](args.get("rid"), args.get("params"), probe,
                           probe_key=args.get("probe_key", "status"),
                           wait_seconds=args.get("wait_seconds", 4.0))
        elif func == "_projects_handler":
            fn = rec(ctx, "projects_fn", lambda rid, params: {"rid": rid, "params": params})
            handler = NS[func](args.get("name"))(fn)
            ret = handler(args.get("rid"), args.get("params"))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    if func in READINESS_FUNCS and inflight.touched:
        state_out = dict(state_in)
        state_out["inflight"] = [
            [list(k), (v.id if isinstance(v, FutureDouble) else None)]
            for k, v in inflight.items()]
    else:
        state_out = dict(state_in)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# -- three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) --
SEAM_NAMES = [
    "err", "ok", "load_cfg", "display_cfg", "resolve_model", "normalize_provider",
    "list_available_providers", "completion_cwd", "git_branch",
    "active_personality_name", "sessions_get", "session_runtime_view",
    "parse_reasoning_effort", "load_service_tier", "compute_mcp_rev",
    "config_version_or_none", "version_token", "display_hermes_home",
    "load_busy_input_mode", "load_approval_mode", "coerce_statusbar",
    "load_tool_progress_mode", "display_mouse_tracking",
    "repo_discovery_policy", "repo_discovery_policy_key",
    "repo_discovery_policy_is_default", "reconcile_discovered_repos_policy",
    "record_discovered_repos", "clear_discovered_repos",
    "scan_discovered_repos_remote", "discover_repos_payload",
    "profile_db", "connect_closing", "build_project_tree",
    "response_profile_name", "stamp_profile", "profile_exists", "profile_home",
    "session_profile_runtime_scope", "pool_submit", "add_done_callback",
    "future_result", "future_cancelled", "future_exception", "run_probe", "probe",
    "logger_warning", "logger_debug", "wait_for_record",
    "has_any_provider_configured", "free_tier_route", "failure_fields",
    "resolve_startup_runtime", "resolve_runtime_provider",
    "resolve_agent_model_runtime", "runtime_has_credentials",
    "missing_credentials_suffix", "route_is_welcome_host", "readiness_failure",
    "redact_debug_support_text", "collect_share_bundle", "build_nous_bundle",
    "share_to_nous", "projects_fn", "method", "profile_scoped",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "tui_methods_config_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_tui_methods_config_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    c_seams = set(re.findall(r'tmc_call\(ctx,\s*"([^"]+)"', csrc))
    unknown_c = c_seams - registry
    if unknown_c:
        print(f"FATAL oracle_tui_methods_config_flow: C seams outside registry: {sorted(unknown_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'rec\(\s*\n\s*ctx,\s*"([^"]+)"', osrc))
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_tui_methods_config_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "tui_methods_config_flow.jsonl"
    if fix.exists():
        fix_keys = set()
        fix_funcs = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_funcs.add(case.get("func"))
            inp = case.get("in") or {}
            fix_keys |= set(inp.get("io") or {})
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_tui_methods_config_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - (set(PORTED) | {"_config_getter"})
        if badf:
            print(f"FATAL oracle_tui_methods_config_flow: fixture funcs outside PORTED: {sorted(badf)}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 12:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")
        for k in sorted(keys)[:3]:
            print(f"    {k} want={json.dumps(ws.get(k), ensure_ascii=False)[:200]}")
            print(f"    {k} got ={json.dumps(gs.get(k), ensure_ascii=False)[:200]}")


def main():
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec_in = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec_in.get("func", "")
        inp = rec_in.get("in") or {}
        got = rec_in.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print(f"MISMATCH {func}: harness emitted no result: {got}")
            continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1
            print(f"MISMATCH {func}: oracle drive raised {type(exc).__name__}: {exc}")
            continue
        g = canon(got)
        if g != want:
            m += 1
            report_mismatch(func, want, g, shown)
            shown += 1
    print(f"ORACLE tui_methods_config_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "tui_methods_config_flow.jsonl"
    n = bad = 0
    counts = {}
    ported_set = set(PORTED) | set(SEAM_NAMES)
    for line in open(fix, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            out = drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
            continue
        r = out["ret"]
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            if not case.get("fail_closed"):
                bad += 1
                print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            # builtin-type attribute errors are legitimate live edges (null
            # handles etc.); the dead-stub family names a double/custom type
            builtin_types = ("'NoneType'", "'str'", "'int'", "'float'", "'bool'",
                             "'list'", "'dict'")
            if not msg.startswith(builtin_types) and \
                    any(f"'{nm}'" in msg for nm in ported_set):
                bad += 1
                print(f"SELFTEST ATTR FAMILY {func} case {n}: {msg}")
    # hand-computed spot checks (guard against uniform wrong answers)
    spot = drive("_display_word", {"args": {"key": "thinking_mode", "default": "",
                                            "allowed": ["collapsed", "truncated", "full"]},
                                   "io": {"load_cfg": {"display": {"thinking_mode": " FULL "}}}})
    assert spot["ret"] == "full", spot
    spot = drive("_safe_client_label", {"args": {"label": "../../etc pass"}})
    assert spot["ret"] == "etc pass", spot
    spot = drive("_cfg_get_thinking_mode",
                 {"args": {"params": {}},
                  "io": {"load_cfg": [{"display": {}}, {"display": {"details_mode": "expanded"}}]}})
    assert spot["ret"] == {"value": "full"}, spot
    spot = drive("_cfg_get_fast",
                 {"args": {"params": {"session_id": "s"}},
                  "io": {"sessions_get": {"agent": {"service_tier": "priority"}}}})
    assert spot["ret"] == {"value": "fast"}, spot
    spot = drive("_h_config_get",
                 {"args": {"rid": "r1", "params": {"key": "nope"}},
                  "io": {"err": {"jsonrpc": "2.0", "id": "r1",
                                 "error": {"code": 4002, "message": "unknown config key: nope"}}}})
    assert spot["trace"][0]["name"] == "err", spot
    spot = drive("_readiness_share",
                 {"args": {"rid": "r1", "key": ["status", ""], "wait_seconds": 12.0},
                  "io": {"pool_submit": {"id": "f1"}, "run_probe": None,
                         "add_done_callback": None,
                         "future_result": {"__script__": True,
                                           "raise": {"type": "TimeoutError", "message": ""}},
                         "logger_warning": None,
                         "err": {"jsonrpc": "2.0", "id": "r1",
                                 "error": {"code": 5097, "message": "readiness check still in progress; retrying next tick"}}}})
    assert spot["ret"]["error"]["code"] == 5097, spot
    assert spot["state"]["inflight"] == [[["status", ""], "f1"]], spot
    thin = [f for f in PORTED if counts.get(f, 0) < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN COVERAGE (<5 cases): {thin}")
    print(f"SELFTEST tui_methods_config_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
