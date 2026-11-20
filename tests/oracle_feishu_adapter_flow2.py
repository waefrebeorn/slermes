#!/usr/bin/env python3
"""Oracle for the Feishu adapter inbound-event + card-action FLOW remainder
port (src/gateway/platforms/feishu_adapter_flow2.c).

Drives the LIVE plugins/platforms/feishu/adapter.py event-handler / card-action
cluster with SCRIPTED seams: every SEAMED sibling / foreign call is a recording
stub fed the same scripts the C harness feeds its universal seam fa2_call.
Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace") plus the final return ("ret") and the mutated state ("state").

The live defs are AST-extracted from the FeishuAdapter class body (module
constants seeded, Assign AND AnnAssign, plain-Name targets only; logger re-set
AFTER seeding). Every exec namespace is a registered sys.modules module
(py3.14). The heavy quarry import chain (aiohttp / lark_oapi / gateway.*) is
never imported; the foreign approval_actor names and time.time are stubbed.

Mode table (mirrors the C port): every ported function runs its REAL body when
it is the case target; all sibling / foreign calls are SEAMED (scripted). A
ported sibling called BY the target is scripted everywhere except when it is
itself the target. Logger levels are recorded seams {fmt, args, kwargs}.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included); scripted raises round-trip their spec
bare. Summary: 'ORACLE feishu_adapter_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- self is a SimpleNamespace double; action.value stays a plain dict.
- loop double {is_closed:b}; SDK availability via args.sdk; response projects
  to {card: null|{type:"raw",data:...}} (+ toast for _card_toast).
- _validate_card_action 5-tuple -> list; ids travel as strings.
- Integral floats == ints (canon folds). dict comparison order-insensitive;
  trace event LIST order is semantic.
"""
import ast
import copy
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "plugins", "platforms", "feishu", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

TARGETS = [
    "format_message", "_on_message_read_event", "_on_p2p_chat_entered",
    "_on_message_recalled", "_on_bot_membership_change", "_on_bot_added_to_chat",
    "_on_bot_removed_from_chat", "_loop_accepts_callbacks",
    "_is_interactive_operator_authorized", "_card_response", "_card_toast",
    "_validate_card_action", "_handle_approval_card_action",
    "_handle_update_prompt_card_action", "_pop_validated_prompt_state",
    "_is_card_action_duplicate", "_on_reaction_event", "_on_card_action_trigger",
]
STATIC = {"_loop_accepts_callbacks", "_card_response"}
# Method seams bound on the self double (ported siblings + seamed-only helpers).
METHOD_SEAMS = [
    "_on_bot_membership_change", "_loop_accepts_callbacks",
    "_is_interactive_operator_authorized", "_card_response", "_card_toast",
    "_validate_card_action", "_handle_approval_card_action",
    "_handle_update_prompt_card_action", "_handle_clarify_card_action",
    "_handle_choice_picker_card_action", "_submit_on_loop",
    "_get_cached_sender_name", "_is_sender_authorized", "_handle_reaction_event",
    "_handle_card_action_event", "_resolve_approval", "_resolve_update_prompt",
    "_build_resolved_approval_card", "_build_resolved_update_prompt_card",
]
FOREIGN_SEAMS = ["approval_refusal", "refusal_text", "update_prompt_refusal"]
LOGGER_SEAMS = ["logger_debug", "logger_info", "logger_warning"]


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
    mod = new_module("fa2_oracle_adapter")
    ns = mod.__dict__
    import collections
    import logging
    import threading
    import time as _time
    from typing import Any, Dict, List, Optional, Sequence
    ns.update({"re": re, "json": json, "collections": collections,
               "threading": threading, "logging": logging, "os": os, "time": _time,
               "Any": Any, "Dict": Dict, "List": List, "Optional": Optional,
               "Sequence": Sequence, "OrderedDict": collections.OrderedDict})
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # module constants (plain-Name Assign / AnnAssign targets only)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    # the 18 target methods from the FeishuAdapter class body (strip decorators)
    found = set()
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "FeishuAdapter":
            for ch in node.body:
                if isinstance(ch, (ast.FunctionDef, ast.AsyncFunctionDef)) and ch.name in TARGETS:
                    c = copy.copy(ch)
                    c.decorator_list = []
                    exec_node(src, c, ns, SRC)
                    found.add(ch.name)
    missing_targets = [t for t in TARGETS if t not in found]
    if missing_targets:
        print(f"FATAL oracle_feishu_adapter_flow2: adapter missing targets: {missing_targets}")
        sys.exit(2)
    # foreign / SDK / logger defaults (overridden per case)
    for nm in FOREIGN_SEAMS:
        ns.setdefault(nm, None)
    ns["P2CardActionTriggerResponse"] = None
    ns["CallBackCard"] = None
    ns["CallBackToast"] = None
    ns["logger"] = _SilentLogger()
    required = TARGETS + ["_APPROVAL_CHOICE_MAP", "_FEISHU_CARD_ACTION_DEDUP_TTL_SECONDS"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_feishu_adapter_flow2: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()


# ── SEAMED names: regex-extracted from the C port's fa2_call sites ──
SEAMED = set(re.findall(r'fa2_call\(\s*ctx,\s*"([^"]+)"',
                        open(_REPO / "src" / "gateway" / "platforms" / "feishu_adapter_flow2.c",
                             encoding="utf-8").read()))
SEAMED |= set(re.findall(r'fa2_log\(\s*ctx,\s*"([^"]+)"',
                         open(_REPO / "src" / "gateway" / "platforms" / "feishu_adapter_flow2.c",
                              encoding="utf-8").read()))
REAL_DEFS = {nm: NS[nm] for nm in SEAMED if nm in NS and nm in TARGETS}


# ── canonical projection ──
def canon(v):
    def conv(x):
        if isinstance(x, frozenset):
            return sorted(conv(i) for i in x)
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection ──
EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "Exception": Exception, "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": type("CancelledError", (Exception,), {}),
    "StopIteration": StopIteration,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    return cls(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── attribute / loop projections for seam args ──
def proj_attr(o):
    if isinstance(o, types.SimpleNamespace):
        return {k: proj_attr(v) for k, v in vars(o).items()}
    if isinstance(o, dict):
        return {k: proj_attr(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [proj_attr(i) for i in o]
    return o


def proj_loop(l):
    if l is None:
        return None
    return getattr(l, "_raw", None)


ARGMAP = {
    "logger_debug": lambda fmt, *a, **k: {"fmt": fmt, "args": list(a), "kwargs": k},
    "logger_info": lambda fmt, *a, **k: {"fmt": fmt, "args": list(a), "kwargs": k},
    "logger_warning": lambda fmt, *a, **k: {"fmt": fmt, "args": list(a), "kwargs": k},
    "_on_bot_membership_change": lambda data, verb: {"data": proj_attr(data), "verb": verb},
    "_loop_accepts_callbacks": lambda loop: {"loop": proj_loop(loop)},
    "_is_interactive_operator_authorized": lambda open_id, *, user_id="", union_id="", chat_id="":
        {"open_id": open_id, "user_id": user_id, "union_id": union_id, "chat_id": chat_id},
    "_card_response": lambda card_data=None: {"card_data": card_data},
    "_card_toast": lambda text: {"text": text},
    "_validate_card_action": lambda *, event, state, label, ident:
        {"event": proj_attr(event), "state": state, "label": label, "ident": ident},
    "_handle_approval_card_action": lambda *, event, action_value, loop:
        {"event": proj_attr(event), "action_value": action_value, "loop": proj_loop(loop)},
    "_handle_update_prompt_card_action": lambda *, event, action_value, loop:
        {"event": proj_attr(event), "action_value": action_value, "loop": proj_loop(loop)},
    "_handle_clarify_card_action": lambda *, event, action_value, loop:
        {"event": proj_attr(event), "action_value": action_value, "loop": proj_loop(loop)},
    "_handle_choice_picker_card_action": lambda *, event, action_value, loop:
        {"event": proj_attr(event), "action_value": action_value, "loop": proj_loop(loop)},
    "_submit_on_loop": lambda loop, coro: {"loop": proj_loop(loop), "coro": coro},
    "_get_cached_sender_name": lambda sender_id: {"sender_id": sender_id},
    "_is_sender_authorized": lambda user_id, chat_type=None, chat_id=None, **k:
        {"user_id": user_id, "chat_type": chat_type, "chat_id": chat_id},
    "_handle_reaction_event": lambda event_type, data: {"event_type": event_type, "data": proj_attr(data)},
    "_handle_card_action_event": lambda data: {"data": proj_attr(data)},
    "_resolve_approval": lambda **k: dict(k),
    "_resolve_update_prompt": lambda prompt_id, answer, user_name, *, open_id="", chat_id="", user_id="", union_id="":
        {"prompt_id": prompt_id, "answer": answer, "user_name": user_name, "open_id": open_id,
         "chat_id": chat_id, "user_id": user_id, "union_id": union_id},
    "_build_resolved_approval_card": lambda *, choice, user_name, body=None:
        {"choice": choice, "user_name": user_name, "body": body},
    "_build_resolved_update_prompt_card": lambda *, answer, user_name:
        {"answer": answer, "user_name": user_name},
    "approval_refusal": lambda session_key, actor_id, *, extra=None, request_id=None, choice=None, **k:
        {"session_key": session_key, "actor_id": actor_id, "extra": extra,
         "request_id": request_id, "choice": choice},
    "refusal_text": lambda requester: {"requester": requester},
    "update_prompt_refusal": lambda actor_id, *, extra=None, **k: {"actor_id": actor_id, "extra": extra},
    "time_time": lambda: {},
}
BUILD = {
    "_card_response": lambda r: types.SimpleNamespace(**r) if isinstance(r, dict) else None,
    "_validate_card_action": lambda r: tuple(r) if r is not None else None,
}


# ── per-case script backend (mirrors the C fa2_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.scripted = []

    def _raise(self, name, args, spec, exc):
        self.trace.append({"name": name, "args": canon(args), "ret": spec})
        self.scripted.append((exc, spec))
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            msg = f"no script for {name}"
            self._raise(name, args, {"__raise__": "RuntimeError", "message": msg}, RuntimeError(msg))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                msg = f"script exhausted for {name}"
                self._raise(name, args, {"__raise__": "RuntimeError", "message": msg}, RuntimeError(msg))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                rs = used["raise"]
                spec = {"__raise__": rs.get("type", "RuntimeError"), "message": rs.get("message", "")}
                self._raise(name, args, spec, make_exc(rs))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return ret


def make_recorder(ctx, nm):
    argfn = ARGMAP[nm]
    build = BUILD.get(nm)

    def f(*a, **k):
        raw = ctx.call(nm, argfn(*a, **k))
        return build(raw) if build else raw
    return f


class RecLogger:
    def __init__(self, ctx):
        self.debug = make_recorder(ctx, "logger_debug")
        self.info = make_recorder(ctx, "logger_info")
        self.warning = make_recorder(ctx, "logger_warning")


def install_recorders(ctx, target):
    for nm in REAL_DEFS:
        NS[nm] = REAL_DEFS[nm]
    for nm in FOREIGN_SEAMS:
        NS[nm] = make_recorder(ctx, nm)
    NS["time"] = types.SimpleNamespace(time=make_recorder(ctx, "time_time"))
    NS["logger"] = RecLogger(ctx)
    NS["P2CardActionTriggerResponse"] = lambda: types.SimpleNamespace(card=None)
    NS["CallBackCard"] = lambda: types.SimpleNamespace(type=None, data=None)
    NS["CallBackToast"] = lambda: types.SimpleNamespace(type=None, content=None)


# ── argument materialization ──
def mat_attr(d, _key=None):
    if isinstance(d, dict):
        if _key == "value":          # action.value stays a plain dict
            return copy.deepcopy(d)
        return types.SimpleNamespace(**{k: mat_attr(v, k) for k, v in d.items()})
    if isinstance(d, list):
        return [mat_attr(i) for i in d]
    return d


def mat_loop(d):
    if d is None:
        return None
    b = bool(d.get("is_closed", False))
    ns = types.SimpleNamespace(is_closed=lambda: b)
    ns._raw = d
    return ns


def build_self(sj, target, rec):
    sj = sj or {}
    slf = types.SimpleNamespace()
    slf._loop = mat_loop(sj.get("_loop"))
    slf._chat_info_cache = dict(sj.get("_chat_info_cache") or {})
    slf._card_action_tokens = dict(sj.get("_card_action_tokens") or {})
    slf._approval_state = dict(sj.get("_approval_state") or {})
    slf._update_prompt_state = dict(sj.get("_update_prompt_state") or {})
    slf._admins = set(sj.get("_admins") or [])
    slf._allowed_group_users = set(sj.get("_allowed_group_users") or [])
    slf.config = types.SimpleNamespace(extra=dict((sj.get("config") or {}).get("extra") or {}))
    # class-level constant referenced through self (adapter_choice_cards.py)
    slf._CARD_ACTION_ROUTES = (
        ("hermes_action", "_handle_approval_card_action"),
        ("hermes_update_prompt_action", "_handle_update_prompt_card_action"),
        ("hermes_clarify_action", "_handle_clarify_card_action"),
        ("hermes_choice_picker_action", "_handle_choice_picker_card_action"),
    )
    for nm in METHOD_SEAMS:
        if nm == target:
            continue
        setattr(slf, nm, rec(nm, slf))
    return slf


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


def _set_sdk(args):
    sdk = args.get("sdk") or {}
    NS["P2CardActionTriggerResponse"] = (lambda: types.SimpleNamespace(card=None)) if sdk.get("response", True) else None
    NS["CallBackCard"] = (lambda: types.SimpleNamespace(type=None, data=None)) if sdk.get("card", True) else None
    NS["CallBackToast"] = (lambda: types.SimpleNamespace(type=None, content=None)) if sdk.get("toast", True) else None


# ── per-case live call (materialize + dispatch; recorders installed by caller) ──
def live_call(func, args, state_out, ctx, rec=None):
    if rec is None:
        rec = lambda nm, slf: make_recorder(ctx, nm)
    slf = build_self(args.get("self"), func, rec)
    M = lambda nm: types.MethodType(NS[nm], slf)
    if func == "format_message":
        ret = M(func)(args.get("content", ""))
    elif func == "_on_message_read_event":
        ret = M(func)(mat_attr(args.get("data")))
    elif func == "_on_p2p_chat_entered":
        ret = M(func)(mat_attr(args.get("data")))
    elif func == "_on_message_recalled":
        ret = M(func)(mat_attr(args.get("data")))
    elif func == "_on_bot_membership_change":
        state_out["_chat_info_cache"] = slf._chat_info_cache
        ret = M(func)(mat_attr(args.get("data")), args.get("verb", ""))
    elif func == "_on_bot_added_to_chat":
        ret = M(func)(mat_attr(args.get("data")))
    elif func == "_on_bot_removed_from_chat":
        ret = M(func)(mat_attr(args.get("data")))
    elif func == "_loop_accepts_callbacks":
        ret = NS[func](mat_loop(args.get("loop")))
    elif func == "_is_interactive_operator_authorized":
        ret = M(func)(args.get("open_id", ""), user_id=args.get("user_id", ""),
                      union_id=args.get("union_id", ""), chat_id=args.get("chat_id", ""))
    elif func == "_card_response":
        _set_sdk(args)
        ret = NS[func](card_data=args.get("card_data"))
    elif func == "_card_toast":
        _set_sdk(args)
        ret = M(func)(args.get("text", ""))
    elif func == "_validate_card_action":
        ret = M(func)(event=mat_attr(args.get("event")), state=dict(args.get("state") or {}),
                      label=args.get("label", ""), ident=args.get("ident"))
    elif func == "_handle_approval_card_action":
        ret = M(func)(event=mat_attr(args.get("event")), action_value=dict(args.get("action_value") or {}),
                      loop=mat_loop(args.get("loop")))
    elif func == "_handle_update_prompt_card_action":
        ret = M(func)(event=mat_attr(args.get("event")), action_value=dict(args.get("action_value") or {}),
                      loop=mat_loop(args.get("loop")))
    elif func == "_pop_validated_prompt_state":
        states = dict(args.get("states") or {})
        state_out["states"] = states
        ret = M(func)(states=states, ident=args.get("ident"), label=args.get("label", ""),
                      open_id=args.get("open_id", ""), chat_id=args.get("chat_id", ""),
                      unauthorized_fmt=args.get("unauthorized_fmt", ""),
                      operator_repr=args.get("operator_repr", ""),
                      user_id=args.get("user_id", ""), union_id=args.get("union_id", ""))
    elif func == "_is_card_action_duplicate":
        state_out["_card_action_tokens"] = slf._card_action_tokens
        ret = M(func)(args.get("token", ""))
    elif func == "_on_reaction_event":
        ret = M(func)(args.get("event_type", ""), mat_attr(args.get("data")))
    elif func == "_on_card_action_trigger":
        ret = M(func)(mat_attr(args.get("data")))
    else:
        raise RuntimeError(f"unknown func {func}")
    return ret


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    install_recorders(ctx, func)
    state_out = {}
    try:
        ret = live_call(func, args, state_out, ctx)
    except BaseException as exc:  # SystemExit included
        ret = exc_spec(exc, ctx)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check ──
def cross_check():
    missing_argmap = sorted(SEAMED - set(ARGMAP))
    if missing_argmap:
        print(f"FATAL oracle_feishu_adapter_flow2: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra_argmap = sorted(set(ARGMAP) - SEAMED)
    if extra_argmap:
        print(f"FATAL oracle_feishu_adapter_flow2: ARGMAP seams absent from C: {extra_argmap}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow2.jsonl"
    if fix.exists():
        fix_keys = set()
        fix_funcs = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func"))
        bad = fix_keys - SEAMED
        if bad:
            print(f"FATAL oracle_feishu_adapter_flow2: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        missing_funcs = sorted(set(TARGETS) - fix_funcs)
        if missing_funcs:
            print(f"FATAL oracle_feishu_adapter_flow2: targets without fixture cases: {missing_funcs}")
            sys.exit(2)


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
    print(f"ORACLE feishu_adapter_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow2.jsonl"
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
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            bad += 1
            print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
        # flag AttributeError specs naming a ported method / seam (binding bug)
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            for nm in list(TARGETS) + METHOD_SEAMS:
                if f"'{nm}'" in msg:
                    bad += 1
                    print(f"SELFTEST BINDING GAP {func} case {n}: {msg}")
                    break
    print(f"SELFTEST feishu_adapter_flow2: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()