#!/usr/bin/env python3
"""Oracle for the Feishu adapter remainders FLOW port (C sibling:
src/gateway/platforms/feishu_adapter_flow14.c).

Drives the LIVE plugins/platforms/feishu/adapter.py defs with SCRIPTED seams:
every collaborator is a recording stub fed the same scripts the C harness feeds
its universal seam fa14_call. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the
(always empty) state.

The live defs are AST-extracted from adapter.py; the heavy quarry import chain
is never imported. Fail-closed: missing defs or 0 cases => exit 2. Per-case
wrapper catches BaseException (SystemExit included); scripted raises round-trip
their spec. Summary: 'ORACLE feishu_adapter_flow14: N cases, M mismatches'.
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
    "_run_blocking", "_run_official_feishu_ws_client", "_send_exec_approval_prompt",
    "_send_interactive_card", "_send_raw_message", "_send_uploaded_file_message",
    "_send_uploaded_key", "_shutdown_sdk_executor", "_standalone_send",
    "_stop_webhook_server", "_submit_on_loop", "_supervise_websocket_thread",
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
    mod = types.ModuleType("fa14_oracle_adapter")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    from typing import Any, Dict, List, Optional, Sequence
    ns.update({
        "Path": PurePosixPath, "PurePath": PurePath, "json": json,
        "SimpleNamespace": types.SimpleNamespace,
        "Any": Any, "Dict": Dict, "List": List,
        "Optional": Optional, "Sequence": Sequence,
        "FeishuAdapter": None,
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
        print(f"FATAL oracle_feishu_adapter_flow14: adapter missing targets: {missing}")
        sys.exit(2)
    return ns


NS = extract()
LIVE_NS = dict(NS)

# ── seam names regex-extracted from the C port (SEAM-NAME SET EQUALITY) ──
_CSRC = open(_REPO / "src" / "gateway" / "platforms" / "feishu_adapter_flow14.c",
             encoding="utf-8").read()
SEAMED = set(re.findall(r'fa14_call\(\s*ctx,\s*"([^"]+)"', _CSRC))


# ── canonical projection ──
def canon(v):
    def conv(x):
        if isinstance(x, BaseException):
            return str(x)
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
    "CancelledError": asyncio.CancelledError,
    "asyncio.CancelledError": asyncio.CancelledError,
}
BASE_EXC = {"BaseException", "SystemExit", "KeyboardInterrupt", "GeneratorExit",
            "CancelledError", "asyncio.CancelledError"}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = (BaseException,) if kind in BASE_EXC else (Exception,)
        cls = type(kind, cls, {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── seam argument projections (MUST match fa14_call args in the C port) ──
ARGMAP = {
    # F1 _run_blocking
    "get_running_loop": lambda: {},
    "get_sdk_executor": lambda: {},
    "run_in_executor": lambda executor, func_args: {
        "executor": canon(executor), "func_args": canon(func_args)},
    # F2 _run_official_feishu_ws_client
    "new_event_loop": lambda: {},
    "set_event_loop": lambda loop: {"loop": canon(_marker(loop))},
    "install_ws_isolation": lambda *a, **k: {},
    "apply_overrides": lambda nonce, reconnect_interval, ping_interval: {
        "nonce": canon(nonce), "reconnect_interval": canon(reconnect_interval),
        "ping_interval": canon(ping_interval)},
    "logger_debug": lambda fmt, *a: {"message": canon((fmt % a) if a else fmt)},
    "start": lambda: {},
    "all_tasks": lambda loop: {"loop": canon(_marker(loop))},
    "task_done": lambda task: {"task": canon(_marker(task))},
    "task_cancel": lambda task: {"task": canon(_marker(task))},
    "gather": lambda *tasks, **kw: {"tasks": canon([_marker(t) for t in tasks])},
    "run_until_complete": lambda coro: {"coro": canon(coro)},
    "loop_stop": lambda: {},
    "loop_close": lambda: {},
    # F3 _send_exec_approval_prompt
    "proxy_error": lambda error: {"error": canon(error)},
    "next_approval": lambda: {},
    "t": lambda key: {"key": canon(key)},
    "card_button": lambda label, style, value: {
        "label": canon(label), "style": canon(style), "value": canon(value)},
    "card": lambda header, template, markdown, actions: {
        "header": canon(header), "template": canon(template),
        "markdown": canon(markdown), "actions": canon(actions)},
    "ea_deadline_line": lambda: {},
    "send_interactive_card": lambda chat_id, card, metadata, failure_message, state_map_kind, state_id, session_key, extra_state: {
        "chat_id": canon(chat_id), "card": canon(card), "metadata": canon(metadata),
        "failure_message": canon(failure_message), "state_map_kind": canon(state_map_kind),
        "state_id": canon(state_id), "session_key": canon(session_key),
        "extra_state": canon(extra_state)},
    "log_warning": lambda fmt, *a: {"message": canon((fmt % a) if a else fmt)},
    # F4 _send_interactive_card
    "json_dumps": lambda value, **kw: {"value": canon(value)},
    "feishu_send_with_retry": lambda chat_id, msg_type, payload, reply_to, metadata: {
        "chat_id": canon(chat_id), "msg_type": canon(msg_type), "payload": canon(payload),
        "reply_to": canon(reply_to), "metadata": canon(metadata)},
    "finalize_send_result": lambda response, failure_message: {
        "response": canon(response), "failure_message": canon(failure_message)},
    "state_set": lambda state_id, session_key, message_id, chat_id, extra_state: {
        "state_id": canon(state_id), "session_key": canon(session_key),
        "message_id": canon(message_id), "chat_id": canon(chat_id),
        "extra_state": canon(extra_state)},
    # F5 _send_raw_message
    "uuid4": lambda: {},
    "build_reply_message_body": lambda content, msg_type, reply_in_thread, uuid_value: {
        "content": canon(content), "msg_type": canon(msg_type),
        "reply_in_thread": canon(reply_in_thread), "uuid_value": canon(uuid_value)},
    "build_reply_message_request": lambda reply_to, body: {
        "reply_to": canon(reply_to), "body": canon(body)},
    "build_create_message_body": lambda receive_id, msg_type, content, uuid_value: {
        "receive_id": canon(receive_id), "msg_type": canon(msg_type),
        "content": canon(content), "uuid_value": canon(uuid_value)},
    "build_create_message_request": lambda receive_id_type, body: {
        "receive_id_type": canon(receive_id_type), "body": canon(body)},
    "run_blocking": lambda method, request: {"method": canon(method), "request": canon(request)},
    # F6 _send_uploaded_file_message
    "path_exists": lambda path: {"path": canon(path)},
    "path_basename": lambda path: {"path": canon(path)},
    "resolve_outbound_file_routing": lambda file_path, requested_message_type: {
        "file_path": canon(file_path), "requested_message_type": canon(requested_message_type)},
    "get_audio_duration_ms": lambda file_path: {"file_path": canon(file_path)},
    "open": lambda path, mode: {"path": canon(path), "mode": canon(mode)},
    "build_file_upload_body": lambda file_type, file_name, duration, **kw: {
        "file_type": canon(file_type), "file_name": canon(file_name), "duration": canon(duration)},
    "build_file_upload_request": lambda body: {"body": canon(body)},
    "extract_response_field": lambda response, field: {
        "response": canon(response), "field": canon(field)},
    "response_error_result": lambda response, default_message, override_error: {
        "response": canon(response), "default_message": canon(default_message),
        "override_error": canon(override_error)},
    "send_uploaded_key": lambda chat_id, reply_to, metadata, caption, key_msg_type, key_payload, media_tag: {
        "chat_id": canon(chat_id), "reply_to": canon(reply_to), "metadata": canon(metadata),
        "caption": canon(caption), "key_msg_type": canon(key_msg_type),
        "key_payload": canon(key_payload), "media_tag": canon(media_tag)},
    "response_succeeded": lambda response: {"response": canon(response)},
    "fetch_last_message_in_thread": lambda thread_id: {"thread_id": canon(thread_id)},
    "log_info": lambda fmt, *a: {"message": canon((fmt % a) if a else fmt)},
    "log_error": lambda fmt, *a: {"message": canon((fmt % a) if a else fmt)},
    # F7 _send_uploaded_key
    "build_media_post_payload": lambda caption, media_tag: {
        "caption": canon(caption), "media_tag": canon(media_tag)},
    # F8 _shutdown_sdk_executor
    "lock_enter": lambda: {},
    "take_executor": lambda: {},
    "lock_exit": lambda: {},
    "executor_shutdown": lambda wait, cancel_futures: {
        "wait": canon(wait), "cancel_futures": canon(cancel_futures)},
    # F9 _standalone_send
    "load_lark_oapi": lambda: {},
    "send_error": lambda message: {"message": canon(message)},
    "build_lark_client": lambda domain: {"domain": canon(domain)},
    "send": lambda chat_id, message, metadata: {
        "chat_id": canon(chat_id), "message": canon(message), "metadata": canon(metadata)},
    "media_send": lambda sender, chat_id, path, metadata: {
        "sender": canon(sender), "chat_id": canon(chat_id), "path": canon(path),
        "metadata": canon(metadata)},
    # F10 _stop_webhook_server
    "webhook_cleanup": lambda: {},
    "clear_webhook_state": lambda: {},
    # F11 _submit_on_loop
    "safe_schedule_threadsafe": lambda coro, loop, log_message, log_level: {
        "coro": canon(coro), "loop": canon(loop),
        "log_message": canon(log_message), "log_level": canon(log_level)},
    "add_done_callback": lambda future: {"future": canon(_marker(future))},
    # F12 _supervise_websocket_thread
    "shield": lambda future: {"future": canon(_marker(future))},
    "logger_error": lambda fmt, *a: {"message": canon((fmt % a) if a else fmt)},
    "write_runtime_status": lambda status, platform_state, error_code, error_message: {
        "status": canon(status), "platform_state": canon(platform_state),
        "error_code": canon(error_code), "error_message": canon(error_message)},
    "sleep": lambda seconds: {"seconds": canon(seconds)},
    "connect_websocket": lambda: {},
    "logger_warning": lambda fmt, *a: {"message": canon((fmt % a) if a else fmt)},
}


def _marker(x):
    return getattr(x, "_marker", x)


AWAIT_SEAMS = {
    "run_in_executor", "send_interactive_card", "feishu_send_with_retry",
    "run_blocking", "send_uploaded_key", "fetch_last_message_in_thread",
    "load_lark_oapi", "send", "media_send", "webhook_cleanup",
    "shield", "sleep", "connect_websocket",
}


# ── per-case script backend (mirrors C fa14_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.scripted = []
        self.running_i = 0
        self.future_i = 0
        self.objs = {}

    def obj(self, marker):
        """Stable object identity per marker (for `is` comparisons)."""
        key = ("obj", str(marker))
        if key not in self.objs:
            self.objs[key] = types.SimpleNamespace(_marker=marker)
        return self.objs[key]

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


class _Self(types.SimpleNamespace):
    pass


def make_recorder(ctx, nm, argfn=None):
    fn = argfn or ARGMAP[nm]
    def build(*a, **k):
        return ctx.call(nm, fn(*a, **k))
    if nm in AWAIT_SEAMS:
        async def af(*a, **k):
            return build(*a, **k)
        return af
    return build


def make_log(ctx, nm):
    def log(fmt, *a, **k):
        msg = (fmt % a) if a else fmt
        return ctx.call(nm, {"message": canon(msg)})
    return log


def make_sr(ctx, nm, argname=None):
    """Recorder that converts a scripted SendResult dict into a SendResult object."""
    def build(*a, **k):
        r = ctx.call(nm, ARGMAP[nm](*a, **k))
        return _sr(r)
    if nm in AWAIT_SEAMS:
        async def af(*a, **k):
            return build(*a, **k)
        return af
    return build


def make_resp(ctx, nm):
    """Recorder returning an attribute double that preserves every script field."""
    def build(*a, **k):
        v = ctx.call(nm, ARGMAP[nm](*a, **k))
        if isinstance(v, dict):
            return _Resp(**v)
        return v
    if nm in AWAIT_SEAMS:
        async def af(*a, **k):
            return build(*a, **k)
        return af
    return build


class _Resp(types.SimpleNamespace):
    pass


def make_sic(ctx):
    """_send_exec_approval_prompt -> _send_interactive_card composed seam."""
    async def sic(chat_id, card, metadata, failure_message, *, state_map=None,
                  state_id=None, session_key=None, extra_state=None):
        r = ctx.call("send_interactive_card", {
            "chat_id": canon(chat_id), "card": canon(card), "metadata": canon(metadata),
            "failure_message": canon(failure_message), "state_map_kind": "approval",
            "state_id": canon(state_id), "session_key": canon(session_key),
            "extra_state": canon(extra_state)})
        return _sr(r)
    return sic


def make_rb(ctx):
    """_run_blocking seam: the first positional arg is the SDK method marker."""
    async def rb(func, *a):
        return ctx.call("run_blocking", {
            "method": canon(getattr(func, "_method", "?")),
            "request": canon(a[0] if a else None)})
    return rb


class SendResult(types.SimpleNamespace):
    def __init__(self, success=False, message_id=None, error=None):
        super().__init__(success=success, message_id=message_id, error=error)


def _sr(r):
    if isinstance(r, dict):
        return SendResult(success=bool(r.get("success", False)),
                          message_id=r.get("message_id"), error=r.get("error"))
    return r


def _stub_pkg(name):
    m = types.ModuleType(name); m.__path__ = []
    sys.modules[name] = m
    return m


# ── doubles ──
class _ExecLoop:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    async def run_in_executor(self, executor, run_fn, *args):
        # live passes (executor, copy_context().run, func, *call_args): drop `func`
        call_args = list(args[1:]) if args else []
        return self._ctx.call("run_in_executor", {
            "executor": canon(executor), "func_args": canon(call_args)})


class _LoopD(types.SimpleNamespace):
    def __init__(self, ctx, marker):
        super().__init__(_marker=marker)
        object.__setattr__(self, "_ctx", ctx)
    def run_until_complete(self, coro):
        return object.__getattribute__(self, "_ctx").call("run_until_complete", {"coro": canon(coro)})
    def stop(self):
        return object.__getattribute__(self, "_ctx").call("loop_stop", {})
    def close(self):
        return object.__getattribute__(self, "_ctx").call("loop_close", {})


class _WSClient:
    """WS client double: the first runtime-override attribute write fires the
    single `apply_overrides` seam; a scripted raise is caught by live's
    `except Exception` -> logger.debug (or propagates for a BaseException)."""
    def __init__(self, ctx, params):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "_params", params)
        object.__setattr__(self, "_fired", False)

    def __getattr__(self, k):
        return None

    def __setattr__(self, k, v):
        if k == "_reconnect_nonce" and not object.__getattribute__(self, "_fired"):
            object.__setattr__(self, "_fired", True)
            p = object.__getattribute__(self, "_params")
            return object.__getattribute__(self, "_ctx").call("apply_overrides", {
                "nonce": canon(p.get("nonce", "")),
                "reconnect_interval": canon(p.get("reconnect_interval", 1)),
                "ping_interval": canon(p.get("ping_interval", None))})
        object.__setattr__(self, k, v)

    def start(self):
        return object.__getattribute__(self, "_ctx").call("start", {})


class _TaskD(types.SimpleNamespace):
    def __init__(self, ctx, marker):
        super().__init__(_marker=marker)
        object.__setattr__(self, "_ctx", ctx)
    def done(self):
        return object.__getattribute__(self, "_ctx").call("task_done", {"task": canon(_marker(self))})
    def cancel(self):
        return object.__getattribute__(self, "_ctx").call("task_cancel", {"task": canon(_marker(self))})


class _Counter:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    def __next__(self): return object.__getattribute__(self, "_ctx").call("next_approval", {})


class _StateMap:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    def __setitem__(self, key, val):
        object.__getattribute__(self, "_ctx").call("state_set", {
            "state_id": canon(key), "session_key": canon(val.get("session_key")),
            "message_id": canon(val.get("message_id")), "chat_id": canon(val.get("chat_id")),
            "extra_state": canon({k: v for k, v in val.items()
                                  if k not in ("session_key", "message_id", "chat_id")})})


class _Method:
    def __init__(self, ctx, nm, method=None):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "_nm", nm)
        object.__setattr__(self, "_method", method)
    def __call__(self, *a, **k):
        return object.__getattribute__(self, "_ctx").call(object.__getattribute__(self, "_nm"),
                                                          ARGMAP[object.__getattribute__(self, "_nm")](*a, **k))


class _AMethod:
    def __init__(self, ctx, nm, method=None):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "_nm", nm)
        object.__setattr__(self, "_method", method)
    async def __call__(self, *a, **k):
        return object.__getattribute__(self, "_ctx").call(object.__getattribute__(self, "_nm"),
                                                          ARGMAP[object.__getattribute__(self, "_nm")](*a, **k))


def _sdk_message(ctx, method):
    """A client.im.v1.message.<method> callable that records its method tag."""
    return _AMethod(ctx, "run_blocking", method)


class _ShutdownLock:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    def __enter__(self): return self._ctx.call("lock_enter", {})
    def __exit__(self, *a): return self._ctx.call("lock_exit", {})


class _FileD:
    def __init__(self, marker): self._marker = marker
    def __enter__(self): return self
    def __exit__(self, *a): return False


def _open_rec(ctx, path, mode):
    return _FileD(ctx.call("open", {"path": canon(path), "mode": canon(mode)}))


class _Executor(types.SimpleNamespace):
    def __init__(self, ctx):
        super().__init__()
        object.__setattr__(self, "_ctx", ctx)
    def shutdown(self, *, wait=False, cancel_futures=False):
        return object.__getattribute__(self, "_ctx").call("executor_shutdown", {
            "wait": canon(wait), "cancel_futures": canon(cancel_futures)})


class _ShutdownSelf(types.SimpleNamespace):
    def __init__(self, ctx, args):
        super().__init__()
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "_args", args)
    @property
    def _sdk_executor_lock(self):
        ctx = object.__getattribute__(self, "_ctx")
        args = object.__getattribute__(self, "_args")
        return _ShutdownLock(ctx) if args.get("has_lock") else None
    @property
    def _sdk_executor(self):
        ctx = object.__getattribute__(self, "_ctx")
        v = ctx.call("take_executor", {})
        return _Executor(ctx) if v is not None else None
    @_sdk_executor.setter
    def _sdk_executor(self, val):
        pass


class _Runner:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    async def cleanup(self): return self._ctx.call("webhook_cleanup", {})


class _WebhookSelf(_Self):
    @property
    def _webhook_runner(self):
        return object.__getattribute__(self, "_runner")
    @_webhook_runner.setter
    def _webhook_runner(self, val):
        ctx = object.__getattribute__(self, "_ctx")
        ctx.call("clear_webhook_state", {})
        object.__setattr__(self, "_runner", val)


class _Future:
    def __init__(self, ctx, marker):
        object.__setattr__(self, "_marker", marker)
        object.__setattr__(self, "_ctx", ctx)
    def add_done_callback(self, cb):
        return self._ctx.call("add_done_callback", {"future": canon(_marker(self))})


class _StandaloneAdapter:
    def __init__(self, ctx, pconfig):
        self._ctx = ctx
        self._domain_name = pconfig.get("domain", "feishu") if isinstance(pconfig, dict) else "feishu"
    def _build_lark_client(self, domain):
        return self._ctx.call("build_lark_client", {"domain": canon(domain)})
    async def send(self, chat_id, message, *, metadata=None):
        return _sr(self._ctx.call("send", {"chat_id": canon(chat_id), "message": canon(message),
                                           "metadata": canon(metadata)}))
    async def _media(self, kind, chat_id, path, metadata=None):
        return _sr(self._ctx.call("media_send", {"sender": kind, "chat_id": canon(chat_id),
                                                 "path": canon(path), "metadata": canon(metadata)}))
    async def send_image_file(self, chat_id, path, *, metadata=None):
        return await self._media("image_file", chat_id, path, metadata)
    async def send_video(self, chat_id, path, *, metadata=None):
        return await self._media("video", chat_id, path, metadata)
    async def send_voice(self, chat_id, path, *, metadata=None):
        return await self._media("voice", chat_id, path, metadata)
    async def send_document(self, chat_id, path, *, metadata=None):
        return await self._media("document", chat_id, path, metadata)


class _WSFuture:
    def __init__(self, marker): object.__setattr__(self, "_marker", marker)


class _SuperviseSelf(_Self):
    @property
    def _running(self):
        ctx = object.__getattribute__(self, "_ctx")
        i = ctx.running_i; ctx.running_i += 1
        vals = object.__getattribute__(self, "_runnings")
        return bool(vals[i]) if i < len(vals) else False
    @property
    def _ws_future(self):
        ctx = object.__getattribute__(self, "_ctx")
        i = ctx.future_i; ctx.future_i += 1
        vals = object.__getattribute__(self, "_futures")
        v = vals[i] if i < len(vals) else None
        return None if v is None else ctx.obj(v)
    @property
    def _ws_client(self):
        return object.__getattribute__(self, "_client")


SELF = None
_KIND = [""]


def install_recorders(ctx, target, args):
    global SELF
    NS.clear(); NS.update(LIVE_NS)
    NS["print"] = lambda *a, **k: None
    NS["json"] = types.SimpleNamespace(dumps=make_recorder(ctx, "json_dumps"))
    NS["SendResult"] = SendResult
    NS["logger"] = types.SimpleNamespace(
        warning=make_log(ctx, "log_warning"), error=make_log(ctx, "log_error"),
        info=make_log(ctx, "log_info"), debug=make_log(ctx, "logger_debug"),
        warning2=make_log(ctx, "logger_warning"),
        exception=lambda *a, **k: None)
    # proxy error helpers are silent; describe_proxy_error fires the proxy_error seam
    NS["_onboard_open_base_url"] = lambda d: "https://open.feishu.cn"
    NS["env_proxy_for_redaction"] = lambda v=None, *a: None
    NS["describe_proxy_error"] = lambda exc, env=None: ctx.call(
        "proxy_error", {"error": canon(str(exc))})
    SELF = _Self(_ctx=ctx)

    if target == "_run_blocking":
        NS["contextvars"] = types.SimpleNamespace(
            copy_context=lambda: types.SimpleNamespace(run=lambda fn, *a: fn(*a)))
        NS["asyncio"] = types.SimpleNamespace(get_running_loop=make_recorder(ctx, "get_running_loop"))
        _install(SELF, "_get_sdk_executor", make_recorder(ctx, "get_sdk_executor"))
        NS["__execloop__"] = _ExecLoop(ctx)
        def _get_loop():
            ctx.call("get_running_loop", {})
            return NS["__execloop__"]
        NS["asyncio"] = types.SimpleNamespace(get_running_loop=_get_loop)
    elif target == "_run_official_feishu_ws_client":
        def _new_loop():
            marker = ctx.call("new_event_loop", {})
            return _LoopD(ctx, marker)
        def _all_tasks(loop):
            markers = ctx.call("all_tasks", {"loop": canon(_marker(loop))})
            return [_TaskD(ctx, m) for m in (markers or [])]
        NS["asyncio"] = types.SimpleNamespace(
            new_event_loop=_new_loop,
            set_event_loop=make_recorder(ctx, "set_event_loop"),
            all_tasks=_all_tasks,
            gather=make_recorder(ctx, "gather"))
        NS["_install_lark_ws_isolation"] = make_recorder(ctx, "install_ws_isolation")
        NS["_ws_isolation_state"] = types.SimpleNamespace(
            loop=None, connect_kwargs=None, on_link_up=None, adapter=None)
        NS["logger"] = types.SimpleNamespace(
            debug=make_log(ctx, "logger_debug"),
            warning=lambda *a, **k: None, error=lambda *a, **k: None,
            info=lambda *a, **k: None, exception=lambda *a, **k: None)
        for mn in ("lark_oapi", "lark_oapi.ws", "lark_oapi.ws.client"):
            _stub_pkg(mn)
    elif target == "_send_exec_approval_prompt":
        NS["t"] = make_recorder(ctx, "t")
        NS["_card_button"] = make_recorder(ctx, "card_button")
        NS["_card"] = make_recorder(ctx, "card")
        _install(SELF, "_approval_counter", _Counter(ctx))
        _install(SELF, "_ea_deadline_line", make_recorder(ctx, "ea_deadline_line"))
        _install(SELF, "_EA_CARD_ACTIONS", args.get("card_actions", {}))
        _install(SELF, "_approval_state", {})
        _install(SELF, "_client", object() if args.get("has_client") else None)
        _install(SELF, "_domain_name", args.get("domain", "feishu"))
        _install(SELF, "_send_interactive_card", make_sic(ctx))
    elif target == "_send_interactive_card":
        _install(SELF, "_feishu_send_with_retry", make_recorder(ctx, "feishu_send_with_retry"))
        _install(SELF, "_finalize_send_result", make_sr(ctx, "finalize_send_result"))
    elif target == "_send_raw_message":
        NS["uuid"] = types.SimpleNamespace(uuid4=make_recorder(ctx, "uuid4"))
        _install(SELF, "_client", types.SimpleNamespace(im=types.SimpleNamespace(
            v1=types.SimpleNamespace(message=types.SimpleNamespace(
                reply=_sdk_message(ctx, "reply"), create=_sdk_message(ctx, "create"))))))
        _install(SELF, "_build_reply_message_body", make_recorder(ctx, "build_reply_message_body"))
        _install(SELF, "_build_reply_message_request", make_recorder(ctx, "build_reply_message_request"))
        _install(SELF, "_build_create_message_body", make_recorder(ctx, "build_create_message_body"))
        _install(SELF, "_build_create_message_request", make_recorder(ctx, "build_create_message_request"))
        _install(SELF, "_run_blocking", make_rb(ctx))
    elif target == "_send_uploaded_file_message":
        _install(SELF, "_domain_name", args.get("domain", "feishu"))
        NS["os"] = types.SimpleNamespace(path=types.SimpleNamespace(
            exists=make_recorder(ctx, "path_exists"),
            basename=make_recorder(ctx, "path_basename")))
        NS["open"] = lambda path, mode: _open_rec(ctx, path, mode)
        for nm in ("_resolve_outbound_file_routing", "_get_audio_duration_ms",
                   "_build_file_upload_body", "_build_file_upload_request",
                   "_extract_response_field", "_response_succeeded",
                   "_fetch_last_message_in_thread"):
            _install(SELF, nm, make_recorder(ctx, {
                "_resolve_outbound_file_routing": "resolve_outbound_file_routing",
                "_get_audio_duration_ms": "get_audio_duration_ms",
                "_build_file_upload_body": "build_file_upload_body",
                "_build_file_upload_request": "build_file_upload_request",
                "_extract_response_field": "extract_response_field",
                "_response_succeeded": "response_succeeded",
                "_fetch_last_message_in_thread": "fetch_last_message_in_thread"}[nm]))
        _install(SELF, "_response_error_result", make_resp(ctx, "response_error_result"))
        _install(SELF, "_send_uploaded_key", make_resp(ctx, "send_uploaded_key"))
        _install(SELF, "_feishu_send_with_retry", make_resp(ctx, "feishu_send_with_retry"))
        _install(SELF, "_finalize_send_result", make_resp(ctx, "finalize_send_result"))
        _install(SELF, "_run_blocking", make_rb(ctx))
        _install(SELF, "_client", None if not args.get("has_client") else types.SimpleNamespace(
            im=types.SimpleNamespace(v1=types.SimpleNamespace(
                file=types.SimpleNamespace(create=_sdk_message(ctx, "file.create"))))))
    elif target == "_send_uploaded_key":
        _install(SELF, "_build_media_post_payload", make_recorder(ctx, "build_media_post_payload"))
        _install(SELF, "_feishu_send_with_retry", make_recorder(ctx, "feishu_send_with_retry"))
    elif target == "_shutdown_sdk_executor":
        if args.get("has_lock"):
            _install(SELF, "_sdk_executor_lock", _ShutdownLock(ctx))
        if args.get("has_executor"):
            _install(SELF, "_sdk_executor", _Executor(ctx))
    elif target == "_standalone_send":
        import posixpath
        NS["asyncio"] = types.SimpleNamespace(to_thread=lambda fn: _to_thread(ctx))
        NS["_load_lark_oapi"] = lambda: True
        NS["_sdk_domain"] = lambda d: d
        NS["FeishuAdapter"] = lambda pconfig: _StandaloneAdapter(ctx, pconfig)
        NS["send_error"] = make_recorder(ctx, "send_error")
        NS["os"] = types.SimpleNamespace(path=types.SimpleNamespace(
            exists=make_recorder(ctx, "path_exists"), splitext=posixpath.splitext))
        NS["_MIGRATION_IMAGE_EXTS"] = {".jpg", ".jpeg", ".png", ".webp", ".gif"}
        NS["_MIGRATION_VIDEO_EXTS"] = {".mp4", ".mov", ".avi", ".mkv", ".webm", ".3gp"}
        NS["_MIGRATION_AUDIO_EXTS"] = {".ogg", ".opus", ".mp3", ".wav", ".m4a", ".flac"}
        NS["_ONBOARD_OPEN_URLS"] = {"feishu": "https://open.feishu.cn",
                                   "lark": "https://open.larksuite.com"}
    elif target == "_stop_webhook_server":
        global _WEBHOOK_CTX
        _WEBHOOK_CTX = ctx
    elif target == "_submit_on_loop":
        NS["logging"] = types.SimpleNamespace(WARNING="WARNING")
        NS["logger"] = types.SimpleNamespace(warning=lambda *a, **k: None)
        _install(SELF, "_log_background_failure", lambda *a, **k: None)
        _stub_pkg("agent")
        au = _stub_pkg("agent.async_utils")
        def _ss(coro, loop, *, logger=None, log_message=None, log_level=None):
            v = ctx.call("safe_schedule_threadsafe", {
                "coro": canon(coro), "loop": canon(loop),
                "log_message": canon(log_message), "log_level": canon(log_level)})
            return _Future(ctx, v) if v is not None else None
        au.safe_schedule_threadsafe = _ss
    elif target == "_supervise_websocket_thread":
        ss = _SuperviseSelf(_ctx=ctx, _runnings=list(args.get("runnings", [])),
                            _futures=list(args.get("futures", [])),
                            _client=object() if args.get("ws_client_present", True) else None,
                            _ws_restart_backoff=args.get("backoff", 5.0))
        global SELF_REF
        SELF_REF = ss
        _install(ss, "_connect_websocket", make_recorder(ctx, "connect_websocket"))
        _install(ss, "_write_runtime_status_safe", make_recorder(ctx, "write_runtime_status"))
        NS["asyncio"] = types.SimpleNamespace(
            shield=make_recorder(ctx, "shield"), sleep=make_recorder(ctx, "sleep"),
            CancelledError=asyncio.CancelledError)
        NS["logger"] = types.SimpleNamespace(
            error=make_log(ctx, "logger_error"), warning=make_log(ctx, "logger_warning"),
            info=lambda *a, **k: None, debug=lambda *a, **k: None,
            exception=lambda *a, **k: None)


async def _to_thread(ctx):
    return ctx.call("load_lark_oapi", {})


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
    if func == "_run_blocking":
        return NS[func](SELF, lambda: None, *g("func_args", []))
    if func == "_run_official_feishu_ws_client":
        adapter = types.SimpleNamespace(
            _ws_reconnect_nonce=g("nonce", ""), _ws_reconnect_interval=g("reconnect_interval", 1),
            _ws_ping_interval=g("ping_interval", None), _ws_ping_timeout=g("ping_timeout", None),
            _loop=None, _ws_thread_loop=None,
            _ws_link_retrying=lambda w: None, _ws_link_up=lambda w: None)
        ws = _WSClient(ctx, args)
        return NS[func](ws, adapter)
    if func == "_send_exec_approval_prompt":
        prompt = types.SimpleNamespace(
            chat_id=g("chat_id", ""), text=g("text", ""), actions=g("actions", []),
            metadata=g("metadata", None), session_key=g("session_key", ""),
            request_id=g("request_id", None))
        return NS[func](SELF, prompt)
    if func == "_send_interactive_card":
        return NS[func](SELF, g("chat_id", ""), g("card", {}), g("metadata", None),
                        g("failure_message", ""), state_map=_StateMap(ctx),
                        state_id=g("state_id", 0), session_key=g("session_key", ""),
                        extra_state=g("extra_state", None))
    if func == "_send_raw_message":
        return NS[func](SELF, chat_id=g("chat_id", ""), msg_type=g("msg_type", "text"),
                        payload=g("payload", ""), reply_to=g("reply_to", None),
                        metadata=g("metadata", None))
    if func == "_send_uploaded_file_message":
        return NS[func](SELF, chat_id=g("chat_id", ""), file_path=g("file_path", ""),
                        reply_to=g("reply_to", None), metadata=g("metadata", None),
                        caption=g("caption", None), file_name=g("file_name", None),
                        outbound_message_type=g("outbound_message_type", "file"))
    if func == "_send_uploaded_key":
        return NS[func](SELF, chat_id=g("chat_id", ""), reply_to=g("reply_to", None),
                        metadata=g("metadata", None), caption=g("caption", None),
                        key_msg_type=g("key_msg_type", "file"), key_payload=g("key_payload", {}),
                        media_tag=g("media_tag", {}))
    if func == "_shutdown_sdk_executor":
        return NS[func](_ShutdownSelf(ctx, args))
    if func == "_standalone_send":
        return NS[func](g("pconfig", {}), g("chat_id", ""), g("message", ""),
                        thread_id=g("thread_id", None), media_files=g("media_files", None),
                        force_document=bool(g("force_document", False)))
    if func == "_stop_webhook_server":
        slf = _WebhookSelf(_ctx=_WEBHOOK_CTX)
        _install(slf, "_runner", _Runner(ctx) if g("has_runner") else None)
        _install(slf, "_webhook_site", None)
        return NS[func](slf)
    if func == "_submit_on_loop":
        return NS[func](SELF, g("loop", "LOOP"), g("coro", "CORO"))
    if func == "_supervise_websocket_thread":
        return NS[func](SELF_REF)
    raise RuntimeError(f"unknown func {func}")


def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    install_recorders(ctx, func, args)
    try:
        ret = _run(live_call(func, args, ctx))
    except BaseException as exc:
        ret = exc_spec(exc, ctx)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": {}}


# ── cross-checks ──
def cross_check():
    missing_argmap = sorted(SEAMED - set(ARGMAP))
    if missing_argmap:
        print(f"FATAL oracle_feishu_adapter_flow14: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra = sorted(set(ARGMAP) - SEAMED)
    if extra:
        print(f"FATAL oracle_feishu_adapter_flow14: ARGMAP seams absent from C: {extra}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow14.jsonl"
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
            print(f"FATAL oracle_feishu_adapter_flow14: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        missing_funcs = sorted(set(TARGETS) - fix_funcs)
        if missing_funcs:
            print(f"FATAL oracle_feishu_adapter_flow14: targets without fixture cases: {missing_funcs}")
            sys.exit(2)
        unknown_funcs = sorted(fix_funcs - set(TARGETS))
        if unknown_funcs:
            print(f"FATAL oracle_feishu_adapter_flow14: fixture funcs not in TARGETS: {unknown_funcs}")
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
    print(f"ORACLE feishu_adapter_flow14: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow14.jsonl"
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
    print(f"SELFTEST feishu_adapter_flow14: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
