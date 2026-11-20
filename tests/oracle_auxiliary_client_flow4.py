#!/usr/bin/env python3
"""Oracle for auxiliary_client_flow4 (UNCLAIMED deep async/client-builder remainder).

Reads harness JSONL on stdin, drives LIVE agent/auxiliary_client.py under the
same seam scripts, diffs {"ret","trace","state"}, prints
    ORACLE auxiliary_client_flow4: N cases, M mismatches

Ported targets (11 strict-PoP-unclaimed deep orchestrators):
  _named_custom_openai_wire_client, _api_key_profile_supplied_client,
  _build_vertex_client, _build_bedrock_client, _wrap_transport,
  _project_provider_profile, _resolve_call_client, _acreate_with_progress,
  _try_discovery_chain, _translate_anthropic_response_format,
  _pool_credential_digest.
All collaborators ride the injected universal seam; C scripts results and
records the ordered trace the oracle diffs against LIVE under the same scripts.
The FULL builtin exception hierarchy is mirrored in make_exc / exc family.
"""
import ast
import builtins
import contextlib
import __future__
import hashlib
import json
import math
import os
import sys
import types
from pathlib import Path
from typing import Any, Callable, Optional

os.environ.setdefault("HOME", str(Path.home()))
os.environ.setdefault("USERPROFILE", str(Path.home()))

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
SRC = str(QUARRY / "agent" / "auxiliary_client.py")
NAME = "auxiliary_client_flow4"

PORTED = [
    "_named_custom_openai_wire_client", "_api_key_profile_supplied_client",
    "_build_vertex_client", "_build_bedrock_client", "_wrap_transport",
    "_project_provider_profile", "_resolve_call_client", "_acreate_with_progress",
    "_try_discovery_chain", "_translate_anthropic_response_format",
    "_pool_credential_digest",
]
WANTED = set(PORTED)

_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, (ast.FunctionDef, ast.AsyncFunctionDef)) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node, SRC)
_missing = WANTED - set(DEF_CODE)
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)


# ── canonical projection ────────────────────────────────────────────────────
class SeamFn:
    """A callable double that records a seam and returns a scripted result."""
    def __init__(self, name, argmap=None):
        self.name = name
        self.argmap = argmap

    def __call__(self, *a, **kw):
        args = self.argmap(a, kw) if self.argmap else {"args": list(a)}
        return record(self.name, args)


class AsyncSeamFn(SeamFn):
    async def __call__(self, *a, **kw):
        return super().__call__(*a, **kw)


class ObjDouble:
    """Attribute carrier for a scripted object {field: value}."""
    def __init__(self, d, methods=None):
        self.__dict__.update(d or {})
        for k, v in (methods or {}).items():
            setattr(self, k, v)


def norm(v):
    if isinstance(v, SeamFn):
        return v.name
    if callable(v) and not isinstance(v, type):
        return getattr(v, "__name__", "<callable>")
    if isinstance(v, CodexAuxiliaryClient):
        return norm(getattr(v, "_real_client", None))
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, ObjDouble):
        return {k: norm(x) for k, x in vars(v).items() if not k.startswith("__sfn__")}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items()}
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    if cls_name in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError",
                    "BaseExceptionGroup", "ExceptionGroup"):
        # Attribute-derived str(); keep empty-message round-trip only.
        try:
            e = cls.__new__(cls)
        except Exception:
            e = object.__new__(cls)
        Exception.__init__(e, msg)
        return e
    return cls(msg)


# ── scripted universal seam ──────────────────────────────────────────────────
_SCRIPTS = None


class Scripts:
    def __init__(self, io, trace):
        self.io = io or {}
        self.trace = trace
        self.counters = {}

    def next(self, name):
        if name not in self.io:
            return None
        sc = self.io[name]
        if isinstance(sc, dict) and isinstance(sc.get("$seq"), list):
            i = self.counters.get(name, 0)
            seq = sc["$seq"]
            self.counters[name] = i + 1
            return seq[i] if i < len(seq) else {"__raise__": "RuntimeError",
                                                "msg": "script exhausted: " + name}
        return sc


def record(name, args):
    scripts = _SCRIPTS
    args = {k: norm(v) for k, v in args.items() if v is not None}
    script = scripts.next(name)
    if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
        marker = dict(script)
        scripts.trace.append({"ev": name, "args": args, "ret": marker})
        raise make_exc(marker)
    scripts.trace.append({"ev": name, "args": args, "ret": norm(script)})
    return materialize(name, script)


class LoggerDouble:
    def _log(self, level, msg, args, kw):
        d = {"msg": msg, "args": [norm(x) if not isinstance(x, BaseException)
                                  else {"__raise__": type(x).__name__, "msg": str(x)}
                                  for x in args]}
        if kw.get("exc_info"):
            d["exc_info"] = True
        return record("logger." + level, d)

    def debug(self, msg, *args, **kw):
        return self._log("debug", msg, args, kw)

    def warning(self, msg, *args, **kw):
        return self._log("warning", msg, args, kw)

    def info(self, msg, *args, **kw):
        return self._log("info", msg, args, kw)

    def exception(self, msg, *args, **kw):
        return self._log("error", msg, args, dict(kw, exc_info=True))


_LOGGER = LoggerDouble()


# ── module stubs + bare globals ─────────────────────────────────────────────
import collections
import base64

_PROFILE_FIELDS = ("body", "reasoning_extra", "top_level", "handles_reasoning", "messages_wire")
_ROUTE_FIELDS = ("client", "final_model", "resolved_provider", "effective_provider")
_REQ_FIELDS = ("provider", "original_provider", "model", "async_mode", "raw_codex",
              "explicit_base_url", "explicit_api_key", "api_mode", "main_runtime",
              "is_vision", "task", "pool_choice")
ProfileProjection = collections.namedtuple("_ProfileProjection", _PROFILE_FIELDS)
ResolvedAuxRoute = collections.namedtuple("_ResolvedAuxRoute", _ROUTE_FIELDS)
ResolveRequest = collections.namedtuple("_ResolveRequest", _REQ_FIELDS)
ProfileProjection.__new__.__defaults__ = (False,)
ResolveRequest.__new__.__defaults__ = (None,)


class AuxiliaryClientUnavailable(RuntimeError):
    pass


class _ClassifiedAttempt(Exception):
    def __init__(self, failure):
        self.failure = failure
        err = (failure.get("error", "") if isinstance(failure, dict)
               else getattr(failure, "error", ""))
        super().__init__(str(err))


_STREAM_REJECTED = object()


class _AuxErrors:
    def stream_negotiation_reraises(self, verdict):
        return SeamFn("stream_negotiation_reraises", lambda a, k: {"verdict": a[0]})(verdict)


class _ProviderProfileBase:
    def build_api_kwargs_extras(self, **kw):
        return SeamFn("build_api_kwargs_extras",
                      lambda a, k: {"reasoning_config": kw.get("reasoning_config"),
                                    "model": kw.get("model"), "base_url": kw.get("base_url")})(**kw)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _install_imports(gate):
    def ok(name):
        return gate.get(name, True)

    agent = _mod("agent")
    agent.__path__ = []
    # vertex
    if ok("vertex"):
        _mod("agent.vertex_adapter",
             get_vertex_config=SeamFn("get_vertex_config"),
             has_vertex_credentials=SeamFn("has_vertex_credentials"))
    else:
        sys.modules["agent.vertex_adapter"] = None
    agent.vertex_adapter = sys.modules.get("agent.vertex_adapter")
    # bedrock
    if ok("bedrock"):
        _mod("agent.bedrock_adapter",
             has_aws_credentials=SeamFn("has_aws_credentials"),
             is_anthropic_bedrock_model=SeamFn("is_anthropic_bedrock_model",
                                               lambda a, k: {"model": a[0]}),
             resolve_bedrock_runtime_region=SeamFn("resolve_bedrock_runtime_region"),
             bedrock_openai_uses_mantle=SeamFn("bedrock_openai_uses_mantle",
                                               lambda a, k: {"model": a[0]}),
             bedrock_openai_base_url=SeamFn("bedrock_openai_base_url",
                                            lambda a, k: {"region": a[0]}),
             resolve_bedrock_bearer_token=SeamFn("resolve_bedrock_bearer_token"),
             configure_bedrock_openai_client_kwargs=SeamFn(
                 "configure_bedrock_openai_client_kwargs", lambda a, k: {"kwargs": a[0]}))
        _mod("agent.anthropic_adapter",
             build_anthropic_bedrock_client=SeamFn("build_anthropic_bedrock_client",
                                                   lambda a, k: {"region": a[0]}))
    else:
        sys.modules["agent.bedrock_adapter"] = None
        sys.modules["agent.anthropic_adapter"] = None
    # opencode
    if ok("opencode"):
        _mod("agent.opencode_affinity",
             opencode_transport=SeamFn("opencode_transport",
                 lambda a, k: {"provider": a[0], "model": a[1], "base_url": a[2]}))
    else:
        sys.modules["agent.opencode_affinity"] = None
    # providers
    if ok("providers"):
        _mod("providers", get_provider_profile=SeamFn("get_provider_profile",
                                                     lambda a, k: {"provider": a[0]}))
        _mod("providers.base", ProviderProfile=_ProviderProfileBase)
    else:
        sys.modules["providers"] = None
        sys.modules["providers.base"] = None
    # command token source / credential pool
    if ok("cmd"):
        _mod("agent.command_token_source",
             build_command_token_provider=SeamFn("build_command_token_provider",
                 lambda a, k: {"cmd": a[0], "name": a[1]}))
    else:
        sys.modules["agent.command_token_source"] = None
    if ok("credential_pool"):
        _mod("agent.credential_pool",
             custom_provider_pool_key_candidates=SeamFn(
                 "custom_provider_pool_key_candidates",
                 lambda a, k: {"base_url": a[0], "pool_name": a[1]}))
    else:
        sys.modules["agent.credential_pool"] = None
    # openai (VertexOpenAI)
    _mod("openai", OpenAI=SeamFn("vertex_openai_ctor",
                                 lambda a, k: {"api_key": k.get("api_key"),
                                               "base_url": k.get("base_url")}))


_BASE_NS = {
    "__name__": "ac4_live_auxiliary_client",
    "__builtins__": builtins,
    "contextlib": contextlib,
    "hashlib": hashlib,
    "base64": base64,
    "json": json,
    "logger": _LOGGER,
    "Any": Any, "Optional": Optional, "Callable": Callable,
    "ProfileProjection": ProfileProjection, "_ProfileProjection": ProfileProjection,
    "ResolvedAuxRoute": ResolvedAuxRoute, "_ResolvedAuxRoute": ResolvedAuxRoute,
    "ResolveRequest": ResolveRequest, "_ResolveRequest": ResolveRequest,
    "AuxiliaryClientUnavailable": AuxiliaryClientUnavailable,
    "_ClassifiedAttempt": _ClassifiedAttempt, "_STREAM_REJECTED": _STREAM_REJECTED,
    "aux_errors": _AuxErrors(),
    "dict": dict, "str": str, "bool": bool, "type": type, "frozenset": frozenset,
}
_MOD = types.ModuleType("ac4_live_auxiliary_client")
_MOD.__dict__.update(_BASE_NS)
sys.modules["ac4_live_auxiliary_client"] = _MOD
_NS = _MOD.__dict__
for _nm, _code in DEF_CODE.items():
    exec(_code, _NS)


def _B(name, argmap=None):
    return SeamFn(name, argmap)


def cmarker(v):
    if isinstance(v, ObjDouble):
        return v.__dict__.get("id", "client")
    return str(v)


# Bare-global collaborators (module-top imports / lazy-imported names).
_BARE = {
    "_extract_url_query_params": ("extract_url_query_params", lambda a, k: {"url": a[0]}),
    "_to_openai_base_url": ("to_openai_base_url", lambda a, k: {"base": a[0]}),
    "_apply_user_default_headers": ("apply_user_default_headers", lambda a, k: {"client": a[0]}),
    "_create_openai_client": ("create_openai_client", lambda a, k: {kk: v for kk, v in k.items() if v is not None}),
    "_normalize_resolved_model": ("normalize_resolved_model",
                                  lambda a, k: {"model": a[0], "provider": a[1]}),
    "_is_actual_auxiliary_route": ("is_actual_auxiliary_route",
                                   lambda a, k: {"req": {"provider": a[0].provider,
                                                         "api_mode": a[0].api_mode,
                                                         "raw_codex": a[0].raw_codex},
                                                 "base_url": a[1]}),
    "_maybe_wrap_anthropic": ("maybe_wrap_anthropic", lambda a, k: {"client": a[0], "model": a[1], "api_key": a[2], "base_url": a[3], "api_mode": a[4]}),
    "_profile_declared_messages_wire": ("profile_declared_messages_wire",
                                        lambda a, k: {"provider": a[0]}),
    "base_url_hostname": ("base_url_hostname", lambda a, k: {"base_url": a[0]}),
    "CodexAuxiliaryClient": ("codex_client_ctor", lambda a, k: {"client": a[0], "model": a[1]}),
    "AnthropicAuxiliaryClient": ("anthropic_client_ctor", lambda a, k: {"real_client": a[0], "model": a[1], "api_key": k.get("api_key"), "base_url": k.get("base_url")}),
    "BedrockAuxiliaryClient": ("bedrock_client_ctor",
                               lambda a, k: {"region": a[0], "model": a[1]}),
    "OpenAI": ("openai_ctor", lambda a, k: {kk: v for kk, v in k.items() if v is not None}),
    "_routes_to_custom_endpoint": ("routes_to_custom_endpoint", lambda a, k: {"provider": a[0]}),
    "contains_profile_reasoning_fields": ("contains_profile_reasoning_fields",
                                          lambda a, k: {"d": a[0]}),
    "_effective_provider_for_client": ("effective_provider_for_client",
                                       lambda a, k: {"client": a[0], "provider": a[1]}),
    "_get_cached_client": ("get_cached_client", lambda a, k: dict({"provider": a[0], "model": a[1] if len(a) > 1 else None}, **k)),
    "_to_async_client": ("to_async_client", lambda a, k: {"client": a[0], "model": a[1], "is_vision": k.get("is_vision")}),
    "_try_configured_fallback_for_unavailable_client": ("try_configured_fallback_for_unavailable_client",
                                                        lambda a, k: {"task": a[0], "provider": a[1]}),
    "missing_provider_credentials_message": ("missing_provider_credentials_message",
                                             lambda a, k: {"provider": a[0]}),
    "nous_credential_failure_detail": ("nous_credential_failure_detail", lambda a, k: {}),
    "resolve_vision_provider_client": ("resolve_vision_provider_client", lambda a, k: {kk: v for kk, v in k.items() if v is not None}),
    "bypass_chat_sdk_request_transform": ("bypass_chat_sdk_request_transform",
                                          lambda a, k: {"kwargs": a[0], "client": cmarker(a[1])}),
    "_aux_progress_active": ("aux_progress_active", lambda a, k: {}),
    "_async_client_streams_internally": ("async_client_streams_internally",
                                         lambda a, k: {"client": cmarker(a[0])}),
    "_notify_aux_dispatch": ("notify_aux_dispatch", lambda a, k: {}),
    "_notify_aux_provider_response": ("notify_aux_provider_response", lambda a, k: {}),
    "_stream_request_plan": ("stream_request_plan", lambda a, k: {"kwargs": a[0], "client": cmarker(a[1])}),
    "_classify_failure": ("classify_failure", lambda a, k: {"exc": a[0], "provider": a[1], "client": cmarker(a[2]), "model": a[3]}),
    "_aggregate_chat_stream_async": ("aggregate_chat_stream_async", lambda a, k: dict({"chunks": a[0]}, **k)),
    "_get_provider_chain": ("get_provider_chain", lambda a, k: {}),
    "_custom_health_base_url": ("custom_health_base_url", lambda a, k: {"label": a[0]}),
    "_is_provider_unhealthy": ("is_provider_unhealthy",
                               lambda a, k: {"label": a[0], "base_url": a[1]}),
    "_log_skip_unhealthy": ("log_skip_unhealthy",
                            lambda a, k: {"label": a[0], "base_url": k.get("base_url")}),
    "_pool_runtime_api_key": ("pool_runtime_api_key", lambda a, k: {"entry": a[0]}),
    "entry_scoped_pool": ("entry_scoped_pool", lambda a, k: {"pool": a[0], "pool_key": a[1], "custom_entry": a[2], "base_url": a[3]}),
    "load_pool": ("load_pool", lambda a, k: {"pool_key": a[0]}),
}
for _live, (_seam, _amap) in _BARE.items():
    _NS[_live] = AsyncSeamFn(_seam, _amap) if _seam == "aggregate_chat_stream_async" else _B(_seam, _amap)


def _openai_ctor(**kw):
    args = {k: v for k, v in kw.items() if v is not None}
    return record("create_openai_client", args)


def _vertex_ctor(**kw):
    return record("vertex_openai_ctor", {k: v for k, v in kw.items() if v is not None})


_NS["create_openai_client"] = None  # placeholder; unused


# ── materialization of seam results into attribute doubles ─────────────────
class CodexAuxiliaryClient:
    def __new__(cls, client, model):
        return record("codex_client_ctor", {"client": client, "model": model})


def mat_profile(script):
    if not isinstance(script, dict):
        return script
    overridden = bool(script.get("__overridden__", False))

    class _P(_ProviderProfileBase):
        def __init__(self, d):
            self.__dict__.update({k: v for k, v in d.items() if not k.startswith("__")})

        def create_client(self, **kw):
            return record("profile.create_client", {"kwargs": kw})

        def build_extra_body(self, **kw):
            return record("build_extra_body",
                          {"model": kw.get("model"), "base_url": kw.get("base_url"),
                           "reasoning_config": kw.get("reasoning_config"),
                           "data_use": kw.get("data_use")})

    if overridden:
        def _overridden_build(self, **kw):
            return record("build_api_kwargs_extras",
                          {"reasoning_config": kw.get("reasoning_config"),
                           "model": kw.get("model"), "base_url": kw.get("base_url")})
        _P.build_api_kwargs_extras = _overridden_build
    return _P(script)


def mat_pool(marker):
    d = ObjDouble({})
    def entries_fn():
        return record("pool.entries", {"pool": marker})
    d.entries = entries_fn
    d.has_credentials = SeamFn("pool.has_credentials", lambda a, k: {})
    d.select = SeamFn("pool.select", lambda a, k: {"model": k.get("model")})
    return d


def materialize(name, script):
    if name == "get_provider_profile":
        return mat_profile(script) if script is not None else None
    if name == "load_pool":
        return mat_pool(script) if script is not None else None
    if name == "classify_failure" and isinstance(script, dict):
        return ObjDouble(script)
    if name == "get_provider_chain" and isinstance(script, list):
        return [(row[0], SeamFn("try_fn." + row[1], lambda a, k: {})) for row in script]
    if name == "create" and script == "__STREAM_REJECTED__":
        return _STREAM_REJECTED
    if name in ("get_cached_client", "resolve_vision_provider_client",
                "try_configured_fallback_for_unavailable_client", "to_async_client",
                "stream_request_plan", "build_api_kwargs_extras") and isinstance(script, (list, tuple)):
        return tuple(script)
    return script


# record() uses materialize (defined above at call time)
def _record_patch(src):
    return src


class _Blake2bDouble:
    def __init__(self, **kw):
        self.parts = []

    def update(self, b):
        if b != b"\0":
            self.parts.append(b.decode("utf-8", "replace"))

    def hexdigest(self):
        return record("blake2b_hexdigest", {"keys": self.parts})


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    gate = inp.get("gate") or {}
    _install_imports(gate)
    trace = []
    scripts = Scripts(inp.get("io") or {}, trace)
    _SCRIPTS = scripts
    # Real class for isinstance(client, CodexAuxiliaryClient)
    _NS["CodexAuxiliaryClient"] = CodexAuxiliaryClient
    _NS["hashlib"] = types.SimpleNamespace(blake2b=lambda **kw: _Blake2bDouble(**kw))
    return _NS, trace, scripts


def mat_client(d):
    if d is None:
        return None
    if isinstance(d, dict):
        if d.get("kind") == "codex":
            o = object.__new__(CodexAuxiliaryClient)
            o.client = d.get("real_client")
            o.model = d.get("model", "")
            rc = d.get("real_client")
            o._real_client = ObjDouble(dict(rc)) if isinstance(rc, dict) else rc
            return o
        o = ObjDouble({k: v for k, v in d.items() if k != "kind"})
        if "real_client" in d:
            rc = d["real_client"]
            o._real_client = ObjDouble(dict(rc)) if isinstance(rc, dict) else rc
        return o
    return d


def mat_chat_client(d):
    """Client carrying chat.completions.create seam."""
    marker = (d.get("id", "c") if isinstance(d, dict) else str(d))
    create_fn = AsyncSeamFn("create", lambda a, k: {kk: v for kk, v in k.items() if v is not None})
    comp = ObjDouble({"create": create_fn})
    return ObjDouble({"id": marker, "chat": ObjDouble({"completions": comp})})


def proj_state(st):
    out = {}
    for k, v in st.items():
        if isinstance(v, CodexAuxiliaryClient):
            out[k] = {"kind": "codex", "real_client": norm(v._real_client), "model": norm(v.model)}
        elif isinstance(v, ResolveRequest) or (hasattr(v, "_asdict") and hasattr(v, "_replace")):
            out[k] = {kk: norm(vv) for kk, vv in v._asdict().items() if vv is not None}
        elif isinstance(v, ObjDouble):
            out[k] = {kk: norm(vv) for kk, vv in vars(v).items() if not kk.startswith("__sfn__")}
        else:
            out[k] = norm(v)
    return out


def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    fn = ns[func]

    if func == "_named_custom_openai_wire_client":
        return fn(inp.get("custom_base", ""), inp.get("custom_key"), inp.get("extra_headers"))

    if func == "_api_key_profile_supplied_client":
        return fn(inp.get("provider", ""), **(inp.get("client_kwargs") or {}))

    if func == "_build_vertex_client":
        return fn(inp.get("provider", ""), inp.get("model"))

    if func == "_build_bedrock_client":
        return fn(inp.get("provider", ""), inp.get("model"), raw_codex=bool(inp.get("raw_codex", False)))

    if func == "_wrap_transport":
        reqd = dict(inp.get("req") or {})
        req = ResolveRequest(**reqd)
        client = mat_client(inp.get("client"))
        ret = fn(req, client, inp.get("final_model", ""), inp.get("base_url", "") or "",
                 inp.get("api_key", "") or "")
        real = getattr(client, "_real_client", None) if client is not None else None
        holder["_state"] = {
            "api_mode": req.api_mode,
            "base_url": inp.get("base_url", "") or "",
            "flag": getattr(client, "_hermes_aux_effective_provider", None) if client is not None else None,
            "real_flag": getattr(real, "_hermes_aux_effective_provider", None) if real is not None else None,
        }
        return ret

    if func == "_project_provider_profile":
        return fn(inp.get("provider", ""), inp.get("provider_norm", ""), inp.get("model", ""),
                  inp.get("base_url", ""), inp.get("reasoning_config"),
                  frozenset(inp.get("data_use") or []))

    if func == "_resolve_call_client":
        return fn(inp.get("task"), provider=inp.get("provider"), model=inp.get("model"),
                  base_url=inp.get("base_url"), api_key=inp.get("api_key"),
                  resolved_provider=inp.get("resolved_provider", ""),
                  resolved_model=inp.get("resolved_model"),
                  resolved_base_url=inp.get("resolved_base_url"),
                  resolved_api_key=inp.get("resolved_api_key"),
                  resolved_api_mode=inp.get("resolved_api_mode"),
                  main_runtime=inp.get("main_runtime"),
                  async_mode=bool(inp.get("async_mode", False)))

    if func == "_acreate_with_progress":
        client = mat_chat_client(inp.get("client"))
        return fn(client, dict(inp.get("kwargs") or {}), inp.get("task"),
                  force_stream=bool(inp.get("force_stream", False)),
                  provider=inp.get("provider", ""))

    if func == "_try_discovery_chain":
        return fn()

    if func == "_translate_anthropic_response_format":
        ak = inp.get("anthropic_kwargs") or {}
        holder["_state"] = {"anthropic_kwargs": ak}
        fn(ak, inp.get("response_format"))
        return None

    if func == "_pool_credential_digest":
        pool_marker = inp.get("pool")
        pool = mat_pool(pool_marker) if pool_marker is not None else None
        return fn(pool, inp.get("entry"))

    raise KeyError("unknown func " + str(func))


def state_of(holder, raised):
    return proj_state(holder["_state"]) if ("_state" in holder and not raised) else {}


def main():
    import asyncio, inspect
    n = mism = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        holder = {}
        raised = False
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                ret = drive(rec, ns, scripts, holder)
                if inspect.isawaitable(ret):
                    ret = asyncio.run(ret)
                out = {"ret": norm(ret), "trace": trace, "state": state_of(holder, False)}
            except BaseException as e:
                raised = True
                out = {"ret": {"__raise__": type(e).__name__}, "trace": trace,
                       "state": state_of(holder, True)}
        except BaseException as e:
            out = {"ret": {"__raise__": type(e).__name__}, "trace": [], "state": {}}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 12:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:1200]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:1200]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


def selftest(fixtures):
    import asyncio, inspect
    bad = 0
    for i, rec in enumerate(fixtures):
        holder = {}
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                ret = drive(rec, ns, scripts, holder)
                if inspect.isawaitable(ret):
                    asyncio.run(ret)
            except BaseException as e:
                if type(e).__name__ == "AttributeError":
                    bad += 1
                    print(f"SELFTEST case {i} {rec['func']}: AttributeError: {e}")
        except BaseException as e:
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = _HERE.parent / "fixtures" / "auxiliary_client_flow4.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()
