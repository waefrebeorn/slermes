#!/usr/bin/env python3
"""Oracle for src/agent/tool_result_processing_flow.c — drives the LIVE
agent/tool_executor.py tool-result-processing FLOW defs + leaves under
IDENTICAL scripted seams.

NOTE: the task brief names "agent/tool_result_processing.py"; no such quarry
file exists. The live tool-result-processing code is in agent/tool_executor.py
and the PoP lines in the C port point there.

The 24 targets (19 module functions + _ToolCallRef.middleware_kwargs /
emit_post / emit_cancelled / emit_invalid_arguments + _ParsedCall.ref) are
AST-extracted in source order; module Assign constants are seeded
(plain-Name targets only), the real _ToolCallRef / _ParsedCall /
_ManagedToolResult / _SequentialDispatch dataclasses and the real
_ToolTimeoutResult / _ToolCancelledResult marker classes are extracted, and
every node is compiled with the PEP 563 future flag so annotations stay
strings. The exec namespace is a registered module (py3.14 dataclasses needs
sys.modules[__name__]).

All live sibling / hook / clock / callback I/O is replaced by recording stubs
consuming the SAME "io" scripts the C harness feeds its universal seam:
absent/null -> None on every call; {"seq": [...]} -> per-call sequence (None
when exhausted); any other value -> reused. Each call records {ev, args, ret}
(ret = RAW scripted value) in call order. In-function imports record an
import_<name> event through a PEP 562 module __getattr__; a scripted raise on
that event fails the import exactly as live.

Value model: attribute doubles {"__attr__": ...} <-> SimpleNamespace;
callable doubles {"__callable__": ...}; facade doubles {"__budget__"},
{"__guardrails__"}, {"__subdir__"}, {"__spinner__"}, {"__event__"},
{"__thread__"}, {"__lock__"}, {"__set__": true, "items": [...]}; dataclasses /
marker strings normalize to tagged dicts on BOTH sides; _SequentialDispatch
callables project to kind descriptors (execute by closure inspection,
error_result by applying a probe exception); frozenset[str] normalizes to a
sorted deduped array. A scripted BaseException (KeyboardInterrupt /
SystemExit / GeneratorExit / CancelledError) propagates past `except
Exception` guards exactly as live; the C port walks the FULL builtin exception
parent table for the same check. Raise-spec msg carries str(e).

State mutations made before a raise stay visible in the state window on both
sides (the C harness mutates its state copy in place).

Input: stdin JSON lines from t_port_tool_result_processing_flow:
    {"func": ..., "in": {...}, "out": {"ret","raise","trace","state"}}
Summary: ORACLE tool_result_processing_flow: N cases, M mismatches.
Fail-closed: exit 1 on any mismatch, on a PYERR, or when zero cases ran.
"""
import ast
import contextlib
import copy
import dataclasses
import json
import os
import sys
import types
import __future__
from dataclasses import dataclass
from typing import Any, Callable, Optional

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "agent", "tool_executor.py")
INTERRUPT_SRC = os.path.join(QUARRY, "agent", "interrupt_control.py")

MODULE_TARGETS = {
    "_record_persisted_path_for_stub", "_canonical_tool_name",
    "_append_skipped_tool_results", "_commit_tool_result",
    "_finalize_tool_batch", "_print_tool_completed", "_append_batch_results",
    "_start_quiet_tool_spinner", "_finish_quiet_tool_spinner",
    "_resolve_sequential_dispatch", "_skip_remaining_sequential",
    "_append_invalid_arguments_result", "_run_sequential_call",
    "_publish_sequential_result", "_execute_tool_calls_sequential",
    "_set_worker_activity_callback", "_run_tool_activity_heartbeat",
    "_registered_tool_worker", "_run_with_activity_heartbeat",
}
METHOD_TARGETS = {"middleware_kwargs", "emit_post", "emit_cancelled",
                  "emit_invalid_arguments", "ref"}
WANTED = MODULE_TARGETS | METHOD_TARGETS

# Claimed siblings + shared leaves seamed when not the target.
MODULE_SEAMS = {
    "_tc_name", "_pairing_tool_call_id", "_flush_session_db_after_tool_progress",
    "_preview", "_safe_callback", "_tool_progress_enabled", "_parse_tool_call",
    "_budget_for_agent", "_run_sequential_tool_execution_middleware",
    "_unfinished_tool_result", "_emit_tool_complete_and_risk",
    "_persist_multimodal_text_parts", "_delegate_spinner_label",
    "_emit_terminal_post_tool_call", "interrupt_skip_wording",
    "maybe_persist_tool_result", "enforce_turn_budget", "extract_persisted_path",
    "_redact_tool_args_for_display", "_is_multimodal_tool_result",
    "_multimodal_text_summary", "_append_subdir_hint_to_multimodal",
    "get_active_env", "note_tool_result", "record_tool_batch", "stamp_data_use",
    "sync_rewritten_tool_rows", "observe_onboarding_result",
    "apply_transform_tool_result", "_detect_tool_failure",
    "_get_cute_tool_message_impl", "_tool_row_emoji", "_build_tool_label",
    "is_context_engine_tool", "is_memory_provider_tool", "conversation_id",
    "_ra", "make_tool_result_message",
}

CUR = {"in": {}, "trace": [], "counts": {}}


def rec(ev):
    CUR["trace"].append(ev)


def script_next(name):
    io = CUR["in"].get("io") or {}
    s = io.get(name)
    if s is None:
        return None
    if isinstance(s, dict) and "seq" in s:
        seq = s["seq"]
        i = CUR["counts"].get(name, 0)
        CUR["counts"][name] = i + 1
        return seq[i] if i < len(seq) else None
    return s


BUILTIN_EXC = {
    "Exception": Exception, "RuntimeError": RuntimeError, "ValueError": ValueError,
    "TypeError": TypeError, "KeyError": KeyError, "AttributeError": AttributeError,
    "OSError": OSError, "TimeoutError": TimeoutError, "LookupError": LookupError,
    "IndexError": IndexError, "NameError": NameError, "AssertionError": AssertionError,
    "ImportError": ImportError, "StopIteration": StopIteration,
    "CancelledError": __import__("asyncio").CancelledError, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
}


def make_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    if cls == "KeyError":
        try:
            return KeyError(ast.literal_eval(msg))
        except (ValueError, SyntaxError):
            return KeyError(msg)
    if cls in BUILTIN_EXC:
        return BUILTIN_EXC[cls](msg)
    return type(cls, (Exception,), {})(msg)


# ── value model: normalize (live -> tagged JSON) / materialize (JSON -> live) ──

class CallableDouble:
    """Inert truthy callable (never invoked in the fixture domain)."""
    def __call__(self, *a, **k):
        raise AssertionError("inert callable double invoked")


class BudgetDouble:
    pass


class GuardrailsDouble:
    def record_persisted_result(self, tool_call_id, path):
        return stub("guardrails.record_persisted_result")(tool_call_id, path)


class SubdirHintsDouble:
    def check_tool_call(self, name, args):
        return stub("subdirectory_hints.check_tool_call")(name, args)


class SpinnerDouble:
    def start(self):
        return stub("spinner.start")()

    def stop(self, text):
        return stub("spinner.stop")(text)


class EventDouble:
    def wait(self, timeout=None):
        return stub("event.wait")(timeout)

    def set(self):
        return stub("event.set")()


class ThreadDouble:
    def start(self):
        return stub("thread.start")()

    def join(self, timeout=None):
        return stub("thread.join")(timeout=timeout)


class LockDouble:
    def __enter__(self):
        r = script_next("lock.__enter__")
        rec({"ev": "lock.__enter__", "args": {}, "ret": r})
        if isinstance(r, dict) and "__raise__" in r:
            raise make_exc(r)
        return r

    def __exit__(self, *a):
        r = script_next("lock.__exit__")
        rec({"ev": "lock.__exit__", "args": {}, "ret": r})
        if isinstance(r, dict) and "__raise__" in r:
            raise make_exc(r)
        return False


class SetDouble:
    def __init__(self, items=None):
        self.items = list(items or [])

    def add(self, tid):
        r = stub("worker_threads.add")(tid)
        if tid not in self.items:
            self.items.append(tid)
        return r

    def discard(self, tid):
        r = stub("worker_threads.discard")(tid)
        if tid in self.items:
            self.items.remove(tid)
        return r


class LoggerDouble:
    def _log(self, level, fmt, *args, **kw):
        return stub("logger." + level)(fmt, *args, **kw)

    def debug(self, fmt, *args, **kw):
        return self._log("debug", fmt, *args, **kw)

    def info(self, fmt, *args, **kw):
        return self._log("info", fmt, *args, **kw)

    def warning(self, fmt, *args, **kw):
        return self._log("warning", fmt, *args, **kw)

    def error(self, fmt, *args, **kw):
        return self._log("error", fmt, *args, **kw)


def cb_proj(v):
    return "<callable>" if callable(v) else norm_ret(v)


def proj_execute(fn, name):
    """Project a dispatch execute callable to its kind descriptor (the C port
    emits the same descriptor from the branch it took). A seam-scripted
    dispatch already carries the descriptor dict — pass it through."""
    if fn is None:
        return None
    if isinstance(fn, dict):
        return fn
    if callable(fn) and hasattr(fn, "__seam_name__"):
        return {"__callable__": fn.__seam_name__}
    if getattr(fn, "__name__", "") == "_execute":
        return {"kind": "registry", "name": name}
    fv = fn.__code__.co_freevars
    if "inline_executor" in fv:
        ex = fn.__closure__[fv.index("inline_executor")].cell_contents
        return {"kind": "inline", "executor": norm_ret(ex)}
    if "call_with_arg_checks" in fv:
        return {"kind": "delegate", "name": name}
    if "messages" in fv:
        return {"kind": "context_engine", "name": name}
    return {"kind": "memory", "name": name}


def proj_error_result(fn, name):
    """Project a dispatch error_result lambda by applying a probe exception
    (pure formatting lambda; no seam fires). Descriptor dicts pass through."""
    if fn is None:
        return None
    if isinstance(fn, dict):
        return fn
    if callable(fn) and hasattr(fn, "__seam_name__"):
        return {"__callable__": fn.__seam_name__}
    s = fn(Exception("probe"))
    if s.startswith('{"error": "Context engine tool '):
        return {"kind": "context_engine", "name": name}
    if s.startswith('{"error": "Memory tool '):
        return {"kind": "memory", "name": name}
    return {"kind": "registry", "name": name}


def norm_dispatch(d, name):
    if d is None:
        return None
    return {
        "_type": "_SequentialDispatch",
        "execute": proj_execute(d.execute, name),
        "spinner": norm_ret(d.spinner),
        "middleware_trace_arg": norm_ret(d.middleware_trace_arg),
        "error_result": proj_error_result(d.error_result, name),
        "error_log": d.error_log,
        "handles_keyboard_interrupt": d.handles_keyboard_interrupt,
        "is_delegate": d.is_delegate,
        "finish_spinner": d.finish_spinner,
        "finish_in_finally": d.finish_in_finally,
        "transform_applied": d.transform_applied,
    }


def norm_ret(x):
    if isinstance(x, BaseException):
        return {"__raise__": type(x).__name__, "msg": str(x)}
    if isinstance(x, CallableDouble):
        return {"__callable__": True}
    if isinstance(x, BudgetDouble):
        return {"__budget__": True}
    if isinstance(x, GuardrailsDouble):
        return {"__guardrails__": True}
    if isinstance(x, SubdirHintsDouble):
        return {"__subdir__": True}
    if isinstance(x, SpinnerDouble):
        return {"__spinner__": True}
    if isinstance(x, EventDouble):
        return {"__event__": True}
    if isinstance(x, ThreadDouble):
        return {"__thread__": True}
    if isinstance(x, LockDouble):
        return {"__lock__": True}
    if isinstance(x, SetDouble):
        return {"__set__": True, "items": [norm_ret(v) for v in x.items]}
    if callable(x) and hasattr(x, "__seam_name__"):
        return {"__callable__": x.__seam_name__}
    if dataclasses.is_dataclass(x) and not isinstance(x, type):
        tn = type(x).__name__
        if tn == "_SequentialDispatch":
            return norm_dispatch(x, getattr(x, "_probe_name", "") or "")
        if tn in ("_ToolCallRef", "_ManagedToolResult", "_ParsedCall"):
            d = {"_type": tn}
            for f in dataclasses.fields(x):
                d[f.name] = norm_ret(getattr(x, f.name))
            return d
        return norm_ret(dataclasses.asdict(x))
    if type(x).__name__ in ("_ToolTimeoutResult", "_ToolCancelledResult"):
        return {"_type": type(x).__name__, "value": str(x)}
    if isinstance(x, types.SimpleNamespace):
        return {"__attr__": {k: norm_ret(v) for k, v in vars(x).items()}}
    if isinstance(x, frozenset):
        return sorted(norm_ret(v) for v in x)
    if isinstance(x, tuple):
        return [norm_ret(v) for v in x]
    if isinstance(x, list):
        return [norm_ret(v) for v in x]
    if isinstance(x, dict):
        return {k: norm_ret(v) for k, v in x.items()}
    if isinstance(x, float) and x.is_integer():
        return int(x)
    return x


def patch_ref_methods(ref):
    """Instance-level seam overrides for the _ToolCallRef methods a flow calls
    on an INPUT ref (the methods themselves are separate port targets)."""
    if isinstance(ref, _ToolCallRef):
        ref.emit_post = stub("emit_post")
        ref.emit_cancelled = stub("emit_cancelled")
        ref.emit_invalid_arguments = stub("emit_invalid_arguments")
        ref.middleware_kwargs = stub("middleware_kwargs")
    return ref


def materialize_dispatch(j):
    if j is None:
        return None
    er = j.get("error_result")
    if isinstance(er, dict) and er.get("__callable__"):
        error_result = stub(er["__callable__"]) if isinstance(er["__callable__"], str) \
            else CallableDouble()
    elif er is None:
        error_result = None
    else:
        error_result = CallableDouble()
    return _SequentialDispatch(
        execute=materialize(j.get("execute")) if j.get("execute") is not None else None,
        spinner=materialize(j.get("spinner")),
        middleware_trace_arg=materialize(j.get("middleware_trace_arg")),
        error_result=error_result,
        error_log=j.get("error_log", ""),
        handles_keyboard_interrupt=j.get("handles_keyboard_interrupt", False),
        is_delegate=j.get("is_delegate", False),
        finish_spinner=j.get("finish_spinner", True),
        finish_in_finally=j.get("finish_in_finally", True),
        transform_applied=j.get("transform_applied", False),
    )


def materialize(j):
    if isinstance(j, dict):
        if "__attr__" in j:
            return types.SimpleNamespace(
                **{k: materialize(v) for k, v in j["__attr__"].items()})
        cb = j.get("__callable__")
        if cb is True:
            return CallableDouble()
        if isinstance(cb, str):
            return stub(cb)
        if j.get("__budget__"):
            return BudgetDouble()
        if j.get("__guardrails__"):
            return GuardrailsDouble()
        if j.get("__subdir__"):
            return SubdirHintsDouble()
        if j.get("__spinner__"):
            return SpinnerDouble()
        if j.get("__event__"):
            return EventDouble()
        if j.get("__thread__"):
            return ThreadDouble()
        if j.get("__lock__"):
            return LockDouble()
        if j.get("__set__"):
            return SetDouble(j.get("items"))
        t = j.get("_type")
        if t == "_ToolCallRef":
            return patch_ref_methods(_ToolCallRef(
                name=j.get("name", ""), args=materialize(j.get("args")) or {},
                task_id=j.get("task_id", ""), call_id=j.get("call_id", ""),
                trace=materialize(j.get("trace")) or []))
        if t == "_ManagedToolResult":
            return _ManagedToolResult(
                result=materialize(j.get("result")),
                args=materialize(j.get("args")) or {},
                middleware_trace=materialize(j.get("middleware_trace")) or [],
                blocked=j.get("blocked", False), dispatched=j.get("dispatched", False),
                data_use=frozenset(j.get("data_use") or []))
        if t == "_ParsedCall":
            pc = _ParsedCall(
                tool_call=materialize(j.get("tool_call")), name=j.get("name", ""),
                args=materialize(j.get("args")) or {},
                middleware_trace=materialize(j.get("middleware_trace")) or [],
                parse_error=j.get("parse_error"), scope_block=j.get("scope_block"))
            pc.ref = stub("parsed_call_ref")
            return pc
        if t == "_SequentialDispatch":
            return materialize_dispatch(j)
        if t in ("_ToolTimeoutResult", "_ToolCancelledResult"):
            return NS[t](j.get("value", ""))
        return {k: materialize(v) for k, v in j.items()}
    if isinstance(j, list):
        return [materialize(v) for v in j]
    return j


# ── per-seam argument projection (identical field sets to the C args) ──

def _log_args(a, k):
    d = {"fmt": a[0], "args": [norm_ret(x) for x in a[1:]]}
    if "exc_info" in k:
        d["exc_info"] = k["exc_info"]
    return d


ARGMAP = {
    # module siblings / leaves
    "_tc_name": lambda a, k: {"tool_call": norm_ret(a[0])},
    "_pairing_tool_call_id": lambda a, k: {"tool_call": norm_ret(a[0])},
    "hook_id": lambda a, k: {"tool_call": norm_ret(a[0])},
    "interrupt_skip_wording": lambda a, k: {"agent": norm_ret(a[0])},
    "make_tool_result_message": lambda a, k: {
        "name": a[0], "result": norm_ret(a[1]), "tool_call_id": a[2],
        "effect_disposition": k.get("effect_disposition")},
    "_flush_session_db_after_tool_progress": lambda a, k: {
        "messages": norm_ret(a[1]), "stage": k.get("stage")},
    "emit_post": lambda a, k: dict(
        {"result": norm_ret(a[1]), "trace": k.get("trace")},
        **{kk: norm_ret(vv) for kk, vv in k.items() if kk != "trace"}),
    "emit_cancelled": lambda a, k: {"start_time": a[1]},
    "emit_invalid_arguments": lambda a, k: {"result": a[1]},
    "middleware_kwargs": lambda a, k: {},
    "parsed_call_ref": lambda a, k: {"task_id": a[0]},
    "_emit_terminal_post_tool_call": lambda a, k: dict(
        {"function_name": k.get("function_name"), "function_args": k.get("function_args"),
         "result": norm_ret(k.get("result")), "effective_task_id": k.get("effective_task_id"),
         "tool_call_id": k.get("tool_call_id"), "middleware_trace": k.get("middleware_trace")},
        **{kk: norm_ret(vv) for kk, vv in k.items()
           if kk not in ("function_name", "function_args", "result", "effective_task_id",
                         "tool_call_id", "middleware_trace")}),
    "extract_persisted_path": lambda a, k: {"candidate": norm_ret(a[0])},
    "guardrails.record_persisted_result": lambda a, k: {
        "tool_call_id": a[0], "path": a[1]},
    "_is_multimodal_tool_result": lambda a, k: {"result": norm_ret(a[0])},
    "observe_tool_outcome": lambda a, k: {"function_name": a[1], "is_error": a[2]},
    "observe_onboarding_result": lambda a, k: {
        "function_name": a[1], "function_args": a[2], "result": norm_ret(a[3])},
    "_append_guardrail_observation": lambda a, k: {
        "function_name": a[0], "function_args": a[1], "result": norm_ret(a[2]),
        "failed": k.get("failed"), "tool_call_id": k.get("tool_call_id")},
    "_record_file_mutation_result": lambda a, k: {
        "function_name": a[0], "function_args": a[1], "result": norm_ret(a[2]),
        "is_error": a[3], "task_id": k.get("task_id")},
    "_touch_activity": lambda a, k: {"label": a[0]},
    "error_preview": lambda a, k: {"result": norm_ret(a[0])},
    "verbose_text": lambda a, k: {"result": norm_ret(a[0])},
    "get_active_env": lambda a, k: {"task_id": a[0]},
    "_persist_multimodal_text_parts": lambda a, k: {
        "result": norm_ret(a[0]), "tool_name": a[1], "tool_call_id": a[2],
        "env": norm_ret(a[3]), "config": norm_ret(a[4])},
    "maybe_persist_tool_result": lambda a, k: {
        "content": norm_ret(k.get("content")), "tool_name": k.get("tool_name"),
        "tool_use_id": k.get("tool_use_id"), "env": norm_ret(k.get("env")),
        "config": norm_ret(k.get("config"))},
    "_record_persisted_path_for_stub": lambda a, k: {
        "tool_call_id": a[1], "result": norm_ret(a[2])},
    "note_tool_result": lambda a, k: {
        "function_name": a[1], "tool_call_id": a[2], "result": norm_ret(a[3]),
        "persisted_result": norm_ret(a[4])},
    "subdirectory_hints.check_tool_call": lambda a, k: {"name": a[0], "args": a[1]},
    "_append_subdir_hint_to_multimodal": lambda a, k: {
        "result": norm_ret(a[0]), "hints": norm_ret(a[1])},
    "_tool_result_content_for_active_model": lambda a, k: {
        "function_name": a[0], "result": norm_ret(a[1])},
    "lsp.forward_to_agent": lambda a, k: {},
    "plugin_live_notes.attach_to_tool_message": lambda a, k: {
        "tool_message": norm_ret(a[1])},
    "plugin_live_notes.keep_after_turn_budget": lambda a, k: {"batch": norm_ret(a[1])},
    "_redact_tool_args_for_display": lambda a, k: {"name": a[0], "args": a[1]},
    "prepare_metadata": lambda a, k: {
        "tool_call_id": a[0], "function_name": a[1], "display_args": norm_ret(a[2]),
        "result": norm_ret(a[3])},
    "stamp_data_use": lambda a, k: {"tool_message": norm_ret(a[0]), "data_use": a[1]},
    "_safe_callback": lambda a, k: {
        "callback": cb_proj(a[0]), "label": a[1],
        "args": [norm_ret(v) for v in a[2:]], "kwargs": dict(k)},
    "enforce_turn_budget": lambda a, k: {
        "batch": norm_ret(a[0]), "env": norm_ret(k.get("env")),
        "config": norm_ret(k.get("config"))},
    "sync_rewritten_tool_rows": lambda a, k: {
        "batch": norm_ret(a[1]), "contents_before": norm_ret(a[2])},
    "record_tool_batch": lambda a, k: {
        "batch": norm_ret(a[1]), "contents_before": norm_ret(a[2])},
    "_apply_pending_steer_to_tool_results": lambda a, k: {
        "messages": norm_ret(a[0]), "num_tools": a[1]},
    "print": lambda a, k: {"text": a[0]},
    "_wrap_verbose": lambda a, k: {"prefix": a[0], "result": norm_ret(a[1])},
    "_preview": lambda a, k: {"text": a[0], "limit": a[1]},
    "_unfinished_tool_result": lambda a, k: {
        "ref": norm_ret(a[1]), "timed_out": k.get("timed_out"),
        "timeout_s": k.get("timeout_s")},
    "_commit_tool_result": lambda a, k: {
        "ref": norm_ret(a[2]), "result": norm_ret(a[3]),
        "budget": norm_ret(k.get("budget")), "tool_duration": k.get("tool_duration"),
        "is_error": norm_ret(k.get("is_error")), "blocked": k.get("blocked"),
        "effect_disposition": k.get("effect_disposition"),
        "observed": k.get("observed", False), "data_use": norm_ret(k.get("data_use")),
        "success_log_chars": k.get("success_log_chars")},
    "_should_emit_quiet_tool_messages": lambda a, k: {},
    "_should_start_quiet_spinner": lambda a, k: {},
    "_get_cute_tool_message_impl": lambda a, k: {
        "name": a[0], "args": a[1], "tool_duration": a[2],
        "result": norm_ret(k.get("result"))},
    "_safe_print": lambda a, k: {"text": a[0]},
    "_vprint": lambda a, k: {"text": a[0], "force": k.get("force")},
    "_tool_progress_enabled": lambda a, k: {},
    "_multimodal_text_summary": lambda a, k: {"result": norm_ret(a[0])},
    "_print_tool_completed": lambda a, k: {
        "index": a[1], "tool_duration": a[2], "result": norm_ret(a[3])},
    "_emit_tool_complete_and_risk": lambda a, k: {
        "ref": norm_ret(a[1]), "result": norm_ret(a[2]),
        "risk_metadata": norm_ret(a[3]), "blocked": a[4]},
    "kawaii.get_waiting_faces": lambda a, k: {},
    "random.choice": lambda a, k: {"seq": norm_ret(a[0])},
    "_tool_row_emoji": lambda a, k: {"name": a[0], "args": a[1]},
    "_build_tool_label": lambda a, k: {"name": a[0], "args": a[1]},
    "kawaii.ctor": lambda a, k: {
        "text": a[0], "spinner_type": k.get("spinner_type"),
        "print_fn": cb_proj(k.get("print_fn"))},
    "spinner.start": lambda a, k: {},
    "spinner.stop": lambda a, k: {"text": a[0]},
    "_delegate_spinner_label": lambda a, k: {"function_args": a[0]},
    "_start_quiet_tool_spinner": lambda a, k: {
        "name": a[1], "args": a[2], "gate": k.get("gate", True),
        "label": k.get("label")},
    "_finish_quiet_tool_spinner": lambda a, k: {
        "spinner": norm_ret(a[1]), "name": a[2], "args": a[3],
        "tool_duration": a[4], "result": norm_ret(a[5])},
    "inline_tool_context.ctor": lambda a, k: {
        "effective_task_id": k.get("effective_task_id"),
        "tool_call_id": k.get("tool_call_id"), "messages": norm_ret(k.get("messages"))},
    "is_context_engine_tool": lambda a, k: {"function_name": a[1]},
    "is_memory_provider_tool": lambda a, k: {"function_name": a[1]},
    "_append_skipped_tool_results": lambda a, k: dict(
        {"remaining": norm_ret(a[2]), "effective_task_id": a[3]},
        **{kk: (cb_proj(vv) if kk == "hook_id" else norm_ret(vv))
           for kk, vv in k.items()}),
    "_skip_remaining_sequential": lambda a, k: dict(
        {"remaining": norm_ret(a[2]), "effective_task_id": a[3]},
        **{kk: (cb_proj(vv) if kk == "hook_id" else norm_ret(vv))
           for kk, vv in k.items()}),
    "_append_invalid_arguments_result": lambda a, k: {
        "ref": norm_ret(a[2]), "parse_error": a[3]},
    "_run_sequential_tool_execution_middleware": lambda a, k: {
        "function_name": k.get("function_name"), "function_args": k.get("function_args"),
        "effective_task_id": k.get("effective_task_id"),
        "tool_call_id": k.get("tool_call_id"),
        "middleware_trace": k.get("middleware_trace"),
        "execute": cb_proj(k.get("execute")), "scope_block": k.get("scope_block"),
        "display_index": k.get("display_index")},
    "dispatch_error_result": lambda a, k: {"error": norm_ret(a[0])},
    "agent.interrupt": lambda a, k: {"reason": a[0], "tool_reason": k.get("tool_reason")},
    "interrupt": lambda a, k: {"reason": a[0], "tool_reason": k.get("tool_reason")},
    "logger.debug": _log_args,
    "logger.info": _log_args,
    "logger.warning": _log_args,
    "logger.error": _log_args,
    "logging.debug": _log_args,
    "time.time": lambda a, k: {},
    "apply_transform_tool_result": lambda a, k: {
        "function_name": k.get("function_name"), "function_args": k.get("function_args"),
        "result": norm_ret(k.get("result")),
        "effective_task_id": k.get("effective_task_id"),
        "tool_call_id": k.get("tool_call_id"), "duration_ms": k.get("duration_ms")},
    "_detect_tool_failure": lambda a, k: {"name": a[0], "result": norm_ret(a[1])},
    "mark_batch_outcome": lambda a, k: {"failed": a[0]},
    "_resolve_sequential_dispatch": lambda a, k: {
        "ref": norm_ret(a[1]), "messages": norm_ret(a[2])},
    "_run_sequential_call": lambda a, k: {
        "dispatch": norm_ret(a[1]), "ref": norm_ret(a[2]),
        "scope_block": k.get("scope_block"),
        "remaining": norm_ret(k.get("remaining_calls")),
        "display_index": k.get("display_index"),
        "tool_start_time": k.get("tool_start_time")},
    "_publish_sequential_result": lambda a, k: {
        "ref": norm_ret(a[2]), "managed": norm_ret(a[3]),
        "tool_duration": k.get("tool_duration"), "index": k.get("index"),
        "budget": norm_ret(k.get("budget")),
        "transform_applied": k.get("transform_applied")},
    "_parse_tool_call": lambda a, k: {
        "tool_call": norm_ret(a[1]), "flatten_probe": k.get("flatten_probe", False)},
    "_budget_for_agent": lambda a, k: {},
    "_finalize_tool_batch": lambda a, k: {
        "messages": norm_ret(a[1]), "effective_task_id": a[2], "num_tools": a[3],
        "budget": norm_ret(a[4])},
    "set_activity_callback": lambda a, k: {"callback": cb_proj(a[0])},
    "is_thread_interrupted": lambda a, k: {"tid": a[0]},
    "event.wait": lambda a, k: {"timeout": a[0]},
    "event.set": lambda a, k: {},
    "current_thread_ident": lambda a, k: {},
    "threading.Event": lambda a, k: {},
    "threading.Thread": lambda a, k: {
        "target": cb_proj(k.get("target")),
        "args": [norm_ret(x) for x in (k.get("args") or ())],
        "kwargs": {kk: norm_ret(vv) for kk, vv in (k.get("kwargs") or {}).items()},
        "daemon": k.get("daemon"), "name": k.get("name")},
    "thread.start": lambda a, k: {},
    "thread.join": lambda a, k: {"timeout": k.get("timeout")},
    "fn": lambda a, k: {},
    "lock.__enter__": lambda a, k: {},
    "lock.__exit__": lambda a, k: {},
    "worker_threads.add": lambda a, k: {"tid": a[0]},
    "worker_threads.discard": lambda a, k: {"tid": a[0]},
    "_ra": lambda a, k: {},
    "_set_interrupt": lambda a, k: {"value": a[0], "tid": a[1], "reason": k.get("reason")},
    "body": lambda a, k: {"tid": a[0]},
    # import seams (args {})
    "import_observe_tool_outcome": lambda a, k: {},
    "import_legacy_tool_aliases": lambda a, k: {},
    "import_arg_validation": lambda a, k: {},
    "import_registry": lambda a, k: {},
    "import_mark_batch_outcome": lambda a, k: {},
    "import_set_activity_callback": lambda a, k: {},
    "import_is_thread_interrupted": lambda a, k: {},
}


def materialize_result(name, r):
    if name == "_ra":
        return types.SimpleNamespace(_set_interrupt=stub("_set_interrupt"))
    if name == "_parse_tool_call":
        return materialize(r)   # _ParsedCall with .ref seamed
    if name == "parsed_call_ref":
        return materialize(r)   # _ToolCallRef with methods seamed
    if name == "_run_sequential_tool_execution_middleware":
        return materialize(r)
    if name == "_resolve_sequential_dispatch":
        return materialize_dispatch(r) if isinstance(r, dict) else r
    if name == "_budget_for_agent":
        return materialize(r)
    if name == "kawaii.ctor":
        return SpinnerDouble()
    if name == "threading.Event":
        return EventDouble()
    if name == "threading.Thread":
        return ThreadDouble()
    if name == "inline_tool_context.ctor":
        return types.SimpleNamespace()
    return r


def stub(name):
    def f(*a, **k):
        proj = ARGMAP.get(name, lambda a, k: {})(a, k)
        args = {kk: norm_ret(vv) for kk, vv in proj.items() if vv is not None}
        r = script_next(name)
        # snapshot ret at call time: live code may mutate the returned object
        # later (e.g. tool_message["display_metadata"]), and the C harness
        # records the script as it was at the call.
        rec({"ev": name, "args": args, "ret": copy.deepcopy(r)})
        if isinstance(r, dict) and "__raise__" in r:
            raise make_exc(r)
        return materialize_result(name, r)
    f.__seam_name__ = name
    return f


# ── extraction ──

def compile_node(node, path):
    return compile(ast.Module(body=[node], type_ignores=[]), path, "exec",
                   flags=__future__.annotations.compiler_flag)


def extract_assign(path, name):
    tree = ast.parse(open(path, encoding="utf-8").read())
    ns = {}
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == name for t in node.targets):
            exec(compile_node(node, path), ns)
            return ns[name]
    raise RuntimeError("Assign not found: %s in %s" % (name, path))


def extract():
    global _ToolCallRef, _ManagedToolResult, _ParsedCall, _SequentialDispatch
    global _ToolTimeoutResult, _ToolCancelledResult, NS
    modname = "tool_result_processing_flow_extracted"
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    NS = ns
    logger = LoggerDouble()
    ns.update({
        "__name__": modname,
        "json": json, "contextlib": contextlib, "copy": copy,
        "dataclass": dataclass, "Any": Any, "Optional": Optional, "Callable": Callable,
        "threading": types.SimpleNamespace(
            Lock=object,
            current_thread=lambda: types.SimpleNamespace(
                ident=stub("current_thread_ident")()),
            Event=lambda: _ctor_seam("threading.Event", EventDouble),
            Thread=lambda **kw: _thread_ctor(kw)),
        "time": types.SimpleNamespace(time=stub("time.time")),
        "random": types.SimpleNamespace(choice=stub("random.choice")),
        "print": stub("print"),
        "logging": types.SimpleNamespace(debug=stub("logging.debug"),
                                         getLogger=lambda name: logger),
        "logger": logger,
        "KawaiiSpinner": _KawaiiFactory(),
        "InlineToolContext": _InlineCtxFactory(),
        "plugin_live_notes": types.SimpleNamespace(
            attach_to_tool_message=stub("plugin_live_notes.attach_to_tool_message"),
            keep_after_turn_budget=stub("plugin_live_notes.keep_after_turn_budget")),
        "lsp_deferred": types.SimpleNamespace(
            forward_to_agent=stub("lsp.forward_to_agent")),
        "coalesce_tool_call_id": stub("_pairing_tool_call_id"),
        "emit_terminal_post_tool_call": stub("_emit_terminal_post_tool_call"),
        "_REASON_USER_INTERRUPT": extract_assign(INTERRUPT_SRC, "_REASON_USER_INTERRUPT"),
        "interrupt_skip_wording": stub("interrupt_skip_wording"),
        "observe_onboarding_result": stub("observe_onboarding_result"),
        "conversation_id": stub("conversation_id"),
        "apply_transform_tool_result": stub("apply_transform_tool_result"),
        "is_context_engine_tool": stub("is_context_engine_tool"),
        "is_memory_provider_tool": stub("is_memory_provider_tool"),
        "stamp_data_use": stub("stamp_data_use"),
        "sync_rewritten_tool_rows": stub("sync_rewritten_tool_rows"),
        "_is_multimodal_tool_result": stub("_is_multimodal_tool_result"),
        "_multimodal_text_summary": stub("_multimodal_text_summary"),
        "get_active_env": stub("get_active_env"),
        "maybe_persist_tool_result": stub("maybe_persist_tool_result"),
        "enforce_turn_budget": stub("enforce_turn_budget"),
        "extract_persisted_path": stub("extract_persisted_path"),
        "note_tool_result": stub("note_tool_result"),
        "record_tool_batch": stub("record_tool_batch"),
        "_append_subdir_hint_to_multimodal": stub("_append_subdir_hint_to_multimodal"),
        "_redact_tool_args_for_display": stub("_redact_tool_args_for_display"),
        "_get_cute_tool_message_impl": stub("_get_cute_tool_message_impl"),
        "_tool_row_emoji": stub("_tool_row_emoji"),
        "_build_tool_label": stub("_build_tool_label"),
        "_detect_tool_failure": stub("_detect_tool_failure"),
        "make_tool_result_message": stub("make_tool_result_message"),
        "INLINE_TOOL_EXECUTORS": {},
    })

    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # Module constants: Assign AND AnnAssign, plain-Name targets only.
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(
                isinstance(t, ast.Name) for t in node.targets):
            try:
                exec(compile_node(node, SRC), ns)
            except Exception:
                pass
        elif (isinstance(node, ast.AnnAssign) and node.value is not None
              and isinstance(node.target, ast.Name)):
            try:
                exec(compile_node(node, SRC), ns)
            except Exception:
                pass
    ns["logger"] = logger   # restore after the logger Assign seeding

    real = {}
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in (
                "_ToolCallRef", "_ManagedToolResult", "_ParsedCall",
                "_SequentialDispatch", "_ToolTimeoutResult", "_ToolCancelledResult"):
            exec(compile_node(node, SRC), ns)
            if node.name in ("_ToolCallRef", "_ParsedCall"):
                for sub in node.body:
                    if isinstance(sub, ast.FunctionDef) and sub.name in METHOD_TARGETS:
                        if node.name == "_ParsedCall" and sub.name != "ref":
                            continue
                        if node.name == "_ToolCallRef" and sub.name == "ref":
                            continue
                        sub = copy.copy(sub)
                        sub.decorator_list = []
                        exec(compile_node(sub, SRC), ns)
                        real[sub.name] = ns[sub.name]
        elif isinstance(node, ast.FunctionDef) and node.name in MODULE_TARGETS:
            exec(compile_node(node, SRC), ns)
            real[node.name] = ns[node.name]
    _ToolCallRef = ns["_ToolCallRef"]
    _ManagedToolResult = ns["_ManagedToolResult"]
    _ParsedCall = ns["_ParsedCall"]
    _SequentialDispatch = ns["_SequentialDispatch"]
    _ToolTimeoutResult = ns["_ToolTimeoutResult"]
    _ToolCancelledResult = ns["_ToolCancelledResult"]
    missing = WANTED - set(real)
    if missing:
        raise RuntimeError("extraction missed: %s" % sorted(missing))
    for const in ("_LARGE_TOOL_RESULT_TRIM_CHARS", "_TOOL_ACTIVITY_HEARTBEAT_INTERVAL_S",
                  "_REASON_USER_INTERRUPT"):
        if const not in ns:
            raise RuntimeError("constant not seeded: %s" % const)
    return ns, real


def _ctor_seam(name, factory):
    r = script_next(name)
    rec({"ev": name, "args": {}, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r)
    return factory()


def _thread_ctor(kw):
    proj = ARGMAP["threading.Thread"]((), kw)
    args = {kk: norm_ret(vv) for kk, vv in proj.items() if vv is not None}
    r = script_next("threading.Thread")
    rec({"ev": "threading.Thread", "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r)
    return ThreadDouble()


class _KawaiiFactory:
    def get_waiting_faces(self):
        return stub("kawaii.get_waiting_faces")()

    def __call__(self, text, *, spinner_type="dots", print_fn=None):
        return stub("kawaii.ctor")(text, spinner_type=spinner_type, print_fn=print_fn)


class _InlineCtxFactory:
    def __call__(self, **kw):
        return stub("inline_tool_context.ctor")(**kw)


def modstub(fullname):
    parts = fullname.split(".")
    for i in range(1, len(parts) + 1):
        nm = ".".join(parts[:i])
        m = sys.modules.get(nm)
        if m is None:
            m = types.ModuleType(nm)
            m.__path__ = []
            sys.modules[nm] = m
        if i > 1:
            setattr(sys.modules[".".join(parts[:i - 1])], parts[i - 1], m)
    return sys.modules[fullname]


# `from X import Y` does TWO getattrs (the _handle_fromlist hasattr probe and
# IMPORT_FROM); cache the produced attribute after the first so the import
# seam records exactly once. setup_case clears the cache per case.
_IMPORT_SEAM_CACHE = []


def import_seam_module(fullname, imports):
    m = modstub(fullname)

    def getattr_(attr):
        if attr in imports:
            seam_name, factory = imports[attr]
            r = script_next(seam_name)
            rec({"ev": seam_name, "args": {}, "ret": r})
            if isinstance(r, dict) and "__raise__" in r:
                raise make_exc(r)
            value = factory(r)
            m.__dict__[attr] = value
            return value
        raise AttributeError(attr)

    m.__dict__["__getattr__"] = getattr_
    _IMPORT_SEAM_CACHE.append((m, list(imports)))
    return m


def install_module_stubs():
    import_seam_module("hermes_cli.observability.shared_metrics_harness", {
        "observe_tool_outcome": ("import_observe_tool_outcome",
                                 lambda r: stub("observe_tool_outcome"))})
    # model_tools._LEGACY_TOOL_ALIASES: the scripted dict IS the aliases table.
    import_seam_module("model_tools", {
        "_LEGACY_TOOL_ALIASES": ("import_legacy_tool_aliases",
                                 lambda r: materialize(r) if isinstance(r, dict) else {})})
    import_seam_module("tools.arg_validation", {
        "call_with_arg_checks": ("import_arg_validation",
                                 lambda r: stub("call_with_arg_checks"))})
    import_seam_module("tools.registry", {
        "registry": ("import_registry",
                     lambda r: types.SimpleNamespace(get_entry=stub("registry.get_entry")))})
    import_seam_module("agent.terminal_approval_batch", {
        "mark_batch_outcome": ("import_mark_batch_outcome",
                               lambda r: stub("mark_batch_outcome"))})
    import_seam_module("tools.environments.base", {
        "set_activity_callback": ("import_set_activity_callback",
                                  lambda r: stub("set_activity_callback"))})
    import_seam_module("tools.interrupt", {
        "is_thread_interrupted": ("import_is_thread_interrupted",
                                  lambda r: stub("is_thread_interrupted"))})


# ── per-case setup ──

def setup_case(ns, real, inp, target):
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["counts"] = {}
    for m, attrs in _IMPORT_SEAM_CACHE:
        for a in attrs:
            m.__dict__.pop(a, None)
    for name in MODULE_TARGETS | MODULE_SEAMS:
        if name == target:
            ns[name] = real[name]
        else:
            ns[name] = stub(name)
    for name in METHOD_TARGETS:
        if name == target:
            ns[name] = real[name]
        else:
            ns[name] = stub(name)
    ns["_ToolCallRef"] = _ToolCallRef
    ns["INLINE_TOOL_EXECUTORS"] = {}
    if target == "_append_skipped_tool_results":
        # inline _ToolCallRef(...).emit_post: the ctor stays real, the method
        # is seamed through a subclass.
        class SeamedRef(_ToolCallRef):
            def emit_post(self, agent, result, *, trace=None, **outcome):
                return stub("emit_post")(agent, result, trace=trace, **outcome)
        ns["_ToolCallRef"] = SeamedRef
    if target == "_resolve_sequential_dispatch":
        ns["INLINE_TOOL_EXECUTORS"] = materialize(inp.get("inline_executors") or {})


def attempt(fn):
    try:
        return fn(), None
    except BaseException as e:   # SystemExit / KeyboardInterrupt / GeneratorExit included
        return None, {"__raise__": type(e).__name__, "msg": str(e)}


def _state_objs(st):
    agent = materialize(st.get("agent"))
    messages = materialize(st.get("messages")) if "messages" in st else None
    ref = materialize(st.get("ref")) if "ref" in st else None
    return agent, messages, ref


def _state_out(agent, messages, ref, keys):
    out = {}
    if "agent" in keys:
        out["agent"] = norm_ret(agent)
    if "messages" in keys:
        out["messages"] = norm_ret(messages)
    if "ref" in keys:
        out["ref"] = norm_ret(ref)
    return out


def drive(ns, real, func, inp):
    """Returns (ret, state_dict, raise_spec)."""
    st = inp.get("state") or {}
    agent, messages, ref = _state_objs(st)

    if func == "_record_persisted_path_for_stub":
        ret, exc = attempt(lambda: ns[func](
            agent, inp.get("tool_call_id", ""), materialize(inp.get("function_result"))))
        return ret, _state_out(agent, messages, ref, ("agent",)), exc

    if func == "_canonical_tool_name":
        ret, exc = attempt(lambda: ns[func](inp.get("function_name", "")))
        return ret, {}, exc

    if func == "_append_skipped_tool_results":
        ret, exc = attempt(lambda: ns[func](
            agent, messages, materialize(inp.get("tool_calls") or []),
            inp.get("effective_task_id", ""),
            content=inp.get("content", ""),
            hook_error_type=inp.get("hook_error_type"),
            hook_id=materialize(inp.get("hook_id")),
            flush_stage=inp.get("flush_stage"),
            stop_on_flush_failure=inp.get("stop_on_flush_failure", True)))
        return ret, _state_out(agent, messages, ref, ("agent", "messages")), exc

    if func == "_commit_tool_result":
        r = materialize(inp.get("ref"))
        kw = dict(budget=materialize(inp.get("budget")),
                  tool_duration=inp.get("tool_duration", 0.0),
                  is_error=inp.get("is_error", False),
                  blocked=inp.get("blocked", False),
                  effect_disposition=inp.get("effect_disposition"),
                  observed=inp.get("observed", False),
                  data_use=frozenset(inp.get("data_use") or []),
                  success_log_chars=inp.get("success_log_chars"))
        if inp.get("error_preview") is not None:
            kw["error_preview"] = materialize(inp["error_preview"])
        if inp.get("verbose_text") is not None:
            kw["verbose_text"] = materialize(inp["verbose_text"])
        ret, exc = attempt(lambda: ns[func](
            agent, messages, r, materialize(inp.get("function_result")), **kw))
        return ret, _state_out(agent, messages, ref, ("agent", "messages")), exc

    if func == "_finalize_tool_batch":
        ret, exc = attempt(lambda: ns[func](
            agent, messages, inp.get("effective_task_id", ""),
            inp.get("num_tools", 0), materialize(inp.get("budget"))))
        return ret, _state_out(agent, messages, ref, ("agent", "messages")), exc

    if func == "_print_tool_completed":
        ret, exc = attempt(lambda: ns[func](
            agent, inp.get("index", 0), inp.get("tool_duration", 0.0),
            materialize(inp.get("result"))))
        return ret, _state_out(agent, messages, ref, ("agent",)), exc

    if func == "_append_batch_results":
        b = (inp.get("batch") or {}).get("__attr__", inp.get("batch") or {})
        pcs = [materialize(pc) for pc in (b.get("parsed_calls") or [])]
        results = []
        for rj in (b.get("results") or []):
            if rj is None:
                results.append(None)
                continue
            rn = materialize(rj)
            if isinstance(getattr(rn, "ref", None), _ToolCallRef):
                patch_ref_methods(rn.ref)
            if hasattr(rn, "data_use"):
                rn.data_use = frozenset(rn.data_use or [])
            results.append(rn)
        batch = types.SimpleNamespace(
            parsed_calls=pcs, results=results,
            timed_out_indices=b.get("timed_out_indices") or [],
            timeout_s=b.get("timeout_s"))
        ret, exc = attempt(lambda: ns[func](
            agent, messages, inp.get("effective_task_id", ""), batch,
            materialize(inp.get("budget"))))
        return ret, _state_out(agent, messages, ref, ("agent", "messages")), exc

    if func == "_start_quiet_tool_spinner":
        kw = {}
        if "gate" in inp:
            kw["gate"] = inp["gate"]
        if "label" in inp:
            kw["label"] = inp["label"]
        ret, exc = attempt(lambda: ns[func](
            agent, inp.get("function_name", ""), inp.get("function_args") or {}, **kw))
        return ret, _state_out(agent, messages, ref, ("agent",)), exc

    if func == "_finish_quiet_tool_spinner":
        ret, exc = attempt(lambda: ns[func](
            agent, materialize(inp.get("spinner")), inp.get("function_name", ""),
            inp.get("function_args") or {}, inp.get("tool_duration", 0.0),
            materialize(inp.get("result"))))
        return ret, _state_out(agent, messages, ref, ("agent",)), exc

    if func == "_resolve_sequential_dispatch":
        r = materialize(inp.get("ref"))
        ret, exc = attempt(lambda: ns[func](agent, r, messages))
        if exc is None:
            ret = norm_dispatch(ret, inp.get("ref", {}).get("name", ""))
        return ret, _state_out(agent, messages, ref, ("agent", "messages")), exc

    if func == "_skip_remaining_sequential":
        sk = dict(inp.get("skip_kwargs") or {})
        if "hook_id" in sk:
            sk["hook_id"] = materialize(sk["hook_id"])
        ret, exc = attempt(lambda: ns[func](
            agent, messages, materialize(inp.get("remaining") or []),
            inp.get("effective_task_id", ""),
            notice=inp.get("notice", ""), **sk))
        return ret, _state_out(agent, messages, ref, ("agent", "messages")), exc

    if func == "_append_invalid_arguments_result":
        r = materialize(inp.get("ref"))
        ret, exc = attempt(lambda: ns[func](agent, messages, r, inp.get("parse_error", "")))
        return ret, _state_out(agent, messages, ref, ("agent", "messages")), exc

    if func == "_run_sequential_call":
        dispatch = materialize_dispatch(inp.get("dispatch"))
        ret, exc = attempt(lambda: ns[func](
            agent, dispatch, ref,
            scope_block=inp.get("scope_block"), messages=messages,
            remaining_calls=materialize(inp.get("remaining_calls") or []),
            display_index=inp.get("display_index", 0),
            tool_start_time=inp.get("tool_start_time", 0.0)))
        return ret, _state_out(agent, messages, ref, ("agent", "ref", "messages")), exc

    if func == "_publish_sequential_result":
        managed = materialize(inp.get("managed"))
        ret, exc = attempt(lambda: ns[func](
            agent, messages, ref, managed,
            tool_duration=inp.get("tool_duration", 0.0), index=inp.get("index", 0),
            budget=materialize(inp.get("budget")),
            transform_applied=inp.get("transform_applied", False)))
        return ret, _state_out(agent, messages, ref, ("agent", "ref", "messages")), exc

    if func == "_execute_tool_calls_sequential":
        am = materialize(inp.get("assistant_message"))
        ret, exc = attempt(lambda: ns[func](
            agent, am, messages, inp.get("effective_task_id", ""),
            inp.get("api_call_count", 0), finalize=inp.get("finalize", True)))
        return ret, _state_out(agent, messages, ref, ("agent", "messages")), exc

    if func == "middleware_kwargs":
        r = materialize(inp.get("ref"))
        m = types.MethodType(real["middleware_kwargs"], r)
        ret, exc = attempt(lambda: m())
        return ret, {}, exc

    if func == "emit_post":
        r = _ToolCallRef(
            name=(inp.get("ref") or {}).get("name", ""),
            args=materialize((inp.get("ref") or {}).get("args")) or {},
            task_id=(inp.get("ref") or {}).get("task_id", ""),
            call_id=(inp.get("ref") or {}).get("call_id", ""),
            trace=materialize((inp.get("ref") or {}).get("trace")) or [])
        ag = materialize(inp.get("agent"))
        kw = {}
        if "trace" in inp:
            kw["trace"] = inp["trace"]
        kw.update(inp.get("outcome") or {})
        m = types.MethodType(real["emit_post"], r)
        ret, exc = attempt(lambda: m(ag, materialize(inp.get("result")), **kw))
        return ret, {}, exc

    if func == "emit_cancelled":
        r = materialize(inp.get("ref"))
        ag = materialize(inp.get("agent"))
        m = types.MethodType(real["emit_cancelled"], r)
        ret, exc = attempt(lambda: m(ag, inp.get("start_time", 0.0)))
        return ret, {}, exc

    if func == "emit_invalid_arguments":
        r = materialize(inp.get("ref"))
        ag = materialize(inp.get("agent"))
        m = types.MethodType(real["emit_invalid_arguments"], r)
        ret, exc = attempt(lambda: m(ag, inp.get("result", "")))
        return ret, {}, exc

    if func == "ref":
        pc = _ParsedCall(
            tool_call=materialize((inp.get("pc") or {}).get("tool_call")),
            name=(inp.get("pc") or {}).get("name", ""),
            args=materialize((inp.get("pc") or {}).get("args")) or {},
            middleware_trace=materialize((inp.get("pc") or {}).get("middleware_trace")) or [],
            parse_error=(inp.get("pc") or {}).get("parse_error"),
            scope_block=(inp.get("pc") or {}).get("scope_block"))
        m = types.MethodType(real["ref"], pc)
        ret, exc = attempt(lambda: m(inp.get("task_id", "")))
        return ret, {}, exc

    if func == "_set_worker_activity_callback":
        ret, exc = attempt(lambda: ns[func](agent))
        return ret, _state_out(agent, messages, ref, ("agent",)), exc

    if func == "_run_tool_activity_heartbeat":
        stop_event = materialize(inp.get("stop_event"))
        ret, exc = attempt(lambda: ns[func](
            agent, stop_event, inp.get("label", ""),
            interval=inp.get("interval", 30.0), worker_tid=inp.get("worker_tid")))
        return ret, _state_out(agent, messages, ref, ("agent",)), exc

    if func == "_registered_tool_worker":
        def so():
            return _state_out(agent, messages, ref, ("agent",))
        try:
            cm = ns[func](agent)
            tid = cm.__enter__()
        except BaseException as e:
            return None, so(), {"__raise__": type(e).__name__, "msg": str(e)}
        body_r = script_next("body")
        rec({"ev": "body", "args": {} if tid is None else {"tid": norm_ret(tid)},
             "ret": body_r})
        if isinstance(body_r, dict) and "__raise__" in body_r:
            e = make_exc(body_r)
            try:
                cm.__exit__(type(e), e, None)
            except BaseException as e2:
                return None, so(), {"__raise__": type(e2).__name__, "msg": str(e2)}
            return None, so(), {"__raise__": type(e).__name__, "msg": str(e)}
        try:
            cm.__exit__(None, None, None)
        except BaseException as e:
            return None, so(), {"__raise__": type(e).__name__, "msg": str(e)}
        return tid, so(), None

    if func == "_run_with_activity_heartbeat":
        fn = materialize(inp.get("fn"))
        ret, exc = attempt(lambda: ns[func](agent, inp.get("function_name", ""), fn))
        return ret, _state_out(agent, messages, ref, ("agent",)), exc

    raise ValueError("unknown func: %s" % func)


def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()),
                              key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def main():
    try:
        ns, real = extract()
        install_module_stubs()
    except BaseException as e:
        print("PYERR extract: %s: %s" % (type(e).__name__, e))
        return 1
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        setup_case(ns, real, inp, func)
        try:
            ret, state, exc = drive(ns, real, func, inp)
        except BaseException as e:
            print("PYERR %s: %s: %s" % (func, type(e).__name__, e))
            mism += 1
            continue
        want = {"ret": norm_ret(ret), "raise": exc, "state": norm_ret(state),
                "trace": CUR["trace"]}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE tool_result_processing_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())