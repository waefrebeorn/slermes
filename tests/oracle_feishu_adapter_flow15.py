#!/usr/bin/env python3
"""Oracle for the Feishu adapter remainders FLOW port (C sibling:
src/gateway/platforms/feishu_adapter_flow15.c).

Drives the LIVE plugins/platforms/feishu/adapter.py defs with SCRIPTED seams:
every collaborator is a recording stub fed the same scripts the C harness feeds
its universal seam fa15_call. Compares the DECISION SEQUENCE (ordered seam
events {"name","args","ret"} in "trace") plus the final return ("ret") and the
(always empty) state.

Logger calls are modeled SILENT (no seam) on both sides; only observable
decision/collaborator calls are seamed. Fail-closed: missing defs or 0 cases
=> exit 2. Per-case wrapper catches BaseException (SystemExit included).
Summary: 'ORACLE feishu_adapter_flow15: N cases, M mismatches'.
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
from pathlib import PurePath
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
    "_EA_ACTION_LABELS", "_target", "_teardown_ws_thread", "_tenant_get_raw",
    "_tenant_get_request", "_write_update_prompt_response", "delete_message",
    "edit_message", "get_chat_info", "on_processing_complete", "on_processing_start",
    "qr_register", "send_animation", "send_document",
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
    mod = types.ModuleType("fa15_oracle_adapter")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    from typing import Any, Dict, List, Optional, Sequence
    ns.update({
        "Path": __import__("pathlib").PurePosixPath, "PurePath": PurePath, "json": json,
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
    anim_node = None
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in TARGETS:
            c = copy.copy(node); c.decorator_list = []
            exec_node(src, c, ns, SRC); done.add(node.name)
        elif isinstance(node, ast.ClassDef) and node.name == "FeishuAdapter":
            for ch in node.body:
                if isinstance(ch, (ast.FunctionDef, ast.AsyncFunctionDef)) and ch.name in TARGETS:
                    if ch.name == "send_animation":
                        anim_node = copy.copy(ch); anim_node.decorator_list = []
                        continue
                    c = copy.copy(ch); c.decorator_list = []
                    exec_node(src, c, ns, SRC); done.add(ch.name)
    if anim_node is not None:
        # send_animation calls zero-arg super(); host it in a synthetic class.
        base = type("BaseAnim", (), {})
        ns["BaseAnim"] = base
        cls = ast.ClassDef(name="AnimDouble", bases=[ast.Name(id="BaseAnim", ctx=ast.Load())],
                           body=[anim_node], decorator_list=[], keywords=[])
        mod2 = ast.Module(body=[cls], type_ignores=[])
        ast.fix_missing_locations(mod2)
        exec(compile(mod2, SRC, "exec", flags=_FLAGS, dont_inherit=True), ns)
        ns["send_animation"] = ns["AnimDouble"].send_animation
        done.add("send_animation")
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in TARGETS and node.name not in done:
            if node.name == "send_animation":
                continue
            c = copy.copy(node); c.decorator_list = []
            exec_node(src, c, ns, SRC); done.add(node.name)
    missing = [t for t in TARGETS if t not in ns]
    if missing:
        print(f"FATAL oracle_feishu_adapter_flow15: adapter missing targets: {missing}")
        sys.exit(2)
    return ns


NS = extract()
LIVE_NS = dict(NS)

# ── seam names regex-extracted from the C port (SEAM-NAME SET EQUALITY) ──
_CSRC = open(_REPO / "src" / "gateway" / "platforms" / "feishu_adapter_flow15.c",
             encoding="utf-8").read()
SEAMED = set(re.findall(r'fa15_call\(\s*ctx,\s*"([^"]+)"', _CSRC))


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
class URLError(OSError):
    pass


EXC_CLASSES = {
    "Exception": Exception, "BaseException": BaseException,
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "OSError": OSError, "IOError": OSError,
    "URLError": URLError, "FileNotFoundError": FileNotFoundError,
    "IsADirectoryError": IsADirectoryError, "PermissionError": PermissionError,
    "TimeoutError": TimeoutError, "ConnectionError": ConnectionError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeError": UnicodeError,
    "JSONDecodeError": json.JSONDecodeError, "json.JSONDecodeError": json.JSONDecodeError,
    "LookupError": LookupError, "StopIteration": StopIteration, "IndexError": IndexError,
    "ZeroDivisionError": ZeroDivisionError, "AssertionError": AssertionError,
    "NotImplementedError": NotImplementedError, "SyntaxError": SyntaxError,
    "Warning": Warning, "ArithmeticError": ArithmeticError,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "CancelledError": asyncio.CancelledError, "asyncio.CancelledError": asyncio.CancelledError,
}
BASE_EXC = {"BaseException", "SystemExit", "KeyboardInterrupt", "GeneratorExit",
            "CancelledError", "asyncio.CancelledError"}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    msg = spec.get("message", "")
    if kind in ("JSONDecodeError", "json.JSONDecodeError"):
        return json.JSONDecodeError(msg, "", 0)
    if cls is None:
        cls = (BaseException,) if kind in BASE_EXC else (Exception,)
        cls = type(kind, cls, {})
    return cls(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── seam argument projections (MUST match fa15_call args in the C port) ──
ARGMAP = {
    # F _target
    "ws_loop": lambda: {},
    # F _EA_ACTION_LABELS
    "t": lambda key: {"key": canon(key)},
    # F _tenant_get_request
    "base_builder": lambda: {},
    "http_method": lambda v: {"method": canon(v)},
    "uri": lambda uri: {"uri": canon(uri)},
    "queries": lambda queries: {"queries": canon(queries)},
    "token_types": lambda tokens: {"token_types": canon(tokens)},
    "build": lambda: {},
    # F _tenant_get_raw
    "tenant_get_request": lambda uri, queries=None: {"uri": canon(uri), "queries": canon(queries)},
    "run_blocking": lambda method, request: {"method": canon(method), "request": canon(request)},
    # F _write_update_prompt_response
    "get_hermes_home": lambda: {},
    "path_join": lambda base, part: {"base": canon(base), "part": canon(part)},
    "path_with_suffix": lambda path, suffix: {"path": canon(path), "suffix": canon(suffix)},
    "write_text": lambda path, answer, encoding=None: {
        "path": canon(path), "answer": canon(answer), "encoding": canon(encoding)},
    "path_replace": lambda src, dst: {
        "src": canon(src), "dst": canon(dst.path if isinstance(dst, _PPath) else dst)},
    # F delete_message
    "build_delete_request": lambda message_id: {"message_id": canon(message_id)},
    "response_succeeded": lambda response: {"response": canon(response)},
    # F edit_message
    "format_message": lambda content: {"content": canon(content)},
    "build_outbound_payload": lambda content: {"content": canon(content)},
    "build_update_body": lambda msg_type, content: {
        "msg_type": canon(msg_type), "content": canon(content)},
    "build_update_request": lambda message_id, request_body: {
        "message_id": canon(message_id), "request_body": canon(request_body)},
    "finalize_send_result": lambda response, failure_message: {
        "response": canon(response), "failure_message": canon(failure_message)},
    "post_invalid_search": lambda error: {"error": canon(error)},
    "json_dumps": lambda value, **kw: {"value": canon(value)},
    "strip_markdown": lambda content: {"content": canon(content)},
    "proxy_error": lambda exc, env=None: {"error": canon(str(exc))},
    # F get_chat_info
    "cache_get": lambda chat_id: {"chat_id": canon(chat_id)},
    "build_chat_request": lambda chat_id: {"chat_id": canon(chat_id)},
    "map_chat_type": lambda raw_type: {"raw_type": canon(raw_type)},
    "cache_set": lambda chat_id, info: {"chat_id": canon(chat_id), "info": canon(info)},
    # F on_processing_start
    "reactions_enabled": lambda: {},
    "cache_contains": lambda message_id: {"message_id": canon(message_id)},
    "add_reaction": lambda message_id, kind: {
        "message_id": canon(message_id), "kind": canon(kind)},
    "cache_store": lambda message_id, reaction_id: {
        "message_id": canon(message_id), "reaction_id": canon(reaction_id)},
    # F on_processing_complete
    "proc_cache_get": lambda message_id: {"message_id": canon(message_id)},
    "remove_reaction": lambda message_id, reaction_id: {
        "message_id": canon(message_id), "reaction_id": canon(reaction_id)},
    "cache_pop": lambda message_id, default=None: {"message_id": canon(message_id)},
    # F send_document
    "send_uploaded_file_message": lambda chat_id, file_path, reply_to=None, metadata=None,
    caption=None, file_name=None: {
        "chat_id": canon(chat_id), "file_path": canon(file_path), "reply_to": canon(reply_to),
        "metadata": canon(metadata), "caption": canon(caption), "file_name": canon(file_name)},
    # F send_animation
    "download_remote_document": lambda url, default_ext=None, preferred_name=None: {
        "url": canon(url), "default_ext": canon(default_ext), "preferred_name": canon(preferred_name)},
    "super_send_animation": lambda chat_id, animation_url, caption=None, reply_to=None, metadata=None: {
        "chat_id": canon(chat_id), "animation_url": canon(animation_url),
        "caption": canon(caption), "reply_to": canon(reply_to), "metadata": canon(metadata)},
    "t_anim": lambda key, caption=None: {"key": canon(key), "caption": canon(caption)},
    "warning_text": lambda text, caption=None: {"text": canon(text), "caption": canon(caption)},
    "send_document": lambda chat_id, file_path, file_name=None, caption=None, reply_to=None, metadata=None: {
        "chat_id": canon(chat_id), "file_path": canon(file_path), "file_name": canon(file_name),
        "caption": canon(caption), "reply_to": canon(reply_to), "metadata": canon(metadata)},
    # F qr_register
    "qr_inner": lambda initial_domain="feishu", timeout_seconds=600: {
        "initial_domain": canon(initial_domain), "timeout_seconds": canon(timeout_seconds)},
    # F _teardown_ws_thread
    "loop_is_closed": lambda: {},
    "ws_disconnect": lambda: {},
    "close_frame": lambda: {},
    "all_tasks": lambda: {},
    "task_done": lambda task: {"task": canon(task)},
    "task_cancel": lambda task: {"task": canon(task)},
    "call_later": lambda delay, cb=None: {"delay": canon(delay)},
    "call_soon_threadsafe": lambda fn=None: {},
    "wait_ws_future": lambda: {},
}

AWAIT_SEAMS = {
    "run_blocking", "add_reaction", "remove_reaction", "send_uploaded_file_message",
    "download_remote_document", "super_send_animation", "send_document",
    "close_frame", "wait_ws_future",
}

_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.scripted = []
        self.objs = {}
        self.phase = "close"

    def obj(self, marker):
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


class SendResult(types.SimpleNamespace):
    def __init__(self, success=False, message_id=None, error=None):
        super().__init__(success=success, message_id=message_id, error=error)


def _sr(r):
    if isinstance(r, dict):
        return SendResult(success=bool(r.get("success", False)),
                          message_id=r.get("message_id"), error=r.get("error"))
    return r


def make_sr(ctx, nm):
    def build(*a, **k):
        r = ctx.call(nm, ARGMAP[nm](*a, **k))
        return _sr(r)
    if nm in AWAIT_SEAMS:
        async def af(*a, **k):
            return build(*a, **k)
        return af
    return build


def _wrap(v):
    if isinstance(v, dict):
        return types.SimpleNamespace(**{k: _wrap(x) for k, x in v.items()})
    return v


# ── doubles ──
class _Builder:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    def _fire(self, nm, args):
        object.__getattribute__(self, "_ctx").call(nm, args)
        return _Builder(object.__getattribute__(self, "_ctx"))
    def http_method(self, v): return self._fire("http_method", ARGMAP["http_method"](v))
    def uri(self, v): return self._fire("uri", ARGMAP["uri"](v))
    def queries(self, v): return self._fire("queries", ARGMAP["queries"](v))
    def token_types(self, v): return self._fire("token_types", ARGMAP["token_types"](v))
    def build(self):
        return object.__getattribute__(self, "_ctx").call("build", {})


class _PPath:
    def __init__(self, ctx, path):
        object.__setattr__(self, "_ctx", ctx); object.__setattr__(self, "path", path)
    def __truediv__(self, part):
        ctx = object.__getattribute__(self, "_ctx")
        v = ctx.call("path_join", ARGMAP["path_join"](object.__getattribute__(self, "path"), part))
        return _PPath(ctx, v)
    def with_suffix(self, suf):
        ctx = object.__getattribute__(self, "_ctx")
        v = ctx.call("path_with_suffix", ARGMAP["path_with_suffix"](object.__getattribute__(self, "path"), suf))
        return _PPath(ctx, v)
    def write_text(self, answer, encoding=None):
        ctx = object.__getattribute__(self, "_ctx")
        return ctx.call("write_text", ARGMAP["write_text"](object.__getattribute__(self, "path"), answer, encoding))
    def replace(self, dst):
        ctx = object.__getattribute__(self, "_ctx")
        return ctx.call("path_replace", ARGMAP["path_replace"](object.__getattribute__(self, "path"), dst))


class _Method:
    def __init__(self, ctx, nm, method=None):
        object.__setattr__(self, "_ctx", ctx); object.__setattr__(self, "_nm", nm)
        object.__setattr__(self, "_method", method)
    def __call__(self, *a, **k):
        nm = object.__getattribute__(self, "_nm")
        return object.__getattribute__(self, "_ctx").call(nm, ARGMAP[nm](*a, **k))


class _AMethod:
    def __init__(self, ctx, nm, method=None):
        object.__setattr__(self, "_ctx", ctx); object.__setattr__(self, "_nm", nm)
        object.__setattr__(self, "_method", method)
    async def __call__(self, *a, **k):
        nm = object.__getattribute__(self, "_nm")
        return object.__getattribute__(self, "_ctx").call(nm, ARGMAP[nm](*a, **k))


class _ChatCache:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    def get(self, k):
        return object.__getattribute__(self, "_ctx").call("cache_get", {"chat_id": canon(k)})
    def __setitem__(self, k, v):
        return object.__getattribute__(self, "_ctx").call("cache_set", {"chat_id": canon(k), "info": canon(v)})


class _ReactCache:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    def __contains__(self, k):
        return object.__getattribute__(self, "_ctx").call("cache_contains", {"message_id": canon(k)})
    def __setitem__(self, k, v):
        return object.__getattribute__(self, "_ctx").call("cache_store", {"message_id": canon(k), "reaction_id": canon(v)})
    def move_to_end(self, k): return None
    def __len__(self): return 0
    def popitem(self, last=True): return (None, None)
    def get(self, k):
        return object.__getattribute__(self, "_ctx").call("proc_cache_get", {"message_id": canon(k)})
    def pop(self, k, d=None):
        return object.__getattribute__(self, "_ctx").call("cache_pop", {"message_id": canon(k)})


class _Event:
    def __init__(self, args): object.__setattr__(self, "message_id", args.get("message_id", ""))


class _ProcessingOutcome:
    FAILURE = object()
    SUCCESS = object()
    PARTIAL = object()


class _WSClient:
    def __init__(self, ctx, has_disc):
        object.__setattr__(self, "_ctx", ctx)
        if has_disc:
            object.__setattr__(self, "_disconnect", make_recorder(ctx, "ws_disconnect"))
    def __getattr__(self, k): return None


class _WSLoop:
    def __init__(self, ctx):
        object.__setattr__(self, "_ctx", ctx)
    def is_closed(self):
        return object.__getattribute__(self, "_ctx").call("loop_is_closed", {})
    def call_later(self, delay, cb):
        return object.__getattribute__(self, "_ctx").call("call_later", {"delay": canon(delay)})
    def call_soon_threadsafe(self, fn):
        ctx = object.__getattribute__(self, "_ctx")
        ctx.call("call_soon_threadsafe", {})
        fn()
        return None
    def stop(self):
        return None


class _TaskD:
    def __init__(self, ctx, marker):
        object.__setattr__(self, "_ctx", ctx); object.__setattr__(self, "_marker", marker)
    def done(self):
        return object.__getattribute__(self, "_ctx").call("task_done", {"task": canon(_marker(self))})
    def cancel(self):
        return object.__getattribute__(self, "_ctx").call("task_cancel", {"task": canon(_marker(self))})


def _marker(x):
    return getattr(x, "_marker", x)


SELF = None
_ANIM = None


def install_recorders(ctx, target, args):
    global SELF, _ANIM
    NS.clear(); NS.update(LIVE_NS)
    NS["print"] = lambda *a, **k: None
    NS["logger"] = types.SimpleNamespace(
        warning=lambda *a, **k: None, error=lambda *a, **k: None,
        info=lambda *a, **k: None, debug=lambda *a, **k: None,
        exception=lambda *a, **k: None)
    SELF = _Self(_ctx=ctx)

    if target == "_target":
        class _IsoState:
            loop = None
        def _get_loop(self): return ctx.call("ws_loop", {})
        NS["_ws_isolation_state"] = _IsoState()
        type(NS["_ws_isolation_state"]).loop = property(_get_loop)
        _install(SELF, "_fallback", args.get("_fallback"))

    elif target == "_EA_ACTION_LABELS":
        NS["t"] = make_recorder(ctx, "t")

    elif target == "_tenant_get_request":
        NS["BaseRequest"] = types.SimpleNamespace(
            builder=lambda: (ctx.call("base_builder", {}), _Builder(ctx))[1])
        NS["HttpMethod"] = types.SimpleNamespace(GET="GET")
        NS["AccessTokenType"] = types.SimpleNamespace(TENANT="TENANT")

    elif target == "_tenant_get_raw":
        NS["_tenant_get_request"] = make_recorder(ctx, "tenant_get_request")
        def _rb(func, req):
            v = ctx.call("run_blocking", ARGMAP["run_blocking"](getattr(func, "_method", "request"), req))
            return _wrap(v)
        async def arb(func, req): return _rb(func, req)
        _install(SELF, "_run_blocking", arb)
        _install(SELF, "_client", types.SimpleNamespace(request=_Method(ctx, "_noop", "request")))

    elif target == "_write_update_prompt_response":
        def gh(): return _PPath(ctx, ctx.call("get_hermes_home", {}))
        NS["get_hermes_home"] = gh

    elif target == "delete_message":
        NS["SendResult"] = SendResult
        _install(SELF, "_client", object() if args.get("has_client") else None)
        _install(SELF, "_build_delete_message_request", make_recorder(ctx, "build_delete_request"))
        _install(SELF, "_response_succeeded", make_recorder(ctx, "response_succeeded"))
        async def rb(func, req):
            return ctx.call("run_blocking", ARGMAP["run_blocking"](getattr(func, "_method", "delete"), req))
        _install(SELF, "_run_blocking", rb)
        # attach im.v1.message.delete marker
        if args.get("has_client"):
            _install(SELF, "_client", types.SimpleNamespace(im=types.SimpleNamespace(
                v1=types.SimpleNamespace(message=types.SimpleNamespace(delete=_Method(ctx, "_noop", "delete"))))))

    elif target == "edit_message":
        NS["SendResult"] = SendResult
        _install(SELF, "_domain_name", args.get("domain", "feishu"))
        NS["json"] = types.SimpleNamespace(dumps=make_recorder(ctx, "json_dumps"))
        NS["_POST_CONTENT_INVALID_RE"] = types.SimpleNamespace(
            search=make_recorder(ctx, "post_invalid_search"))
        NS["_strip_markdown_to_plain_text"] = make_recorder(ctx, "strip_markdown")
        NS["describe_proxy_error"] = make_recorder(ctx, "proxy_error")
        NS["env_proxy_for_redaction"] = lambda *a, **k: None
        NS["_onboard_open_base_url"] = lambda d: "https://open.feishu.cn"
        _install(SELF, "_client", object() if args.get("has_client") else None)
        _install(SELF, "format_message", make_recorder(ctx, "format_message"))
        _install(SELF, "_build_outbound_payload", make_recorder(ctx, "build_outbound_payload"))
        _install(SELF, "_build_update_message_body", make_recorder(ctx, "build_update_body"))
        _install(SELF, "_build_update_message_request", make_recorder(ctx, "build_update_request"))
        _install(SELF, "_finalize_send_result", make_sr(ctx, "finalize_send_result"))
        async def rb(func, req):
            return ctx.call("run_blocking", ARGMAP["run_blocking"](getattr(func, "_method", "update"), req))
        _install(SELF, "_run_blocking", rb)
        if args.get("has_client"):
            _install(SELF, "_client", types.SimpleNamespace(im=types.SimpleNamespace(
                v1=types.SimpleNamespace(message=types.SimpleNamespace(update=_Method(ctx, "_noop", "update"))))))

    elif target == "get_chat_info":
        NS["SendResult"] = SendResult
        _install(SELF, "_client", object() if args.get("has_client") else None)
        _install(SELF, "_chat_info_cache", _ChatCache(ctx))
        _install(SELF, "_build_get_chat_request", make_recorder(ctx, "build_chat_request"))
        _install(SELF, "_map_chat_type", make_recorder(ctx, "map_chat_type"))
        async def rb(func, req):
            v = ctx.call("run_blocking", ARGMAP["run_blocking"](getattr(func, "_method", "chat.get"), req))
            if isinstance(v, dict):
                d = dict(v)
                if isinstance(d.get("success"), bool):
                    b = d["success"]; d["success"] = lambda: b
                if "data" in d:
                    d["data"] = _wrap(d["data"])
                return types.SimpleNamespace(**d)
            return v
        _install(SELF, "_run_blocking", rb)
        if args.get("has_client"):
            _install(SELF, "_client", types.SimpleNamespace(im=types.SimpleNamespace(
                v1=types.SimpleNamespace(chat=types.SimpleNamespace(get=_Method(ctx, "_noop", "chat.get"))))))

    elif target == "on_processing_start":
        _install(SELF, "_reactions_enabled", make_recorder(ctx, "reactions_enabled"))
        _install(SELF, "_pending_processing_reactions", _ReactCache(ctx))
        _install(SELF, "_add_reaction", make_recorder(ctx, "add_reaction"))
        NS["_FEISHU_REACTION_IN_PROGRESS"] = "in_progress"
        NS["_FEISHU_PROCESSING_REACTION_CACHE_SIZE"] = 8

    elif target == "on_processing_complete":
        _install(SELF, "_reactions_enabled", make_recorder(ctx, "reactions_enabled"))
        _install(SELF, "_pending_processing_reactions", _ReactCache(ctx))
        _install(SELF, "_remove_reaction", make_recorder(ctx, "remove_reaction"))
        _install(SELF, "_add_reaction", make_recorder(ctx, "add_reaction"))
        NS["_FEISHU_REACTION_FAILURE"] = "failure"
        NS["ProcessingOutcome"] = _ProcessingOutcome

    elif target == "send_document":
        _install(SELF, "_send_uploaded_file_message", make_recorder(ctx, "send_uploaded_file_message"))

    elif target == "send_animation":
        NS["t"] = make_recorder(ctx, "t_anim")
        NS["describe_proxy_error"] = lambda exc, env=None: None
        NS["env_proxy_for_redaction"] = lambda *a, **k: None
        NS["_FEISHU_REACTION_IN_PROGRESS"] = "in_progress"
        NS["BaseAnim"].send_animation = staticmethod(make_recorder(ctx, "super_send_animation"))
        inst = NS["AnimDouble"]()
        _install(inst, "_download_remote_document", make_recorder(ctx, "download_remote_document"))
        _install(inst, "warning_text", make_recorder(ctx, "warning_text"))
        _install(inst, "send_document", make_recorder(ctx, "send_document"))
        global _ANIM
        _ANIM = inst

    elif target == "qr_register":
        NS["_qr_register_inner"] = make_recorder(ctx, "qr_inner")
        NS["json"] = json
        NS["URLError"] = URLError

    elif target == "_teardown_ws_thread":
        NS["TimeoutError"] = TimeoutError
        NS["asyncio"] = types.SimpleNamespace(
            CancelledError=asyncio.CancelledError,
            run_coroutine_threadsafe=lambda coro, loop: (setattr(ctx, "phase", "close") or "F"),
            wrap_future=lambda fut: fut,
            all_tasks=lambda loop: [ _TaskD(ctx, m) for m in (ctx.call("all_tasks", {}) or []) ],
            shield=lambda fut: (setattr(ctx, "phase", "wait") or fut),
            wait_for=_teardown_waitfor(ctx),
        )
        _install(SELF, "_ws_future", object() if args.get("ws_future_present") else None)


def _teardown_waitfor(ctx):
    async def wf(awaitable, timeout):
        nm = "close_frame" if getattr(ctx, "phase", "close") == "close" else "wait_ws_future"
        return ctx.call(nm, {})
    return wf


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
    if func == "_target":
        return NS[func](SELF)
    if func == "_EA_ACTION_LABELS":
        return NS[func](SELF)
    if func == "_tenant_get_request":
        return NS[func](g("uri", ""), queries=g("queries"))
    if func == "_tenant_get_raw":
        return NS[func](SELF, g("uri", ""), queries=g("queries"))
    if func == "_write_update_prompt_response":
        return NS[func](g("answer", ""))
    if func == "delete_message":
        return NS[func](SELF, g("chat_id", ""), g("message_id", ""))
    if func == "edit_message":
        return NS[func](SELF, g("chat_id", ""), g("message_id", ""), g("content", ""), finalize=bool(g("finalize", False)))
    if func == "get_chat_info":
        return NS[func](SELF, g("chat_id", ""))
    if func == "on_processing_start":
        return NS[func](SELF, _Event(args))
    if func == "on_processing_complete":
        outcome = _ProcessingOutcome.FAILURE if g("outcome", "") == "FAILURE" else _ProcessingOutcome.SUCCESS
        return NS[func](SELF, _Event(args), outcome)
    if func == "send_document":
        return NS[func](SELF, g("chat_id", ""), g("file_path", ""), caption=g("caption"),
                        file_name=g("file_name"), reply_to=g("reply_to"), metadata=g("metadata"))
    if func == "send_animation":
        return NS[func](_ANIM, g("chat_id", ""), g("animation_url", ""), caption=g("caption"),
                        reply_to=g("reply_to"), metadata=g("metadata"))
    if func == "qr_register":
        return NS[func](initial_domain=g("initial_domain", "feishu"), timeout_seconds=g("timeout_seconds", 600))
    if func == "_teardown_ws_thread":
        ws = None
        if args.get("ws_client_present"):
            ws = _WSClient(ctx, bool(args.get("ws_has_disconnect")))
        loop = _WSLoop(ctx) if args.get("loop_present") else None
        return NS[func](SELF, ws, loop)
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
        print(f"FATAL oracle_feishu_adapter_flow15: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra = sorted(set(ARGMAP) - SEAMED)
    if extra:
        print(f"FATAL oracle_feishu_adapter_flow15: ARGMAP seams absent from C: {extra}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow15.jsonl"
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
            print(f"FATAL oracle_feishu_adapter_flow15: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        missing_funcs = sorted(set(TARGETS) - fix_funcs)
        if missing_funcs:
            print(f"FATAL oracle_feishu_adapter_flow15: targets without fixture cases: {missing_funcs}")
            sys.exit(2)
        unknown_funcs = sorted(fix_funcs - set(TARGETS))
        if unknown_funcs:
            print(f"FATAL oracle_feishu_adapter_flow15: fixture funcs not in TARGETS: {unknown_funcs}")
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
    print(f"ORACLE feishu_adapter_flow15: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow15.jsonl"
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
    print(f"SELFTEST feishu_adapter_flow15: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
