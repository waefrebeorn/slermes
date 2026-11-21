#!/usr/bin/env python3
"""Oracle for the Discord adapter FLOW part 13 port
(src/gateway/platforms/discord_adapter_flow13.c).

Drives the LIVE plugins/platforms/discord/adapter.py defs with SCRIPTED seams.
Every non-pure collaborator is a recording stub fed the SAME scripts the C
harness feeds its universal seam. Compares the ordered seam trace + final
return + state echo.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE discord_adapter_flow13: N cases, M mismatches'.

Representation contract:
- bytes/bytearray payloads are transported as ASCII-safe hex strings; the C
  port decodes them with the same convention.
- None == absent; JSON null is a real null value.
- raise spec: {"__raise__": kind, "message": msg}.
"""
import ast
import asyncio
import builtins
import contextlib
import copy
import enum
import json
import math
import os
import re
import subprocess
import sys
import textwrap
import types
from pathlib import Path
from types import SimpleNamespace
import __future__ as _fut

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
A_SRC = os.path.join(Q, "plugins", "platforms", "discord", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag
MAX_REDIRECTS = 10
REDIRECT_STATUSES = {301, 302, 303, 307, 308}
REKEY_FAILURE_STREAK = 25

# kind: instance | module
KINDS = {
    "_on_platform_message_edit": "instance",
    "_on_platform_message_delete": "instance",
    "_on_platform_thread_create": "instance",
    "_on_platform_thread_update": "instance",
    "_read_websocket_health": "instance",
    "_read_command_sync_state": "instance",
    "_record_command_sync_rate_limit": "instance",
    "_probe_audio_duration_seconds": "instance",
    "_process_voice_input": "instance",
    "_observe_exec_approval_settle": "instance",
    "_read_attachment_bytes": "instance",
    "_on_packet": "instance",
    "_read_url_image_with_redirect_guard": "module",
}
ORDER = list(KINDS)

# EXACTLY the C da13_call seam names (set-equality enforced in cross_check).
SEAM_NAMES = [
    # event-handler family
    "emit_platform_event", "message_event_parts", "thread_event_parts",
    # websocket health
    "ws_is_ready", "ws_is_closed", "perf_counter",
    # command-sync state
    "command_sync_state_path", "path_exists", "path_read_text", "json_loads",
    "time_time", "update_command_sync_entry",
    # audio probe
    "import_module", "mutagen_file", "subprocess_run",
    # voice input processing
    "named_tempfile", "to_thread", "is_whisper_hallucination", "logger_info",
    "voice_input_callback", "logger_warning", "os_unlink",
    # attachment read
    "validate_media_size", "attachment_read",
    # URL image redirect guard
    "is_safe_url", "session_get", "urljoin", "resp_read",
    # exec-approval settle
    "get_running_loop", "settle_reason", "view_retire", "loop_create_task",
    "bg_add", "add_done_callback", "call_soon_threadsafe", "observe_gateway_settle",
    # packet pipeline
    "logger_debug", "nacl_decrypt", "refresh_credentials", "infer_user_for_ssrc",
    "dave_decrypt", "monotonic", "opus_decode",
]


def seg(src, node):
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


_MOD = new_module("da13_oracle_adapter")
NS = _MOD.__dict__
NS["__builtins__"] = builtins


def extract():
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    nodes = {}
    mod_targets = {k for k, v in KINDS.items() if v == "module"}
    for n in tree.body:
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name in mod_targets:
            nodes[n.name] = n
    classes = {c.name: c for c in tree.body if isinstance(c, ast.ClassDef)}
    for cname in ("VoiceReceiver", "DiscordAdapter"):
        cls = classes.get(cname)
        if not cls:
            continue
        for n in cls.body:
            if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name in KINDS:
                nodes[n.name] = n
    missing = [k for k in KINDS if k not in nodes]
    if missing:
        print(f"FATAL oracle_discord_adapter_flow13: missing defs: {missing}")
        sys.exit(2)
    return nodes


_NODES = extract()


# ── canonical projection ──
def canon(v):
    def conv(x):
        tn = type(x).__name__
        if tn in ("DA13Double",):
            return "<self>"
        if tn == "_View":
            return conv(x._d)
        if tn in ("_Loop", "_Task", "_CM", "_Resp", "_Stamp", "_File", "_Proc",
                  "_Matcher", "_Client", "_Dave", "_OpusDecoder", "_Path"):
            return conv(getattr(x, "_proj", lambda: {})())
        if isinstance(x, enum.Enum):
            return x.value
        if isinstance(x, BaseException):
            return {"__raise__": type(x).__name__, "message": str(x)}
        if isinstance(x, types.SimpleNamespace):
            return {k: conv(val) for k, val in vars(x).items() if not callable(val)}
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, (bytes, bytearray)):
            return bytes(x).hex()
        if callable(x):
            return "<callable>"
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = asyncio.CancelledError if kind == "CancelledError" else getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C da13_call exactly) ──
_MISSING = object()
_CUR = None


class Ctx:
    def __init__(self, io, state):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.st = state if isinstance(state, dict) else {}
        self.inst = None

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": canon(args), "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return ret


class _NoBind:
    """Callable wrapper with no __get__, so class-attribute stubs never bind self."""
    def __init__(self, fn):
        self._fn = fn

    def __call__(self, *a, **k):
        return self._fn(*a, **k)


def rec(name, args_fn, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k)
        raw = _CUR.call(name, args)
        return build(raw) if build else raw
    return f


def arec(name, args_fn, build=None):
    async def f(*a, **k):
        args = args_fn(*a, **k)
        raw = _CUR.call(name, args)
        return build(raw) if build else raw
    return f


# ── object doubles ──
class _View:
    """view double for _observe_exec_approval_settle, backed by ctx.state['view']."""
    def __init__(self, d, retire):
        object.__setattr__(self, "_d", d)
        object.__setattr__(self, "retire", retire)

    def __getattr__(self, k):
        return self._d.get(k)

    def __setattr__(self, k, v):
        self._d[k] = v

    def _proj(self):
        return dict(self._d)


class _Loop:
    def __init__(self):
        self.create_task = rec("loop_create_task", lambda coro: {"coro": _proj_marker(coro)},
                               build=lambda raw: _Task(raw))
        self.call_soon_threadsafe = None  # set per case to call schedule()

    def _proj(self):
        return "<loop>"


class _BGTasks:
    """Set-like holder of background tasks used by _observe_exec_approval_settle."""
    def __init__(self, st):
        self.add = rec("bg_add", lambda task: {"task": _proj_marker(task)})
        self.discard = lambda task: None
        self.discard.__name__ = "discard"

    def _proj(self):
        return "<bg_tasks>"


class _Task:
    def __init__(self, raw):
        self._raw = raw
        self.add_done_callback = rec("add_done_callback", lambda cb: {"cb": _cb_marker(cb)})

    def _proj(self):
        return self._raw


class _CM:
    """async context manager for session.get(); fires session_get on __aenter__."""
    def __init__(self, args):
        self._args = args

    async def __aenter__(self):
        raw = _CUR.call("session_get", self._args)
        return _Resp(raw, arec("resp_read", lambda: {}))

    async def __aexit__(self, *a):
        return False

    def _proj(self):
        return dict(self._args)


class _Path:
    """path double for _read_command_sync_state."""
    def __init__(self, raw, st):
        self._raw = raw
        self.exists = rec("path_exists", lambda: {"path": _proj_marker(raw)})
        self.read_text = rec(
            "path_read_text", lambda encoding: {"path": _proj_marker(raw), "encoding": encoding})

    def _proj(self):
        return self._raw


class _Resp:
    """response double for the URL image guard."""
    def __init__(self, raw, read):
        d = raw if isinstance(raw, dict) else {}
        self.headers = d.get("headers", {})
        self.status = d.get("status", 0)
        self.read = read

    def _proj(self):
        return {"headers": self.headers, "status": self.status}


class _Stamp:
    def __init__(self, iso):
        self._iso = iso

    def isoformat(self):
        return self._iso

    def _proj(self):
        return {"__stamp__": self._iso}


class _File:
    def __init__(self, name):
        self.name = name
        self.closed = False

    def close(self):
        self.closed = True

    def _proj(self):
        return {"name": self.name}


class _Proc:
    def __init__(self, raw):
        self.returncode = raw.get("returncode", 0) if isinstance(raw, dict) else 0
        self.stdout = raw.get("stdout", "") if isinstance(raw, dict) else ""
        self.stderr = raw.get("stderr", None) if isinstance(raw, dict) else None

    def _proj(self):
        return {"returncode": self.returncode, "stdout": self.stdout}


class _Matcher:
    def __init__(self, seam):
        self._seam = seam

    def __call__(self, text):
        return _CUR.call(self._seam, {"text": text})

    def _proj(self):
        return "<matcher>"


class _Client:
    def __init__(self, d, is_ready, is_closed):
        self._d = d
        self.is_ready = is_ready
        self.is_closed = is_closed

    def __getattr__(self, k):
        return self._d.get(k)

    def _proj(self):
        return dict(self._d)


def _proj_marker(v):
    tn = type(v).__name__
    if tn == "_Task":
        return v._raw
    if tn == "_Loop":
        return "<loop>"
    if tn == "_View":
        return v._d
    return v


def _cb_marker(cb):
    n = getattr(cb, "__name__", "")
    if n == "_consume_background_task_result" or n == "discard":
        return n
    if hasattr(cb, "__self__") and type(cb.__self__).__name__ == "set":
        return "discard"
    return "<callback>"


def ns_double(v):
    if isinstance(v, dict):
        return SimpleNamespace(**{k: ns_double(x) for k, x in v.items()})
    if isinstance(v, list):
        return [ns_double(x) for x in v]
    return v


def stamp_double(v):
    """Materialize edited_at-style dicts into a _Stamp when tagged."""
    if isinstance(v, dict) and "__stamp__" in v:
        return _Stamp(v["__stamp__"])
    if isinstance(v, dict):
        return SimpleNamespace(**{k: stamp_double(x) for k, x in v.items()})
    if isinstance(v, list):
        return [stamp_double(x) for x in v]
    return v


# ── seam stubs ──
def build_seams():
    st = {}
    # event handlers
    async def _emit(event_type, build):
        built = build()
        return _CUR.call("emit_platform_event", {"event_type": event_type, "built": built})

    st["emit_platform_event"] = _emit

    def _mep(message, extra_payload):
        author = getattr(message, "author", None)
        extra = extra_payload(message, author)
        return _CUR.call("message_event_parts", {"message": message, "extra": extra})

    st["message_event_parts"] = _mep

    def _tep(thread, extra_payload):
        owner_id = getattr(thread, "owner_id", None)
        extra = extra_payload(thread, owner_id)
        return _CUR.call("thread_event_parts", {"thread": thread, "extra": extra})

    st["thread_event_parts"] = _tep

    # websocket health
    st["ws_is_ready"] = rec("ws_is_ready", lambda: {})
    st["ws_is_closed"] = rec("ws_is_closed", lambda: {})
    st["perf_counter"] = rec("perf_counter", lambda: {})

    # command sync state
    st["command_sync_state_path"] = rec(
        "command_sync_state_path", lambda: {}, build=lambda raw: _Path(raw, st))
    st["path_exists"] = rec("path_exists", lambda path: {"path": _proj_marker(path)})
    st["path_read_text"] = rec(
        "path_read_text", lambda path, encoding: {"path": _proj_marker(path), "encoding": encoding})
    st["json_loads"] = rec("json_loads", lambda text: {"text": text})
    st["time_time"] = rec("time_time", lambda: {})
    st["update_command_sync_entry"] = rec(
        "update_command_sync_entry",
        lambda app_id, fingerprint, keep_existing, fields: {
            "app_id": _proj_marker(app_id), "fingerprint": fingerprint,
            "keep_existing": keep_existing, "fields": fields})

    def _update_ces(app_id, fingerprint, *, keep_existing, fields):
        # The live `fields(now)` lambda calls time.time() itself (ignoring now).
        vals = fields(None)
        return _CUR.call("update_command_sync_entry", {
            "app_id": _proj_marker(app_id), "fingerprint": fingerprint,
            "keep_existing": keep_existing, "fields": vals})

    st["update_command_sync_entry"] = _update_ces

    # audio probe
    st["import_module"] = rec("import_module", lambda name: {"name": name})
    # the file's `mutagen = importlib.import_module("mutagen")` is used as mutagen.File(...)
    def _import_module(name):
        raw = _CUR.call("import_module", {"name": name})
        return SimpleNamespace(File=lambda path: st["mutagen_file"](path))
    st["import_module"] = _import_module
    st["mutagen_file"] = rec("mutagen_file", lambda path: {"path": path},
                             build=lambda raw: ns_double(raw))
    st["subprocess_run"] = rec(
        "subprocess_run",
        lambda cmd, check, capture_output, text, timeout, stdin: {
            "cmd": cmd, "check": check, "capture_output": capture_output,
            "text": text, "timeout": timeout, "stdin": _proj_marker(stdin)},
        build=lambda raw: _Proc(raw))

    # voice input
    st["named_tempfile"] = rec(
        "named_tempfile", lambda suffix, prefix, delete: {
            "suffix": suffix, "prefix": prefix, "delete": delete},
        build=lambda raw: _File(raw.get("name", "/tmp/vc.wav") if isinstance(raw, dict) else "/tmp/vc.wav"))
    st["to_thread"] = arec(
        "to_thread",
        lambda func, *a: {"func": _func_marker(func), "args": [_proj_marker(x) for x in a]})
    st["is_whisper_hallucination"] = rec(
        "is_whisper_hallucination", lambda transcript: {"transcript": transcript})
    st["logger_info"] = rec("logger_info", lambda *a: {"args": list(a)})
    st["voice_input_callback"] = arec(
        "voice_input_callback",
        lambda **kw: {"guild_id": kw.get("guild_id"), "user_id": kw.get("user_id"),
                      "transcript": kw.get("transcript")})
    st["logger_warning"] = rec("logger_warning", lambda *a, **k: {"args": list(a), "kwargs": dict(k)})
    st["os_unlink"] = rec("os_unlink", lambda path: {"path": path})

    # attachment read
    st["validate_media_size"] = rec(
        "validate_media_size", lambda size, media_type: {"size": size, "media_type": media_type})
    st["attachment_read"] = arec("attachment_read", lambda: {})

    # url image redirect guard
    st["is_safe_url"] = rec("is_safe_url", lambda url: {"url": url})
    st["session_get"] = None  # installed by a helper at drive time
    st["urljoin"] = rec("urljoin", lambda base, rel: {"base": base, "rel": rel})
    st["resp_read"] = arec("resp_read", lambda: {})

    # exec approval settle
    st["get_running_loop"] = rec("get_running_loop", lambda: {})
    st["settle_reason"] = rec("settle_reason", lambda: {})
    st["view_retire"] = arec("view_retire", lambda reason: {"reason": reason})
    st["loop_create_task"] = rec(
        "loop_create_task", lambda coro: {"coro": _proj_marker(coro)},
        build=lambda raw: _Task(raw))
    st["bg_add"] = rec("bg_add", lambda task: {"task": _proj_marker(task)})
    st["add_done_callback"] = rec("add_done_callback", lambda cb: {"cb": _cb_marker(cb)})
    st["call_soon_threadsafe"] = rec("call_soon_threadsafe", lambda schedule: {"schedule": "<schedule>"})
    st["observe_gateway_settle"] = rec(
        "observe_gateway_settle",
        lambda session_key, request_id, retire: {
            "session_key": session_key, "request_id": request_id, "retire": "<retire>"})

    # packet pipeline
    st["logger_debug"] = rec("logger_debug", lambda *a: {"args": list(a)})
    st["nacl_decrypt"] = rec(
        "nacl_decrypt",
        lambda secret_key, encrypted, header, nonce: {
            "secret_key": secret_key, "encrypted": encrypted, "header": header, "nonce": nonce},
        build=lambda raw: bytes.fromhex(raw) if isinstance(raw, str) else raw)
    st["refresh_credentials"] = rec(
        "refresh_credentials", lambda reason: {"reason": reason})
    st["infer_user_for_ssrc"] = rec(
        "infer_user_for_ssrc", lambda ssrc: {"ssrc": ssrc})
    st["dave_decrypt"] = rec(
        "dave_decrypt", lambda user_id, media_type, data: {
            "user_id": user_id, "media_type": media_type, "data": data},
        build=lambda raw: bytes.fromhex(raw) if isinstance(raw, str) else raw)
    st["monotonic"] = rec("monotonic", lambda: {})
    st["opus_decode"] = rec(
        "opus_decode", lambda ssrc, data: {"ssrc": ssrc, "data": data})
    return st


def _func_marker(func):
    n = getattr(func, "__name__", "")
    if n == "pcm_to_wav":
        return "pcm_to_wav"
    if n == "transcribe_audio":
        return "transcribe_audio"
    return n or "<func>"


# ── synthetic class hosting the ported instance methods ──
class BaseDouble:
    pass


NS["BaseDouble"] = BaseDouble
_syn = ast.ClassDef(
    name="DA13Double", bases=[ast.Name(id="BaseDouble", ctx=ast.Load())],
    keywords=[], body=[_NODES[n] for n in ORDER if KINDS[n] == "instance"],
    decorator_list=[])
ast.fix_missing_locations(_syn)
_mod_node = ast.Module(body=[_syn], type_ignores=[])
ast.fix_missing_locations(_mod_node)
exec(compile(_mod_node, A_SRC, "exec", flags=_FLAGS, dont_inherit=True), NS)
DA13Double = NS["DA13Double"]
PORTED = {n: DA13Double.__dict__[n] for n in ORDER if KINDS[n] == "instance"}
# module-level function: exec into NS directly
exec_node(open(A_SRC, encoding="utf-8").read(), _NODES["_read_url_image_with_redirect_guard"], NS, A_SRC)
PORTED["_read_url_image_with_redirect_guard"] = NS["_read_url_image_with_redirect_guard"]


METHOD_STUBS = {
    "_emit_platform_event": "emit_platform_event",
    "_message_event_parts": "message_event_parts",
    "_thread_event_parts": "thread_event_parts",
    "refresh_credentials": "refresh_credentials",
    "_infer_user_for_ssrc": "infer_user_for_ssrc",
    "_update_command_sync_entry": "update_command_sync_entry",
    "_command_sync_state_path": "command_sync_state_path",
}


def _mod(name, **attrs):
    m = sys.modules.get(name) or types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def seed(st):
    NS["logger"] = SimpleNamespace(
        info=st["logger_info"], debug=st["logger_debug"],
        warning=st["logger_warning"],
        error=rec("logger_error", lambda *a: {"args": list(a)}))
    NS["logger_error"] = rec("logger_error", lambda *a: {"args": list(a)})
    NS["time"] = SimpleNamespace(
        monotonic=st["monotonic"], perf_counter=st["perf_counter"], time=st["time_time"])
    NS["math"] = math
    NS["struct"] = __import__("struct")
    NS["contextlib"] = contextlib
    NS["os"] = SimpleNamespace(unlink=st["os_unlink"], DEVNULL=-3)
    NS["subprocess"] = SimpleNamespace(
        run=st["subprocess_run"], DEVNULL=-3, CalledProcessError=subprocess.CalledProcessError)
    NS["tempfile"] = SimpleNamespace(NamedTemporaryFile=st["named_tempfile"])
    NS["asyncio"] = SimpleNamespace(to_thread=st["to_thread"])
    NS["is_safe_url"] = st["is_safe_url"]
    NS["json"] = SimpleNamespace(loads=st["json_loads"])
    NS["urljoin"] = st["urljoin"]
    NS["validate_inbound_media_size"] = st["validate_media_size"]
    NS["_DISCORD_IMAGE_MAX_REDIRECTS"] = MAX_REDIRECTS
    NS["_DISCORD_IMAGE_REDIRECT_STATUSES"] = REDIRECT_STATUSES
    def _consume_bg(task):
        return None
    _consume_bg.__name__ = "_consume_background_task_result"
    NS["_consume_background_task_result"] = _consume_bg
    # in-function imports
    _mod("importlib", import_module=st["import_module"], util=__import__("importlib.util", fromlist=["util"]))
    tm = _mod("tools")
    tm.__path__ = []
    _mod("tools.voice_mode_transcript", is_whisper_hallucination=st["is_whisper_hallucination"])
    _mod("tools.transcription_tools", transcribe_audio=_func_marker_fn("transcribe_audio", st))
    _mod("tools.approval", observe_gateway_settle=st["observe_gateway_settle"])
    # nacl / davey / discord.opus imported inside the packet pipeline
    nm = _mod("nacl")
    nm.secret = _mod("nacl.secret", Aead=lambda key: SimpleNamespace(
        decrypt=lambda e, h, n: st["nacl_decrypt"](key, e, h, n)))
    _mod("davey", MediaType=SimpleNamespace(audio="audio"))
    _mod("discord")
    _mod("discord.opus", Decoder=_OpusCtor(st))
    NS["discord"] = SimpleNamespace(opus=SimpleNamespace(Decoder=_OpusCtor(st)))
    NS["VoiceReceiver"] = SimpleNamespace(pcm_to_wav=_func_marker_fn("pcm_to_wav", st))
    DA13Double.REKEY_FAILURE_STREAK = REKEY_FAILURE_STREAK
    DA13Double._lock = contextlib.nullcontext()
    DA13Double._background_tasks = _BGTasks(st)
    for m, seam in METHOD_STUBS.items():
        setattr(DA13Double, m, _NoBind(st[seam]))


def _func_marker_fn(name, st):
    """A plain named callable used only as an asyncio.to_thread function marker."""
    def f(*a, **k):
        return None
    f.__name__ = name
    return f


class _OpusCtor:
    """Callable returning decoder objects whose decode() fires the opus_decode seam.

    The SSRC is supplied through the per-case Ctx (set from the packet bytes),
    mirroring how the C port derives it from the same packet.
    """
    def __init__(self, st):
        self._st = st

    def __call__(self):
        return _OpusDecoder(self._st)


class _OpusDecoder:
    def __init__(self, st):
        self._st = st

    def decode(self, data):
        return self._st["opus_decode"](_CUR.ssrc, data)

    def _proj(self):
        return "<decoder>"


class _Dave:
    """DAVE session double: decrypt() fires the dave_decrypt seam."""
    def __init__(self, st, tag):
        self._st = st
        self._tag = tag

    def decrypt(self, user_id, media_type, data):
        return self._st["dave_decrypt"](user_id, media_type, data)

    def _proj(self):
        return {"__dave__": self._tag}


def dave_double(v, st):
    """Materialize a __dave__-tagged state entry into a _Dave recorder."""
    if isinstance(v, dict) and "__dave__" in v:
        return _Dave(st, v["__dave__"])
    return v


def make_self(state, st):
    inst = DA13Double()
    for k, v in (state or {}).items():
        vv = copy.deepcopy(v)
        if k == "_creds" and isinstance(vv, list) and len(vv) >= 2:
            vv[1] = dave_double(vv[1], st)
        setattr(inst, k, vv)
    return inst


def run_call(fn, *a, **kw):
    r = fn(*a, **kw)
    if asyncio.iscoroutine(r):
        return asyncio.run(r)
    return r


def drive(func, inp):
    global _CUR
    args = copy.deepcopy(inp.get("args") or {})
    state = copy.deepcopy(inp.get("state") or {})
    ctx = Ctx(inp.get("io"), state)
    _CUR = ctx
    st = build_seams()
    seed(st)
    inst = make_self(state, st)
    ctx.inst = inst
    fn = PORTED[func]

    def A(k):
        return stamp_double(copy.deepcopy(args.get(k)))

    try:
        # ── event handlers ──
        if func == "_on_platform_message_edit":
            ret = run_call(fn, inst, stamp_double(A("before")), stamp_double(A("after")))

        elif func == "_on_platform_message_delete":
            ret = run_call(fn, inst, A("message"))

        elif func == "_on_platform_thread_create":
            ret = run_call(fn, inst, A("thread"))

        elif func == "_on_platform_thread_update":
            ret = run_call(fn, inst, A("before"), A("after"))

        # ── websocket health ──
        elif func == "_read_websocket_health":
            cd = args.get("client") or {}
            client = _Client(
                copy.deepcopy(cd),
                rec("ws_is_ready", lambda: {}),
                rec("ws_is_closed", lambda: {}))
            for k in ("ws", "latency"):
                if k in client._d:
                    setattr(client, k, ns_double(client._d[k]))
            ret = fn(inst, client)

        # ── command sync state ──
        elif func == "_read_command_sync_state":
            ret = fn(inst)

        elif func == "_record_command_sync_rate_limit":
            ret = fn(inst, args.get("app_id"), args.get("fingerprint", ""),
                     args.get("retry_after", 0))

        # ── audio probe ──
        elif func == "_probe_audio_duration_seconds":
            ret = fn(inst, args.get("audio_path", ""))

        # ── voice input ──
        elif func == "_process_voice_input":
            if args.get("voice_cb"):
                inst._voice_input_callback = st["voice_input_callback"]
            elif "_voice_input_callback" in state:
                inst._voice_input_callback = st["voice_input_callback"]
            vtc = args.get("voice_text_channels") or {}
            inst._voice_text_channels = {int(k): v for k, v in vtc.items()}
            ret = run_call(fn, inst, args.get("guild_id"), args.get("user_id"),
                           args.get("pcm_data", ""), args.get("captured_for"))

        # ── attachment read ──
        elif func == "_read_attachment_bytes":
            att = A("att")
            reader = args.get("att", {}).get("read") if isinstance(args.get("att"), dict) else None
            if reader == "callable":
                att.read = arec("attachment_read", lambda: {})
            ret = run_call(fn, inst, att, media_type=args.get("media_type", "media"))

        # ── url image guard ──
        elif func == "_read_url_image_with_redirect_guard":
            def _session_get(url, timeout, allow_redirects, **kw):
                return _CM({"url": url, "timeout": timeout,
                            "allow_redirects": allow_redirects, **kw})
            session = SimpleNamespace(get=_session_get)
            ret = run_call(fn, session, args.get("url", ""),
                           timeout=args.get("timeout"),
                           request_kwargs=copy.deepcopy(args.get("request_kwargs") or {}))

        # ── exec approval settle ──
        elif func == "_observe_exec_approval_settle":
            vd = copy.deepcopy(args.get("view") or {})
            ctx.st["view"] = vd
            loop = _Loop()

            def _vretire(reason):
                return run_call(st["view_retire"], reason)

            view = _View(vd, _vretire)
            inst.view = view  # echo the view's mutated fields via state

            def _cst(schedule):
                r = _CUR.call("call_soon_threadsafe", {})
                if ta13_script_calls_schedule(_CUR, "call_soon_threadsafe"):
                    schedule()
                return r

            loop.call_soon_threadsafe = _cst

            def _getloop():
                _CUR.call("get_running_loop", {})
                return loop

            NS["asyncio"] = SimpleNamespace(get_running_loop=_getloop)

            def _observe(session_key, request_id, retire):
                result = _CUR.call("observe_gateway_settle", {
                    "session_key": session_key, "request_id": request_id})
                reason = _CUR.call("settle_reason", {})
                retire(reason)
                return result

            sys.modules["tools.approval"].observe_gateway_settle = _observe
            ret = fn(inst, args.get("session_key", ""), view)

        # ── packet pipeline ──
        elif func == "_on_packet":
            data = bytes.fromhex(args.get("data", ""))
            _CUR.ssrc = int.from_bytes(data[8:12], "big") if len(data) >= 12 else 0
            for key in ("_ssrc_to_user", "_decoders", "_buffers", "_last_packet_time"):
                d = getattr(inst, key, None)
                if isinstance(d, dict):
                    setattr(inst, key, {int(k): v for k, v in d.items()})
            ret = fn(inst, data)

        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        ret = proj_exc(exc)

    echo = {}
    for k, v in vars(inst).items():
        if callable(v) or k.startswith("__"):
            continue
        echo[k] = v
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(echo)}


def ta13_script_calls_schedule(ctx, name):
    """True when the fixture explicitly opts into invoking the scheduled callback."""
    entry = ctx.io.get(name)
    if isinstance(entry, dict):
        return bool(entry.get("__call_schedule__"))
    if isinstance(entry, list) and entry:
        first = entry[0]
        return isinstance(first, dict) and bool(first.get("__call_schedule__"))
    return True  # default: exercise the callback path


# ── cross-checks ──
def cross_check():
    csrc = (_REPO / "src" / "gateway" / "platforms" /
            "discord_adapter_flow13.c").read_text(encoding="utf-8")
    c_seams = set(re.findall(r'da13_call\(\s*ctx,\s*"([^"]+)"', csrc))
    if c_seams != set(SEAM_NAMES):
        print(f"FATAL oracle_discord_adapter_flow13: C seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - c_seams)} extra={sorted(c_seams - set(SEAM_NAMES))}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'(?:rec|arec)\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'_CUR\.call\(\s*"([^"]+)"', osrc))
    used &= set(SEAM_NAMES)
    if used != set(SEAM_NAMES):
        print(f"FATAL oracle_discord_adapter_flow13: oracle seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - used)} extra={sorted(used - set(SEAM_NAMES))}")
        sys.exit(2)
    pops = set(re.findall(
        r'PoP:\s*da13_\w+\s*@\s*plugins/platforms/discord/adapter\.py:(\w+)', csrc))
    if pops != set(KINDS):
        print(f"FATAL oracle_discord_adapter_flow13: PoP/KINDS drift: "
              f"c={sorted(pops)} k={sorted(KINDS)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow13.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in fix.open(encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func", ""))
        if fix_keys != set(SEAM_NAMES):
            print(f"FATAL oracle_discord_adapter_flow13: fixture io key set != SEAM_NAMES: "
                  f"missing={sorted(set(SEAM_NAMES) - fix_keys)} extra={sorted(fix_keys - set(SEAM_NAMES))}")
            sys.exit(2)
        if fix_funcs != set(KINDS):
            print(f"FATAL oracle_discord_adapter_flow13: fixture func set != KINDS: "
                  f"missing={sorted(set(KINDS) - fix_funcs)} extra={sorted(fix_funcs - set(KINDS))}")
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
    if shown >= 30:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:800]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:800]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:800]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:800]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")
        for k in sorted(keys)[:4]:
            print(f"    {k}: want={json.dumps(ws.get(k), ensure_ascii=False)[:300]} got={json.dumps(gs.get(k), ensure_ascii=False)[:300]}")


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
    print(f"ORACLE discord_adapter_flow13: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow13.jsonl"
    n = bad = 0
    counts = {}
    for line in fix.open(encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            w = drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
            continue
        r = w.get("ret")
        if isinstance(r, dict) and "__raise__" in r:
            msg = r.get("message", "")
            bad_flags = ("takes ", "got multiple values", "missing 1 required",
                         "is not defined", "is not a container", "unexpected keyword",
                         "not callable", "object has no attribute")
            if any(fl in msg for fl in bad_flags):
                bad += 1
                print(f"SELFTEST SUSPICIOUS RAISE {func} case {n}: {r}")
    if set(counts) != set(KINDS):
        print(f"SELFTEST KINDS DRIFT: fixture funcs={sorted(counts)} kinds={sorted(KINDS)}")
        bad += 1
    print(f"SELFTEST discord_adapter_flow13: {n} cases, {bad} drive failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


def dump():
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow13.jsonl"
    out = _REPO / "tests" / "fixtures" / "discord_adapter_flow13.wants.jsonl"
    n = 0
    with out.open("w", encoding="utf-8", newline="\n") as f:
        for line in fix.open(encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            try:
                w = drive(case["func"], case.get("in") or {})
            except BaseException as exc:
                w = {"ret": proj_exc(exc), "trace": [], "state": {}, "_drive_error": True}
            f.write(json.dumps({"func": case["func"], "in": case.get("in"), "out": w},
                               ensure_ascii=False) + "\n")
            n += 1
    print(f"dumped {n} wants to {out}")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    if len(sys.argv) > 1 and sys.argv[1] == "--dump":
        dump()
        sys.exit(0)
    main()
