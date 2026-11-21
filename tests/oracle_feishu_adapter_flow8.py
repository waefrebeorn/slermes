#!/usr/bin/env python3
"""Oracle for the Feishu adapter card/payload/lifecycle/batch/connection FLOW
port (C sibling: src/gateway/platforms/feishu_adapter_flow8.c).

Drives the LIVE plugins/platforms/feishu/adapter.py defs with SCRIPTED seams:
every collaborator is a recording stub fed the same scripts the C harness feeds
its universal seam fa8_call. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the
(always empty) state.

The live defs are AST-extracted from adapter.py (future-annotations compiler
flag; decorators stripped); the heavy quarry import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included); scripted raises round-trip their spec.
Summary: 'ORACLE feishu_adapter_flow8: N cases, M mismatches'.
"""
import ast
import asyncio
import copy
import inspect
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import PurePosixPath

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
    "_build_update_prompt_card", "_build_resolved_approval_card",
    "_build_resolved_update_prompt_card", "_build_outbound_payload",
    "_build_media_post_payload", "_cancel_pending_tasks",
    "_disable_websocket_auto_reconnect", "_clear_webhook_anomaly",
    "_dedup_persist_lock_or_create", "_build_event_handler",
    "_dispatch_inbound_event", "_delayed_flush", "_dispatch_text_batch",
    "_connect_with_retry", "_connect_websocket", "_connect_webhook",
]


def _lit_constant(path, name):
    tree = ast.parse(open(path, encoding="utf-8").read())
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            for t in tgts:
                if isinstance(t, ast.Name) and t.id == name:
                    try:
                        return ast.literal_eval(node.value)
                    except Exception:
                        env = {}
                        return eval(compile(ast.Expression(node.value), path, "eval"), env)
    raise KeyError(name)


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def extract():
    mod = types.ModuleType("fa8_oracle_adapter")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    from typing import Any, Dict, List, Literal, Optional, Sequence
    ns.update({
        "Path": PurePosixPath,
        "SimpleNamespace": types.SimpleNamespace,
        "Any": Any, "Dict": Dict, "List": List, "Literal": Literal,
        "Optional": Optional, "Sequence": Sequence,
        "FeishuAdapter": None,
        "_APPROVAL_LABEL_KEYS": _lit_constant(SRC, "_APPROVAL_LABEL_KEYS"),
        "_FEISHU_CONNECT_ATTEMPTS": _lit_constant(SRC, "_FEISHU_CONNECT_ATTEMPTS"),
        "_FEISHU_WEBHOOK_MAX_BODY_BYTES": _lit_constant(SRC, "_FEISHU_WEBHOOK_MAX_BODY_BYTES"),
    })
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "FeishuAdapter":
            for ch in node.body:
                if isinstance(ch, (ast.FunctionDef, ast.AsyncFunctionDef)) and ch.name in TARGETS:
                    c = copy.copy(ch)
                    c.decorator_list = []
                    exec_node(src, c, ns, SRC)
    missing = [t for t in TARGETS if t not in ns]
    if missing:
        print(f"FATAL oracle_feishu_adapter_flow8: adapter missing targets: {missing}")
        sys.exit(2)
    return ns


NS = extract()

# ── seam names regex-extracted from the C port (SEAM-NAME SET EQUALITY) ──
_CSRC = open(_REPO / "src" / "gateway" / "platforms" / "feishu_adapter_flow8.c",
             encoding="utf-8").read()
SEAMED = set(re.findall(r'fa8_call\(\s*ctx,\s*"([^"]+)"', _CSRC))
REAL_DEFS = {nm: NS[nm] for nm in SEAMED if nm in NS}
PORTED_SIBLINGS = set(SEAMED) & set(TARGETS)

# ── canonical projection ──
class _Token:
    def __init__(self, value): object.__setattr__(self, "v", value)
    def __repr__(self): return repr(self.v)

_TOKENS = {}
def tok(value):
    key = json.dumps(canon(value), sort_keys=True, ensure_ascii=False)
    if key not in _TOKENS:
        _TOKENS[key] = _Token(canon(value))
    return _TOKENS[key]


def canon(v):
    def conv(x):
        if isinstance(x, _Token):
            return conv(x.v)
        if isinstance(x, _Task):
            return conv(x._tid)
        if isinstance(x, _EventDouble):
            return conv({k: v for k, v in vars(x).items() if k != "_ctx"})
        if isinstance(x, _AppDouble):
            return conv(object.__getattribute__(x, "_raw"))
        if isinstance(x, (frozenset, set)):
            return sorted(conv(i) for i in x)
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
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


# ── exception hierarchy (full builtin side) ──
EXC_CLASSES = {
    "Exception": Exception, "BaseException": BaseException,
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "OSError": OSError, "IOError": OSError,
    "LookupError": LookupError, "StopIteration": StopIteration, "IndexError": IndexError,
    "ZeroDivisionError": ZeroDivisionError, "AssertionError": AssertionError,
    "NotImplementedError": NotImplementedError, "SyntaxError": SyntaxError,
    "UnicodeError": UnicodeError, "Warning": Warning, "ArithmeticError": ArithmeticError,
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


# ── seam argument projections (MUST match fa8_call args in the C port) ──
def _t_args(key, **kw):
    d = {"key": key}
    d.update(kw)
    return d


def _card_args(title, template, text, **kw):
    d = {"title": title, "template": template, "text": text}
    if "actions" in kw:
        d["actions"] = kw["actions"]
    return d


def _card_button_args(label, btn_type, body):
    return {"label": label, "type": btn_type, "body": body}


def _gather_args(*tasks, **kw):
    return {"tasks": list(tasks)}


def _sleep_args(delay):
    return {"delay": delay}


def _new_ws_args(app_id, app_secret, log_level, event_handler, domain, extra_ua_tags):
    return {"app_id": app_id, "app_secret": app_secret, "log_level": log_level,
            "event_handler": event_handler, "domain": domain, "extra_ua_tags": extra_ua_tags}


def _bind_args(self, app, host, port, path):
    return {"host": host, "port": port, "path": path}


def _py_dumps_args(obj, **kw):
    return {"obj": obj}


def _logger_args(message, *a, **k):
    return {"message": (message % a) if a else message}


ARGMAP = {
    "add_post": lambda path, handler: {"path": path, "handler": handler},
    "anomaly_pop": lambda key, default=None: {"key": key},
    "async_sleep": _sleep_args,
    "auto_reconnect_set": lambda client, name, value: {"client": client, "name": name, "value": value},
    "bind_listener": _bind_args,
    "build": lambda: {},
    "builder": lambda encrypt_key, verification_token: {"encrypt_key": encrypt_key, "verification_token": verification_token},
    "card": _card_args,
    "card_button": _card_button_args,
    "connect_webhook": lambda: {},
    "connect_websocket": lambda: {},
    "current_task": lambda: {},
    "disable_ws_reconnect": lambda: {},
    "enqueue_media": lambda event: {"event": event},
    "enqueue_text": lambda event: {"event": event},
    "flush_now": lambda key: {"key": key},
    "gather": _gather_args,
    "handle_guards": lambda event: {"event": event},
    "hydrate_bot_identity": lambda: {},
    "is_command": lambda: {"event": None},  # replaced per-case by event double
    "logger_warning": _logger_args,
    "markdown_hint": lambda content: {"content": content},
    "md_post": lambda content: {"content": content},
    "new_lock": lambda: {},
    "new_ws_client": _new_ws_args,
    "prepare_client": lambda: {},
    "py_dumps": _py_dumps_args,
    "register": lambda event, handler: {"event": event, "handler": handler},
    "run_in_executor": lambda client, runner: {"client": client, "runner": runner},
    "self_setattr": lambda name, value: {"name": name, "value": value},
    "should_batch_media": lambda event: {"event": event},
    "stop_webhook_server": lambda: {},
    "t": _t_args,
    "task_cancel": lambda: {"task": None},
    "task_done": lambda: {"task": None},
    "task_map_get": lambda key, default=None: {"key": key},
    "task_map_pop": lambda key, default=None: {"key": key},
    "tasks_clear": lambda: {},
    "tasks_values": lambda: {},
    "web_application": lambda client_max_size: {"client_max_size": client_max_size},
}

# awaitable seam names (live code awaits these)
AWAIT_SEAMS = {
    "gather", "async_sleep", "flush_now", "enqueue_media", "enqueue_text",
    "handle_guards", "hydrate_bot_identity", "stop_webhook_server", "bind_listener",
    "connect_websocket", "connect_webhook",
}

# ── per-case script backend (mirrors C fa8_call exactly) ──
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


# ── carrier doubles ──
class _Self:
    """SELF double: reads of installed attrs are inert (C reads args directly);
    writes fire the self_setattr seam. Methods are installed via object.__setattr__."""
    def __init__(self, ctx):
        object.__setattr__(self, "_ctx", ctx)

    def __setattr__(self, name, value):
        ctx = object.__getattribute__(self, "_ctx")
        ctx.call("self_setattr", {"name": name, "value": canon(value)})
        object.__setattr__(self, name, value)


def _install(obj, name, value):
    object.__setattr__(obj, name, value)


class _AttrSink:
    def __init__(self, ctx, raw, seam):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "_raw", raw)
        object.__setattr__(self, "_seam", seam)

    def __setattr__(self, name, value):
        ctx = object.__getattribute__(self, "_ctx")
        ctx.call(object.__getattribute__(self, "_seam"),
                 {"client": canon(object.__getattribute__(self, "_raw")), "name": name, "value": canon(value)})


class _Task:
    def __init__(self, ctx, tid):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "_tid", tid)

    def done(self):
        return self._ctx.call("task_done", {"task": self._tid})

    def cancel(self):
        return self._ctx.call("task_cancel", {"task": self._tid})

    def __bool__(self):
        return True


class _TaskMap:
    def __init__(self, ctx, ids):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "_ids", ids)

    def values(self):
        ids = self._ctx.call("tasks_values", {})
        return [_Task(self._ctx, i) for i in (ids or [])]

    def clear(self):
        return self._ctx.call("tasks_clear", {})

    def get(self, key, default=None):
        return tok(self._ctx.call("task_map_get", {"key": key}))

    def pop(self, key, default=None):
        return self._ctx.call("task_map_pop", {"key": key})


class _MapPop:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    def pop(self, key, default=None):
        return self._ctx.call("anomaly_pop", {"key": key})


class _EventDouble:
    """Event carrier: data fields live in __dict__; is_command is a class method
    (a recorder) so it never pollutes canon(vars(event))."""
    def __init__(self, ctx, raw):
        object.__setattr__(self, "_ctx", ctx)
        for k, v in (raw or {}).items():
            object.__setattr__(self, k, mat_event(v))

    def is_command(self):
        return self._ctx.call("is_command", {"event": canon(self)})


_REG_MAP = {
    "register_p2_im_message_message_read_v1": ("p2_im_message_message_read_v1", "_on_message_read_event"),
    "register_p2_im_message_receive_v1": ("p2_im_message_receive_v1", "_on_message_event"),
    "register_p2_im_message_reaction_created_v1": ("p2_im_message_reaction_created_v1", "lambda:created"),
    "register_p2_im_message_reaction_deleted_v1": ("p2_im_message_reaction_deleted_v1", "lambda:deleted"),
    "register_p2_card_action_trigger": ("p2_card_action_trigger", "_on_card_action_trigger"),
    "register_p2_im_chat_member_bot_added_v1": ("p2_im_chat_member_bot_added_v1", "_on_bot_added_to_chat"),
    "register_p2_im_chat_member_bot_deleted_v1": ("p2_im_chat_member_bot_deleted_v1", "_on_bot_removed_from_chat"),
    "register_p2_im_chat_access_event_bot_p2p_chat_entered_v1": ("p2_im_chat_access_event_bot_p2p_chat_entered_v1", "_on_p2p_chat_entered"),
    "register_p2_im_message_recalled_v1": ("p2_im_message_recalled_v1", "_on_message_recalled"),
}


def _hid(h):
    if isinstance(h, str):
        return h
    return getattr(h, "__name__", "handler")


class _Chain:
    """Chained event-handler builder double: each register_* fires one seam."""
    def __init__(self, ctx):
        object.__setattr__(self, "_ctx", ctx)

    def __getattr__(self, name):
        ctx = object.__getattribute__(self, "_ctx")
        if name == "build":
            return lambda: ctx.call("build", {})
        if name == "register_p2_customized_event":
            def _custom(event, handler):
                ctx.call("register", {"event": event, "handler": _hid(handler)})
                return self
            return _custom
        if name in _REG_MAP:
            ev, handler_id = _REG_MAP[name]
            def _reg(handler, _ev=ev, _h=handler_id):
                ctx.call("register", {"event": _ev, "handler": _h})
                return self
            return _reg
        return lambda *a, **k: ctx.call("register", {"event": name, "handler": "?"})


class _LoopDouble:
    def __init__(self, ctx, closed, self_obj):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "_closed", closed)
        object.__setattr__(self, "_self", self_obj)

    def is_closed(self):
        return self._closed

    def run_in_executor(self, executor, runner, *a):
        client = object.__getattribute__(self._self, "_ws_client")
        return self._ctx.call("run_in_executor", {"client": canon(client), "runner": "copy_context.run"})


class _AppDouble:
    def __init__(self, ctx, raw=None):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "_raw", raw)
        object.__setattr__(self, "router", types.SimpleNamespace(
            add_post=lambda path, handler: ctx.call("add_post", {"path": path, "handler": handler})))


_ACTIVE_CTX = [None]
_ACTIVE_TARGET = [None]
SELF = None


def make_recorder(ctx, nm):
    argfn = ARGMAP[nm]

    def wrapped(r):
        if nm in ("current_task", "task_map_get"):
            return tok(r)
        if nm == "web_application" and r is not None:
            return _AppDouble(ctx, r)
        return r

    if nm in AWAIT_SEAMS:
        async def af(*a, **k):
            return wrapped(ctx.call(nm, argfn(*a, **k)))
        return af

    def sf(*a, **k):
        return wrapped(ctx.call(nm, argfn(*a, **k)))
    return sf


def install_recorders(ctx, target):
    global SELF
    for nm in SEAMED:
        NS[nm] = (REAL_DEFS[nm] if nm == target else make_recorder(ctx, nm)) if nm in REAL_DEFS else make_recorder(ctx, nm)
    NS["FeishuAdapter"] = None
    # module collaborator doubles
    NS["json"] = types.SimpleNamespace(dumps=make_recorder(ctx, "py_dumps"), loads=json.loads)
    NS["logger"] = types.SimpleNamespace(warning=make_recorder(ctx, "logger_warning"),
                                         debug=make_recorder(ctx, "logger_warning"))
    NS["asyncio"] = types.SimpleNamespace(
        gather=make_recorder(ctx, "gather"),
        sleep=make_recorder(ctx, "async_sleep"),
        current_task=make_recorder(ctx, "current_task"),
        Lock=make_recorder(ctx, "new_lock"),
        CancelledError=type("CancelledError", (BaseException,), {}),
    )
    NS["lark"] = types.SimpleNamespace(LogLevel=types.SimpleNamespace(INFO="INFO"))
    NS["contextvars"] = types.SimpleNamespace(copy_context=lambda: types.SimpleNamespace(run="run"))
    NS["_run_official_feishu_ws_client"] = "official_ws_runner"
    # in-function `from gateway.platforms.shared_ingress import bind_listener`
    for _pkg in ("gateway", "gateway.platforms"):
        _m = types.ModuleType(_pkg)
        _m.__path__ = []
        sys.modules[_pkg] = _m
    _si = types.ModuleType("gateway.platforms.shared_ingress")
    _si.bind_listener = make_recorder(ctx, "bind_listener")
    sys.modules["gateway.platforms.shared_ingress"] = _si
    NS["web"] = types.SimpleNamespace(Application=make_recorder(ctx, "web_application"))
    NS["FeishuWSClient"] = make_recorder(ctx, "new_ws_client")
    NS["bind_listener"] = make_recorder(ctx, "bind_listener")
    # module-level helpers referenced by extracted defs (seam aliases)
    NS["_card"] = make_recorder(ctx, "card")
    NS["_card_button"] = make_recorder(ctx, "card_button")
    NS["_build_markdown_post_payload"] = make_recorder(ctx, "md_post")
    NS["_MARKDOWN_HINT_RE"] = types.SimpleNamespace(search=make_recorder(ctx, "markdown_hint"))
    NS["MessageType"] = types.SimpleNamespace(TEXT="text", PHOTO="photo", VIDEO="video",
                                              DOCUMENT="document", AUDIO="audio", VOICE="voice",
                                              COMMAND="command")
    SELF = _Self(ctx)


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


def mat_event(raw):
    if isinstance(raw, dict):
        return types.SimpleNamespace(**{k: mat_event(v) for k, v in raw.items()})
    if isinstance(raw, list):
        return [mat_event(i) for i in raw]
    return raw


def _run(x):
    if inspect.isawaitable(x):
        return asyncio.run(x)
    return x


# ── per-case live call ──
def live_call(func, args, ctx):
    _ACTIVE_CTX[0] = ctx
    g = args.get

    if func == "_build_update_prompt_card":
        return NS[func](prompt=g("prompt", ""), default=g("default", ""), prompt_id=g("prompt_id", 0))
    if func == "_build_resolved_approval_card":
        return NS[func](choice=g("choice", ""), user_name=g("user_name", ""), body=g("body"))
    if func == "_build_resolved_update_prompt_card":
        return NS[func](answer=g("answer", ""), user_name=g("user_name", ""))
    if func == "_build_outbound_payload":
        return NS[func](SELF, g("content", ""), prefer_post=g("prefer_post", False))
    if func == "_build_media_post_payload":
        return NS[func](SELF, caption=g("caption", ""), media_tag=g("media_tag", {}))

    if func == "_cancel_pending_tasks":
        tm = _TaskMap(ctx, g("tasks", []))
        return NS[func](SELF, tm)

    if func == "_disable_websocket_auto_reconnect":
        client = g("ws_client")
        _install(SELF, "_ws_client", _AttrSink(ctx, client, "auto_reconnect_set") if client is not None else None)
        return NS[func](SELF)

    if func == "_clear_webhook_anomaly":
        _install(SELF, "_webhook_anomaly_counts", _MapPop(ctx))
        return NS[func](SELF, g("remote_ip", ""))

    if func == "_dedup_persist_lock_or_create":
        lock = g("dedup_persist_lock")
        if lock is not None:
            _install(SELF, "_dedup_persist_lock", lock)
        return NS[func](SELF)

    if func == "_build_event_handler":
        NS["EventDispatcherHandler"] = None if not g("handler_available", False) else _EdhBuilder(ctx)
        _install(SELF, "_encrypt_key", g("encrypt_key", ""))
        _install(SELF, "_verification_token", g("verification_token", ""))
        for nm in ("_on_message_read_event", "_on_message_event", "_on_card_action_trigger",
                   "_on_bot_added_to_chat", "_on_bot_removed_from_chat", "_on_p2p_chat_entered",
                   "_on_message_recalled", "_on_drive_comment_event", "_on_meeting_invited_event"):
            _install(SELF, nm, nm.lstrip("_"))
        _install(SELF, "_on_reaction_event", "on_reaction_event")
        return NS[func](SELF)

    if func == "_dispatch_inbound_event":
        ev = _EventDouble(ctx, g("event"))
        _install(SELF, "_enqueue_text_event", make_recorder(ctx, "enqueue_text"))
        _install(SELF, "_should_batch_media_event", make_recorder(ctx, "should_batch_media"))
        _install(SELF, "_enqueue_media_event", make_recorder(ctx, "enqueue_media"))
        _install(SELF, "_handle_message_with_guards", make_recorder(ctx, "handle_guards"))
        return NS[func](SELF, ev)

    if func == "_delayed_flush":
        tm = _TaskMap(ctx, g("task_map", []))
        async def flush_now(key):
            return ctx.call("flush_now", {"key": key})
        return NS[func](tm, g("key", ""), g("delay", 0.0), flush_now)

    if func == "_dispatch_text_batch":
        _install(SELF, "_handle_message_with_guards", make_recorder(ctx, "handle_guards"))
        return NS[func](SELF, mat_event(g("event") or {}))

    if func == "_connect_with_retry":
        _install(SELF, "_connection_mode", g("connection_mode", ""))
        _install(SELF, "_connect_websocket", make_recorder(ctx, "connect_websocket"))
        _install(SELF, "_connect_webhook", make_recorder(ctx, "connect_webhook"))
        _install(SELF, "_disable_websocket_auto_reconnect", make_recorder(ctx, "disable_ws_reconnect"))
        _install(SELF, "_stop_webhook_server", make_recorder(ctx, "stop_webhook_server"))
        return NS[func](SELF)

    if func == "_connect_websocket":
        NS["FEISHU_WEBSOCKET_AVAILABLE"] = g("ws_available", False)
        _install(SELF, "_ws_client", g("ws_client"))
        _install(SELF, "_prepare_client", make_recorder(ctx, "prepare_client"))
        if g("loop_obj") is None:
            _install(SELF, "_loop", None)
        else:
            loop = _LoopDouble(ctx, g("loop_closed", False), SELF)
            _install(SELF, "_loop", loop)
        _install(SELF, "_hydrate_bot_identity", make_recorder(ctx, "hydrate_bot_identity"))
        _install(SELF, "_app_id", g("app_id", ""))
        _install(SELF, "_app_secret", g("app_secret", ""))
        _install(SELF, "_event_handler", g("event_handler"))
        _install(SELF, "_get_sdk_executor", lambda: "EXECUTOR")
        return NS[func](SELF)

    if func == "_connect_webhook":
        NS["FEISHU_WEBHOOK_AVAILABLE"] = g("webhook_available", False)
        _install(SELF, "_prepare_client", make_recorder(ctx, "prepare_client"))
        _install(SELF, "_hydrate_bot_identity", make_recorder(ctx, "hydrate_bot_identity"))
        _install(SELF, "_webhook_path", g("webhook_path", ""))
        _install(SELF, "_webhook_host", g("webhook_host", ""))
        _install(SELF, "_webhook_port", g("webhook_port", 0))
        _install(SELF, "_handle_webhook_request", "handler")
        return NS[func](SELF)

    raise RuntimeError(f"unknown func {func}")


class _EdhBuilder:
    def __init__(self, ctx):
        object.__setattr__(self, "_ctx", ctx)

    def builder(self, encrypt_key, verification_token):
        self._ctx.call("builder", {"encrypt_key": encrypt_key, "verification_token": verification_token})
        return _Chain(self._ctx)


def make_recorder_dynamic(ctx, nm, **extra):
    argfn = ARGMAP[nm]
    def sf():
        d = dict(argfn())
        d.update(extra)
        return ctx.call(nm, d)
    return sf


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    _ACTIVE_TARGET[0] = func
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
        print(f"FATAL oracle_feishu_adapter_flow8: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra = sorted(set(ARGMAP) - SEAMED)
    if extra:
        print(f"FATAL oracle_feishu_adapter_flow8: ARGMAP seams absent from C: {extra}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow8.jsonl"
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
            print(f"FATAL oracle_feishu_adapter_flow8: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        missing_funcs = sorted(set(TARGETS) - fix_funcs)
        if missing_funcs:
            print(f"FATAL oracle_feishu_adapter_flow8: targets without fixture cases: {missing_funcs}")
            sys.exit(2)
        unknown_funcs = sorted(fix_funcs - set(TARGETS))
        if unknown_funcs:
            print(f"FATAL oracle_feishu_adapter_flow8: fixture funcs not in TARGETS: {unknown_funcs}")
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
    if shown >= 15:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:500]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:500]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:500]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:500]}")


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
    print(f"ORACLE feishu_adapter_flow8: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow8.jsonl"
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
    print(f"SELFTEST feishu_adapter_flow8: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    import inspect
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
