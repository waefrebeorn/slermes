#!/usr/bin/env python3
"""Shadow mirror of src/gateway/session_transcript_flow.c — a LITERAL Python
transcription of the C algorithms (json presence semantics, seam consumption,
state effects, raise classification, evaluation order), diffed against the
LIVE oracle drive over every fixture case (ret + ordered trace + state).
Code-only verification substitute for the C gate: catches transcription slips
without compiling/linking.

C json_t* model: Python None == C NULL pointer (absent / seam-None); the JNULL
singleton == a JSON_NULL node (present null). json_obj_get returns None for an
absent key and JNULL for a present null; json_set/json_append NO-OP on a None
value (C ignores NULL); m_copy deep-copies. Emit converts JNULL -> JSON null.

Run: python tests/shadow_session_transcript_flow.py
     python tests/shadow_session_transcript_flow.py --proxy   (gate proxy:
     emits harness-shaped JSONL for piping into the oracle's main())
"""
import copy
import importlib.util
import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location(
    "oracle_stx", HERE / "oracle_session_transcript_flow.py")
oracle = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(oracle)


class _JNull:
    _inst = None

    def __new__(cls):
        if cls._inst is None:
            cls._inst = super().__new__(cls)
        return cls._inst

    def __repr__(self):
        return "JNULL"

    def __deepcopy__(self, memo):
        return self


JNULL = _JNull()


def to_c(v):
    if v is None:
        return JNULL
    if isinstance(v, dict):
        return {k: to_c(x) for k, x in v.items()}
    if isinstance(v, list):
        return [to_c(x) for x in v]
    return v


def from_c(v):
    if v is None or v is JNULL:
        return None
    if isinstance(v, dict):
        return {k: from_c(x) for k, x in v.items()}
    if isinstance(v, list):
        return [from_c(x) for x in v]
    return v


def m_is_none(v):
    return v is None or v is JNULL


def m_is_obj(v):
    return isinstance(v, dict)


def m_is_arr(v):
    return isinstance(v, list)


def m_is_str(v):
    return isinstance(v, str)


def m_is_num(v):
    return isinstance(v, (int, float)) and not isinstance(v, bool)


def m_truthy(v):
    if v is None or v is JNULL:
        return False
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v != 0.0
    if isinstance(v, str):
        return len(v) > 0
    if isinstance(v, (list, dict)):
        return len(v) > 0
    return False


def m_get(obj, key):
    if m_is_obj(obj):
        return obj.get(key)
    return None


def m_get_str(obj, key, default):
    v = m_get(obj, key)
    return v if m_is_str(v) else default


def m_get_num(obj, key, default):
    v = m_get(obj, key)
    return v if m_is_num(v) else default


def m_get_bool(obj, key, default):
    v = m_get(obj, key)
    return v if isinstance(v, bool) else default


def m_set(obj, key, val):
    # C json_set: NO-OP on NULL val; replace in place (keep position)
    if not m_is_obj(obj) or val is None:
        return
    obj[key] = val


def m_append(arr, item):
    # C json_append drops a NULL item
    if m_is_arr(arr) and item is not None:
        arr.append(item)


def m_copy(v):
    return copy.deepcopy(v)


def m_copy_or_null(v):
    return m_copy(v) if v is not None else JNULL


def m_del(obj, key):
    if m_is_obj(obj):
        obj.pop(key, None)


def m_take(obj, key):
    # stx_obj_take: detach WITHOUT freeing
    if m_is_obj(obj) and key in obj:
        return obj.pop(key)
    return None


def m_pop0(arr):
    if m_is_arr(arr) and len(arr) > 0:
        del arr[0]


def m_len(v):
    # c.count on a calloc'ed non-container node reads 0
    if m_is_arr(v) or m_is_obj(v):
        return len(v)
    return 0


def m_json_eq(a, b):
    if a is None and b is None:
        return True
    if a is None or b is None:
        return False
    if a is JNULL and b is JNULL:
        return True
    if a is JNULL or b is JNULL:
        return False
    if isinstance(a, bool) != isinstance(b, bool):
        return False
    if isinstance(a, bool):
        return a == b
    if m_is_num(a) and m_is_num(b):
        return a == b
    if m_is_num(a) != m_is_num(b):
        return False
    if m_is_str(a) and m_is_str(b):
        return a == b
    if m_is_str(a) != m_is_str(b):
        return False
    if m_is_arr(a) and m_is_arr(b):
        return len(a) == len(b) and all(m_json_eq(x, y) for x, y in zip(a, b))
    if m_is_arr(a) != m_is_arr(b):
        return False
    if m_is_obj(a) and m_is_obj(b):
        if len(a) != len(b):
            return False
        for k in a:
            if k not in b or not m_json_eq(a[k], b[k]):
                return False
        return True
    return False


def m_arr_has(arr, v):
    if not m_is_arr(arr):
        return False
    return any(m_json_eq(x, v) for x in arr)


def m_arr_add_once(arr, v):
    if not m_is_arr(arr) or v is None or m_arr_has(arr, v):
        return
    arr.append(m_copy(v))


def m_integral(d):
    return d == math.floor(d)


def m_float_repr(d):
    # literal stx_float_repr: shortest roundtrip %.*e then Python formatting
    sci = None
    for prec in range(1, 18):
        sci = format(d, ".%de" % (prec - 1))
        if float(sci) == d:
            break
    neg = sci.startswith("-")
    p = sci[1:] if neg else sci
    dig = ""
    i = 0
    while i < len(p) and p[i] not in "eE":
        if p[i].isdigit():
            dig += p[i]
        i += 1
    E = int(p[i + 1:]) if i < len(p) else 0
    while len(dig) > 1 and dig.endswith("0"):
        dig = dig[:-1]
    if -4 <= E < 16:
        point = E + 1
        if point <= 0:
            return "%s0.%s%s" % ("-" if neg else "", "0" * (-point), dig)
        if point >= len(dig):
            return "%s%s%s.0" % ("-" if neg else "", dig,
                                 "0" * (point - len(dig)))
        return "%s%s.%s" % ("-" if neg else "", dig[:point], dig[point:])
    if len(dig) > 1:
        return "%s%c.%se%c%02d" % ("-" if neg else "", dig[0], dig[1:],
                                   "-" if E < 0 else "+", abs(E))
    return "%s%ce%c%02d" % ("-" if neg else "", dig[0],
                            "-" if E < 0 else "+", abs(E))


def m_pystr(v):
    if m_is_none(v):
        return "None"
    if isinstance(v, bool):
        return "True" if v else "False"
    if m_is_str(v):
        return v
    if m_is_num(v):
        d = float(v)
        if m_integral(d) and -9.2e18 < d < 9.2e18:
            return "%d" % int(d)
        return m_float_repr(d)
    return json.dumps(from_c(v), separators=(",", ":"))


def m_type_name(v):
    if m_is_none(v):
        return "NoneType"
    if isinstance(v, bool):
        return "bool"
    if m_is_num(v):
        return "int" if m_integral(v) else "float"
    if m_is_str(v):
        return "str"
    if m_is_arr(v):
        return "list"
    if m_is_obj(v):
        return "dict"
    return "object"


def m_attr_msg(type_name, attr):
    return "'%s' object has no attribute '%s'" % (type_name, attr)


# ── Python json.dumps(default=str) transcription (stx_dumps) ────────────────
def m_dumps_str(s):
    out = ['"']
    for ch in s:
        c = ord(ch)
        if ch == '"':
            out.append('\\"')
        elif ch == "\\":
            out.append("\\\\")
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\r":
            out.append("\\r")
        elif ch == "\t":
            out.append("\\t")
        elif ch == "\b":
            out.append("\\b")
        elif ch == "\f":
            out.append("\\f")
        elif c < 0x20:
            out.append("\\u%04x" % c)
        elif c < 0x7F:
            out.append(ch)
        elif c == 0x7F:
            out.append("\\u007f")
        elif c >= 0x10000:
            cp = c - 0x10000
            out.append("\\u%04x" % (0xD800 + (cp >> 10)))
            out.append("\\u%04x" % (0xDC00 + (cp & 0x3FF)))
        else:
            out.append("\\u%04x" % c)
    out.append('"')
    return "".join(out)


def m_dumps_val(v):
    if m_is_none(v):
        return "null"
    if isinstance(v, bool):
        return "true" if v else "false"
    if m_is_num(v):
        d = float(v)
        if m_integral(d):
            return "%.0f" % d
        return m_float_repr(d)
    if m_is_str(v):
        return m_dumps_str(v)
    if m_is_arr(v):
        return "[" + ", ".join(m_dumps_val(x) for x in v) + "]"
    if m_is_obj(v):
        return "{" + ", ".join(m_dumps_str(k) + ": " + m_dumps_val(x)
                               for k, x in v.items()) + "}"
    return "null"


def m_dumps(v):
    return m_dumps_val(v)


# ── FULL builtin exception hierarchy (literal stx_exc_table) ────────────────
EXC_TABLE = {
    "BaseException": "",
    "SystemExit": "BaseException",
    "KeyboardInterrupt": "BaseException",
    "GeneratorExit": "BaseException",
    "Exception": "BaseException",
    "StopIteration": "Exception",
    "StopAsyncIteration": "Exception",
    "ArithmeticError": "Exception",
    "FloatingPointError": "ArithmeticError",
    "OverflowError": "ArithmeticError",
    "ZeroDivisionError": "ArithmeticError",
    "AssertionError": "Exception",
    "AttributeError": "Exception",
    "BufferError": "Exception",
    "EOFError": "Exception",
    "ImportError": "Exception",
    "ModuleNotFoundError": "ImportError",
    "LookupError": "Exception",
    "IndexError": "LookupError",
    "KeyError": "LookupError",
    "MemoryError": "Exception",
    "NameError": "Exception",
    "UnboundLocalError": "NameError",
    "OSError": "Exception",
    "BlockingIOError": "OSError",
    "ChildProcessError": "OSError",
    "ConnectionError": "OSError",
    "BrokenPipeError": "ConnectionError",
    "ConnectionAbortedError": "ConnectionError",
    "ConnectionRefusedError": "ConnectionError",
    "ConnectionResetError": "ConnectionError",
    "FileExistsError": "OSError",
    "FileNotFoundError": "OSError",
    "InterruptedError": "OSError",
    "IsADirectoryError": "OSError",
    "NotADirectoryError": "OSError",
    "PermissionError": "OSError",
    "ProcessLookupError": "OSError",
    "TimeoutError": "OSError",
    "ReferenceError": "Exception",
    "RuntimeError": "Exception",
    "NotImplementedError": "RuntimeError",
    "RecursionError": "RuntimeError",
    "SyntaxError": "Exception",
    "IndentationError": "SyntaxError",
    "TabError": "IndentationError",
    "SystemError": "Exception",
    "TypeError": "Exception",
    "ValueError": "Exception",
    "UnicodeError": "ValueError",
    "UnicodeDecodeError": "UnicodeError",
    "UnicodeEncodeError": "UnicodeError",
    "UnicodeTranslateError": "UnicodeError",
    "Warning": "Exception",
    "DeprecationWarning": "Warning",
    "PendingDeprecationWarning": "Warning",
    "RuntimeWarning": "Warning",
    "SyntaxWarning": "Warning",
    "UserWarning": "Warning",
    "FutureWarning": "Warning",
    "ImportWarning": "Warning",
    "UnicodeWarning": "Warning",
    "BytesWarning": "Warning",
    "EncodingWarning": "Warning",
    "ResourceWarning": "Warning",
    "BaseExceptionGroup": "BaseException",
    "ExceptionGroup": "BaseExceptionGroup",
    "CancelledError": "BaseException",
    "TranscriptReadError": "RuntimeError",
}


def m_is_exception_kind(kind):
    if not kind:
        return True
    if kind == "ExceptionGroup":
        return True
    cur = kind
    for _ in range(64):
        if cur == "Exception":
            return True
        if cur == "BaseException":
            return False
        parent = EXC_TABLE.get(cur)
        if parent is None:
            return True
        if parent == "":
            return False
        cur = parent
    return True


def m_is_raise(r):
    return m_is_obj(r) and m_is_str(m_get(r, "__raise__"))


def m_norm_raise(r):
    kind = m_get_str(r, "__raise__", "Exception")
    d = {"__raise__": kind}
    if kind == "TranscriptReadError":
        sid = m_get(r, "session_id")
        d["message"] = "transcript read failed for session %s" % m_pystr(sid)
        d["session_id"] = m_copy_or_null(sid)
    else:
        d["message"] = m_get_str(r, "message", "")
    return d


class MCtx:
    def __init__(self, io, state):
        self.io = io
        self.state = state
        self.trace = []
        self.pos = {}
        self.exc = None
        self.cur = None


def m_raise(ctx, kind, msg):
    ctx.exc = {"__raise__": kind, "message": msg}


def m_raise_transcript(ctx, sid_v):
    ctx.exc = {
        "__raise__": "TranscriptReadError",
        "message": "transcript read failed for session %s" % m_pystr(sid_v),
        "session_id": m_copy_or_null(sid_v),
    }


def m_prop(ctx, r):
    # stx_prop: any raise propagates; True == bail
    if not m_is_raise(r):
        return False
    ctx.exc = m_norm_raise(r)
    return True


def m_exc_class(ctx, r):
    # stx_exc_class: NON-CONSUMING; 0 / 1 (cur) / 2 (exc)
    if not m_is_raise(r):
        return 0
    kind = m_get_str(r, "__raise__", "Exception")
    if m_is_exception_kind(kind):
        ctx.cur = m_norm_raise(r)
        return 1
    ctx.exc = m_norm_raise(r)
    return 2


def m_internal_exc(ctx, kind, msg):
    if not m_is_exception_kind(kind):
        m_raise(ctx, kind, msg)
        return 2
    ctx.cur = {"__raise__": kind, "message": msg}
    return 1


def m_end_catch(ctx):
    ctx.cur = None


def m_cur_msg(ctx):
    return m_get_str(ctx.cur, "message", "") if ctx.cur is not None else ""


def m_next_script(ctx, name):
    v = m_get(ctx.io, name)
    if v is None:
        return None
    if m_is_arr(v):
        if len(v) == 0:
            return None
        i = int(m_get_num(ctx.pos, name, 0))
        if i >= len(v):
            i = len(v) - 1
        else:
            m_set(ctx.pos, name, i + 1)
        return v[i]
    if m_is_obj(v) and m_get_bool(v, "__script__", False):
        return m_get(v, "ret")
    return v


def m_call(ctx, name, args):
    # harness stub: log special-cased (no ret); others record raw script ret
    if name == "log":
        ev = {"ev": "log",
              "level": m_copy(m_get(args, "level")),
              "fmt": m_copy(m_get(args, "fmt")),
              "args": m_copy(m_get(args, "args"))}
        if m_get(args, "exc_info") is not None:
            ev["exc_info"] = m_copy(m_get(args, "exc_info"))
        ctx.trace.append(ev)
        return None
    script = m_next_script(ctx, name)
    ev = {"ev": name, "args": m_copy(args) if args is not None else JNULL,
          "ret": m_copy(script) if script is not None else JNULL}
    ctx.trace.append(ev)
    if script is None or script is JNULL:
        return None
    return m_copy(script)


def m_log(ctx, level, fmt, args, exc_info):
    a = {"level": level, "fmt": fmt, "args": args if args is not None else []}
    if exc_info:
        a["exc_info"] = True
    m_call(ctx, "log", a)


def m_log_args1(v):
    return [m_copy_or_null(v)]


def m_log_args2(v, s):
    return [m_copy_or_null(v), s if s is not None else ""]


# ── state access (literal stx_*) ────────────────────────────────────────────
def m_sect(ctx, name):
    return m_get(ctx.state, name)


def m_dirty(ctx):
    return m_sect(ctx, "dirty_transcripts")


def m_failures(ctx):
    return m_sect(ctx, "append_failures")


def m_entries(ctx):
    return m_sect(ctx, "entries")


def m_attr_present(ctx, name):
    v = m_sect(ctx, name)
    return v is not None and v is not JNULL


def m_lazy_obj(ctx, name):
    v = m_sect(ctx, name)
    if not m_is_obj(v):
        v = {}
        m_set(ctx.state, name, v)
    return v


def m_lazy_set(ctx, name):
    v = m_sect(ctx, name)
    if not m_is_arr(v):
        v = []
        m_set(ctx.state, name, v)
    return v


def m_is_store(ctx, v):
    return m_is_str(v) and m_get(m_sect(ctx, "stores"), v) is not None


def m_store_truthy(ctx, v):
    # registered store == object (always truthy); else JSON truthiness
    return m_is_store(ctx, v) or m_truthy(v)


def m_row_truthy(v):
    # dict projection == types.SimpleNamespace object (always truthy)
    if m_is_obj(v):
        return True
    return m_truthy(v)


def m_sessions_dir(home):
    # literal stx_sessions_dir
    norm = []
    i = 0
    while i < len(home):
        while i < len(home) and home[i] in "/\\":
            i += 1
        if i >= len(home):
            break
        j = i
        while j < len(home) and home[j] not in "/\\":
            j += 1
        comp = home[i:j]
        if comp != ".":
            norm.append(comp)
        i = j
    if not norm:
        return "sessions"
    return "\\".join(norm) + "\\sessions"


# ── shared leaves ───────────────────────────────────────────────────────────
def m_api_content_sidecar(msg):
    v = m_get(msg, "api_content")
    return m_copy(v) if m_is_str(v) else JNULL


def m_bindable_value(v):
    if m_is_none(v):
        return JNULL
    if m_is_str(v) or isinstance(v, bool):
        return m_copy(v)
    if m_is_num(v):
        d = float(v)
        if m_integral(d):
            if -9223372036854775808.0 <= d < 9223372036854775808.0:
                return m_copy(v)
            return "%.0f" % d
        return m_copy(v)
    return m_dumps(v)


def m_spool_dropped_fn(ctx, sid_v, msg):
    a = {"session_id": m_copy_or_null(sid_v), "message": m_copy_or_null(msg)}
    r = m_call(ctx, "spool_dropped_transcript_message", a)
    rc = m_exc_class(ctx, r)
    if rc == 2:
        return None
    if rc == 1:
        m_end_catch(ctx)
        return JNULL
    return r


def m_follow_reroutes_fn(ctx, sid_v):
    rr = m_lazy_obj(ctx, "reroutes")
    cur = m_copy_or_null(sid_v)
    seen = []
    while True:
        if not m_is_str(cur):
            break
        nxt = m_get(rr, cur)
        if nxt is None:
            break
        if m_arr_has(seen, cur):
            break
        m_append(seen, m_copy(cur))
        cur = m_copy(nxt)
    return cur


# ── ported functions (literal C transcription) ──────────────────────────────
def m_transcript_read_error_init(ctx, inp):
    sid = m_get(inp, "session_id")
    return {"__raise__": "TranscriptReadError",
            "message": "transcript read failed for session %s" % m_pystr(sid),
            "session_id": m_copy_or_null(sid)}


def m_spool_dropped(ctx, inp):
    r = m_spool_dropped_fn(ctx, m_get(inp, "session_id"), m_get(inp, "message"))
    if ctx.exc is not None:
        return None
    return r if r is not None else JNULL


def m_bindable_text(ctx, inp):
    return m_bindable_value(m_get(inp, "value"))


def m_transcript_append_kwargs(ctx, inp):
    assistant_keys = ["reasoning", "reasoning_content", "reasoning_details",
                      "codex_reasoning_items", "codex_message_items"]
    bound_text_keys = ["role", "tool_name", "tool_call_id", "reasoning",
                       "reasoning_content", "platform_message_id"]
    msg = m_get(inp, "message")
    if not m_is_obj(msg):
        m_raise(ctx, "AttributeError", m_attr_msg(m_type_name(msg), "get"))
        return None
    role = m_get(msg, "role")
    is_assistant = m_is_str(role) and role == "assistant"
    ts = m_get(msg, "timestamp")
    kw = {}
    m_set(kw, "session_id", m_copy_or_null(m_get(inp, "session_id")))
    m_set(kw, "role", m_copy(role) if role is not None else "unknown")
    for k in ["content", "tool_name", "tool_calls", "tool_call_id"]:
        m_set(kw, k, m_copy_or_null(m_get(msg, k)))
    for k in assistant_keys:
        m_set(kw, k, m_copy_or_null(m_get(msg, k)) if is_assistant else JNULL)
    pmid = m_get(msg, "platform_message_id")
    if not m_truthy(pmid):
        pmid = m_get(msg, "message_id")
    m_set(kw, "platform_message_id", m_copy_or_null(pmid))
    m_set(kw, "observed", m_truthy(m_get(msg, "observed")))
    m_set(kw, "timestamp",
          m_copy_or_null(m_get(inp, "fallback_ts")) if m_is_none(ts)
          else m_copy(ts))
    m_set(kw, "api_content", m_api_content_sidecar(msg))
    m_set(kw, "display_kind", m_copy_or_null(m_get(msg, "display_kind")))
    m_set(kw, "display_metadata", m_copy_or_null(m_get(msg, "display_metadata")))
    for k in bound_text_keys:
        m_set(kw, k, m_bindable_value(m_get(kw, k)))
    return kw


def m_follow_reroutes(ctx, inp):
    return m_follow_reroutes_fn(ctx, m_get(inp, "session_id"))


def m_enqueue_transcript_message(ctx, inp):
    sid = m_get_str(inp, "session_id", "")
    msg = m_get(inp, "message")
    cap = m_get_num(inp, "cap", 200)
    dirty = m_dirty(ctx)
    pending = m_get(dirty, sid)
    if pending is None:
        pending = []
        m_set(dirty, sid, pending)
    if not m_is_obj(msg):
        m_raise(ctx, "TypeError",
                "'%s' object is not iterable" % m_type_name(msg))
        return None
    m_append(pending, m_copy(msg))
    if float(len(pending)) > cap:
        oldest = pending[0]
        del pending[0]
        sid_v = sid
        sp = m_spool_dropped_fn(ctx, sid_v, oldest)
        if ctx.exc is not None:
            return None
        if not m_is_none(sp):
            m_arr_add_once(m_lazy_set(ctx, "spooled_drop_sessions"), sid)
            m_log(ctx, 30,
                  "Session DB transcript pending queue full for %s (cap=%d); "
                  "spooled oldest message to %s for replay after DB recovery",
                  [sid, cap, m_copy(sp)], False)
        else:
            m_log(ctx, 30,
                  "Session DB transcript pending queue full for %s (cap=%d); "
                  "dropping oldest message to make room (on-disk spool "
                  "unavailable)",
                  [sid, cap], False)
    return m_copy(pending)


def m_divert_transcript_after_db_replaced(ctx, inp):
    sid = m_get_str(inp, "session_id", "")
    qsid = m_get_str(inp, "queue_session_id", "")
    exc = m_get(inp, "exc")
    kind = m_get_str(exc, "__raise__", "Exception")
    emsg = m_get_str(exc, "message", "")
    m_log(ctx, 40,
          "Session DB refused further writes on this handle for %s (%s); "
          "stopping SQLite writes and diverting pending transcripts to the "
          "on-disk fallback: %s",
          [sid, kind, emsg], False)
    dirty = m_dirty(ctx)
    remaining = m_copy(m_get(dirty, qsid))
    if remaining is None:
        remaining = []
    m_del(dirty, qsid)
    m_del(m_failures(ctx), sid)
    sid_v = sid
    for i in range(m_len(remaining)):
        sp = m_spool_dropped_fn(ctx, sid_v, remaining[i])
        if ctx.exc is not None:
            return None
        if m_is_none(sp):
            m_log(ctx, 30,
                  "pending fallback failed for replaced state.db transcript "
                  "on %s",
                  m_log_args1(sid_v), True)
    home = m_call(ctx, "get_hermes_home", {})
    rc = m_exc_class(ctx, home)
    if rc == 2:
        return None
    if rc == 0:
        if not m_is_str(home):
            m_internal_exc(ctx, "TypeError",
                           "unsupported operand type(s) for /: '%s' and 'str'"
                           % m_type_name(home))
            rc = 1
        else:
            dir_ = m_sessions_dir(home)
            a = {"session_id": sid, "remaining": m_copy(remaining),
                 "sessions_dir": dir_}
            r2 = m_call(ctx, "divert_session_transcript_jsonl", a)
            rc = m_exc_class(ctx, r2)
    if rc == 2:
        return None
    if rc == 1:
        m_log(ctx, 30,
              "JSONL divert failed for replaced state.db transcript on %s",
              m_log_args1(sid_v), True)
        m_end_catch(ctx)
    return JNULL


def m_live_compression_child(ctx, inp):
    sid_v = m_get(inp, "session_id")
    owner = m_call(ctx, "persistence_for_session_id",
                   {"session_id": m_copy_or_null(sid_v)})
    if m_prop(ctx, owner):
        return None
    tip = None
    if not m_is_none(owner):
        if not m_is_store(ctx, owner):
            m_raise(ctx, "AttributeError",
                    m_attr_msg(m_type_name(owner), "lineage"))
            return None
        tip = m_call(ctx, "lineage.tip",
                     {"store": m_copy(owner), "session_id": m_copy_or_null(sid_v)})
        if m_prop(ctx, tip):
            return None
    ret = ""
    if m_truthy(tip) and not m_json_eq(tip, sid_v):
        row = m_call(ctx, "sessions.get",
                     {"store": m_copy(owner), "session_id": m_copy(tip)})
        if m_prop(ctx, row):
            return None
        if not m_is_none(row):
            if not m_is_obj(row):
                m_raise(ctx, "AttributeError",
                        m_attr_msg(m_type_name(row), "ended_at"))
                return None
            if m_get(row, "ended_at") is None:
                m_raise(ctx, "AttributeError",
                        m_attr_msg("types.SimpleNamespace", "ended_at"))
                return None
            if m_is_null(m_get(row, "ended_at")):
                ret = m_pystr(tip)
    return ret


def m_is_null(v):
    return v is None or v is JNULL


def m_migrate_transcript_queue_to_child(ctx, inp):
    sid = m_get_str(inp, "session_id", "")
    qsid = m_get_str(inp, "queue_session_id", "")
    child = m_get_str(inp, "child_id", "")
    head = m_get_bool(inp, "msg_is_head", False)
    dirty = m_dirty(ctx)
    pending = m_get(dirty, qsid)
    if pending is not None and len(pending) > 0 and head:
        m_pop0(pending)
    existing = m_get(dirty, child)
    existing_nonempty = m_is_arr(existing) and len(existing) > 0
    if pending is not None and len(pending) > 0:
        if existing_nonempty:
            n = len(existing)
            for i in range(n):
                m_append(pending, m_copy(existing[i]))
        P = m_take(dirty, qsid)
        m_set(dirty, child, P)
        if child == qsid:
            ret = m_take(dirty, child)
        else:
            ret = m_copy(P)
    else:
        if existing_nonempty:
            ret = m_copy(existing)
        else:
            ret = []
        m_del(dirty, qsid)
    failures = m_failures(ctx)
    previous = m_get_num(failures, qsid, 0)
    m_del(failures, qsid)
    if previous != 0:
        child_prev = m_get_num(failures, child, 0)
        m_set(failures, child, max(previous, child_prev))
    if not m_attr_present(ctx, "reroutes"):
        m_raise(ctx, "AttributeError",
                "'StoreDouble' object has no attribute '_transcript_reroutes'")
        return None
    m_set(m_sect(ctx, "reroutes"), sid, child)
    return ret


def m_publish_transcript_reroute(ctx, inp):
    sid_v = m_get(inp, "session_id")
    child = m_get_str(inp, "child_id", "")
    entries = m_entries(ctx)
    if m_is_obj(entries):
        for entry in list(entries.values()):
            if not m_is_obj(entry):
                m_raise(ctx, "AttributeError",
                        m_attr_msg(m_type_name(entry), "session_id"))
                return None
            if m_get(entry, "session_id") is None:
                m_raise(ctx, "AttributeError",
                        m_attr_msg("types.SimpleNamespace", "session_id"))
                return None
            if m_json_eq(m_get(entry, "session_id"), sid_v):
                m_set(entry, "session_id", child)
    r = m_call(ctx, "save", {})
    if m_prop(ctx, r):
        return None
    hints = m_sect(ctx, "session_owner_hints")
    if m_truthy(hints) and m_is_obj(hints):
        m_del(hints, child)
    return JNULL


def m_spool_stalled_backlog(ctx, inp):
    sid = m_get_str(inp, "session_id", "")
    qsid = m_get_str(inp, "queue_session_id", "")
    dirty = m_dirty(ctx)
    pending = m_get(dirty, qsid)
    backlog = m_copy(pending) if pending is not None else []
    spooled = 0
    sid_v = sid
    for i in range(m_len(backlog)):
        sp = m_spool_dropped_fn(ctx, sid_v, backlog[i])
        if ctx.exc is not None:
            return None
        if m_is_none(sp):
            break
        spooled += 1
    if spooled > 0:
        m_arr_add_once(m_lazy_set(ctx, "spooled_drop_sessions"), sid_v)
        for _ in range(spooled):
            m_pop0(pending)
        if len(pending) == 0:
            m_del(dirty, qsid)
    return spooled


def m_mark_spooled_drop_sessions(ctx, inp):
    ids = m_get(inp, "session_ids")
    if not m_truthy(ids):
        return JNULL
    set_ = m_lazy_set(ctx, "spooled_drop_sessions")
    if m_is_arr(ids):
        for item in ids:
            m_arr_add_once(set_, item)
    elif m_is_str(ids):
        for ch in ids:
            m_arr_add_once(set_, ch)
    return JNULL


def m_has_platform_message_id(ctx, inp):
    sid_v = m_get(inp, "session_id")
    store = m_call(ctx, "persistence_for_session_id",
                   {"session_id": m_copy_or_null(sid_v)})
    if m_prop(ctx, store):
        return None
    if not m_store_truthy(ctx, store):
        return False
    ret = None
    rc = 0
    if not m_is_store(ctx, store):
        rc = m_internal_exc(ctx, "AttributeError",
                            m_attr_msg(m_type_name(store), "messages"))
    else:
        ret = m_call(ctx, "messages.has_platform_message_id",
                     {"store": m_copy(store), "session_id": m_copy_or_null(sid_v),
                      "platform_message_id":
                          m_copy_or_null(m_get(inp, "platform_message_id"))})
        rc = m_exc_class(ctx, ret)
        if rc == 1:
            ret = None
    if rc == 2:
        return None
    if rc == 1:
        m_log(ctx, 10, "has_platform_message_id lookup failed", [], True)
        m_end_catch(ctx)
        return False
    return ret if ret is not None else JNULL


def m_transcript_tail_role(ctx, inp):
    cur = m_follow_reroutes_fn(ctx, m_get(inp, "session_id"))
    tip = m_call(ctx, "compression_tip_for_session_id",
                 {"session_id": m_copy(cur)})
    if m_prop(ctx, tip):
        return None
    store = m_call(ctx, "persistence_for_session_id",
                   {"session_id": m_copy_or_null(tip)})
    if m_prop(ctx, store):
        return None
    if not m_store_truthy(ctx, store):
        return JNULL
    ret = None
    rc = 0
    if not m_is_store(ctx, store):
        rc = m_internal_exc(ctx, "AttributeError",
                            m_attr_msg(m_type_name(store), "messages"))
    else:
        ret = m_call(ctx, "messages.latest_conversation_role",
                     {"store": m_copy(store), "session_id": m_copy_or_null(tip)})
        rc = m_exc_class(ctx, ret)
        if rc == 1:
            ret = None
    if rc == 2:
        return None
    if rc == 1:
        m_log(ctx, 10, "transcript tail lookup failed for %s",
              m_log_args1(tip), True)
        m_end_catch(ctx)
        return JNULL
    return ret if ret is not None else JNULL


def m_is_published_compression_child(ctx, inp):
    p_v = m_get(inp, "parent_session_id")
    c_v = m_get(inp, "child_session_id")
    if not m_truthy(p_v) or not m_truthy(c_v):
        return False
    store = m_call(ctx, "persistence_for_session_id",
                   {"session_id": m_copy(p_v)})
    if m_prop(ctx, store):
        return None
    if m_is_null(store):
        return False
    ret = False
    rc = 0
    child = r = w = None
    if not m_is_store(ctx, store):
        rc = m_internal_exc(ctx, "AttributeError",
                            m_attr_msg(m_type_name(store), "sessions"))
    else:
        child = m_call(ctx, "sessions.get",
                       {"store": m_copy(store), "session_id": m_copy(c_v)})
        rc = m_exc_class(ctx, child)
        if rc == 0:
            if m_row_truthy(child):
                if not m_is_obj(child):
                    rc = m_internal_exc(ctx, "AttributeError",
                                        m_attr_msg(m_type_name(child),
                                                   "parent_session_id"))
                elif m_get(child, "parent_session_id") is None:
                    rc = m_internal_exc(ctx, "AttributeError",
                                        m_attr_msg("types.SimpleNamespace",
                                                   "parent_session_id"))
                elif m_json_eq(m_get(child, "parent_session_id"), p_v):
                    r = m_call(ctx, "lineage.is_compression_child",
                               {"store": m_copy(store), "child": m_copy(child)})
                    rc = m_exc_class(ctx, r)
                    if rc == 0 and m_truthy(r):
                        w = m_call(ctx, "messages.active_watermark",
                                   {"store": m_copy(store),
                                    "session_id": m_copy(c_v)})
                        rc = m_exc_class(ctx, w)
                        if rc == 0:
                            if m_is_null(w):
                                rc = m_internal_exc(ctx, "AttributeError",
                                                    m_attr_msg("NoneType",
                                                               "last_active"))
                            elif not m_is_obj(w):
                                rc = m_internal_exc(ctx, "AttributeError",
                                                    m_attr_msg(m_type_name(w),
                                                               "last_active"))
                            elif m_get(w, "last_active") is None:
                                rc = m_internal_exc(ctx, "AttributeError",
                                                    m_attr_msg("types.SimpleNamespace",
                                                               "last_active"))
                            else:
                                ret = not m_is_null(m_get(w, "last_active"))
    if rc == 2:
        return None
    if rc == 1:
        m_log(ctx, 10, "published-compression-child probe failed for %s",
              m_log_args1(c_v), True)
        m_end_catch(ctx)
        ret = False
    return ret


def m_persist_rotated_compression_child(ctx, inp):
    r = m_is_published_compression_child(ctx, inp)
    if ctx.exc is not None:
        return None
    if m_truthy(r):
        return r
    rw = m_call(ctx, "rewrite_transcript",
                {"session_id": m_copy_or_null(m_get(inp, "child_session_id")),
                 "messages": m_copy_or_null(m_get(inp, "messages")),
                 "active_only": False, "reject_active_turn_lease": False})
    if m_prop(ctx, rw):
        return None
    return rw if rw is not None else JNULL


def m_has_input_owner(ctx, inp):
    sid_v = m_get(inp, "session_id")
    owner_v = m_get(inp, "owner")
    cur = m_follow_reroutes_fn(ctx, sid_v)
    store = m_call(ctx, "persistence_for_session_id",
                   {"session_id": m_copy(cur)})
    rc = m_exc_class(ctx, store)
    ret = None
    seen = []
    t = r = row = None
    if rc == 0:
        if not m_is_store(ctx, store):
            rc = m_internal_exc(ctx, "AttributeError",
                                m_attr_msg(m_type_name(store), "lineage"))
        else:
            t = m_call(ctx, "lineage.tip",
                       {"store": m_copy(store), "session_id": m_copy(cur)})
            rc = m_exc_class(ctx, t)
            if rc == 0:
                if m_truthy(t):
                    cur = m_copy(t)
                while m_truthy(cur) and not m_arr_has(seen, cur):
                    m_append(seen, m_copy(cur))
                    r = m_call(ctx, "messages.has_gateway_input_owner",
                               {"store": m_copy(store), "session_id": m_copy(cur),
                                "owner": m_copy_or_null(owner_v)})
                    rc = m_exc_class(ctx, r)
                    if rc != 0:
                        break
                    if m_truthy(r):
                        ret = True
                        break
                    r = None
                    row = m_call(ctx, "sessions.get",
                                 {"store": m_copy(store),
                                  "session_id": m_copy(cur)})
                    rc = m_exc_class(ctx, row)
                    if rc != 0:
                        break
                    if not m_row_truthy(row):
                        break
                    ic = m_call(ctx, "lineage.is_compression_child",
                                {"store": m_copy(store), "child": m_copy(row)})
                    rc = m_exc_class(ctx, ic)
                    if rc != 0:
                        break
                    is_child = m_truthy(ic)
                    if not is_child:
                        break
                    if not m_is_obj(row):
                        rc = m_internal_exc(ctx, "AttributeError",
                                            m_attr_msg(m_type_name(row),
                                                       "parent_session_id"))
                        break
                    if m_get(row, "parent_session_id") is None:
                        rc = m_internal_exc(ctx, "AttributeError",
                                            m_attr_msg("types.SimpleNamespace",
                                                       "parent_session_id"))
                        break
                    cur = m_copy(m_get(row, "parent_session_id"))
                    row = None
                if rc == 0 and ret is None:
                    ret = False
    if rc == 2:
        return None
    if rc == 1:
        m_end_catch(ctx)
        m_raise_transcript(ctx, sid_v)
        return None
    return ret if ret is not None else JNULL


def m_load_transcript(ctx, inp):
    sid_v = m_get(inp, "session_id")
    probe = m_call(ctx, "persistence_for_session_id",
                   {"session_id": m_copy_or_null(sid_v)})
    if m_prop(ctx, probe):
        return None
    if not m_store_truthy(ctx, probe):
        return []
    cur = m_follow_reroutes_fn(ctx, sid_v)
    store1 = m_call(ctx, "persistence_for_session_id",
                    {"session_id": m_copy(cur)})
    rc = m_exc_class(ctx, store1)
    if rc == 2:
        return None
    if rc == 0:
        if m_is_store(ctx, store1):
            t = m_call(ctx, "lineage.tip",
                       {"store": m_copy(store1), "session_id": m_copy(cur)})
            rc2 = m_exc_class(ctx, t)
            if rc2 == 2:
                return None
            if rc2 == 0:
                if m_truthy(t):
                    cur = m_copy(t)
            else:
                m_end_catch(ctx)
    else:
        m_end_catch(ctx)
    ret = rows = store2 = None
    rc = 0
    store2 = m_call(ctx, "persistence_for_session_id",
                    {"session_id": m_copy(cur)})
    rc = m_exc_class(ctx, store2)
    if rc == 2:
        return None
    if rc == 0:
        if not m_is_store(ctx, store2):
            m_internal_exc(ctx, "AttributeError",
                           m_attr_msg(m_type_name(store2), "messages"))
            rc = 1
        else:
            rows = m_call(ctx, "messages.read_conversation",
                          {"store": m_copy(store2), "session_id": m_copy(cur),
                           "repair_alternation": True})
            rc = m_exc_class(ctx, rows)
            if rc == 0:
                if not m_is_arr(rows):
                    m_internal_exc(ctx, "TypeError",
                                   "'%s' object is not iterable"
                                   % m_type_name(rows))
                    rc = 1
                else:
                    ret = []
                    for i in range(len(rows)):
                        m = m_call(ctx, "as_openai_message",
                                   {"message": m_copy(rows[i])})
                        rc = m_exc_class(ctx, m)
                        if rc == 2:
                            return None
                        if rc == 1:
                            ret = None
                            break
                        m_append(ret, m if m is not None else JNULL)
    if rc == 1:
        m_log(ctx, 40,
              "Transcript read failed for session %s; refusing to treat the "
              "conversation as empty: %s",
              m_log_args2(cur, m_cur_msg(ctx)), True)
        m_end_catch(ctx)
        m_raise_transcript(ctx, cur)
        return None
    return ret


def m_retain_refused_turn_for_display(ctx, inp):
    sid_v = m_get(inp, "session_id")
    sid = m_get_str(inp, "session_id", "")
    store = m_call(ctx, "persistence_for_session_id",
                   {"session_id": m_copy_or_null(sid_v)})
    if m_prop(ctx, store):
        return None
    if not m_store_truthy(ctx, store):
        return JNULL
    lock = m_call(ctx, "get_transcript_drain_lock", {})
    if m_prop(ctx, lock):
        return None
    ret = None
    rc = 0
    if not m_is_store(ctx, store):
        rc = m_internal_exc(ctx, "AttributeError",
                            m_attr_msg(m_type_name(store), "messages"))
    else:
        ret = m_call(ctx, "messages.retain_refused_turn_for_display",
                     {"store": m_copy(store), "session_id": m_copy_or_null(sid_v),
                      "notice": m_copy_or_null(m_get(inp, "notice")),
                      "boundary_row_id":
                          m_copy_or_null(m_get(inp, "boundary_row_id"))})
        rc = m_exc_class(ctx, ret)
        if rc == 1:
            ret = None
    if rc == 2:
        return None
    if rc == 1:
        m_log(ctx, 10, "retain_refused_turn_for_display: failed: %s",
              [m_cur_msg(ctx)], False)
        m_end_catch(ctx)
        return JNULL
    if not m_truthy(ret):
        return JNULL
    c = m_call(ctx, "clear_dirty_transcript", {"session_id": sid})
    if m_prop(ctx, c):
        return None
    m_del(m_dirty(ctx), sid)
    m_del(m_failures(ctx), sid)
    return {"retained": True}


DISPATCH = {
    "TranscriptReadError": m_transcript_read_error_init,
    "_spool_dropped": m_spool_dropped,
    "_bindable_text": m_bindable_text,
    "transcript_append_kwargs": m_transcript_append_kwargs,
    "_follow_reroutes": m_follow_reroutes,
    "_enqueue_transcript_message": m_enqueue_transcript_message,
    "_divert_transcript_after_db_replaced": m_divert_transcript_after_db_replaced,
    "_live_compression_child": m_live_compression_child,
    "_migrate_transcript_queue_to_child": m_migrate_transcript_queue_to_child,
    "_publish_transcript_reroute": m_publish_transcript_reroute,
    "_spool_stalled_backlog": m_spool_stalled_backlog,
    "mark_spooled_drop_sessions": m_mark_spooled_drop_sessions,
    "has_platform_message_id": m_has_platform_message_id,
    "transcript_tail_role": m_transcript_tail_role,
    "is_published_compression_child": m_is_published_compression_child,
    "persist_rotated_compression_child": m_persist_rotated_compression_child,
    "has_input_owner": m_has_input_owner,
    "load_transcript": m_load_transcript,
    "retain_refused_turn_for_display": m_retain_refused_turn_for_display,
}


def mirror_run(rec_):
    inp = rec_["in"]
    func = rec_["func"]
    state = to_c(inp.get("state") or {})
    if not m_is_obj(state):
        state = {}
    for key in ("dirty_transcripts", "append_failures", "entries", "stores"):
        if not m_is_obj(m_get(state, key)):
            m_set(state, key, {})
    for key in ("reroutes", "spooled_drop_sessions", "session_owner_hints"):
        if m_get(state, key) is None:
            m_set(state, key, JNULL)
    if func == "_migrate_transcript_queue_to_child":
        qsid = inp.get("queue_session_id", "")
        pending = inp.get("pending")
        m_set(m_get(state, "dirty_transcripts"), qsid,
              to_c(pending) if pending is not None else [])
    io = to_c(inp.get("io") or {})
    ctx = MCtx(io, state)
    g = lambda k: to_c(inp[k]) if k in inp else None
    cin = {k: to_c(v) for k, v in inp.items()}
    ret = DISPATCH[func](ctx, cin)
    if ctx.exc is not None:
        ret_out = ctx.exc
    else:
        ret_out = ret if ret is not None else JNULL
    rr = m_sect(ctx, "reroutes")
    sp = m_sect(ctx, "spooled_drop_sessions")
    hints = m_sect(ctx, "session_owner_hints")
    st = {
        "dirty_transcripts": m_copy(m_sect(ctx, "dirty_transcripts")),
        "append_failures": m_copy(m_sect(ctx, "append_failures")),
        "reroutes": m_copy(rr) if (rr is not None and rr is not JNULL) else JNULL,
        "spooled_drop_sessions":
            sorted(m_copy(sp)) if (sp is not None and sp is not JNULL) else JNULL,
        "session_owner_hints":
            m_copy(hints) if (hints is not None and hints is not JNULL) else JNULL,
        "entries": m_copy(m_sect(ctx, "entries")),
    }
    return {"ret": from_c(ret_out), "trace": from_c(ctx.trace),
            "state": from_c(st)}


def main():
    proxy = "--proxy" in sys.argv
    ns = oracle.extract()
    oracle.install_stubs(ns)
    oracle.CUR["cls"] = oracle.make_double_class(ns)
    fx = HERE / "fixtures" / "session_transcript_flow.jsonl"
    recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").split("\n")
            if l.strip()]
    if proxy:
        # GATE PROXY: serialize the mirror envelopes and pipe into the oracle
        for rec_ in recs:
            mir = mirror_run(rec_)
            print(json.dumps({"func": rec_["func"], "in": rec_["in"],
                              "out": mir}, ensure_ascii=False))
        return 0
    mism = 0
    shown = 0
    for i, rec_ in enumerate(recs):
        live = oracle.run_case(rec_)
        mir = mirror_run(rec_)
        if json.dumps(mir, sort_keys=False, ensure_ascii=False) != \
                json.dumps(live, sort_keys=False, ensure_ascii=False):
            mism += 1
            if shown < 10:
                shown += 1
                print(f"SHADOW MISMATCH case {i} {rec_['func']}:")
                print(f"  MIRROR={json.dumps(mir, ensure_ascii=False)[:800]}")
                print(f"  LIVE  ={json.dumps(live, ensure_ascii=False)[:800]}")
    print(f"SHADOW session_transcript_flow: {len(recs)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())