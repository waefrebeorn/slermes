#!/usr/bin/env python3
"""Oracle for the aux-call FLOW-core port (src/agent/auxiliary_client_flow.c).

Drives the LIVE agent/auxiliary_client.py orchestration defs with SCRIPTED I/O:
every module-level I/O call, config/registry read, clock read and aux_errors
verdict predicate is a recording stub fed the same scripts the C harness feeds
its seams. Compares the DECISION SEQUENCE (ordered seam calls {"name","args"}
in "trace") plus the final return and the relay-context state — not just a
final scalar.

The live defs are AST-extracted (module constants + helpers + dataclasses
seeded) so the heavy quarry import chain is never imported; in-function
imports are satisfied with sys.modules stubs delegating to the current case's
recorder. Fail-closed: missing defs or 0 cases => exit 2.
Summary line: 'ORACLE auxiliary_client_flow: N cases, M mismatches'.
"""
import ast
import asyncio
import json
import os
import re
import sys
import types
import functools
import enum
import contextvars
from dataclasses import dataclass, field
from typing import Any, Optional, NamedTuple, Callable

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "agent", "auxiliary_client.py")
POLICY_SRC = os.path.join(Q, "agent", "auxiliary_error_policy.py")
CLASSIFIER_SRC = os.path.join(Q, "agent", "error_classifier.py")
ENV_SRC = os.path.join(Q, "utils", "env.py")

# ── AST extraction ────────────────────────────────────────────────────────
TARGETS = [
    "_field", "_message_field", "_contains_any", "_creds_pair", "_creds_have_api_key",
    "_coerce_positive_timeout", "_without_temperature", "_without_max_tokens",
    "_without_reasoning_fields", "_affordable_max_tokens_from_error", "_elapsed_ms",
    "_stamp_latency_once", "_record_route_info", "_fallback_lane", "_coerce_llm_message",
    "_dedupe_tool_names", "_tag_backend", "_provider_outcome", "_ladder_step_call",
    "_failure_sender", "_is_structured_output_rejection", "_is_reasoning_field_rejection",
    "_is_reasoning_required_rejection", "_is_max_tokens_rejection", "_param_rung_accepts",
    "_task_prefers_fast_model", "_fast_lane_config_fields",
    "_compression_config_claims_fast_lane", "_compression_fast_lane_controls",
    "_get_task_no_progress_timeout", "_unwrap_moa_provider",
    "_preserve_provider_with_base_url", "_builtin_provider_present",
    "_profile_credentials_bypass_custom", "_named_custom_provider_present",
    "_routes_to_custom_endpoint", "_classification_provider", "_provider_for_host",
    "_relay_path", "_context_too_small", "_nous_on_messages_wire",
    "_is_gemini_native_route", "_forwards_max_tokens", "_stream_request_plan",
    "_with_custom_endpoint_extra_body", "_merge_aux_extra_body", "_unwrap_data_envelope",
    "_note_relay_auxiliary_error", "_should_skip_same_provider_retry",
    "_should_retry_same_provider", "_outcome", "_aoutcome", "_parameter_rungs",
    "_classify_failure", "_failure", "_rung", "_ladder_parameter_rungs",
    "_refreshed_nous_step", "_ladder_nous_rungs", "_failure_pool",
    "_recover_failure_pool", "_ladder_credential_rungs", "_next_fallback_after_quarantine",
    "_ladder_provider_fallback", "_aux_recovery_ladder", "_drive_ladder",
    "_primary_failure", "_start_recovery_ladder", "_prepare_aux_request", "_plan_aux_call",
    "_fallback_request_kwargs", "_plan_fallback_candidate",
    "_quarantine_fallback_candidate", "_plan_fallback_auth_retry",
    "_fallback_candidate_ladder", "_candidate_classification_provider",
]

CLASSES_NEEDED = [
    "_ClassifiedAttempt", "_LadderStep", "_LadderRoute", "_PreparedAuxRequest",
    "_FallbackDestination", "_ParameterRepair", "_RelayAttempt", "_ProfileProjection",
    "CompressionFastLane",
]

def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])

def exec_node(src, node, ns, path):
    exec(compile(seg(src, node), path, "exec"), ns)

def extract():
    src = open(SRC, encoding="utf-8").read()
    psrc = open(POLICY_SRC, encoding="utf-8").read()
    csrc = open(CLASSIFIER_SRC, encoding="utf-8").read()
    esrc = open(ENV_SRC, encoding="utf-8").read()
    ns = {
        "re": re, "json": json, "os": os, "sys": sys, "types": types,
        "functools": functools, "enum": enum, "contextvars": contextvars,
        "dataclass": dataclass, "field": field,
        "Any": Any, "Optional": Optional, "NamedTuple": NamedTuple, "Callable": Callable,
        "SimpleNamespace": types.SimpleNamespace,
    }
    tree = ast.parse(src)
    ctree = ast.parse(csrc)
    ptree = ast.parse(psrc)
    etree = ast.parse(esrc)
    # FailoverReason (real enum: live code compares members with `is`)
    for node in ctree.body:
        if isinstance(node, ast.ClassDef) and node.name == "FailoverReason":
            exec_node(csrc, node, ns, CLASSIFIER_SRC)
    # ClassifiedError (real dataclass)
    for node in ctree.body:
        if isinstance(node, ast.ClassDef) and node.name == "ClassifiedError":
            exec_node(csrc, node, ns, CLASSIFIER_SRC)
    # aux_errors stub module carrying the REAL Failure namedtuple + constants;
    # its predicate functions become per-case recorders.
    aux_errors = types.ModuleType("agent.auxiliary_error_policy")
    pns = dict(ns)
    for node in ptree.body:
        if isinstance(node, ast.ClassDef) and node.name == "Failure":
            exec_node(psrc, node, pns, POLICY_SRC)
        if isinstance(node, ast.Assign):
            try:
                exec_node(psrc, node, pns, POLICY_SRC)
            except Exception:
                pass
    aux_errors.Failure = pns["Failure"]
    aux_errors.ClassifiedError = ns["ClassifiedError"]
    for cname in ("AUTH_ERROR", "PAYMENT_ERROR", "MODEL_INCOMPATIBLE", "MODEL_NOT_FOUND"):
        setattr(aux_errors, cname, pns.get(cname))
    ns["aux_errors"] = aux_errors
    ns["Failure"] = pns["Failure"]
    # auxiliary_client classes (annotations need aux_errors + typing seeded)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in CLASSES_NEEDED:
            exec_node(src, node, ns, SRC)
    # module constants (Assign/AnnAssign with plain Name targets only)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    # utils/env.py: is_truthy_value + TRUTHY_STRINGS (live, pure)
    for node in etree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec_node(esrc, node, ns, ENV_SRC)
            except Exception:
                pass
        if isinstance(node, ast.FunctionDef) and node.name == "is_truthy_value":
            exec_node(esrc, node, ns, ENV_SRC)
    # all module-level functions (targets + helpers); recorders overwrite the
    # I/O names afterwards, targets stay live.
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    # imported constants the targets read (not module Assigns)
    ns.setdefault("PROFILE_REASONING_KEYS", {
        "reasoning", "reasoning_effort", "thinking", "thinking_config", "thinkingconfig",
        "thinking_budget", "thinkingbudget", "enable_thinking", "think", "verbosity",
    })
    missing = [t for t in TARGETS if t not in ns]
    missing += [c for c in CLASSES_NEEDED if c not in ns]
    for cname in ("FailoverReason", "ClassifiedError", "is_truthy_value", "TRUTHY_STRINGS"):
        if cname not in ns:
            missing.append(cname)
    if missing:
        print(f"FATAL oracle_auxiliary_client_flow: missing defs: {missing}")
        sys.exit(2)
    return ns, aux_errors

NS, AUX_ERRORS = extract()

# ── per-case registries + materializers ───────────────────────────────────
class ClientNS:
    pass

class CodexAuxiliaryClient(ClientNS):
    pass

class AsyncCodexAuxiliaryClient(ClientNS):
    pass

NS["CodexAuxiliaryClient"] = CodexAuxiliaryClient
NS["AsyncCodexAuxiliaryClient"] = AsyncCodexAuxiliaryClient

class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log

NS["logger"] = _SilentLogger()

ROUTE_FIELDS = ["client", "task", "tag", "async_mode", "base_info",
                "resolved_provider", "resolved_model", "resolved_base_url",
                "resolved_api_key", "resolved_api_mode", "final_model",
                "main_runtime", "route_info", "timeout"]
PREPARED_FIELDS = ["client", "final_model", "kwargs", "resolved_provider",
                   "request_provider", "resolved_model", "resolved_base_url",
                   "resolved_api_key", "resolved_api_mode", "effective_timeout",
                   "effective_extra_body", "base_info"]

MATCHER_NAMES = ["temperature", "structured_output", "reasoning_required",
                 "reasoning_field", "max_tokens"]
STRIP_NAMES = ["without_temperature", "without_structured_output_format",
               "with_reasoning_floor", "without_reasoning_fields", "without_max_tokens"]

def make_client(ctx, cid, spec):
    if cid in ctx.clients:
        return ctx.clients[cid]
    kind = spec.get("kind", "")
    cls = CodexAuxiliaryClient if kind == "codex" else (
        AsyncCodexAuxiliaryClient if kind == "async_codex" else ClientNS)
    c = cls()
    c._acf_id = cid
    for k, v in spec.items():
        if k in ("id", "kind"):
            continue
        setattr(c, k, v)
    if kind:
        c._acf_kind = kind
    ctx.clients[cid] = c
    return c

def make_exc(ctx, spec):
    if spec is None:
        return None
    eid = spec.get("exc_id", "")
    if eid and eid in ctx.excs:
        return ctx.excs[eid]
    tname = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    if tname == "_ClassifiedAttempt":
        e = NS["_ClassifiedAttempt"](make_failure(ctx, spec.get("classified")))
    elif tname == "BaseException":
        e = BaseException(msg)
    else:
        cls = {"RuntimeError": RuntimeError, "TimeoutError": TimeoutError,
               "AttributeError": AttributeError, "TypeError": TypeError,
               "ValueError": ValueError, "Exception": Exception}.get(tname)
        if cls is None:
            cls = type(tname, (Exception,), {})
        e = cls(msg)
    e._acf_id = eid
    if spec.get("status_code") is not None:
        e.status_code = spec["status_code"]
    if eid:
        ctx.excs[eid] = e
    return e

def make_verdict(spec):
    if spec is None:
        return None
    if isinstance(spec, NS["ClassifiedError"]):
        return spec
    reason = spec.get("reason")
    v = NS["ClassifiedError"](
        reason=getattr(NS["FailoverReason"], reason) if reason else NS["FailoverReason"].unknown,
        error_context=dict(spec.get("error_context") or {}),
        retryable=bool(spec.get("retryable", True)),
        should_rotate_credential=bool(spec.get("should_rotate_credential", False)),
        should_fallback=bool(spec.get("should_fallback", False)),
        model=spec.get("model"),
    )
    v._acf_spec = spec
    return v

def make_failure(ctx, spec):
    if spec is None:
        return None
    client = spec.get("client")
    return NS["Failure"](
        error=make_exc(ctx, spec.get("error")),
        verdict=make_verdict(spec.get("verdict")),
        rejects_parameter=bool(spec.get("rejects_parameter", False)),
        client=ctx.clients.get(client) if client else None,
    )

def make_failure_or_exc(ctx, spec):
    if spec is None:
        return None
    if isinstance(spec, dict) and "error" in spec:
        return make_failure(ctx, spec)
    return make_exc(ctx, spec)

def make_route(ctx, spec):
    spec = dict(spec or {})
    kw = {}
    for f in ROUTE_FIELDS:
        kw[f] = spec.get(f)
    kw["tag"] = spec.get("tag", "")
    kw["async_mode"] = bool(spec.get("async_mode", False))
    if kw["client"]:
        kw["client"] = ctx.clients[kw["client"]]
    if kw["route_info"] is not None:
        kw["route_info"] = dict(kw["route_info"])
    return NS["_LadderRoute"](**kw)

STRIP_BY_NAME = {}   # filled per case (recorder identities change)

def make_step(ctx, spec):
    kind = spec.get("kind", "")
    args = []
    for a in spec.get("args", []):
        args.append(materialize_arg(ctx, a))
    return NS["_LadderStep"](kind, tuple(args))

def materialize_arg(ctx, a):
    # client ids -> instances; repairs name lists -> strip functions;
    # destination dicts -> _FallbackDestination; dicts stay dicts.
    if isinstance(a, str) and a in ctx.clients:
        return ctx.clients[a]
    if isinstance(a, dict) and set(a.keys()) == {"provider", "base_url", "api_mode", "model"}:
        return NS["_FallbackDestination"](**a)
    if isinstance(a, list) and a and all(isinstance(x, str) and x in STRIP_BY_NAME for x in a):
        return tuple(STRIP_BY_NAME[x] for x in a)
    return a

def make_relay_context(ctx, spec):
    if not isinstance(spec, dict):
        return None
    ctxd = dict(spec)
    att = spec.get("attempt")
    if isinstance(att, dict):
        ctxd["attempt"] = NS["_RelayAttempt"](
            error=make_exc(ctx, att.get("error")),
            provider=att.get("provider"),
            model=att.get("model"),
            failure=make_failure(ctx, att.get("failure")),
            attempt=att.get("attempt"),
        )
    return ctxd

def make_response(ctx, spec):
    """dict responses get a dict subclass carrying model_validate (the seam);
    __obj__ responses get a SimpleNamespace subclass carrying it."""
    if isinstance(spec, dict) and spec.get("__obj__"):
        cls = type("_RespObj", (types.SimpleNamespace,), {})
        cls.model_validate = staticmethod(ctx.recorder("model_validate"))
        return cls(**{k: v for k, v in spec.items() if k != "__obj__"})
    if isinstance(spec, dict):
        cls = type("_RespDict", (dict,), {})
        cls.model_validate = staticmethod(ctx.recorder("model_validate"))
        return cls(spec)
    return spec

# ── projections (live values -> the C json shapes) ────────────────────────
def proj_exc(e):
    d = {"__raise__": type(e).__name__, "message": str(e)}
    eid = getattr(e, "_acf_id", None)
    if eid is not None:
        d["exc_id"] = eid
    if getattr(e, "status_code", None) is not None:
        d["status_code"] = e.status_code
    if isinstance(e, NS["_ClassifiedAttempt"]):
        d["classified"] = proj_failure(e.failure)
    if isinstance(e, BaseException) and not isinstance(e, Exception):
        d["base_only"] = True
    return d

def proj_failure(f):
    return {"__failure__": True, "error": proj_exc(f.error),
            "verdict": proj(f.verdict),
            "rejects_parameter": bool(f.rejects_parameter),
            "client": f.client._acf_id if f.client is not None else None}

def proj_client_full(c):
    d = {"id": c._acf_id}
    for k, v in vars(c).items():
        if k.startswith("_acf_"):
            continue
        d[k] = v
    return d

def proj_arg(a):
    # step args: strip-function tuples project to their names
    if isinstance(a, tuple) and a and all(callable(x) for x in a):
        return [STRIP_NAME_OF.get(id(x), "?") for x in a]
    return proj(a)

def proj(v):
    if isinstance(v, ClientNS):
        return v._acf_id
    if isinstance(v, BaseException):
        return proj_exc(v)
    if isinstance(v, NS["Failure"]):
        return proj_failure(v)
    if isinstance(v, NS["ClassifiedError"]):
        return getattr(v, "_acf_spec", {
            "reason": v.reason.name if v.reason else None,
            "error_context": v.error_context,
            "retryable": v.retryable,
            "should_rotate_credential": v.should_rotate_credential,
            "should_fallback": v.should_fallback, "model": v.model})
    if isinstance(v, NS["_LadderRoute"]):
        return {f: proj(getattr(v, f)) for f in ROUTE_FIELDS}
    if isinstance(v, NS["_LadderStep"]):
        return {"kind": v.kind, "args": [proj_arg(a) for a in v.args]}
    if isinstance(v, NS["_PreparedAuxRequest"]):
        return {f: proj(getattr(v, f)) for f in PREPARED_FIELDS}
    if isinstance(v, NS["_FallbackDestination"]):
        return {"provider": v.provider, "base_url": v.base_url,
                "api_mode": v.api_mode, "model": v.model}
    if isinstance(v, NS["_RelayAttempt"]):
        return {"error": proj_exc(v.error), "provider": v.provider,
                "model": v.model,
                "failure": proj_failure(v.failure) if v.failure is not None else None,
                "attempt": v.attempt}
    if isinstance(v, NS["_ParameterRepair"]):
        return {"response": proj(v.response),
                "failure": proj_failure(v.failure) if v.failure is not None else None,
                "kwargs": v.kwargs, "repairs": [STRIP_NAME_OF.get(id(s), "?") for s in v.repairs]}
    if isinstance(v, (set, frozenset)):
        return sorted((proj(x) for x in v),
                      key=lambda x: json.dumps(x, separators=(",", ":"), ensure_ascii=True))
    if isinstance(v, tuple):
        return [proj_arg(x) for x in v]
    if isinstance(v, list):
        return [proj(x) for x in v]
    if isinstance(v, dict):
        return {k: proj(x) for k, x in v.items()}
    if isinstance(v, types.SimpleNamespace):
        return {"__obj__": True, **{k: proj(x) for k, x in vars(v).items()}}
    return v

def norm_ret(r):
    if isinstance(r, BaseException):
        return proj_exc(r)
    return proj(r)
# ── scripted seam recorder ────────────────────────────────────────────────
SEAM_POS = {
    "time.sleep": ["seconds"],
    "_is_unsupported_parameter_error": ["exc", "param"],
    "is_reasoning_field_rejection": ["error_msg"],
    "is_reasoning_required_rejection": ["error_msg"],
    "ladder_accepts": ["failure"],
    "fallback_label": ["failure"],
    "auth_refresh": ["verdict"],
    "benches_provider": ["verdict"],
    "same_key_retry": ["verdict"],
    "same_provider_retry": ["verdict"],
    "resend_cannot_help": ["verdict"],
    "classify_attempt": ["exc", "provider", "client", "model", "base_url"],
    "_get_auxiliary_task_config": ["task"],
    "_get_task_extra_body": ["task"],
    "_effective_aux_timeout": ["task", "timeout"],
    "parse_reasoning_effort": ["effort"],
    "resolve_compression_fast_lane": ["actual_provider", "actual_model"],
    "_resolve_moa_aggregator": ["preset_name"],
    "get_provider": ["name"],
    "get_provider_profile": ["name"],
    "_get_named_custom_provider": ["name"],
    "profile_for_host": ["base_url"],
    "nous_api_mode": ["model"],
    "is_native_gemini_base_url": ["base_url"],
    "_normalize_aux_provider": ["provider"],
    "_fallback_provider_from_label": ["label"],
    "_effective_provider_for_client": ["client", "fallback"],
    "_normalize_main_runtime": ["main_runtime"],
    "_recoverable_pool_provider": ["resolved_provider", "client"],
    "base_url_host_matches": ["base_url", "domain"],
    "_to_openai_base_url": ["base_url"],
    "base_url_path": ["base_url"],
    "_fallback_entry_api_key": ["entry"],
    "_candidate_context_window": ["provider", "model"],
    "_is_anthropic_compat_endpoint": ["provider", "base_url"],
    "_is_managed_local_endpoint": ["base_url"],
    "profile_forwards_max_tokens": ["provider_norm"],
    "with_prompt_progress": ["kwargs"],
    "_aux_stream_total_ceiling": ["effective_timeout"],
    "load_config_readonly": [],
    "get_compatible_custom_providers": ["config"],
    "generic_nested_reasoning": ["reasoning_config"],
    "clamp_reasoning_config": ["reasoning_config"],
    "_nous_portal_tags": [],
    "get_conversation_context": [],
    "model_validate": ["data"],
    "_set_relay_auxiliary_route": ["provider", "model", "api_mode"],
    "_build_call_kwargs": ["provider", "model", "messages"],
    "_convert_openai_images_to_anthropic": ["messages"],
    "_replan_synchronous_cache_sections": ["messages", "tools"],
    "_fallback_destination": ["task", "fb_client", "fb_model", "fb_label"],
    "_fallback_entry_timeout": ["task", "fb_label"],
    "_fallback_chain_entry": ["task", "fb_label"],
    "_resolve_task_provider_model": ["task", "provider", "model", "base_url", "api_key"],
    "_refresh_provider_credentials": ["provider"],
    "_auth_refresh_provider_for_route": ["resolved_provider", "client_base_url", "effective_provider"],
    "_evict_cached_clients": ["provider"],
    "_evict_cached_client_instance": ["target"],
    "_recover_provider_pool": ["provider", "exc"],
    "_mark_provider_unhealthy": ["provider"],
    "fallback_candidate_quarantine_ttl": ["reason"],
    "_custom_health_base_url": ["provider", "explicit_base_url"],
    "_try_configured_fallback_chain": ["task", "failed_provider"],
    "_try_main_fallback_chain": ["task", "failed_provider"],
    "_try_payment_fallback": ["failed_provider", "task"],
    "_try_main_agent_model_fallback": ["failed_provider", "task"],
    "_get_cached_client": ["provider", "model"],
    "_refresh_nous_recommended_model": [],
    "_nous_portal_account_has_fresh_paid_access": [],
    "remember_temperature_rejection": ["provider", "base_url", "rejected_kwargs", "error"],
    "remember_structured_output_rejection": ["provider", "base_url", "rejected_kwargs", "error"],
    "remember_reasoning_floor": ["provider", "base_url", "rejected_kwargs", "error"],
    "_without_structured_output_format": ["kwargs"],
    "with_reasoning_floor": ["kwargs"],
    "reapply_repairs": ["kwargs", "repairs"],
    "rebuild": ["provider", "client", "model"],
}

RECORDER_NAMES = sorted(set(SEAM_POS) | {
    "_resolve_call_client", "_refresh_nous_auxiliary_client",
    "_try_configured_fallback_chain", "_try_main_fallback_chain",
    "_try_payment_fallback", "_try_main_agent_model_fallback",
})

class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.io = inp.get("io", {}) or {}
        self.cursors = {}
        self.clients = {}
        self.excs = {}
        self.relay_context = None
        self.ladder_accepts_rec = None

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
            if name == "perform":
                canon = proj(args[0]) if args else {}
            else:
                pos = SEAM_POS.get(name, [])
                canon = {}
                for i, a in enumerate(args):
                    canon[pos[i] if i < len(pos) else f"arg{i}"] = proj(a)
                for k, v in kwargs.items():
                    canon[k] = proj(v)
            ctx.trace.append({"name": name, "args": canon})
            entry = ctx.script_next(name)
            if isinstance(entry, dict) and entry.get("__script__"):
                if isinstance(entry.get("raise"), dict):
                    raise make_exc(ctx, entry["raise"])
                ret = entry.get("ret")
            else:
                ret = entry
            return ctx.convert(name, ret, args, kwargs)
        return call

    def client_of(self, cid):
        return self.clients.get(cid) if cid else None

    def convert(self, name, ret, args, kwargs):
        if name == "classify_attempt":
            return make_verdict(ret)
        if name == "_resolve_task_provider_model":
            d = ret or {}
            return (d.get("resolved_provider"), d.get("resolved_model"),
                    d.get("resolved_base_url"), d.get("resolved_api_key"),
                    d.get("resolved_api_mode"))
        if name == "_resolve_call_client":
            d = ret or {}
            return (self.client_of(d.get("client")), d.get("final_model"),
                    d.get("resolved_provider"), d.get("effective_provider"))
        if name in ("_get_cached_client", "_refresh_nous_auxiliary_client"):
            d = ret or {}
            return (self.client_of(d.get("client")), d.get("model"))
        if name == "_fallback_destination":
            return NS["_FallbackDestination"](**(ret or {}))
        if name == "_replan_synchronous_cache_sections":
            d = ret or {}
            return (d.get("messages"), d.get("tools"))
        if name in ("_try_configured_fallback_chain", "_try_main_fallback_chain",
                    "_try_payment_fallback", "_try_main_agent_model_fallback"):
            d = ret or {}
            return (self.client_of(d.get("client")), d.get("model"), d.get("label", ""))
        if name == "_resolve_moa_aggregator":
            d = ret or {}
            return (d.get("provider"), d.get("model"))
        if name in ("get_provider_profile", "profile_for_host"):
            return types.SimpleNamespace(**ret) if isinstance(ret, dict) else None
        if name == "rebuild":
            d = ret or {}
            return (NS["_FallbackDestination"](**d["destination"]), d.get("kwargs"))
        if name == "perform":
            step = args[0] if args else None
            if step is not None and step.kind == "attempt":
                d = ret or {}
                repairs = tuple(STRIP_BY_NAME[n] for n in (d.get("repairs") or []))
                return (d.get("resp"), make_failure(self, d.get("failure")), repairs)
            return ret
        return ret

def proj_accept(ctx, accept):
    if isinstance(accept, functools.partial):
        if accept.func is ctx.ladder_accepts_rec:
            return "ladder_spent" if accept.keywords.get("rung_spent") else "ladder_unspent"
    if accept is NS.get("_param_rung_accepts"):
        return "param"
    return "always"

ACCEPT_BY_TAG = {}   # filled per case

# ── sys.modules stubs for in-function imports ─────────────────────────────
CURRENT = {"rec": None, "ctx": None}

def _stub_fn(name):
    def f(*a, **k):
        return CURRENT["rec"](name)(*a, **k)
    return f

class FailureScope(enum.Enum):
    MODEL = "model"
    CREDENTIAL = "credential"
    ENDPOINT = "endpoint"

def _rlrr_stub(first_err, route, kwargs, accept):
    """rate_limit_retry_rungs: a live yield-from generator. Its scripted
    wait/call sub-steps flow through the driver's perform/time.sleep seams."""
    ctx = CURRENT["ctx"]
    canon = {"first_err": proj(first_err), "route": proj(route),
             "kwargs": proj(kwargs), "accept": proj_accept(ctx, accept)}
    ctx.trace.append({"name": "rate_limit_retry_rungs", "args": canon})
    entry = ctx.script_next("rate_limit_retry_rungs") or {}
    for ss in entry.get("steps") or []:
        yield make_step(ctx, ss)
    if isinstance(entry.get("raise"), dict):
        raise make_exc(ctx, entry["raise"])
    return (entry.get("resp"), make_failure(ctx, entry.get("failure")))

def install_module_stubs():
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m
    agent = sys.modules.get("agent")
    if agent is None:
        agent = types.ModuleType("agent")
        agent.__path__ = []
        sys.modules["agent"] = agent
    hermes_cli = sys.modules.get("hermes_cli")
    if hermes_cli is None:
        hermes_cli = types.ModuleType("hermes_cli")
        hermes_cli.__path__ = []
        sys.modules["hermes_cli"] = hermes_cli
    mod("providers", profile_for_host=_stub_fn("profile_for_host"),
        get_provider_profile=_stub_fn("get_provider_profile"))
    mod("hermes_cli.providers", get_provider=_stub_fn("get_provider"),
        nous_api_mode=_stub_fn("nous_api_mode"))
    mod("hermes_cli.runtime_provider",
        _get_named_custom_provider=_stub_fn("_get_named_custom_provider"))
    mod("hermes_cli.config", load_config_readonly=_stub_fn("load_config_readonly"),
        get_compatible_custom_providers=_stub_fn("get_compatible_custom_providers"))
    mod("hermes_constants", parse_reasoning_effort=_stub_fn("parse_reasoning_effort"))
    mod("agent.gemini_native_adapter",
        is_native_gemini_base_url=_stub_fn("is_native_gemini_base_url"))
    mod("agent.reasoning_effort", generic_nested_reasoning=_stub_fn("generic_nested_reasoning"),
        clamp_reasoning_config=_stub_fn("clamp_reasoning_config"))
    mod("agent.portal_tags", get_conversation_context=_stub_fn("get_conversation_context"))
    mod("agent.agent_init",
        _custom_provider_extra_body_for_agent=_stub_fn("_custom_provider_extra_body_for_agent"))
    mod("agent.backend_identity", FailureScope=FailureScope)
    mod("agent.auxiliary_rate_limit_backoff", rate_limit_retry_rungs=_rlrr_stub)
    mod("agent.auxiliary_error_policy", **{k: getattr(AUX_ERRORS, k) for k in dir(AUX_ERRORS) if not k.startswith("__")})

install_module_stubs()

# extend proj for FailureScope members
_proj_base = proj
def proj(v):  # noqa: F811
    if isinstance(v, FailureScope):
        return v.name.lower()
    return _proj_base(v)

# ── per-case seeding ──────────────────────────────────────────────────────
def seed_case(ctx):
    global STRIP_BY_NAME, STRIP_NAME_OF
    CURRENT["rec"] = ctx.recorder
    CURRENT["ctx"] = ctx
    for name in RECORDER_NAMES:
        NS[name] = ctx.recorder(name)
    for pname in ("classify_attempt", "ladder_accepts", "fallback_label", "auth_refresh",
                  "benches_provider", "same_key_retry", "same_provider_retry",
                  "resend_cannot_help"):
        rec = ctx.recorder(pname)
        setattr(AUX_ERRORS, pname, rec)
        if pname == "ladder_accepts":
            ctx.ladder_accepts_rec = rec
    NS["time"] = types.SimpleNamespace(
        monotonic=ctx.recorder("time.monotonic"), sleep=ctx.recorder("time.sleep"))
    NS["_RELAY_AUX_CALL_CONTEXT"] = types.SimpleNamespace(get=lambda: ctx.relay_context)
    STRIP_BY_NAME = {
        "without_temperature": NS["_without_temperature"],
        "without_structured_output_format": NS["_without_structured_output_format"],
        "with_reasoning_floor": NS["with_reasoning_floor"],
        "without_reasoning_fields": NS["_without_reasoning_fields"],
        "without_max_tokens": NS["_without_max_tokens"],
    }
    STRIP_NAME_OF = {id(f): n for n, f in STRIP_BY_NAME.items()}
    ACCEPT_BY_TAG.clear()
    ACCEPT_BY_TAG.update({
        "param": NS["_param_rung_accepts"],
        "always": lambda _f: True,
        "ladder_spent": functools.partial(AUX_ERRORS.ladder_accepts, rung_spent=True),
        "ladder_unspent": functools.partial(AUX_ERRORS.ladder_accepts, rung_spent=False),
    })

STRIP_NAME_OF = {}

def drive(gen, ctx):
    return NS["_drive_ladder"](gen, ctx.recorder("perform"))

def prep(ctx, inp):
    """Materialize input specs exactly as the C harness prep_input does."""
    p = dict(inp)
    for cid, spec in (inp.get("clients") or {}).items():
        make_client(ctx, cid, spec)
    ctx.relay_context = make_relay_context(ctx, inp.get("relay_context"))
    if isinstance(p.get("exc"), dict):
        p["exc"] = make_failure_or_exc(ctx, p["exc"])
    if isinstance(p.get("failure"), dict):
        p["failure"] = make_failure(ctx, p["failure"])
    if isinstance(p.get("first_err"), dict):
        p["first_err"] = make_failure_or_exc(ctx, p["first_err"])
    if isinstance(p.get("error"), dict):
        p["error"] = make_exc(ctx, p["error"])
    if isinstance(p.get("fb_err"), dict):
        p["fb_err"] = make_exc(ctx, p["fb_err"])
    if isinstance(p.get("judged"), list):
        p["judged"] = [make_exc(ctx, e) for e in p["judged"]]
    if isinstance(p.get("route"), dict):
        p["route"] = make_route(ctx, p["route"])
    if isinstance(p.get("context"), dict):
        p["context"] = make_relay_context(ctx, p["context"])
    if isinstance(p.get("client"), dict):
        p["client"] = make_client(ctx, p["client"].get("id", ""), p["client"])
    if isinstance(p.get("fb_client"), str) and p["fb_client"] in ctx.clients:
        p["fb_client"] = ctx.clients[p["fb_client"]]
    if isinstance(p.get("destination"), dict):
        p["destination"] = NS["_FallbackDestination"](**p["destination"])
    if isinstance(p.get("step"), dict):
        p["step"] = make_step(ctx, p["step"])
    if isinstance(p.get("req"), dict):
        rd = dict(p["req"])
        if rd.get("client") in ctx.clients:
            rd["client"] = ctx.clients[rd["client"]]
        p["req"] = types.SimpleNamespace(**rd)
    if isinstance(p.get("response"), dict):
        p["response"] = make_response(ctx, p["response"])
    if isinstance(p.get("obj"), dict):
        p["obj"] = make_response(ctx, p["obj"])
    if isinstance(p.get("msg"), dict):
        p["msg"] = make_response(ctx, p["msg"])
    if isinstance(p.get("projection"), dict):
        p["projection"] = types.SimpleNamespace(**p["projection"])
    return p
# ── per-func live replay ──────────────────────────────────────────────────
def run_case(func, inp):
    ctx = CaseCtx(inp)
    seed_case(ctx)
    p = prep(ctx, inp)
    remember_names = {id(NS[n]): n for n in
                      ("remember_temperature_rejection",
                       "remember_structured_output_rejection",
                       "remember_reasoning_floor")}
    out = {}

    def client_of(cid):
        return ctx.clients.get(cid) if cid else None

    def kwargs_copy():
        return dict(inp.get("kwargs") or {})

    def route_info_copy():
        ri = inp.get("route_info")
        return dict(ri) if isinstance(ri, dict) else None

    try:
        if func == "_field":
            r = NS["_field"](p.get("obj"), inp.get("key", ""), inp.get("default"))
            ret = proj(r)
        elif func == "_message_field":
            ret = proj(NS["_message_field"](p.get("msg"), inp.get("name", "")))
        elif func == "_contains_any":
            ret = NS["_contains_any"](inp.get("text", ""), tuple(inp.get("needles") or []))
        elif func == "_creds_pair":
            ret = proj(NS["_creds_pair"](inp.get("creds") or {}))
        elif func == "_creds_have_api_key":
            ret = NS["_creds_have_api_key"](inp.get("creds") or {})
        elif func == "_coerce_positive_timeout":
            ret = NS["_coerce_positive_timeout"](inp.get("raw"))
        elif func == "_without_temperature":
            ret = NS["_without_temperature"](inp.get("kwargs") or {})
        elif func == "_without_max_tokens":
            ret = NS["_without_max_tokens"](inp.get("kwargs") or {})
        elif func == "_without_reasoning_fields":
            ret = NS["_without_reasoning_fields"](inp.get("kwargs") or {})
        elif func == "_affordable_max_tokens_from_error":
            ret = NS["_affordable_max_tokens_from_error"](p["exc"])
        elif func == "_elapsed_ms":
            ret = NS["_elapsed_ms"](inp.get("started_at", 0.0), now=inp.get("now"))
        elif func == "_stamp_latency_once":
            li = inp.get("latency_info")
            li = dict(li) if isinstance(li, dict) else None
            NS["_stamp_latency_once"](li, inp.get("key", ""), inp.get("started_at", 0.0))
            ret = {"latency_info": li}
        elif func == "_record_route_info":
            ri = route_info_copy()
            NS["_record_route_info"](ri, inp.get("provider"), inp.get("model"))
            ret = {"route_info": ri}
        elif func == "_fallback_lane":
            ret = proj(NS["_fallback_lane"](p.get("client"), inp.get("model"), inp.get("label", "")))
        elif func == "_coerce_llm_message":
            ret = proj(NS["_coerce_llm_message"](p.get("response")))
        elif func == "_dedupe_tool_names":
            ret = NS["_dedupe_tool_names"](inp.get("tools") or [], inp.get("provider", ""), inp.get("model", ""))
        elif func == "_tag_backend":
            r = NS["_tag_backend"](p.get("client"), inp.get("provider"))
            ret = proj_client_full(r) if r is not None else None
        elif func == "_provider_outcome":
            judged = p.get("judged") or []
            try:
                r = NS["_provider_outcome"](p.get("response"), p.get("error"), judged)
                rr = proj(r)
            except BaseException as e:
                rr = proj_exc(e)
            ret = {"ret": rr, "judged": [proj_exc(e) for e in judged]}
        elif func == "_ladder_step_call":
            kind, args, kw = NS["_ladder_step_call"](
                p["step"], p["req"], inp.get("retry_kwargs") or {}, inp.get("candidate_kwargs") or {})
            ret = [kind, [proj_arg(a) for a in args], proj(kw)]
        elif func == "_failure_sender":
            sender = NS["_failure_sender"](p["route"], p["failure"])
            ret = sender._acf_id if sender is not None else None
        elif func == "_is_structured_output_rejection":
            ret = NS["_is_structured_output_rejection"](p["exc"])
        elif func == "_is_reasoning_field_rejection":
            ret = NS["_is_reasoning_field_rejection"](p["exc"])
        elif func == "_is_reasoning_required_rejection":
            ret = NS["_is_reasoning_required_rejection"](p["exc"])
        elif func == "_is_max_tokens_rejection":
            ret = NS["_is_max_tokens_rejection"](p["exc"], p.get("client"))
        elif func == "_param_rung_accepts":
            ret = NS["_param_rung_accepts"](p["failure"])
        elif func == "_task_prefers_fast_model":
            ret = NS["_task_prefers_fast_model"](inp.get("task"))
        elif func == "_fast_lane_config_fields":
            ret = proj(NS["_fast_lane_config_fields"](inp.get("config") or {}))
        elif func == "_compression_config_claims_fast_lane":
            ret = NS["_compression_config_claims_fast_lane"](inp.get("config") or {})
        elif func == "_compression_fast_lane_controls":
            r = NS["_compression_fast_lane_controls"](
                inp.get("task"), actual_provider=inp.get("actual_provider"),
                actual_model=inp.get("actual_model"),
                requested_provider=inp.get("requested_provider"),
                requested_model=inp.get("requested_model"),
                route_config=inp.get("route_config"),
                leak_guard_config=inp.get("leak_guard_config"),
                max_tokens=inp.get("max_tokens"), extra_body=inp.get("extra_body"))
            ret = [proj(r[0]), proj(r[1])]
        elif func == "_get_task_no_progress_timeout":
            ret = NS["_get_task_no_progress_timeout"](inp.get("task"))
        elif func == "_unwrap_moa_provider":
            ret = proj(NS["_unwrap_moa_provider"](inp.get("prov", ""), inp.get("mdl")))
        elif func == "_preserve_provider_with_base_url":
            ret = NS["_preserve_provider_with_base_url"](inp.get("prov"))
        elif func == "_builtin_provider_present":
            ret = NS["_builtin_provider_present"](inp.get("name", ""))
        elif func == "_profile_credentials_bypass_custom":
            ret = NS["_profile_credentials_bypass_custom"](inp.get("name", ""))
        elif func == "_named_custom_provider_present":
            ret = NS["_named_custom_provider_present"](inp.get("name", ""))
        elif func == "_routes_to_custom_endpoint":
            ret = NS["_routes_to_custom_endpoint"](inp.get("provider_norm", ""))
        elif func == "_classification_provider":
            ret = NS["_classification_provider"](p.get("client"), inp.get("fallback", ""),
                                                 inp.get("main_runtime"))
        elif func == "_provider_for_host":
            table = tuple(tuple(row) for row in (inp.get("table") or []))
            ret = NS["_provider_for_host"](inp.get("base_url", ""), table)
        elif func == "_relay_path":
            ret = NS["_relay_path"](inp.get("base_url", ""))
        elif func == "_context_too_small":
            ret = NS["_context_too_small"](
                inp.get("entry") or {}, inp.get("provider", ""), inp.get("model", ""),
                inp.get("min_ctx"), task=inp.get("task"), label=inp.get("label", ""),
                name_model=inp.get("name_model", False))
        elif func == "_nous_on_messages_wire":
            ret = NS["_nous_on_messages_wire"](inp.get("provider_norm", ""), inp.get("model", ""))
        elif func == "_is_gemini_native_route":
            ret = NS["_is_gemini_native_route"](inp.get("provider_norm", ""), inp.get("effective_base", ""))
        elif func == "_forwards_max_tokens":
            ret = NS["_forwards_max_tokens"](inp.get("provider", ""), inp.get("provider_norm", ""),
                                             inp.get("model", ""), inp.get("effective_base", ""),
                                             inp.get("task"))
        elif func == "_stream_request_plan":
            ret = proj(NS["_stream_request_plan"](inp.get("kwargs") or {}, p.get("client")))
        elif func == "_with_custom_endpoint_extra_body":
            ret = NS["_with_custom_endpoint_extra_body"](inp.get("extra_body"), inp.get("provider"),
                                                         inp.get("model"), inp.get("base_url"))
        elif func == "_merge_aux_extra_body":
            ret = NS["_merge_aux_extra_body"](inp.get("extra_body"), p.get("projection"),
                                              inp.get("reasoning_config"), inp.get("provider_norm", ""))
        elif func == "_unwrap_data_envelope":
            r = NS["_unwrap_data_envelope"](p.get("response"), inp.get("task"))
            ret = proj(r)
        elif func == "_note_relay_auxiliary_error":
            context = p.get("context")
            NS["_RELAY_AUX_CALL_CONTEXT"] = types.SimpleNamespace(get=lambda: context)
            NS["_note_relay_auxiliary_error"](p.get("exc"), p.get("failure"))
            ret = {"context": proj(context)}
        elif func == "_should_skip_same_provider_retry":
            ret = NS["_should_skip_same_provider_retry"](inp.get("task"), p["failure"])
        elif func == "_should_retry_same_provider":
            ret = NS["_should_retry_same_provider"](inp.get("task"), p["failure"], inp.get("tag", ""))
        elif func == "_outcome":
            thunk = ctx.recorder(inp.get("thunk", "thunk"))
            r, e = NS["_outcome"](thunk, **(inp.get("args") or {}))
            ret = {"result": proj(r), "error": proj_exc(e) if e is not None else None}
        elif func == "_aoutcome":
            thunk = ctx.recorder(inp.get("thunk", "thunk"))
            async def awaitable():
                return thunk(**(inp.get("args") or {}))
            r, e = asyncio.run(NS["_aoutcome"](awaitable()))
            ret = {"result": proj(r), "error": proj_exc(e) if e is not None else None}
        elif func == "_parameter_rungs":
            rungs = NS["_parameter_rungs"](client_of(inp.get("client")), inp.get("max_tokens"))
            rows = []
            for i, (m, s, msg, rem) in enumerate(rungs):
                if m is NS["_is_structured_output_rejection"]:
                    mn = "structured_output"
                elif m is NS["_is_reasoning_required_rejection"]:
                    mn = "reasoning_required"
                elif m is NS["_is_reasoning_field_rejection"]:
                    mn = "reasoning_field"
                else:
                    mn = MATCHER_NAMES[i]
                rows.append([mn, STRIP_NAME_OF.get(id(s), "?"), msg,
                             remember_names.get(id(rem)) if rem is not None else None])
            ret = rows
        elif func == "_classify_failure":
            f = NS["_classify_failure"](p["exc"], inp.get("provider"), client_of(inp.get("client")),
                                        inp.get("model"), inp.get("base_url"), inp.get("main_runtime"))
            ret = proj_failure(f)
        elif func == "_failure":
            f = NS["_failure"](p["route"], p["exc"], p.get("step"))
            ret = proj_failure(f)
        elif func == "_rung":
            gen = NS["_rung"](p["step"], ACCEPT_BY_TAG[inp.get("accept", "always")], p["route"])
            r = drive(gen, ctx)
            ret = {"result": proj(r[0]), "failure": proj_failure(r[1]) if r[1] is not None else None}
        elif func == "_ladder_parameter_rungs":
            kw = kwargs_copy()
            gen = NS["_ladder_parameter_rungs"](p["first_err"], p["route"], kw, inp.get("max_tokens"))
            repair = drive(gen, ctx)
            ret = proj(repair)
        elif func == "_refreshed_nous_step":
            kw = kwargs_copy()
            step = NS["_refreshed_nous_step"](p["route"], kw, inp.get("message", ""))
            ret = {"step": proj(step) if step is not None else None, "kwargs": kw}
        elif func == "_ladder_nous_rungs":
            kw = kwargs_copy()
            repairs = tuple(STRIP_BY_NAME[n] for n in (inp.get("repairs") or []))
            gen = NS["_ladder_nous_rungs"](p["first_err"], p["route"], kw,
                                           inp.get("client_is_nous", False), repairs)
            resp, failure = drive(gen, ctx)
            ret = {"response": proj(resp), "failure": proj_failure(failure) if failure is not None else None,
                   "kwargs": kw}
        elif func == "_failure_pool":
            ret = proj(NS["_failure_pool"](p["route"], p["failure"]))
        elif func == "_recover_failure_pool":
            ret = NS["_recover_failure_pool"](p["route"], p["failure"])
        elif func == "_ladder_credential_rungs":
            kw = kwargs_copy()
            repairs = tuple(STRIP_BY_NAME[n] for n in (inp.get("repairs") or []))
            gen = NS["_ladder_credential_rungs"](p["first_err"], p["route"], kw,
                                                 inp.get("client_is_nous", False), repairs)
            resp, failure = drive(gen, ctx)
            ret = {"response": proj(resp), "failure": proj_failure(failure) if failure is not None else None,
                   "kwargs": kw}
        elif func == "_next_fallback_after_quarantine":
            scope = FailureScope.ENDPOINT if inp.get("failure_scope") == "endpoint" else None
            lanes = set(tuple(l) for l in (inp.get("skip_lanes") or []))
            client, model, label = NS["_next_fallback_after_quarantine"](
                inp.get("task"), inp.get("resolved_provider"), inp.get("is_auto", False),
                p["route"], inp.get("failed_model"), scope,
                task_chain_only=inp.get("task_chain_only", False), skip_lanes=lanes)
            ret = {"client": client._acf_id if client is not None else None,
                   "model": model, "label": label}
        elif func == "_ladder_provider_fallback":
            gen = NS["_ladder_provider_fallback"](p["failure"], p["route"],
                                                  rung_spent=inp.get("rung_spent", False))
            resp = drive(gen, ctx)
            ret = {"ret": proj(resp), "route_info": p["route"].route_info}
        elif func == "_aux_recovery_ladder":
            kw = kwargs_copy()
            ri = route_info_copy()
            gen = NS["_aux_recovery_ladder"](
                p["first_err"], client=client_of(inp.get("client")), kwargs=kw,
                task=inp.get("task"), async_mode=inp.get("async_mode", False),
                base_info=inp.get("base_info"), resolved_provider=inp.get("resolved_provider"),
                resolved_model=inp.get("resolved_model"), resolved_base_url=inp.get("resolved_base_url"),
                resolved_api_key=inp.get("resolved_api_key"), resolved_api_mode=inp.get("resolved_api_mode"),
                final_model=inp.get("final_model"), max_tokens=inp.get("max_tokens"),
                main_runtime=inp.get("main_runtime"), route_info=ri)
            resp = drive(gen, ctx)
            ret = {"ret": proj(resp), "route_info": ri, "kwargs": kw}
        elif func == "_drive_ladder":
            gspec = inp.get("gen") or {}
            steps = [make_step(ctx, s) for s in (gspec.get("steps") or [])]
            final = gspec.get("final")
            on_throw = gspec.get("on_throw", "propagate")
            def scripted():
                for s in steps:
                    try:
                        yield s
                    except BaseException:
                        if on_throw == "propagate":
                            raise
                return final
            r = drive(scripted(), ctx)
            ret = proj(r)
        elif func == "_primary_failure":
            f = NS["_primary_failure"](p["req"], p["exc"])
            ret = proj_failure(f)
        elif func == "_start_recovery_ladder":
            ri = route_info_copy()
            gen = NS["_start_recovery_ladder"](p["first_err"], p["req"],
                                               inp.get("retry_kwargs") or {},
                                               task=inp.get("task"),
                                               async_mode=inp.get("async_mode", False),
                                               route_info=ri)
            loc = gen.gi_frame.f_locals
            keys = ["first_err", "client", "kwargs", "task", "async_mode", "base_info",
                    "resolved_provider", "resolved_model", "resolved_base_url",
                    "resolved_api_key", "resolved_api_mode", "final_model",
                    "max_tokens", "main_runtime", "route_info"]
            ret = {k: proj(loc[k]) for k in keys}
        elif func == "_prepare_aux_request":
            ri = route_info_copy()
            req = NS["_prepare_aux_request"](
                inp.get("task"), provider=inp.get("provider"), model=inp.get("model"),
                base_url=inp.get("base_url"), api_key=inp.get("api_key"),
                main_runtime=inp.get("main_runtime"), messages=inp.get("messages") or [],
                temperature=inp.get("temperature"), max_tokens=inp.get("max_tokens"),
                tools=inp.get("tools"), timeout=inp.get("timeout"),
                extra_body=inp.get("extra_body"), reasoning_config=inp.get("reasoning_config"),
                extra_headers=inp.get("extra_headers"), api_mode=inp.get("api_mode"),
                route_info=ri, async_mode=inp.get("async_mode", False))
            ret = {**proj(req), "route_info": ri}
        elif func == "_plan_aux_call":
            ri = route_info_copy()
            req, retry_kwargs, candidate_kwargs = NS["_plan_aux_call"](
                inp.get("task"), async_mode=inp.get("async_mode", False),
                provider=inp.get("provider"), model=inp.get("model"),
                base_url=inp.get("base_url"), api_key=inp.get("api_key"),
                main_runtime=inp.get("main_runtime"), messages=inp.get("messages") or [],
                temperature=inp.get("temperature"), max_tokens=inp.get("max_tokens"),
                tools=inp.get("tools"), timeout=inp.get("timeout"),
                extra_body=inp.get("extra_body"), reasoning_config=inp.get("reasoning_config"),
                extra_headers=inp.get("extra_headers"), api_mode=inp.get("api_mode"),
                route_info=ri)
            ret = {"req": {**proj(req), "route_info": ri},
                   "retry_kwargs": retry_kwargs, "candidate_kwargs": candidate_kwargs}
        elif func == "_fallback_request_kwargs":
            ret = NS["_fallback_request_kwargs"](
                p["destination"], task=inp.get("task"), messages=inp.get("messages") or [],
                tools=inp.get("tools"), temperature=inp.get("temperature"),
                max_tokens=inp.get("max_tokens"), effective_timeout=inp.get("effective_timeout"),
                effective_extra_body=inp.get("effective_extra_body"),
                reasoning_config=inp.get("reasoning_config"),
                fallback_entry=inp.get("fallback_entry") or {},
                task_config=inp.get("task_config") or {},
                apply_fast_lane=inp.get("apply_fast_lane", False))
        elif func == "_plan_fallback_candidate":
            dest, kw, rebuild = NS["_plan_fallback_candidate"](
                p.get("fb_client"), inp.get("fb_model"), inp.get("fb_label", ""),
                task=inp.get("task"), effective_timeout=inp.get("effective_timeout"),
                apply_fast_lane=inp.get("apply_fast_lane", False), **(inp.get("request") or {}))
            ret = {"destination": proj(dest), "kwargs": kw}
            rc = inp.get("rebuild_call")
            if isinstance(rc, dict):
                rd, rk = rebuild(rc.get("provider"), client_of(rc.get("client")), rc.get("model"))
                ret["rebuild"] = {"destination": proj(rd), "kwargs": rk}
        elif func == "_quarantine_fallback_candidate":
            NS["_quarantine_fallback_candidate"](
                inp.get("task"), inp.get("fb_label", ""), inp.get("fb_provider", ""),
                p.get("fb_err"), base_url=inp.get("base_url", ""), tag=inp.get("tag", ""),
                reason=inp.get("reason"), model=inp.get("model"))
            ret = None
        elif func == "_plan_fallback_auth_retry":
            fp, retry = NS["_plan_fallback_auth_retry"](
                p["destination"], ctx.recorder("rebuild"),
                async_mode=inp.get("async_mode", False),
                failed_api_key=inp.get("failed_api_key", ""))
            ret = [fp, [retry[0]._acf_id, retry[1], proj(retry[2])] if retry is not None else None]
        elif func == "_fallback_candidate_ladder":
            gen = NS["_fallback_candidate_ladder"](
                p.get("fb_client"), inp.get("fb_kwargs") or {}, p["destination"],
                ctx.recorder("rebuild"), task=inp.get("task"), fb_label=inp.get("fb_label", ""),
                tag=inp.get("tag", ""), async_mode=inp.get("async_mode", False))
            resp = drive(gen, ctx)
            ret = {"ret": norm_ret(resp)}
        elif func == "_candidate_classification_provider":
            ret = NS["_candidate_classification_provider"](
                p.get("fb_client"), inp.get("fb_model"), inp.get("fb_label", ""), inp.get("task"))
        else:
            out["error"] = "unknown func"
            return out
    except BaseException as exc:  # noqa: BLE001 - raise sentinel parity
        ret = proj_exc(exc)
        if func in ("_aux_recovery_ladder",):
            out["ret"] = {"ret": ret, "route_info": locals().get("ri")}
            out["trace"] = ctx.trace
            out["relay_context"] = proj(ctx.relay_context) if ctx.relay_context is not None else None
            return out
    out["ret"] = ret
    out["trace"] = ctx.trace
    out["relay_context"] = proj(ctx.relay_context) if ctx.relay_context is not None else None
    return out

# ── comparison ────────────────────────────────────────────────────────────
def deep_eq(a, b):
    return json.loads(json.dumps(a, default=str)) == json.loads(json.dumps(b, default=str))

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
                print(f"  C.{key}={json.dumps(c_out.get(key), default=str)[:600]}")
                print(f"  PY.{key}={json.dumps(py_out.get(key), default=str)[:600]}")
    if cases == 0:
        print("FATAL oracle_auxiliary_client_flow: 0 cases")
        print("ORACLE auxiliary_client_flow: 0 cases, 0 mismatches")
        return 2
    print(f"ORACLE auxiliary_client_flow: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
