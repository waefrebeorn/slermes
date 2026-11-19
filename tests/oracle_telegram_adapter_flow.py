#!/usr/bin/env python3
"""Oracle for the Telegram adapter FLOW port
(src/gateway/platforms/telegram_adapter_flow.c).

Drives the LIVE plugins/platforms/telegram/adapter.py decision chains with
SCRIPTED seams: parse_telegram_message_id, inspect.iscoroutinefunction, the
unported _rich_message_payload composer and the telegram.error isinstance
probes are recording stubs fed the same scripts the C harness feeds its
universal seam. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the
adapter state echo ("state").

The live defs are AST-extracted from the TelegramAdapter class body
(decorators stripped, segments dedented) plus the module-level
_iter_exception_graph; class constants (RICH_MESSAGE_MAX_CHARS,
_GENERAL_TOPIC_THREAD_ID, the three rich regexes) and the imported
TABLE_SEPARATOR_RE (gateway/platforms/helpers.py) are seeded. Every exec
namespace is a registered sys.modules module (py3.14 dataclasses/annotation
resolution). The heavy quarry import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE telegram_adapter_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
  Scripted seam raises round-trip bare (no KeyError/errno scripts: their
  str() is not the raw message).
- in.state: adapter attributes ("_reply_to_mode", "_notifications_mode",
  "_rich_messages_enabled" default True, "_rich_send_disabled",
  "_allow_cjk_rich_messages", "config" null|{"extra":{...}}, "_bot"
  null|{"do_api_request": marker}) plus the case flags "ptb_available"
  (default true) and "timedout_missing" (default false). State is echoed,
  never mutated by the ported defs.
- in.errors: id -> {"class", "message", "base" (synthetic classes only),
  "cause": id|null, "context": id|null, "error_code": any}. Real exception
  objects are built (builtin class when the name is one, else a synthetic
  subclass of "base"); __context__ then __cause__ are wired. Registry
  classes must not be KeyError (str() would repr the message).
- telegram.error probes: the seam "telegram_error_isinstance"
  {"error": id, "class": name}, ONE event per class in tuple order,
  short-circuiting on the first true (PTB class doubles use a metaclass
  __instancecheck__ recorder). ptb_available=false removes the stub
  modules so the live import raises ImportError (no seam events);
  timedout_missing=true makes telegram.error.__getattr__ raise
  AttributeError for TimedOut only.
- _iter_exception_graph projects the yield order to an id list.
- chat/result doubles: dicts become SimpleNamespace; an EMPTY chat dict
  models a falsy chat (None). str() slots carry scalars only; lower()/
  regex \\s fold ASCII; splitlines uses the ASCII boundary corpus.
- logger calls are silent on both sides (none of the ported defs log).
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
H_SRC = os.path.join(Q, "gateway", "platforms", "helpers.py")

_FLAGS = _fut.annotations.compiler_flag

# method name -> decorator kind (probe-verified against the live class)
KINDS = {
    "_notification_kwargs": "instance",
    "_normalize_chat_type": "staticmethod",
    "_gateway_chat_type": "classmethod",
    "_metadata_thread_id": "classmethod",
    "_metadata_direct_messages_topic_id": "classmethod",
    "_metadata_reply_to_message_id": "classmethod",
    "_dm_topic_fallback": "staticmethod",
    "_is_private_dm_topic_send": "classmethod",
    "_reply_to_message_id_for_send": "classmethod",
    "_thread_kwargs_for_send": "classmethod",
    "_direct_topic_kwargs": "classmethod",
    "_thread_kwargs_for_draft": "instance",
    "_message_thread_id_for_send": "classmethod",
    "_message_thread_id_for_typing": "classmethod",
    "_is_thread_not_found_error": "staticmethod",
    "_is_bad_request_error": "staticmethod",
    "_should_retry_without_dm_topic_reply_anchor": "classmethod",
    "_is_reply_anchor_missing_error": "staticmethod",
    "_looks_like_auth_error": "staticmethod",
    "_looks_like_network_error": "staticmethod",
    "_exception_graph_matches": "staticmethod",
    "_looks_like_connect_timeout": "classmethod",
    "_looks_like_pool_timeout": "staticmethod",
    "_coerce_bool_extra": "instance",
    "_content_fits_rich_limits": "instance",
    "_bot_supports_rich": "instance",
    "_has_telegram_desktop_details_math_crash_shape": "instance",
    "_has_telegram_desktop_cjk_rich_garble_shape": "instance",
    "_needs_rich_rendering": "instance",
    "_rich_delivery_enabled": "instance",
    "_rich_content_ok": "instance",
    "_rich_eligible": "instance",
    "_should_attempt_rich": "instance",
    "prefers_fresh_final_streaming": "instance",
    "_rich_transport_available": "instance",
    "streaming_overflow_limit": "instance",
    "_is_rich_capability_error": "instance",
    "_is_rich_fallback_error": "instance",
    "_chunk_reply_routing": "instance",
    "_compute_single_send_routing": "instance",
    "_is_timed_out": "staticmethod",
    "_send_retry_is_final": "instance",
    "_should_thread_reply": "instance",
    "_chat_type_str": "staticmethod",
}
CLASS_CONSTS = ["RICH_MESSAGE_MAX_CHARS", "_GENERAL_TOPIC_THREAD_ID",
                "_RICH_DETAILS_RE", "_RICH_MATH_IN_DETAILS_RE", "_RICH_CJK_RE"]


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
    mod = new_module("ta_oracle_adapter")
    ns = mod.__dict__
    import inspect as _inspect
    ns.update({"re": re, "os": os, "json": json, "inspect": _inspect,
               "logger": _SilentLogger(),
               "parse_telegram_message_id": None})
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # module-level target def
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_iter_exception_graph":
            exec_node(src, node, ns, A_SRC)
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "TelegramAdapter")
    # class-level constants first (the regexes need re)
    for node in cls.body:
        if isinstance(node, ast.Assign):
            names = [t.id for t in node.targets if isinstance(t, ast.Name)]
            if any(nm in CLASS_CONSTS for nm in names):
                exec_node(src, node, ns, A_SRC)
    for node in cls.body:
        if isinstance(node, ast.FunctionDef) and node.name in KINDS:
            exec_node(src, node, ns, A_SRC)
    # imported TABLE_SEPARATOR_RE: exec the REAL Assign from helpers.py
    hsrc = open(H_SRC, encoding="utf-8").read()
    for node in ast.parse(hsrc).body:
        if isinstance(node, ast.Assign):
            names = [t.id for t in node.targets if isinstance(t, ast.Name)]
            if "TABLE_SEPARATOR_RE" in names:
                exec_node(hsrc, node, ns, H_SRC)
    ns["_TABLE_SEPARATOR_RE"] = ns["TABLE_SEPARATOR_RE"]
    ns["logger"] = _SilentLogger()
    required = list(KINDS) + ["_iter_exception_graph"] + CLASS_CONSTS + ["_TABLE_SEPARATOR_RE"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_telegram_adapter_flow: missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()

# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
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


# ── per-case script backend (mirrors the C ta_call exactly) ──
_MISSING = object()


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


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


# ── telegram.error stub module (scripted isinstance via metaclass) ──
_CUR = None
_EXC_IDS = {}
_PTB_NAMES = ("BadRequest", "Forbidden", "InvalidToken", "NetworkError", "RetryAfter", "TimedOut")


class _PtbMeta(type):
    def __instancecheck__(cls, inst):
        eid = _EXC_IDS.get(id(inst))
        return _CUR.call("telegram_error_isinstance", {"error": eid, "class": cls.__name__})


def setup_ptb(state):
    avail = (state or {}).get("ptb_available", True)
    missing = (state or {}).get("timedout_missing", False)
    if not avail:
        sys.modules.pop("telegram.error", None)
        sys.modules.pop("telegram", None)
        return
    pkg = sys.modules.get("telegram")
    if pkg is None:
        pkg = types.ModuleType("telegram")
        pkg.__path__ = []
        sys.modules["telegram"] = pkg
    err = types.ModuleType("telegram.error")
    for nm in _PTB_NAMES:
        setattr(err, nm, _PtbMeta(nm, (Exception,), {}))
    if missing:
        del err.TimedOut

        def _ga(name):
            raise AttributeError(f"module 'telegram.error' has no attribute '{name}'")
        err.__getattr__ = _ga
    sys.modules["telegram.error"] = err
    pkg.error = err


# ── exception-graph doubles ──
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
        exc = classes[eid](e.get("message", ""))
        if "error_code" in e:
            exc.error_code = e["error_code"]
        objs[eid] = exc
    for eid, e in spec.items():
        if e.get("context"):
            objs[eid].__context__ = objs[e["context"]]
        if e.get("cause"):
            objs[eid].__cause__ = objs[e["cause"]]
    ids = {id(o): eid for eid, o in objs.items()}
    return objs, ids


# ── adapter double ──
def make_double(state, ctx):
    clsdict = {}
    for name, kind in KINDS.items():
        fn = NS[name]
        if kind == "classmethod":
            clsdict[name] = classmethod(fn)
        elif kind == "staticmethod":
            clsdict[name] = staticmethod(fn)
        else:
            clsdict[name] = fn
    for cname in CLASS_CONSTS:
        clsdict[cname] = NS[cname]

    def rich_message_payload(self, content, *, skip_entity_detection=False):
        return ctx.call("rich_message_payload",
                        {"content": content, "skip_entity_detection": skip_entity_detection})
    clsdict["_rich_message_payload"] = rich_message_payload
    D = type("TelegramAdapter", (), clsdict)
    inst = D()
    for k, v in (state or {}).items():
        if k in ("ptb_available", "timedout_missing"):
            continue
        if k in ("config", "_bot") and isinstance(v, dict):
            setattr(inst, k, types.SimpleNamespace(**v))
        else:
            setattr(inst, k, v)
    return D, inst


def chat_double(v):
    if isinstance(v, dict):
        return types.SimpleNamespace(**v) if v else None
    return v


def result_double(v):
    return types.SimpleNamespace(**v) if isinstance(v, dict) else v


# ── per-case drive ──
def drive(func, inp):
    global _CUR, _EXC_IDS
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    _CUR = ctx
    objs, ids = build_errors(inp.get("errors"))
    _EXC_IDS = ids
    setup_ptb(state)
    NS["parse_telegram_message_id"] = rec(
        ctx, "parse_telegram_message_id", lambda v: {"value": v})
    NS["inspect"] = types.SimpleNamespace(
        iscoroutinefunction=rec(ctx, "iscoroutinefunction", lambda v: {"value": v}))
    D, inst = make_double(state, ctx)

    def err():
        return objs[args["error"]]

    try:
        if func == "_metadata_thread_id":
            ret = D._metadata_thread_id(args.get("metadata"))
        elif func == "_metadata_direct_messages_topic_id":
            ret = D._metadata_direct_messages_topic_id(args.get("metadata"))
        elif func == "_metadata_reply_to_message_id":
            ret = D._metadata_reply_to_message_id(args.get("metadata"))
        elif func == "_dm_topic_fallback":
            ret = D._dm_topic_fallback(args.get("metadata"))
        elif func == "_is_private_dm_topic_send":
            ret = D._is_private_dm_topic_send(args.get("chat_id"), args.get("thread_id"), args.get("metadata"))
        elif func == "_reply_to_message_id_for_send":
            ret = D._reply_to_message_id_for_send(args.get("reply_to"), args.get("metadata"), args.get("reply_to_mode"))
        elif func == "_message_thread_id_for_send":
            ret = D._message_thread_id_for_send(args.get("thread_id"))
        elif func == "_message_thread_id_for_typing":
            ret = D._message_thread_id_for_typing(args.get("thread_id"))
        elif func == "_direct_topic_kwargs":
            ret = D._direct_topic_kwargs(args.get("metadata"))
        elif func == "_thread_kwargs_for_send":
            ret = D._thread_kwargs_for_send(
                args.get("chat_id"), args.get("thread_id"), args.get("metadata"),
                reply_to_message_id=args.get("reply_to_message_id"),
                reply_to_mode=args.get("reply_to_mode"))
        elif func == "_thread_kwargs_for_draft":
            ret = inst._thread_kwargs_for_draft(args.get("chat_id"), args.get("metadata"))
        elif func == "_should_thread_reply":
            ret = inst._should_thread_reply(args.get("reply_to"), args.get("chunk_index"))
        elif func == "_chunk_reply_routing":
            ret = inst._chunk_reply_routing(args.get("chat_id"), args.get("reply_to"),
                                            args.get("metadata"), args.get("thread_id"), args.get("index"))
        elif func == "_compute_single_send_routing":
            ret = inst._compute_single_send_routing(args.get("chat_id"), args.get("reply_to"),
                                                    args.get("metadata"), args.get("thread_id"))
        elif func == "_iter_exception_graph":
            ret = [ids[id(x)] for x in NS["_iter_exception_graph"](err())]
        elif func == "_exception_graph_matches":
            ret = D._exception_graph_matches(err(), args["name_marker"], *args.get("text_markers", []))
        elif func == "_looks_like_connect_timeout":
            ret = D._looks_like_connect_timeout(err())
        elif func == "_looks_like_pool_timeout":
            ret = D._looks_like_pool_timeout(err())
        elif func == "_is_thread_not_found_error":
            ret = D._is_thread_not_found_error(err())
        elif func == "_is_bad_request_error":
            ret = D._is_bad_request_error(err())
        elif func == "_is_reply_anchor_missing_error":
            ret = D._is_reply_anchor_missing_error(err())
        elif func == "_should_retry_without_dm_topic_reply_anchor":
            ret = D._should_retry_without_dm_topic_reply_anchor(err(), args.get("metadata"), args.get("reply_to_message_id"))
        elif func == "_looks_like_auth_error":
            ret = D._looks_like_auth_error(err())
        elif func == "_looks_like_network_error":
            ret = D._looks_like_network_error(err())
        elif func == "_is_timed_out":
            ret = D._is_timed_out(err())
        elif func == "_is_rich_capability_error":
            ret = inst._is_rich_capability_error(err())
        elif func == "_is_rich_fallback_error":
            ret = inst._is_rich_fallback_error(err())
        elif func == "_rich_delivery_enabled":
            ret = inst._rich_delivery_enabled()
        elif func == "_bot_supports_rich":
            ret = inst._bot_supports_rich()
        elif func == "_needs_rich_rendering":
            ret = inst._needs_rich_rendering(args.get("content"))
        elif func == "_has_telegram_desktop_cjk_rich_garble_shape":
            ret = inst._has_telegram_desktop_cjk_rich_garble_shape(args.get("content"))
        elif func == "_has_telegram_desktop_details_math_crash_shape":
            ret = inst._has_telegram_desktop_details_math_crash_shape(args.get("content"))
        elif func == "_content_fits_rich_limits":
            ret = inst._content_fits_rich_limits(args.get("content"))
        elif func == "_rich_content_ok":
            ret = inst._rich_content_ok(args.get("content"), check_size=args.get("check_size", True))
        elif func == "_rich_eligible":
            ret = inst._rich_eligible(args.get("content"))
        elif func == "_should_attempt_rich":
            ret = inst._should_attempt_rich(args.get("content"), args.get("metadata"))
        elif func == "prefers_fresh_final_streaming":
            ret = inst.prefers_fresh_final_streaming(args.get("content"), args.get("metadata"))
        elif func == "_rich_transport_available":
            ret = inst._rich_transport_available()
        elif func == "streaming_overflow_limit":
            ret = inst.streaming_overflow_limit()
        elif func == "_normalize_chat_type":
            ret = D._normalize_chat_type(args.get("chat_type"), is_forum=args.get("is_forum", False))
        elif func == "_chat_type_str":
            ret = D._chat_type_str(chat_double(args.get("chat")))
        elif func == "_gateway_chat_type":
            ret = D._gateway_chat_type(chat_double(args.get("chat")), is_forum=args.get("is_forum", False))
        elif func == "_coerce_bool_extra":
            ret = inst._coerce_bool_extra(args["key"], args.get("default", False))
        elif func == "_notification_kwargs":
            ret = inst._notification_kwargs(args.get("metadata"))
        elif func == "_send_retry_is_final":
            ret = inst._send_retry_is_final(result_double(args.get("result")))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = ["parse_telegram_message_id", "iscoroutinefunction",
              "rich_message_payload", "telegram_error_isinstance"]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "telegram_adapter_flow.c",
                encoding="utf-8").read()
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_telegram_adapter_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'_CUR\.call\("([^"]+)"', osrc))
    used |= set(re.findall(r'ctx\.call\("([^"]+)"', osrc))
    unknown = used - set(SEAM_NAMES)
    if unknown:
        print(f"FATAL oracle_telegram_adapter_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow.jsonl"
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
            print(f"FATAL oracle_telegram_adapter_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        unbound = fix_funcs - set(KINDS) - {"_iter_exception_graph"}
        if unbound:
            print(f"FATAL oracle_telegram_adapter_flow: fixture funcs missing from KINDS: {sorted(unbound)}")
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
    print(f"ORACLE telegram_adapter_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow.jsonl"
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
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError" and \
                any(f"'{nm}'" in str(r.get("message", "")) for nm in list(KINDS) + ["_rich_message_payload"]):
            bad += 1
            print(f"SELFTEST BINDING GAP {func} case {n}: {r['message']}")
            continue
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            # fail-closed cases are INTENTIONAL when the case marks them
            if not (case.get("in") or {}).get("expect_fail_closed"):
                bad += 1
                print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
    print(f"SELFTEST telegram_adapter_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()