#!/usr/bin/env python3
"""Oracle for the Feishu adapter remainders FLOW port (C sibling:
src/gateway/platforms/feishu_adapter_flow13.c).

Drives the LIVE plugins/platforms/feishu/adapter.py defs with SCRIPTED seams:
every collaborator is a recording stub fed the same scripts the C harness feeds
its universal seam fa13_call. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the
(always empty) state.

The live defs are AST-extracted from adapter.py; the heavy quarry import chain
is never imported. Fail-closed: missing defs or 0 cases => exit 2. Per-case
wrapper catches BaseException (SystemExit included); scripted raises round-trip
their spec. Summary: 'ORACLE feishu_adapter_flow13: N cases, M mismatches'.
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
from pathlib import PurePosixPath, PurePath
import __future__ as _fut

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = __import__("pathlib").Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "plugins", "platforms", "feishu", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

TARGETS = [
    "_record_webhook_anomaly", "_release_app_lock", "_reschedule_batch_task",
    "_reset_batch_buffers", "_resolve_approval", "_resolve_channel_prompt",
    "_resolve_normalized_message_type", "_resolve_sender_name_from_api",
    "_resolve_sender_profile", "_resolve_update_prompt",
    "_schedule_text_batch_flush", "_schedule_media_batch_flush",
    "_EA_REASON_LABEL", "_EA_SMART_DENY_LINE",
]


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def extract():
    mod = types.ModuleType("fa13_oracle_adapter")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    from typing import Any, Dict, List, Optional, Sequence
    ns.update({
        "Path": PurePosixPath,
        "PurePath": PurePath,
        "json": json,
        "SimpleNamespace": types.SimpleNamespace,
        "Any": Any, "Dict": Dict, "List": List,
        "Optional": Optional, "Sequence": Sequence,
        "FeishuAdapter": None,
        "_FEISHU_WEBHOOK_ANOMALY_TTL_SECONDS": 21600,
        "_FEISHU_WEBHOOK_ANOMALY_THRESHOLD": 25,
        "_FEISHU_APP_LOCK_SCOPE": "feishu-app-id",
        "_FEISHU_SENDER_NAME_TTL_SECONDS": 600,
        "FEISHU_AVAILABLE": False,
        "logger": types.SimpleNamespace(
            warning=lambda *a, **k: None, error=lambda *a, **k: None,
            info=lambda *a, **k: None, debug=lambda *a, **k: None,
            exception=lambda *a, **k: None),
        "print": lambda *a, **k: None,
    })
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    done = set()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in TARGETS:
            c = copy.copy(node); c.decorator_list = []
            exec_node(src, c, ns, SRC); done.add(node.name)
        elif isinstance(node, ast.ClassDef) and node.name == "FeishuAdapter":
            for ch in node.body:
                if isinstance(ch, (ast.FunctionDef, ast.AsyncFunctionDef)) and ch.name in TARGETS:
                    c = copy.copy(ch); c.decorator_list = []
                    exec_node(src, c, ns, SRC); done.add(ch.name)
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in TARGETS and node.name not in done:
            c = copy.copy(node); c.decorator_list = []
            exec_node(src, c, ns, SRC); done.add(node.name)
    missing = [t for t in TARGETS if t not in ns]
    if missing:
        print(f"FATAL oracle_feishu_adapter_flow13: adapter missing targets: {missing}")
        sys.exit(2)
    return ns


NS = extract()
LIVE_NS = dict(NS)

# ── seam names regex-extracted from the C port (SEAM-NAME SET EQUALITY) ──
_CSRC = open(_REPO / "src" / "gateway" / "platforms" / "feishu_adapter_flow13.c",
             encoding="utf-8").read()
SEAMED = set(re.findall(r'fa13_call\(\s*ctx,\s*"([^"]+)"', _CSRC))


# ── canonical projection ──
class MsgType:
    def __init__(self, value): object.__setattr__(self, "value", value)
    def __repr__(self): return f"MsgType({self.value!r})"
    def __eq__(self, other): return isinstance(other, MsgType) and self.value == other.value
    def __hash__(self): return hash(("MsgType", self.value))


def canon(v):
    def conv(x):
        if isinstance(x, BaseException):
            return str(x)
        if isinstance(x, MsgType):
            return x.value
        if isinstance(x, (bytes, bytearray)):
            return bytes(x).decode("utf-8", "replace")
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, (frozenset, set)):
            return sorted(conv(i) for i in x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, PurePath):
            return str(x)
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception hierarchy ──
EXC_CLASSES = {
    "Exception": Exception, "BaseException": BaseException,
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "OSError": OSError, "IOError": OSError,
    "FileNotFoundError": FileNotFoundError, "IsADirectoryError": IsADirectoryError,
    "PermissionError": PermissionError, "TimeoutError": TimeoutError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeError": UnicodeError,
    "JSONDecodeError": json.JSONDecodeError,
    "LookupError": LookupError, "StopIteration": StopIteration, "IndexError": IndexError,
    "ZeroDivisionError": ZeroDivisionError, "AssertionError": AssertionError,
    "NotImplementedError": NotImplementedError, "SyntaxError": SyntaxError,
    "Warning": Warning, "ArithmeticError": ArithmeticError,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "CancelledError": type("CancelledError", (BaseException,), {}),
}
BASE_EXC = {"BaseException", "SystemExit", "KeyboardInterrupt", "GeneratorExit", "CancelledError"}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = (BaseException,) if kind in BASE_EXC else (Exception,)
        cls = type(kind, cls, {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── seam argument projections (MUST match fa13_call args in the C port) ──
ARGMAP = {
    "time_time": lambda: {},
    "anomaly_set": lambda remote_ip, count, status, first_seen: {
        "remote_ip": remote_ip, "count": canon(count), "status": canon(status),
        "first_seen": canon(first_seen)},
    "release_scoped_lock": lambda scope, identity: {"scope": scope, "identity": canon(identity)},
    "clear_app_lock_identity": lambda: {},
    "task_done": lambda: {},
    "task_cancel": lambda: {},
    "flush_fn": lambda key: {"key": canon(key)},
    "create_task": lambda: {},
    "clear_text_batches": lambda: {},
    "clear_text_counts": lambda: {},
    "clear_media_batches": lambda: {},
    "pop_validated_prompt_state": lambda ident, label, open_id, chat_id, user_id, union_id, operator_repr: {
        "ident": canon(ident), "label": canon(label), "open_id": canon(open_id),
        "chat_id": canon(chat_id), "user_id": canon(user_id), "union_id": canon(union_id),
        "operator_repr": canon(operator_repr)},
    "resolve_gateway_approval_card": lambda session_key, choice, request_id, actor_id, extra: {
        "session_key": canon(session_key), "choice": canon(choice),
        "request_id": canon(request_id), "actor_id": canon(actor_id), "extra": canon(extra)},
    "t": lambda key: {"key": canon(key)},
    "send": lambda chat_id, message: {"chat_id": canon(chat_id), "message": canon(message)},
    "resolve_channel_prompt": lambda extra, chat_id, parent_id: {
        "extra": canon(extra), "chat_id": canon(chat_id), "parent_id": canon(parent_id)},
    "resolve_media_message_type": lambda media_type, default: {
        "media_type": canon(media_type), "default": canon(default)},
    "get_cached_sender_name": lambda trimmed: {"trimmed": canon(trimmed)},
    "fetch_bot_names": lambda ids: {"ids": canon(ids)},
    "sender_cache_set": lambda oid, name, expires: {
        "oid": canon(oid), "name": canon(name), "expires": canon(expires)},
    "sender_cache_get": lambda trimmed: {"trimmed": canon(trimmed)},
    "build_get_user_request": lambda trimmed, id_type: {
        "trimmed": canon(trimmed), "id_type": canon(id_type)},
    "run_blocking": lambda request: {"request": canon(request)},
    "resolve_sender_name_from_api": lambda sender_id, is_bot: {
        "sender_id": canon(sender_id), "is_bot": canon(is_bot)},
    "write_update_prompt_response": lambda answer: {"answer": canon(answer)},
    "reschedule_batch_task": lambda kind, key: {"kind": canon(kind), "key": canon(key)},
}

AWAIT_SEAMS = {"send", "resolve_sender_name_from_api", "fetch_bot_names"}


# ── per-case script backend (mirrors C fa13_call exactly) ──
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


def _install(obj, name, value):
    object.__setattr__(obj, name, value)


class _Self:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)


def make_recorder(ctx, nm):
    argfn = ARGMAP[nm]
    if nm in AWAIT_SEAMS:
        async def af(*a, **k):
            return ctx.call(nm, argfn(*a, **k))
        return af
    def sf(*a, **k):
        return ctx.call(nm, argfn(*a, **k))
    return sf


def _stub_pkg(name):
    m = types.ModuleType(name); m.__path__ = []
    sys.modules[name] = m
    return m


class _AnomalyDict:
    def __init__(self, ctx, prior):
        object.__setattr__(self, "_ctx", ctx); object.__setattr__(self, "_prior", prior)
    def get(self, key, default=None):
        p = object.__getattribute__(self, "_prior")
        return default if p is None else p
    def __setitem__(self, key, val):
        ctx = object.__getattribute__(self, "_ctx")
        ctx.call("anomaly_set", {"remote_ip": key, "count": canon(val[0]),
                                 "status": canon(val[1]), "first_seen": canon(val[2])})


class _LockSelf:
    def __init__(self, ctx, identity):
        object.__setattr__(self, "_ctx", ctx); object.__setattr__(self, "_id", identity)
    @property
    def _app_lock_identity(self):
        return object.__getattribute__(self, "_id")
    @_app_lock_identity.setter
    def _app_lock_identity(self, value):
        object.__getattribute__(self, "_ctx").call("clear_app_lock_identity", {})
        object.__setattr__(self, "_id", value)


class _Task:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    def done(self): return object.__getattribute__(self, "_ctx").call("task_done", {})
    def cancel(self): return object.__getattribute__(self, "_ctx").call("task_cancel", {})


class _TaskMap:
    def __init__(self, ctx, has_prior):
        object.__setattr__(self, "_task", _Task(ctx) if has_prior else None)
    def get(self, key, default=None):
        t = object.__getattribute__(self, "_task")
        return t if t is not None else default
    def __setitem__(self, key, val): pass


class _ClearDict:
    def __init__(self, ctx, name):
        object.__setattr__(self, "_ctx", ctx); object.__setattr__(self, "_nm", name)
    def clear(self):
        object.__getattribute__(self, "_ctx").call(object.__getattribute__(self, "_nm"), {})


class _SenderCache:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    def get(self, key, default=None):
        return object.__getattribute__(self, "_ctx").call("sender_cache_get", {"trimmed": key})
    def __setitem__(self, key, val):
        object.__getattribute__(self, "_ctx").call(
            "sender_cache_set", {"oid": key, "name": canon(val[0]), "expires": canon(val[1])})


class _MTGroup:
    TEXT = MsgType("text")
    COMMAND = MsgType("command")
    PHOTO = MsgType("photo")
    VIDEO = MsgType("video")
    AUDIO = MsgType("audio")
    VOICE = MsgType("voice")
    DOCUMENT = MsgType("document")
    STICKER = MsgType("sticker")
    LOCATION = MsgType("location")


MT = _MTGroup

SELF = None
_KIND = [""]


def install_recorders(ctx, target):
    global SELF
    NS.clear(); NS.update(LIVE_NS)
    NS["print"] = lambda *a, **k: None
    NS["logger"] = types.SimpleNamespace(
        warning=lambda *a, **k: None, error=lambda *a, **k: None,
        info=lambda *a, **k: None, debug=lambda *a, **k: None,
        exception=lambda *a, **k: None)
    NS["time"] = types.SimpleNamespace(time=make_recorder(ctx, "time_time"))
    NS["t"] = make_recorder(ctx, "t")
    NS["MessageType"] = MT
    SELF = _Self(ctx)

    if target == "_record_webhook_anomaly":
        pass  # SELF dict installed in live_call (needs per-case prior)
    elif target == "_release_app_lock":
        NS["release_scoped_lock"] = make_recorder(ctx, "release_scoped_lock")
    elif target == "_reschedule_batch_task":
        NS["asyncio"] = types.SimpleNamespace(create_task=lambda c: ctx.call("create_task", {}))
    elif target == "_reset_batch_buffers":
        pass  # SELF clear-dicts installed in live_call
    elif target == "_resolve_approval":
        def pop_rec(**kw):
            return ctx.call("pop_validated_prompt_state", {
                "ident": canon(kw.get("ident")), "label": canon(kw.get("label")),
                "open_id": canon(kw.get("open_id")), "chat_id": canon(kw.get("chat_id")),
                "user_id": canon(kw.get("user_id")), "union_id": canon(kw.get("union_id")),
                "operator_repr": canon(kw.get("operator_repr"))})
        _install(SELF, "_pop_validated_prompt_state", pop_rec)
        _install(SELF, "_approval_state", {})
        _install(SELF, "send", make_recorder(ctx, "send"))
        _install(SELF, "config", types.SimpleNamespace(extra={}))
        _stub_pkg("tools")
        ta = _stub_pkg("tools.approval")
        ta.resolve_gateway_approval_card = make_recorder(ctx, "resolve_gateway_approval_card")
    elif target == "_resolve_channel_prompt":
        _install(SELF, "config", types.SimpleNamespace(extra={}))
        _stub_pkg("gateway")
        _stub_pkg("gateway.platforms")
        gb = _stub_pkg("gateway.platforms.base")
        gb.resolve_channel_prompt = make_recorder(ctx, "resolve_channel_prompt")
    elif target == "_resolve_normalized_message_type":
        _install(SELF, "_resolve_media_message_type", make_recorder(ctx, "resolve_media_message_type"))
    elif target == "_resolve_sender_name_from_api":
        _install(SELF, "_get_cached_sender_name", make_recorder(ctx, "get_cached_sender_name"))
        _install(SELF, "_fetch_bot_names", make_recorder(ctx, "fetch_bot_names"))
        _install(SELF, "_sender_name_cache", _SenderCache(ctx))
        async def rb(func, request):
            r = ctx.call("run_blocking", {"request": canon(request)})
            if isinstance(r, dict):
                user = types.SimpleNamespace(**{k: v for k, v in (r.get("user") or {}).items()})
                return types.SimpleNamespace(
                    success=lambda: bool(r.get("success")),
                    data=types.SimpleNamespace(user=user))
            return r
        _install(SELF, "_run_blocking", rb)
        def make_getuser():
            class B:
                def user_id(self, v): object.__setattr__(self, "_uid", v); return self
                def user_id_type(self, v): object.__setattr__(self, "_idt", v); return self
                def build(self):
                    return ctx.call("build_get_user_request", {
                        "trimmed": object.__getattribute__(self, "_uid"),
                        "id_type": object.__getattribute__(self, "_idt")})
            class G:
                @staticmethod
                def builder(): return B()
            return G
        _stub_pkg("lark_oapi")
        _stub_pkg("lark_oapi.api")
        _stub_pkg("lark_oapi.api.contact")
        v3 = _stub_pkg("lark_oapi.api.contact.v3")
        v3.GetUserRequest = make_getuser()
    elif target == "_resolve_sender_profile":
        _install(SELF, "_resolve_sender_name_from_api", make_recorder(ctx, "resolve_sender_name_from_api"))
    elif target == "_resolve_update_prompt":
        def pop_rec2(**kw):
            return ctx.call("pop_validated_prompt_state", {
                "ident": canon(kw.get("ident")), "label": canon(kw.get("label")),
                "open_id": canon(kw.get("open_id")), "chat_id": canon(kw.get("chat_id")),
                "user_id": canon(kw.get("user_id")), "union_id": canon(kw.get("union_id")),
                "operator_repr": canon(kw.get("operator_repr"))})
        _install(SELF, "_pop_validated_prompt_state", pop_rec2)
        _install(SELF, "_update_prompt_state", {})
        _install(SELF, "_write_update_prompt_response", make_recorder(ctx, "write_update_prompt_response"))
    elif target in ("_schedule_text_batch_flush", "_schedule_media_batch_flush"):
        _KIND[0] = "text" if target == "_schedule_text_batch_flush" else "media"
        def rbt(task_map, k, flush_fn):
            return ctx.call("reschedule_batch_task", {"kind": _KIND[0], "key": canon(k)})
        _install(SELF, "_reschedule_batch_task", rbt)
        _install(SELF, "_pending_text_batch_tasks", {})
        _install(SELF, "_flush_text_batch", lambda k: None)
        _install(SELF, "_pending_media_batch_tasks", {})
        _install(SELF, "_flush_media_batch", lambda k: None)
    elif target in ("_EA_REASON_LABEL", "_EA_SMART_DENY_LINE"):
        pass


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


def _run(x):
    import inspect
    if inspect.isawaitable(x):
        return asyncio.run(x)
    return x


def live_call(func, args, ctx):
    g = args.get
    if func == "_record_webhook_anomaly":
        _install(SELF, "_webhook_anomaly_counts", _AnomalyDict(ctx, g("prior")))
        return NS[func](SELF, g("remote_ip", ""), g("status", ""))
    if func == "_release_app_lock":
        ls = _LockSelf(ctx, g("app_lock_identity", ""))
        return NS[func](ls)
    if func == "_reschedule_batch_task":
        tm = _TaskMap(ctx, bool(g("has_prior", False)))
        def flush_fn(k): return ctx.call("flush_fn", {"key": canon(k)})
        return NS[func](tm, g("key", ""), flush_fn)
    if func == "_reset_batch_buffers":
        _install(SELF, "_pending_text_batches", _ClearDict(ctx, "clear_text_batches"))
        _install(SELF, "_pending_text_batch_counts", _ClearDict(ctx, "clear_text_counts"))
        _install(SELF, "_pending_media_batches", _ClearDict(ctx, "clear_media_batches"))
        return NS[func](SELF)
    if func == "_resolve_approval":
        _install(SELF, "config", types.SimpleNamespace(extra=g("extra")))
        return NS[func](SELF, g("ident"), g("choice", ""), g("user_name", ""),
                        open_id=g("open_id", ""), chat_id=g("chat_id", ""),
                        user_id=g("user_id", ""), union_id=g("union_id", ""))
    if func == "_resolve_channel_prompt":
        _install(SELF, "config", types.SimpleNamespace(extra=g("extra")))
        return NS[func](SELF, g("chat_id", ""), g("parent_id", None))
    if func == "_resolve_normalized_message_type":
        normalized = types.SimpleNamespace(preferred_message_type=g("preferred", ""))
        return NS[func](SELF, normalized, g("media_types", []))
    if func == "_resolve_sender_name_from_api":
        client = None
        if g("has_client"):
            client = types.SimpleNamespace(
                contact=types.SimpleNamespace(v3=types.SimpleNamespace(
                    user=types.SimpleNamespace(get=lambda: None))))
        _install(SELF, "_client", client)
        return NS[func](SELF, g("sender_id"), is_bot=bool(g("is_bot", False)))
    if func == "_resolve_sender_profile":
        sd = types.SimpleNamespace(**{k: g("sender_id", {}).get(k) for k in ("open_id", "user_id", "union_id")})
        return NS[func](SELF, sd, is_bot=bool(g("is_bot", False)))
    if func == "_resolve_update_prompt":
        return NS[func](SELF, g("ident"), g("answer", ""), g("user_name", ""),
                        open_id=g("open_id", ""), chat_id=g("chat_id", ""),
                        user_id=g("user_id", ""), union_id=g("union_id", ""))
    if func == "_schedule_text_batch_flush":
        return NS[func](SELF, g("key", ""))
    if func == "_schedule_media_batch_flush":
        return NS[func](SELF, g("key", ""))
    if func == "_EA_REASON_LABEL":
        return NS[func](SELF)
    if func == "_EA_SMART_DENY_LINE":
        return NS[func](SELF)
    raise RuntimeError(f"unknown func {func}")


def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    install_recorders(ctx, func)
    try:
        ret = _run(live_call(func, args, ctx))
    except BaseException as exc:
        ret = exc_spec(exc, ctx)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": {}}


# ── cross-checks ──
def cross_check():
    missing_argmap = sorted(SEAMED - set(ARGMAP))
    if missing_argmap:
        print(f"FATAL oracle_feishu_adapter_flow13: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra = sorted(set(ARGMAP) - SEAMED)
    if extra:
        print(f"FATAL oracle_feishu_adapter_flow13: ARGMAP seams absent from C: {extra}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow13.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func"))
        bad = fix_keys - SEAMED
        if bad:
            print(f"FATAL oracle_feishu_adapter_flow13: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        missing_funcs = sorted(set(TARGETS) - fix_funcs)
        if missing_funcs:
            print(f"FATAL oracle_feishu_adapter_flow13: targets without fixture cases: {missing_funcs}")
            sys.exit(2)
        unknown_funcs = sorted(fix_funcs - set(TARGETS))
        if unknown_funcs:
            print(f"FATAL oracle_feishu_adapter_flow13: fixture funcs not in TARGETS: {unknown_funcs}")
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
    if shown >= 200:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:600]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:600]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:600]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:600]}")


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
    print(f"ORACLE feishu_adapter_flow13: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow13.jsonl"
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
        if case.get("_note") == "fail-closed":
            continue
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            bad += 1
            print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            for nm in TARGETS:
                if f"'{nm}'" in msg:
                    bad += 1
                    print(f"SELFTEST BINDING GAP {func} case {n}: {msg}")
                    break
    print(f"SELFTEST feishu_adapter_flow13: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
