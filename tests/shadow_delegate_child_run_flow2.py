#!/usr/bin/env python3
"""Literal Python shadow-mirror of src/tools/delegate_child_run_flow2.c (dcr2_*).

Code-only verification (no C compile/link): transcribes the C helper algorithms
and entry flows FAITHFULLY (presence semantics via a MISSING sentinel, the same
exception-parent table, the same value model / seam dispatcher / raise-spec
propagation), then:
  diff  -- mirror {ret,trace,st} vs the oracle's LIVE drive over every fixture.
  proxy -- serialize mirror envelopes to JSONL and pipe into the oracle's REAL
           main() (subprocess stdin); expect 'ORACLE ...: N cases, 0 mismatches'.
Run: python tests/shadow_delegate_child_run_flow2.py [diff|proxy|both]
"""
import copy
import json
import math
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
FIX = HERE / "fixtures" / "delegate_child_run_flow2.jsonl"
ORACLE = HERE / "oracle_delegate_child_run_flow2.py"
PYBIN = "C:/Users/eman5/hermes-agent/.venv/Scripts/python.exe"

MISSING = object()

# ---- exception-parent table (verbatim from the C) ----
EXC_PARENT = {
    "ArithmeticError": "Exception", "AssertionError": "Exception", "AttributeError": "Exception",
    "BaseExceptionGroup": "BaseException", "BlockingIOError": "OSError", "BrokenPipeError": "ConnectionError",
    "BufferError": "Exception", "BytesWarning": "Warning", "CancelledError": "BaseException",
    "ChildProcessError": "OSError", "ConnectionAbortedError": "ConnectionError", "ConnectionError": "OSError",
    "ConnectionRefusedError": "ConnectionError", "ConnectionResetError": "ConnectionError",
    "DeprecationWarning": "Warning", "EOFError": "Exception", "EncodingWarning": "Warning",
    "EnvironmentError": "OSError", "Exception": "BaseException", "ExceptionGroup": "Exception",
    "FileExistsError": "OSError", "FileNotFoundError": "OSError", "FloatingPointError": "ArithmeticError",
    "FutureWarning": "Warning", "GeneratorExit": "BaseException", "IOError": "OSError",
    "ImportError": "Exception", "ImportWarning": "Warning", "IndentationError": "SyntaxError",
    "IndexError": "LookupError", "InterruptedError": "OSError", "IsADirectoryError": "OSError",
    "KeyError": "LookupError", "KeyboardInterrupt": "BaseException", "LookupError": "Exception",
    "MemoryError": "Exception", "ModuleNotFoundError": "ImportError", "NameError": "Exception",
    "NotADirectoryError": "OSError", "NotImplementedError": "RuntimeError", "OSError": "Exception",
    "OverflowError": "ArithmeticError", "PendingDeprecationWarning": "Warning", "PermissionError": "OSError",
    "ProcessLookupError": "OSError", "PythonFinalizationError": "RuntimeError", "RecursionError": "RuntimeError",
    "ReferenceError": "Exception", "ResourceWarning": "Warning", "RuntimeError": "Exception",
    "RuntimeWarning": "Warning", "StopAsyncIteration": "Exception", "StopIteration": "Exception",
    "SyntaxError": "Exception", "SyntaxWarning": "Warning", "SystemError": "Exception",
    "SystemExit": "BaseException", "TabError": "IndentationError", "TimeoutError": "OSError",
    "TypeError": "Exception", "UnboundLocalError": "NameError", "UnicodeDecodeError": "UnicodeError",
    "UnicodeEncodeError": "UnicodeError", "UnicodeError": "ValueError", "UnicodeTranslateError": "UnicodeError",
    "UnicodeWarning": "Warning", "UserWarning": "Warning", "ValueError": "Exception",
    "Warning": "Exception", "WindowsError": "OSError", "ZeroDivisionError": "ArithmeticError",
    "_IncompleteInputError": "SyntaxError",
}


def exc_parent(name):
    return EXC_PARENT.get(name, "Exception")


def exc_is_instance(kind, base):
    cur = kind
    for _ in range(32):
        if cur == base:
            return True
        p = exc_parent(cur)
        if p is None or cur == p:
            break
        cur = p
        if cur == "BaseException":
            return base == "BaseException"
    return False


def is_exception(kind):
    return exc_is_instance(kind, "Exception")


# ---- value helpers (mirror C; MISSING models an absent key / NULL pointer) ----
def is_null(v):
    return v is None or v is MISSING


def is_obj(v):
    return isinstance(v, dict)


def is_arr(v):
    return isinstance(v, list)


def is_str(v):
    return isinstance(v, str)


def is_num(v):
    return isinstance(v, (int, float)) and not isinstance(v, bool)


def is_bool(v):
    return isinstance(v, bool)


def truth(v):
    if v is None or v is MISSING:
        return False
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v != 0
    if isinstance(v, str):
        return len(v) > 0
    if isinstance(v, (list, dict)):
        return len(v) > 0
    return False


def jstr(v):
    return v if isinstance(v, str) else None


def jnum(v):
    if isinstance(v, bool):
        return 1.0 if v else 0.0
    if isinstance(v, (int, float)):
        return float(v)
    return 0.0


def jget(o, k):
    if isinstance(o, dict) and k in o:
        return o[k]
    return MISSING


def is_plain_dict(v):
    return isinstance(v, dict) and "__attr__" not in v


def is_number_or_bool(v):
    return is_num(v) or is_bool(v)


def is_int(v):
    if is_bool(v):
        return True
    if not is_num(v):
        return False
    d = v
    return math.isfinite(d) and d == int(d)


def typename(v):
    if v is None or v is MISSING:
        return "NoneType"
    if isinstance(v, bool):
        return "bool"
    if isinstance(v, (int, float)):
        d = v
        return "int" if (math.isfinite(d) and d == int(d) and abs(d) < 9.2e18) else "float"
    if isinstance(v, str):
        return "str"
    if isinstance(v, list):
        return "list"
    if isinstance(v, dict):
        tag = jstr(jget(v, "__class__"))
        if tag:
            return tag
        if "__attr__" in v:
            return "types.SimpleNamespace"
        if "__callable__" in v:
            return "function"
        return "dict"
    return "object"


# ---- attribute doubles ----
def dattr(d, name):
    return jget(jget(d, "__attr__"), name)


def getattr_(d, name, defval):
    inner = jget(d, "__attr__")
    if isinstance(inner, dict):
        v = jget(inner, name)
        return defval if v is MISSING else v
    return defval


def hasattr_(d, name):
    inner = jget(d, "__attr__")
    return isinstance(inner, dict) and name in inner


def dset(d, name, val):
    inner = jget(d, "__attr__")
    if isinstance(inner, dict):
        inner[name] = val


# ---- raise specs ----
def is_raise(r):
    return isinstance(r, dict) and "__raise__" in r


def kind(r):
    return jstr(jget(r, "__raise__")) or ""


def raise_msg(r):
    return jstr(jget(r, "message")) or ""


def mkraise(k, m):
    return {"__raise__": k, "message": m if m is not None else ""}


def attr_raise(obj, name):
    return mkraise("AttributeError", "'%s' object has no attribute '%s'" % (typename(obj), name))


def not_callable_raise(v):
    return mkraise("TypeError", "'%s' object is not callable" % typename(v))


def is_callable(v):
    return isinstance(v, dict) and "__callable__" in v


# ---- string / number helpers ----
def pystr(v):
    if v is None or v is MISSING:
        return "None"
    if isinstance(v, bool):
        return "True" if v else "False"
    if isinstance(v, str):
        return v
    if isinstance(v, (int, float)):
        d = v
        if math.isfinite(d) and d == int(d) and abs(d) < 9.2e18:
            return "%.0f" % d
        return float_repr(d)
    return ""


def float_repr(d):
    for p in range(1, 18):
        s = "%.*g" % (p, d)
        try:
            if float(s) == d:
                return s
        except ValueError:
            pass
    return "%.17g" % d


def slice_(s, n):
    p = jstr(s)
    if p is None:
        return ""
    return p[:n]


def count_pct(s):
    return (s or "").count("%")


def is_strip_ws(c):
    return c in " \t\n\r\v\f" or (0x1c <= ord(c) <= 0x1f)


def strip_(s):
    if s is None:
        return ""
    a = 0
    while a < len(s) and is_strip_ws(s[a]):
        a += 1
    b = len(s)
    while b > a and is_strip_ws(s[b - 1]):
        b -= 1
    return s[a:b]


def rstrip_(s):
    if s is None:
        return ""
    b = len(s)
    while b > 0 and is_strip_ws(s[b - 1]):
        b -= 1
    return s[:b]


def round_nd(x, nd):
    if not math.isfinite(x):
        return x
    return float("%.*f" % (nd, x))


def fmt_f0(x):
    return "%.0f" % x


def fmt_g(x):
    return "%g" % x


def fmt_code_raise(v, code):
    if is_str(v):
        return mkraise("ValueError", "Unknown format code '%s' for object of type 'str'" % code)
    return mkraise("TypeError", "unsupported format string passed to %s.__format__" % typename(v))


def iprefix(s, pref):
    return s[:len(pref)].lower() == pref.lower()


def float_grammar(s):
    p = 0
    if p < len(s) and s[p] in "+-":
        p += 1
    if iprefix(s[p:], "inf"):
        p += 3
        if iprefix(s[p:], "inity"):
            p += 5
        return p == len(s)
    if iprefix(s[p:], "nan"):
        p += 3
        return p == len(s)
    digits = 0
    while p < len(s) and s[p].isdigit():
        p += 1; digits = 1
    if p < len(s) and s[p] == ".":
        p += 1
        while p < len(s) and s[p].isdigit():
            p += 1; digits = 1
    if not digits:
        return False
    if p < len(s) and s[p] in "eE":
        p += 1
        if p < len(s) and s[p] in "+-":
            p += 1
        ed = 0
        while p < len(s) and s[p].isdigit():
            p += 1; ed = 1
        if not ed:
            return False
    return p == len(s)


def py_float(v):
    if is_num(v):
        return float(v)
    if is_bool(v):
        return 1.0 if v else 0.0
    if is_str(v):
        s = v
        a = 0
        while a < len(s) and is_strip_ws(s[a]):
            a += 1
        b = len(s)
        while b > a and is_strip_ws(s[b - 1]):
            b -= 1
        clean = s[a:b]
        if len(clean) == 0 or not float_grammar(clean):
            return mkraise("ValueError", "could not convert string to float: '%s'" % clean)
        return float(clean)
    return mkraise("TypeError", "float() argument must be a string or a real number, not '%s'" % typename(v))


def py_int(v):
    if is_bool(v):
        return 1.0 if v else 0.0
    if is_num(v):
        return float(int(v))
    if is_str(v):
        s = v
        a = 0
        while a < len(s) and s[a] in " \t\n\r\v\f":
            a += 1
        b = len(s)
        while b > a and s[b - 1] in " \t\n\r\v\f":
            b -= 1
        clean = s[a:b]
        p = 0
        if p < len(clean) and clean[p] in "+-":
            p += 1
        digits = 0
        while p < len(clean) and clean[p].isdigit():
            p += 1; digits = 1
        if not digits or p != len(clean):
            return mkraise("ValueError", "invalid literal for int() with base 10: '%s'" % clean)
        return float(int(clean, 10))
    return mkraise("TypeError", "int() argument must be a string, a bytes-like object or a real number, not '%s'" % typename(v))


# ---- norm_value projection (mirror C dcr2_norm_value) ----
def norm_value_mirror(v):
    if v is MISSING:
        return None
    if isinstance(v, dict):
        inner = v.get("__attr__")
        if isinstance(inner, dict):
            return {k: norm_value_mirror(x) for k, x in inner.items() if not k.startswith("__td2")}
        if "__callable__" in v:
            return copy.deepcopy(v)
        return {k: norm_value_mirror(x) for k, x in v.items()}
    if isinstance(v, list):
        return [norm_value_mirror(x) for x in v]
    if isinstance(v, float):
        if math.isfinite(v) and v == int(v) and abs(v) < 9.2e18:
            return int(v)
        return v
    return copy.deepcopy(v)


# ---- list coercion / unpack / join / json.dumps ----
def list_coerce(v):
    if is_arr(v):
        return copy.deepcopy(v)
    if is_str(v):
        return list(v)
    if is_obj(v):
        return list(v.keys())
    return mkraise("TypeError", "'%s' object is not iterable" % typename(v))


def unpack2(v):
    if is_arr(v):
        n = len(v)
        if n < 2:
            return mkraise("ValueError", "not enough values to unpack (expected 2, got %d)" % n), None, None
        if n > 2:
            return mkraise("ValueError", "too many values to unpack (expected 2, got %d)" % n), None, None
        return None, copy.deepcopy(v[0]), copy.deepcopy(v[1])
    if is_str(v):
        n = len(v)
        if n < 2:
            return mkraise("ValueError", "not enough values to unpack (expected 2, got %d)" % n), None, None
        if n > 2:
            return mkraise("ValueError", "too many values to unpack (expected 2)"), None, None
        return None, v[0], v[1]
    return mkraise("TypeError", "cannot unpack non-iterable %s object" % typename(v)), None, None


def join_(arr, sep):
    if not is_arr(arr):
        return mkraise("TypeError", "can only join an iterable")
    parts = []
    for i, e in enumerate(arr):
        if not is_str(e):
            return mkraise("TypeError", "sequence item %d: expected str instance, %s found" % (i, typename(e)))
        parts.append(e)
    return sep.join(parts)


def dumps_str(s):
    out = ['"']
    for c in s:
        o = ord(c)
        if c == '"':
            out.append('\\"')
        elif c == "\\":
            out.append("\\\\")
        elif c == "\n":
            out.append("\\n")
        elif c == "\r":
            out.append("\\r")
        elif c == "\t":
            out.append("\\t")
        elif c == "\b":
            out.append("\\b")
        elif c == "\f":
            out.append("\\f")
        elif o < 0x20:
            out.append("\\u%04x" % o)
        else:
            out.append(c)
    out.append('"')
    return "".join(out)


def dumps_buf(v):
    if v is None or v is MISSING:
        return "null"
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, (int, float)):
        d = v
        if math.isfinite(d) and d == int(d) and abs(d) < 9.2e18:
            return "%.0f" % d
        return float_repr(d)
    if isinstance(v, str):
        return dumps_str(v)
    if isinstance(v, list):
        return "[" + ", ".join(dumps_buf(x) for x in v) + "]"
    if isinstance(v, dict):
        if "__attr__" in v or "__callable__" in v:
            return dumps_str(pystr(v))
        return "{" + ", ".join(dumps_str(k) + ": " + dumps_buf(x) for k, x in v.items()) + "}"
    return "null"


def json_dumps(v):
    return dumps_buf(v)


# ---- the seam dispatcher + io (mirror C harness) ----
class IO:
    def __init__(self, io_scripts):
        self.io = io_scripts or {}
        self.cursors = {}
        self.trace = []

    def strip_nulls(self, v):
        if v is MISSING:
            return None
        if isinstance(v, list):
            return [self.strip_nulls(x) for x in v]
        if not isinstance(v, dict):
            return copy.deepcopy(v)
        return {k: self.strip_nulls(x) for k, x in v.items() if x is not None}

    def call(self, name, args):
        entry = self.io.get(name, MISSING)
        present = entry is not MISSING
        exhausted = False
        result = None
        if present:
            if isinstance(entry, list):
                idx = self.cursors.get(name, 0)
                if idx >= len(entry):
                    exhausted = True
                else:
                    result = entry[idx]
                    self.cursors[name] = idx + 1
            else:
                result = entry
        failclosed = (not present) or exhausted
        actual = result
        if not failclosed and isinstance(actual, dict) and actual.get("__script__") is True:
            actual = actual.get("ret")
        if failclosed:
            retnode = mkraise("RuntimeError", "no script for " + name)
        else:
            retnode = copy.deepcopy(actual) if actual is not None else None
        ev = {"ev": name, "args": self.strip_nulls(args), "ret": copy.deepcopy(retnode)}
        self.trace.append(ev)
        if is_raise(retnode):
            return retnode
        if retnode is None:
            return None
        return retnode


def arg_s(a, k, v):
    if v is not None:
        a[k] = v


def arg_j(a, k, v):
    if v is not None and v is not MISSING:
        a[k] = copy.deepcopy(v)


def arg_b(a, k, v):
    a[k] = bool(v)


def arg_n(a, k, v):
    a[k] = v


def logger(io, level, msg, args_arr, exc_info):
    a = {}
    arg_s(a, "msg", msg)
    a["args"] = args_arr if args_arr is not None else []
    if exc_info:
        arg_b(a, "exc_info", 1)
    return io.call("logger." + level, a)


def quiet_factory(io, msg, log_args, exc_info):
    a = {}
    arg_s(a, "log_message", msg)
    if is_arr(log_args) and len(log_args) > 0:
        a["log_args"] = copy.deepcopy(log_args)
    if exc_info:
        arg_b(a, "exc_info", 1)
    r = io.call("quiet", a)
    if is_raise(r):
        return r
    return None


def quiet_exit(io, msg, log_args, exc_info, r):
    if not is_exception(kind(r)):
        return r
    if msg is None:
        return None
    pct = count_pct(msg)
    nargs = len(log_args) if is_arr(log_args) else 0
    la = [copy.deepcopy(x) for x in (log_args if is_arr(log_args) else [])]
    if pct > nargs:
        la.append(raise_msg(r))
    return logger(io, "debug", msg, la, exc_info)


def cm_factory(io, name, args):
    r = io.call(name, args)
    if is_raise(r):
        return r
    return None


def str_or_none_seam(io, v):
    a = {}
    arg_j(a, "value", v)
    return io.call("_str_or_none", a)


def num_seam(io, v):
    a = {}
    arg_j(a, "value", v)
    return io.call("_num", a)


def route_report_seam(io, child):
    a = {}
    arg_j(a, "child", child)
    return io.call("_route_report", a)


# =====================================================================
# transcribed entry flows (mirror C dcr2_*)
# =====================================================================
def f_is_image_url(io, inp, st):
    ref = jget(inp, "ref")
    if not is_str(ref):
        return attr_raise(ref, "startswith")
    s = ref
    return s.startswith("http://") or s.startswith("https://") or s.startswith("data:image/")


def is_image_url_v(ref):
    if not is_str(ref):
        return False
    return ref.startswith("http://") or ref.startswith("https://") or ref.startswith("data:image/")


def budget_warning_text_v(idle, ct):
    if not is_number_or_bool(idle):
        return fmt_code_raise(idle, "f")
    if not is_number_or_bool(ct):
        return fmt_code_raise(ct, "f")
    a = fmt_f0(jnum(idle))
    b = fmt_f0(jnum(ct))
    return ("[delegation budget warning] No progress signal for %ss of your %ss "
            "inactivity window. Finish the current step and return your summary now \u2014 "
            "the work is discarded if the window elapses." % (a, b))


def f_budget_warning_text(io, inp, st):
    return budget_warning_text_v(jget(inp, "idle_seconds"), jget(inp, "child_timeout"))


def f_warn_child_budget(io, inp, st):
    child = jget(st, "child")
    idle = jget(inp, "idle_seconds")
    ct = jget(inp, "child_timeout")
    steer = getattr_(child, "steer", None)
    if not is_callable(steer):
        return None
    text = budget_warning_text_v(idle, ct)
    body_raise = None
    if is_raise(text):
        body_raise = text
    else:
        seam = jstr(jget(steer, "__callable__"))
        a = {}
        arg_j(a, "text", text)
        r = io.call(seam or "child.steer", a)
        if is_raise(r):
            body_raise = r
    if body_raise:
        if not is_exception(kind(body_raise)):
            return body_raise
        la = [raise_msg(body_raise)]
        lr = logger(io, "debug", "budget warning steer failed: %s", la, 0)
        if is_raise(lr):
            return lr
    return None


def f_format_thread_stack(io, inp, st):
    frame = jget(inp, "frame")
    indent = jstr(jget(inp, "indent"))
    if indent is None:
        indent = ""
    a = {}
    arg_j(a, "frame", frame)
    fs = io.call("traceback.format_stack", a)
    if is_raise(fs):
        return fs
    if not is_arr(fs):
        return mkraise("TypeError", "'%s' object is not iterable" % typename(fs))
    out = []
    for fl in fs:
        if not is_str(fl):
            return attr_raise(fl, "rstrip")
        rs = rstrip_(fl)
        for sub in rs.split("\n"):
            out.append(indent + sub)
    return out


def diag_section_core(io, label, produce, ctx):
    prod = produce(io, ctx)
    if is_raise(prod):
        if not is_exception(kind(prod)):
            return prod
        return ["  %s%s>" % (label, raise_msg(prod))]
    lst = list_coerce(prod)
    if is_raise(lst):
        if not is_exception(kind(lst)):
            return lst
        return ["  %s%s>" % (label, raise_msg(lst))]
    return lst


def diag_produce_seam(io, ctx):
    produce = ctx
    if not is_callable(produce):
        return not_callable_raise(produce)
    seam = jstr(jget(produce, "__callable__"))
    return io.call(seam or "diag.produce", {})


def f_diag_section(io, inp, st):
    label = jstr(jget(inp, "label"))
    if label is None:
        label = ""
    produce = jget(inp, "produce")
    return diag_section_core(io, label, diag_produce_seam, produce)


def diag_prompt(io, child):
    sp = getattr_(child, "ephemeral_system_prompt", None)
    if not truth(sp):
        sp = getattr_(child, "system_prompt", None)
    if not truth(sp):
        sp = ""
    is_s = is_str(sp)
    s = sp if is_s else ""
    slen = len(s)
    b1 = ("  system_prompt_bytes: %d" % slen) if is_s else "  system_prompt_bytes: n/a"
    b2 = ("  system_prompt_chars: %d" % slen) if is_s else "  system_prompt_chars: n/a"
    return [b1, b2]


def diag_tools(io, child):
    ts = getattr_(child, "tools", None)
    if is_null(ts):
        return []
    if is_arr(ts) or is_obj(ts):
        count = len(ts)
    elif is_str(ts):
        count = len(ts)
    else:
        return mkraise("TypeError", "object of type '%s' has no len()" % typename(ts))
    dumps = json_dumps(ts)
    nbytes = len(dumps)
    return ["  tool_schema_count: %d" % count, "  tool_schema_bytes: %d" % nbytes]


def f_diag_sizes(io, inp, st):
    child = jget(st, "child")
    out = ["## Prompt / schema sizes"]
    p = diag_section_core(io, "system_prompt: <error: ", diag_prompt, child)
    if is_raise(p):
        return p
    out.extend(p)
    t = diag_section_core(io, "tool_schema: <error: ", diag_tools, child)
    if is_raise(t):
        return t
    out.extend(t)
    return out


def f_child_last_event_age(io, inp, st):
    child = jget(st, "child")
    if not hasattr_(child, "get_activity_summary"):
        return None
    gas = dattr(child, "get_activity_summary")
    if not is_callable(gas):
        return None
    gas_seam = jstr(jget(gas, "__callable__"))
    summ = io.call(gas_seam or "child.get_activity_summary", {})
    if is_raise(summ):
        if is_exception(kind(summ)):
            return None
        return summ
    if not truth(summ):
        d = {}
    else:
        d = summ
    if not is_plain_dict(d):
        return None
    ts = jget(d, "last_activity_ts")
    if is_null(ts):
        return None
    now = io.call("time.time", {})
    if is_raise(now):
        if is_exception(kind(now)):
            return None
        return now
    ft = py_float(ts)
    if is_raise(ft):
        if is_exception(kind(ft)):
            return None
        return ft
    diff = jnum(now) - jnum(ft)
    m = diff if diff > 0.0 else 0.0
    return round_nd(m, 2)


def f_merge_late_steer(io, inp, st):
    result = jget(st, "result")
    child = jget(st, "child")
    sid = jget(inp, "subagent_id")
    late = None
    if truth(sid):
        a = {}
        arg_j(a, "subagent_id", sid)
        arg_j(a, "child", child)
        late = io.call("_close_subagent_steering", a)
        if is_raise(late):
            return late
    if not truth(late):
        return None
    existing = jget(result, "pending_steer")
    if is_str(existing) and truth(existing):
        val = "%s\n%s" % (existing, jstr(late) or "")
    else:
        val = copy.deepcopy(late)
    result["pending_steer"] = val
    return None


def f_reserve_child_transcript(io, inp, st):
    child = jget(st, "child")
    if not hasattr_(child, "_session_db"):
        return attr_raise(child, "_session_db")
    db = dattr(child, "_session_db")
    if is_null(db):
        return None
    if not hasattr_(child, "_persist_disabled"):
        return attr_raise(child, "_persist_disabled")
    pd = dattr(child, "_persist_disabled")
    if truth(pd):
        return None
    dbclass = jstr(jget(db, "__class__"))
    if dbclass == "RuntimeSessionStore":
        return None
    body_raise = None
    close_raise = None

    def handle(body_raise):
        close = getattr_(child, "close", None)
        if is_callable(close):
            seam = jstr(jget(close, "__callable__"))
            r = io.call(seam or "child.close", {})
            cr = r if is_raise(r) else None
        elif is_null(close):
            cr = attr_raise(child, "close")
        else:
            cr = not_callable_raise(close)
        if cr:
            return cr
        return body_raise

    if not hasattr_(child, "_parent_session_id"):
        return handle(attr_raise(child, "_parent_session_id"))
    parent_sid = dattr(child, "_parent_session_id")
    if not hasattr_(child, "persistence"):
        return handle(attr_raise(child, "persistence"))
    persistence = dattr(child, "persistence")
    if not hasattr_(persistence, "sessions"):
        return handle(attr_raise(persistence, "sessions"))
    sessions = dattr(persistence, "sessions")
    if not hasattr_(sessions, "create"):
        return handle(attr_raise(sessions, "create"))
    create = dattr(sessions, "create")
    if not hasattr_(child, "session_id"):
        return handle(attr_raise(child, "session_id"))
    session_id = dattr(child, "session_id")
    na = {}
    arg_j(na, "session_id", session_id)
    arg_s(na, "source", "subagent")
    arg_j(na, "parent_session_id", parent_sid)
    if truth(parent_sid):
        na["model_config"] = {"_delegate_from": copy.deepcopy(parent_sid)}
    ns = io.call("NewSession", na)
    if is_raise(ns):
        return handle(ns)
    if not is_callable(create):
        return handle(not_callable_raise(create))
    seam = jstr(jget(create, "__callable__"))
    ca = {}
    arg_j(ca, "new_session", ns)
    cr = io.call(seam or "child.persistence.sessions.create", ca)
    if is_raise(cr):
        return handle(cr)
    dset(child, "_session_db_reserved", True)
    return None


def f_register_child(io, inp, st):
    child = jget(st, "child")
    parent = jget(st, "parent_agent")
    goal = jget(inp, "goal")

    def _in(k):
        v = jget(inp, k)
        return None if v is MISSING else copy.deepcopy(v)

    osid = _in("owner_session_id")
    otrans = _in("owner_transport")
    orec = _in("owner_session_record")
    sid = getattr_(child, "_subagent_id", None)
    if not is_str(sid) or not truth(sid):
        return None
    if is_null(osid):
        qf = quiet_factory(io, None, None, 0)
        if qf:
            return qf
        ga = {}
        arg_s(ga, "key", "HERMES_UI_SESSION_ID")
        arg_s(ga, "default", "")
        env = io.call("get_session_env", ga)
        if is_raise(env):
            ex = quiet_exit(io, None, None, 0, env)
            if ex:
                return ex
        else:
            osid = env if truth(env) else None
    if truth(osid) and (is_null(otrans) or is_null(orec)):
        ca = {}
        arg_j(ca, "owner_session_id", osid)
        cap = io.call("_capture_gateway_steer_authority", ca)
        if is_raise(cap):
            return cap
        ur, t, r = unpack2(cap)
        if ur:
            return ur
        otrans, orec = t, r
    raw_depth = getattr_(child, "_delegate_depth", None)
    depth = 0.0
    if is_int(raw_depth):
        d = jnum(raw_depth) - 1.0
        depth = d if d > 0.0 else 0.0
    entry = {}
    entry["subagent_id"] = copy.deepcopy(sid)
    psid = getattr_(child, "_parent_subagent_id", None)
    parent_id = str_or_none_seam(io, psid)
    if is_raise(parent_id):
        return parent_id
    entry["parent_id"] = parent_id
    entry["depth"] = depth
    entry["goal"] = copy.deepcopy(goal) if goal is not MISSING else None
    did = getattr_(child, "_delegation_id", None)
    deleg = str_or_none_seam(io, did)
    if is_raise(deleg):
        return deleg
    entry["delegation_id"] = deleg
    model = getattr_(child, "model", None)
    model_s = str_or_none_seam(io, model)
    if is_raise(model_s):
        return model_s
    entry["model"] = model_s
    now = io.call("time.time", {})
    if is_raise(now):
        return now
    entry["started_at"] = now
    entry["status"] = "running"
    entry["tool_count"] = 0
    entry["agent"] = norm_value_mirror(child)
    cpsid = getattr_(child, "_parent_session_id", None)
    c1 = pystr(cpsid) if truth(cpsid) else ""
    pssid = getattr_(parent, "session_id", None)
    c2 = pystr(pssid) if truth(pssid) else ""
    if c1:
        entry["owner_agent_session_id"] = c1
    elif c2:
        entry["owner_agent_session_id"] = c2
    else:
        entry["owner_agent_session_id"] = None
    entry["owner_session_id"] = copy.deepcopy(osid)
    entry["owner_transport"] = copy.deepcopy(otrans)
    entry["owner_session_record"] = copy.deepcopy(orec)
    ra = {"entry": entry}
    reg = io.call("_register_subagent", ra)
    if is_raise(reg):
        return reg
    return copy.deepcopy(sid)


def f_create_isolated_worktree(io, inp, st):
    parent = jget(st, "parent_agent")
    parent_task_id = jget(inp, "parent_task_id")
    subagent_id = jget(inp, "subagent_id")
    iso = io.call("_get_worktree_isolation", {})
    if is_raise(iso):
        return iso
    enabled = truth(iso)
    if not enabled:
        return None
    qf = quiet_factory(io, "worktree isolation setup failed: %s", None, 0)
    if qf:
        return qf
    body_raise = None
    result = None
    done = False
    parent_cwd = None
    lb = io.call("local_backend_active", {})
    if is_raise(lb):
        body_raise = lb
    else:
        local = truth(lb)
        if not local:
            lr = logger(io, "debug", "worktree isolation skipped: non-local terminal backend", [], 0)
            if is_raise(lr):
                body_raise = lr
            else:
                result = None; done = True
        else:
            qf2 = quiet_factory(io, None, None, 0)
            if qf2:
                body_raise = qf2
            else:
                ga = {}
                arg_j(ga, "task_id", parent_task_id)
                cwd = io.call("get_session_cwd", ga)
                if is_raise(cwd):
                    ex = quiet_exit(io, None, None, 0, cwd)
                    if ex:
                        body_raise = ex
                else:
                    parent_cwd = cwd
                if body_raise is None:
                    if truth(parent_cwd):
                        cwd_arg = copy.deepcopy(parent_cwd)
                    else:
                        ha = {}
                        arg_j(ha, "parent_agent", parent)
                        cwd_arg = io.call("_resolve_workspace_hint", ha)
                        if is_raise(cwd_arg):
                            body_raise = cwd_arg
                    if body_raise is None:
                        ca = {}
                        arg_j(ca, "cwd", cwd_arg)
                        arg_j(ca, "subagent_id", subagent_id)
                        cr = io.call("create_subagent_worktree", ca)
                        if is_raise(cr):
                            body_raise = cr
                        else:
                            result = cr; done = True
    if body_raise:
        ex = quiet_exit(io, "worktree isolation setup failed: %s", None, 0, body_raise)
        if ex:
            return ex
        return None
    if done:
        return result
    return None


def f_lease_child_credential(io, inp, st):
    child = jget(st, "child")
    pool = getattr_(child, "_credential_pool", None)
    if is_null(pool):
        return [None, None]
    base_url = getattr_(child, "base_url", None)
    cm = {}
    arg_j(cm, "child", child)
    cmr = cm_factory(io, "child_profile_scope", cm)
    if cmr:
        return cmr
    body_raise = None
    leased_id = None
    acq = io.call("pool.acquire_lease", {})
    if is_raise(acq):
        body_raise = acq
    else:
        leased_id = acq
        if not is_null(leased_id):
            qf = quiet_factory(io, "Failed to bind child to leased credential: %s", None, 0)
            if qf:
                body_raise = qf
            else:
                bind_raise = None
                leased_entry = None
                entries = io.call("pool.entries", {})
                if is_raise(entries):
                    bind_raise = entries
                elif not is_arr(entries):
                    bind_raise = mkraise("TypeError", "'NoneType' object is not iterable")
                else:
                    for e in entries:
                        if not hasattr_(e, "id"):
                            bind_raise = attr_raise(e, "id"); break
                        eid = dattr(e, "id")
                        if eid is not MISSING and leased_id is not None and type(eid) == type(leased_id):
                            eq = False
                            if is_str(eid) and is_str(leased_id):
                                eq = eid == leased_id
                            elif is_num(eid) and is_num(leased_id):
                                eq = jnum(eid) == jnum(leased_id)
                            if eq:
                                leased_entry = e; break
                if bind_raise is None:
                    sa = {}
                    arg_j(sa, "entry", leased_entry)
                    arg_j(sa, "base_url", base_url)
                    serves = io.call("credential_pool_entry_serves_endpoint", sa)
                    if is_raise(serves):
                        bind_raise = serves
                    else:
                        ok = truth(serves)
                        if not ok:
                            rl = {}
                            arg_j(rl, "cred_id", leased_id)
                            rr = io.call("pool.release_lease", rl)
                            if is_raise(rr):
                                bind_raise = rr
                            else:
                                entries2 = io.call("pool.entries", {})
                                leased_entry = None
                                if is_raise(entries2):
                                    bind_raise = entries2
                                elif not is_arr(entries2):
                                    bind_raise = mkraise("TypeError", "'NoneType' object is not iterable")
                                else:
                                    for e in entries2:
                                        if not hasattr_(e, "last_status"):
                                            bind_raise = attr_raise(e, "last_status"); break
                                        ls = dattr(e, "last_status")
                                        dead = is_str(ls) and ls == "dead"
                                        if dead:
                                            continue
                                        sa2 = {}
                                        arg_j(sa2, "entry", e)
                                        arg_j(sa2, "base_url", base_url)
                                        sv = io.call("credential_pool_entry_serves_endpoint", sa2)
                                        if is_raise(sv):
                                            bind_raise = sv; break
                                        if truth(sv):
                                            leased_entry = e; break
                                if bind_raise is None:
                                    if leased_entry is not None:
                                        if not hasattr_(leased_entry, "id"):
                                            bind_raise = attr_raise(leased_entry, "id"); leased_id = None
                                        else:
                                            eid = dattr(leased_entry, "id")
                                            aa = {}
                                            arg_j(aa, "cred_id", eid)
                                            acq2 = io.call("pool.acquire_lease", aa)
                                            if is_raise(acq2):
                                                bind_raise = acq2; leased_id = None
                                            else:
                                                leased_id = acq2
                                    else:
                                        leased_id = None
                        if bind_raise is None and leased_entry is not None and hasattr_(child, "_swap_credential"):
                            swap = dattr(child, "_swap_credential")
                            if not is_callable(swap):
                                bind_raise = not_callable_raise(swap)
                            else:
                                seam = jstr(jget(swap, "__callable__"))
                                swa = {}
                                arg_j(swa, "entry", leased_entry)
                                swr = io.call(seam or "child._swap_credential", swa)
                                if is_raise(swr):
                                    bind_raise = swr
                if bind_raise:
                    ex = quiet_exit(io, "Failed to bind child to leased credential: %s", None, 0, bind_raise)
                    if ex:
                        body_raise = ex
    if body_raise:
        return body_raise
    return [copy.deepcopy(pool), leased_id if leased_id is not None else None]


def build_tool_trace_v(io, messages):
    trace = []
    if not is_arr(messages):
        return trace
    by_id = []  # list of {id, idx}
    for msg in messages:
        if not is_plain_dict(msg):
            continue
        role = jget(msg, "role")
        rs = jstr(role)
        if rs == "assistant":
            tcs = jget(msg, "tool_calls")
            if not truth(tcs):
                continue
            if not is_arr(tcs):
                return mkraise("TypeError", "'%s' object is not iterable" % typename(tcs))
            for tc in tcs:
                if not is_plain_dict(tc):
                    return attr_raise(tc, "get")
                fn = jget(tc, "function")
                if fn is MISSING:
                    fn = None
                if fn is not None and not is_plain_dict(fn):
                    return attr_raise(fn, "get")
                args = jget(fn, "arguments") if fn is not None else MISSING
                if args is MISSING:
                    args = None
                if is_str(args):
                    abytes = len(args)
                elif is_arr(args) or is_obj(args):
                    abytes = len(args)
                elif is_null(args):
                    abytes = 0
                else:
                    return mkraise("TypeError", "object of type '%s' has no len()" % typename(args))
                name = jget(fn, "name") if fn is not None else MISSING
                sa = {}
                arg_j(sa, "arguments", args)
                summary = io.call("_summarize_tool_arguments", sa)
                if is_raise(summary):
                    return summary
                entry = {}
                entry["tool"] = copy.deepcopy(name) if name is not MISSING else "unknown"
                entry["args_bytes"] = abytes
                entry["input_summary"] = summary
                idx = len(trace)
                trace.append(entry)
                tid = jget(tc, "id")
                if truth(tid):
                    found = -1
                    for k, pair in enumerate(by_id):
                        kid = pair["id"]
                        if type(kid) == type(tid):
                            eq = (kid == tid) if is_str(kid) else (jnum(kid) == jnum(tid) if is_num(kid) else False)
                            if eq:
                                found = k; break
                    pair = {"id": copy.deepcopy(tid), "idx": idx}
                    if found >= 0:
                        by_id[found] = pair
                    else:
                        by_id.append(pair)
        elif rs == "tool":
            content = jget(msg, "content")
            ca = {}
            arg_j(ca, "content", content)
            content_s = io.call("_stringify_tool_content", ca)
            if is_raise(content_s):
                return content_s
            if is_str(content_s):
                rbytes = len(content_s)
            elif is_arr(content_s) or is_obj(content_s):
                rbytes = len(content_s)
            else:
                return mkraise("TypeError", "object of type '%s' has no len()" % typename(content_s))
            la = {}
            arg_j(la, "content", content_s)
            looks = io.call("_looks_like_error_output", la)
            if is_raise(looks):
                return looks
            is_err = truth(looks)
            meta_status = "error" if is_err else "ok"
            tc_id = jget(msg, "tool_call_id")
            target = -1
            if truth(tc_id):
                for pair in by_id:
                    kid = pair["id"]
                    if type(kid) == type(tc_id):
                        eq = (kid == tc_id) if is_str(kid) else (jnum(kid) == jnum(tc_id) if is_num(kid) else False)
                        if eq:
                            target = pair["idx"]; break
            if target < 0 and len(trace) > 0:
                target = len(trace) - 1
            if target >= 0:
                trace[target]["result_bytes"] = rbytes
                trace[target]["status"] = meta_status
    return trace


def f_build_tool_trace(io, inp, st):
    return build_tool_trace_v(io, jget(inp, "messages"))


def schema_outcome(schema, valid, errors, retries):
    return {"schema": schema, "valid": valid, "errors": errors if errors is not None else [], "retries": retries}


def f_validate_child_output_schema(io, inp, st):
    child = jget(st, "child")
    result = jget(st, "result")
    task_index = jget(inp, "task_index")
    child_task_id = jget(inp, "child_task_id")
    relay = jget(inp, "relay_child_text")
    schema = getattr_(child, "_delegate_output_schema", None)
    if not is_plain_dict(schema):
        return schema_outcome(copy.deepcopy(schema) if schema is not MISSING else None, None, None, 0)
    fr = jget(result, "final_response")
    first_text = copy.deepcopy(fr) if truth(fr) else ""
    va = {}
    arg_j(va, "text", first_text)
    arg_j(va, "schema", schema)
    vr = io.call("validate_output", va)
    if is_raise(vr):
        return vr
    ur, valid, errors = unpack2(vr)
    if ur:
        return ur
    short_circuit = truth(valid)
    if not short_circuit:
        if not is_str(first_text):
            return attr_raise(first_text, "strip")
        if not strip_(first_text):
            short_circuit = True
    if not short_circuit:
        if truth(jget(result, "interrupted")):
            short_circuit = True
    if short_circuit:
        return schema_outcome(copy.deepcopy(schema), valid, errors, 0)
    msgs = jget(result, "messages")
    history = msgs if is_arr(msgs) else None
    retry_result = None
    retry_raise = None
    cm1 = {}
    arg_j(cm1, "child", child)
    cm1r = cm_factory(io, "child_profile_scope", cm1)
    if cm1r:
        retry_raise = cm1r
    else:
        sid = getattr_(child, "session_id", None)
        sid_str = pystr(sid) if truth(sid) else ""
        cm2 = {}
        arg_s(cm2, "session_id", sid_str)
        cm2r = cm_factory(io, "delegated_child_context", cm2)
        if cm2r:
            retry_raise = cm2r
        else:
            ba = {}
            arg_j(ba, "errors", errors)
            retry_msg = io.call("build_retry_message", ba)
            if is_raise(retry_msg):
                retry_raise = retry_msg
            else:
                aa = {}
                arg_j(aa, "message", retry_msg)
                arg_j(aa, "schema", schema)
                user_msg = io.call("append_output_contract", aa)
                if is_raise(user_msg):
                    retry_raise = user_msg
                else:
                    run = getattr_(child, "run_conversation", None)
                    if not is_callable(run):
                        retry_raise = attr_raise(child, "run_conversation") if is_null(run) else not_callable_raise(run)
                    else:
                        seam = jstr(jget(run, "__callable__"))
                        ra = {}
                        arg_j(ra, "user_message", user_msg)
                        arg_j(ra, "task_id", child_task_id)
                        arg_j(ra, "stream_callback", relay)
                        if history is not None:
                            ra["conversation_history"] = copy.deepcopy(history)
                        rr = io.call(seam or "child.run_conversation", ra)
                        if is_raise(rr):
                            retry_raise = rr
                        else:
                            retry_result = rr
    if retry_raise:
        if not is_exception(kind(retry_raise)):
            return retry_raise
        la = [copy.deepcopy(task_index) if task_index is not MISSING else 0, raise_msg(retry_raise)]
        lr = logger(io, "warning", "Subagent %d schema-retry turn failed: %s", la, 0)
        if is_raise(lr):
            return lr
    if is_plain_dict(retry_result):
        rfr = jget(retry_result, "final_response")
        retry_text = copy.deepcopy(rfr) if truth(rfr) else ""
        if not is_str(retry_text):
            return attr_raise(retry_text, "strip")
        if strip_(retry_text):
            result["final_response"] = copy.deepcopy(retry_text)
        rc = jget(result, "api_calls")
        rc_v = rc if truth(rc) else None
        i1 = py_int(rc_v if rc_v is not None else 0)
        ac = jget(retry_result, "api_calls")
        ac_v = ac if truth(ac) else None
        i2 = py_int(ac_v if ac_v is not None else 0)
        if not is_raise(i1) and not is_raise(i2):
            result["api_calls"] = jnum(i1) + jnum(i2)
        rmsgs = jget(retry_result, "messages")
        if is_arr(rmsgs) and history is not None:
            start_node = jget(retry_result, "current_turn_user_idx")
            if is_int(start_node):
                start = int(jnum(start_node))
            else:
                mx = -1
                for i, m in enumerate(rmsgs):
                    if not is_plain_dict(m):
                        return attr_raise(m, "get")
                    mrole = jget(m, "role")
                    if is_str(mrole) and mrole == "user":
                        mx = i
                start = mx if mx >= 0 else 0
            n = len(rmsgs)
            s = start
            if s < 0:
                s = n + s
            if s < 0:
                s = 0
            if s > n:
                s = n
            newmsgs = copy.deepcopy(history) + [copy.deepcopy(x) for x in rmsgs[s:]]
            result["messages"] = newmsgs
        va2 = {}
        arg_j(va2, "text", retry_text)
        arg_j(va2, "schema", schema)
        vr2 = io.call("validate_output", va2)
        if is_raise(vr2):
            return vr2
        ur2, v2, e2 = unpack2(vr2)
        if ur2:
            return ur2
        valid, errors = v2, e2
    return schema_outcome(copy.deepcopy(schema), valid, errors, 1)


def f_build_result_entry(io, inp, st):
    child = jget(st, "child")
    result = jget(st, "result")
    task_index = jget(inp, "task_index")
    duration = jget(inp, "duration")
    schema = jget(inp, "schema")
    sch_schema = jget(schema, "schema")
    sch_valid = jget(schema, "valid")
    sch_errors = jget(schema, "errors")
    sch_retries = jget(schema, "retries")
    fr = jget(result, "final_response")
    summary = copy.deepcopy(fr) if truth(fr) else ""
    usable = False
    if truth(summary):
        if not is_str(summary):
            return attr_raise(summary, "strip")
        usable = strip_(summary) != "(empty)"
    status = None
    exit_reason = None
    interrupt_note = ""
    if truth(jget(result, "interrupted")):
        status = "interrupted"; exit_reason = "interrupted"
        sum_strip = strip_(summary if is_str(summary) else "")
        msgs = jget(result, "messages")
        mlist = msgs if truth(msgs) else None
        partial = None
        if mlist is not None and not is_arr(mlist):
            return mkraise("TypeError", "'%s' object is not reversible" % typename(mlist))
        if mlist is not None:
            for k in range(len(mlist) - 1, -1, -1):
                m = mlist[k]
                if not is_plain_dict(m):
                    return attr_raise(m, "get")
                mrole = jget(m, "role")
                if not (is_str(mrole) and mrole == "assistant"):
                    continue
                content = jget(m, "content")
                fa = {}
                arg_j(fa, "content", content)
                flat = io.call("flatten_message_text", fa)
                if is_raise(flat):
                    return flat
                if not is_str(flat):
                    return attr_raise(flat, "strip")
                t = strip_(flat)
                in_ph = (t == "") or (t == sum_strip) or (t == "Operation interrupted.")
                if not in_ph:
                    partial = t; break
        if partial:
            interrupt_note = sum_strip
            sum_strip = None
            summary = partial
        if sum_strip is not None:
            pass
        stale = jget(result, "_stale_stop_seconds")
        if not is_null(stale):
            status = "timeout"; exit_reason = "timeout"
            if not is_number_or_bool(stale):
                return fmt_code_raise(stale, "g")
            gs = fmt_g(jnum(stale))
            interrupt_note = ("Subagent stopped making progress \u2014 no activity for %ss "
                              "(heartbeat stale threshold); it was stopped." % gs)
    else:
        failed = jget(result, "failed")
        error = jget(result, "error")
        if truth(failed) or truth(error):
            status = "failed"; exit_reason = "error"
        else:
            completed = jget(result, "completed")
            exit_reason = "completed" if truth(completed) else "max_iterations"
            status = "completed" if usable else "failed"
    entry = {}
    entry["task_index"] = copy.deepcopy(task_index) if task_index is not MISSING else 0
    entry["status"] = status
    entry["summary"] = copy.deepcopy(summary)
    api_calls = jget(result, "api_calls")
    entry["api_calls"] = copy.deepcopy(api_calls) if api_calls is not MISSING else 0
    entry["duration_seconds"] = copy.deepcopy(duration) if duration is not MISSING else 0
    model = getattr_(child, "model", None)
    model_s = str_or_none_seam(io, model)
    if is_raise(model_s):
        return model_s
    entry["model"] = model_s
    entry["exit_reason"] = exit_reason
    entry["truncated"] = exit_reason == "max_iterations"
    tokens = {}
    pt = getattr_(child, "session_prompt_tokens", None)
    pt_v = pt if pt is not MISSING else 0
    in_num = num_seam(io, pt_v)
    if is_raise(in_num):
        return in_num
    tokens["input"] = in_num
    ct = getattr_(child, "session_completion_tokens", None)
    ct_v = ct if ct is not MISSING else 0
    out_num = num_seam(io, ct_v)
    if is_raise(out_num):
        return out_num
    tokens["output"] = out_num
    entry["tokens"] = tokens
    msgs = jget(result, "messages")
    mlist = msgs if truth(msgs) else []
    tt = build_tool_trace_v(io, mlist)
    if is_raise(tt):
        return tt
    entry["tool_trace"] = tt
    role = getattr_(child, "_delegate_role", None)
    entry["_child_role"] = copy.deepcopy(role) if role is not MISSING else None
    cost = getattr_(child, "session_estimated_cost_usd", None)
    cost_usd = 0.0
    if is_number_or_bool(cost):
        cost_usd = jnum(cost) if truth(cost) else 0.0
    entry["_child_cost_usd"] = cost_usd
    entry["cost_usd"] = round_nd(cost_usd, 6)
    cost_status = getattr_(child, "session_cost_status", None)
    entry["cost_status"] = copy.deepcopy(cost_status) if (is_str(cost_status) and truth(cost_status)) else "unknown"
    route = route_report_seam(io, child)
    if is_raise(route):
        return route
    if not is_null(route):
        entry["route"] = route
    if status == "failed":
        err = jget(result, "error")
        entry["error"] = copy.deepcopy(err) if err is not MISSING else "Subagent did not produce a response."
        frs = jget(result, "failure_reason")
        if is_str(frs) and truth(frs):
            entry["failure_reason"] = copy.deepcopy(frs)
        resets = jget(result, "failure_resets_at")
        if is_number_or_bool(resets) and not is_bool(resets):
            entry["failure_resets_at"] = copy.deepcopy(resets)
        ul = jget(result, "usage_limit")
        if is_plain_dict(ul):
            entry["usage_limit"] = copy.deepcopy(ul)
    elif interrupt_note:
        entry["error"] = interrupt_note
    if is_plain_dict(sch_schema):
        entry["schema_valid"] = bool(truth(sch_valid))
        if truth(sch_retries):
            entry["schema_retries"] = copy.deepcopy(sch_retries)
        if not truth(sch_valid) and truth(sch_errors):
            entry["schema_errors"] = copy.deepcopy(sch_errors)
        if is_bool(sch_valid) and not sch_valid and usable:
            retry_suffix = " (after 1 retry)" if truth(sch_retries) else ""
            entry["schema_note"] = ("Final answer does not satisfy the declared output_schema%s; `summary` is the "
                                    "child's raw, UNVALIDATED final text \u2014 extract what you need from it yourself "
                                    "(see schema_errors) rather than re-running the task." % retry_suffix)
    missed = jget(result, "pending_steer")
    if is_str(missed):
        ms = strip_(missed)
        if ms:
            entry["missed_steer"] = copy.deepcopy(missed)
            note = "[steer did not land \u2014 the subagent finished before it could be delivered: %s]" % missed
            if truth(summary):
                newsum = "%s\n\n%s" % (jstr(summary) or "", note)
            else:
                newsum = note
            entry["summary"] = newsum
    return entry


def f_build_child_goal_message(io, inp, st):
    child = jget(st, "child")
    goal = jget(inp, "goal")
    images = jget(inp, "images")
    goal_s = jstr(goal) or ""
    body_raise = None
    result = None
    data_urls = []
    urls = []
    paths = []
    if not is_arr(images):
        body_raise = mkraise("TypeError", "'%s' object is not iterable" % typename(images))
    else:
        for s in images:
            if not is_str(s):
                body_raise = attr_raise(s, "startswith"); break
            is_data = s.startswith("data:image/")
            is_url = is_image_url_v(s)
            if is_data:
                data_urls.append(s)
            if is_url and not is_data:
                urls.append(s)
            if not is_url:
                paths.append(s)
    cfg = None
    if body_raise is None:
        qf = quiet_factory(io, None, None, 0)
        if qf:
            body_raise = qf
        else:
            lc = io.call("load_config_readonly", {})
            if is_raise(lc):
                ex = quiet_exit(io, None, None, 0, lc)
                if ex:
                    body_raise = ex
            else:
                cfg = lc
    if body_raise is None:
        prov = getattr_(child, "provider", None)
        mod = getattr_(child, "model", None)
        rprov = getattr_(child, "requested_provider", None)
        prov_s = pystr(prov) if truth(prov) else ""
        mod_s = pystr(mod) if truth(mod) else ""
        rprov_s = pystr(rprov) if truth(rprov) else ""
        da = {}
        arg_s(da, "provider", prov_s)
        arg_s(da, "model", mod_s)
        arg_j(da, "cfg", cfg)
        arg_s(da, "requested_provider", rprov_s)
        mode = io.call("decide_image_input_mode", da)
        if is_raise(mode):
            body_raise = mode
        else:
            ms = jstr(mode)
            native = ms == "native"
            if native:
                ba = {}
                arg_s(ba, "goal", goal_s)
                ba["paths"] = copy.deepcopy(paths)
                ba["urls"] = copy.deepcopy(urls)
                br = io.call("build_native_content_parts", ba)
                if is_raise(br):
                    body_raise = br
                else:
                    ur, parts, skipped = unpack2(br)
                    if ur:
                        body_raise = ur
                    else:
                        if truth(skipped):
                            nskip = len(skipped) if is_arr(skipped) else 0
                            first3 = skipped[:3] if is_arr(skipped) else []
                            js = join_(first3, ", ")
                            if is_raise(js):
                                body_raise = js
                            else:
                                la = [nskip, js]
                                lr = logger(io, "warning", "delegate_task: skipped %d unreadable image(s) for subagent: %s", la, 0)
                                if is_raise(lr):
                                    body_raise = lr
                        if body_raise is None and truth(data_urls):
                            if truth(parts):
                                base = parts
                            else:
                                base = [{"type": "text", "text": goal_s}]
                            for u in data_urls:
                                base = base + [{"type": "image_url", "image_url": {"url": u}}]
                            parts = base
                        if body_raise is None:
                            has_image = False
                            if is_arr(parts):
                                for p in parts:
                                    if not is_plain_dict(p):
                                        body_raise = attr_raise(p, "get"); break
                                    pt = jget(p, "type")
                                    if is_str(pt) and pt == "image_url":
                                        has_image = True; break
                            if body_raise is None:
                                if has_image:
                                    result = parts
                                else:
                                    result = goal_s
            else:
                if truth(data_urls):
                    la = [len(data_urls)]
                    lr = logger(io, "warning", "delegate_task: %d inline data-URL image(s) dropped for a non-vision subagent", la, 0)
                    if is_raise(lr):
                        body_raise = lr
                if body_raise is None:
                    hints = []
                    for p in paths:
                        fa = {}
                        arg_j(fa, "path", p)
                        isf = io.call("os.path.isfile", fa)
                        if is_raise(isf):
                            body_raise = isf; break
                        if truth(isf):
                            hints.append("[Image attached at: %s]" % p)
                        else:
                            la = [copy.deepcopy(p)]
                            lr = logger(io, "warning", "delegate_task: image path not found, not forwarded: %s", la, 0)
                            if is_raise(lr):
                                body_raise = lr; break
                    if body_raise is None:
                        for u in urls:
                            hints.append("[Image attached: %s]" % u)
                        if not truth(hints):
                            result = goal_s
                        else:
                            js = join_(hints, "\n")
                            if is_raise(js):
                                body_raise = js
                            else:
                                result = "%s\n\n%s\nUse vision_analyze to inspect these images." % (goal_s, js)
    if body_raise:
        if not is_exception(kind(body_raise)):
            return body_raise
        lr = logger(io, "warning", "delegate_task: image forwarding failed; sending text-only goal", [], 1)
        if is_raise(lr):
            return lr
        return goal_s
    return result


def flush_attr(inner):
    if not is_obj(inner):
        return None
    if "__attr__" in inner:
        v = dattr(inner, "_flush")
        return None if v is MISSING else v
    v = jget(inner, "_flush")
    return None if v is MISSING else v


def f_checkpointing(io, inp, st):
    inner = jget(inp, "inner")
    cb = {"__callable__": "checkpointing.cb"}
    flush = flush_attr(inner)
    if flush is not None:
        cb["_flush"] = copy.deepcopy(flush)
    return cb


def cb_invoke(io, inp, event):
    inner = jget(inp, "inner")
    checkpoint = jget(inp, "checkpoint")
    et = jget(event, "event_type")
    args = jget(event, "args")
    kwargs = jget(event, "kwargs")
    if is_callable(inner):
        seam = jstr(jget(inner, "__callable__"))
        a = {}
        arg_j(a, "event_type", et)
        a["args"] = copy.deepcopy(args) if args is not MISSING else []
        a["kwargs"] = copy.deepcopy(kwargs) if kwargs is not MISSING else {}
        r = io.call(seam or "inner.cb", a)
        if is_raise(r):
            return r
    ets = jstr(et)
    if ets == "tool.completed":
        qf = quiet_factory(io, "Durable subagent checkpoint failed: %s", None, 0)
        if qf:
            return qf
        body_raise = None
        if is_callable(checkpoint):
            seam = jstr(jget(checkpoint, "__callable__"))
            r = io.call(seam or "checkpoint.cb", {})
            if is_raise(r):
                body_raise = r
        else:
            body_raise = not_callable_raise(checkpoint)
        if body_raise:
            ex = quiet_exit(io, "Durable subagent checkpoint failed: %s", None, 0, body_raise)
            if ex:
                return ex
    return None


DISPATCH = {
    "_reserve_child_transcript": f_reserve_child_transcript,
    "_format_thread_stack": f_format_thread_stack,
    "_diag_section": f_diag_section,
    "_diag_sizes": f_diag_sizes,
    "_budget_warning_text": f_budget_warning_text,
    "_warn_child_budget": f_warn_child_budget,
    "_child_last_event_age": f_child_last_event_age,
    "_register_child": f_register_child,
    "_create_isolated_worktree": f_create_isolated_worktree,
    "_lease_child_credential": f_lease_child_credential,
    "_merge_late_steer": f_merge_late_steer,
    "_validate_child_output_schema": f_validate_child_output_schema,
    "_build_tool_trace": f_build_tool_trace,
    "_build_result_entry": f_build_result_entry,
    "_is_image_url": f_is_image_url,
    "_build_child_goal_message": f_build_child_goal_message,
    "_checkpointing": f_checkpointing,
}


def run_mirror(func, inp):
    st = copy.deepcopy(inp.get("state") or {})
    io = IO(inp.get("io"))
    fn = DISPATCH.get(func)
    if fn is None:
        ret = mkraise("RuntimeError", "unknown func")
    else:
        ret = fn(io, inp, st)
        if func == "_checkpointing" and not is_raise(ret):
            events = jget(inp, "events")
            if is_arr(events):
                for ev in events:
                    r = cb_invoke(io, inp, ev)
                    if is_raise(r):
                        ret = r
                        break
        if ret is None:
            ret = None
    return {"func": func, "in": copy.deepcopy(inp), "ret": ret, "trace": io.trace, "st": st}


def load_oracle():
    import importlib.util
    spec = importlib.util.spec_from_file_location("orc_dcr2", str(ORACLE))
    orc = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(orc)
    orc.load_ns()
    return orc


def read_fixtures():
    return [json.loads(l) for l in FIX.read_text(encoding="utf-8").splitlines() if l.strip()]


def diff_mode():
    orc = load_oracle()
    recs = read_fixtures()
    n = 0
    mism = 0
    for rec in recs:
        n += 1
        func = rec["func"]
        inp = rec["in"]
        mir = run_mirror(func, inp)
        got = orc.norm_value({"ret": mir["ret"], "trace": mir["trace"], "st": mir["st"]})
        want = orc.run_case(func, inp)
        if got != want:
            mism += 1
            if mism <= 15:
                print("SHADOW MISMATCH [%s] case %d" % (func, n))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s MIR : %s" % (key, json.dumps(got.get(key), ensure_ascii=False, default=str)[:500]))
                        print("  %s LIVE: %s" % (key, json.dumps(want.get(key), ensure_ascii=False, default=str)[:500]))
    print("SHADOW delegate_child_run_flow2 diff: %d cases, %d mismatches" % (n, mism))
    return 1 if mism else 0


def proxy_mode():
    recs = read_fixtures()
    lines = []
    for rec in recs:
        mir = run_mirror(rec["func"], rec["in"])
        lines.append(json.dumps(mir, ensure_ascii=False))
    payload = "\n".join(lines) + "\n"
    p = subprocess.run([PYBIN, str(ORACLE)], input=payload, capture_output=True, text=True, encoding="utf-8")
    sys.stdout.write(p.stdout)
    if p.stderr:
        sys.stderr.write(p.stderr)
    return p.returncode


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "both"
    rc = 0
    if mode in ("diff", "both"):
        rc |= diff_mode()
    if mode in ("proxy", "both"):
        rc |= proxy_mode()
    return rc


if __name__ == "__main__":
    sys.exit(main())