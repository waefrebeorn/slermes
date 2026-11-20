#!/usr/bin/env python3
"""Shadow mirror of src/cli/observability_metrics_flow.c — a LITERAL Python
transcription of the C port (same tables, same statement order, same
json_obj_get PRESENCE semantics via the ABSENT sentinel, same fail-closed
dispatcher, same raise-spec values). Diffs the mirror against the LIVE
AST-extracted oracle drive over EVERY fixture case, plus fuzz probes:

  - full builtin exception-hierarchy MRO completeness (every builtins
    BaseException subclass vs the transcribed OM_EXC_PARENTS table),
  - exception_failure_class on randomized synthetic classes (network modules,
    builtin bases, tags, hooks),
  - _long_session / milestones_for predicates on randomized ASCII dicts.

Code-only verification: no compile / link / oracle-gate needed.
Usage: python tests/shadow_observability_metrics_flow.py [--fuzz]
"""
import importlib.util
import json
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location(
    "oracle_observability_metrics_flow", os.path.join(HERE, "oracle_observability_metrics_flow.py"))
O = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(O)

ABSENT = object()   # C NULL pointer from json_obj_get on a missing key


class RaiseVal:
    def __init__(self, spec):
        self.spec = spec


def is_raise(v):
    return isinstance(v, RaiseVal)


def mkraise(kind, msg):
    return RaiseVal({"__raise__": kind, "message": msg})


# ── accessors (C om_* literal) ──
def om_get(o, k):
    if isinstance(o, dict) and k in o:
        return o[k]
    return ABSENT


def om_is_null(v):
    return v is ABSENT or v is None


def om_is_obj(v):
    return isinstance(v, dict)


def om_is_arr(v):
    return isinstance(v, list)


def om_is_str(v):
    return isinstance(v, str)


def om_is_num(v):
    return isinstance(v, (int, float)) and not isinstance(v, bool)


def om_is_bool(v):
    return isinstance(v, bool)


def om_str(v):
    return v if isinstance(v, str) else None


def om_str_eq(a, b):
    return (a if a is not None else "") == (b if b is not None else "")


def om_truth(v):
    if v is ABSENT or v is None:
        return False
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v != 0
    if isinstance(v, str):
        return len(v) > 0
    if isinstance(v, (list, dict)):
        return len(v) > 0
    return False


def om_num(o, k, default):
    v = om_get(o, k)
    return v if om_is_num(v) else default


def om_num_integral(d):
    return float(d) == float(int(d)) and abs(d) < 9.2e18


def om_type_name(v):
    if v is ABSENT or v is None:
        return "NoneType"
    if isinstance(v, bool):
        return "bool"
    if isinstance(v, (int, float)):
        return "int" if om_num_integral(v) else "float"
    if isinstance(v, str):
        return "str"
    if isinstance(v, list):
        return "list"
    if isinstance(v, dict):
        if om_truth(om_get(v, "__exc__")):
            t = om_str(om_get(v, "type"))
            return t if t is not None else "Exception"
        return "SimpleNamespace"
    return "object"


def om_set_hashable(v):
    if om_is_obj(v):
        return mkraise("TypeError", "cannot use 'dict' as a set element (unhashable type: 'dict')")
    if om_is_arr(v):
        return mkraise("TypeError", "cannot use 'list' as a set element (unhashable type: 'list')")
    return None


def om_get_carrier(v):
    if v is ABSENT or isinstance(v, dict):
        return None
    return mkraise("AttributeError", "'%s' object has no attribute 'get'" % om_type_name(v))


def om_split_carrier(v):
    if om_is_str(v):
        return None
    return mkraise("AttributeError", "'%s' object has no attribute 'split'" % om_type_name(v))


def om_split_colon(s):
    s = s if s is not None else ""
    return s.split(":", 1)[0]


def om_str_in(v, set_):
    s = om_str(v)
    if s is None:
        return False
    return s in set_


def om_arg_j(a, k, v):
    if v is not ABSENT and v is not None:
        a[k] = v


def om_arg_s(a, k, v):
    if v is not None:
        a[k] = v


# ── static tables (transcribed from the C) ──
OM_SESSION_DURATION_THRESHOLDS = [(60000, "lt_1m"), (300000, "1m_to_5m"), (1800000, "5m_to_30m"),
                                  (7200000, "30m_to_2h"), (28800000, "2h_to_8h")]
OM_INSTALL_AGE_THRESHOLDS = [(3600, "lt_1h"), (86400, "1h_to_1d"), (604800, "1d_to_7d"),
                             (2592000, "7d_to_30d"), (7776000, "30d_to_90d")]
OM_TTFT_THRESHOLDS = [(0.5, "lt_500ms"), (1.0, "500ms_to_1s"), (2.0, "1s_to_2s"),
                      (5.0, "2s_to_5s"), (15.0, "5s_to_15s")]
OM_CONTEXT_FILL_THRESHOLDS = [(50, "lt_50"), (75, "50_to_75"), (90, "75_to_90"), (100, "90_to_100")]
OM_USAGE_KEYS = [("input", "input_tokens"), ("output", "output_tokens"),
                 ("cache_read", "cache_read_tokens"), ("cache_write", "cache_write_tokens"),
                 ("reasoning", "reasoning_tokens")]
OM_TASK_OUTCOMES = {"cancelled", "failed", "success", "timed_out", "unknown"}
OM_SETUP_SURFACES = {"cli", "desktop", "other"}
OM_EXTENSION_KINDS = {"mcp_server", "plugin", "skill"}
OM_EXTENSION_SOURCES = {"bundled", "catalog", "hub", "local", "other", "url"}
OM_COMPRESSION_FAILURE_CLASSES = {
    "attempt_superseded", "empty_post_handoff_window", "explicit_interrupt",
    "insufficient_messages", "lock_contended", "no_compressible_window", "snapshot_stale",
    "aux_model_fallback", "commit_fence_cancelled", "empty_transcript", "exception",
    "feasibility_skip", "no_progress", "none", "other", "pool_saturated", "rollback",
    "session_split_failed", "stall_deterministic_fallback", "stall_interrupted",
    "summary_auth_failure", "summary_empty_content_failure", "summary_generation_aborted",
    "summary_generation_failed", "summary_network_failure", "summary_overload_degraded",
    "summary_overload_failure", "summary_truncated_failure", "unknown", "would_grow",
}
OM_EXT_COMMON_FAILURE_CLASSES = {"exception", "filesystem_error", "network", "none", "other", "permission"}
OM_EXT_SKILL_EXTRA = {"ambiguous", "auth_rejected", "fetch_failed", "invalid_bundle", "invalid_name",
                      "not_found", "rate_limited", "scan_blocked", "stale_index"}
OM_EXT_PLUGIN_EXTRA = {"already_installed", "clone_failed", "deps_declined", "deps_failed", "git_missing",
                       "incompatible", "invalid_source", "manifest_invalid", "non_interactive",
                       "removed_from_catalog", "scan_blocked"}
OM_EXT_MCP_EXTRA = {"auth_required", "bootstrap_failed", "clone_failed", "config_invalid",
                    "config_rejected", "connect_failed", "git_missing", "missing_credentials",
                    "server_start_failed"}
OM_EXTENSION_REGISTRIES = {"browse-sh", "clawhub", "github", "hermes-index", "lobehub", "official",
                           "skills-sh", "url", "well-known", "none", "other", "unresolved"}
OM_SOURCE_ID_ALIASES = {"skills.sh": "skills-sh"}
OM_MODEL_IDENTIFIER_MAX_LENGTH = 256
OM_COMPRESSION_TRIGGERS = {
    "auto": "auto", "threshold": "auto", "preflight": "auto", "auto_threshold": "auto",
    "idle": "auto", "turn_start_threshold": "auto", "engine_preflight": "auto",
    "pre_api": "auto", "post_tool": "auto", "gateway_hygiene": "auto", "cache_expiry": "auto",
    "overflow": "overflow", "context_overflow": "overflow", "overflow_error": "overflow",
    "error": "overflow",
    "manual": "manual", "user": "manual", "command": "manual", "slash": "manual",
}
OM_COMPRESSION_OUTCOMES = {
    "success": "success", "ok": "success", "compressed": "success", "completed": "success",
    "skipped": "skipped", "noop": "skipped", "not_needed": "skipped",
}
OM_COMPRESSION_FAILURE_CLASS_ALIASES = {"summary_model_benched": "feasibility_skip"}
OM_NETWORK_MODULES = ["aiohttp", "h11", "h2", "httpcore", "httpx", "requests", "ssl", "urllib3", "websockets"]
OM_TASK_STARTED_METRIC = "hermes.task_run.started"
OM_TASK_FINISHED_METRIC = "hermes.task_run.finished"
OM_TOOL_USAGE_METRIC = "hermes.tool.usage.count"
OM_SKILL_LIFECYCLE_METRIC = "hermes.skill.lifecycle.count"
OM_SKILL_LOAD_METRIC = "hermes.skill.load.count"
OM_SESSION_METRIC = "hermes.session.count"
OM_SETUP_COMPLETED_METRIC = "hermes.setup.completed"

OM_EXC_PARENTS = {
    "BaseException": "object", "object": None,
    "ArithmeticError": "Exception", "AssertionError": "Exception", "AttributeError": "Exception",
    "BaseExceptionGroup": "BaseException", "BlockingIOError": "OSError",
    "BrokenPipeError": "ConnectionError", "BufferError": "Exception", "BytesWarning": "Warning",
    "ChildProcessError": "OSError", "ConnectionAbortedError": "ConnectionError",
    "ConnectionError": "OSError", "ConnectionRefusedError": "ConnectionError",
    "ConnectionResetError": "ConnectionError", "DeprecationWarning": "Warning",
    "EOFError": "Exception", "EncodingWarning": "Warning", "Exception": "BaseException",
    "ExceptionGroup": "BaseExceptionGroup", "FileExistsError": "OSError",
    "FileNotFoundError": "OSError", "FloatingPointError": "ArithmeticError",
    "FutureWarning": "Warning", "GeneratorExit": "BaseException", "ImportError": "Exception",
    "ImportWarning": "Warning", "IndentationError": "SyntaxError", "IndexError": "LookupError",
    "InterruptedError": "OSError", "IsADirectoryError": "OSError", "KeyError": "LookupError",
    "KeyboardInterrupt": "BaseException", "LookupError": "Exception", "MemoryError": "Exception",
    "ModuleNotFoundError": "ImportError", "NameError": "Exception",
    "NotADirectoryError": "OSError", "NotImplementedError": "RuntimeError", "OSError": "Exception",
    "OverflowError": "ArithmeticError", "PendingDeprecationWarning": "Warning",
    "PermissionError": "OSError", "ProcessLookupError": "OSError",
    "PythonFinalizationError": "RuntimeError", "RecursionError": "RuntimeError",
    "ReferenceError": "Exception", "ResourceWarning": "Warning", "RuntimeError": "Exception",
    "RuntimeWarning": "Warning", "StopAsyncIteration": "Exception", "StopIteration": "Exception",
    "SyntaxError": "Exception", "SyntaxWarning": "Warning", "SystemError": "Exception",
    "SystemExit": "BaseException", "TabError": "IndentationError", "TimeoutError": "OSError",
    "TypeError": "Exception", "UnboundLocalError": "NameError",
    "UnicodeDecodeError": "UnicodeError", "UnicodeEncodeError": "UnicodeError",
    "UnicodeError": "ValueError", "UnicodeTranslateError": "UnicodeError",
    "UnicodeWarning": "Warning", "UserWarning": "Warning", "ValueError": "Exception",
    "Warning": "Exception", "ZeroDivisionError": "ArithmeticError",
    "_IncompleteInputError": "SyntaxError",
}


def om_exc_builtin(name):
    return name != "object" and name in OM_EXC_PARENTS


def om_exc_mro(err):
    names, mods = [], []
    if om_is_obj(err) and om_truth(om_get(err, "__exc__")):
        t = om_str(om_get(err, "type"))
        if t is None:
            t = "Exception"
        if t == "ExceptionGroup":
            return (["ExceptionGroup", "BaseExceptionGroup", "Exception", "BaseException", "object"],
                    ["builtins"] * 5)
        if om_exc_builtin(t):
            cur = t
            while cur is not None and len(names) < 16:
                names.append(cur)
                mods.append("builtins")
                cur = OM_EXC_PARENTS[cur]
            return names, mods
        mro = om_get(err, "mro")
        if om_is_arr(mro):
            for e in mro:
                if len(names) >= 16:
                    break
                nm = om_str(om_get(e, "name"))
                md = om_str(om_get(e, "module"))
                names.append(nm if nm is not None else "object")
                mods.append(md if md is not None else "builtins")
            if names:
                return names, mods
        md = om_str(om_get(err, "module"))
        return [t, "object"], [md if md is not None else "builtins", "builtins"]
    if om_is_obj(err):
        return ["SimpleNamespace", "object"], ["types", "builtins"]
    if om_is_null(err):
        return ["NoneType", "object"], ["builtins", "builtins"]
    if om_is_str(err):
        return ["str", "object"], ["builtins", "builtins"]
    if om_is_bool(err):
        return ["bool", "object"], ["builtins", "builtins"]
    if om_is_arr(err):
        return ["list", "object"], ["builtins", "builtins"]
    if om_is_num(err):
        return ["int" if om_num_integral(err) else "float", "object"], ["builtins", "builtins"]
    return ["object", "object"], ["builtins", "builtins"]


# ── seam dispatcher (C harness om_h_dispatch literal) ──
class ShadowCtx:
    def __init__(self, io):
        self.io = io if isinstance(io, dict) else {}
        self.cursors = {}
        self.trace = []


def strip_nulls(v):
    if v is ABSENT:
        return None
    if not isinstance(v, dict):
        return v
    return {k: strip_nulls(x) for k, x in v.items() if x is not None}


def shadow_call(ctx, name, args):
    entry = ctx.io.get(name, ABSENT)
    present = entry is not ABSENT
    exhausted = False
    result = None
    if present:
        if isinstance(entry, list):
            idx = ctx.cursors.get(name, 0)
            if idx >= len(entry):
                exhausted = True
            else:
                result = entry[idx]
                ctx.cursors[name] = idx + 1
        else:
            result = entry
    failclosed = not present or exhausted
    actual = result
    if not failclosed and isinstance(actual, dict) and actual.get("__script__") is True:
        actual = actual.get("ret")
    if failclosed:
        retnode = {"__raise__": "RuntimeError", "message": "no script for " + name}
    else:
        retnode = actual
    ctx.trace.append({"ev": name, "args": strip_nulls(args),
                      "ret": retnode if failclosed else O.norm_value(retnode)})
    if failclosed:
        return RaiseVal(retnode)
    if actual is None:
        return None
    if isinstance(actual, dict) and "__raise__" in actual:
        return RaiseVal(actual)
    return actual


# ── seam wrappers ──
def om_norm(ctx, value):
    a = {}
    om_arg_j(a, "value", value)
    return shadow_call(ctx, "_norm", a)


def om_nn(ctx, value):
    a = {}
    om_arg_j(a, "value", value)
    return shadow_call(ctx, "contract._non_negative_number", a)


def om_bucket(ctx, value, th, last):
    a = {"value": value, "thresholds": [[u, l] for u, l in th], "last": last}
    return shadow_call(ctx, "_bucket", a)


def om_count_bucket(ctx, count):
    return shadow_call(ctx, "contract.count_bucket", {"count": count})


def om_size_bucket(ctx, count):
    return shadow_call(ctx, "contract.size_bucket", {"count": count})


def om_exec_surface(ctx, surface):
    kw = {}
    om_arg_j(kw, "execution_surface", surface)
    return shadow_call(ctx, "contract.execution_surface", {"kwargs": kw})


def om_model_error_class(ctx, reason):
    kw = {}
    om_arg_j(kw, "reason", reason)
    return shadow_call(ctx, "contract.model_error_class", {"kwargs": kw})


def om_provider_name(ctx, raw):
    a = {}
    om_arg_j(a, "raw", raw)
    return shadow_call(ctx, "catalog.provider_metric_name", a)


def om_model_name(ctx, raw, provider):
    a = {}
    om_arg_j(a, "raw", raw)
    om_arg_j(a, "provider", provider)
    a["max_length"] = OM_MODEL_IDENTIFIER_MAX_LENGTH
    return shadow_call(ctx, "catalog.model_metric_name", a)


def om_aux_name(ctx, raw):
    a = {}
    om_arg_j(a, "raw", raw)
    return shadow_call(ctx, "catalog.aux_task_metric_name", a)


def om_slash_name(ctx, raw):
    a = {}
    om_arg_j(a, "raw", raw)
    return shadow_call(ctx, "catalog.slash_command_metric_name", a)


def om_ext_name(ctx, kind, raw):
    a = {}
    om_arg_s(a, "kind", kind)
    om_arg_j(a, "raw", raw)
    return shadow_call(ctx, "catalog.extension_metric_name", a)


def om_user_created_skill(ctx, d):
    a = {}
    om_arg_j(a, "d", d)
    return shadow_call(ctx, "user_created_skill", a)


# ── exception cores ──
def om_tagged_core(error):
    tagged = om_get(error, "failure_class")
    if om_is_str(tagged) and tagged:
        return tagged
    return None


def om_exception_failure_class_core(error):
    tagged = om_tagged_core(error)
    if tagged is not None:
        return tagged
    names, mods = om_exc_mro(error)
    for nm in names:
        if nm == "PermissionError":
            return "permission"
    for nm, md in zip(names, mods):
        if nm in ("ConnectionError", "TimeoutError"):
            return "network"
        if nm == "gaierror" and md == "socket":
            return "network"
        if nm == "URLError" and md == "urllib.error":
            return "network"
        root = (md or "").split(".", 1)[0]
        if root in OM_NETWORK_MODULES:
            return "network"
    for nm in names:
        if nm == "OSError":
            return "filesystem_error"
    return "exception:" + names[0]


# ── ported functions (C statement order literal) ──
def om_number_core(ctx, value):
    return om_nn(ctx, value)


def om_provider_identifier_core(ctx, value):
    return om_provider_name(ctx, value)


def om_session_fields(ctx, inp):
    sf = om_get(inp, "start_fields")
    active_ms = om_num(inp, "active_ms", 0)
    mx = 0 if active_ms < 0 else active_ms
    ab = om_bucket(ctx, mx, OM_SESSION_DURATION_THRESHOLDS, "gte_8h")
    if is_raise(ab):
        return ab
    bad = om_get_carrier(sf)
    if bad is not None:
        return bad
    ep = om_get(sf, "entrypoint")
    es = om_get(sf, "execution_surface")
    ct = om_count_bucket(ctx, om_num(inp, "failed_turns", 0))
    if is_raise(ct):
        return ct
    lo = om_get(inp, "last_outcome")
    uh = om_set_hashable(lo)
    if uh is not None:
        return uh
    pf = om_get(sf, "platform")
    tb = om_size_bucket(ctx, om_num(inp, "turns", 0))
    if is_raise(tb):
        return tb
    return {
        "active_duration_bucket": ab,
        "entrypoint": ep if ep is not ABSENT else "unknown",
        "execution_surface": es if es is not ABSENT else "unknown",
        "failed_turn_count_bucket": ct,
        "last_outcome": lo if om_str_in(lo, OM_TASK_OUTCOMES) else "unknown",
        "platform": pf if pf is not ABSENT else "none",
        "turn_count_bucket": tb,
    }


def om_install_age_bucket(ctx, inp):
    age = om_number_core(ctx, om_get(inp, "age_seconds"))
    if is_raise(age):
        return age
    if age is None:
        return "unknown"
    a = age if om_is_num(age) else 0
    return om_bucket(ctx, a, OM_INSTALL_AGE_THRESHOLDS, "gte_90d")


def om_ttft_bucket(ctx, inp):
    ev = om_get(inp, "event")
    bad = om_get_carrier(ev)
    if bad is not None:
        return bad
    started = om_nn(ctx, om_get(ev, "started_at"))
    if is_raise(started):
        return started
    first = om_nn(ctx, om_get(ev, "first_chunk_at"))
    if is_raise(first):
        return first
    if started is None:
        return "unknown"
    if first is None:
        return "not_streamed"
    s = started if om_is_num(started) else 0
    f = first if om_is_num(first) else 0
    diff = f - s
    mx = 0.0 if diff < 0 else diff
    return om_bucket(ctx, mx, OM_TTFT_THRESHOLDS, "gte_15s")


def om_context_fill_core(ctx, tokens_raw, length_raw):
    tokens = om_nn(ctx, tokens_raw)
    if is_raise(tokens):
        return tokens
    length = om_nn(ctx, length_raw)
    if is_raise(length):
        return length
    if tokens is None:
        return "unknown"
    if length is None or (om_is_num(length) and length == 0):
        return "unknown"
    t = tokens if om_is_num(tokens) else 0
    l = length if om_is_num(length) else 0
    return om_bucket(ctx, 100.0 * t / l, OM_CONTEXT_FILL_THRESHOLDS, "gte_100")


def om_model_token_fields(ctx, inp):
    usage = om_get(inp, "usage")
    if not om_is_obj(usage):
        return None
    amounts = []
    any_ = False
    for _tt, key in OM_USAGE_KEYS:
        v = om_get(usage, key)
        if om_is_num(v) and om_num_integral(v) and v > 0:
            amounts.append(int(v))
            any_ = True
        else:
            amounts.append(0)
    if not any_:
        return None
    call_role = om_get(inp, "call_role")
    provider = om_get(inp, "provider")
    o = {}
    if om_str_eq(om_str(call_role), "auxiliary"):
        aux = om_aux_name(ctx, om_get(inp, "aux_task"))
        if is_raise(aux):
            return aux
        o["aux_task"] = aux
    else:
        o["aux_task"] = "none"
    o["call_role"] = call_role if call_role is not ABSENT else None
    p1 = om_provider_identifier_core(ctx, provider)
    if is_raise(p1):
        return p1
    model = om_model_name(ctx, om_get(inp, "model"), p1)
    if is_raise(model):
        return model
    o["model"] = model
    p2 = om_provider_identifier_core(ctx, provider)
    if is_raise(p2):
        return p2
    o["provider"] = p2
    for (tt, _key), amt in zip(OM_USAGE_KEYS, amounts):
        o[tt] = amt
    return o


def om_setup_completed_fields(ctx, inp):
    value = om_norm(ctx, om_get(inp, "surface"))
    if is_raise(value):
        return value
    o = {}
    provider = om_get(inp, "provider")
    if om_truth(provider):
        p = om_provider_identifier_core(ctx, provider)
        if is_raise(p):
            return p
        o["provider"] = p
    else:
        o["provider"] = "none"
    uh = om_set_hashable(value)
    if uh is not None:
        return uh
    o["surface"] = value if om_str_in(value, OM_SETUP_SURFACES) else "other"
    return o


def om_compression_failure_class_core(ctx, outcome, failure_class):
    nv = om_norm(ctx, failure_class if om_is_str(failure_class) else None)
    if is_raise(nv):
        return nv
    bad = om_split_carrier(nv)
    if bad is not None:
        return bad
    value = om_split_colon(nv)
    if om_str_eq(outcome, "success"):
        value = OM_COMPRESSION_FAILURE_CLASS_ALIASES.get(value, value)
        if value == "" or value == "none":
            return "none"
    if value == "":
        return "unknown"
    ok = value in OM_COMPRESSION_FAILURE_CLASSES
    if value in ("none", "unknown"):
        ok = False
    return value if ok else "other"


def om_compression_fields(ctx, inp):
    ov = om_norm(ctx, om_get(inp, "outcome"))
    if is_raise(ov):
        return ov
    outcome_value = "failed"
    ovs = om_str(ov)
    if ovs is not None and ovs in OM_COMPRESSION_OUTCOMES:
        outcome_value = OM_COMPRESSION_OUTCOMES[ovs]
    o = {}
    cf = om_context_fill_core(ctx, om_get(inp, "tokens_before"), om_get(inp, "context_length"))
    if is_raise(cf):
        return cf
    o["context_fill_bucket"] = cf
    fc = om_compression_failure_class_core(ctx, outcome_value, om_get(inp, "failure_class"))
    if is_raise(fc):
        return fc
    o["failure_class"] = fc
    o["outcome"] = outcome_value
    tv = om_norm(ctx, om_get(inp, "trigger"))
    if is_raise(tv):
        return tv
    trigger = "other"
    tvs = om_str(tv)
    if tvs is not None and tvs in OM_COMPRESSION_TRIGGERS:
        trigger = OM_COMPRESSION_TRIGGERS[tvs]
    o["trigger"] = trigger
    return o


def om_model_switch_fields(ctx, inp):
    o = {}
    es = om_exec_surface(ctx, om_get(inp, "surface"))
    if is_raise(es):
        return es
    o["execution_surface"] = es
    fp = om_provider_identifier_core(ctx, om_get(inp, "from_provider"))
    if is_raise(fp):
        return fp
    o["from_provider"] = fp
    tp = om_provider_identifier_core(ctx, om_get(inp, "to_provider"))
    if is_raise(tp):
        return tp
    o["to_provider"] = tp
    return o


def om_fallback_fields(ctx, inp):
    reason = om_get(inp, "reason")
    value = reason
    if om_is_obj(reason):
        v = om_get(reason, "value")
        if v is not ABSENT:
            value = v
    o = {}
    ec = om_model_error_class(ctx, value)
    if is_raise(ec):
        return ec
    o["error_class"] = ec
    fp = om_provider_identifier_core(ctx, om_get(inp, "from_provider"))
    if is_raise(fp):
        return fp
    o["from_provider"] = fp
    tp = om_provider_identifier_core(ctx, om_get(inp, "to_provider"))
    if is_raise(tp):
        return tp
    o["to_provider"] = tp
    return o


def om_slash_command_fields(ctx, inp):
    o = {}
    cn = om_slash_name(ctx, om_get(inp, "command"))
    if is_raise(cn):
        return cn
    o["command"] = cn
    es = om_exec_surface(ctx, om_get(inp, "surface"))
    if is_raise(es):
        return es
    o["execution_surface"] = es
    return o


def om_extension_failure_class_core(ctx, kind, outcome, failure_class, error):
    if om_str_eq(outcome, "success"):
        return "none"
    value = None
    if om_is_str(failure_class) and failure_class:
        value = failure_class
    if value is None and om_is_obj(error) and om_truth(om_get(error, "__exc__")):
        value = om_exception_failure_class_core(error)
    nv = om_norm(ctx, value)
    if is_raise(nv):
        return nv
    bad = om_split_carrier(nv)
    if bad is not None:
        return bad
    val = om_split_colon(nv)
    if kind == "skill":
        extra = OM_EXT_SKILL_EXTRA
    elif kind == "plugin":
        extra = OM_EXT_PLUGIN_EXTRA
    elif kind == "mcp_server":
        extra = OM_EXT_MCP_EXTRA
    else:
        return mkraise("KeyError", "'%s'" % kind)
    ok = val in OM_EXT_COMMON_FAILURE_CLASSES or val in extra
    if val == "none":
        ok = False
    return val if ok else "other"


def om_extension_registry_core(ctx, kind, registry):
    if not om_str_eq(kind, "skill"):
        return "none"
    nv = om_norm(ctx, registry)
    if is_raise(nv):
        return nv
    value = om_str(nv)
    if not value:
        value = "none"
    value = OM_SOURCE_ID_ALIASES.get(value, value)
    return value if value in OM_EXTENSION_REGISTRIES else "other"


def om_extension_install_fields(ctx, inp):
    kv = om_norm(ctx, om_get(inp, "kind"))
    if is_raise(kv):
        return kv
    uh = om_set_hashable(kv)
    if uh is not None:
        return uh
    if not om_str_in(kv, OM_EXTENSION_KINDS):
        return None
    sv = om_norm(ctx, om_get(inp, "source"))
    if is_raise(sv):
        return sv
    ov = om_norm(ctx, om_get(inp, "outcome"))
    if is_raise(ov):
        return ov
    outcome_value = "success" if om_str(ov) in ("success", "ok", "installed") else "failed"
    o = {"kind": kv}
    name = om_ext_name(ctx, kv, om_get(inp, "name"))
    if is_raise(name):
        return name
    o["name"] = name
    o["outcome"] = outcome_value
    uh2 = om_set_hashable(sv)
    if uh2 is not None:
        return uh2
    o["source"] = sv if om_str_in(sv, OM_EXTENSION_SOURCES) else "other"
    fc = om_extension_failure_class_core(ctx, kv, outcome_value, om_get(inp, "failure_class"),
                                         om_get(inp, "error"))
    if is_raise(fc):
        return fc
    o["failure_class"] = fc
    reg = om_extension_registry_core(ctx, kv, om_get(inp, "registry"))
    if is_raise(reg):
        return reg
    o["registry"] = reg
    return o


def om_long_session_core(ctx, d):
    bad = om_get_carrier(d)
    if bad is not None:
        return bad
    v = om_get(d, "turn_count_bucket")
    uh = om_set_hashable(v)
    if uh is not None:
        return uh
    return not om_str_in(v, {"0", "1", "2", "3_to_5"})


OM_PRED_ALWAYS, OM_PRED_PLATFORM, OM_PRED_SCHEDULED, OM_PRED_OUTCOME_SUCCESS, \
    OM_PRED_MCP, OM_PRED_DELEGATE, OM_PRED_USER_SKILL, OM_PRED_REUSED, \
    OM_PRED_LONG_SESSION = range(9)
OM_MILESTONE_RULES = [
    (OM_TASK_STARTED_METRIC, "first_task_started", OM_PRED_ALWAYS),
    (OM_TASK_STARTED_METRIC, "first_gateway_message", OM_PRED_PLATFORM),
    (OM_TASK_STARTED_METRIC, "first_scheduled_task", OM_PRED_SCHEDULED),
    (OM_TASK_FINISHED_METRIC, "first_task_success", OM_PRED_OUTCOME_SUCCESS),
    (OM_TOOL_USAGE_METRIC, "first_tool_success", OM_PRED_OUTCOME_SUCCESS),
    (OM_TOOL_USAGE_METRIC, "first_mcp_tool_success", OM_PRED_MCP),
    (OM_TOOL_USAGE_METRIC, "first_delegation", OM_PRED_DELEGATE),
    (OM_SKILL_LIFECYCLE_METRIC, "first_skill_created", OM_PRED_USER_SKILL),
    (OM_SKILL_LOAD_METRIC, "first_skill_reused", OM_PRED_REUSED),
    (OM_SESSION_METRIC, "first_long_session", OM_PRED_LONG_SESSION),
    (OM_SETUP_COMPLETED_METRIC, "setup_completed", OM_PRED_ALWAYS),
]


def om_milestones_for(ctx, inp):
    metric = om_str(om_get(inp, "metric_name"))
    dims = om_get(inp, "dimensions")
    out = []
    for m, milestone, pred in OM_MILESTONE_RULES:
        if not om_str_eq(metric if metric is not None else "", m):
            continue
        reached = False
        err = None
        if pred == OM_PRED_ALWAYS:
            reached = True
        elif pred == OM_PRED_PLATFORM:
            err = om_get_carrier(dims)
            if err is None:
                p = om_get(dims, "platform")
                err = om_set_hashable(p)
                if err is None:
                    reached = not (om_is_null(p) or om_str_eq(om_str(p), "none"))
        elif pred == OM_PRED_SCHEDULED:
            err = om_get_carrier(dims)
            if err is None:
                reached = om_str_eq(om_str(om_get(dims, "entrypoint")), "scheduled_task")
        elif pred == OM_PRED_OUTCOME_SUCCESS:
            err = om_get_carrier(dims)
            if err is None:
                reached = om_str_eq(om_str(om_get(dims, "outcome")), "success")
        elif pred == OM_PRED_MCP:
            err = om_get_carrier(dims)
            if err is None:
                reached = (om_str_eq(om_str(om_get(dims, "outcome")), "success")
                           and om_str_eq(om_str(om_get(dims, "tool_name")), "mcp"))
        elif pred == OM_PRED_DELEGATE:
            err = om_get_carrier(dims)
            if err is None:
                reached = (om_str_eq(om_str(om_get(dims, "outcome")), "success")
                           and om_str_eq(om_str(om_get(dims, "tool_name")), "delegate_task"))
        elif pred == OM_PRED_USER_SKILL:
            res = om_user_created_skill(ctx, dims)
            if is_raise(res):
                err = res
            else:
                reached = om_truth(res)
        elif pred == OM_PRED_REUSED:
            err = om_get_carrier(dims)
            if err is None:
                reached = om_str_eq(om_str(om_get(dims, "reuse_state")), "reused")
        elif pred == OM_PRED_LONG_SESSION:
            res = om_long_session_core(ctx, dims)
            if is_raise(res):
                err = res
            else:
                reached = om_truth(res)
        if err is not None:
            return err
        if reached:
            out.append(milestone)
    return out


DISPATCH = {
    "_number": lambda ctx, inp: om_number_core(ctx, om_get(inp, "value")),
    "provider_identifier": lambda ctx, inp: om_provider_identifier_core(ctx, om_get(inp, "value")),
    "session_fields": om_session_fields,
    "install_age_bucket": om_install_age_bucket,
    "ttft_bucket": om_ttft_bucket,
    "context_fill_bucket": lambda ctx, inp: om_context_fill_core(
        ctx, om_get(inp, "tokens_before"), om_get(inp, "context_length")),
    "model_token_fields": om_model_token_fields,
    "setup_completed_fields": om_setup_completed_fields,
    "compression_failure_class": lambda ctx, inp: om_compression_failure_class_core(
        ctx, om_str(om_get(inp, "outcome")) or "", om_get(inp, "failure_class")),
    "compression_fields": om_compression_fields,
    "model_switch_fields": om_model_switch_fields,
    "fallback_fields": om_fallback_fields,
    "slash_command_fields": om_slash_command_fields,
    "extension_install_fields": om_extension_install_fields,
    "tagged_failure_class": lambda ctx, inp: om_tagged_core(om_get(inp, "error")),
    "exception_failure_class": lambda ctx, inp: om_exception_failure_class_core(om_get(inp, "error")),
    "extension_failure_class": lambda ctx, inp: om_extension_failure_class_core(
        ctx, om_str(om_get(inp, "kind")) or "", om_str(om_get(inp, "outcome")) or "",
        om_get(inp, "failure_class"), om_get(inp, "error")),
    "extension_registry": lambda ctx, inp: om_extension_registry_core(
        ctx, om_str(om_get(inp, "kind")) or "", om_get(inp, "registry")),
    "_long_session": lambda ctx, inp: om_long_session_core(ctx, om_get(inp, "d")),
    "milestones_for": om_milestones_for,
}


def run_shadow(func, inp):
    ctx = ShadowCtx(inp.get("io"))
    fn = DISPATCH.get(func)
    if fn is None:
        ret = {"__raise__": "RuntimeError", "message": "unknown func"}
    else:
        r = fn(ctx, inp)
        ret = r.spec if is_raise(r) else r
    return O.norm_value({"ret": ret, "trace": ctx.trace, "st": {}})


def diff_fixtures(fixtures, verbose=True):
    mism = 0
    for i, rec in enumerate(fixtures):
        func = rec["func"]
        inp = rec.get("in") or {}
        want = O.run_case(func, inp)
        got = run_shadow(func, inp)
        if got != want:
            mism += 1
            if verbose and mism <= 12:
                print("SHADOW MISMATCH [%s] case %d" % (func, i))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s SHADOW: %s" % (key, json.dumps(got.get(key))[:700]))
                        print("  %s LIVE  : %s" % (key, json.dumps(want.get(key))[:700]))
    print("SHADOW observability_metrics_flow: %d cases, %d mismatches" % (len(fixtures), mism))
    return mism


# ── fuzz probes ──
def fuzz_mro():
    bad = 0
    for nm, cls in O.BUILTIN_EXC.items():
        names, mods = om_exc_mro({"__exc__": True, "type": nm, "module": "builtins"})
        real = [c.__name__ for c in cls.__mro__]
        if names != real or mods != ["builtins"] * len(real):
            bad += 1
            print("MRO MISMATCH %s: shadow=%s real=%s" % (nm, names, real))
    print("FUZZ mro: %d builtin classes, %d mismatches" % (len(O.BUILTIN_EXC), bad))
    return bad


def fuzz_exception_class(n=600):
    rng = random.Random(20261119)
    bases = ["Exception", "OSError", "PermissionError", "ConnectionError", "TimeoutError",
             "ValueError", "RuntimeError", "FileNotFoundError", "BaseException",
             "ConnectionResetError", "InterruptedError", "UnicodeDecodeError"]
    mods = ["builtins", "probe", "httpx", "aiohttp", "requests", "ssl", "urllib3", "websockets",
            "h11", "h2", "httpcore", "socket", "urllib.error", "my.pkg.sub"]
    bad = 0
    for i in range(n):
        base = rng.choice(bases)
        mod = rng.choice(mods)
        t = "Fuzz%d" % i
        real_base = O.BUILTIN_EXC[base]
        cls = type(t, (real_base,), {"__module__": mod})
        mro = [{"name": c.__name__, "module": c.__module__} for c in cls.__mro__]
        double = {"__exc__": True, "type": t, "module": mod, "mro": mro, "message": "x"}
        if rng.random() < 0.2:
            double["failure_class"] = rng.choice(["tagged", "", "network"])
        inst = O.mk_exc_double(double)
        want = O.NS["exception_failure_class"](inst)
        got = om_exception_failure_class_core(double)
        if got != want:
            bad += 1
            if bad <= 8:
                print("EXC FUZZ MISMATCH %s base=%s mod=%s: shadow=%r live=%r" % (t, base, mod, got, want))
        # tagged path
        want_t = O.NS["tagged_failure_class"](inst)
        got_t = om_tagged_core(double)
        if got_t != want_t:
            bad += 1
            print("TAG FUZZ MISMATCH %s: shadow=%r live=%r" % (t, got_t, want_t))
    print("FUZZ exception_failure_class: %d cases, %d mismatches" % (n, bad))
    return bad


def fuzz_predicates(n=600):
    rng = random.Random(424242)
    buckets = ["0", "1", "2", "3_to_5", "6_to_10", "gte_11", None, 5, "bogus"]
    platforms = ["none", "telegram", "cli", None, "discord", 0]
    outcomes = ["success", "failed", None, "cancelled"]
    tools = ["mcp", "delegate_task", "browser", None, "terminal"]
    bad = 0
    for _ in range(n):
        d = {}
        if rng.random() < 0.8:
            d["turn_count_bucket"] = rng.choice(buckets)
        if rng.random() < 0.8:
            d["platform"] = rng.choice(platforms)
        if rng.random() < 0.8:
            d["outcome"] = rng.choice(outcomes)
        if rng.random() < 0.8:
            d["tool_name"] = rng.choice(tools)
        if rng.random() < 0.5:
            d["entrypoint"] = rng.choice(["scheduled_task", "cli", None])
        if rng.random() < 0.5:
            d["reuse_state"] = rng.choice(["reused", "first_use", None])
        want = O.NS["_long_session"](d)
        got = om_long_session_core(None, d)
        if is_raise(got) or got != want:
            bad += 1
            print("LONGSESSION FUZZ MISMATCH %r: shadow=%r live=%r" % (d, got, want))
        for metric in (OM_TASK_STARTED_METRIC, OM_TASK_FINISHED_METRIC, OM_TOOL_USAGE_METRIC,
                       OM_SKILL_LOAD_METRIC, OM_SESSION_METRIC, OM_SETUP_COMPLETED_METRIC, "nope"):
            want = O.NS["milestones_for"](metric, d)
            got = om_milestones_for(ShadowCtx({}), {"metric_name": metric, "dimensions": d})
            if is_raise(got):
                bad += 1
                print("MILESTONE FUZZ RAISE %s %r: %r" % (metric, d, got.spec))
            elif tuple(got) != want:
                bad += 1
                print("MILESTONE FUZZ MISMATCH %s %r: shadow=%r live=%r" % (metric, d, got, want))
    print("FUZZ predicates: %d dicts, %d mismatches" % (n, bad))
    return bad


def main():
    fx = os.path.join(HERE, "fixtures", "observability_metrics_flow.jsonl")
    recs = [json.loads(l) for l in open(fx, encoding="utf-8").read().splitlines() if l.strip()]
    bad = diff_fixtures(recs)
    if "--fuzz" in sys.argv:
        bad += fuzz_mro()
        bad += fuzz_exception_class()
        bad += fuzz_predicates()
    print("SHADOW TOTAL: %d mismatches" % bad)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())