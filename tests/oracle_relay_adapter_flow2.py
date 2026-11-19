#!/usr/bin/env python3
"""Oracle for src/gateway/relay_adapter_flow2.c — drives the LIVE
gateway/relay/adapter.py UNCLAIMED-remainder FLOW defs (20 methods) under
IDENTICAL scripted universal seams.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the state window with an ORDERED canon (dict key order is
part of the wire-frame contract). Live defs are AST-extracted from the
RelayAdapter class and re-hosted in a synthetic class whose base supplies the
seamed super().warning_notifications_enabled; claimed siblings / foreign I/O
(transport outbound, descriptor negotiation, draft/card keys, scope/format-hint
stamps, redial toggle, prompt mint/send, logger) are recording seams fed the
SAME scripts the C harness feeds. Ported siblings stay REAL so composition is
exercised (warning -> _chat_platform, _draft_frame -> _text_metadata,
native_task_card_destination_supported -> _chat_platform/_task_card_metadata,
_prepare_slack_egress -> _chat_platform/_stamp_slack_unfurl,
_send_exec_approval_prompt -> _mint_and_send_prompt). The egress-decline leaves
(is_egress_decline / decline_error / log_decline) are the LIVE egress.py defs.

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
("no script for <name>"), recorded as the event ret before propagation; 0 cases
=> exit 1. A scripted BaseException (SystemExit / KeyboardInterrupt /
GeneratorExit / CancelledError) propagates past every `except Exception` guard
exactly as live. Per-case BaseException catch -> __raise__ spec; scripted raises
round-trip BARE via identity match against Script.handed. The state window is
projected from carriers materialized BEFORE the call, so a raise leaves the
mutation history at that point (matching the C in-place st).

Value model: SendResult doubles are tagged {"__send_result__":true,...fields}
(projected to the FULL dataclass field set on return); _MediaNotDelivered is
{"__not_delivered__":true,"reason":...}; callable doubles {"__callable__":seam}
fire that seam with {args,kwargs}; descriptor/config doubles are SimpleNamespace;
tuple/set -> array; integral floats -> ints.

Domain (ASCII scope, documented in the port header): str()/strip()/lower()
operands are ASCII; metadata/fields/state maps are dict-or-falsy; supported_ops
is list-or-falsy; attribute doubles carry the fields the live body reads.
EXCLUDED: integral-FLOAT literals in str()/type-name positions (libjson has no
int/float identity).

Summary: 'ORACLE relay_adapter_flow2: N cases, M mismatches'.
"""
import __future__
import ast
import asyncio
import builtins
import copy
import dataclasses
import json
import os
import sys
import types
from pathlib import Path
from types import SimpleNamespace
from typing import Any, Optional, NamedTuple

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "gateway" / "relay" / "adapter.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "gateway" / "relay" / "adapter.py")
BASE = str(QUARRY / "gateway" / "platforms" / "base.py")
EGRESS = str(QUARRY / "gateway" / "relay" / "egress.py")
NAME = "relay_adapter_flow2"

PORTED = [
    "_chat_platform", "warning_notifications_enabled", "_text_metadata",
    "_draft_frame", "send_draft", "_absorb_into_open_draft", "_card_frame",
    "_task_card_metadata", "native_task_card_destination_supported",
    "_relay_platform_extra", "_slack_flag", "_stamp_slack_unfurl",
    "hold_redial", "release_redial", "_prepare_slack_egress",
    "status_metadata_for_turn", "delete_message", "_send_local_media",
    "_mint_and_send_prompt", "_send_exec_approval_prompt",
]
ASYNC_FUNCS = {
    "send_draft", "_absorb_into_open_draft", "_card_frame", "delete_message",
    "_send_local_media", "_mint_and_send_prompt", "_send_exec_approval_prompt",
}
SYNC_SEAMS = [
    "supports_draft_streaming", "_draft_key", "_evict_oldest",
    "stream_is_message_for_chat", "_match_open_draft", "supports_native_task_cards",
    "_card_key", "_with_scope", "_with_format_hints_for_chat", "_relay_slack_extra",
    "_coerce_flag", "_slack_unfurl_hints", "_toggle_transport_redial",
    "_apply_slack_thread_anchor", "_with_status_thread_anchor", "_descriptor_for_chat",
    "_mint_prompt",
]
ASYNC_SEAMS = ["_outbound", "_seal_open_draft", "_send_media", "_send_prompt"]

MAP_DEFAULTS = {
    "_chat_platform": ["platform_by_chat"],
    "warning_notifications_enabled": ["platform_by_chat"],
    "native_task_card_destination_supported": ["platform_by_chat"],
    "_prepare_slack_egress": ["platform_by_chat"],
    "send_draft": ["sealed_draft_by_chat", "open_draft_by_chat", "tombstone_swallow_logged"],
    "_mint_and_send_prompt": ["pending_prompts"],
    "_send_exec_approval_prompt": ["pending_prompts"],
}
MUT_MAPS = {
    "send_draft": ["sealed_draft_by_chat", "open_draft_by_chat", "tombstone_swallow_logged"],
    "_mint_and_send_prompt": ["pending_prompts"],
    "_send_exec_approval_prompt": ["pending_prompts"],
}
MD_FUNCS = {"_stamp_slack_unfurl", "_prepare_slack_egress"}

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, path=SRC):
    return compile(ast.Module(body=[node], type_ignores=[]), path, "exec",
                   flags=_FLAGS, dont_inherit=True)


METHOD_NODES = {}
PROMPT_UNAVAILABLE_NODE = None
MEDIA_NT_NODE = None
SLACK_NODE = None
for _node in _TREE.body:
    if isinstance(_node, ast.ClassDef) and _node.name == "RelayAdapter":
        for _sub in _node.body:
            if isinstance(_sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and _sub.name in PORTED:
                METHOD_NODES[_sub.name] = _sub
            elif isinstance(_sub, ast.Assign):
                if any(isinstance(t, ast.Name) and t.id == "_PROMPT_UNAVAILABLE" for t in _sub.targets):
                    PROMPT_UNAVAILABLE_NODE = _sub
    elif isinstance(_node, ast.ClassDef) and _node.name == "_MediaNotDelivered":
        MEDIA_NT_NODE = _node
    elif isinstance(_node, ast.Assign):
        if any(isinstance(t, ast.Name) and t.id == "_SLACK" for t in _node.targets):
            SLACK_NODE = _node
_missing = set(PORTED) - set(METHOD_NODES)
if _missing or PROMPT_UNAVAILABLE_NODE is None or MEDIA_NT_NODE is None or SLACK_NODE is None:
    print(f"ORACLE {NAME}: FATAL missing live defs {_missing}", file=sys.stderr)
    sys.exit(1)

# real dataclasses from base.py
_BASE_TREE = ast.parse(Path(BASE).read_text(encoding="utf-8"))
BASE_CLASSES = {}
for _node in _BASE_TREE.body:
    if isinstance(_node, ast.ClassDef) and _node.name in ("SendResult", "ExecApprovalPrompt"):
        BASE_CLASSES[_node.name] = _node
if set(BASE_CLASSES) != {"SendResult", "ExecApprovalPrompt"}:
    print(f"ORACLE {NAME}: FATAL missing base dataclasses", file=sys.stderr)
    sys.exit(1)

# live egress defs
_EGRESS_TREE = ast.parse(Path(EGRESS).read_text(encoding="utf-8"))
EGRESS_NODES = {}
EGRESS_CONSTS = []
for _node in _EGRESS_TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in ("is_egress_decline", "decline_error", "log_decline"):
        EGRESS_NODES[_node.name] = _node
    elif isinstance(_node, ast.Assign):
        EGRESS_CONSTS.append(_node)
if set(EGRESS_NODES) != {"is_egress_decline", "decline_error", "log_decline"}:
    print(f"ORACLE {NAME}: FATAL missing egress defs", file=sys.stderr)
    sys.exit(1)


# ── canonical projection ────────────────────────────────────────────────────
def norm_value(v):
    if isinstance(v, BaseException):
        return str(v)
    if dataclasses.is_dataclass(v) and not isinstance(v, type):
        return norm_value(dataclasses.asdict(v))
    if isinstance(v, SimpleNamespace):
        return {k: norm_value(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, dict):
        return {k: norm_value(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_value(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm_value(x) for x in v), key=lambda s: str(s))
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def norm_args(v):
    v = norm_value(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    if isinstance(v, list):
        return [norm_args(x) for x in v]
    return v


def canon(x):
    if isinstance(x, dict):
        return ("obj", [(k, canon(v)) for k, v in x.items()])
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def make_exc(marker):
    kind = marker.get("__raise__", "RuntimeError")
    msg = marker.get("message", marker.get("msg", ""))
    base = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}
    builtin = {
        "TypeError": TypeError, "ValueError": ValueError, "RuntimeError": RuntimeError,
        "AttributeError": AttributeError, "KeyError": KeyError, "Exception": Exception,
        "TimeoutError": TimeoutError, "OSError": OSError, "LookupError": LookupError,
        "InterruptedError": InterruptedError, "FileNotFoundError": FileNotFoundError,
        "IndexError": IndexError, "StopIteration": StopIteration,
        "NotImplementedError": NotImplementedError, "AssertionError": AssertionError,
        "NameError": NameError, "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
        "ZeroDivisionError": ZeroDivisionError, "OverflowError": OverflowError,
        "UnicodeError": UnicodeError, "RecursionError": RecursionError, "MemoryError": MemoryError,
        "EOFError": EOFError, "SyntaxError": SyntaxError, "SystemError": SystemError,
        "BufferError": BufferError, "ReferenceError": ReferenceError, "Warning": Warning,
        "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
        "PermissionError": PermissionError, "FileExistsError": FileExistsError,
        "BlockingIOError": BlockingIOError, "ChildProcessError": ChildProcessError,
        "ProcessLookupError": ProcessLookupError, "IsADirectoryError": IsADirectoryError,
        "NotADirectoryError": NotADirectoryError, "ConnectionAbortedError": ConnectionAbortedError,
        "ConnectionRefusedError": ConnectionRefusedError, "ConnectionResetError": ConnectionResetError,
        "StopAsyncIteration": StopAsyncIteration, "UnboundLocalError": UnboundLocalError,
        "IndentationError": IndentationError, "TabError": TabError, "FloatingPointError": FloatingPointError,
        "ExceptionGroup": ExceptionGroup, "BaseExceptionGroup": BaseExceptionGroup,
    }
    if kind in base:
        cls = base[kind]
    elif kind in builtin:
        cls = builtin[kind]
    elif kind == "CancelledError":
        cls = asyncio.CancelledError
    else:
        cls = type(kind, (Exception,), {})
    if kind == "KeyError":
        try:
            return cls(ast.literal_eval(msg))
        except (ValueError, SyntaxError):
            return cls(msg)
    return cls(msg)


# ── scripted seams (fail-closed) ────────────────────────────────────────────
_MISSING = object()
CURRENT = {"ctx": None}


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


class Script:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.handed = []

    def next(self, name):
        entry = self.io.get(name, _MISSING)
        if entry is _MISSING:
            return self._fail(name)
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return self._fail(name)
            self.cursors[name] = idx + 1
            val = entry[idx]
        else:
            val = entry
        return self._unwrap(val)

    def _fail(self, name):
        spec = {"__raise__": "RuntimeError", "message": "no script for " + name}
        r = Raise(spec, make_exc(spec))
        self.handed.append(r)
        return r

    def _unwrap(self, val):
        if isinstance(val, dict) and "__raise__" in val:
            r = Raise(val, make_exc(val))
            self.handed.append(r)
            return r
        if isinstance(val, dict) and val.get("__script__") is True:
            return val.get("ret")
        return val


def proj(name, a, k):
    if name == "super.warning_notifications_enabled":
        return {"logical_platform": a[0] if a else None}
    if name == "supports_draft_streaming":
        return {"chat_id": k.get("chat_id")}
    if name == "_draft_key":
        return {"chat_id": a[0], "metadata": a[1] if len(a) > 1 else None}
    if name == "_evict_oldest":
        return {"d": norm_value(a[0])}
    if name == "stream_is_message_for_chat":
        return {"chat_id": a[0]}
    if name == "_match_open_draft":
        return {"chat_id": a[0], "metadata": a[1] if len(a) > 1 else None}
    if name == "_seal_open_draft":
        return {"chat_id": a[0], "content": a[1], "metadata": a[2], "draft_key": k.get("draft_key")}
    if name == "supports_native_task_cards":
        return {}
    if name == "_card_key":
        return {"reply_to": a[0], "metadata": a[1] if len(a) > 1 else None}
    if name in ("_with_scope", "_with_format_hints_for_chat"):
        return {"chat_id": a[0], "metadata": a[1] if len(a) > 1 else None}
    if name == "_outbound":
        return {"chat_id": a[0], "action": norm_value(a[1])}
    if name == "_relay_slack_extra":
        return {}
    if name == "_coerce_flag":
        return {"raw": a[0], "default": a[1]}
    if name == "_slack_unfurl_hints":
        return {"platform": a[0]}
    if name == "_toggle_transport_redial":
        return {"method_name": a[0]}
    if name == "_apply_slack_thread_anchor":
        return {"chat_id": a[0], "reply_to": a[1], "metadata": norm_value(a[2])}
    if name == "_with_status_thread_anchor":
        return {"chat_id": a[0], "metadata": a[1], "anchor": a[2] if len(a) > 2 else None}
    if name == "_descriptor_for_chat":
        return {"chat_id": a[0]}
    if name == "_send_media":
        return {"chat_id": a[0], **{kk: norm_value(vv) for kk, vv in k.items()}}
    if name == "_mint_prompt":
        return {"kind": a[0], "state": norm_value(a[1])}
    if name == "_send_prompt":
        return {"chat_id": a[0], **{kk: norm_value(vv) for kk, vv in k.items()}}
    if name in ("logger.warning", "logger.debug", "logger.info"):
        d = {"msg": a[0], "args": list(a[1:])}
        if k.get("exc_info"):
            d["exc_info"] = True
        return d
    if name == "fallback":
        return {"args": norm_value(list(a)), "kwargs": norm_value(k)}
    return {**{("arg%d" % i): norm_value(v) for i, v in enumerate(a)},
            **{kk: norm_value(vv) for kk, vv in k.items()}}


def record(name, argsdict):
    ctx = CURRENT["ctx"]
    args = norm_args(argsdict)
    raw = ctx.script.next(name)
    if isinstance(raw, Raise):
        ctx.trace.append({"ev": name, "args": args, "ret": raw.spec})
        raise raw.exc
    ctx.trace.append({"ev": name, "args": args, "ret": norm_value(raw)})
    return materialize_result(name, raw)


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **k):
        return record(self.name, proj(self.name, a, k))


class AsyncSeamFn:
    def __init__(self, name):
        self.name = name

    async def __call__(self, *a, **k):
        return record(self.name, proj(self.name, a, k))


class LoggerDouble:
    def _log(self, level, msg, args, kw):
        d = {"msg": msg, "args": list(args)}
        if kw.get("exc_info"):
            d["exc_info"] = True
        return record("logger." + level, d)

    def debug(self, msg, *a, **k):
        return self._log("debug", msg, a, k)

    def info(self, msg, *a, **k):
        return self._log("info", msg, a, k)

    def warning(self, msg, *a, **k):
        return self._log("warning", msg, a, k)

    def error(self, msg, *a, **k):
        return self._log("error", msg, a, k)

    def log(self, level, msg, *a, **k):
        return self._log(str(level), msg, a, k)


# ── attribute-double materializers ──────────────────────────────────────────
def _strip_sr(d):
    return {k: v for k, v in d.items() if k != "__send_result__"}


def mk_sr(d):
    if d is None:
        return None
    return NS["SendResult"](**_strip_sr(d))


def mk_media(d):
    if d is None:
        return None
    if d.get("__not_delivered__"):
        return NS["_MediaNotDelivered"](d.get("reason"))
    return NS["SendResult"](**_strip_sr(d))


def mk_desc(d):
    return SimpleNamespace(**d) if isinstance(d, dict) else d


def mk_async_callable(name):
    async def f(*a, **k):
        return record(name, {"args": list(a), "kwargs": k})
    f.__seam_name__ = name
    return f


ATTR_SEAMS = {
    "_seal_open_draft": mk_sr,
    "_send_prompt": mk_sr,
    "_send_media": mk_media,
    "_descriptor_for_chat": mk_desc,
    "fallback": mk_sr,
}


def materialize_result(name, raw):
    if raw is None:
        return None
    if isinstance(raw, dict) and "__callable__" in raw:
        return mk_async_callable(raw["__callable__"])
    if name in ATTR_SEAMS and isinstance(raw, dict):
        return ATTR_SEAMS[name](raw)
    return raw


def materialize_double(j):
    if isinstance(j, dict) and "__callable__" in j:
        return mk_async_callable(j["__callable__"])
    return j


def mk_prompt(d):
    if not isinstance(d, dict):
        return d
    dd = dict(d)
    dd["actions"] = [tuple(r) for r in dd.get("actions", [])]
    return NS["ExecApprovalPrompt"](**dd)


# ── live namespace + synthetic class ────────────────────────────────────────
_MOD = types.ModuleType("ra2_live")
sys.modules["ra2_live"] = _MOD
NS = _MOD.__dict__
NS.update({
    "__name__": "ra2_live", "__builtins__": builtins,
    "Any": Any, "Optional": Optional, "NamedTuple": NamedTuple,
    "dataclass": dataclasses.dataclass, "field": dataclasses.field,
    "logger": LoggerDouble(),
    "Platform": SimpleNamespace(SLACK=SimpleNamespace(value="slack"),
                                RELAY=SimpleNamespace(value="relay")),
})
# module constant _SLACK
exec(_compile_node(SLACK_NODE), NS)
# _MediaNotDelivered NamedTuple
exec(_compile_node(MEDIA_NT_NODE), NS)
# real base dataclasses
for _nm, _node in BASE_CLASSES.items():
    exec(_compile_node(_node, BASE), NS)


class BaseDouble:
    pass


BaseDouble.warning_notifications_enabled = SeamFn("super.warning_notifications_enabled")
NS["BaseDouble"] = BaseDouble

# synthetic class hosting the ported methods (super() resolves to BaseDouble)
_syn = ast.ClassDef(
    name="RADouble", bases=[ast.Name(id="BaseDouble", ctx=ast.Load())],
    keywords=[], body=[METHOD_NODES[n] for n in PORTED] + [PROMPT_UNAVAILABLE_NODE],
    decorator_list=[],
)
ast.fix_missing_locations(_syn)
exec(_compile_node(_syn), NS)
RADouble = NS["RADouble"]

# live egress namespace (its logger records into the same trace)
_ENS = {"__name__": "ra2_egress", "__builtins__": builtins, "logger": LoggerDouble(),
        "Any": Any, "Dict": dict, "Optional": Optional, "Set": set}
for _node in EGRESS_CONSTS:
    try:
        exec(_compile_node(_node, EGRESS), _ENS)
    except Exception:
        pass
_ENS["logger"] = LoggerDouble()
for _nm, _node in EGRESS_NODES.items():
    exec(_compile_node(_node, EGRESS), _ENS)
for _nm in ("is_egress_decline", "decline_error", "log_decline"):
    NS[_nm] = _ENS[_nm]
NS["logger"] = LoggerDouble()


# ── per-case context ────────────────────────────────────────────────────────
class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.script = Script(inp.get("io"))


def make_self(inp):
    st_in = inp.get("state") or {}
    slf = RADouble()
    slf._platform_by_chat = dict(st_in.get("platform_by_chat") or {})
    dj = st_in.get("descriptor")
    slf.descriptor = SimpleNamespace(**dj) if isinstance(dj, dict) else dj
    cfg = st_in.get("config")
    slf.config = SimpleNamespace(**cfg) if isinstance(cfg, dict) else cfg
    tj = st_in.get("transport")
    slf._transport = None if tj is None else (SimpleNamespace(**tj) if isinstance(tj, dict) else tj)
    slf._sealed_draft_by_chat = dict(st_in.get("sealed_draft_by_chat") or {})
    slf._open_draft_by_chat = dict(st_in.get("open_draft_by_chat") or {})
    slf._tombstone_swallow_logged = dict(st_in.get("tombstone_swallow_logged") or {})
    slf._pending_prompts = dict(st_in.get("pending_prompts") or {})
    for nm in SYNC_SEAMS:
        setattr(slf, nm, SeamFn(nm))
    for nm in ASYNC_SEAMS:
        setattr(slf, nm, AsyncSeamFn(nm))
    return slf


def drive(func, inp, holder):
    st_in = inp.get("state") or {}
    slf = make_self(inp)
    md = copy.deepcopy(inp.get("metadata")) if func in MD_FUNCS else None

    def proj_st():
        out = norm_value(copy.deepcopy(st_in))
        for k in MAP_DEFAULTS.get(func, []):
            if k not in out:
                out[k] = {}
        for k in MUT_MAPS.get(func, []):
            out[k] = norm_value(getattr(slf, "_" + k))
        if func in MD_FUNCS:
            out["metadata"] = norm_value(md)
        return out

    holder["proj"] = proj_st
    g = inp.get
    if func == "_chat_platform":
        return slf._chat_platform(g("chat_id"))
    if func == "warning_notifications_enabled":
        return slf.warning_notifications_enabled(g("logical_platform"),
                                                 chat_id=g("chat_id"), metadata=g("metadata"))
    if func == "_text_metadata":
        return slf._text_metadata(g("chat_id"), g("metadata"))
    if func == "_draft_frame":
        return slf._draft_frame(g("chat_id"), g("draft_id"), g("content"), g("final"), g("metadata"))
    if func == "send_draft":
        return asyncio.run(slf.send_draft(g("chat_id"), g("draft_id"), g("content"), metadata=g("metadata")))
    if func == "_absorb_into_open_draft":
        return asyncio.run(slf._absorb_into_open_draft(g("chat_id"), g("content"), g("metadata"), g("interim")))
    if func == "_card_frame":
        return asyncio.run(slf._card_frame(g("chat_id"), g("op"), g("reply_to"), g("metadata"),
                                           **(g("fields") or {})))
    if func == "_task_card_metadata":
        return slf._task_card_metadata(g("reply_to"), g("metadata"))
    if func == "native_task_card_destination_supported":
        return slf.native_task_card_destination_supported(g("chat_id"), reply_to=g("reply_to"), metadata=g("metadata"))
    if func == "_relay_platform_extra":
        return slf._relay_platform_extra(g("platform"))
    if func == "_slack_flag":
        return slf._slack_flag(g("knob"), g("default"))
    if func == "_stamp_slack_unfurl":
        slf._stamp_slack_unfurl(g("platform"), md)
        return None
    if func == "hold_redial":
        return slf.hold_redial()
    if func == "release_redial":
        return slf.release_redial()
    if func == "_prepare_slack_egress":
        return slf._prepare_slack_egress(g("chat_id"), g("reply_to"), md)
    if func == "status_metadata_for_turn":
        return slf.status_metadata_for_turn(g("chat_id"), g("anchor"), g("metadata"))
    if func == "delete_message":
        return asyncio.run(slf.delete_message(g("chat_id"), g("message_id")))
    if func == "_send_local_media":
        return asyncio.run(slf._send_local_media(g("chat_id"), g("media_kind"), g("path"), g("caption"),
                                                 g("reply_to"), g("metadata"),
                                                 materialize_double(g("fallback")), filename=g("filename")))
    if func == "_mint_and_send_prompt":
        return asyncio.run(slf._mint_and_send_prompt(g("kind"), g("state"), g("chat_id"),
                                                     prompt_kind=g("prompt_kind"), text=g("text"),
                                                     options=g("options"), metadata=g("metadata")))
    if func == "_send_exec_approval_prompt":
        return asyncio.run(slf._send_exec_approval_prompt(mk_prompt(g("prompt"))))
    raise RuntimeError("unknown func " + func)


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    holder = {}
    try:
        ret = drive(func, inp, holder)
        st = holder["proj"]() if "proj" in holder else norm_value(inp.get("state") or {})
        return {"ret": norm_value(ret), "trace": ctx.trace, "st": st}
    except BaseException as e:  # noqa: BLE001 - fail-closed per case
        spec = None
        for r in ctx.script.handed:
            if e is r.exc:
                spec = r.spec
                break
        if spec is None:
            spec = {"__raise__": type(e).__name__, "message": str(e)}
        st = holder["proj"]() if "proj" in holder else {}
        return {"ret": spec, "trace": ctx.trace, "st": st}


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = rec.get("func")
        inp = rec.get("in") or {}
        n += 1
        want = run_case(func, inp)
        got = norm_value({"ret": rec.get("ret"), "trace": rec.get("trace"), "st": rec.get("st")})
        if canon(got) != canon(want):
            mism += 1
            if mism <= 12:
                print("MISMATCH [%s] case %d" % (func, n))
                for key in ("ret", "trace", "st"):
                    if canon(got.get(key)) != canon(want.get(key)):
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:700]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:700]))
    print("ORACLE %s: %d cases, %d mismatches" % (NAME, n, mism))
    if n == 0:
        return 1
    return 1 if mism else 0


# ── self-test: drive every fixture, flag suspicious raises / drive failures ──
def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(60, exit=True)
    bad = 0
    names = set(PORTED)
    funcs_seen = set()
    for i, rec in enumerate(fixtures):
        func = rec.get("func")
        funcs_seen.add(func)
        if func not in names:
            bad += 1
            print(f"SELFTEST case {i}: func {func} not in PORTED")
            continue
        try:
            out = run_case(func, rec.get("in") or {})
            spec = out["ret"]
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                msg = spec.get("message", "")
                if any(("'" + n + "'") in msg for n in names) or "RADouble" in msg:
                    bad += 1
                    print(f"SELFTEST case {i} {func}: suspicious AttributeError {msg}")
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {func}: DRIVE FAILURE {type(e).__name__}: {e}")
    faulthandler.cancel_dump_traceback_later()
    missing = names - funcs_seen
    if missing:
        bad += 1
        print(f"SELFTEST: funcs with no fixture: {sorted(missing)}")
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = _HERE.parent / "fixtures" / "relay_adapter_flow2.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    sys.exit(main())