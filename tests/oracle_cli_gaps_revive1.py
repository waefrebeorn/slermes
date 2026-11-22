#!/usr/bin/env python3
"""Differential oracle for the cli_gaps.c revival group 1 ports.

Reads the harness JSONL on stdin ({func,in,out}), replays the SAME scripted
seam results through the LIVE quarry Python (AST-extracted defs), and compares
ordered trace + ret + state + raise. Fail-closed; catches BaseException per
case (SystemExit included). Summary: ORACLE cli_gaps_revive1: N cases, M mismatches.

Gate:
  bash tests/run_one_oracle.sh cli_gaps_revive1 oracle_cli_gaps_revive1.py "" \
       "src/cli/cli_gaps.o lib/libjson/json.o"
"""
import ast
import asyncio
import contextlib
import contextvars
import dataclasses
import enum
import json
import logging
import os
import queue
import re
import sys
import threading
import time
import types
import uuid
from collections import OrderedDict, deque
from functools import partial
from pathlib import Path
from typing import Any, Callable, Deque, Dict, Hashable, Iterable, Iterator, List, Optional, Tuple

_REPO = Path(__file__).resolve().parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
os.environ.setdefault("HOME", str(_REPO.parent.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home_cg1"))

import __future__
_ANN_FLAG = __future__.annotations.compiler_flag
SN = types.SimpleNamespace
MISSING = object()

# func -> (module, live fn name, is_method)
TARGETS = {
    "_is_pausable_gateway": ("hermes_cli/_scan_venv_blockers.py", "_is_pausable_gateway", False),
    "probe_port": ("hermes_cli/local_runtime/detect.py", "probe_port", False),
    "pairing_command": ("hermes_cli/pairing.py", "pairing_command", False),
    "extract_compress_flags": ("hermes_cli/partial_compress.py", "extract_compress_flags", False),
    "summarize_compress_preview": ("hermes_cli/partial_compress.py", "summarize_compress_preview", False),
    "is_valid": ("tools/browser_supervisor_capture.py", "is_valid", True),
    "_invalid": ("tools/browser_supervisor_capture.py", "_invalid", True),
    "_own_launcher": ("tools/bot_desktop/idle_watch.py", "_own_launcher", False),
    "kanban_toolset_requested": ("tools/kanban_toolset_context.py", "kanban_toolset_requested", False),
    "publish": ("gateway/session_control_actions.py", "publish", False),
    "set_accounting_context": ("agent/aux_accounting.py", "set_accounting_context", False),
    "reset_accounting_context": ("agent/aux_accounting.py", "reset_accounting_context", False),
    "error_class": ("agent/auxiliary_call_outcome.py", "error_class", False),
    "record_compression_regions": ("agent/context_compressor_telemetry.py", "_record_compression_regions", True),
    "note_aux_route_fallback": ("agent/context_compressor_telemetry.py", "_note_aux_route_fallback", True),
    "iter_configured_targets": ("agent/outbound_webhooks.py", "iter_configured_targets", False),
    "redirect_request": ("agent/outbound_webhooks.py", "redirect_request", True),
    "cache_path_base": ("agent/terminal_env_provider.py", "cache_path_base", True),
    "default_cwd": ("agent/terminal_env_provider.py", "default_cwd", True),
    "check_requirements": ("agent/terminal_env_provider.py", "check_requirements", True),
    "setup_instructions": ("agent/terminal_env_provider.py", "setup_instructions", True),
    "post_setup": ("agent/terminal_env_provider.py", "post_setup", True),
    "doctor_checks": ("agent/terminal_env_provider.py", "doctor_checks", True),
    "blocked": ("gateway/bot_loop_guard.py", "blocked", True),
    "live_dm_allow_from": ("gateway/platforms/access_policy_mixin.py", "_live_dm_allow_from", True),
    "checkout": ("gateway/platforms/api_server_memory_sessions.py", "checkout", True),
    "close_all": ("gateway/platforms/api_server_memory_sessions.py", "close_all", True),
    "_timer": ("gateway/platforms/webhook_coalesce.py", "_timer", True),
    "_bearer": ("gateway/relay/media.py", "_bearer", True),
}


def canon(x):
    if isinstance(x, bool):
        return x
    if isinstance(x, float):
        return int(x) if x.is_integer() else x
    if isinstance(x, dict):
        return {k: canon(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [canon(v) for v in x]
    if isinstance(x, Path):
        return x.as_posix()
    return x


def seed_basics(ns):
    ns.update(dict(Any=Any, Callable=Callable, Deque=Deque, Dict=Dict, Hashable=Hashable,
                   Iterable=Iterable, Iterator=Iterator, List=List, Optional=Optional, Tuple=Tuple,
                   dataclass=dataclasses.dataclass, field=dataclasses.field, enum=enum,
                   logging=logging, threading=threading, time=time, asyncio=asyncio,
                   contextlib=contextlib, queue=queue, os=os, sys=sys, re=re, json=json,
                   uuid=uuid, math=__import__("math"), Path=Path, OrderedDict=OrderedDict,
                   deque=deque, partial=partial, ContextVar=contextvars.ContextVar,
                   nullcontext=contextlib.nullcontext, suppress=contextlib.suppress))


def extract(module_rel):
    src = (QUARRY / module_rel).read_text(encoding="utf-8")
    tree = ast.parse(src)
    name = "cg1live_" + re.sub(r"\W", "_", module_rel)
    mod = types.ModuleType(name)
    mod.__file__ = str(QUARRY / module_rel)
    sys.modules[name] = mod
    ns = mod.__dict__
    seed_basics(ns)
    flags = _ANN_FLAG

    def ex(node):
        try:
            exec(compile(ast.Module(body=[node], type_ignores=[]), mod.__file__, "exec",
                         flags=flags, dont_inherit=True), ns)
        except Exception:
            pass

    for node in tree.body:
        if isinstance(node, ast.Assign):
            if all(isinstance(t, ast.Name) for t in node.targets):
                ex(node)
        elif isinstance(node, ast.AnnAssign):
            if isinstance(node.target, ast.Name) and node.value is not None:
                ex(node)
        elif isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            ex(node)
    # bare methods for method targets
    for node in tree.body:
        if isinstance(node, ast.ClassDef):
            for sub in node.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)):
                    # LIVE @property methods are invoked as plain functions by
                    # the C harness (it passes a fake self), so strip decorators.
                    sub.decorator_list = []
                    ex(sub)
    return ns


class SilentLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


def build_exc(kind, message, ns):
    builtin = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
               "GeneratorExit": GeneratorExit, "ValueError": ValueError, "TypeError": TypeError,
               "RuntimeError": RuntimeError, "AttributeError": AttributeError, "KeyError": KeyError,
               "ImportError": ImportError, "OSError": OSError, "TimeoutError": TimeoutError,
               "CancelledError": asyncio.CancelledError, "Exception": Exception}
    if kind in builtin:
        return builtin[kind](message)
    cls = ns.get(kind)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls(message)
    return type(kind, (Exception,), {})(message)


class Rig:
    """Per-case recording rig driven by the io scripts."""

    def __init__(self, io, ns):
        self.io = io or {}
        self.ns = ns
        self.trace = []
        self.counters = {}

    def script(self, name):
        if name not in self.io:
            return MISSING
        s = self.io[name]
        if isinstance(s, dict) and "__seq__" in s:
            seq = s["__seq__"]
            k = self.counters.get(name, 0)
            self.counters[name] = k + 1
            return seq[min(k, len(seq) - 1)] if seq else None
        return s

    def rec(self, name, args, materialize=lambda v: v):
        value = self.script(name)
        if value is MISSING:
            value = {"__raise__": "MissingScript", "message": name}
        self.trace.append({"ev": name, "args": canon(args), "ret": canon(value)})
        if isinstance(value, dict) and "__raise__" in value:
            raise build_exc(value["__raise__"], value.get("message", ""), self.ns)
        return materialize(value)

    def stub(self, name, params=(), materialize=lambda v: v):
        def f(*a, **k):
            args = dict(zip(params, a))
            args.update(k)
            return self.rec(name, args, materialize)
        return f


def logger_double(rig):
    lg = SN()
    lg.debug = lambda fmt, *a, exc_info=False: rig.rec(
        "logger_debug", {"fmt": fmt, "session_id": a[0] if a else None, "exc_info": exc_info})
    lg.warning = lambda fmt, *a, exc_info=False: rig.rec(
        "logger_warning", {"fmt": fmt, "type_name": a[0] if a else None})
    lg.info = lambda *a, **k: None
    return lg


def norm_ret(x):
    if dataclasses.is_dataclass(x) and not isinstance(x, type):
        return {f.name: canon(getattr(x, f.name)) for f in dataclasses.fields(x)}
    if isinstance(x, BaseException):
        return {"__exc__": type(x).__name__, "message": str(x)}
    return canon(x)


def stub_module(dotted, **attrs):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        dn = ".".join(parts[:i])
        if dn not in sys.modules or sys.modules[dn] is None:
            m = types.ModuleType(dn)
            m.__path__ = []
            sys.modules[dn] = m
    m = sys.modules[dotted]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


# ── per-function drivers: return (ret, state_after, raise_spec) ──

def d__is_pausable_gateway(rig, ns, inp, state):
    if inp.get("import_ok"):
        stub_module("gateway.status",
                    looks_like_gateway_command_line=rig.stub("looks_like_gateway_command_line", ("cmdline",)))
    else:
        sys.modules["gateway.status"] = None
        stub_module("gateway")
    ret = ns["_is_pausable_gateway"](inp.get("cmdline"))
    return norm_ret(ret), state, None


def d_probe_port(rig, ns, inp, state):
    ns["_get"] = lambda url, timeout_s=3: rig.rec("_get", {"url": url}, lambda v: (v["status"], v["body"]))
    ret = ns["probe_port"](inp["port"])
    return norm_ret(ret), state, None


def d_pairing_command(rig, ns, inp, state):
    stub_module("gateway.pairing", PairingStore=rig.stub("PairingStore"))
    ns["_cmd_list"] = rig.stub("_cmd_list", ("store",))
    ns["_cmd_approve"] = rig.stub("_cmd_approve", ("store", "platform", "code"))
    ns["_cmd_revoke"] = rig.stub("_cmd_revoke", ("store", "platform", "user_id"))
    ns["_cmd_clear_pending"] = rig.stub("_cmd_clear_pending", ("store",))
    ns["print"] = lambda text="", *a, **k: rig.rec("print", {"text": text})
    args = SN(**inp.get("args", {}))
    ret = ns["pairing_command"](args)
    return norm_ret(ret), state, None


def d_extract_compress_flags(rig, ns, inp, state):
    ret = ns["extract_compress_flags"](inp.get("raw_args"))
    return norm_ret(ret), state, None


def d_summarize_compress_preview(rig, ns, inp, state):
    ret = ns["summarize_compress_preview"](inp.get("history"), inp.get("partial"),
                                         inp.get("keep_last"), inp.get("focus_topic"),
                                         inp.get("approx_tokens"))
    return norm_ret(ret), state, None


def d_is_valid(rig, ns, inp, state):
    ws_objs = {}

    def ws(i):
        if i is None:
            return None
        return ws_objs.setdefault(i, object())

    sup = None
    if state.get("sup_id") is not None:
        sup = SN(_active=state.get("sup_active"), _stop_requested=state.get("sup_stop_requested"),
                 _ws=ws(state.get("sup_ws_id")))
    other = object()
    objs = {"sup": sup, "other": other}
    registry = SN(get=lambda task_id: rig.rec("registry_get", {"task_id": task_id}, lambda v: objs.get(v)))
    loop = SN(is_running=lambda: rig.rec("loop_is_running", {}))
    slf = SN(_supervisor=sup, _registry=registry, task_id=state.get("task_id"),
             _ws=ws(state.get("self_ws_id")), _loop=loop)
    ret = ns["is_valid"](slf)
    return norm_ret(ret), state, None


def d__invalid(rig, ns, inp, state):
    slf = SN(task_id=inp.get("task_id"))
    ret = ns["_invalid"](slf)
    return norm_ret(ret), state, None


def d__own_launcher(rig, ns, inp, state):
    rt = SN(_create_time=lambda pid: rig.rec("_create_time", {"pid": pid}))
    stub_module("tools.bot_desktop.runtime", _create_time=rt._create_time)
    ns["runtime"] = rt
    ret = ns["_own_launcher"](inp.get("argv"))
    return norm_ret(ret), state, None


def d_kanban_toolset_requested(rig, ns, inp, state):
    ns["_requested"] = SN(get=lambda: rig.rec("contextvar_get", {}))
    ret = ns["kanban_toolset_requested"]()
    return norm_ret(ret), state, None


def d_publish(rig, ns, inp, state):
    ns["AuthorityRefusal"] = type("AuthorityRefusal", (Exception,), {})
    ns["logger"] = logger_double(rig)
    live = SN(control_seq=state.get("control_seq", 0))
    ns["_read_control_snapshot"] = lambda authority, session_id: rig.rec(
        "_read_control_snapshot", {"session_id": session_id})

    def pub_snap(authority, live_, session_id, snapshot, ticket, only_if_present=False, only_if_changed=False):
        return rig.rec("_publish_snapshot", {"session_id": session_id, "snapshot": snapshot,
                                             "ticket": ticket, "only_if_present": only_if_present,
                                             "only_if_changed": only_if_changed})

    ns["_publish_snapshot"] = pub_snap
    authority = SN(sessions=SN(get=lambda sid: rig.rec("sessions_get", {"session_id": sid},
                                                         lambda v: live if v else None)))
    if not inp.get("authority_present"):
        authority = None
    state = dict(state)
    try:
        ret = ns["publish"](authority, inp.get("session_id"),
                            only_if_present=inp.get("only_if_present", False),
                            only_if_changed=inp.get("only_if_changed", False))
    except BaseException as ex:
        state["control_seq"] = live.control_seq
        return None, state, {"kind": type(ex).__name__, "message": str(ex)}
    state["control_seq"] = live.control_seq
    return norm_ret(ret), state, None


def d_set_accounting_context(rig, ns, inp, state):
    last = {}

    def setv(value):
        last["v"] = value
        return rig.rec("contextvar_set", {"value": value})

    ns["_accounting"] = SN(set=setv)
    stub_module("agent.agent_init_session",
                writer_interface=rig.stub("writer_interface", ("persistence", "session_db")))
    ret = ns["set_accounting_context"](inp.get("session_db"), inp.get("session_id"),
                                       persistence=inp.get("persistence"))
    state = dict(state)
    state["context"] = canon(last.get("v"))
    return norm_ret(ret), state, None


def d_reset_accounting_context(rig, ns, inp, state):
    ns["_accounting"] = SN(reset=rig.stub("contextvar_reset", ("token",)),
                           set=lambda value: rig.rec("contextvar_set", {"value": value}))
    state = dict(state)
    try:
        ret = ns["reset_accounting_context"](inp.get("token"))
    except BaseException as ex:
        return None, state, {"kind": type(ex).__name__, "message": str(ex)}
    # except Exception path sets None; success leaves context
    if rig.trace and rig.trace[-1]["ev"] == "contextvar_set":
        state["context"] = None
    return norm_ret(ret), state, None


def d_error_class(rig, ns, inp, state):
    ns["logger"] = logger_double(rig)
    errspec = inp.get("error", {})
    kind = errspec.get("kind", "Exception")
    err = build_exc(kind, errspec.get("message", ""), ns)
    if "provider_error" in errspec:
        try:
            setattr(err, "provider_error", errspec["provider_error"])
        except Exception:
            pass
    embedded_cls = ns.get("_EmbeddedProviderError")

    def classify(target, provider="", model=""):
        if embedded_cls is not None and isinstance(target, embedded_cls):
            tr = {"embedded": True, "message": str(target), "body": target.body}
        else:
            tr = {"embedded": False, "kind": type(target).__name__, "message": str(target)}
        return rig.rec("classify_api_error", {"target": tr, "provider": provider, "model": model},
                       lambda v: SN(reason=SN(value=v["reason"]["value"])))

    stub_module("agent.error_classifier", classify_api_error=classify)
    ret = ns["error_class"](err, provider=inp.get("provider", ""), model=inp.get("model", ""))
    return norm_ret(ret), state, None


def d_record_compression_regions(rig, ns, inp, state):
    ns["estimate_messages_tokens_rough"] = rig.stub("estimate_messages_tokens_rough", ("messages",))
    tel = state.get("_active_compression_telemetry")
    slf = SN(_active_compression_telemetry=tel)
    state = dict(state)
    try:
        ret = ns["_record_compression_regions"](slf, head_messages=inp.get("head_messages"),
                                                middle_messages=inp.get("middle_messages"),
                                                tail_messages=inp.get("tail_messages"))
    except BaseException as ex:
        state["_active_compression_telemetry"] = slf._active_compression_telemetry
        return None, state, {"kind": type(ex).__name__, "message": str(ex)}
    state["_active_compression_telemetry"] = slf._active_compression_telemetry
    return norm_ret(ret), state, None


def d_note_aux_route_fallback(rig, ns, inp, state):
    slf = SN(**state)
    try:
        ret = ns["_note_aux_route_fallback"](slf, inp.get("route"))
    except BaseException as ex:
        after = {k: canon(getattr(slf, k, None)) for k in state}
        return None, after, {"kind": type(ex).__name__, "message": str(ex)}
    after = {k: canon(getattr(slf, k, None)) for k in state}
    return norm_ret(ret), after, None


def d_iter_configured_targets(rig, ns, inp, state):
    ns["logger"] = logger_double(rig)
    ns["_parse_single_target"] = rig.stub("_parse_single_target", ("index", "entry"))
    ret = ns["iter_configured_targets"](inp.get("cfg"))
    return norm_ret(ret), state, None


def d_redirect_request(rig, ns, inp, state):
    ret = ns["redirect_request"](SN(), None, None, 302, "msg", {}, "http://x")
    return norm_ret(ret), state, None


def d_cache_path_base(rig, ns, inp, state):
    return norm_ret(ns["cache_path_base"](SN())), state, None


def d_default_cwd(rig, ns, inp, state):
    return norm_ret(ns["default_cwd"](SN())), state, None


def d_check_requirements(rig, ns, inp, state):
    slf = SN(is_available=rig.stub("is_available"))
    ret = ns["check_requirements"](slf, inp.get("config", {}))
    return norm_ret(ret), state, None


def d_setup_instructions(rig, ns, inp, state):
    return norm_ret(ns["setup_instructions"](SN())), state, None


def d_post_setup(rig, ns, inp, state):
    return norm_ret(ns["post_setup"](SN())), state, None


def d_doctor_checks(rig, ns, inp, state):
    slf = SN(is_available=rig.stub("is_available"), display_name=state.get("display_name", ""))
    ret = ns["doctor_checks"](slf)
    return norm_ret(ret), state, None


def d_blocked(rig, ns, inp, state):
    slf = SN(_settings=lambda: rig.rec("_settings", {}, lambda v: SN(**v)),
             _clock=lambda: rig.rec("_clock", {}),
             _lock=SilentLock(),
             _cooldown_until=state.get("cooldown_until", {}))
    ret = ns["blocked"](slf, inp.get("conversation"))
    return norm_ret(ret), state, None


def d_live_dm_allow_from(rig, ns, inp, state):
    slf = SN(_allow_from=state.get("_allow_from"))
    ret = ns["_live_dm_allow_from"](slf)
    return norm_ret(ret), state, None


def _entries_from(state):
    od = OrderedDict()
    for k, v in (state.get("entries") or {}).items():
        hk, sid = k.split("|", 1)
        od[(hk, sid)] = tuple(v)
    return od


def _entries_to(od):
    return {"%s|%s" % k: canon(list(v)) for k, v in od.items()}


def d_checkout(rig, ns, inp, state):
    vocab = (QUARRY / "persistence/authority/vocabulary.py").read_text(encoding="utf-8")
    horizon = None
    for node in ast.parse(vocab).body:
        if isinstance(node, ast.Assign) and any(getattr(t, "id", None) == "RETRY_HORIZON_S" for t in node.targets):
            horizon = eval(compile(ast.Expression(node.value), "<h>", "eval"), {})
    stub_module("persistence.authority.vocabulary", RETRY_HORIZON_S=horizon)
    ns["time"] = SN(time=lambda: rig.rec("time_time", {}))
    od = _entries_from(state)
    slf = SN(_owner_home=lambda: rig.rec("_owner_home", {}, lambda v: (v["home_key"],
                                                                         Path(v["home"]) if v.get("home") else None)),
             _lock=SilentLock(), _entries=od,
             _shutdown_async=rig.stub("_shutdown_async", ("manager", "owner")))
    state = dict(state)
    try:
        ret = ns["checkout"](slf, inp.get("session_id"))
    except BaseException as ex:
        state["entries"] = _entries_to(od)
        return None, state, {"kind": type(ex).__name__, "message": str(ex)}
    state["entries"] = _entries_to(od)
    return norm_ret(ret), state, None


def d_close_all(rig, ns, inp, state):
    od = _entries_from(state)
    slf = SN(_lock=SilentLock(), _entries=od, _shutdown=rig.stub("_shutdown", ("manager", "owner")))
    state = dict(state)
    try:
        ret = ns["close_all"](slf)
    except BaseException as ex:
        state["entries"] = _entries_to(od)
        return None, state, {"kind": type(ex).__name__, "message": str(ex)}
    state["entries"] = _entries_to(od)
    return norm_ret(ret), state, None


def d__timer(rig, ns, inp, state):
    async def sleep(delay):
        return rig.rec("asyncio_sleep", {"delay": delay})

    ns["asyncio"] = SN(sleep=sleep, CancelledError=asyncio.CancelledError)
    slf = SN(_timers=state.get("_timers", {}), _pending=state.get("_pending", {}),
             _settle=rig.stub("_settle", ("group_key", "entry")))
    coro = ns["_timer"](slf, inp.get("group_key"), inp.get("delay"))
    state = dict(state)
    try:
        ret = asyncio.run(coro)
    except BaseException as ex:
        state["_timers"] = slf._timers
        state["_pending"] = slf._pending
        return None, state, {"kind": type(ex).__name__, "message": str(ex)}
    state["_timers"] = slf._timers
    state["_pending"] = slf._pending
    return norm_ret(ret), state, None


def d__bearer(rig, ns, inp, state):
    ns["make_upgrade_token"] = rig.stub("make_upgrade_token", ("gateway_id", "secret"))
    slf = SN(_gateway_id=state.get("_gateway_id"), _secret=state.get("_secret"))
    ret = ns["_bearer"](slf)
    return norm_ret(ret), state, None


DRIVERS = {
    "_is_pausable_gateway": d__is_pausable_gateway,
    "probe_port": d_probe_port,
    "pairing_command": d_pairing_command,
    "extract_compress_flags": d_extract_compress_flags,
    "summarize_compress_preview": d_summarize_compress_preview,
    "is_valid": d_is_valid,
    "_invalid": d__invalid,
    "_own_launcher": d__own_launcher,
    "kanban_toolset_requested": d_kanban_toolset_requested,
    "publish": d_publish,
    "set_accounting_context": d_set_accounting_context,
    "reset_accounting_context": d_reset_accounting_context,
    "error_class": d_error_class,
    "record_compression_regions": d_record_compression_regions,
    "note_aux_route_fallback": d_note_aux_route_fallback,
    "iter_configured_targets": d_iter_configured_targets,
    "redirect_request": d_redirect_request,
    "cache_path_base": d_cache_path_base,
    "default_cwd": d_default_cwd,
    "check_requirements": d_check_requirements,
    "setup_instructions": d_setup_instructions,
    "post_setup": d_post_setup,
    "doctor_checks": d_doctor_checks,
    "blocked": d_blocked,
    "live_dm_allow_from": d_live_dm_allow_from,
    "checkout": d_checkout,
    "close_all": d_close_all,
    "_timer": d__timer,
    "_bearer": d__bearer,
}


def run_case(func, inp, c_out):
    module, fn, _is_method = TARGETS[func]
    ns = extract(module)
    rig = Rig(inp.get("io"), ns)
    state = inp.get("state") or {}
    driver = DRIVERS[func]
    try:
        ret, state_after, raise_spec = driver(rig, ns, inp, state)
    except BaseException as ex:  # SystemExit included
        ret, state_after, raise_spec = None, state, {"kind": type(ex).__name__, "message": str(ex)}
    p_out = {"ret": canon(ret), "trace": canon(rig.trace), "state": canon(state_after),
             "raise": canon(raise_spec)}
    return p_out


def main():
    data = sys.stdin.read()
    cases = []
    for line in data.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception as ex:
            print("ORACLE cli_gaps_revive1: FATAL bad harness line: %s" % ex)
            sys.exit(2)
    if not cases:
        print("ORACLE cli_gaps_revive1: FATAL no cases (harness produced nothing)")
        sys.exit(2)
    mismatches = 0
    for c in cases:
        func = c.get("func")
        if func not in TARGETS:
            print("MISMATCH unknown func %r" % func)
            mismatches += 1
            continue
        c_out = c.get("out") or {}
        try:
            p_out = run_case(func, c.get("in") or {}, c_out)
        except BaseException as ex:
            print("MISMATCH %s oracle-driver raised %s: %s" % (func, type(ex).__name__, ex))
            mismatches += 1
            continue
        if canon(c_out) != p_out:
            mismatches += 1
            print("MISMATCH %s" % func)
            print("  C   : %s" % json.dumps(canon(c_out), sort_keys=True)[:600])
            print("  live: %s" % json.dumps(p_out, sort_keys=True)[:600])
    print("ORACLE cli_gaps_revive1: %d cases, %d mismatches" % (len(cases), mismatches))
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()
