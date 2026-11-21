#!/usr/bin/env python3
"""Oracle for the Feishu adapter remainders FLOW port (C sibling:
src/gateway/platforms/feishu_adapter_flow12.c).

Drives the LIVE plugins/platforms/feishu/adapter.py defs with SCRIPTED seams:
every collaborator is a recording stub fed the same scripts the C harness feeds
its universal seam fa12_call. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the
(always empty) state.

The live defs are AST-extracted from adapter.py; the heavy quarry import chain
is never imported. Fail-closed: missing defs or 0 cases => exit 2. Per-case
wrapper catches BaseException (SystemExit included); scripted raises round-trip
their spec. Summary: 'ORACLE feishu_adapter_flow12: N cases, M mismatches'.
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
    "_poll_registration", "_pop_text_batch", "_post_registration", "_prepare_client",
    "_probe_bot_http", "_process_inbound_message", "_qr_register_inner",
    "_reaction_call", "_read_binary_response", "_read_limited_feishu_webhook_body",
    "_should_batch_media_event", "_resolve_media_message_type", "_queue_empty",
    "_render_qr",
]

SRC_KEYS = [
    "chat_id", "chat_name", "chat_type", "user_id", "user_name", "thread_id",
    "user_id_alt", "is_bot", "message_id",
]
EVENT_KEYS = [
    "text", "message_type", "source", "raw_message", "message_id", "media_urls",
    "media_types", "media_text_inlined", "reply_to_message_id", "reply_to_text",
    "channel_prompt", "timestamp",
]
REG_KEYS = ["device_code", "interval", "expire_in", "domain"]


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def extract():
    mod = types.ModuleType("fa12_oracle_adapter")
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
        "_REGISTRATION_PATH": "/oauth/v1/app/registration",
        "_ONBOARD_REQUEST_TIMEOUT_S": 10,
        "_FEISHU_DEDUP_TTL_SECONDS": 86400,
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
    # nested closures (portable FLOW targets)
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in TARGETS and node.name not in done:
            c = copy.copy(node); c.decorator_list = []
            exec_node(src, c, ns, SRC); done.add(node.name)
    missing = [t for t in TARGETS if t not in ns]
    if missing:
        print(f"FATAL oracle_feishu_adapter_flow12: adapter missing targets: {missing}")
        sys.exit(2)
    return ns


NS = extract()
# Freshly-extracted live namespace; restored per case so target stubs installed
# for one function never leak into another (e.g. _post_registration recorder
# installed for _poll_registration must not replace the live _post_registration).
LIVE_NS = dict(NS)

# ── seam names regex-extracted from the C port (SEAM-NAME SET EQUALITY) ──
_CSRC = open(_REPO / "src" / "gateway" / "platforms" / "feishu_adapter_flow12.c",
             encoding="utf-8").read()
SEAMED = set(re.findall(r'fa12_call\(\s*ctx,\s*"([^"]+)"', _CSRC))


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


# ── seam argument projections (MUST match fa12_call args in the C port) ──
ARGMAP = {
    "monotonic": lambda: {},
    "sleep": lambda interval: {"interval": canon(interval)},
    "accounts_base_url": lambda domain: {"domain": domain},
    "post_registration": lambda base_url, body: {"base_url": base_url, "body": canon(body)},
    "post_form": lambda url, body: {"url": url, "body": canon(body)},
    "sdk_domain": lambda domain_name: {"domain_name": canon(domain_name)},
    "build_lark_client": lambda domain: {"domain": canon(domain)},
    "build_event_handler": lambda: {},
    "onboard_open_base_url": lambda domain: {"domain": domain},
    "probe_get_json": lambda path, data, extra_headers: {
        "path": path, "data": canon(data), "extra_headers": canon(extra_headers)},
    "parse_bot_response": lambda data: {"data": canon(data)},
    "extract_content": lambda message: {"message": canon(message)},
    "strip_edge_mentions": lambda text, mentions: {
        "text": canon(text), "mentions": canon(mentions)},
    "build_mention_hint": lambda mentions: {"mentions": canon(mentions)},
    "fetch_message_text": lambda message_id: {"message_id": canon(message_id)},
    "get_chat_info": lambda chat_id: {"chat_id": chat_id},
    "resolve_sender_profile": lambda sender_id, is_bot: {
        "sender_id": canon(sender_id), "is_bot": is_bot},
    "resolve_source_chat_type": lambda chat_info, event_chat_type: {
        "chat_info": canon(chat_info), "event_chat_type": event_chat_type},
    "resolve_channel_prompt": lambda chat_id, thread_id: {
        "chat_id": chat_id, "thread_id": canon(thread_id)},
    "build_source": lambda **kw: {k: canon(kw.get(k)) for k in SRC_KEYS},
    "make_message_event": lambda **kw: {k: canon(kw.get(k)) for k in EVENT_KEYS},
    "dispatch_inbound": lambda event: {"event": canon(event)},
    "init_registration": lambda domain: {"domain": domain},
    "begin_registration": lambda domain: {"domain": domain},
    "render_qr": lambda url: {"url": url},
    "install_hint": lambda kind: {"kind": kind},
    "poll_registration": lambda **kw: {k: canon(kw.get(k)) for k in REG_KEYS},
    "probe_bot": lambda app_id, app_secret, domain: {
        "app_id": app_id, "app_secret": app_secret, "domain": domain},
    "build_request": lambda: {},
    "run_blocking": lambda api, request: {"api": canon(api), "request": canon(request)},
    "response_succeeded": lambda response: {"response": canon(response)},
    "getvalue": lambda: {},
    "read": lambda: {},
    "readexactly": lambda n: {"n": canon(n)},
    "qr_construct": lambda: {},
    "qr_add_data": lambda url: {"url": url},
    "qr_make": lambda fit: {"fit": fit},
    "qr_print_ascii": lambda invert: {"invert": invert},
    "pop_count": lambda key: {"key": key},
    "pop_batch": lambda key: {"key": key},
}

AWAIT_SEAMS = {
    "extract_content", "fetch_message_text", "get_chat_info",
    "resolve_sender_profile", "run_blocking", "dispatch_inbound",
}

# ── per-case script backend (mirrors C fa12_call exactly) ──
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


class _DummyLock:
    def __enter__(self): return self
    def __exit__(self, *a): return False


class _PopDict:
    def __init__(self, ctx, name): object.__setattr__(self, "_ctx", ctx); object.__setattr__(self, "_nm", name)
    def pop(self, key, default=None):
        return self._ctx.call(self._nm, {"key": key})


class _RespCtx:
    def __init__(self, raw):
        object.__setattr__(self, "_raw", raw)
    def __enter__(self): return self
    def __exit__(self, *a): return False
    def read(self):
        return object.__getattribute__(self, "_raw")


class _Req:
    def __init__(self, url, data=None, headers=None):
        object.__setattr__(self, "full_url", url)
        object.__setattr__(self, "data", data)
        object.__setattr__(self, "headers", headers or {})


class _Encoded:
    def __init__(self, body): object.__setattr__(self, "body", body)
    def encode(self, enc): return self


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


def mat_event(raw):
    if isinstance(raw, dict):
        return types.SimpleNamespace(**{k: mat_event(v) for k, v in raw.items()})
    if isinstance(raw, list):
        return [mat_event(i) for i in raw]
    return raw


SELF = None
_CUR_CTX = None
_CUR_BASE = [""]
_ORIG_IMPORTLIB = sys.modules.get("importlib")


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


def install_recorders(ctx, target):
    global SELF, _CUR_CTX
    NS.clear()
    NS.update(LIVE_NS)
    _CUR_CTX = ctx
    NS["print"] = lambda *a, **k: None
    NS["logger"] = types.SimpleNamespace(
        warning=lambda *a, **k: None, error=lambda *a, **k: None,
        info=lambda *a, **k: None, debug=lambda *a, **k: None,
        exception=lambda *a, **k: None)
    SELF = _Self(ctx)
    NS["MessageType"] = MT
    NS["asyncio"] = types.SimpleNamespace(IncompleteReadError=asyncio.IncompleteReadError)
    NS["URLError"] = OSError

    if target == "_poll_registration":
        NS["time"] = types.SimpleNamespace(
            monotonic=make_recorder(ctx, "monotonic"), sleep=make_recorder(ctx, "sleep"))
        NS["_post_registration"] = make_recorder(ctx, "post_registration")
        NS["_accounts_base_url"] = make_recorder(ctx, "accounts_base_url")
    elif target == "_pop_text_batch":
        _install(SELF, "_pending_text_batch_counts", _PopDict(ctx, "pop_count"))
        _install(SELF, "_pending_text_batches", _PopDict(ctx, "pop_batch"))
    elif target == "_post_registration":
        NS["Request"] = _Req
        NS["urlencode"] = lambda body: _Encoded(body)
        NS["HTTPError"] = type("HTTPError", (Exception,), {})
        NS["_REGISTRATION_PATH"] = "/oauth/v1/app/registration"
        def urlopen_post(req, timeout=None):
            result = ctx.call("post_form", ARGMAP["post_form"](req.full_url, req.data.body))
            return _RespCtx(json.dumps(result, ensure_ascii=False).encode("utf-8"))
        NS["urlopen"] = urlopen_post
    elif target == "_prepare_client":
        _install(SELF, "_build_lark_client", make_recorder(ctx, "build_lark_client"))
        _install(SELF, "_build_event_handler", make_recorder(ctx, "build_event_handler"))
        NS["_sdk_domain"] = make_recorder(ctx, "sdk_domain")
    elif target == "_probe_bot_http":
        def ob_base(domain):
            result = ctx.call("onboard_open_base_url", {"domain": domain})
            _CUR_BASE[0] = result or ""
            return result
        NS["_onboard_open_base_url"] = ob_base
        NS["Request"] = _Req
        def urlopen_probe(req, timeout=None):
            path = req.full_url[len(_CUR_BASE[0]):]
            data = json.loads(req.data.decode("utf-8")) if req.data else None
            extra = {k: v for k, v in req.headers.items() if k != "Content-Type"}
            result = ctx.call("probe_get_json", ARGMAP["probe_get_json"](path, data, extra))
            return _RespCtx(json.dumps(result, ensure_ascii=False).encode("utf-8"))
        NS["urlopen"] = urlopen_probe
        NS["_parse_bot_response"] = make_recorder(ctx, "parse_bot_response")
        NS["_onboard_open_base_url_rec"] = make_recorder(ctx, "onboard_open_base_url")
    elif target == "_process_inbound_message":
        async def extract_rec(message):
            result = ctx.call("extract_content", ARGMAP["extract_content"](message))
            r = list(result)
            if len(r) > 1 and r[1] is not None:
                r[1] = MsgType(r[1])
            return tuple(r)
        _install(SELF, "_extract_message_content", extract_rec)
        NS["_strip_edge_self_mentions"] = make_recorder(ctx, "strip_edge_mentions")
        NS["_build_mention_hint"] = make_recorder(ctx, "build_mention_hint")
        _install(SELF, "_fetch_message_text", make_recorder(ctx, "fetch_message_text"))
        _install(SELF, "get_chat_info", make_recorder(ctx, "get_chat_info"))
        _install(SELF, "_resolve_sender_profile", make_recorder(ctx, "resolve_sender_profile"))
        _install(SELF, "_resolve_source_chat_type", make_recorder(ctx, "resolve_source_chat_type"))
        _install(SELF, "_resolve_channel_prompt", make_recorder(ctx, "resolve_channel_prompt"))
        _install(SELF, "build_source", make_recorder(ctx, "build_source"))
        NS["MessageEvent"] = make_recorder(ctx, "make_message_event")
        _install(SELF, "_dispatch_inbound_event", make_recorder(ctx, "dispatch_inbound"))
        NS["datetime"] = types.SimpleNamespace(now=lambda: "TS")
    elif target == "_qr_register_inner":
        NS["_init_registration"] = make_recorder(ctx, "init_registration")
        NS["_begin_registration"] = make_recorder(ctx, "begin_registration")
        NS["_render_qr"] = make_recorder(ctx, "render_qr")
        NS["install_hint"] = make_recorder(ctx, "install_hint")
        NS["_poll_registration"] = make_recorder(ctx, "poll_registration")
        NS["probe_bot"] = make_recorder(ctx, "probe_bot")
    elif target == "_reaction_call":
        _install(SELF, "_run_blocking", make_recorder(ctx, "run_blocking"))
        _install(SELF, "_response_succeeded", make_recorder(ctx, "response_succeeded"))
        # live does getattr(response, "data", None): materialize the scripted
        # response as an attribute double (a plain dict would raise).
        _orig_rb = SELF._run_blocking
        async def rb_mat(api, request):
            return mat_event(await _orig_rb(api, request))
        _install(SELF, "_run_blocking", rb_mat)
    elif target == "_read_binary_response":
        pass  # setup in live_call (per-case file double)
    elif target == "_read_limited_feishu_webhook_body":
        pass  # setup in live_call
    elif target == "_should_batch_media_event":
        pass
    elif target == "_resolve_media_message_type":
        pass
    elif target == "_queue_empty":
        NS["self"] = SELF
    elif target == "_render_qr":
        pass  # setup in live_call (per-case qrcode module)


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
    if func == "_poll_registration":
        return NS[func](device_code=g("device_code", ""), interval=g("interval", 0),
                        expire_in=g("expire_in", 0), domain=g("domain", "feishu"))
    if func == "_pop_text_batch":
        return NS[func](SELF, g("key", ""))
    if func == "_post_registration":
        return NS[func](g("base_url", ""), g("body", {}))
    if func == "_prepare_client":
        _install(SELF, "_domain_name", g("domain_name", ""))
        return NS[func](SELF)
    if func == "_probe_bot_http":
        return NS[func](g("app_id", ""), g("app_secret", ""), g("domain", ""))
    if func == "_process_inbound_message":
        return NS[func](SELF, data=mat_event(g("data")), message=mat_event(g("message")),
                        sender_id=mat_event(g("sender_id")), chat_type=g("chat_type", ""),
                        message_id=g("message_id", ""), is_bot=g("is_bot", False))
    if func == "_qr_register_inner":
        return NS[func](initial_domain=g("initial_domain", "feishu"),
                        timeout_seconds=g("timeout_seconds", 0))
    if func == "_reaction_call":
        build_request = make_recorder(ctx, "build_request")
        return NS[func](SELF, g("verb", ""), g("message_id", ""), g("ident", ""),
                        build_request, g("api"))
    if func == "_read_binary_response":
        spec = g("response") or {}
        fspec = spec.get("file") if isinstance(spec, dict) else None
        if not fspec:
            return NS[func](types.SimpleNamespace())
        if fspec.get("has_getvalue"):
            class _F:
                def getvalue(self):
                    r = ctx.call("getvalue", {})
                    return r if isinstance(r, bytes) else str(r).encode("utf-8")
            return NS[func](types.SimpleNamespace(file=_F()))
        class _F:
            def read(self):
                r = ctx.call("read", {})
                return r if isinstance(r, bytes) else str(r).encode("utf-8")
        return NS[func](types.SimpleNamespace(file=_F()))
    if func == "_read_limited_feishu_webhook_body":
        class _Content:
            async def readexactly(self, n):
                r = ctx.call("readexactly", {"n": n})
                if isinstance(r, dict) and "__partial__" in r:
                    p = r["__partial__"]
                    return p if isinstance(p, bytes) else str(p).encode("utf-8")
                return r if isinstance(r, bytes) else str(r).encode("utf-8")
        return NS[func](types.SimpleNamespace(content=_Content()), g("max_bytes", 0))
    if func == "_should_batch_media_event":
        ev = mat_event(g("event"))
        if isinstance(ev, types.SimpleNamespace) and getattr(ev, "message_type", None) is not None:
            object.__setattr__(ev, "message_type", MsgType(ev.message_type))
        return NS[func](SELF, ev)
    if func == "_resolve_media_message_type":
        return NS[func](g("media_type", ""), default=g("default", "text"))
    if func == "_queue_empty":
        _install(SELF, "_pending_inbound_events", list(g("events") or []))
        _install(SELF, "_pending_inbound_lock", _DummyLock())
        return NS[func]()
    if func == "_render_qr":
        if not g("available", False):
            NS["_qrcode_mod"] = None
        else:
            class _QR:
                def __init__(self): pass
                def add_data(self, url): return ctx.call("qr_add_data", {"url": url})
                def make(self, fit=True): return ctx.call("qr_make", {"fit": fit})
                def print_ascii(self, invert=True): return ctx.call("qr_print_ascii", {"invert": invert})
            class _Mod:
                @staticmethod
                def QRCode():
                    ctx.call("qr_construct", {})
                    return _QR()
            NS["_qrcode_mod"] = _Mod
        return NS[func](g("url", ""))
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
        print(f"FATAL oracle_feishu_adapter_flow12: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra = sorted(set(ARGMAP) - SEAMED)
    if extra:
        print(f"FATAL oracle_feishu_adapter_flow12: ARGMAP seams absent from C: {extra}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow12.jsonl"
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
            print(f"FATAL oracle_feishu_adapter_flow12: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        missing_funcs = sorted(set(TARGETS) - fix_funcs)
        if missing_funcs:
            print(f"FATAL oracle_feishu_adapter_flow12: targets without fixture cases: {missing_funcs}")
            sys.exit(2)
        unknown_funcs = sorted(fix_funcs - set(TARGETS))
        if unknown_funcs:
            print(f"FATAL oracle_feishu_adapter_flow12: fixture funcs not in TARGETS: {unknown_funcs}")
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
    print(f"ORACLE feishu_adapter_flow12: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow12.jsonl"
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
    print(f"SELFTEST feishu_adapter_flow12: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
