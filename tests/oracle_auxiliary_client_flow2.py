#!/usr/bin/env python3
"""Oracle for the agent/auxiliary_client.py UNCLAIMED-remainder FLOW port
(src/agent/auxiliary_client_flow2.c).

Drives the LIVE auxiliary_client.py defs with SCRIPTED seams: every collaborator
(claimed siblings, foreign-module imports, config/registry/pool/clock reads) is a
recording stub fed the same scripts the C harness feeds ac2_call. Compares the
DECISION SEQUENCE (ordered "trace" of {ev,args,ret} seam events), the canonical
return ("ret") and the post-call out-state ("state"). The ported functions call
each other REAL (AST-extracted into one namespace), so internal composition
(_endpoint_default_headers -> _route_client_headers/_profile_default_headers,
_route_or_warn -> _route_client, _notify_aux_provider_response ->
_notify_aux_timing_response -> _tick_hook, _borrowed_main_credential_key ->
_read_main_api_key_if_same_origin, _vision_auto_route -> _finalize_vision_client)
is exercised, matching the C port's internal transcription.

Live defs are AST-extracted from the quarry (annotations kept as strings via the
__future__ flag); only the needed module constants are seeded. The heavy quarry
import chain is never imported. Every exec namespace is a registered sys.modules
module (py3.14 dataclasses/NamedTuple _is_type needs a real module alias).
Fail-closed: 0 cases => exit 1; any harness/oracle exception per case rides as
{"__raise__": cls} (BaseException, so SystemExit/KeyboardInterrupt propagate).
Summary: 'ORACLE auxiliary_client_flow2: N cases, M mismatches'.

Representation contract (both sides, see src/agent/auxiliary_client_flow2.c):
- object doubles (pool/profile/client/refreshed) are attribute namespaces;
  tuple/set -> array (sets sorted); None -> null; integral floats collapse to
  ints; Path -> str; uncaught exception -> {"__raise__": cls}; an exception
  passed as a logger arg projects to {"__raise__": cls, "msg": str(e)}.
- scripts: absent key -> None; {"$seq": [...]} per-call queue (exhaustion raises
  RuntimeError("script exhausted: <name>")); {"__raise__": cls, "msg": m} raises
  (trace ret is the WHOLE marker); any other value reused. None-valued arg keys
  dropped on both sides.
- object-valued seam results are null or a NON-EMPTY record (empty {} is outside
  the live domain for truthiness-gated pool getters); profile {} is used only for
  the `is not None` -> .api_mode AttributeError branch.
"""
import __future__
import ast
import builtins
import contextlib
import json
import logging
import os
import sys
import threading
import types
from pathlib import Path, PurePath, PurePosixPath
from typing import Any, Callable, NamedTuple, Optional

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "auxiliary_client.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "auxiliary_client.py")

NAME = "auxiliary_client_flow2"

PORTED = [
    "_tick_hook", "_notify_aux_dispatch", "_notify_aux_timing_response",
    "_notify_aux_provider_response", "_ai_gateway_headers", "_auth_json_path",
    "_load_pool_with_credentials", "_close_quietly", "_attempt_stream_socket",
    "_route_client_headers", "_profile_default_headers", "_endpoint_default_headers",
    "_codex_base_url_override", "_profile_declared_messages_wire", "_route_client",
    "_route_or_warn", "_resolve_openrouter_branch", "_resolve_nous_branch",
    "_resolve_xai_oauth_branch", "_resolve_azure_foundry_branch",
    "_resolve_openai_codex_branch", "_discovery_chain_allowed", "_log_once_debug",
    "_warn_stale_openai_base_url", "_is_actual_auxiliary_route", "_main_route_target",
    "_read_main_field", "_read_main_api_key_if_same_origin",
    "_borrowed_main_credential_key", "_cached_semaphore", "_current_event_loop",
    "_async_client_streams_internally", "_refresh_codex_credentials",
    "_refresh_nous_credentials", "_refresh_anthropic_credentials",
    "_refresh_xai_oauth_credentials", "_refresh_vertex_credentials",
    "_deepinfra_strict_vision_backend", "_finalize_vision_client", "_vision_auto_route",
]
CLASSES = ["_ResolveRequest"]
CONSTANTS = ["_PROVIDER_VISION_MODELS", "_VISION_AUTO_PROVIDER_ORDER"]
WANTED = set(PORTED)

logging.disable(logging.CRITICAL)

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
CLASS_CODE = {}
CONST_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node, SRC)
    elif isinstance(_node, ast.ClassDef) and _node.name in CLASSES:
        CLASS_CODE[_node.name] = _compile_node(_node, SRC)
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(tg, ast.Name) and tg.id in CONSTANTS:
            CONST_CODE[tg.id] = _compile_node(_node, SRC)
_missing = (WANTED - set(DEF_CODE)) | (set(CLASSES) - set(CLASS_CODE)) | (set(CONSTANTS) - set(CONST_CODE))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)


def _extract_const(name):
    ns = {"__builtins__": builtins}
    exec(CONST_CODE[name], ns)
    return ns[name]


_PROVIDER_VISION_MODELS = _extract_const("_PROVIDER_VISION_MODELS")
_VISION_AUTO_PROVIDER_ORDER = _extract_const("_VISION_AUTO_PROVIDER_ORDER")

# ── canonical projection ────────────────────────────────────────────────────


def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, PurePath):
        return str(v)
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    return cls(msg)


# ── scripted seams ──────────────────────────────────────────────────────────
_SCRIPTS = None


class Scripts:
    def __init__(self, io, trace):
        self.io = io or {}
        self.trace = trace
        self.cursors = {}

    def next(self, name):
        script = self.io.get(name)
        if isinstance(script, dict) and "$seq" in script:
            seq = script["$seq"]
            i = self.cursors.get(name, 0)
            if i >= len(seq):
                return {"__raise__": "RuntimeError", "msg": f"script exhausted: {name}"}
            self.cursors[name] = i + 1
            return seq[i]
        return script


# ── object doubles / result materialization ─────────────────────────────────


def _rec(name, argmap):
    def f(*a, **kw):
        return record(name, argmap(a, kw))
    return f


def mk_pool(d):
    if not isinstance(d, dict):
        return d
    ns = types.SimpleNamespace(**d)
    ns.has_credentials = _rec("pool.has_credentials", lambda a, k: {})
    ns.entry_id_for_api_key = _rec("pool.entry_id_for_api_key", lambda a, k: {"api_key": a[0]})
    ns.try_refresh_matching = _rec("pool.try_refresh_matching", lambda a, k: {"api_key_hint": k["api_key_hint"]})
    ns.select = _rec("pool.select", lambda a, k: {})
    ns.try_refresh_current = _rec("pool.try_refresh_current", lambda a, k: {})
    return ns


def mk_client(d):
    if isinstance(d, dict):
        return types.SimpleNamespace(**d)
    return d


CLIENT_SINGLE = {"_create_openai_client", "_maybe_wrap_anthropic"}
CLIENT_TUPLE = {"_try_openrouter", "_try_nous", "_try_azure_foundry",
                "_build_xai_oauth_aux_client", "_build_codex_client", "_to_async_client",
                "resolve_provider_client", "_resolve_strict_vision_backend",
                "_vision_main_provider_client"}
PLAIN_TUPLE = {"_resolve_codex_credential_and_base", "get_vertex_config",
               "_resolve_moa_aggregator", "base_url_origin"}


def materialize(name, script):
    if script is None:
        return None
    if name == "load_pool":
        return mk_pool(script)
    if name == "get_provider_profile":
        return types.SimpleNamespace(**script) if isinstance(script, dict) else script
    if name == "pool.try_refresh_current":
        return types.SimpleNamespace(**script) if isinstance(script, dict) else script
    if name == "read_claude_code_credentials":
        return script  # keep dict (isinstance dict check)
    if name in CLIENT_SINGLE:
        return mk_client(script)
    if name in CLIENT_TUPLE:
        if isinstance(script, (list, tuple)):
            return tuple([mk_client(script[0])] + list(script[1:]))
        return script
    if name in PLAIN_TUPLE:
        return tuple(script) if isinstance(script, (list, tuple)) else script
    return script


# ── seam arg projections ────────────────────────────────────────────────────
SEAM_ARGS = {
    "hook": lambda a, k: {},
    "_notify_aux_progress": lambda a, k: {},
    "hermes_user_agent": lambda a, k: {"product": a[0]},
    "_auth_file_path": lambda a, k: {},
    "load_pool": lambda a, k: {"provider": a[0]},
    "pool.has_credentials": lambda a, k: {},
    "pool.entry_id_for_api_key": lambda a, k: {"api_key": a[0]},
    "pool.try_refresh_matching": lambda a, k: {"api_key_hint": k["api_key_hint"]},
    "pool.select": lambda a, k: {},
    "pool.try_refresh_current": lambda a, k: {},
    "close": lambda a, k: {},
    "_socket_from_stream": lambda a, k: {"network_stream": a[0]},
    "route_answer": lambda a, k: {"kind": a[0], "provider": k["provider"],
                                  "base_url": k["base_url"], "has_images": k["has_images"]},
    "get_provider_profile": lambda a, k: {"provider": a[0]},
    "base_url_host_matches": lambda a, k: {"base_url": a[0], "host": a[1]},
    "build_nvidia_nim_headers": lambda a, k: {"base_url": a[0]},
    "hermes_xai_default_headers": lambda a, k: {},
    "_apply_user_default_headers": lambda a, k: {"headers": a[0]},
    "_scoped_key_env": lambda a, k: {"name": a[0]},
    "asyncio.get_event_loop": lambda a, k: {},
    "os.getenv": lambda a, k: {"name": a[0], "default": a[1]},
    "_to_async_client": lambda a, k: {"client": a[0], "model": a[1], "is_vision": k["is_vision"]},
    "_normalize_resolved_model": lambda a, k: {"model": a[0], "provider": a[1]},
    "_try_openrouter": lambda a, k: {"explicit_api_key": k.get("explicit_api_key"),
                                     "model": k.get("model"), "explicit_base_url": k.get("explicit_base_url")},
    "_describe_openrouter_unavailable": lambda a, k: {"model": k.get("model")},
    "_try_nous": lambda a, k: {"vision": k["vision"]},
    "nous_api_mode": lambda a, k: {"model": a[0]},
    "_maybe_wrap_anthropic": lambda a, k: {"client": a[0], "model": a[1], "api_key": a[2],
                                           "base_url": a[3], "api_mode": a[4]},
    "_build_xai_oauth_aux_client": lambda a, k: {"model": a[0]},
    "_try_azure_foundry": lambda a, k: {"model": k.get("model"), "explicit_api_key": k.get("explicit_api_key"),
                                        "explicit_base_url": k.get("explicit_base_url"), "api_mode": k.get("api_mode")},
    "_resolve_codex_credential_and_base": lambda a, k: {"model": a[0]},
    "_codex_cloudflare_headers": lambda a, k: {"token": a[0], "base_url": k["base_url"]},
    "_create_openai_client": lambda a, k: {"api_key": k.get("api_key"), "base_url": k.get("base_url"),
                                           "default_headers": k.get("default_headers")},
    "_build_codex_client": lambda a, k: {"model": a[0]},
    "is_actual_route": lambda a, k: {"provider": a[0], "base_url": a[1]},
    "_normalize_main_runtime": lambda a, k: {"main_runtime": a[0]},
    "normalize_actual_base_url": lambda a, k: {"base_url": a[0]},
    "normalize_route_base_url": lambda a, k: {"base_url": a[0]},
    "_read_main_provider": lambda a, k: {},
    "_read_main_model": lambda a, k: {},
    "_read_main_api_key": lambda a, k: {},
    "_read_main_base_url": lambda a, k: {},
    "_runtime_main_value": lambda a, k: {"field": a[0]},
    "load_config_readonly": lambda a, k: {},
    "load_config": lambda a, k: {},
    "base_url_origin": lambda a, k: {"base_url": a[0]},
    "_task_prefers_fast_model": lambda a, k: {"task": a[0]},
    "fast_catalog_provider": lambda a, k: {"runtime": a[0], "provider": a[1]},
    "_get_aux_model_for_provider": lambda a, k: {"provider": a[0], "prefer_fast": k["prefer_fast"]},
    "_resolve_moa_aggregator": lambda a, k: {"preset_name": a[0]},
    "_normalize_aux_provider": lambda a, k: {"provider": a[0]},
    "_normalize_api_key": lambda a, k: {"api_key": a[0]},
    "_to_openai_base_url": lambda a, k: {"base_url": a[0]},
    "declared_custom_credential_identity": lambda a, k: {"base_url": a[0], "provider": a[1]},
    "_runtime_cache_discriminator": lambda a, k: {"field": a[0], "value": a[1]},
    "factory": lambda a, k: {"limit": a[0]},
    "resolve_codex_runtime_credentials": lambda a, k: {"force_refresh": k["force_refresh"]},
    "resolve_nous_runtime_credentials": lambda a, k: {"timeout_seconds": k["timeout_seconds"],
                                                      "force_refresh": k["force_refresh"]},
    "resolve_xai_oauth_runtime_credentials": lambda a, k: {"force_refresh": k["force_refresh"]},
    "_creds_have_api_key": lambda a, k: {"creds": a[0]},
    "env_float": lambda a, k: {"name": a[0], "default": a[1]},
    "read_claude_code_credentials": lambda a, k: {},
    "_refresh_oauth_token": lambda a, k: {"creds": a[0]},
    "get_vertex_config": lambda a, k: {},
    "_resolve_provider_vision_default": lambda a, k: {"provider": a[0]},
    "resolve_provider_client": lambda a, k: {"provider": a[0], "model": a[1], "is_vision": k.get("is_vision")},
    "_resolve_strict_vision_backend": lambda a, k: {"provider": a[0], "model": a[1] if len(a) > 1 else None},
    "_vision_main_provider_client": lambda a, k: {"main_provider": a[0], "main_model": a[1],
                                                  "runtime": a[2], "resolved_model": a[3],
                                                  "resolved_api_mode": a[4]},
}


def record(name, args):
    scripts = _SCRIPTS
    args = {k: norm(v) for k, v in args.items() if v is not None}
    script = scripts.next(name)
    if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
        marker = dict(script)
        scripts.trace.append({"ev": name, "args": args, "ret": marker})
        raise make_exc(marker)
    result = materialize(name, script)
    scripts.trace.append({"ev": name, "args": args, "ret": norm(script)})
    return result


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **kw):
        return record(self.name, SEAM_ARGS[self.name](a, kw))


class LoggerDouble:
    def _log(self, level, msg, args, kw):
        d = {"msg": msg, "args": list(args)}
        if kw.get("exc_info"):
            d["exc_info"] = True
        return record("logger." + level, d)

    def debug(self, msg, *args, **kw):
        return self._log("debug", msg, args, kw)

    def warning(self, msg, *args, **kw):
        return self._log("warning", msg, args, kw)

    def info(self, msg, *args, **kw):
        return self._log("info", msg, args, kw)


_LOGGER_SINGLETON = LoggerDouble()


class _OsDouble:
    def getenv(self, name, default=None):
        return record("os.getenv", {"name": name, "default": default})


_OS_SINGLETON = _OsDouble()

# ── stub modules for lazy imports (singletons dispatch to current scripts) ──


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def install_stubs():
    agent = _mod("agent")
    agent.__path__ = []
    _mod("agent.provider_route_hooks", route_answer=SeamFn("route_answer"))
    _mod("agent.agent_runtime_helpers", _socket_from_stream=SeamFn("_socket_from_stream"))
    _mod("agent.anthropic_credentials",
         read_claude_code_credentials=SeamFn("read_claude_code_credentials"),
         _refresh_oauth_token=SeamFn("_refresh_oauth_token"))
    _mod("agent.vertex_adapter", get_vertex_config=SeamFn("get_vertex_config"))
    agent.provider_route_hooks = sys.modules["agent.provider_route_hooks"]
    agent.agent_runtime_helpers = sys.modules["agent.agent_runtime_helpers"]
    agent.anthropic_credentials = sys.modules["agent.anthropic_credentials"]
    agent.vertex_adapter = sys.modules["agent.vertex_adapter"]

    providers = _mod("providers", get_provider_profile=SeamFn("get_provider_profile"))

    hc = _mod("hermes_cli")
    hc.__path__ = []
    auth = _mod("hermes_cli.auth",
                _auth_file_path=SeamFn("_auth_file_path"),
                resolve_codex_runtime_credentials=SeamFn("resolve_codex_runtime_credentials"),
                resolve_nous_runtime_credentials=SeamFn("resolve_nous_runtime_credentials"),
                resolve_xai_oauth_runtime_credentials=SeamFn("resolve_xai_oauth_runtime_credentials"),
                normalize_actual_base_url=SeamFn("normalize_actual_base_url"))
    hp = _mod("hermes_cli.providers", is_actual_route=SeamFn("is_actual_route"),
              nous_api_mode=SeamFn("nous_api_mode"))
    ri = _mod("hermes_cli.route_identity", normalize_route_base_url=SeamFn("normalize_route_base_url"))
    cfg = _mod("hermes_cli.config", load_config_readonly=SeamFn("load_config_readonly"),
               load_config=SeamFn("load_config"))
    vi = _mod("hermes_cli.version_info", hermes_user_agent=SeamFn("hermes_user_agent"))
    hc.auth = auth
    hc.providers = hp
    hc.route_identity = ri
    hc.config = cfg
    hc.version_info = vi

    tools = _mod("tools")
    tools.__path__ = []
    xai = _mod("tools.xai_http", hermes_xai_default_headers=SeamFn("hermes_xai_default_headers"))
    tools.xai_http = xai

    _mod("asyncio", get_event_loop=SeamFn("asyncio.get_event_loop"))


# ── base namespace ──────────────────────────────────────────────────────────
_BASE_NS = {
    "__name__": "ac2_live_auxiliary_client",
    "__builtins__": builtins,
    "contextlib": contextlib,
    "threading": threading,
    "logging": logging,
    "logger": _LOGGER_SINGLETON,
    "os": _OS_SINGLETON,
    "json": json,
    "Any": Any, "Optional": Optional, "Callable": Callable, "NamedTuple": NamedTuple,
    "_PROVIDER_VISION_MODELS": _PROVIDER_VISION_MODELS,
    "_VISION_AUTO_PROVIDER_ORDER": _VISION_AUTO_PROVIDER_ORDER,
    "_aux_sem_lock": threading.Lock(),
    "_stale_base_url_warned": False,
}
_MOD = types.ModuleType("ac2_live_auxiliary_client")
_MOD.__dict__.update(_BASE_NS)
sys.modules["ac2_live_auxiliary_client"] = _MOD
_BASE_NS = _MOD.__dict__
for _nm in CLASSES:
    exec(CLASS_CODE[_nm], _BASE_NS)
for _nm, _code in DEF_CODE.items():
    exec(_code, _BASE_NS)
_BASE_NS["logger"] = _LOGGER_SINGLETON
_ResolveRequest = _BASE_NS["_ResolveRequest"]

# Install seam singletons for every collaborator the ported defs call.
_COLLABS = [n for n in SEAM_ARGS if n not in (
    "hook", "close", "factory", "pool.has_credentials", "pool.entry_id_for_api_key",
    "pool.try_refresh_matching", "pool.select", "pool.try_refresh_current",
    "os.getenv", "asyncio.get_event_loop", "logger.debug", "logger.warning", "logger.info",
    "load_config_readonly", "load_config", "route_answer", "get_provider_profile",
    "_auth_file_path", "_socket_from_stream", "hermes_xai_default_headers", "nous_api_mode",
    "is_actual_route", "normalize_actual_base_url", "normalize_route_base_url",
    "read_claude_code_credentials", "_refresh_oauth_token", "get_vertex_config",
    "resolve_codex_runtime_credentials", "resolve_nous_runtime_credentials",
    "resolve_xai_oauth_runtime_credentials", "hermes_user_agent",
)]
for _nm in _COLLABS:
    _BASE_NS[_nm] = SeamFn(_nm)
# top-level imported names that are also lazy-imported elsewhere
for _nm in ("load_pool", "base_url_host_matches", "base_url_origin", "env_float",
            "_to_openai_base_url", "_normalize_api_key", "declared_custom_credential_identity",
            "fast_catalog_provider", "hermes_user_agent"):
    _BASE_NS[_nm] = SeamFn(_nm)

install_stubs()


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    env = inp.get("env") or {}
    trace = []
    scripts = Scripts(inp.get("io") or {}, trace)
    _SCRIPTS = scripts
    # per-case import gates: `from hermes_cli import config` reads the parent's
    # attribute, so gate BOTH sys.modules and the parent attribute.
    hc = sys.modules["hermes_cli"]
    if env.get("config_import_fails"):
        sys.modules["hermes_cli.config"] = None
        hc.config = None
    else:
        cfg = sys.modules.get("hermes_cli.config")
        if cfg is None:
            cfg = _mod("hermes_cli.config", load_config_readonly=SeamFn("load_config_readonly"),
                       load_config=SeamFn("load_config"))
        hc.config = cfg
    # per-case constants / globals (PurePosixPath: str keeps POSIX separators)
    _BASE_NS["_stale_base_url_warned"] = env.get("warned", False)
    _BASE_NS["_AUTH_JSON_PATH"] = PurePosixPath(env.get("auth_json_path", "/default/auth.json"))
    _BASE_NS["_AUTH_JSON_PATH_AT_IMPORT"] = PurePosixPath(env.get("auth_json_path_at_import", "/default/auth.json"))
    return _BASE_NS, trace, scripts


# ── input doubles ───────────────────────────────────────────────────────────


def mk_local(d):
    d = d or {}
    ns = types.SimpleNamespace()
    if d.get("hook") is not None:
        ns.hook = _rec("hook", lambda a, k: {})
    return ns


def mk_target(d):
    d = d or {}
    ns = types.SimpleNamespace()
    if "close" in d:
        ns.close = None if d["close"] is None else _rec("close", lambda a, k: {})
    return ns


def mk_stream(d):
    d = d or {}
    ns = types.SimpleNamespace()
    if "response" in d and isinstance(d["response"], dict):
        resp = types.SimpleNamespace()
        if "extensions" in d["response"]:
            resp.extensions = d["response"]["extensions"]
        ns.response = resp
    return ns


# isinstance classes for _async_client_streams_internally
class AsyncCodexAuxiliaryClient:
    pass


class AsyncAnthropicAuxiliaryClient:
    pass


class AsyncBedrockAuxiliaryClient:
    pass


class CodexAuxiliaryClient:
    pass


class SubAsyncCodex(AsyncCodexAuxiliaryClient):
    pass


_KIND_MAP = {
    "AsyncCodexAuxiliaryClient": AsyncCodexAuxiliaryClient,
    "AsyncAnthropicAuxiliaryClient": AsyncAnthropicAuxiliaryClient,
    "AsyncBedrockAuxiliaryClient": AsyncBedrockAuxiliaryClient,
    "CodexAuxiliaryClient": CodexAuxiliaryClient,
    "SubAsyncCodex": SubAsyncCodex,
}
# the live isinstance() reads these as module globals
_BASE_NS["AsyncCodexAuxiliaryClient"] = AsyncCodexAuxiliaryClient
_BASE_NS["AsyncAnthropicAuxiliaryClient"] = AsyncAnthropicAuxiliaryClient
_BASE_NS["AsyncBedrockAuxiliaryClient"] = AsyncBedrockAuxiliaryClient


def mk_isinstance_client(d):
    if d is None:
        return None
    kind = d.get("kind") if isinstance(d, dict) else None
    cls = _KIND_MAP.get(kind)
    if cls is not None:
        return cls()
    return types.SimpleNamespace(**(d if isinstance(d, dict) else {}))


def mk_req(d):
    return _ResolveRequest(**(d or {}))


# ── live driver ─────────────────────────────────────────────────────────────


def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    if func == "_tick_hook":
        ns[func](mk_local(inp.get("local")), inp.get("label", ""))
        return None
    if func == "_notify_aux_dispatch":
        ns["_aux_dispatch"] = mk_local(inp.get("local"))
        ns[func]()
        return None
    if func == "_notify_aux_timing_response":
        ns["_aux_provider_response"] = mk_local(inp.get("local"))
        ns[func]()
        return None
    if func == "_notify_aux_provider_response":
        ns["_aux_provider_response"] = mk_local(inp.get("local"))
        ns[func]()
        return None
    if func == "_ai_gateway_headers":
        return ns[func]()
    if func == "_auth_json_path":
        return ns[func]()
    if func == "_load_pool_with_credentials":
        return ns[func](inp.get("provider", ""), inp.get("note", ""))
    if func == "_close_quietly":
        ns[func](mk_target(inp.get("target")), inp.get("failure_note"))
        return None
    if func == "_attempt_stream_socket":
        return ns[func](mk_stream(inp.get("stream")))
    if func == "_route_client_headers":
        return ns[func](inp.get("base_url", ""), inp.get("provider", ""),
                        is_vision=inp.get("is_vision", False))
    if func == "_profile_default_headers":
        return ns[func](inp.get("provider", ""))
    if func == "_endpoint_default_headers":
        return ns[func](inp.get("base_url", ""), inp.get("provider", ""),
                        is_vision=inp.get("is_vision", False), xai=inp.get("xai", False))
    if func == "_codex_base_url_override":
        return ns[func]()
    if func == "_profile_declared_messages_wire":
        return ns[func](inp.get("provider"))
    if func == "_route_client":
        return ns[func](mk_req(inp.get("req")), mk_client(inp.get("client_obj")),
                        inp.get("final_model_str"))
    if func == "_route_or_warn":
        return ns[func](mk_req(inp.get("req")), mk_client(inp.get("client")),
                        inp.get("default"), inp.get("unavailable_msg", ""), *(inp.get("args") or []))
    if func == "_resolve_openrouter_branch":
        return ns[func](mk_req(inp.get("req")))
    if func == "_resolve_nous_branch":
        return ns[func](mk_req(inp.get("req")))
    if func == "_resolve_xai_oauth_branch":
        return ns[func](mk_req(inp.get("req")))
    if func == "_resolve_azure_foundry_branch":
        return ns[func](mk_req(inp.get("req")))
    if func == "_resolve_openai_codex_branch":
        return ns[func](mk_req(inp.get("req")))
    if func == "_discovery_chain_allowed":
        return ns[func](inp.get("main_provider"), inp.get("task"))
    if func == "_log_once_debug":
        seen = set(inp.get("seen") or [])
        ns[func](seen, inp.get("key"), inp.get("msg", ""), *(inp.get("args") or []))
        holder["state"] = {"seen": norm(seen)}
        return None
    if func == "_warn_stale_openai_base_url":
        ns[func](inp.get("runtime_provider", ""))
        holder["state"] = {"warned": bool(ns["_stale_base_url_warned"])}
        return None
    if func == "_is_actual_auxiliary_route":
        return ns[func](mk_req(inp.get("req")), inp.get("base_url", ""))
    if func == "_main_route_target":
        return ns[func](inp.get("runtime"), inp.get("task"))
    if func == "_read_main_field":
        return ns[func](inp.get("field", ""), readonly=inp.get("readonly", False),
                        lower=inp.get("lower", False))
    if func == "_read_main_api_key_if_same_origin":
        return ns[func](inp.get("aux_base_url", ""))
    if func == "_borrowed_main_credential_key":
        return ns[func](inp.get("provider", ""), inp.get("base_url"), inp.get("api_key"),
                        inp.get("runtime") or {})
    if func == "_cached_semaphore":
        store = {k: (tuple(v) if isinstance(v, list) else v)
                 for k, v in (inp.get("store") or {}).items()}
        factory = _rec("factory", lambda a, k: {"limit": a[0]})
        ret = ns[func](store, inp.get("key"), inp.get("limit", 0), factory)
        holder["state"] = {"store": norm(store)}
        return ret
    if func == "_current_event_loop":
        return ns[func]()
    if func == "_async_client_streams_internally":
        return ns[func](mk_isinstance_client(inp.get("client")))
    if func == "_refresh_codex_credentials":
        return ns[func]()
    if func == "_refresh_nous_credentials":
        return ns[func]()
    if func == "_refresh_anthropic_credentials":
        return ns[func](inp.get("failed_api_key", ""))
    if func == "_refresh_xai_oauth_credentials":
        return ns[func]()
    if func == "_refresh_vertex_credentials":
        return ns[func]()
    if func == "_deepinfra_strict_vision_backend":
        return ns[func](inp.get("model"))
    if func == "_finalize_vision_client":
        return ns[func](inp.get("resolved_provider", ""), mk_client(inp.get("sync_client")),
                        inp.get("default_model"), inp.get("resolved_model"), inp.get("async_mode", False))
    if func == "_vision_auto_route":
        return ns[func](inp.get("runtime") or {}, inp.get("resolved_model"),
                        inp.get("resolved_api_mode"), inp.get("async_mode", False))
    raise KeyError(f"unknown func {func}")


def main():
    n = mism = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        holder = {}
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                ret = drive(rec, ns, scripts, holder)
                out = {"ret": norm(ret), "trace": trace, "state": holder.get("state", {})}
            except BaseException as e:  # noqa: BLE001 - fail-closed per case
                out = {"ret": {"__raise__": type(e).__name__}, "trace": trace,
                       "state": holder.get("state", {})}
        except BaseException as e:  # noqa: BLE001 - seeding failure is a mismatch
            out = {"ret": {"__raise__": type(e).__name__}, "trace": [], "state": {}}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:800]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:800]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


# ── self-test: drive every fixture, flag drive failures / suspicious raises ──


def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(60, exit=True)
    bad = 0
    names = set(PORTED)
    for i, rec in enumerate(fixtures):
        holder = {}
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                ret = drive(rec, ns, scripts, holder)
                spec = norm(ret)
            except BaseException as e:  # noqa: BLE001
                spec = {"__raise__": type(e).__name__}
            # flag AttributeError naming a ported method/seam (binding bug)
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                bad += 1
                print(f"SELFTEST case {i} {rec['func']}: AttributeError (check binding/materialization)")
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = Path(__file__).resolve().parent / "fixtures" / "auxiliary_client_flow2.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()