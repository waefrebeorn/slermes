#!/usr/bin/env python3
"""Oracle for the Matrix-adapter sync/event + reaction flow port
(src/gateway/platforms/matrix_adapter_flow3.c).

Drives the LIVE plugins/platforms/matrix/adapter.py UNCLAIMED methods for the
encrypted-drop warning / room allowlist gate and the reaction-prompt lifecycle
(processing eyes ticks, reaction routing, claim/validate/expire/redact) with
SCRIPTED seams: every seamed sibling, every module collaborator, the in-function
tools.approval import, hash_for_log, t(), and prompt.on_selected are recording
stubs fed the same scripts the C harness feeds its universal seam. Compares the
DECISION SEQUENCE (ordered seam events {"name","args","ret"} in "trace") plus
the final return ("ret") and mutated state ("state").

The live defs are AST-extracted from adapter.py's MatrixAdapter ClassDef
(decorators stripped, dedented) and bound onto SimpleNamespace self doubles.
Async methods drive via asyncio.run with async seam recorders. The heavy quarry
import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit/CancelledError included). Scripted raises round-trip
their spec verbatim; unscripted live raises project {__raise__, message:str(exc)}.
Summary: 'ORACLE matrix_adapter_flow3: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- Plain JSON numbers are ints; bools are bools; a type-sensitive float travels
  as {"__float__": true, "v": x}; integral floats normalize to int (canon).
- Prompt/registry entries are {"__attr__": {...}} attribute carriers; the oracle
  builds SimpleNamespace doubles, keeping choices/offered/bot_reaction_events as
  plain dicts and on_selected as a callable marker.
- The registry / config / prompt objects live in args.self and are NOT echoed;
  only the state keys in WRITE_SET (plus input state) are echoed.
- logger calls are silent (not traced); the f-string cause of _warn_encrypted_drops
  is unobservable.
"""
import ast
import asyncio
import copy
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import Path
from contextlib import suppress
import typing

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "plugins", "platforms", "matrix", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

METHOD_FUNCS = [
    "_warn_encrypted_drops", "_is_allowed_matrix_room_event", "_redact_reaction",
    "on_processing_start", "on_processing_complete", "_on_reaction",
    "_claim_reaction_prompt", "_handle_approval_reaction",
    "_handle_model_picker_reaction", "_handle_choice_picker_reaction",
    "_handle_picker_reaction", "_validate_matrix_prompt_reactor",
    "_send_invalid_reaction_feedback", "_expire_matrix_approval_prompt",
    "_expire_matrix_model_picker_prompt", "_redact_bot_approval_reactions",
    "_redact_bot_model_picker_reactions",
]
ASYNC_FUNCS = set(METHOD_FUNCS) - {"_warn_encrypted_drops"}
STATIC_METHODS = set()
WRITE_SET = {
    "_warn_encrypted_drops": {"_warned_encrypted_drop_rooms"},
    "on_processing_start": {"_pending_reactions"},
    "on_processing_complete": {"_pending_reactions"},
}
REGISTRY_KEYS = [
    "_approval_prompts_by_event", "_approval_prompt_by_session",
    "_model_picker_prompts_by_event", "_choice_picker_prompts_by_event",
]

def registry_proj(registry):
    """Project a self registry of prompt doubles back to {"__attr__":{...}}."""
    if not isinstance(registry, dict):
        return registry
    out = {}
    for k, v in registry.items():
        if isinstance(v, types.SimpleNamespace):
            out[k] = {"__attr__": dict(vars(v))}
        else:
            out[k] = v
    return out


def claim_build(raw):
    """Scripted claim result: element 1 is a prompt carrier -> attribute double."""
    if isinstance(raw, list) and len(raw) >= 2:
        out = list(raw)
        out[1] = prompt_double(out[1], _build_ctx[0]) if isinstance(out[1], dict) else out[1]
        return tuple(out) if isinstance(raw, tuple) else out
    return raw


_build_ctx = [None]  # set per case in drive()


# name -> args_fn, is_async, build  (the single source of truth for seam args)
SIBLING_SPECS = {
    "_is_self_sender": (lambda sender: {"sender": sender}, False, None),
    "_is_duplicate_event": (lambda event_id: {"event_id": event_id}, False, None),
    "hash_for_log": (lambda room_id: {"room_id": room_id}, False, None),
    "_handle_approval_reaction": (lambda room_id, reacts_to, key, sender: {
        "room_id": room_id, "reacts_to": reacts_to, "key": key, "sender": sender}, True, None),
    "_handle_model_picker_reaction": (lambda room_id, reacts_to, key, sender: {
        "room_id": room_id, "reacts_to": reacts_to, "key": key, "sender": sender}, True, None),
    "_handle_picker_reaction": (lambda registry, room_id, reacts_to, key, sender, label,
                               invalid_text, on_expired, verbs, redact_bot_reactions=False: {
        "registry": registry_proj(registry), "room_id": room_id, "reacts_to": reacts_to, "key": key,
        "sender": sender, "label": label, "invalid_text": invalid_text,
        "choices": None, "verbs": list(verbs),
        "redact_bot_reactions": bool(redact_bot_reactions)}, True, None),
    "_handle_choice_picker_reaction": (lambda room_id, reacts_to, key, sender: {
        "room_id": room_id, "reacts_to": reacts_to, "key": key, "sender": sender}, True, None),
    "_send_reaction": (lambda room_id, event_id, emoji: {
        "room_id": room_id, "event_id": event_id, "emoji": emoji}, True, None),
    "_schedule_reaction_redaction": (lambda room_id, reaction_event_id, reason=None: {
        "room_id": room_id, "reaction_event_id": reaction_event_id, "reason": reason or ""}, False, None),
    "_claim_reaction_prompt": (lambda registry, room_id, reacts_to, key, sender, label, invalid_text,
                               on_expired, choices=None: {
        "registry": registry_proj(registry), "room_id": room_id, "reacts_to": reacts_to,
        "key": key, "sender": sender, "label": label, "invalid_text": invalid_text,
        "choices": choices}, True, claim_build),
    "on_expired": (lambda room_id, target_event_id, prompt: {
        "room_id": room_id, "target_event_id": target_event_id, "prompt": prompt}, True, None),
    "_validate_matrix_prompt_reactor": (lambda room_id, target_event_id, sender, prompt, prompt_label: {
        "room_id": room_id, "target_event_id": target_event_id, "sender": sender,
        "prompt": prompt, "prompt_label": prompt_label}, True, None),
    "_send_invalid_reaction_feedback": (lambda room_id, target_event_id, text: {
        "room_id": room_id, "target_event_id": target_event_id, "text": text}, True, None),
    "_is_authorized_user": (lambda user_id, room_id=None: {
        "user_id": user_id, "room_id": room_id}, False, None),
    "t": (lambda key, **kw: dict(key=key, **kw), False, None),
    "approval_refusal": (lambda session_key, sender, extra=None, request_id=None, choice=None: {
        "session_key": session_key, "sender": sender, "extra": extra,
        "request_id": request_id, "choice": choice}, False, None),
    "refusal_text": (lambda requester: {"requester": requester}, False, None),
    "_redact_bot_approval_reactions": (lambda room_id, prompt: {
        "room_id": room_id, "prompt": prompt}, True, None),
    "resolve_gateway_approval_card": (lambda session_key, choice, request_id, actor_id=None, extra=None: {
        "session_key": session_key, "choice": choice, "request_id": request_id,
        "actor_id": actor_id, "extra": extra}, False, None),
    "_redact_bot_model_picker_reactions": (lambda room_id, prompt: {
        "room_id": room_id, "prompt": prompt}, True, None),
    "send": (lambda room_id, text, reply_to=None: {
        "room_id": room_id, "text": text, "reply_to": reply_to}, True, None),
    "_expire_matrix_approval_prompt": (lambda room_id, target_event_id, prompt: {
        "room_id": room_id, "target_event_id": target_event_id, "prompt": prompt}, True, None),
    "prompt.on_selected": (lambda room_id, *args: {"room_id": room_id, "args": list(args)}, True, None),
    "picker_refusal": (lambda on_selected, sender, extra=None: {
        "on_selected": str(on_selected) if on_selected is not None else None,
        "sender": sender, "extra": extra}, False, None),
    "_is_dm_room": (lambda room_id: {"room_id": room_id}, True, None),
    "_matrix_prompt_expired": (lambda prompt: {"prompt": prompt}, False, None),
    "redact_message": (lambda room_id, event_id, reason=None: {
        "room_id": room_id, "event_id": event_id, "reason": reason or ""}, True, None),
}

SEAMED_SIBLINGS = {
    "_warn_encrypted_drops": [],
    "_is_allowed_matrix_room_event": ["_is_dm_room"],
    "_redact_reaction": ["redact_message"],
    "on_processing_start": ["_send_reaction"],
    "on_processing_complete": ["_schedule_reaction_redaction", "_send_reaction"],
    "_on_reaction": ["_is_self_sender", "_is_duplicate_event", "hash_for_log",
                     "_handle_approval_reaction", "_handle_model_picker_reaction",
                     "_handle_choice_picker_reaction"],
    "_claim_reaction_prompt": ["_matrix_prompt_expired", "on_expired",
                               "_validate_matrix_prompt_reactor", "_send_invalid_reaction_feedback"],
    "_handle_approval_reaction": ["_claim_reaction_prompt", "_send_invalid_reaction_feedback",
                                  "approval_refusal", "refusal_text",
                                  "resolve_gateway_approval_card",
                                  "_redact_bot_approval_reactions",
                                  "_expire_matrix_approval_prompt", "t"],
    "_handle_model_picker_reaction": ["_handle_picker_reaction", "t"],
    "_handle_choice_picker_reaction": ["_handle_picker_reaction", "t"],
    "_handle_picker_reaction": ["_claim_reaction_prompt", "prompt.on_selected",
                                "_redact_bot_model_picker_reactions", "send", "t"],
    "_validate_matrix_prompt_reactor": ["_is_authorized_user", "_send_invalid_reaction_feedback",
                                        "picker_refusal", "refusal_text", "t"],
    "_send_invalid_reaction_feedback": ["send"],
    "_expire_matrix_approval_prompt": ["_redact_bot_approval_reactions",
                                       "_send_invalid_reaction_feedback", "t"],
    "_expire_matrix_model_picker_prompt": ["_redact_bot_model_picker_reactions",
                                           "_send_invalid_reaction_feedback", "t"],
    "_redact_bot_approval_reactions": ["_schedule_reaction_redaction"],
    "_redact_bot_model_picker_reactions": ["redact_message"],
}


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


def extract():
    mod = new_module("ma3_oracle_adapter")
    ns = mod.__dict__
    ns.update({
        "re": re, "os": os, "json": json, "Path": Path, "suppress": suppress,
        "asyncio": asyncio, "typing": typing, "Any": typing.Any,
        "Optional": typing.Optional, "Dict": typing.Dict, "Set": typing.Set,
        "RoomID": str, "EventID": str, "UserID": str, "ContentURI": str,
        "EventType": type("_EventTypeStub", (), {"ROOM_MESSAGE": "m.room.message",
            "REACTION": "m.reaction", "ROOM_ENCRYPTED": "m.room.encrypted"}),
        "PresenceState": type("_PresenceStateStub", (), {
            "ONLINE": "online", "OFFLINE": "offline", "UNAVAILABLE": "unavailable"}),
        "ProcessingOutcome": type("_ProcessingOutcomeStub", (), {
            "CANCELLED": "cancelled", "SUCCESS": "success", "FAILED": "failed"}),
    })
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "MatrixAdapter")
    class_consts = {}
    for n in cls.body:
        if isinstance(n, (ast.Assign, ast.AnnAssign)):
            tgts = n.targets if isinstance(n, ast.Assign) else [n.target]
            if all(isinstance(t, ast.Name) for t in tgts):
                try:
                    exec_node(src, n, ns, SRC)
                    for t in tgts:
                        class_consts[t.id] = ns[t.id]
                except Exception:
                    pass
    for n in cls.body:
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name in METHOD_FUNCS:
            n2 = copy.copy(n)
            n2.decorator_list = []
            exec_node(src, n2, ns, SRC)
    ns["logger"] = _SilentLogger()
    missing = [k for k in METHOD_FUNCS if k not in ns]
    if missing:
        print(f"FATAL oracle_matrix_adapter_flow3: adapter missing: {missing}")
        sys.exit(2)
    ns["__class_consts__"] = class_consts
    return ns


NS = extract()


def canon(v):
    def conv(x):
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, BaseException):
            return str(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "OSError": OSError, "Exception": Exception,
    "StopIteration": StopIteration, "IndexError": IndexError, "LookupError": LookupError,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": asyncio.CancelledError,
    "TimeoutError": TimeoutError, "AssertionError": AssertionError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.scripted = []

    def _fail(self, name, args, msg):
        exc = RuntimeError(msg)
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        if entry is _MISSING:
            self._fail(name, args, f"no script for {name}")
        used = entry
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._fail(name, args, f"script exhausted for {name}")
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                sp = used["raise"]
                exc = make_exc(sp)
                spec = {"__raise__": sp.get("type", "RuntimeError"),
                        "message": sp.get("message", "")}
                self.scripted.append((exc, spec))
                self.trace.append({"name": name, "args": args, "ret": spec})
                raise exc
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret

    def spec_for(self, exc):
        for e, spec in self.scripted:
            if e is exc:
                return spec
        return None


def rec(ctx, name, args_fn=None, build=None, is_async=False):
    if is_async:
        async def f(*a, **k):
            args = args_fn(*a, **k) if args_fn else {}
            raw = ctx.call(name, args)
            return build(raw) if build else raw
        return f

    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


def pyval(v):
    if isinstance(v, dict) and v.get("__float__") is True:
        return float(v.get("v", 0))
    return v


class MarkerCallable:
    """prompt.on_selected recorder with a canonical string projection."""
    def __init__(self, marker, fn):
        self.marker = marker
        self.fn = fn

    async def __call__(self, *a, **k):
        return await self.fn(*a, **k)

    def __str__(self):
        return self.marker


class TaskSetElem:
    """(unused placeholder kept out of state echo.)"""
    pass


def prompt_double(spec, ctx):
    if spec is None:
        return None
    if not isinstance(spec, dict):
        return spec
    inner = spec.get("__attr__", spec)
    ns = types.SimpleNamespace()
    for k, v in inner.items():
        if k == "on_selected":
            r = rec(ctx, "prompt.on_selected", SIBLING_SPECS["prompt.on_selected"][0],
                    None, is_async=True)
            marker = v if isinstance(v, str) else "__on_selected__"
            setattr(ns, k, MarkerCallable(marker, r))
        elif k in ("choices", "offered", "bot_reaction_events"):
            setattr(ns, k, copy.deepcopy(v))
        else:
            setattr(ns, k, pyval(v))
    return ns


def client_double(spec, ctx):
    if spec is None:
        return None
    if not isinstance(spec, dict):
        return spec
    inner = spec.get("__attr__", spec)
    ns = types.SimpleNamespace()
    for k, v in inner.items():
        setattr(ns, k, pyval(v))
    return ns


def install_global_seams(ctx):
    # module-level free functions only (never clobber the extracted methods)
    for nm in ("t", "hash_for_log", "approval_refusal", "refusal_text",
               "picker_refusal", "resolve_gateway_approval_card"):
        NS[nm] = rec(ctx, nm, SIBLING_SPECS[nm][0], None, False)
    # in-function import interception (a stub parent halts the editable finder)
    if "tools" not in sys.modules:
        parent = new_module("tools")
        parent.__path__ = []
    stub = new_module("tools.approval")
    stub.resolve_gateway_approval_card = NS["resolve_gateway_approval_card"]


def mk_self(func, args, state_in, ctx):
    attrs = args.get("self") or {}
    slf = types.SimpleNamespace()
    for k, v in attrs.items():
        if k in REGISTRY_KEYS and isinstance(v, dict):
            setattr(slf, k, {rk: prompt_double(pv, ctx) for rk, pv in v.items()})
        elif k == "_client":
            setattr(slf, k, client_double(v, ctx))
        elif k == "config":
            inner = (v or {}).get("__attr__", v or {}) if isinstance(v, dict) else {}
            setattr(slf, k, types.SimpleNamespace(**{kk: pyval(vv) for kk, vv in inner.items()}))
        else:
            setattr(slf, k, pyval(v))
    for k, v in (state_in or {}).items():
        if k == "_warned_encrypted_drop_rooms":
            setattr(slf, k, set(v or []))
        elif k == "_pending_reactions":
            # fixture keys are str(tuple); materialize the real tuple key so
            # live pop((room_id, msg_id)) hits, then echo projects back to str
            dd = {}
            for rk, rv in (v or {}).items():
                try:
                    key = ast.literal_eval(rk) if isinstance(rk, str) and rk.startswith("(") else rk
                except Exception:
                    key = rk
                dd[key] = pyval(rv)
            setattr(slf, k, dd)
        else:
            setattr(slf, k, pyval(v))
    for k, v in NS["__class_consts__"].items():
        setattr(slf, k, v)
    # bind the target method REAL (never seamed); siblings are recorders below
    if func in NS:
        setattr(slf, func, types.MethodType(NS[func], slf))
    # attributes only passed as objects to a seamed sibling (never fired)
    if func == "_handle_model_picker_reaction":
        slf._expire_matrix_model_picker_prompt = lambda *a, **k: None
    for nm in SEAMED_SIBLINGS.get(func, []):
        if nm == "prompt.on_selected":
            continue
        afn, is_async, build = SIBLING_SPECS[nm]
        seam_name = "self." + nm if nm.startswith("_") or nm in ("redact_message", "send") else nm
        setattr(slf, nm, rec(ctx, seam_name, afn, build, is_async))
    return slf


def echo_state(func, slf, state_in):
    keys = set(state_in or {}) | WRITE_SET.get(func, set())
    out = {}
    for k in keys:
        if not hasattr(slf, k):
            continue
        v = getattr(slf, k)
        if isinstance(v, (set, frozenset)):
            out[k] = sorted(str(i) if not isinstance(i, (int, float, str, bool)) else i for i in v)
        elif k == "_pending_reactions" and isinstance(v, dict):
            # live uses a tuple key (room_id, msg_id); project it to str(key)
            out[k] = {str(rk): rv for rk, rv in v.items()}
        else:
            out[k] = v
    return out


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    state_in = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    _build_ctx[0] = ctx
    install_global_seams(ctx)
    slf = None
    state = {}
    try:
        slf = mk_self(func, args, state_in, ctx)
        ret = _invoke(func, slf, args, ctx)
    except BaseException as exc:
        spec = ctx.spec_for(exc)
        ret = spec if spec is not None else proj_exc(exc)
    if slf is not None:
        state = echo_state(func, slf, state_in)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


def _invoke(func, slf, args, ctx):
    g = args.get
    if func == "_warn_encrypted_drops":
        coro = slf._warn_encrypted_drops(g("rooms_join") or {}, client_double(g("client"), ctx))
    elif func == "_is_allowed_matrix_room_event":
        coro = slf._is_allowed_matrix_room_event(g("room_id"))
    elif func == "_redact_reaction":
        coro = slf._redact_reaction(g("room_id"), g("reaction_event_id"), g("reason") or "")
    elif func == "on_processing_start":
        coro = slf.on_processing_start(_event(g("event")))
    elif func == "on_processing_complete":
        coro = slf.on_processing_complete(_event(g("event")), g("outcome") or "")
    elif func == "_on_reaction":
        coro = slf._on_reaction(_event(g("event")))
    elif func == "_claim_reaction_prompt":
        on_expired = getattr(slf, "on_expired", None)
        coro = slf._claim_reaction_prompt(
            _registry(slf), g("room_id"), g("reacts_to"), g("key"),
            g("sender"), g("label"), g("invalid_text"), on_expired, g("choices"))
    elif func == "_handle_approval_reaction":
        coro = slf._handle_approval_reaction(g("room_id"), g("reacts_to"), g("key"), g("sender"))
    elif func == "_handle_model_picker_reaction":
        coro = slf._handle_model_picker_reaction(g("room_id"), g("reacts_to"), g("key"), g("sender"))
    elif func == "_handle_choice_picker_reaction":
        coro = slf._handle_choice_picker_reaction(g("room_id"), g("reacts_to"), g("key"), g("sender"))
    elif func == "_handle_picker_reaction":
        coro = slf._handle_picker_reaction(
            _registry(slf), g("room_id"), g("reacts_to"), g("key"),
            g("sender"), g("label"), g("invalid_text"), None, tuple(g("verbs") or []),
            redact_bot_reactions=bool(g("redact_bot_reactions")))
    elif func == "_validate_matrix_prompt_reactor":
        coro = slf._validate_matrix_prompt_reactor(
            g("room_id"), g("target_event_id"), g("sender"), _prompt(g("prompt"), ctx),
            g("prompt_label"))
    elif func == "_send_invalid_reaction_feedback":
        coro = slf._send_invalid_reaction_feedback(g("room_id"), g("target_event_id"), g("text"))
    elif func == "_expire_matrix_approval_prompt":
        coro = slf._expire_matrix_approval_prompt(g("room_id"), g("target_event_id"),
                                                  _prompt(g("prompt"), ctx))
    elif func == "_expire_matrix_model_picker_prompt":
        coro = slf._expire_matrix_model_picker_prompt(g("room_id"), g("target_event_id"),
                                                      _prompt(g("prompt"), ctx))
    elif func == "_redact_bot_approval_reactions":
        coro = slf._redact_bot_approval_reactions(g("room_id"), _prompt(g("prompt"), ctx))
    elif func == "_redact_bot_model_picker_reactions":
        coro = slf._redact_bot_model_picker_reactions(g("room_id"), _prompt(g("prompt"), ctx))
    else:
        raise RuntimeError(f"unknown func {func}")
    if func in ASYNC_FUNCS:
        return asyncio.run(coro)
    return coro


def _registry(slf):
    for k in REGISTRY_KEYS:
        if hasattr(slf, k):
            return getattr(slf, k)
    return {}


def _prompt(spec, ctx):
    return prompt_double(spec, ctx)


def _event(spec):
    if spec is None:
        return None
    if not isinstance(spec, dict):
        return spec
    def conv(x, key=None):
        if isinstance(x, dict):
            # live reads event.content via .get/isinstance(dict) in these flows;
            # keep the content sub-dict as a plain dict, nest everything else.
            if key == "content":
                return copy.deepcopy(x)
            return types.SimpleNamespace(**{k: conv(v, k) for k, v in x.items()})
        return pyval(x)
    return conv(spec)


# ── seam-name set cross-check (C calls must equal oracle stubs) ──
SEAM_NAMES = [
    "self._is_self_sender", "self._is_duplicate_event", "hash_for_log",
    "self._handle_approval_reaction", "self._handle_model_picker_reaction",
    "self._handle_choice_picker_reaction", "self._handle_picker_reaction", "self._send_reaction",
    "self._schedule_reaction_redaction", "self._claim_reaction_prompt",
    "on_expired", "self._validate_matrix_prompt_reactor",
    "self._send_invalid_reaction_feedback", "self._is_authorized_user",
    "t", "approval_refusal", "refusal_text",
    "self._redact_bot_approval_reactions", "resolve_gateway_approval_card",
    "self._redact_bot_model_picker_reactions", "self.send",
    "self._expire_matrix_approval_prompt",
    "prompt.on_selected", "picker_refusal",
    "self._is_dm_room", "self._matrix_prompt_expired", "self.redact_message",
]


def _c_seams():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "matrix_adapter_flow3.c",
                encoding="utf-8").read()
    return set(re.findall(r'ma3_call\s*\(\s*ctx\s*,\s*"([^"]+)"', csrc))


def cross_check():
    registry = set(SEAM_NAMES)
    cseams = _c_seams()
    if cseams != registry:
        print(f"FATAL oracle_matrix_adapter_flow3: seam set drift C vs registry: "
              f"extra={sorted(cseams - registry)} missing={sorted(registry - cseams)}")
        sys.exit(2)
    used = set()
    for nm, seam_name in _oracle_seam_names().items():
        used.add(seam_name)
    if used != registry:
        print(f"FATAL oracle_matrix_adapter_flow3: seam set drift oracle vs registry: "
              f"extra={sorted(used - registry)} missing={sorted(registry - used)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "matrix_adapter_flow3.jsonl"
    if fix.exists():
        bad = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            bad |= set((json.loads(line).get("in") or {}).get("io") or {})
        bad -= registry
        if bad:
            print(f"FATAL oracle_matrix_adapter_flow3: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)


def _oracle_seam_names():
    out = {}
    for nm in SIBLING_SPECS:
        if nm == "prompt.on_selected":
            out[nm] = nm
        elif nm.startswith("_") or nm in ("redact_message", "send"):
            out[nm] = "self." + nm
        else:
            out[nm] = nm
    return out


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 12:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")
        for k in sorted(keys)[:3]:
            print(f"    {k} want={json.dumps(ws.get(k), ensure_ascii=False)[:200]}")
            print(f"    {k} got ={json.dumps(gs.get(k), ensure_ascii=False)[:200]}")


def main():
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec_in = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec_in.get("func", "")
        inp = rec_in.get("in") or {}
        got = rec_in.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print(f"MISMATCH {func}: harness emitted no result: {got}")
            continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1
            print(f"MISMATCH {func}: oracle drive raised {type(exc).__name__}: {exc}")
            continue
        g = canon(got)
        if g != want:
            m += 1
            report_mismatch(func, want, g, shown)
            shown += 1
    print(f"ORACLE matrix_adapter_flow3: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "matrix_adapter_flow3.jsonl"
    n = bad = 0
    counts = {}
    for line in open(fix, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            out = drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
            continue
        r = out["ret"]
        if not case.get("fc") and isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            bad += 1
            print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
        if isinstance(r, dict) and r.get("message") and \
                r.get("message", "").startswith(("_", "no script")) is False and \
                r.get("__raise__") == "KeyError":
            msg = str(r.get("message", ""))
            for nm in SIBLING_SPECS:
                if nm in msg:
                    bad += 1
                    print(f"SELFTEST MISSING-SPEC {func} case {n}: {msg}")
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            for nm in METHOD_FUNCS:
                if f"'{nm}'" in msg:
                    bad += 1
                    print(f"SELFTEST BINDING {func} case {n}: {msg}")
    print(f"SELFTEST matrix_adapter_flow3: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    low = [f for f, c in counts.items() if c < 6]
    if low:
        print("WARNING funcs with <6 cases:", low)
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
