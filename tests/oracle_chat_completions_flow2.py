#!/usr/bin/env python3
"""Oracle for the Chat Completions transport UNCLAIMED-remainder FLOW port (wave 2;
src/agent/transports/chat_completions_flow2.c).

Drives the LIVE agent/transports/chat_completions.py defs with SCRIPTED seams:
every claimed sibling (flow1 openai_transport_flow.c leaves/flows, the helpers-flow
is_router_timeout_shim, provider_profile.c _reasoning_config_for_model), every
foreign-module import, and every profile-method / self-method double is a recording
stub fed the same scripts the C harness feeds cc3_call. Compares the DECISION
SEQUENCE (ordered "trace" of {ev,args,ret} seam events), the canonical return
("ret") and the post-call mutation window ("state").

16 revival targets driven live (8 module defs + 8 ChatCompletionsTransport
methods). Ported siblings called by a target (_build_gemini_thinking_config,
_snake_case_gemini_thinking_config, _is_gemini_openai_compat_base_url,
_is_openai_api_base_url, _static_prompt_instructions, _model_consumes_thought_signature,
convert_messages, _build_kwargs_from_profile) stay REAL via AST extraction, matching
the C port's internal composition. Claimed siblings are SEAMED.

Live defs are AST-extracted from the quarry; module constants are seeded
(_HIGH_EFFORTS / _GEMINI_SIGNATURE_HOSTS / _GEMINI_SIGNATURE_PROVIDERS local;
GEMINI4_THINKING_LEVELS / KIMI_K3_EFFORTS / KIMI_K3_OVERRIDES from
agent.reasoning_effort; NormalizedResponse / ToolCall / Usage from
agent.transports.types). The heavy quarry import chain is never imported:
in-function imports ride sys.modules stubs (every intermediate package registered).
The exec namespace is a registered sys.modules module (py3.14 annotation resolution
needs a real module alias). Fail-closed: 0 cases => exit 1; a MISSING seam script
raises RuntimeError("no script for <name>"); any harness/oracle exception per case
rides as {"__raise__": cls} (BaseException catch, so SystemExit/KeyboardInterrupt/
GeneratorExit pass through as specs, not exits).
Summary: 'ORACLE chat_completions_flow2: N cases, M mismatches'.

Representation contract (both sides, see the port .c header):
- scripts: MISSING key -> RuntimeError("no script for <name>") (fail-closed); null ->
  None; {"$seq":[...]} per-call queue (exhaustion raises RuntimeError("script exhausted:
  <name>")); {"__raise__":cls,"msg":m,...} raises (trace ret is the WHOLE marker); any
  other value reused (a bare ARRAY is a LIST RESULT, not a queue). None-valued seam arg
  keys dropped on both sides.
- _wire_reasoning_carriers results are frozensets (sorted-array projection);
  kimi_supported_efforts "__KIMI_K3_EFFORTS__" marker -> the real K3 constant (is test);
  build_api_kwargs_extras / declared_carrier_keys results are 2-tuples; usage_from_openai
  results materialize into Usage; _apply_max_tokens / _bound_prompt_cache_key_field are
  void mutators ({"__update__":{}} merges, {"__pop__":[keys]} removes).
- tuple/set/frozenset -> array (sets sorted); None -> null; integral floats collapse to
  ints; uncaught exception -> {"__raise__": cls} (class only).
- except (TypeError, ValueError) / except Exception sites swallow the matching family;
  the BaseException trio passes through. Scripted raises materialize to REAL builtin
  exception instances (str(e) == the scripted msg).
- profile.fixed_temperature "__OMIT_TEMPERATURE__" marker rides VERBATIM (the is-test
  lives inside the seamed _base_kwargs). finish_reason {"__int__":n} marker models a
  Python int (Poolside). _static_prompt_instructions content {"__pyobj__":s} /
  {"__pyobj_falsy__":s} markers model opaque objects (json.dumps TypeError -> str(content or "")).
"""
import __future__
import ast
import builtins
import dataclasses
import json
import math
import os
import sys
import types
from pathlib import Path
from typing import Any
from urllib.parse import urlparse

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "transports" / "chat_completions.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "transports" / "chat_completions.py")
RE_SRC = str(QUARRY / "agent" / "reasoning_effort.py")
TYPES_SRC = str(QUARRY / "agent" / "transports" / "types.py")
PORT_SRC = str(_REPO / "src" / "agent" / "transports" / "chat_completions_flow2.c")

NAME = "chat_completions_flow2"

PORTED = [
    "_build_gemini_thinking_config", "_snake_case_gemini_thinking_config",
    "_is_gemini_openai_compat_base_url", "_model_consumes_thought_signature",
    "_static_prompt_instructions", "_add_prompt_cache_key",
    "_is_openai_api_base_url", "_raise_gemini_thinking_max_tokens",
]
METHODS = [
    "api_mode", "convert_messages", "convert_tools", "build_kwargs",
    "_build_kwargs_from_profile", "normalize_response", "validate_response",
    "extract_cache_stats",
]
CC_CONSTANTS = ["_HIGH_EFFORTS", "_GEMINI_SIGNATURE_HOSTS", "_GEMINI_SIGNATURE_PROVIDERS"]
RE_CONSTANTS = ["GEMINI4_THINKING_LEVELS", "KIMI_K3_EFFORTS", "KIMI_K3_OVERRIDES"]
TYPE_CLASSES = ["ToolCall", "Usage", "NormalizedResponse"]
WANTED = set(PORTED)
FUNCS = set(PORTED) | set(METHODS)

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src, flags=0):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=flags, dont_inherit=True)


DEF_CODE = {}
METHOD_CODE = {}
CONST_CODE = {}
RE_CONST_CODE = {}
TYPE_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node, SRC)
    elif isinstance(_node, ast.ClassDef) and _node.name == "ChatCompletionsTransport":
        for _b in _node.body:
            if isinstance(_b, ast.FunctionDef) and _b.name in METHODS:
                _b.decorator_list = []   # api_mode is a @property; drive it as a plain method
                METHOD_CODE[_b.name] = _compile_node(_b, SRC)
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(tg, ast.Name) and tg.id in CC_CONSTANTS:
            CONST_CODE[tg.id] = _compile_node(_node, SRC)
for _node in ast.parse(Path(RE_SRC).read_text(encoding="utf-8")).body:
    if isinstance(_node, (ast.Assign, ast.AnnAssign)):
        tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(tg, ast.Name) and tg.id in RE_CONSTANTS:
            RE_CONST_CODE[tg.id] = _compile_node(_node, RE_SRC)
for _node in ast.parse(Path(TYPES_SRC).read_text(encoding="utf-8")).body:
    if isinstance(_node, ast.ClassDef) and _node.name in TYPE_CLASSES:
        TYPE_CODE[_node.name] = _compile_node(_node, TYPES_SRC, flags=_FLAGS)
_missing = ((WANTED - set(DEF_CODE)) | (set(METHODS) - set(METHOD_CODE))
            | (set(CC_CONSTANTS) - set(CONST_CODE)) | (set(RE_CONSTANTS) - set(RE_CONST_CODE))
            | (set(TYPE_CLASSES) - set(TYPE_CODE)))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

# ── canonical projection ────────────────────────────────────────────────────


def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items()}
    if dataclasses.is_dataclass(v) and not isinstance(v, type):
        return {k: norm(x) for k, x in vars(v).items()}
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, float) and math.isfinite(v) and v.is_integer():
        return int(v)
    return v


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    if cls_name in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    return cls(msg)


# ── scripted seams (fail-closed) ────────────────────────────────────────────
_SCRIPTS = None


class Scripts:
    def __init__(self, io, trace):
        self.io = io or {}
        self.trace = trace
        self.cursors = {}

    def next(self, name):
        if name not in self.io:
            return {"__raise__": "RuntimeError", "msg": "no script for " + name}
        script = self.io[name]
        if isinstance(script, dict) and "$seq" in script:
            seq = script["$seq"]
            i = self.cursors.get(name, 0)
            if i >= len(seq):
                return {"__raise__": "RuntimeError", "msg": "script exhausted: " + name}
            self.cursors[name] = i + 1
            return seq[i]
        return script


SEAM_ARGS = {
    "_profile_replays_tool_call_signatures": lambda a, k: {"provider": a[0]},
    "_wire_reasoning_carriers": lambda a, k: {"provider": a[0], "base_url": a[1]},
    "_route_requires_mistral_tool_ids": lambda a, k: {"provider": a[0], "model": a[1], "base_url": a[2]},
    "_sanitize_message": lambda a, k: {"msg": a[0], "strip_extra_content": a[1],
                                       "strip_reasoning": a[2], "native_reasoning_details_type": a[3],
                                       "mistral_tool_ids": k["mistral_tool_ids"]},
    "_swap_developer_role": lambda a, k: {"sanitized": a[0], "model_lower": a[1]},
    "_base_kwargs": lambda a, k: {"model": a[0], "sanitized": a[1], "tools": a[2],
                                  "params": a[3], "profile": k.get("profile")},
    "_apply_max_tokens": lambda a, k: {"api_kwargs": a[0], "model": a[1],
                                       "reasoning_config": a[2], "params": a[3],
                                       "profile_max": k.get("profile_max")},
    "_pareto_score": lambda a, k: {"raw": a[0]},
    "_finish_kwargs": lambda a, k: {"api_kwargs": a[0], "sanitized": a[1], "params": a[2],
                                    "supports_prompt_cache_key": k["supports_prompt_cache_key"]},
    "_normalize_tool_call": lambda a, k: {"tc": a[0]},
    "_attr_or_model_extra": lambda a, k: {"obj": a[0], "name": a[1]},
    "_dump_extra_content": lambda a, k: {"extra": a[0]},
    "usage_from_openai": lambda a, k: {"u": a[0]},
    "is_router_timeout_shim": lambda a, k: {"response": a[0]},
    "_reasoning_config_for_model": lambda a, k: {"model": a[0], "reasoning_config": a[1], "base_url": a[2]},
    "gemini_uses_thinking_level": lambda a, k: {"model": a[0]},
    "clamp_effort": lambda a, k: {"effort": a[0], "supported": a[1],
                                  "overrides": a[2] if len(a) > 2 else k.get("overrides")},
    "_effective_gemini_max_output_tokens": lambda a, k: {"requested": a[0], "thinking_config": a[1]},
    "base_url_host_matches": lambda a, k: {"base_url": a[0], "host": a[1]},
    "_bound_prompt_cache_key_field": lambda a, k: {"container": a[0]},
    "_cache_scope_from_session_id": lambda a, k: {"session_id": a[0]},
    "_content_cache_key": lambda a, k: {"instructions": a[0], "tools": a[1], "scope_id": a[2]},
    "data_use_requirement": lambda a, k: {"messages": a[0]},
    "requested_effort": lambda a, k: {"reasoning_config": a[0]},
    "kimi_supported_efforts": lambda a, k: {"model": a[0]},
    "tokenhub_effort": lambda a, k: {"requested": a[0]},
    "prepare_messages": lambda a, k: {"sanitized": a[0]},
    "get_max_tokens": lambda a, k: {"model": a[0]},
    "build_api_kwargs_extras": lambda a, k: {
        "reasoning_config": k["reasoning_config"], "supports_reasoning": k["supports_reasoning"],
        "qwen_session_metadata": k.get("qwen_session_metadata"), "model": k["model"],
        "base_url": k.get("base_url"), "ollama_num_ctx": k.get("ollama_num_ctx"),
        "session_id": k.get("session_id"), "cache_scope_id": k.get("cache_scope_id"),
        "lmstudio_reasoning_options": k.get("lmstudio_reasoning_options")},
    "build_extra_body": lambda a, k: {
        "session_id": k.get("session_id"), "provider_preferences": k.get("provider_preferences"),
        "model": k["model"], "base_url": k.get("base_url"), "reasoning_config": k.get("reasoning_config"),
        "openrouter_min_coding_score": k.get("openrouter_min_coding_score"), "data_use": k.get("data_use")},
    "is_native_gemini_base_url": lambda a, k: {"base_url": a[0]},
    "_normalize_finish_reason": lambda a, k: {"finish_reason": a[0]},
    "declared_carrier_keys": lambda a, k: {},
}

BUILD = {
    "_wire_reasoning_carriers": lambda r: frozenset(r) if isinstance(r, list) else r,
    "kimi_supported_efforts": lambda r: (_BASE_NS["KIMI_K3_EFFORTS"] if r == "__KIMI_K3_EFFORTS__"
                                         else (tuple(r) if isinstance(r, list) else r)),
    "build_api_kwargs_extras": lambda r: tuple(r) if isinstance(r, list) else r,
    "declared_carrier_keys": lambda r: tuple(r) if isinstance(r, list) else r,
    "usage_from_openai": lambda r: _BASE_NS["Usage"](**r) if isinstance(r, dict) else r,
}


def _bound_post(container, r):
    if isinstance(r, dict):
        if isinstance(r.get("__update__"), dict):
            container.update(r["__update__"])
        if isinstance(r.get("__pop__"), list):
            for kk in r["__pop__"]:
                container.pop(kk, None)


POST = {
    "_apply_max_tokens": lambda a, k, r: (
        a[0].update(r["__update__"])
        if isinstance(r, dict) and isinstance(r.get("__update__"), dict) else None),
    "_bound_prompt_cache_key_field": lambda a, k, r: _bound_post(a[0], r),
}


def record(name, args):
    scripts = _SCRIPTS
    args = {k: norm(v) for k, v in args.items() if v is not None}
    script = scripts.next(name)
    if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
        marker = dict(script)
        scripts.trace.append({"ev": name, "args": args, "ret": marker})
        raise make_exc(marker)
    result = BUILD[name](script) if name in BUILD else script
    scripts.trace.append({"ev": name, "args": args, "ret": norm(script)})
    return result


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **kw):
        r = record(self.name, SEAM_ARGS[self.name](a, kw))
        if self.name in POST:
            POST[self.name](a, kw, r)
        return r


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# ── base namespace ──────────────────────────────────────────────────────────
_BASE_NS = {
    "__name__": "cc3_live_chat_completions",
    "__builtins__": builtins,
    "json": json, "Any": Any, "urlparse": urlparse,
    "dataclass": dataclasses.dataclass, "field": dataclasses.field,
}
_MOD = types.ModuleType("cc3_live_chat_completions")
_MOD.__dict__.update(_BASE_NS)
sys.modules["cc3_live_chat_completions"] = _MOD
_BASE_NS = _MOD.__dict__
for _nm, _code in RE_CONST_CODE.items():
    exec(_code, _BASE_NS)
for _nm, _code in CONST_CODE.items():
    exec(_code, _BASE_NS)
for _nm, _code in TYPE_CODE.items():
    exec(_code, _BASE_NS)
for _nm, _code in DEF_CODE.items():
    exec(_code, _BASE_NS)
for _nm, _code in METHOD_CODE.items():
    exec(_code, _BASE_NS)

# Claimed module-def siblings + module-top imported collaborators -> recorders.
_SEAMED_GLOBALS = [
    "_profile_replays_tool_call_signatures", "_wire_reasoning_carriers",
    "_route_requires_mistral_tool_ids", "_sanitize_message", "_swap_developer_role",
    "_base_kwargs", "_reasoning_config_for_model", "_apply_max_tokens", "_pareto_score",
    "_finish_kwargs", "_attr_or_model_extra", "_dump_extra_content",
    "is_router_timeout_shim",
    "clamp_effort", "requested_effort", "kimi_supported_efforts", "tokenhub_effort",
    "data_use_requirement", "_normalize_finish_reason",
]
for _nm in _SEAMED_GLOBALS:
    _BASE_NS[_nm] = SeamFn(_nm)
# Usage.from_openai is a flow1-claimed types.py leaf -> seam the classmethod.
_BASE_NS["Usage"].from_openai = staticmethod(SeamFn("usage_from_openai"))


def install_stubs():
    agent = _mod("agent")
    agent.__path__ = []
    transports = _mod("agent.transports")
    transports.__path__ = []
    agent.transports = transports
    gna = _mod("agent.gemini_native_adapter",
               gemini_uses_thinking_level=SeamFn("gemini_uses_thinking_level"),
               _effective_gemini_max_output_tokens=SeamFn("_effective_gemini_max_output_tokens"),
               is_native_gemini_base_url=SeamFn("is_native_gemini_base_url"))
    agent.gemini_native_adapter = gna
    codex = _mod("agent.transports.codex",
                 _bound_prompt_cache_key_field=SeamFn("_bound_prompt_cache_key_field"),
                 _cache_scope_from_session_id=SeamFn("_cache_scope_from_session_id"),
                 _content_cache_key=SeamFn("_content_cache_key"))
    transports.codex = codex
    rc = _mod("agent.reasoning_carriers", declared_carrier_keys=SeamFn("declared_carrier_keys"))
    agent.reasoning_carriers = rc
    utils = _mod("utils", base_url_host_matches=SeamFn("base_url_host_matches"))
    utils.__path__ = []


install_stubs()


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    trace = []
    _SCRIPTS = Scripts(inp.get("io") or {}, trace)
    return _BASE_NS, trace, _SCRIPTS


# ── input doubles ───────────────────────────────────────────────────────────


class _PyObj:
    def __init__(self, s, truthy):
        self._s = s
        self._t = truthy

    def __str__(self):
        return self._s

    def __bool__(self):
        return self._t


class ProfileDouble(types.SimpleNamespace):
    prepare_messages = SeamFn("prepare_messages")
    get_max_tokens = SeamFn("get_max_tokens")
    build_api_kwargs_extras = SeamFn("build_api_kwargs_extras")
    build_extra_body = SeamFn("build_extra_body")


class SelfDouble(types.SimpleNamespace):
    _normalize_tool_call = SeamFn("_normalize_tool_call")


def mk_profile(d):
    if not isinstance(d, dict):
        return d
    p = ProfileDouble()
    p.__dict__.update(d)
    return p


def mk_self(ns):
    slf = SelfDouble()
    slf.convert_messages = types.MethodType(ns["convert_messages"], slf)
    slf._build_kwargs_from_profile = types.MethodType(ns["_build_kwargs_from_profile"], slf)
    return slf


def mk_choice(c):
    if not isinstance(c, dict):
        return c
    ch = types.SimpleNamespace(**c)
    m = getattr(ch, "message", None)
    if isinstance(m, dict):
        ch.message = types.SimpleNamespace(**m)
    fr = getattr(ch, "finish_reason", None)
    if isinstance(fr, dict) and "__int__" in fr:
        ch.finish_reason = int(fr["__int__"])
    return ch


def mk_response(d):
    if not isinstance(d, dict):
        return d
    r = types.SimpleNamespace(**d)
    cl = getattr(r, "choices", None)
    if isinstance(cl, list):
        r.choices = [mk_choice(c) for c in cl]
    return r


def mk_cache_response(d):
    if not isinstance(d, dict):
        return d
    r = types.SimpleNamespace(**d)
    u = getattr(r, "usage", None)
    if isinstance(u, dict):
        u = types.SimpleNamespace(**u)
        det = getattr(u, "prompt_tokens_details", None)
        if isinstance(det, dict):
            u.prompt_tokens_details = types.SimpleNamespace(**det)
        r.usage = u
    return r


def mk_messages_pyobj(msgs):
    if isinstance(msgs, list) and msgs and isinstance(msgs[0], dict):
        m0 = dict(msgs[0])
        c = m0.get("content")
        if isinstance(c, dict) and "__pyobj__" in c:
            m0["content"] = _PyObj(c["__pyobj__"], True)
        elif isinstance(c, dict) and "__pyobj_falsy__" in c:
            m0["content"] = _PyObj(c["__pyobj_falsy__"], False)
        return [m0] + msgs[1:]
    return msgs


# ── live driver ─────────────────────────────────────────────────────────────


def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    if func == "api_mode":
        return ns["api_mode"](mk_self(ns))
    if func == "convert_tools":
        return ns["convert_tools"](mk_self(ns), inp.get("tools"))
    if func == "convert_messages":
        kw = dict(inp.get("kwargs") or {})
        if "provider_profile" in kw:
            kw["provider_profile"] = mk_profile(kw["provider_profile"])
        return ns["convert_messages"](mk_self(ns), inp.get("messages"), **kw)
    if func == "build_kwargs":
        params = dict(inp.get("params") or {})
        if "provider_profile" in params:
            params["provider_profile"] = mk_profile(params["provider_profile"])
        return ns["build_kwargs"](mk_self(ns), inp.get("model"), inp.get("messages"),
                                  inp.get("tools"), **params)
    if func == "_build_kwargs_from_profile":
        return ns["_build_kwargs_from_profile"](
            mk_self(ns), mk_profile(inp.get("profile")), inp.get("model"),
            inp.get("sanitized"), inp.get("tools"), dict(inp.get("params") or {}))
    if func == "normalize_response":
        return ns["normalize_response"](mk_self(ns), mk_response(inp.get("response")))
    if func == "validate_response":
        return ns["validate_response"](mk_self(ns), mk_response(inp.get("response")))
    if func == "extract_cache_stats":
        return ns["extract_cache_stats"](mk_self(ns), mk_cache_response(inp.get("response")))
    if func == "_static_prompt_instructions":
        return ns["_static_prompt_instructions"](mk_messages_pyobj(inp.get("messages")))
    if func == "_add_prompt_cache_key":
        kw = inp.get("api_kwargs")
        holder["win"] = ("api_kwargs", kw)
        return ns["_add_prompt_cache_key"](
            kw, messages=inp.get("messages"), tools=inp.get("tools"),
            supports_prompt_cache_key=inp.get("supports_prompt_cache_key", False),
            session_id=inp.get("session_id"), cache_scope_id=inp.get("cache_scope_id"))
    if func == "_build_gemini_thinking_config":
        return ns["_build_gemini_thinking_config"](inp.get("model"), inp.get("reasoning_config"))
    if func == "_snake_case_gemini_thinking_config":
        return ns["_snake_case_gemini_thinking_config"](inp.get("config"))
    if func == "_raise_gemini_thinking_max_tokens":
        return ns["_raise_gemini_thinking_max_tokens"](inp.get("model"), inp.get("reasoning_config"),
                                                       inp.get("requested"))
    if func == "_is_gemini_openai_compat_base_url":
        return ns["_is_gemini_openai_compat_base_url"](inp.get("base_url"))
    if func == "_is_openai_api_base_url":
        return ns["_is_openai_api_base_url"](inp.get("base_url"))
    if func == "_model_consumes_thought_signature":
        return ns["_model_consumes_thought_signature"](inp.get("model"), inp.get("base_url"),
                                                       inp.get("provider"))
    raise KeyError(f"unknown func {func}")


def echo_state(holder):
    w = holder.get("win")
    if not w:
        return {}
    kind, obj = w
    return {kind: norm(obj)}


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
                out = {"ret": norm(ret), "trace": trace, "state": echo_state(holder)}
            except BaseException as e:  # noqa: BLE001 - fail-closed per case
                out = {"ret": {"__raise__": type(e).__name__}, "trace": trace,
                       "state": echo_state(holder)}
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


# ── self-test ───────────────────────────────────────────────────────────────


def cross_check(fixtures):
    import re
    bad = 0
    ctext = Path(PORT_SRC).read_text(encoding="utf-8")
    c_names = set(re.findall(r'cc3_call\(ctx,\s*"([^"]+)"', ctext))
    if c_names != set(SEAM_ARGS):
        bad += 1
        print(f"CROSSCHECK seam-name drift: C-only={sorted(c_names - set(SEAM_ARGS))} "
              f"oracle-only={sorted(set(SEAM_ARGS) - c_names)}")
    fx_io = set()
    for rec in fixtures:
        fx_io |= set((rec["in"].get("io") or {}).keys())
    if fx_io - c_names:
        bad += 1
        print(f"CROSSCHECK fixture io keys with no C seam: {sorted(fx_io - c_names)}")
    fx_funcs = {rec["func"] for rec in fixtures}
    if fx_funcs - FUNCS:
        bad += 1
        print(f"CROSSCHECK fixture funcs not extracted: {sorted(fx_funcs - FUNCS)}")
    if FUNCS - fx_funcs:
        bad += 1
        print(f"CROSSCHECK extracted funcs with no fixture: {sorted(FUNCS - fx_funcs)}")
    checks = [
        ("_HIGH_EFFORTS", _BASE_NS["_HIGH_EFFORTS"], {"high", "xhigh", "max", "ultra"}),
        ("_GEMINI_SIGNATURE_HOSTS", _BASE_NS["_GEMINI_SIGNATURE_HOSTS"],
         ("generativelanguage.googleapis.com", "aiplatform.googleapis.com")),
        ("_GEMINI_SIGNATURE_PROVIDERS", _BASE_NS["_GEMINI_SIGNATURE_PROVIDERS"],
         frozenset({"gemini", "vertex"})),
        ("GEMINI4_THINKING_LEVELS", _BASE_NS["GEMINI4_THINKING_LEVELS"], ("low", "medium", "high")),
        ("KIMI_K3_EFFORTS", _BASE_NS["KIMI_K3_EFFORTS"], ("low", "high", "max")),
        ("KIMI_K3_OVERRIDES", _BASE_NS["KIMI_K3_OVERRIDES"], {"medium": "high", "xhigh": "max"}),
    ]
    for nm, got, want in checks:
        if got != want:
            bad += 1
            print(f"CROSSCHECK constant {nm} drifted: {got!r} != {want!r}")
    return bad


def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    bad = cross_check(fixtures)
    names = FUNCS | set(SEAM_ARGS)
    for i, rec in enumerate(fixtures):
        holder = {}
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                ret = drive(rec, ns, scripts, holder)
                spec = norm(ret)
            except BaseException as e:  # noqa: BLE001
                spec = {"__raise__": type(e).__name__, "msg": str(e)}
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                msg = spec.get("msg", "")
                if any(f"'{nm}'" in msg for nm in names):
                    bad += 1
                    print(f"SELFTEST case {i} {rec['func']}: AttributeError naming ported/seam: {msg}")
            if isinstance(spec, dict) and spec.get("__raise__") == "NameError":
                bad += 1
                print(f"SELFTEST case {i} {rec['func']}: NameError (seeding gap): {spec.get('msg', '')}")
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = Path(__file__).resolve().parent / "fixtures" / "chat_completions_flow2.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()