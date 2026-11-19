#!/usr/bin/env python3
"""Oracle for the agent/agent_init.py UNCLAIMED-remainder FLOW port
(src/agent/agent_init_flow.c).

Drives the LIVE agent_init.py defs with SCRIPTED seams: every collaborator
(claimed siblings, foreign-module imports, run_agent / module loggers, self
methods, ctor/factory seams) is a recording stub fed the same scripts the C
harness feeds ai_call. Compares the DECISION SEQUENCE (ordered "trace" of
{ev,args,ret} seam events), the canonical return ("ret") and the post-call
out-state ("state"). The ported functions call each other REAL (AST-extracted
into one namespace).

Live defs are AST-extracted from the quarry (annotations kept as strings via the
__future__ flag); only the needed module constants are seeded (_EXPLICIT_API_MODES,
_RUNTIME_FIRST_PROVIDER_IDS, _TURN_STATE, _USAGE_STATE, CompressionSettings,
ActivityProvenance from agent/session_activity.py, cfg_get from hermes_cli/config.py
+ a documented DEFAULT_CONFIG subset). The heavy quarry import chain is never
imported. Every exec namespace is a registered sys.modules module (py3.14
dataclasses/Enum _is_type needs a real module alias). Fail-closed: 0 cases => exit 1;
a MISSING seam script returns a RuntimeError raise spec on BOTH sides; any
harness/oracle exception per case rides as {"__raise__": cls} (BaseException, so
SystemExit/KeyboardInterrupt/GeneratorExit propagate through except Exception).
Summary: 'ORACLE agent_init_flow: N cases, M mismatches'.

Representation contract (both sides, see src/agent/agent_init_flow.c):
- object doubles (agent/compressor/engine/candidate/tracker) are attribute
  namespaces; tuple/set -> array (sets sorted); None -> null; integral floats
  collapse to ints; uncaught exception -> {"__raise__": cls}; an exception passed
  as a logger arg projects to {"__raise__": cls, "msg": str(e)}.
- scripts: MISSING key -> fail-closed RuntimeError("no script for <name>");
  {"$seq": [...]} per-call queue (exhaustion raises RuntimeError("script exhausted:
  <name>")); {"__raise__": cls, "msg": m} raises (trace ret is the WHOLE marker);
  a present null is a real None result; any other value reused. None-valued arg
  keys dropped on both sides.
- self-method presence is declared by the fixture "methods" array; factory table
  entries fire a "factory.<name>" seam.
- fixture domain: ASCII str operands; int()/float() string operands use the 6 ASCII
  whitespace chars; URL operands bracket-free ASCII; str()-projected fields are
  str/bool/int/null; context_length/max_tokens/num_ctx fields integral ints;
  set-valued seam scripts SORTED string arrays.
"""
import __future__
import ast
import builtins
import contextlib
import copy
import json
import logging
import math
import os
import re
import sys
import threading
import time
import types
import urllib.parse
from enum import Enum
from pathlib import Path, PurePath
from types import SimpleNamespace
from typing import Any, Callable, Dict, List, Optional

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "agent_init.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "agent_init.py")
ARH_SRC = str(QUARRY / "agent" / "agent_runtime_helpers.py")
SA_SRC = str(QUARRY / "agent" / "session_activity.py")
CFG_SRC = str(QUARRY / "hermes_cli" / "config.py")

NAME = "agent_init_flow"

PORTED = [
    "_parse_config_int", "_cfg_flag", "_cfg_dict", "_positive_int", "_set_defaults",
    "_init_turn_state", "_init_usage_state", "_setup_logging", "_init_prompt_cache_config",
    "_resolve_api_mode", "_build_client", "_compression_threshold",
    "_compression_codex_settings", "_parse_compression_config",
    "_custom_provider_configured_base_url", "_configured_default_base_url",
    "_active_route_url", "_scope_context_length_to_default_runtime",
    "set_config_context_length", "config_context_length_for_runtime",
    "_select_context_engine", "_compressor_max_tokens", "_enforce_minimum_context",
    "_warn_nonagentic_hermes_model",
]
HELPERS = ["_ra"]  # from agent/agent_runtime_helpers.py
CONSTANTS = ["_EXPLICIT_API_MODES", "_RUNTIME_FIRST_PROVIDER_IDS", "_TURN_STATE", "_USAGE_STATE"]
WANTED = set(PORTED)

logging.disable(logging.CRITICAL)

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_ARH_TEXT = Path(ARH_SRC).read_text(encoding="utf-8")
_ARH_TREE = ast.parse(_ARH_TEXT)
_SA_TEXT = Path(SA_SRC).read_text(encoding="utf-8")
_SA_TREE = ast.parse(_SA_TEXT)
_CFG_TEXT = Path(CFG_SRC).read_text(encoding="utf-8")
_CFG_TREE = ast.parse(_CFG_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
CONST_CODE = {}
CLASS_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node, SRC)
    elif isinstance(_node, ast.ClassDef) and _node.name == "CompressionSettings":
        CLASS_CODE["CompressionSettings"] = _compile_node(_node, SRC)
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(tg, ast.Name) and tg.id in CONSTANTS:
            CONST_CODE[tg.id] = _compile_node(_node, SRC)
ARH_CODE = {}
for _node in _ARH_TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in HELPERS:
        ARH_CODE[_node.name] = _compile_node(_node, ARH_SRC)
SA_CODE = {}
for _node in _SA_TREE.body:
    if isinstance(_node, ast.ClassDef) and _node.name == "ActivityProvenance":
        SA_CODE["ActivityProvenance"] = _compile_node(_node, SA_SRC)
CFG_CODE = {}
for _node in _CFG_TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name == "cfg_get":
        CFG_CODE["cfg_get"] = _compile_node(_node, CFG_SRC)
_missing = ((WANTED - set(DEF_CODE)) | (set(CONSTANTS) - set(CONST_CODE))
            | (set(HELPERS) - set(ARH_CODE)) | ({"CompressionSettings"} - set(CLASS_CODE))
            | ({"ActivityProvenance"} - set(SA_CODE)) | ({"cfg_get"} - set(CFG_CODE)))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

# ── scripted seams (fail-closed on missing key) ─────────────────────────────
_SCRIPTS = None


class Scripts:
    def __init__(self, io, trace):
        self.io = io or {}
        self.trace = trace
        self.cursors = {}

    def next(self, name):
        if name not in self.io:
            return {"__raise__": "RuntimeError", "msg": f"no script for {name}"}
        script = self.io[name]
        if isinstance(script, dict) and "$seq" in script:
            seq = script["$seq"]
            i = self.cursors.get(name, 0)
            if i >= len(seq):
                return {"__raise__": "RuntimeError", "msg": f"script exhausted: {name}"}
            self.cursors[name] = i + 1
            return seq[i]
        return script


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    return cls(msg)


# ── base namespace (registered module) ──────────────────────────────────────
_BASE_NS = {
    "__name__": "ai_live_agent_init",
    "__builtins__": builtins,
    "contextlib": contextlib, "copy": copy, "json": json, "logging": logging,
    "math": math, "os": os, "re": re, "sys": sys, "threading": threading,
    "time": time, "types": types, "suppress": contextlib.suppress,
    "SimpleNamespace": SimpleNamespace, "Enum": Enum,
    "urlparse": urllib.parse.urlparse, "urlunparse": urllib.parse.urlunparse,
    "parse_qs": urllib.parse.parse_qs,
    "Any": Any, "Callable": Callable, "Dict": Dict, "List": List, "Optional": Optional,
    "MINIMUM_CONTEXT_LENGTH": 64000,
    # documented DEFAULT_CONFIG subset: only compression.threshold_tokens is read
    # by _parse_compression_config (cfg_get(DEFAULT_CONFIG, "compression",
    # "threshold_tokens") == None; verified against hermes_cli/config_defaults.py).
    "DEFAULT_CONFIG": {"compression": {"threshold_tokens": None}},
}
_MOD = types.ModuleType("ai_live_agent_init")
_MOD.__dict__.update(_BASE_NS)
sys.modules["ai_live_agent_init"] = _MOD
_BASE_NS = _MOD.__dict__
exec(SA_CODE["ActivityProvenance"], _BASE_NS)
exec(CLASS_CODE["CompressionSettings"], _BASE_NS)
for _nm in CONSTANTS:
    exec(CONST_CODE[_nm], _BASE_NS)
exec(CFG_CODE["cfg_get"], _BASE_NS)
for _nm in HELPERS:
    exec(ARH_CODE[_nm], _BASE_NS)
for _nm in PORTED:
    exec(DEF_CODE[_nm], _BASE_NS)
_REAL_TURN = _BASE_NS["_TURN_STATE"]
_REAL_USAGE = _BASE_NS["_USAGE_STATE"]


# ── canonical projection ────────────────────────────────────────────────────
def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, PurePath):
        return v.as_posix()
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, float):
        if math.isfinite(v) and v.is_integer():
            return int(v)
        return v
    return v


# ── seam arg projections + materialization ──────────────────────────────────
SEAM_ARGS = {
    "is_actual_route": lambda a, k: {"provider": a[0], "base_url": a[1]},
    "registered_api_modes": lambda a, k: {},
    "nous_api_mode": lambda a, k: {"model": a[0]},
    "host_mandated_api_mode": lambda a, k: {"base_url": a[0]},
    "base_url_host_matches": lambda a, k: {"url": a[0], "host": a[1]},
    "normalize_route_base_url": lambda a, k: {"value": a[0]},
    "normalize_custom_provider_name": lambda a, k: {"value": a[0]},
    "custom_provider_runtime_ids": lambda a, k: {"value": a[0]},
    "is_provider_enabled": lambda a, k: {"entry": a[0]},
    "resolve_provider": lambda a, k: {"provider": a[0]},
    "split_model_config_default": lambda a, k: {"default": a[0]},
    "normalize_model_for_provider": lambda a, k: {"model": a[0], "provider": a[1]},
    "context_route_mismatch": lambda a, k: {
        "configured_base_url": a[0], "active_base_url": a[1],
        "configured_provider": a[2], "active_provider": a[3], **k},
    "resolve_compression_threshold": lambda a, k: {
        "global_threshold": a[0], "model_cthresh": a[1], **k},
    "_compression_threshold_for_model": lambda a, k: {"model": a[0], "provider": a[1], **k},
    "_is_codex_gpt54_or_gpt55": lambda a, k: {"model": a[0], "provider": a[1]},
    "_is_codex_spark": lambda a, k: {"model": a[0], "provider": a[1]},
    "is_truthy_value": lambda a, k: {"value": a[0], **k},
    "_refuse_checkpoint_required_on_codex_app_server": lambda a, k: {
        "checkpoint_required": a[0], "api_mode": a[1]},
    "load_config_readonly": lambda a, k: {},
    "load_config": lambda a, k: {},
    "get_compatible_custom_providers": lambda a, k: {"cfg": a[0]},
    "cache_ttl_means_disabled": lambda a, k: {"ttl": a[0]},
    "auto_start_cache_ttl": lambda a, k: {"agent": a[0]},
    "agent._anthropic_prompt_cache_policy": lambda a, k: {},
    "agent._emit_warning": lambda a, k: {"msg": a[0]},
    "setup_logging": lambda a, k: {**k},
    "setup_verbose_logging": lambda a, k: {},
    "get_hermes_home": lambda a, k: {},
    "get_provider_request_timeout": lambda a, k: {"provider": a[0], "model": a[1]},
    "init_anthropic_client": lambda a, k: {
        "agent": a[0], "api_key": a[1], "base_url": a[2], "provider_timeout": a[3]},
    "init_moa_client": lambda a, k: {"agent": a[0], "api_key": a[1]},
    "init_bedrock_client": lambda a, k: {"agent": a[0], "base_url": a[1]},
    "init_openai_client": lambda a, k: {
        "agent": a[0], "api_key": a[1], "base_url": a[2],
        "fallback_model": a[3], "provider_timeout": a[4]},
    "safe_worker_enabled": lambda a, k: {},
    "load_context_engine": lambda a, k: {"name": a[0]},
    "get_plugin_context_engine": lambda a, k: {},
    "engine.clone_for_agent": lambda a, k: {},
    "is_native_gemini_base_url": lambda a, k: {"base_url": a[0]},
    "is_local_server": lambda a, k: {"base_url": a[0], "provider": a[1]},
    "_check_hermes_model_warning": lambda a, k: {"model": a[0]},
    "scope_terminal_cwd": lambda a, k: {},
    "SubdirectoryHintTracker": lambda a, k: {**k},
    "new_credits_latch": lambda a, k: {},
    "normalize_run_budget_seconds": lambda a, k: {"value": a[0]},
}
SET_SEAMS = {"custom_provider_runtime_ids", "registered_api_modes"}
TUPLE_SEAMS = {"resolve_compression_threshold", "split_model_config_default",
               "agent._anthropic_prompt_cache_policy"}
ENGINE_SEAMS = {"load_context_engine", "get_plugin_context_engine", "engine.clone_for_agent"}
OBJ_SEAMS = {"SubdirectoryHintTracker", "new_credits_latch"}


def _rec(name, argmap):
    def f(*a, **kw):
        return record(name, argmap(a, kw))
    return f


def materialize(name, script):
    if script is None:
        return None
    if name in SET_SEAMS:
        return frozenset(script) if isinstance(script, (list, tuple)) else script
    if name in TUPLE_SEAMS:
        return tuple(script) if isinstance(script, (list, tuple)) else script
    if name in ENGINE_SEAMS:
        if isinstance(script, dict):
            ns = SimpleNamespace(**script)
            if name == "get_plugin_context_engine":
                ns.clone_for_agent = _rec("engine.clone_for_agent", lambda a, k: {})
            return ns
        return script
    if name in OBJ_SEAMS:
        return SimpleNamespace(**script) if isinstance(script, dict) else script
    return script


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
    def __init__(self, prefix=""):
        self.prefix = prefix

    def _log(self, level, msg, args, kw):
        d = {"msg": msg, "args": list(args)}
        if kw.get("exc_info"):
            d["exc_info"] = True
        return record(self.prefix + "logger." + level, d)

    def debug(self, msg, *args, **kw):
        return self._log("debug", msg, args, kw)

    def warning(self, msg, *args, **kw):
        return self._log("warning", msg, args, kw)

    def info(self, msg, *args, **kw):
        return self._log("info", msg, args, kw)

    def error(self, msg, *args, **kw):
        return self._log("error", msg, args, kw)


_LOGGER = LoggerDouble("")
_BASE_NS["logger"] = _LOGGER
# module-level import globals that are seamed collaborators
for _nm in ("normalize_route_base_url", "base_url_host_matches", "is_truthy_value",
            "get_hermes_home", "SubdirectoryHintTracker", "get_provider_request_timeout",
            "is_local_server"):
    _BASE_NS[_nm] = SeamFn(_nm)
# claimed siblings (ported elsewhere) — seamed here under their LIVE global name,
# recording the C seam event name (no leading underscore).
for _live, _seam in (
    ("_normalize_run_budget_seconds", "normalize_run_budget_seconds"),
    ("_resolve_compression_threshold", "resolve_compression_threshold"),
    ("_context_route_mismatch", "context_route_mismatch"),
    ("_normalize_custom_provider_name", "normalize_custom_provider_name"),
    ("_custom_provider_runtime_ids", "custom_provider_runtime_ids"),
):
    _BASE_NS[_live] = _rec(_seam, SEAM_ARGS[_seam])
# the four SDK client initializers are seamed collaborators of _build_client
for _live, _seam in (
    ("_init_anthropic_client", "init_anthropic_client"),
    ("_init_moa_client", "init_moa_client"),
    ("_init_bedrock_client", "init_bedrock_client"),
    ("_init_openai_client", "init_openai_client"),
):
    _BASE_NS[_live] = SeamFn(_seam)


# ── stub modules for lazy imports ───────────────────────────────────────────
def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def install_stubs():
    agent = _mod("agent")
    agent.__path__ = []
    hc = _mod("hermes_cli")
    hc.__path__ = []
    plugins = _mod("plugins")
    plugins.__path__ = []
    prov = _mod("hermes_cli.providers",
                is_actual_route=SeamFn("is_actual_route"),
                nous_api_mode=SeamFn("nous_api_mode"),
                host_mandated_api_mode=SeamFn("host_mandated_api_mode"))
    transports = _mod("agent.transports", registered_api_modes=SeamFn("registered_api_modes"))
    cfg = _mod("hermes_cli.config",
               load_config_readonly=SeamFn("load_config_readonly"),
               load_config=SeamFn("load_config"),
               get_compatible_custom_providers=SeamFn("get_compatible_custom_providers"),
               is_provider_enabled=SeamFn("is_provider_enabled"),
               split_model_config_default=SeamFn("split_model_config_default"))
    arh = _mod("agent.agent_runtime_helpers",
               cache_ttl_means_disabled=SeamFn("cache_ttl_means_disabled"),
               _ra=_BASE_NS["_ra"])
    pc = _mod("agent.prompt_caching", AUTO_CACHE_TTL="auto",
              auto_start_cache_ttl=SeamFn("auto_start_cache_ttl"))
    hl = _mod("hermes_logging", setup_logging=SeamFn("setup_logging"),
              setup_verbose_logging=SeamFn("setup_verbose_logging"))
    aux = _mod("agent.auxiliary_client",
               _compression_threshold_for_model=SeamFn("_compression_threshold_for_model"),
               _is_codex_gpt54_or_gpt55=SeamFn("_is_codex_gpt54_or_gpt55"),
               _is_codex_spark=SeamFn("_is_codex_spark"))
    ccc = _mod("agent.conversation_compression_checkpoint",
               _refuse_checkpoint_required_on_codex_app_server=SeamFn(
                   "_refuse_checkpoint_required_on_codex_app_server"))
    auth = _mod("hermes_cli.auth", resolve_provider=SeamFn("resolve_provider"))
    mn = _mod("hermes_cli.model_normalize",
              normalize_model_for_provider=SeamFn("normalize_model_for_provider"))
    swp = _mod("agent.safe_worker_policy", safe_worker_enabled=SeamFn("safe_worker_enabled"))
    pce = _mod("plugins.context_engine", load_context_engine=SeamFn("load_context_engine"))
    hcp = _mod("hermes_cli.plugins", get_plugin_context_engine=SeamFn("get_plugin_context_engine"))
    gna = _mod("agent.gemini_native_adapter", GEMINI_DEFAULT_MAX_OUTPUT_TOKENS=65535,
               is_native_gemini_base_url=SeamFn("is_native_gemini_base_url"))
    ms = _mod("hermes_cli.model_switch",
              _check_hermes_model_warning=SeamFn("_check_hermes_model_warning"))
    rc = _mod("agent.runtime_cwd", scope_terminal_cwd=SeamFn("scope_terminal_cwd"))
    ct = _mod("agent.credits_tracker", new_credits_latch=SeamFn("new_credits_latch"))
    ra = SimpleNamespace()
    ra.logger = LoggerDouble("ra.")
    sys.modules["run_agent"] = ra
    agent.transports = transports
    agent.agent_runtime_helpers = arh
    agent.prompt_caching = pc
    agent.auxiliary_client = aux
    agent.conversation_compression_checkpoint = ccc
    agent.safe_worker_policy = swp
    agent.gemini_native_adapter = gna
    agent.runtime_cwd = rc
    agent.credits_tracker = ct
    hc.providers = prov
    hc.config = cfg
    hc.auth = auth
    hc.model_normalize = mn
    hc.plugins = hcp
    hc.model_switch = ms
    plugins.context_engine = pce


install_stubs()


def _wrap_factories(table):
    out = {}
    for k, v in table.items():
        if callable(v):
            out[k] = _rec("factory." + k, lambda a, kk: {})
        else:
            out[k] = v
    return out


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    trace = []
    scripts = Scripts(inp.get("io") or {}, trace)
    _SCRIPTS = scripts
    _BASE_NS["logger"] = _LOGGER
    sys.modules["run_agent"].logger = LoggerDouble("ra.")
    # per-case factory-wrapped copies of the state tables (callable entries seam)
    _BASE_NS["_TURN_STATE"] = _wrap_factories(_REAL_TURN)
    _BASE_NS["_USAGE_STATE"] = _wrap_factories(_REAL_USAGE)
    return _BASE_NS, trace, scripts


# ── input doubles ───────────────────────────────────────────────────────────
def mk_agent(d, methods=()):
    d = d or {}
    ns = SimpleNamespace()
    for k, v in d.items():
        if k == "context_compressor" and isinstance(v, dict):
            setattr(ns, k, SimpleNamespace(**v))
        else:
            setattr(ns, k, v)
    for m in methods or []:
        setattr(ns, m, _rec("agent." + m, SEAM_ARGS["agent." + m]))
    return ns


TURN_FIELDS = [
    "_iteration_budget_warning_injected", "_budget_exhausted_injected", "_budget_grace_call",
    "_run_budget_started_at", "_run_budget_wrapup_injected", "_last_activity_ts",
    "_last_activity_desc", "_last_activity_provenance", "_session_activity_last_persist_mono",
    "_current_tool", "_api_call_count", "_skip_mcp_refresh", "_tool_snapshot_generation",
    "_rate_limit_state", "_credits_state", "_credits_session_start_micros", "_or_cache_hits",
    "run_budget_seconds", "_credits_latch",
]
USAGE_FIELDS = [
    "_user_turn_count", "_is_user_initiated_turn", "_usage_anchor", "_turn_base_usage_anchor",
    "_request_pressure_anchored", "session_prompt_tokens", "session_completion_tokens",
    "session_total_tokens", "session_api_calls", "session_input_tokens", "session_output_tokens",
    "session_cache_read_tokens", "session_cache_write_tokens", "session_reasoning_tokens",
    "session_estimated_cost_usd", "session_cost_status", "session_cost_source",
    "_api_latency_history", "_api_output_history", "_subdirectory_hints",
]
PC_FIELDS = ["_use_prompt_caching", "_use_native_cache_layout", "_cache_disabled",
             "_cache_ttl", "_cache_ttl_auto"]


def _echo(agent, fields):
    return {f: norm(getattr(agent, f, None)) for f in fields}


def _factory_table(spec):
    out = {}
    for k, v in (spec or {}).items():
        if isinstance(v, dict) and v.get("__factory__"):
            out[k] = _rec("factory." + k, lambda a, kk: {})
        else:
            out[k] = v
    return out


# ── live driver ─────────────────────────────────────────────────────────────
def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    methods = inp.get("methods") or []
    if func == "_parse_config_int":
        return ns[func](inp.get("raw"), inp.get("default", 0))
    if func == "_cfg_flag":
        return ns[func](inp.get("cfg") or {}, inp.get("key", ""), inp.get("default", False))
    if func == "_cfg_dict":
        return ns[func](inp.get("cfg") or {}, inp.get("key", ""))
    if func == "_positive_int":
        reject = tuple(inp.get("reject") or [])
        # map type-name strings to real types for the isinstance reject gate
        tmap = {"bool": bool, "float": float, "int": int, "str": str}
        reject_types = tuple(tmap[t] for t in reject if t in tmap)
        return ns[func](inp.get("raw"), reject=reject_types)
    if func == "_set_defaults":
        agent = mk_agent(inp.get("agent"), methods)
        table = _factory_table(inp.get("table"))
        ns[func](agent, table)
        holder["state"] = {k: norm(getattr(agent, k, None)) for k in table}
        return None
    if func == "_init_turn_state":
        agent = mk_agent(inp.get("agent"), methods)
        ns[func](agent, inp.get("run_budget_seconds"))
        holder["state"] = _echo(agent, TURN_FIELDS)
        return None
    if func == "_init_usage_state":
        agent = mk_agent(inp.get("agent"), methods)
        ns[func](agent)
        holder["state"] = _echo(agent, USAGE_FIELDS)
        return None
    if func == "_setup_logging":
        agent = mk_agent(inp.get("agent"), methods)
        ns[func](agent)
        return None
    if func == "_init_prompt_cache_config":
        agent = mk_agent(inp.get("agent"), methods)
        ns[func](agent)
        holder["state"] = _echo(agent, PC_FIELDS)
        return None
    if func == "_resolve_api_mode":
        agent = mk_agent(inp.get("agent"), methods)
        ns[func](agent, inp.get("api_mode"), inp.get("provider_name"), inp.get("base_url"))
        holder["state"] = _echo(agent, ["api_mode", "provider"])
        return None
    if func == "_build_client":
        agent = mk_agent(inp.get("agent"), methods)
        ns[func](agent, inp.get("api_key"), inp.get("base_url"), inp.get("fallback_model"))
        holder["state"] = _echo(agent, ["_anthropic_client", "_is_anthropic_oauth"])
        return None
    if func == "_compression_threshold":
        agent = mk_agent(inp.get("agent"), methods)
        ret = ns[func](agent, inp.get("cfg") or {})
        holder["state"] = _echo(agent, ["_compression_threshold_autoraised"])
        return ret
    if func == "_compression_codex_settings":
        return ns[func](inp.get("cfg") or {})
    if func == "_parse_compression_config":
        agent = mk_agent(inp.get("agent"), methods)
        ret = ns[func](agent, inp.get("agent_cfg") or {})
        holder["state"] = _echo(agent, ["_compression_threshold_autoraised"])
        return ret
    if func == "_custom_provider_configured_base_url":
        return ns[func](inp.get("configured_provider"), inp.get("agent_cfg") or {},
                        inp.get("custom_providers") or [])
    if func == "_configured_default_base_url":
        return ns[func](inp.get("agent_cfg") or {}, inp.get("model_cfg") or {},
                        inp.get("custom_providers") or [])
    if func == "_active_route_url":
        agent = mk_agent(inp.get("agent"), methods)
        return ns[func](agent, inp.get("base_url"))
    if func == "_scope_context_length_to_default_runtime":
        agent = mk_agent(inp.get("agent"), methods)
        return ns[func](agent, inp.get("agent_cfg") or {}, inp.get("model_cfg") or {},
                        inp.get("custom_providers") or [], inp.get("config_context_length"),
                        inp.get("base_url"))
    if func == "set_config_context_length":
        agent = mk_agent(inp.get("agent"), methods)
        ns[func](agent, inp.get("value"))
        holder["state"] = {
            "_config_context_length": norm(getattr(agent, "_config_context_length", None)),
            "context_compressor": norm(getattr(agent, "context_compressor", None)),
        }
        return None
    if func == "config_context_length_for_runtime":
        agent = mk_agent(inp.get("agent"), methods)
        return ns[func](agent, inp.get("config"))
    if func == "_select_context_engine":
        return ns[func](inp.get("agent_cfg"))
    if func == "_compressor_max_tokens":
        agent = mk_agent(inp.get("agent"), methods)
        return ns[func](agent)
    if func == "_enforce_minimum_context":
        agent = mk_agent(inp.get("agent"), methods)
        ns[func](agent)
        return None
    if func == "_warn_nonagentic_hermes_model":
        agent = mk_agent(inp.get("agent"), methods)
        ns[func](agent)
        return None
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
    faulthandler.dump_traceback_later(120, exit=True)
    bad = 0
    ported_set = set(PORTED)
    for i, rec in enumerate(fixtures):
        holder = {}
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                ret = drive(rec, ns, scripts, holder)
                spec = norm(ret)
            except BaseException as e:  # noqa: BLE001
                spec = {"__raise__": type(e).__name__}
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
        fx = Path(__file__).resolve().parent / "fixtures" / "agent_init_flow.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").split("\n") if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()