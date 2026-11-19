#!/usr/bin/env python3
"""Oracle for the Telegram adapter FLOW remainder port
(src/gateway/platforms/telegram_adapter_flow2.c).

Drives the LIVE plugins/platforms/telegram/adapter.py polling/identity/config/
media decision chains with SCRIPTED seams: claimed flow1 siblings
(_chat_type_str, _metadata_thread_id, _thread_kwargs_for_send,
_reply_to_message_id_for_send, _notification_kwargs) and unported collaborators
(_bank_conflict_quiet_window, _note_conflict_quiet_progress,
_schedule_polling_recovery, the set-returning gate wrappers, _extra_or_secret,
_decode_json_list_literal, mark_send_path_recovered, normalize_telegram_chat_id,
asyncio.Event, time.monotonic, open/file read, logger) are recording stubs fed
the SAME scripts the C harness feeds its universal seam. Ported siblings compose
for REAL on the double class. Compares the ordered seam trace + final return +
the MUTATED adapter state echo.

The live defs are AST-extracted from the TelegramAdapter class body (decorators
stripped by lineno, segments dedented) plus the _PollingStallError class, the
_teardown_started property and module/class constants. Every exec namespace is a
registered sys.modules module (py3.14 annotation resolution). The heavy quarry
import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE telegram_adapter_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- in.state is MUTATED and echoed. Object carriers materialize as attribute
  doubles: config/_bot/platform -> SimpleNamespace (config.extra stays a dict);
  _polling_progress_event -> EventDouble {"id","set"}; verifier/error tasks ->
  TaskDouble {"id"}. Event/Task identity is a per-id registry (C compares ids).
  Event.set() flips the echoed "set" flag on both sides; is_set/set/done/cancel
  and asyncio.Event() construction are seams. Base properties model from state:
  name == platform.value.title(), has_fatal_error == (_fatal_error_message is
  not None), _teardown_started == getattr(_polling_teardown_started, False).
- in.errors: id -> {"class","message","base"} registry (str(error)=message).
- Set-returning seams are scripted as SORTED lists; sets project to sorted lists.
- logger seams carry {msg, args} (format + raw projected args, never formatted).
- Scripted seam raises round-trip bare; fail-closed missing/exhausted scripts
  are RuntimeError recorded in the trace before propagation.
"""
import ast
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
A_SRC = os.path.join(Q, "plugins", "platforms", "telegram", "adapter.py")

_FLAGS = _fut.annotations.compiler_flag

KINDS = {
    "_fence_polling": "instance",
    "_begin_polling_generation": "instance",
    "_record_polling_progress": "instance",
    "_observe_polling_request_result": "instance",
    "_record_updates_received": "instance",
    "_verifier_stale": "instance",
    "_recovery_in_flight": "instance",
    "_check_ingress_dispatch_stall": "instance",
    "_current_bot_username": "instance",
    "_note_bot_username": "instance",
    "_observe_bot_identity_from_message": "instance",
    "_bot_identity_is_fresh": "instance",
    "_is_reply_to_bot": "instance",
    "_effective_message_thread_id": "classmethod",
    "_extra_bool": "instance",
    "_extra_str_set": "instance",
    "_telegram_ignored_threads": "instance",
    "_telegram_observe_allowed_chats": "instance",
    "_telegram_is_free_response_topic": "instance",
    "_int_or_zero": "staticmethod",
    "_telegram_media_too_large_note": "instance",
    "_telegram_media_size_allowed": "instance",
    "_missing_media_path_error": "instance",
    "_media_send_kwargs": "instance",
    "_sniff_raster_format": "staticmethod",
    "_cold_boot_drop_pending": "instance",
    "_looks_like_polling_conflict": "staticmethod",
}
CLASS_CONSTS = ["_GENERAL_TOPIC_THREAD_ID", "_BOT_IDENTITY_TTL_SECONDS"]
MOD_CONSTS = ["_INGRESS_DISPATCH_STALL_HEARTBEATS", "_MEDIA_SEND_READ_TIMEOUT"]
# pure-leaf helpers composed by ported flows: bound REAL on the double class,
# transcribed inline in C (no PoP re-claim, no seam).
HELPERS = {"_chat_id_str": "staticmethod", "_topic_id_or_general": "classmethod"}


def seg(src, node):
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


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
    mod = new_module("ta2_oracle_adapter")
    ns = mod.__dict__
    ns.update({"re": re, "os": os, "json": json, "logger": _SilentLogger(),
               "time": None, "asyncio": None, "open": None,
               "_extra_or_secret": None, "_decode_json_list_literal": None,
               "mark_send_path_recovered": None, "normalize_telegram_chat_id": None})
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # module-level constants + _PollingStallError class
    for node in tree.body:
        if isinstance(node, ast.Assign):
            names = [t.id for t in node.targets if isinstance(t, ast.Name)]
            if any(nm in MOD_CONSTS for nm in names):
                exec_node(src, node, ns, A_SRC)
        elif isinstance(node, ast.ClassDef) and node.name == "_PollingStallError":
            exec_node(src, node, ns, A_SRC)
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "TelegramAdapter")
    for node in cls.body:
        if isinstance(node, ast.Assign):
            names = [t.id for t in node.targets if isinstance(t, ast.Name)]
            if any(nm in CLASS_CONSTS for nm in names):
                exec_node(src, node, ns, A_SRC)
    for node in cls.body:
        if isinstance(node, ast.FunctionDef) and (node.name in KINDS or node.name in HELPERS or node.name == "_teardown_started"):
            exec_node(src, node, ns, A_SRC)
    ns["logger"] = _SilentLogger()
    required = list(KINDS) + list(HELPERS) + ["_teardown_started", "_PollingStallError"] + CLASS_CONSTS + MOD_CONSTS
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_telegram_adapter_flow2: missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()


# ── canonical projection ──
def canon(v):
    def conv(x):
        if isinstance(x, EventDouble):
            return {"id": x.id, "set": x._set}
        if isinstance(x, types.SimpleNamespace):
            return {k: conv(val) for k, val in vars(x).items()}
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


BUILTIN = {
    "Exception": Exception, "BaseException": BaseException, "OSError": OSError,
    "IOError": OSError, "TimeoutError": TimeoutError, "ConnectionError": ConnectionError,
    "ConnectionResetError": ConnectionResetError, "BrokenPipeError": BrokenPipeError,
    "PermissionError": PermissionError, "FileNotFoundError": FileNotFoundError,
    "AttributeError": AttributeError, "TypeError": TypeError, "ValueError": ValueError,
    "RuntimeError": RuntimeError, "NotImplementedError": NotImplementedError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "KeyError": KeyError, "IndexError": IndexError, "LookupError": LookupError,
    "ArithmeticError": ArithmeticError, "OverflowError": OverflowError,
    "StopIteration": StopIteration, "RecursionError": RecursionError,
    "MemoryError": MemoryError, "NameError": NameError, "AssertionError": AssertionError,
    "EOFError": EOFError, "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "UnicodeError": UnicodeError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = BUILTIN.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C ta2_call exactly) ──
_MISSING = object()
_CUR = None
_EV_REG = {}
_TASK_REG = {}


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
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
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret


def rec(name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = _CUR.call(name, args)
        return build(raw) if build else raw
    return f


# ── object doubles ──
class EventDouble(types.SimpleNamespace):
    def is_set(self):
        return _CUR.call("event_is_set", {"id": self.id})

    def set(self):
        r = _CUR.call("event_set", {"id": self.id})
        self._set = True
        return r


class TaskDouble(types.SimpleNamespace):
    def done(self):
        return _CUR.call("task_done", {"id": self.id})

    def cancel(self):
        return _CUR.call("task_cancel", {"id": self.id})


class RequestDouble:
    def parse_json_payload(self, payload):
        return _CUR.call("request_parse_json_payload", {"payload": payload})


class FileDouble:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    def read(self, n):
        hexstr = _CUR.call("file_read", {"n": n})
        return bytes.fromhex(hexstr) if isinstance(hexstr, str) else b""


def ev_double(eid, set_flag=False):
    if eid in _EV_REG:
        return _EV_REG[eid]
    d = EventDouble(id=eid, _set=set_flag)
    _EV_REG[eid] = d
    return d


def task_double(eid):
    if eid in _TASK_REG:
        return _TASK_REG[eid]
    d = TaskDouble(id=eid)
    _TASK_REG[eid] = d
    return d


def event_factory():
    evid = _CUR.call("event_create", {})
    d = EventDouble(id=evid, _set=False)
    _EV_REG[evid] = d
    return d


class RecLogger:
    def __getattr__(self, level):
        def log(msg, *args, **kw):
            proj = [str(a) if isinstance(a, BaseException) else a for a in args]
            return _CUR.call(f"logger.{level}", {"msg": msg, "args": proj})
        return log


def open_stub(path, mode="r", *a, **k):
    _CUR.call("open", {"path": path, "mode": mode})
    return FileDouble()


def extra_or_secret_stub(extra, key, env, default="", **kw):
    args = {"extra": extra, "key": key, "env_name": env, "default": default}
    args.update(kw)
    return _CUR.call("_extra_or_secret", args)


def build_errors(spec):
    spec = spec or {}
    objs, classes = {}, {}
    for eid, e in spec.items():
        name = e.get("class", "Exception")
        cls = BUILTIN.get(name)
        if cls is None:
            base = BUILTIN.get(e.get("base", "Exception"), Exception)
            cls = type(name, (base,), {})
        classes[eid] = cls
    for eid, e in spec.items():
        objs[eid] = classes[eid](e.get("message", ""))
    return objs


def ns_double(v):
    if isinstance(v, dict):
        return types.SimpleNamespace(**{k: ns_double(x) for k, x in v.items()})
    if isinstance(v, list):
        return [ns_double(x) for x in v]
    return v


def mat_state(k, v):
    if k in ("config", "_bot", "platform") and isinstance(v, dict):
        return types.SimpleNamespace(**v)
    if k == "_polling_progress_event" and isinstance(v, dict):
        return ev_double(v.get("id"), v.get("set", False))
    if k in ("_polling_progress_verifier_task", "_polling_error_task") and isinstance(v, dict):
        return task_double(v.get("id"))
    return v


# ── seamed collaborator stubs (bound under the LIVE attribute name) ──
def _chat_type_str_stub(chat):
    return _CUR.call("self._chat_type_str", {"chat": chat})


def _bank_conflict_quiet_window_stub(self):
    return _CUR.call("self._bank_conflict_quiet_window", {})


def _note_conflict_quiet_progress_stub(self, now):
    return _CUR.call("self._note_conflict_quiet_progress", {"now": now})


def _schedule_polling_recovery_stub(self, error, *, reason):
    return _CUR.call("self._schedule_polling_recovery",
                     {"error": {"class": type(error).__name__, "message": str(error)}, "reason": reason})


def _reply_to_message_id_for_send_stub(cls, reply_to, metadata=None, reply_to_mode=None):
    return _CUR.call("self._reply_to_message_id_for_send",
                     {"reply_to": reply_to, "metadata": metadata, "reply_to_mode": reply_to_mode})


def _metadata_thread_id_stub(cls, metadata):
    return _CUR.call("self._metadata_thread_id", {"metadata": metadata})


def _thread_kwargs_for_send_stub(cls, chat_id, thread_id, metadata=None,
                                 reply_to_message_id=None, reply_to_mode=None):
    return _CUR.call("self._thread_kwargs_for_send",
                     {"chat_id": chat_id, "thread_id": thread_id, "metadata": metadata,
                      "reply_to_message_id": reply_to_message_id, "reply_to_mode": reply_to_mode})


def _notification_kwargs_stub(self, metadata):
    return _CUR.call("self._notification_kwargs", {"metadata": metadata})


def _telegram_group_allowed_chats_stub(self):
    return set(_CUR.call("self._telegram_group_allowed_chats", {}))


def _telegram_allowed_chats_stub(self):
    return set(_CUR.call("self._telegram_allowed_chats", {}))


def _telegram_free_response_topics_stub(self):
    return set(_CUR.call("self._telegram_free_response_topics", {}))


def make_double(state):
    clsdict = {}
    for name, kind in KINDS.items():
        fn = NS[name]
        if kind == "classmethod":
            clsdict[name] = classmethod(fn)
        elif kind == "staticmethod":
            clsdict[name] = staticmethod(fn)
        else:
            clsdict[name] = fn
    clsdict["_teardown_started"] = property(NS["_teardown_started"])
    clsdict["name"] = property(lambda self: self.platform.value.title())
    clsdict["has_fatal_error"] = property(lambda self: self._fatal_error_message is not None)
    for hname, hkind in HELPERS.items():
        clsdict[hname] = staticmethod(NS[hname]) if hkind == "staticmethod" else classmethod(NS[hname])
    for cname in CLASS_CONSTS:
        clsdict[cname] = NS[cname]
    # seamed collaborators AFTER the ported methods
    clsdict["_chat_type_str"] = staticmethod(_chat_type_str_stub)
    clsdict["_bank_conflict_quiet_window"] = _bank_conflict_quiet_window_stub
    clsdict["_note_conflict_quiet_progress"] = _note_conflict_quiet_progress_stub
    clsdict["_schedule_polling_recovery"] = _schedule_polling_recovery_stub
    clsdict["_reply_to_message_id_for_send"] = classmethod(_reply_to_message_id_for_send_stub)
    clsdict["_metadata_thread_id"] = classmethod(_metadata_thread_id_stub)
    clsdict["_thread_kwargs_for_send"] = classmethod(_thread_kwargs_for_send_stub)
    clsdict["_notification_kwargs"] = _notification_kwargs_stub
    clsdict["_telegram_group_allowed_chats"] = _telegram_group_allowed_chats_stub
    clsdict["_telegram_allowed_chats"] = _telegram_allowed_chats_stub
    clsdict["_telegram_free_response_topics"] = _telegram_free_response_topics_stub
    D = type("TelegramAdapter", (), clsdict)
    inst = D()
    for k, v in (state or {}).items():
        setattr(inst, k, mat_state(k, v))
    return D, inst


def drive(func, inp):
    global _CUR, _EV_REG, _TASK_REG
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    _CUR = ctx
    _EV_REG = {}
    _TASK_REG = {}
    objs = build_errors(inp.get("errors"))
    NS["time"] = types.SimpleNamespace(monotonic=rec("monotonic", lambda: {}))
    NS["asyncio"] = types.SimpleNamespace(Event=event_factory)
    NS["logger"] = RecLogger()
    NS["open"] = open_stub
    NS["_extra_or_secret"] = extra_or_secret_stub
    NS["_decode_json_list_literal"] = rec("_decode_json_list_literal", lambda raw: {"raw": raw})
    NS["mark_send_path_recovered"] = rec("mark_send_path_recovered", lambda adapter: {"adapter": "self"})
    NS["normalize_telegram_chat_id"] = rec("normalize_telegram_chat_id", lambda chat_id: {"chat_id": chat_id})
    D, inst = make_double(state)
    try:
        if func == "_fence_polling":
            ret = inst._fence_polling()
        elif func == "_begin_polling_generation":
            ret = inst._begin_polling_generation()
        elif func == "_record_polling_progress":
            ret = inst._record_polling_progress(args.get("generation"))
        elif func == "_observe_polling_request_result":
            req = args.get("request")
            reqd = RequestDouble() if isinstance(req, dict) else None
            ret = inst._observe_polling_request_result(reqd, args.get("generation"), args.get("result"))
        elif func == "_record_updates_received":
            ret = inst._record_updates_received(args.get("result"))
        elif func == "_verifier_stale":
            pg = args.get("progress")
            pgd = ev_double(pg["id"]) if isinstance(pg, dict) else None
            ret = inst._verifier_stale(args.get("generation"), pgd)
        elif func == "_recovery_in_flight":
            ret = inst._recovery_in_flight()
        elif func == "_check_ingress_dispatch_stall":
            ret = inst._check_ingress_dispatch_stall()
        elif func == "_current_bot_username":
            ret = inst._current_bot_username()
        elif func == "_note_bot_username":
            ret = inst._note_bot_username(args.get("username"))
        elif func == "_observe_bot_identity_from_message":
            ret = inst._observe_bot_identity_from_message(ns_double(args.get("message")))
        elif func == "_bot_identity_is_fresh":
            ret = inst._bot_identity_is_fresh()
        elif func == "_is_reply_to_bot":
            ret = inst._is_reply_to_bot(ns_double(args.get("message")))
        elif func == "_effective_message_thread_id":
            ret = D._effective_message_thread_id(ns_double(args.get("message")))
        elif func == "_extra_bool":
            ret = inst._extra_bool(args.get("key"), args.get("env_name"), args.get("default"),
                                   *args.get("fallback_keys", []))
        elif func == "_extra_str_set":
            ret = inst._extra_str_set(args.get("key"), args.get("env_name"))
        elif func == "_telegram_ignored_threads":
            ret = inst._telegram_ignored_threads()
        elif func == "_telegram_observe_allowed_chats":
            ret = inst._telegram_observe_allowed_chats()
        elif func == "_telegram_is_free_response_topic":
            ret = inst._telegram_is_free_response_topic(ns_double(args.get("message")))
        elif func == "_int_or_zero":
            ret = D._int_or_zero(args.get("value"))
        elif func == "_telegram_media_too_large_note":
            ret = inst._telegram_media_too_large_note(args.get("label"), args.get("file_size"), args.get("max_bytes"))
        elif func == "_telegram_media_size_allowed":
            ret = inst._telegram_media_size_allowed(ns_double(args.get("source")), args.get("label"))
        elif func == "_missing_media_path_error":
            ret = inst._missing_media_path_error(args.get("label"), args.get("path"))
        elif func == "_media_send_kwargs":
            ret = inst._media_send_kwargs(args.get("chat_id"), args.get("reply_to"), args.get("metadata"))
        elif func == "_sniff_raster_format":
            ret = D._sniff_raster_format(args.get("image_path"))
        elif func == "_cold_boot_drop_pending":
            ret = inst._cold_boot_drop_pending(is_reconnect=args.get("is_reconnect", False))
        elif func == "_looks_like_polling_conflict":
            ret = D._looks_like_polling_conflict(objs[args["error"]])
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(vars(inst))}


# ── three-way seam-name cross-check ──
SEAM_NAMES = [
    "self._chat_type_str", "self._bank_conflict_quiet_window", "self._note_conflict_quiet_progress",
    "self._schedule_polling_recovery", "self._reply_to_message_id_for_send", "self._metadata_thread_id",
    "self._thread_kwargs_for_send", "self._notification_kwargs", "self._telegram_group_allowed_chats",
    "self._telegram_allowed_chats", "self._telegram_free_response_topics",
    "mark_send_path_recovered", "normalize_telegram_chat_id", "_extra_or_secret", "_decode_json_list_literal",
    "event_is_set", "event_set", "event_create", "task_done", "task_cancel",
    "request_parse_json_payload", "open", "file_read", "monotonic", "logger.info", "logger.warning",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "telegram_adapter_flow2.c",
                encoding="utf-8").read()
    c_seams = set(re.findall(r'ta2_call\(\s*ctx,\s*"([^"]+)"', csrc))
    c_seams |= {f"logger.{lv}" for lv in re.findall(r'ta2_log\(\s*ctx,\s*"([^"]+)"', csrc)}
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc and n not in c_seams]
    if missing_c:
        print(f"FATAL oracle_telegram_adapter_flow2: seams absent from C port: {missing_c}")
        sys.exit(2)
    extra_c = c_seams - set(SEAM_NAMES)
    if extra_c:
        print(f"FATAL oracle_telegram_adapter_flow2: C seams outside registry: {sorted(extra_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'_CUR\.call\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'_CUR\.call\(\s*f"([^"]+)"', osrc))
    used |= set(re.findall(r'rec\(\s*"([^"]+)"', osrc))
    used = {u.replace("{level}", "info") for u in used}
    used |= {"logger.info", "logger.warning"}
    unknown = used - set(SEAM_NAMES)
    if unknown:
        print(f"FATAL oracle_telegram_adapter_flow2: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow2.jsonl"
    if fix.exists():
        fix_keys = set()
        fix_funcs = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func", ""))
        bad = fix_keys - set(SEAM_NAMES)
        if bad:
            print(f"FATAL oracle_telegram_adapter_flow2: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        unbound = fix_funcs - set(KINDS)
        if unbound:
            print(f"FATAL oracle_telegram_adapter_flow2: fixture funcs missing from KINDS: {sorted(unbound)}")
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
        for k in sorted(keys)[:4]:
            print(f"    {k}: want={json.dumps(ws.get(k), ensure_ascii=False)[:200]} got={json.dumps(gs.get(k), ensure_ascii=False)[:200]}")


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
    print(f"ORACLE telegram_adapter_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow2.jsonl"
    n = bad = 0
    counts = {}
    seam_attrs = set(KINDS) | {"_chat_type_str", "_bank_conflict_quiet_window",
                               "_note_conflict_quiet_progress", "_schedule_polling_recovery",
                               "_reply_to_message_id_for_send", "_metadata_thread_id",
                               "_thread_kwargs_for_send", "_notification_kwargs",
                               "_telegram_group_allowed_chats", "_telegram_allowed_chats",
                               "_telegram_free_response_topics"}
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
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError" and \
                any(f"'{nm}'" in str(r.get("message", "")) for nm in seam_attrs):
            bad += 1
            print(f"SELFTEST BINDING GAP {func} case {n}: {r['message']}")
            continue
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            if not (case.get("in") or {}).get("expect_fail_closed"):
                bad += 1
                print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
    print(f"SELFTEST telegram_adapter_flow2: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
