#!/usr/bin/env python3
"""Oracle for the agent/agent_runtime_helpers.py UNCLAIMED-remainder FLOW port
(src/agent/agent_runtime_helpers_flow2.c).

Drives the LIVE agent_runtime_helpers.py defs with SCRIPTED seams: every
collaborator (claimed siblings, foreign-module imports, run_agent logger /
AIAgent statics, pool methods, plugin hooks, session-rule id variants, logger)
is a recording stub fed the same scripts the C harness feeds arh2_call.
Compares the DECISION SEQUENCE (ordered "trace" of {ev,args,ret} seam events),
the canonical return ("ret") and the post-call out-state ("state"). The ported
functions call each other REAL (AST-extracted into one namespace).

Live defs are AST-extracted from the quarry (annotations kept as strings via the
__future__ flag); only the needed module constants are seeded (_SWITCH_SNAPSHOT_FIELDS
needs ROUTE_VERDICT_FIELDS from agent/route_verdicts.py; _MISSING sentinel). The
heavy quarry import chain is never imported. Every exec namespace is a registered
sys.modules module (py3.14 dataclasses/NamedTuple _is_type needs a real module
alias). Fail-closed: 0 cases => exit 1; any harness/oracle exception per case rides
as {"__raise__": cls} (BaseException, so SystemExit/KeyboardInterrupt propagate).
Summary: 'ORACLE agent_runtime_helpers_flow2: N cases, M mismatches'.

Representation contract (both sides, see src/agent/agent_runtime_helpers_flow2.c):
- object doubles (agent/pool/entry/error.response) are attribute namespaces;
  tuple/set -> array (sets sorted); None -> null; integral floats collapse to
  ints; the _MISSING sentinel -> {"__missing__": true}; uncaught exception ->
  {"__raise__": cls}; an exception passed as a logger arg projects to
  {"__raise__": cls, "msg": str(e)}.
- scripts: absent key -> None; {"$seq": [...]} per-call queue (exhaustion raises
  RuntimeError("script exhausted: <name>")); {"__raise__": cls, "msg": m} raises
  (trace ret is the WHOLE marker); any other value reused. None-valued arg keys
  dropped on both sides.
- object-valued seam results consumed by `if obj:` truthiness (pool.current) are
  null or a NON-EMPTY record; empty {} is outside that live domain.
- fixture domain: ASCII str operands; .strip() ASCII; tool_call entries are dicts
  with dict-or-null function; function.name / tool_call_id str-or-null; json.dumps
  content ASCII scalars/containers (no non-integral floats); agent doubles are
  plain attribute namespaces (no property setters).
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
import types
from pathlib import Path, PurePath
from typing import Any, Dict, List, Optional, Tuple

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "agent_runtime_helpers.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "agent_runtime_helpers.py")
RV_SRC = str(QUARRY / "agent" / "route_verdicts.py")

NAME = "agent_runtime_helpers_flow2"

PORTED = [
    "_prepend_corruption_marker", "_find_tool_result", "_drop_invalid_roles",
    "_drop_empty_tool_calls_arrays", "_repair_invalid_tool_call_names",
    "_drop_results_without_ids", "_pair_tool_calls_positionally",
    "_dedupe_tool_call_ids", "_realign_tool_result_names", "tool_results_this_turn",
    "_api_error_debug_info", "sync_credential_pool_entry_id",
    "_failed_credential_identity", "_restore_runtime_capabilities",
    "_apply_primary_runtime_fields", "_revert_credential_rotation",
    "_requeue_pending_steer", "_pre_tool_block_message", "_snapshot_switch_state",
    "_restore_switch_snapshot", "_recover_auth_failure",
    # UNCLAIMED remainder (27 fns)
    "_cursor_skip_prefix", "_recover_rate_limit", "_build_anthropic_client_from_runtime",
    "_rebuild_primary_client", "_primary_reset_gate_blocks", "_rebind_primary_credential_pool",
    "_primary_quota_reopened_early", "_moa_aggregator_cache_policy", "_route_may_be_custom",
    "_provider_supplied_client", "_ensure_route_client_headers", "_gemini_native_client",
    "_apply_switched_provider_request_overrides", "_resolve_switch_destination",
    "_build_switched_client", "_swap_switch_runtime", "_resolve_switch_context_length",
    "_update_switch_compressor", "_finish_switch", "_persist_switch_billing_route",
    "_log_empty_non_final_heal", "looks_like_degenerate_final",
    "promoted_reasoning_announces_action", "_socket_from_stream", "_socket_from_candidate",
    "_socket_from_response", "_shutdown_socket",
]
HELPERS = ["_ra"]
CONSTANTS = [
    "_SWITCH_SNAPSHOT_FIELDS", "_MISSING",
    "_USAGE_LIMIT_REASON_TOKENS", "_USAGE_LIMIT_MESSAGE_TOKENS",
    "_DEGENERATE_FINAL_MAX_CHARS", "_SENTENCE_TERMINALS", "_DEGENERATE_LEADING_PUNCT",
    "_PROMOTED_REASONING_PLAN_TAIL_RE", "_EMPTY_HEAL_WINDOW_S", "_EMPTY_HEAL_MAX_SESSIONS",
]
RV_CONSTANTS = ["ROUTE_REJECTION_FIELDS", "ROUTE_VERDICT_FIELDS"]
WANTED = set(PORTED) | set(HELPERS)

logging.disable(logging.CRITICAL)

DEFAULT_ROLES = ["system", "user", "assistant", "tool", "function", "developer"]
DEFAULT_MARKER = ("[hermes-agent: tool call arguments were corrupted in this session and "
                  "have been dropped to keep the conversation alive. See issue #15236.]")

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_RV_TEXT = Path(RV_SRC).read_text(encoding="utf-8")
_RV_TREE = ast.parse(_RV_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
CONST_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node, SRC)
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(tg, ast.Name) and tg.id in CONSTANTS:
            CONST_CODE[tg.id] = _compile_node(_node, SRC)
RV_CODE = {}
for _node in _RV_TREE.body:
    if isinstance(_node, (ast.Assign, ast.AnnAssign)):
        tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(tg, ast.Name) and tg.id in RV_CONSTANTS:
            RV_CODE[tg.id] = _compile_node(_node, RV_SRC)
_missing = (WANTED - set(DEF_CODE)) | (set(CONSTANTS) - set(CONST_CODE)) | (set(RV_CONSTANTS) - set(RV_CODE))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

# ── scripted seams ──────────────────────────────────────────────────────────
_SCRIPTS = None
_CURRENT_ENV = {}


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


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    return cls(msg)


# ── base namespace (registered module) ──────────────────────────────────────
_BASE_NS = {
    "__name__": "arh2_live_agent_runtime_helpers",
    "__builtins__": builtins,
    "contextlib": contextlib, "copy": copy, "json": json, "logging": logging,
    "math": math, "re": re, "threading": threading, "types": types,
    "Any": Any, "Dict": Dict, "List": List, "Optional": Optional, "Tuple": Tuple,
}
_MOD = types.ModuleType("arh2_live_agent_runtime_helpers")
_MOD.__dict__.update(_BASE_NS)
sys.modules["arh2_live_agent_runtime_helpers"] = _MOD
_BASE_NS = _MOD.__dict__
for _nm in RV_CONSTANTS:
    exec(RV_CODE[_nm], _BASE_NS)
for _nm in CONSTANTS:
    exec(CONST_CODE[_nm], _BASE_NS)
for _nm in HELPERS + PORTED:
    exec(DEF_CODE[_nm], _BASE_NS)
_MISSING_OBJ = _BASE_NS["_MISSING"]


# ── canonical projection ────────────────────────────────────────────────────
def norm(v):
    if v is _MISSING_OBJ:
        return {"__missing__": True}
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
    if isinstance(v, float):
        if math.isfinite(v) and v.is_integer():
            return int(v)
        return v
    return v


# ── seam arg projections + materialization ──────────────────────────────────
SEAM_ARGS = {
    "ra._get_tool_call_id_static": lambda a, k: {"tc": a[0]},
    "ra._get_tool_call_name_static": lambda a, k: {"tc": a[0]},
    "tool_call_id_variants": lambda a, k: {"tc": a[0]},
    "tool_result_id_variants": lambda a, k: {"tool_call_id": a[0]},
    "coalesce_tool_call_id": lambda a, k: {"tc": a[0]},
    "coerce_tool_name": lambda a, k: {"name": a[0]},
    "is_actual_route": lambda a, k: {"provider": a[0], "base_url": a[1]},
    "recover_auth_failure": lambda a, k: {"agent": a[0], "pool": a[1], **k},
    "requeue_drained_steer": lambda a, k: {"agent": a[0], "steer_text": a[1]},
    "_dispatch_pre_tool_call_hooks": lambda a, k: {
        "function_name": a[0], "function_args": a[1],
        "task_id": k["task_id"], "session_id": k["session_id"],
        "tool_call_id": k["tool_call_id"], "turn_id": k["turn_id"],
        "api_request_id": k["api_request_id"], "middleware_trace": k["middleware_trace"]},
    "pool.entry_id_for_api_key": lambda a, k: {"api_key": a[0]},
    "pool.current": lambda a, k: {},
    "pool.reclaim": lambda a, k: {"revert_id": a[0], "model": k["model"]},
    "agent._swap_credential": lambda a, k: {"entry": a[0]},
}
VARIANT_SEAMS = {"tool_call_id_variants", "tool_result_id_variants"}
ENTRY_SEAMS = {"pool.current", "pool.reclaim", "pool.entries"}
TUPLE_SEAMS = {"_dispatch_pre_tool_call_hooks"}

# ── UNCLAIMED-remainder seam arg maps ──────────────────────────────────────
SEAM_ARGS.update({
    "pool.entries": lambda a, k: {},
    "rotate_and_swap": lambda a, k: {"code": a[0], "reason": a[1]},
    "get_provider_request_timeout": lambda a, k: {"provider": a[0], "model": a[1]},
    "build_anthropic_client": lambda a, k: {"api_key": a[0], "base_url": a[1], "timeout": k.get("timeout")},
    "build_moa_facade": lambda a, k: {"agent": a[0], "model": a[1],
                                      **({"api_key": a[2]} if len(a) > 2 else {})},
    "agent._create_openai_client": lambda a, k: {"client_kwargs": a[0], "reason": k.get("reason"),
                                                 "shared": k.get("shared")},
    "bind_bedrock_runtime": lambda a, k: {"agent": a[0], "base_url": a[1], "api_mode": a[2]},
    "matches_primary": lambda a, k: {"obj": a[0]},
    "load_primary_pool": lambda a, k: {},
    "pool.next_available_at": lambda a, k: {"model": k.get("model")},
    "datetime.isoformat": lambda a, k: {"ts": a[0]},
    "pool.has_available": lambda a, k: {"model": k.get("model")},
    "pool.select": lambda a, k: {"model": k.get("model")},
    "agent._swap_credential": lambda a, k: {"entry": a[0]},
    "pool.lift_reopened_cooldowns": lambda a, k: {"model": k.get("model")},
    "_load_moa_cfg": lambda a, k: {},
    "resolve_moa_preset": lambda a, k: {"config": a[0], "model": a[1]},
    "resolve_runtime_provider": lambda a, k: {"requested": k.get("requested"),
                                              "target_model": k.get("target_model")},
    "anthropic_prompt_cache_policy": lambda a, k: {"agent": a[0], **k},
    "normalize_route_base_url": lambda a, k: {"url": a[0]},
    "custom_provider_aliases": lambda a, k: {"name": a[0], "provider_key": a[1]},
    "base_url_hostname": lambda a, k: {"url": a[0]},
    "get_provider": lambda a, k: {"provider": a[0], "allow_network": k.get("allow_network")},
    "get_provider_profile": lambda a, k: {"name": a[0]},
    "_profile_for_base_url": lambda a, k: {"base_url": a[0]},
    "profile.create_client": lambda a, k: dict(k),
    "route_answer": lambda a, k: {"kind": a[0], **k},
    "is_native_gemini_base_url": lambda a, k: {"base_url": a[0]},
    "agent._build_keepalive_http_client": lambda a, k: {"base_url": a[0], "verify": k.get("verify")},
    "GeminiNativeClient": lambda a, k: dict(k),
    "agent._client_log_context": lambda a, k: {},
    "load_config": lambda a, k: {},
    "get_compatible_custom_providers": lambda a, k: {"config": a[0]},
    "_custom_provider_extra_body_for_agent": lambda a, k: dict(k),
    "determine_api_mode": lambda a, k: {"provider": a[0], "base_url": a[1], "model": k.get("model")},
    "is_actual_route": lambda a, k: {"provider": a[0], "base_url": a[1]},
    "normalize_actual_base_url": lambda a, k: {"base_url": a[0]},
    "resolve_native_compaction_capabilities": lambda a, k: dict(k),
    "opencode_provider_family": lambda a, k: {"provider": a[0]},
    "bind_moa_runtime": lambda a, k: {"agent": a[0], "model": a[1], "api_key": a[2]},
    "native_anthropic_fallback_token": lambda a, k: dict(k),
    "build_minimax_oauth_token_provider": lambda a, k: {},
    "anthropic_route_is_oauth": lambda a, k: {"base_url": a[0], "api_key": a[1], "provider": k.get("provider")},
    "load_config_readonly": lambda a, k: {},
    "apply_custom_provider_tls_to_client_kwargs": lambda a, k: {"client_kwargs": a[0], "base_url": a[1],
                                                                 "custom_providers": k.get("custom_providers")},
    "agent._apply_client_headers_for_base_url": lambda a, k: {"base_url": a[0]},
    "agent._read_reasoning_echo_from_config": lambda a, k: {},
    "reset_route_verdicts": lambda a, k: {"agent": a[0]},
    "load_route_pool": lambda a, k: {"provider": k.get("provider"), "base_url": k.get("base_url")},
    "sync_credential_pool_entry_id": lambda a, k: {"agent": a[0]},
    "config_context_length_for_runtime": lambda a, k: {"agent": a[0], "config": a[1]},
    "get_custom_provider_context_length": lambda a, k: dict(k),
    "set_config_context_length": lambda a, k: {"agent": a[0], "intent": k.get("intent")},
    "agent._ensure_lmstudio_runtime_loaded": lambda a, k: {"intent": k.get("intent")},
    "agent._lmstudio_load_was_unverified": lambda a, k: {"runtime_len": k.get("runtime_len")},
    "agent._effective_lmstudio_context_length": lambda a, k: dict(k),
    "get_model_context_length": lambda a, k: dict(k),
    "agent.context_compressor.update_model": lambda a, k: dict(k),
    "revalidate_compression_feasibility": lambda a, k: {"agent": a[0]},
    "agent_interface": lambda a, k: {"agent": a[0]},
    "store.usage.set_billing_route": lambda a, k: dict(k),
    "session_db.update_session_billing_route": lambda a, k: dict(k),
    "_session_id_for_heal_log": lambda a, k: {},
    "_heal_escalation_threshold": lambda a, k: {},
    "publish_lru": lambda a, k: {"key": a[1], "state": norm(a[2])},
    "heal_user_notice.add": lambda a, k: dict(k),
    "heal_pending_notice.set": lambda a, k: dict(k),
    "_summarize_user_message_for_log": lambda a, k: {"message": a[0]},
    "promoted_reasoning_plan_tail_re.search": lambda a, k: {"text": a[0]},
    "stream.get_extra_info": lambda a, k: {"name": a[0]},
    "stream._stream.extra": lambda a, k: {},
    "connection_candidates": lambda a, k: {"start": a[0]},
    "sock.settimeout": lambda a, k: {"timeout": a[0]},
    "sock.shutdown": lambda a, k: {"how": a[0]},
})


def materialize(name, script):
    if script is None:
        return None
    if name in VARIANT_SEAMS:
        return frozenset(script) if isinstance(script, (list, tuple)) else script
    if name == "pool.entries" and isinstance(script, list):
        return [types.SimpleNamespace(**x) if isinstance(x, dict) else x for x in script]
    if name in ENTRY_SEAMS:
        return types.SimpleNamespace(**script) if isinstance(script, dict) else script
    if name in TUPLE_SEAMS:
        return tuple(script) if isinstance(script, (list, tuple)) else script
    if name == "agent_interface" and isinstance(script, dict):
        return types.SimpleNamespace(usage=types.SimpleNamespace(
            set_billing_route=SeamFn("store.usage.set_billing_route")))
    if name == "load_primary_pool" and isinstance(script, dict):
        return mk_pool_full(script)
    if name == "connection_candidates" and isinstance(script, list):
        # The live _socket_from_candidate uses attribute access (getattr(x,
        # "_sock")), so yielded candidates must be namespaces, not raw dicts.
        return [mk_socket_obj(x) for x in script]
    if name == "custom_provider_aliases" and isinstance(script, (list, tuple)):
        # Live helper returns a SET; provider_ids & aliases needs a set.
        return frozenset(script)
    if name == "get_provider" and isinstance(script, dict):
        # Live code reads provider_def.base_url (attribute access).
        return types.SimpleNamespace(**script)
    if name == "pool.select" and isinstance(script, dict):
        return types.SimpleNamespace(**script)
    if name in {"get_provider_profile", "_profile_for_base_url"} and isinstance(script, dict):
        d = dict(script)
        d.pop("create_client", None)
        return types.SimpleNamespace(create_client=SeamFn("profile.create_client"), **d)
    return script


def record(name, args):
    scripts = _SCRIPTS
    if not isinstance(args, dict):
        args = {"value": args}
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
        argmap = SEAM_ARGS.get(self.name)
        return record(self.name, argmap(a, kw) if argmap else dict(kw))


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
for _nm in ("tool_call_id_variants", "tool_result_id_variants", "coalesce_tool_call_id",
            "coerce_tool_name"):
    _BASE_NS[_nm] = SeamFn(_nm)


def _rec(name, argmap=None):
    def f(*a, **kw):
        am = argmap or SEAM_ARGS.get(name)
        return record(name, am(a, kw) if am else dict(kw))
    return f


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
    cc = _mod("agent.context_compressor", _DB_PERSISTED_MARKER="_db_persisted")
    prov = _mod("hermes_cli.providers", determine_api_mode=SeamFn("determine_api_mode"),
                is_actual_route=SeamFn("is_actual_route"),
                custom_provider_aliases=SeamFn("custom_provider_aliases"),
                get_provider=SeamFn("get_provider"))
    car = _mod("agent.credential_auth_recovery", recover_auth_failure=SeamFn("recover_auth_failure"))
    ic = _mod("agent.interrupt_control", requeue_drained_steer=SeamFn("requeue_drained_steer"))
    pl = _mod("hermes_cli.plugins", _dispatch_pre_tool_call_hooks=SeamFn("_dispatch_pre_tool_call_hooks"))
    agent.context_compressor = cc
    agent.credential_auth_recovery = car
    agent.interrupt_control = ic
    hc.providers = prov
    hc.plugins = pl



def _smod(path, pairs):
    parts = path.split(".")
    m = types.ModuleType(path)
    for attr, seam in pairs.items():
        setattr(m, attr, SeamFn(seam))
    sys.modules[path] = m
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        parent = sys.modules.get(pn)
        if parent is None:
            parent = types.ModuleType(pn)
            parent.__path__ = []
            sys.modules[pn] = parent
        setattr(parent, parts[i], m)
    return m


def install_stubs2():
    for path, pairs in [
        ("agent.anthropic_adapter", {"build_anthropic_client": "build_anthropic_client"}),
        ("agent.moa_loop", {"build_moa_facade": "build_moa_facade", "bind_moa_runtime": "bind_moa_runtime"}),
        ("agent.bedrock_adapter", {"bind_bedrock_runtime": "bind_bedrock_runtime"}),
        ("agent.anthropic_credentials", {
            "native_anthropic_fallback_token": "native_anthropic_fallback_token",
            "anthropic_route_is_oauth": "anthropic_route_is_oauth"}),
        ("agent.custom_entry_pool", {"load_route_pool": "load_route_pool"}),
        ("agent.provider_route_hooks", {"route_answer": "route_answer"}),
        ("agent.gemini_native_adapter", {
            "GeminiNativeClient": "GeminiNativeClient",
            "is_native_gemini_base_url": "is_native_gemini_base_url"}),
        ("agent.model_metadata", {"get_model_context_length": "get_model_context_length"}),
        ("agent.conversation_compression_feasibility", {
            "revalidate_compression_feasibility": "revalidate_compression_feasibility"}),
        ("agent.native_compaction", {
            "resolve_native_compaction_capabilities": "resolve_native_compaction_capabilities"}),
        ("agent.codex_responses_adapter", {
            "_summarize_user_message_for_log": "_summarize_user_message_for_log"}),
        ("agent.agent_init_session", {"agent_interface": "agent_interface"}),
        ("agent.agent_init", {
            "_custom_provider_extra_body_for_agent": "_custom_provider_extra_body_for_agent",
            "config_context_length_for_runtime": "config_context_length_for_runtime",
            "set_config_context_length": "set_config_context_length"}),
        ("hermes_cli.timeouts", {"get_provider_request_timeout": "get_provider_request_timeout"}),
        ("hermes_cli.moa_config", {"resolve_moa_preset": "resolve_moa_preset"}),
        ("hermes_cli.config", {"load_config": "load_config",
                               "load_config_readonly": "load_config_readonly",
                               "get_compatible_custom_providers": "get_compatible_custom_providers",
                               "apply_custom_provider_tls_to_client_kwargs":
                                   "apply_custom_provider_tls_to_client_kwargs",
                               "get_custom_provider_context_length": "get_custom_provider_context_length"}),
        ("hermes_cli.runtime_provider", {"resolve_runtime_provider": "resolve_runtime_provider"}),
        ("hermes_cli.route_identity", {"normalize_route_base_url": "normalize_route_base_url"}),
        ("hermes_cli.auth", {"build_minimax_oauth_token_provider": "build_minimax_oauth_token_provider",
                             "normalize_actual_base_url": "normalize_actual_base_url"}),
        ("hermes_cli.auth_codex", {"CODEX_QUOTA_PROBE_MIN_INTERVAL_SECONDS": 30.0}),
        ("hermes_cli.models", {"opencode_provider_family": "opencode_provider_family"}),
        ("utils", {"base_url_hostname": "base_url_hostname"}),
        ("providers", {"get_provider_profile": "get_provider_profile"}),
    ]:
        _smod(path, pairs)
    for nm, seam in [
        ("get_provider_request_timeout", "get_provider_request_timeout"),
        ("base_url_hostname", "base_url_hostname"),
        ("_profile_for_base_url", "_profile_for_base_url"),
        ("publish_lru", "publish_lru"),
        ("_session_id_for_heal_log", "_session_id_for_heal_log"),
        ("_heal_escalation_threshold", "_heal_escalation_threshold"),
        ("reset_route_verdicts", "reset_route_verdicts"),
        ("agent_interface", "agent_interface"),
        ("anthropic_prompt_cache_policy", "anthropic_prompt_cache_policy"),
    ]:
        _BASE_NS[nm] = SeamFn(seam)
    _BASE_NS["STATUS_EXHAUSTED"] = "exhausted"
    # socket.SHUT_RDWR is a REAL int constant (socket.SHUT_RDWR==2).  Using
    # _smod here wrapped it in a SeamFn, so sock.shutdown recorded a SeamFn
    # object as `how` instead of the integer 2.  Register the module raw.
    _mod("socket", SHUT_RDWR=2)
    # CODEX_QUOTA_PROBE_MIN_INTERVAL_SECONDS is a plain float constant; _smod
    # wrapped it in a SeamFn which made `now - checked_at < CONST` raise
    # TypeError inside _primary_quota_reopened_early.  Correct it here and
    # again per-case in build_ns() from the env override.
    sys.modules["hermes_cli.auth_codex"].CODEX_QUOTA_PROBE_MIN_INTERVAL_SECONDS = 30.0
    anyio = _smod("anyio.abc", {})
    anyio.SocketAttribute = types.SimpleNamespace(raw_socket="raw_socket")


class _RegexSeam:
    def search(self, text):
        return record("promoted_reasoning_plan_tail_re.search", {"text": text})


install_stubs2()

install_stubs()



# ── time / datetime / recording-container doubles ──────────────────────────
class _FakeTime:
    def __init__(self, env):
        self.env = env
    def time(self):
        return self.env.get("now", 0.0)
    def monotonic(self):
        return self.env.get("monotonic", 0.0)


class _FakeDateTime:
    def __init__(self, ts):
        self.ts = ts
    def isoformat(self, timespec=None):
        return record("datetime.isoformat", {"ts": self.ts})


class _FakeDatetimeMod:
    @staticmethod
    def fromtimestamp(ts):
        return _FakeDateTime(ts)


class _RecordingSet:
    def __init__(self, vals):
        self.vals = set(vals)
    def add(self, key):
        record("heal_user_notice.add", {"key": key})
        self.vals.add(key)
    def __contains__(self, k):
        return k in self.vals


class _RecordingDict(dict):
    def __setitem__(self, key, value):
        record("heal_pending_notice.set", {"key": key, "total_events": value.get("total_events") if isinstance(value, dict) else None})
        super().__setitem__(key, value)


AGENT_METHODS = {
    "_swap_credential": "agent._swap_credential",
    "_create_openai_client": "agent._create_openai_client",
    "_build_keepalive_http_client": "agent._build_keepalive_http_client",
    "_client_log_context": "agent._client_log_context",
    "_apply_client_headers_for_base_url": "agent._apply_client_headers_for_base_url",
    "_read_reasoning_echo_from_config": "agent._read_reasoning_echo_from_config",
    "_ensure_lmstudio_runtime_loaded": "agent._ensure_lmstudio_runtime_loaded",
    "_lmstudio_load_was_unverified": "agent._lmstudio_load_was_unverified",
    "_effective_lmstudio_context_length": "agent._effective_lmstudio_context_length",
}
POOL_METHODS = {
    "next_available_at": "pool.next_available_at",
    "has_available": "pool.has_available",
    "select": "pool.select",
    "lift_reopened_cooldowns": "pool.lift_reopened_cooldowns",
}
BASE_FIELDS = [
    "_anthropic_api_key", "_anthropic_base_url", "_anthropic_client", "_is_anthropic_oauth", "client",
    "_restore_wait_logged", "_codex_reopen_checked_at", "request_overrides", "_config_context_length",
    "api_key", "_client_kwargs", "_credential_pool_entry_id",
]


def mk_pool_full(d):
    d = d or {}
    ns = types.SimpleNamespace(**{k: v for k, v in d.items() if k not in POOL_METHODS})
    for k, seam in POOL_METHODS.items():
        setattr(ns, k, SeamFn(seam))
    return ns


def mk_agent_full(d):
    d = d or {}
    ns = types.SimpleNamespace()
    for k, v in d.items():
        if k == "_credential_pool" and isinstance(v, dict):
            ns._credential_pool = mk_pool_full(v)
        elif k == "_session_db" and isinstance(v, dict):
            # Live _session_db is an object exposing update_session_billing_route;
            # a bare dict here made the live fn raise AttributeError.
            ns._session_db = types.SimpleNamespace(
                update_session_billing_route=SeamFn("session_db.update_session_billing_route"), **v)
        elif k in AGENT_METHODS:
            continue
        elif k == "context_compressor":
            continue
        else:
            setattr(ns, k, v)
    for k, seam in AGENT_METHODS.items():
        if k == "_ensure_lmstudio_runtime_loaded" and not _CURRENT_ENV.get("has_lmstudio", True):
            continue
        if k == "_lmstudio_load_was_unverified" and not _CURRENT_ENV.get("has_lmstudio_unverified", False):
            continue
        if k == "_effective_lmstudio_context_length" and not _CURRENT_ENV.get("has_effective_lmstudio", False):
            continue
        setattr(ns, k, SeamFn(seam))
    ns.context_compressor = types.SimpleNamespace(update_model=SeamFn("agent.context_compressor.update_model"))
    for _f in ("model", "provider", "requested_provider", "base_url", "api_mode", "api_key", "client",
               "_anthropic_client", "_anthropic_api_key", "_anthropic_base_url", "_is_anthropic_oauth",
               "_client_kwargs", "_credential_pool", "_credential_pool_entry_id", "request_overrides",
               "_config_context_length", "_reasoning_echo_flag", "runtime_capabilities",
               "_fallback_chain", "_fallback_model", "_restore_wait_logged",
               "_fallback_activated", "_provider_fallback_active", "_provider_fallback_route", "_fallback_index",
               "_credential_pool_revert_id"):
        if not hasattr(ns, _f):
            setattr(ns, _f, None)
    return ns


def mk_socket_obj(d):
    if not isinstance(d, dict):
        return d
    out = types.SimpleNamespace()
    for k, v in d.items():
        if k in ("get_extra_info_callable", "extra_callable", "settimeout_callable", "shutdown_callable"):
            # These flags describe a callable METHOD on the live object.  Only
            # install the SeamFn when the flag is true; a false flag means the
            # attribute is absent/non-callable (getattr -> None in the live fn).
            if v:
                seam = {
                    "get_extra_info_callable": "stream.get_extra_info",
                    "extra_callable": "stream._stream.extra",
                    "settimeout_callable": "sock.settimeout",
                    "shutdown_callable": "sock.shutdown",
                }[k]
                setattr(out, k.split("_callable")[0], SeamFn(seam))
        elif k == "extensions":
            # httpx Response.extensions is a plain dict (the live fn calls
            # exts.get(...)); keep it as a dict, not a SimpleNamespace.
            setattr(out, k, v)
        elif isinstance(v, dict):
            setattr(out, k, mk_socket_obj(v))
        elif isinstance(v, list):
            setattr(out, k, [mk_socket_obj(x) for x in v])
        else:
            setattr(out, k, v)
    return out


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    env = inp.get("env") or {}
    global _CURRENT_ENV
    _CURRENT_ENV = env
    trace = []
    scripts = Scripts(inp.get("io") or {}, trace)
    _SCRIPTS = scripts
    # run_agent double (_ra() lazily imports it); env-backed AIAgent constants.
    ra = types.SimpleNamespace()
    ra.logger = LoggerDouble("ra.")
    ra.AIAgent = types.SimpleNamespace(
        _VALID_API_ROLES=frozenset(env.get("valid_api_roles", DEFAULT_ROLES)),
        _get_tool_call_id_static=SeamFn("ra._get_tool_call_id_static"),
        _get_tool_call_name_static=SeamFn("ra._get_tool_call_name_static"),
        _TOOL_CALL_ARGUMENTS_CORRUPTION_MARKER=env.get("corruption_marker", DEFAULT_MARKER),
    )
    sys.modules["run_agent"] = ra
    sys.modules["agent.context_compressor"]._DB_PERSISTED_MARKER = env.get("db_persisted_marker", "_db_persisted")
    sys.modules["hermes_cli.auth_codex"].CODEX_QUOTA_PROBE_MIN_INTERVAL_SECONDS = env.get(
        "codex_quota_probe_min_interval", 30.0)
    _BASE_NS["logger"] = _LOGGER
    # per-case env-backed constants / globals (ASCII fixture domain)
    _BASE_NS["STATUS_EXHAUSTED"] = env.get("status_exhausted", "exhausted")
    _BASE_NS["CODEX_QUOTA_PROBE_MIN_INTERVAL_SECONDS"] = env.get("codex_quota_probe_min_interval", 30.0)
    for _k, _d in [
        ("_DEGENERATE_FINAL_MAX_CHARS", 24),
        ("_SENTENCE_TERMINALS", (".", "!", "?", "\u3002", "\uff01", "\uff1f")),
        ("_DEGENERATE_LEADING_PUNCT", "?!,;:)]}"),
    ]:
        if _k in env:
            _BASE_NS[_k] = env[_k]
        else:
            _BASE_NS[_k] = _BASE_NS.get(_k, _d)
    _BASE_NS["_PROMOTED_REASONING_PLAN_TAIL_RE"] = _RegexSeam()
    _BASE_NS["_empty_heal_log_state"] = copy.deepcopy(env.get("heal_state") or {})
    _BASE_NS["_empty_heal_user_notified"] = set(env.get("heal_notified") or {})
    _BASE_NS["_empty_heal_pending_notice"] = copy.deepcopy(env.get("heal_pending") or {})
    _BASE_NS["_empty_heal_log_lock"] = contextlib.nullcontext()
    _BASE_NS["time"] = _FakeTime(env)
    _BASE_NS["datetime"] = _FakeDatetimeMod()
    _BASE_NS["_empty_heal_user_notified"] = _RecordingSet(env.get("heal_notified") or [])
    _BASE_NS["_empty_heal_pending_notice"] = _RecordingDict(env.get("heal_pending") or {})
    _BASE_NS["_connection_candidates"] = SeamFn("connection_candidates")
    if not env.get("providers_import_ok", True):
        sys.modules.pop("providers", None)
    else:
        install_stubs2()
    return _BASE_NS, trace, scripts


# ── input doubles ───────────────────────────────────────────────────────────
def mk_pool(d):
    if not isinstance(d, dict):
        return d
    ns = types.SimpleNamespace(**d)
    ns.entry_id_for_api_key = _rec("pool.entry_id_for_api_key", lambda a, k: {"api_key": a[0]})
    ns.current = _rec("pool.current", lambda a, k: {})
    ns.reclaim = _rec("pool.reclaim", lambda a, k: {"revert_id": a[0], "model": k["model"]})
    return ns


def mk_agent(d, pool_methods=False, swap=False, transport_cache=False):
    d = d or {}
    ns = types.SimpleNamespace()
    for k, v in d.items():
        if k == "_credential_pool" and pool_methods and isinstance(v, dict):
            ns._credential_pool = mk_pool(v)
        elif k == "_swap_credential" and swap:
            ns._swap_credential = None if v is None else _rec("agent._swap_credential", lambda a, kk: {"entry": a[0]})
        elif k == "_transport_cache" and transport_cache and isinstance(v, dict):
            ns._transport_cache = dict(v)
        else:
            setattr(ns, k, v)
    return ns


def plain_agent(d):
    return types.SimpleNamespace(**(d or {}))


def mk_error(spec):
    spec = spec or {}
    e = make_exc({"__raise__": spec.get("cls", "Exception"), "msg": spec.get("msg", "")})
    for k, v in (spec.get("attrs") or {}).items():
        setattr(e, k, v)
    resp = spec.get("response")
    if isinstance(resp, dict):
        e.response = types.SimpleNamespace(**resp)
    elif resp is not None:
        e.response = resp
    return e


APPLY_FIELDS = ["model", "provider", "requested_provider", "base_url", "api_mode",
                "api_key", "_reasoning_echo_flag", "request_overrides", "_client_kwargs",
                "_transport_cache"]


# ── live driver ─────────────────────────────────────────────────────────────
def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    if func == "_prepend_corruption_marker":
        tm = copy.deepcopy(inp.get("tool_msg") or {})
        ns[func](tm, inp.get("marker", ""))
        holder["state"] = {"tool_msg": norm(tm)}
        return None
    if func == "_find_tool_result":
        return ns[func](inp.get("messages") or [], inp.get("start", 0), inp.get("tool_call") or {})
    if func == "_drop_invalid_roles":
        return ns[func](inp.get("messages") or [])
    if func == "_drop_empty_tool_calls_arrays":
        return ns[func](inp.get("messages") or [])
    if func == "_repair_invalid_tool_call_names":
        msgs = copy.deepcopy(inp.get("messages") or [])
        ns[func](msgs)
        holder["state"] = {"messages": norm(msgs)}
        return None
    if func == "_drop_results_without_ids":
        return ns[func](inp.get("messages") or [])
    if func == "_pair_tool_calls_positionally":
        return ns[func](inp.get("messages") or [])
    if func == "_dedupe_tool_call_ids":
        return ns[func](inp.get("messages") or [])
    if func == "_realign_tool_result_names":
        return ns[func](inp.get("messages") or [])
    if func == "tool_results_this_turn":
        return ns[func](inp.get("messages"))
    if func == "_api_error_debug_info":
        return ns[func](mk_error(inp.get("error")))
    if func == "sync_credential_pool_entry_id":
        agent = mk_agent(inp.get("agent"), pool_methods=True)
        ns[func](agent)
        holder["state"] = {"_credential_pool_entry_id": norm(getattr(agent, "_credential_pool_entry_id", None))}
        return None
    if func == "_failed_credential_identity":
        return ns[func](plain_agent(inp.get("agent")), mk_pool(inp.get("pool")))
    if func == "_restore_runtime_capabilities":
        agent = plain_agent(inp.get("agent"))
        ns[func](agent, inp.get("rt") or {})
        holder["state"] = {"runtime_capabilities": norm(getattr(agent, "runtime_capabilities", None))}
        return None
    if func == "_apply_primary_runtime_fields":
        agent = mk_agent(inp.get("agent"), transport_cache=True)
        ns[func](agent, inp.get("rt") or {})
        holder["state"] = {f: norm(getattr(agent, f, None)) for f in APPLY_FIELDS}
        return None
    if func == "_revert_credential_rotation":
        agent = mk_agent(inp.get("agent"), pool_methods=True, swap=True)
        ns[func](agent)
        holder["state"] = {"_credential_pool_revert_id": norm(getattr(agent, "_credential_pool_revert_id", None))}
        return None
    if func == "_requeue_pending_steer":
        ns[func](plain_agent(inp.get("agent")), inp.get("steer_text", ""))
        return None
    if func == "_pre_tool_block_message":
        return ns[func](plain_agent(inp.get("agent")), inp.get("function_name"),
                        inp.get("function_args"), inp.get("effective_task_id"),
                        inp.get("tool_call_id"), inp.get("middleware_trace"))
    if func == "_snapshot_switch_state":
        return ns[func](plain_agent(inp.get("agent")))
    if func == "_restore_switch_snapshot":
        agent = plain_agent(inp.get("agent"))
        snap = {k: (_MISSING_OBJ if isinstance(v, dict) and v.get("__missing__") else v)
                for k, v in (inp.get("snapshot") or {}).items()}
        ns[func](agent, snap)
        holder["state"] = {k: norm(getattr(agent, k, None)) for k, v in snap.items() if v is not _MISSING_OBJ}
        return None
    if func == "_recover_auth_failure":
        return ns[func](inp.get("agent"), inp.get("pool"), **(inp.get("kwargs") or {}))
    # ── UNCLAIMED remainder (27 fns) ──────────────────────────────────────
    if func == "_cursor_skip_prefix":
        ids = {}
        def _obj(x):
            if not isinstance(x, dict) or "__id" not in x:
                return x
            i = x["__id"]
            if i not in ids:
                ids[i] = types.SimpleNamespace(**x)
            return ids[i]
        msgs = [_obj(m) for m in (inp.get("messages") or [])]
        cur = inp.get("cursor")
        if isinstance(cur, dict) and isinstance(cur.get("prefix"), list):
            cur = dict(cur)
            cur["prefix"] = [_obj(x) for x in cur["prefix"]]
        return ns[func](msgs, cur)
    if func == "_recover_rate_limit":
        pool = types.SimpleNamespace(entries=SeamFn("pool.entries"), current=SeamFn("pool.current"))
        return ns[func](pool, has_retried_429=inp.get("has_retried_429", False),
                        error_context=inp.get("error_context"), api_key_hint=inp.get("api_key_hint"),
                        credential_id=inp.get("credential_id"), rotate_and_swap=SeamFn("rotate_and_swap"))
    if func == "_build_anthropic_client_from_runtime":
        agent = mk_agent_full(inp.get("agent"))
        ns[func](agent, inp.get("rt") or {})
        holder["state"] = {f: norm(getattr(agent, f, None)) for f in
                           ("_anthropic_api_key", "_anthropic_base_url", "_anthropic_client",
                            "_is_anthropic_oauth", "client")}
        return None
    if func == "_rebuild_primary_client":
        agent = mk_agent_full(inp.get("agent"))
        ns[func](agent, inp.get("rt") or {}, reason=inp.get("reason", ""))
        holder["state"] = {f: norm(getattr(agent, f, None)) for f in
                           ("client", "_anthropic_client", "_anthropic_api_key",
                            "_anthropic_base_url", "_is_anthropic_oauth")}
        return None
    if func == "_primary_reset_gate_blocks":
        agent = mk_agent_full(inp.get("agent"))
        r = ns[func](agent, inp.get("rt") or {}, inp.get("primary_provider", ""),
                     inp.get("primary_runtime_base_url", ""), SeamFn("matches_primary"),
                     SeamFn("load_primary_pool"))
        holder["state"] = {"_restore_wait_logged": norm(getattr(agent, "_restore_wait_logged", None))}
        return r
    if func == "_rebind_primary_credential_pool":
        agent = mk_agent_full(inp.get("agent"))
        ns[func](agent, inp.get("primary_provider", ""), inp.get("primary_model", ""),
                 SeamFn("matches_primary"), SeamFn("load_primary_pool"),
                 (mk_pool_full(inp.get("prefetched_pool")) if isinstance(inp.get("prefetched_pool"), dict)
                  else inp.get("prefetched_pool")), inp.get("prefetched", False))
        holder["state"] = {"_credential_pool": norm(getattr(agent, "_credential_pool", None)),
                           "_credential_pool_entry_id": norm(getattr(agent, "_credential_pool_entry_id", None))}
        return None
    if func == "_primary_quota_reopened_early":
        agent = mk_agent_full(inp.get("agent"))
        r = ns[func](agent, inp.get("primary_provider", ""), inp.get("primary_model", ""),
                     SeamFn("matches_primary"), SeamFn("load_primary_pool"))
        holder["state"] = {"_codex_reopen_checked_at": norm(getattr(agent, "_codex_reopen_checked_at", None))}
        return r
    if func == "_moa_aggregator_cache_policy":
        return ns[func](plain_agent(inp.get("agent")), inp.get("eff_model", ""))
    if func == "_route_may_be_custom":
        return ns[func](plain_agent(inp.get("agent")), inp.get("eff_provider", ""),
                        inp.get("provider_lower", ""), inp.get("eff_base_url", ""))
    if func == "_provider_supplied_client":
        return ns[func](plain_agent(inp.get("agent")), copy.deepcopy(inp.get("client_kwargs") or {}))
    if func == "_ensure_route_client_headers":
        ck = copy.deepcopy(inp.get("client_kwargs") or {})
        ns[func](ck, inp.get("provider", ""))
        holder["state"] = {"client_kwargs": norm(ck)}
        return None
    if func == "_gemini_native_client":
        return ns[func](mk_agent_full(inp.get("agent")), copy.deepcopy(inp.get("client_kwargs") or {}),
                        inp.get("httpx_verify"), reason=inp.get("reason", ""), shared=inp.get("shared", False))
    if func == "_apply_switched_provider_request_overrides":
        agent = plain_agent(inp.get("agent"))
        ns[func](agent, inp.get("new_provider", ""))
        holder["state"] = {"request_overrides": norm(getattr(agent, "request_overrides", None))}
        return None
    if func == "_resolve_switch_destination":
        return ns[func](plain_agent(inp.get("agent")), inp.get("new_model", ""), inp.get("new_provider", ""),
                        inp.get("base_url", ""), inp.get("api_mode", ""), inp.get("capabilities"),
                        inp.get("old_norm", ""), inp.get("new_norm", ""))
    if func == "_build_switched_client":
        agent = mk_agent_full(inp.get("agent"))
        ns[func](agent, inp.get("new_provider", ""), inp.get("api_key"), inp.get("base_url", ""),
                 inp.get("api_mode", ""), inp.get("new_norm", ""))
        holder["state"] = {f: norm(getattr(agent, f, None)) for f in
                           ("api_key", "_anthropic_api_key", "_anthropic_base_url", "_anthropic_client",
                            "_is_anthropic_oauth", "client", "_client_kwargs")}
        return None
    if func == "_swap_switch_runtime":
        agent = mk_agent_full(inp.get("agent"))
        _orig_sync = ns.get("sync_credential_pool_entry_id")
        ns["sync_credential_pool_entry_id"] = SeamFn("sync_credential_pool_entry_id")
        try:
            ns[func](agent, inp.get("new_model", ""), inp.get("new_provider", ""), inp.get("api_key"),
                     inp.get("base_url", ""), inp.get("api_mode", ""), inp.get("old_provider", ""),
                     inp.get("old_norm", ""), inp.get("new_norm", ""))
        finally:
            if _orig_sync is not None:
                ns["sync_credential_pool_entry_id"] = _orig_sync
            else:
                ns.pop("sync_credential_pool_entry_id", None)
        holder["state"] = {f: norm(getattr(agent, f, None)) for f in
                           ("_config_context_length", "model", "provider", "requested_provider",
                            "_reasoning_echo_flag", "base_url", "api_mode", "_transport_cache",
                            "api_key", "_credential_pool", "_credential_pool_entry_id",
                            "_anthropic_client", "client", "_client_kwargs")}
        return None
    if func == "_resolve_switch_context_length":
        agent = mk_agent_full(inp.get("agent"))
        r = ns[func](agent, inp.get("snapshot") or {})
        holder["state"] = {"_config_context_length": norm(getattr(agent, "_config_context_length", None))}
        return r
    if func == "_update_switch_compressor":
        agent = mk_agent_full(inp.get("agent"))
        ns[func](agent, inp.get("custom_providers"), inp.get("effective_context_length"),
                 inp.get("snapshot") or {})
        return None
    if func == "_finish_switch":
        agent = mk_agent_full(inp.get("agent"))
        ns[func](agent, inp.get("new_provider", ""), inp.get("old_norm", ""), inp.get("new_norm", ""))
        holder["state"] = {f: norm(getattr(agent, f, None)) for f in
                           ("_fallback_activated", "_provider_fallback_active", "_provider_fallback_route",
                            "_fallback_index", "_credential_pool_revert_id", "_fallback_chain",
                            "_fallback_model", "request_overrides")}
        return None
    if func == "_persist_switch_billing_route":
        return ns[func](mk_agent_full(inp.get("agent")))
    if func == "_log_empty_non_final_heal":
        return ns[func](inp.get("healed", 0))
    if func == "looks_like_degenerate_final":
        return ns[func](inp.get("text", ""), inp.get("user_message"))
    if func == "promoted_reasoning_announces_action":
        return ns[func](inp.get("text", ""))
    if func == "_socket_from_stream":
        return ns[func](mk_socket_obj(inp.get("stream")))
    if func == "_socket_from_candidate":
        return ns[func](mk_socket_obj(inp.get("candidate")))
    if func == "_socket_from_response":
        return ns[func](mk_socket_obj(inp.get("response")))
    if func == "_shutdown_socket":
        return ns[func](mk_socket_obj(inp.get("sock")))
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
    seam_names = set(SEAM_ARGS) | {"logger.debug", "logger.warning", "logger.info", "logger.error",
                                   "ra.logger.debug", "ra.logger.warning", "ra.logger.info", "ra.logger.error"}
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
        fx = Path(__file__).resolve().parent / "fixtures" / "agent_runtime_helpers_flow2.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()