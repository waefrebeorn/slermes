#!/usr/bin/env python3
"""Literal Python shadow-mirror of src/agent/moa_loop_flow3.c (ml3_*).

Code-only verification (no C compile/link): transcribes the C helper
algorithms and entry flows FAITHFULLY (presence semantics via a MISSING
sentinel — json_obj_get returns NULL for an absent key but a JSON_NULL node
for a present null; the same exception-parent table; the same seam dispatcher
/ script shapes; the same state model / normalization), then:
  diff  -- mirror {ret,raise,trace,state} vs the oracle's LIVE drive over
           every fixture case (canon equality).
  proxy -- serialize mirror envelopes to harness-output JSONL and pipe into
           the oracle's REAL main() (subprocess stdin); expect
           'ORACLE moa_loop_flow3: N cases, 0 mismatches'.
Run: python tests/shadow_moa_loop_flow3.py [diff|proxy|both]
"""
import copy
import importlib.util
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
FIX = HERE / "fixtures" / "moa_loop_flow3.jsonl"
ORACLE = HERE / "oracle_moa_loop_flow3.py"
PYBIN = "C:/Users/eman5/hermes-agent/.venv/Scripts/python.exe"

MISSING = object()   # C NULL pointer (absent key); None == JSON_NULL node

# ---- exception-parent table (verbatim from the C) ----
EXC_PARENT = {
    "BaseException": None, "SystemExit": "BaseException",
    "KeyboardInterrupt": "BaseException", "GeneratorExit": "BaseException",
    "CancelledError": "BaseException", "BaseExceptionGroup": "BaseException",
    "Exception": "BaseException", "ExceptionGroup": "Exception",
    "TypeError": "Exception", "StopAsyncIteration": "Exception",
    "StopIteration": "Exception", "ArithmeticError": "Exception",
    "FloatingPointError": "ArithmeticError", "OverflowError": "ArithmeticError",
    "ZeroDivisionError": "ArithmeticError", "AssertionError": "Exception",
    "AttributeError": "Exception", "BufferError": "Exception",
    "EOFError": "Exception", "ImportError": "Exception",
    "ModuleNotFoundError": "ImportError", "LookupError": "Exception",
    "IndexError": "LookupError", "KeyError": "LookupError",
    "MemoryError": "Exception", "NameError": "Exception",
    "UnboundLocalError": "NameError", "OSError": "Exception",
    "EnvironmentError": "OSError", "IOError": "OSError",
    "WindowsError": "OSError", "BlockingIOError": "OSError",
    "ChildProcessError": "OSError", "ConnectionError": "OSError",
    "BrokenPipeError": "ConnectionError",
    "ConnectionAbortedError": "ConnectionError",
    "ConnectionRefusedError": "ConnectionError",
    "ConnectionResetError": "ConnectionError", "FileExistsError": "OSError",
    "FileNotFoundError": "OSError", "InterruptedError": "OSError",
    "IsADirectoryError": "OSError", "NotADirectoryError": "OSError",
    "PermissionError": "OSError", "ProcessLookupError": "OSError",
    "TimeoutError": "OSError", "ReferenceError": "Exception",
    "RuntimeError": "Exception", "NotImplementedError": "RuntimeError",
    "PythonFinalizationError": "RuntimeError", "RecursionError": "RuntimeError",
    "SyntaxError": "Exception", "IndentationError": "SyntaxError",
    "_IncompleteInputError": "SyntaxError", "TabError": "IndentationError",
    "SystemError": "Exception", "ValueError": "Exception",
    "UnicodeError": "ValueError", "UnicodeDecodeError": "UnicodeError",
    "UnicodeEncodeError": "UnicodeError",
    "UnicodeTranslateError": "UnicodeError", "Warning": "Exception",
    "DeprecationWarning": "Warning", "PendingDeprecationWarning": "Warning",
    "RuntimeWarning": "Warning", "SyntaxWarning": "Warning",
    "UserWarning": "Warning", "FutureWarning": "Warning",
    "ImportWarning": "Warning", "UnicodeWarning": "Warning",
    "BytesWarning": "Warning", "ResourceWarning": "Warning",
    "EncodingWarning": "Warning",
}


def is_exception(kind):
    if kind is None:
        return True
    cur = kind
    for _ in range(16):
        if cur == "Exception":
            return True
        if cur == "BaseException":
            return False
        if cur not in EXC_PARENT:
            return True
        cur = EXC_PARENT[cur]
        if cur is None:
            return False
    return True


# ---- value helpers (literal ml3_*) ----

def is_null(v):
    return v is MISSING or v is None


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


def is_dict(v):
    return is_obj(v) and "__attr__" not in v


def attr(v):
    if not is_obj(v):
        return None
    a = v.get("__attr__")
    return a if isinstance(a, dict) else None


def is_callable(v):
    return is_obj(v) and is_str(v.get("__call__"))


def is_acct(v):
    return is_obj(v) and is_str(v.get("_type")) and v["_type"] == "_RefAccounting"


def is_cu(v):
    return is_obj(v) and is_str(v.get("_type")) and v["_type"] == "CanonicalUsage"


def truth(v):
    if v is MISSING or v is None:
        return False
    if is_cu(v):
        return True
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v != 0.0
    if isinstance(v, str):
        return len(v) > 0
    if isinstance(v, (list, dict)):
        return len(v) > 0
    return False


def type_name(v):
    if v is MISSING or v is None:
        return "NoneType"
    if isinstance(v, bool):
        return "bool"
    if isinstance(v, int):
        return "int"
    if isinstance(v, float):
        if v == int(v) and abs(v) < 9.2e18:
            return "int"
        return "float"
    if isinstance(v, str):
        return "str"
    if isinstance(v, list):
        return "list"
    if isinstance(v, dict):
        return "types.SimpleNamespace" if attr(v) else "dict"
    return "object"


def pystr(v):
    if v is MISSING or v is None:
        return "None"
    if isinstance(v, bool):
        return "True" if v else "False"
    if isinstance(v, str):
        return v
    if isinstance(v, int):
        return str(v)
    if isinstance(v, float):
        if v == int(v) and abs(v) < 9.2e18:
            return str(int(v))
        return repr(v)
    return json.dumps(v, ensure_ascii=True)


def pystr_or(v, default):
    if not truth(v):
        return default
    return pystr(v)


def obj_get(d, k):
    if not is_obj(d):
        return MISSING
    return d.get(k, MISSING)


def jcopy(v):
    return MISSING if v is MISSING else copy.deepcopy(v)


# ---- exception helpers ----

def raise_(exc, cls, msg):
    if exc[0] is None:
        exc[0] = {"__raise__": cls, "msg": msg}


def attr_err(exc, carrier, name):
    raise_(exc, "AttributeError",
           "'%s' object has no attribute '%s'" % (type_name(carrier), name))


def ns_attr_err(exc, name):
    raise_(exc, "AttributeError",
           "'types.SimpleNamespace' object has no attribute '%s'" % name)


def swallow(exc):
    if exc[0] is None:
        return True
    if not is_exception(exc[0].get("__raise__")):
        return False
    exc[0] = None
    return True


# ---- seam dispatcher (harness ml3_stub_call + ml3_call) ----

class Seam:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.counts = {}

    def script_next(self, name):
        io = self.inp.get("io") or {}
        s = io.get(name)
        if s is None:
            return None
        if isinstance(s, dict) and "seq" in s:
            seq = s["seq"]
            n = self.counts.get(name, 0)
            self.counts[name] = n + 1
            if isinstance(seq, list) and n < len(seq):
                return copy.deepcopy(seq[n])
            return None
        return copy.deepcopy(s)

    def call(self, name, args, exc):
        r = self.script_next(name)
        self.trace.append({"ev": name, "args": copy.deepcopy(args),
                           "ret": copy.deepcopy(r)})
        if isinstance(r, dict) and "__raise__" in r:
            if exc[0] is None:
                exc[0] = r
            return None
        return r


def arg_j(a, k, v):
    if v is not MISSING and v is not None:
        a[k] = copy.deepcopy(v)


def arg_s(a, k, v):
    if v is not None:
        a[k] = v


def arg_b(a, k, v):
    a[k] = v


def call_double(fn, io, args, exc):
    if is_callable(fn):
        return io.call(fn["__call__"], args, exc)
    raise_(exc, "TypeError", "'%s' object is not callable" % type_name(fn))
    return None


# ---- navigation helpers ----

def dict_get(d, key, exc):
    if not is_dict(d):
        attr_err(exc, d, "get")
        return MISSING
    return d.get(key, MISSING)


def dict_pop(d, key, exc):
    if not is_dict(d):
        attr_err(exc, d, "pop")
        return MISSING
    v = d.get(key, MISSING)
    out = jcopy(v)
    d.pop(key, None)
    return out


def subscript(d, key, exc):
    if is_dict(d):
        if key not in d:
            raise_(exc, "KeyError", "'%s'" % key)
            return MISSING
        return d[key]
    if is_arr(d):
        raise_(exc, "TypeError",
               "list indices must be integers or slices, not str")
        return MISSING
    if is_str(d):
        raise_(exc, "TypeError", "string indices must be integers, not 'str'")
        return MISSING
    raise_(exc, "TypeError",
           "'%s' object is not subscriptable" % type_name(d))
    return MISSING


def self_get(state, name, exc):
    if name not in state:
        ns_attr_err(exc, name)
        return MISSING
    return state[name]


def self_get_def(state, name):
    return state.get(name, MISSING)


def hasattr_(v, name):
    a = attr(v)
    return a is not None and name in a


def unpack(r, n, exc):
    if not is_arr(r):
        raise_(exc, "TypeError",
               "cannot unpack non-iterable %s object" % type_name(r))
        return None
    if len(r) > n:
        raise_(exc, "ValueError",
               "too many values to unpack (expected %d, got %d)" % (n, len(r)))
        return None
    if len(r) < n:
        raise_(exc, "ValueError",
               "not enough values to unpack (expected %d, got %d)" % (n, len(r)))
        return None
    return list(r)


def msg_copies(messages, exc):
    if not is_arr(messages):
        raise_(exc, "TypeError",
               "'%s' object is not iterable" % type_name(messages))
        return None
    out = []
    for m in messages:
        if not is_dict(m):
            raise_(exc, "TypeError",
                   "'%s' object is not iterable" % type_name(m))
            return None
        out.append(copy.deepcopy(m))
    return out


CU_FIELDS = ["input_tokens", "output_tokens", "cache_read_tokens",
             "cache_write_tokens", "reasoning_tokens", "request_count",
             "raw_usage", "cache_write_by_ttl"]
ACCT_FIELDS = ["usage", "cost_usd", "cost_status", "cost_source", "messages",
               "output", "model", "provider", "temperature"]


def cu_new():
    o = {f: 0 for f in CU_FIELDS[:5]}
    o["request_count"] = 1
    o["raw_usage"] = None
    o["cache_write_by_ttl"] = []
    return o


def cu_project(v):
    if not is_cu(v):
        return jcopy(v)
    o = {}
    for f in CU_FIELDS[:5]:
        fv = v.get(f, MISSING)
        o[f] = jcopy(fv) if fv is not MISSING else 0
    rc = v.get("request_count", MISSING)
    o["request_count"] = jcopy(rc) if rc is not MISSING else 1
    ru = v.get("raw_usage", MISSING)
    o["raw_usage"] = jcopy(ru) if ru is not MISSING else None
    ct = v.get("cache_write_by_ttl", MISSING)
    o["cache_write_by_ttl"] = jcopy(ct) if ct is not MISSING else []
    return o


def acct_fill(v):
    if not is_acct(v):
        return jcopy(v)
    o = {"_type": "_RefAccounting"}
    for f in ACCT_FIELDS:
        fv = v.get(f, MISSING)
        o[f] = jcopy(fv) if fv is not MISSING else None
    return o


def deep_eq(a, b):
    an, bn = is_null(a), is_null(b)
    if an or bn:
        return an and bn
    anum = is_num(a) or is_bool(a)
    bnum = is_num(b) or is_bool(b)
    if anum or bnum:
        if not anum or not bnum:
            return False
        av = (1.0 if a else 0.0) if is_bool(a) else a
        bv = (1.0 if b else 0.0) if is_bool(b) else b
        return av == bv
    if type(a) is not type(b):
        return False
    if isinstance(a, str):
        return a == b
    if isinstance(a, list):
        return len(a) == len(b) and all(deep_eq(x, y) for x, y in zip(a, b))
    if isinstance(a, dict):
        if len(a) != len(b):
            return False
        for k in a:
            if k not in b or not deep_eq(a[k], b[k]):
                return False
        return True
    return False


def json_cmp(a, b):
    an, bn = is_null(a), is_null(b)
    if an or bn:
        return (0 if an else 1) - (0 if bn else 1)
    anum = is_num(a) or is_bool(a)
    bnum = is_num(b) or is_bool(b)
    if anum and bnum:
        av = (1.0 if a else 0.0) if is_bool(a) else a
        bv = (1.0 if b else 0.0) if is_bool(b) else b
        return (av > bv) - (av < bv)
    if type(a) is not type(b):
        ra = {type(None): 0, bool: 1, int: 2, float: 2, str: 3, list: 4, dict: 5}
        return ra.get(type(a), 9) - ra.get(type(b), 9)
    if isinstance(a, str):
        return (a > b) - (a < b)
    if isinstance(a, list):
        for x, y in zip(a, b):
            c = json_cmp(x, y)
            if c:
                return c
        return (len(a) > len(b)) - (len(a) < len(b))
    sa = json.dumps(a, ensure_ascii=True)
    sb = json.dumps(b, ensure_ascii=True)
    return (sa > sb) - (sa < sb)


def arr_contains(arr, v):
    if not is_arr(arr):
        return False
    return any(deep_eq(x, v) for x in arr)


def set_add(arr, v):
    if not is_arr(arr) or arr_contains(arr, v):
        return
    arr.append(copy.deepcopy(v))
    i = len(arr) - 1
    while i > 0 and json_cmp(arr[i - 1], arr[i]) > 0:
        arr[i - 1], arr[i] = arr[i], arr[i - 1]
        i -= 1


def sort_array(arr):
    if not is_arr(arr) or len(arr) < 2:
        return
    for i in range(1, len(arr)):
        cur = arr[i]
        j = i
        while j > 0 and json_cmp(arr[j - 1], cur) > 0:
            arr[j] = arr[j - 1]
            j -= 1
        arr[j] = cur


FACADE_BINDS = [
    "__init__", "consume_reference_usage", "last_reference_metrics",
    "_record_late_reference_accounting", "consume_and_save_trace", "_emit",
    "prepare", "rebase_prepared_request", "create", "_call_prepared_aggregator",
    "_fold_pending_accounting", "_fanout_cache_key", "_run_fanout",
    "_build_guidance", "_plan_aggregator_cache",
]


def norm_facade(state):
    if not is_obj(state):
        return
    state.pop("_accounting_lock", None)
    for k in FACADE_BINDS:
        state.pop(k, None)
    pu = state.get("_pending_reference_usage", MISSING)
    if is_cu(pu):
        state["_pending_reference_usage"] = cu_project(pu)
    rco = state.get("_ref_cache_outputs", MISSING)
    if is_arr(rco):
        for row in rco:
            if not is_arr(row):
                continue
            for j in range(len(row)):
                if is_acct(row[j]):
                    row[j] = acct_fill(row[j])
    msrd = state.get("_merge_same_role_destinations", MISSING)
    if is_arr(msrd):
        sort_array(msrd)


# ---- 1. MoAChatCompletions.__init__ ----

def ml3_mcc_init(inp, io, state, exc):
    pn = inp.get("preset_name", MISSING)
    state["preset_name"] = jcopy(pn) if truth(pn) else "default"
    rc = inp.get("reference_callback", MISSING)
    state["reference_callback"] = jcopy(rc) if rc is not MISSING else None
    ag = inp.get("agent", MISSING)
    state["_agent"] = jcopy(ag) if ag is not MISSING else None
    state["_ref_cache_key"] = None
    state["_ref_cache_outputs"] = []
    state["_pending_reference_usage"] = cu_new()
    state["_pending_reference_cost"] = None
    state["last_aggregator_slot"] = None
    state["last_aggregator_route"] = None
    state["last_aggregator_attempt"] = None
    state["_pending_trace"] = None
    state["_last_reference_metrics"] = None
    state["_fanout_iteration_count"] = 0
    state["_fanout_turn_sig"] = None
    state["_fanout_last_state_sig"] = None
    state["_privacy_mode"] = ""
    state["_merge_same_role_destinations"] = []
    return None


# ---- 2. consume_reference_usage ----

def ml3_mcc_consume_reference_usage(inp, io, state, exc):
    pu = self_get(state, "_pending_reference_usage", exc)
    if exc[0]:
        return None
    pc = self_get(state, "_pending_reference_cost", exc)
    if exc[0]:
        return None
    usage = cu_project(pu) if truth(pu) else cu_new()
    cost = jcopy(pc)
    state["_pending_reference_usage"] = cu_new()
    state["_pending_reference_cost"] = None
    return [usage, cost]


# ---- 3. last_reference_metrics ----

def ml3_mcc_last_reference_metrics(inp, io, state, exc):
    m = self_get(state, "_last_reference_metrics", exc)
    if exc[0]:
        return None
    return jcopy(m)


# ---- 4. _record_late_reference_accounting ----

def ml3_mcc_record_late(inp, io, state, exc):
    acct = inp.get("accounting", MISSING)
    if not is_acct(acct):
        return None
    label = inp.get("label", MISSING)
    row = [label if is_str(label) else "", "", acct_fill(acct)]
    a = {}
    arg_j(a, "outputs", [row])
    s = io.call("_sum_reference_accounting", a, exc)
    if exc[0]:
        return None
    if not is_arr(s):
        raise_(exc, "TypeError",
               "moa_loop_flow3_extracted._fold_pending_accounting() "
               "argument after * must be an iterable, not %s" % type_name(s))
        return None
    n = len(s)
    if n == 0:
        raise_(exc, "TypeError",
               "_fold_pending_accounting() missing 2 required positional "
               "arguments: 'usage' and 'cost'")
        return None
    if n == 1:
        raise_(exc, "TypeError",
               "_fold_pending_accounting() missing 1 required positional "
               "argument: 'cost'")
        return None
    if n > 2:
        raise_(exc, "TypeError",
               "_fold_pending_accounting() takes 3 positional arguments "
               "but %d were given" % (n + 1))
        return None
    fa = {}
    arg_j(fa, "usage", s[0])
    arg_j(fa, "cost", s[1])
    io.call("_fold_pending_accounting", fa, exc)
    if exc[0]:
        return None
    return None


# ---- 5. consume_and_save_trace ----

def ml3_mcc_consume_and_save_trace(inp, io, state, exc):
    pending = self_get(state, "_pending_trace", exc)
    if exc[0]:
        return None
    state["_pending_trace"] = None
    if not truth(pending):
        return None
    if is_dict(pending):
        contains = "aggregator_input_messages" in pending
    elif is_arr(pending):
        contains = arr_contains(pending, "aggregator_input_messages")
    elif is_str(pending):
        contains = "aggregator_input_messages" in pending
    else:
        raise_(exc, "TypeError",
               "argument of type '%s' is not a container or iterable"
               % type_name(pending))
        return None
    if not contains:
        return None

    e2 = [None]
    agg_slot = None
    out_v = None
    sa = None
    asv = dict_get(pending, "aggregator_slot", e2)
    if e2[0]:
        return _caught(e2, exc, agg_slot, out_v, sa)
    agg_slot = jcopy(asv) if truth(asv) else {}
    ao = dict_get(pending, "aggregator_output", e2)
    if e2[0]:
        return _caught(e2, exc, agg_slot, out_v, sa)
    out_v = jcopy(ao) if ao is not MISSING else None
    fallback = inp.get("aggregator_output_fallback", MISSING)
    if is_null(out_v) and truth(fallback):
        out_v = jcopy(fallback)
    sa = {}
    arg_j(sa, "session_id", inp.get("session_id", MISSING))
    pv = dict_get(pending, "preset", e2)
    if e2[0]:
        return _caught(e2, exc, agg_slot, out_v, sa)
    if pv is not MISSING and pv is not None:
        arg_j(sa, "preset_name", pv)
    elif pv is MISSING:
        arg_s(sa, "preset_name", "")
    ro = dict_get(pending, "reference_outputs", e2)
    if e2[0]:
        return _caught(e2, exc, agg_slot, out_v, sa)
    if ro is not MISSING and ro is not None:
        arg_j(sa, "reference_outputs", ro)
    elif ro is MISSING:
        sa["reference_outputs"] = []
    al = dict_get(pending, "aggregator_label", e2)
    if e2[0]:
        return _caught(e2, exc, agg_slot, out_v, sa)
    if al is not MISSING and al is not None:
        arg_j(sa, "aggregator_label", al)
    elif al is MISSING:
        arg_s(sa, "aggregator_label", "")
    am = dict_get(agg_slot, "model", e2)
    if e2[0]:
        return _caught(e2, exc, agg_slot, out_v, sa)
    arg_j(sa, "aggregator_model", am)
    ap = dict_get(agg_slot, "provider", e2)
    if e2[0]:
        return _caught(e2, exc, agg_slot, out_v, sa)
    arg_j(sa, "aggregator_provider", ap)
    at = dict_get(pending, "aggregator_temperature", e2)
    if e2[0]:
        return _caught(e2, exc, agg_slot, out_v, sa)
    arg_j(sa, "aggregator_temperature", at)
    aim = dict_get(pending, "aggregator_input_messages", e2)
    if e2[0]:
        return _caught(e2, exc, agg_slot, out_v, sa)
    arg_j(sa, "aggregator_input_messages", aim)
    arg_j(sa, "aggregator_output", out_v)
    as_ = dict_get(pending, "aggregator_streamed", e2)
    if e2[0]:
        return _caught(e2, exc, agg_slot, out_v, sa)
    arg_b(sa, "aggregator_streamed", truth(as_))
    io.call("save_moa_turn", sa, e2)
    sa = None
    if e2[0]:
        return _caught(e2, exc, agg_slot, out_v, sa)
    return None


def _caught(e2, exc, agg_slot, out_v, sa):
    if not swallow(e2):
        exc[0] = e2[0]
        return None
    return None


# ---- 6. _emit ----

def ml3_mcc_emit(inp, io, state, exc):
    cb = self_get(state, "reference_callback", exc)
    if exc[0]:
        return None
    if is_null(cb):
        return None
    kwargs = inp.get("kwargs", MISSING)
    a = {}
    event = inp.get("event", MISSING)
    arg_s(a, "event", event if is_str(event) else "")
    a["kwargs"] = jcopy(kwargs) if is_dict(kwargs) else {}
    e2 = [None]
    call_double(cb, io, a, e2)
    if e2[0] and not swallow(e2):
        exc[0] = e2[0]
        return None
    return None


# ---- 7. prepare ----

def ml3_mcc_prepare(inp, io, state, exc):
    ak = {}
    msgs = inp.get("messages", MISSING)
    ak["messages"] = jcopy(msgs) if msgs is not MISSING else None
    ak["_moa_prepare_only"] = True
    return ml3_mcc_create_core(ak, io, state, exc)


# ---- 8. rebase_prepared_request ----

def ml3_mcc_rebase(inp, io, state, exc):
    prepared = inp.get("prepared", MISSING)
    guidance = dict_get(prepared, "guidance", exc)
    if exc[0]:
        return None
    messages = inp.get("messages", MISSING)
    agg = msg_copies(messages, exc)
    if exc[0]:
        return None
    if truth(guidance):
        gs = pystr(guidance)
        a = {}
        arg_j(a, "agg_messages", agg)
        arg_s(a, "guidance", gs)
        io.call("_attach_reference_guidance", a, exc)
        if exc[0]:
            return None
    ret = jcopy(prepared)
    ret["messages"] = agg
    return ret


# ---- 9. create ----

def ml3_mcc_create_core(ak, io, state, exc):
    pr = ak.get("_moa_prepared_request", MISSING)
    prep_copy = jcopy(pr) if pr is not MISSING else None
    ak.pop("_moa_prepared_request", None)
    if prep_copy is not None:
        if not is_dict(prep_copy):
            raise_(exc, "TypeError", "_moa_prepared_request must be a dict")
            return None
        return ml3_mcc_call_prepared_core(prep_copy, ak, io, state, exc)

    pn = self_get(state, "preset_name", exc)
    if exc[0]:
        return None
    a = {}
    arg_j(a, "preset_name", pn)
    res = io.call("_resolve_preset_cached", a, exc)
    if exc[0]:
        return None
    ri = unpack(res, 2, exc)
    if exc[0]:
        return None
    preset, moa_raw = ri
    a = {}
    arg_j(a, "moa_raw", moa_raw)
    pm = io.call("_moa_privacy_mode", a, exc)
    if exc[0]:
        return None
    state["_privacy_mode"] = pm if pm is not None else None

    mv = dict_get(ak, "messages", exc)
    if exc[0]:
        return None
    if not truth(mv):
        messages = []
    elif is_arr(mv):
        messages = jcopy(mv)
    else:
        raise_(exc, "TypeError", "'%s' object is not iterable" % type_name(mv))
        return None

    ref_models = []
    pen = dict_get(preset, "enabled", exc)
    if exc[0]:
        return None
    enabled = truth(pen) if pen is not MISSING else True
    if enabled:
        rm = dict_get(preset, "reference_models", exc)
        if exc[0]:
            return None
        if truth(rm):
            if not is_arr(rm):
                raise_(exc, "TypeError",
                       "'%s' object is not iterable" % type_name(rm))
                return None
            for slot in rm:
                en = dict_get(slot, "enabled", exc)
                if exc[0]:
                    return None
                keep = truth(en) if en is not MISSING else True
                if keep:
                    ref_models.append(jcopy(slot))

    agv = dict_get(preset, "aggregator", exc)
    if exc[0]:
        return None
    aggregator = jcopy(agv) if truth(agv) else {}
    if truth(aggregator):
        if not is_dict(aggregator):
            raise_(exc, "TypeError",
                   "'%s' object is not iterable" % type_name(aggregator))
            return None
        state["last_aggregator_slot"] = jcopy(aggregator)
    else:
        state["last_aggregator_slot"] = None

    a = {}
    arg_j(a, "preset", preset)
    arg_s(a, "key", "aggregator_temperature")
    at = io.call("_preset_temperature", a, exc)
    if exc[0]:
        return None
    if is_null(at):
        tv = dict_get(ak, "temperature", exc)
        if exc[0]:
            return None
        if tv is not MISSING and tv is not None:
            at = jcopy(tv)

    a = {}
    arg_j(a, "messages", messages)
    ref_messages = io.call("_reference_messages", a, exc)
    if exc[0]:
        return None

    a = {}
    arg_j(a, "preset", preset)
    arg_j(a, "ref_messages", ref_messages)
    arg_j(a, "reference_models", ref_models)
    cache_key = io.call("_fanout_cache_key", a, exc)
    if exc[0]:
        return None

    rck = self_get(state, "_ref_cache_key", exc)
    if exc[0]:
        return None
    rco = self_get(state, "_ref_cache_outputs", exc)
    if exc[0]:
        return None
    cache_hit = deep_eq(cache_key, rck) and truth(rco)
    if cache_hit:
        if not is_arr(rco):
            raise_(exc, "TypeError",
                   "'%s' object is not iterable" % type_name(rco))
            return None
        reference_outputs = jcopy(rco)
        state["_pending_trace"] = None
    else:
        a = {}
        arg_j(a, "preset", preset)
        arg_j(a, "ref_messages", ref_messages)
        arg_j(a, "reference_models", ref_models)
        arg_j(a, "aggregator", aggregator)
        arg_j(a, "aggregator_temperature", at)
        arg_j(a, "cache_key", cache_key)
        reference_outputs = io.call("_run_fanout", a, exc)
        if exc[0]:
            return None

    agg_messages = msg_copies(messages, exc)
    if exc[0]:
        return None
    dp = dict_get(preset, "degraded_reference_policy", exc)
    if exc[0]:
        return None
    ds = pystr_or(dp, "loud")
    stale = False
    if cache_hit:
        a = {}
        arg_j(a, "messages", messages)
        tar = io.call("_tool_activity_since_last_user", a, exc)
        if exc[0]:
            return None
        stale = truth(tar)
    a = {}
    arg_j(a, "reference_outputs", reference_outputs)
    arg_j(a, "aggregator", aggregator)
    arg_s(a, "degraded_reference_policy", ds)
    arg_b(a, "stale", stale)
    guidance = io.call("_build_guidance", a, exc)
    if exc[0]:
        return None
    if truth(guidance):
        a = {}
        arg_j(a, "agg_messages", agg_messages)
        arg_j(a, "guidance", guidance)
        io.call("_attach_reference_guidance", a, exc)
        if exc[0]:
            return None
    prepared = {
        "messages": jcopy(agg_messages),
        "guidance": jcopy(guidance) if guidance is not None else None,
        "aggregator": jcopy(aggregator),
        "aggregator_temperature": jcopy(at) if at is not None else None,
    }
    po = ak.get("_moa_prepare_only", MISSING)
    prepare_only = truth(po)
    ak.pop("_moa_prepare_only", None)
    if prepare_only:
        return prepared
    return ml3_mcc_call_prepared_core(prepared, ak, io, state, exc)


def ml3_mcc_create(inp, io, state, exc):
    akj = inp.get("api_kwargs", MISSING)
    ak = jcopy(akj) if is_dict(akj) else {}
    return ml3_mcc_create_core(ak, io, state, exc)


# ---- 10. _call_prepared_aggregator ----

def send_args(send_kwargs, msgs):
    a = jcopy(send_kwargs)
    a["messages"] = jcopy(msgs)
    return {k: v for k, v in a.items() if v is not None}


def trace_set(trace, key, val, exc):
    if is_arr(trace):
        raise_(exc, "TypeError",
               "list indices must be integers or slices, not str")
        return
    if not is_dict(trace):
        raise_(exc, "TypeError",
               "'%s' object does not support item assignment" % type_name(trace))
        return
    trace[key] = val


def ml3_mcc_call_prepared_core(prepared, ak, io, state, exc):
    aggregator = subscript(prepared, "aggregator", exc)
    if exc[0]:
        return None
    prov = dict_get(aggregator, "provider", exc)
    if exc[0]:
        return None
    if is_str(prov) and prov == "moa":
        raise_(exc, "RuntimeError",
               "MoA aggregator cannot be another MoA preset")
        return None
    a = {}
    arg_j(a, "slot", aggregator)
    agg_runtime = io.call("_slot_runtime", a, exc)
    if exc[0]:
        return None

    pm = subscript(prepared, "messages", exc)
    if exc[0]:
        return None
    tl = dict_get(ak, "tools", exc)
    if exc[0]:
        return None
    gd = dict_get(prepared, "guidance", exc)
    if exc[0]:
        return None
    a = {}
    arg_j(a, "agg_messages", pm)
    arg_j(a, "tools", tl)
    arg_j(a, "guidance", gd)
    arg_j(a, "agg_runtime", agg_runtime)
    plan = io.call("_plan_aggregator_cache", a, exc)
    if exc[0]:
        return None
    pi = unpack(plan, 2, exc)
    if exc[0]:
        return None
    agg_messages = jcopy(pi[0]) if pi[0] is not None else None
    tools = jcopy(pi[1]) if pi[1] is not None else None

    route = {}
    for k in ("provider", "model", "base_url", "api_mode"):
        v = dict_get(agg_runtime, k, exc)
        if exc[0]:
            return None
        route[k] = jcopy(v) if v is not MISSING else None
    state["last_aggregator_route"] = route

    trace = self_get(state, "_pending_trace", exc)
    if exc[0]:
        return None
    if not is_null(trace):
        a = {}
        arg_j(a, "slot", aggregator)
        lbl = io.call("_slot_label", a, exc)
        if exc[0]:
            return None
        trace_set(trace, "aggregator_label", lbl if lbl is not None else None, exc)
        if exc[0]:
            return None

    sv = dict_get(ak, "stream", exc)
    if exc[0]:
        return None
    stream = truth(sv)
    stream_kwargs = {}
    if stream:
        stream_kwargs["stream"] = True
        so = dict_get(ak, "stream_options", exc)
        if exc[0]:
            return None
        if truth(so):
            stream_kwargs["stream_options"] = jcopy(so)
        else:
            stream_kwargs["stream_options"] = {"include_usage": True}
        to = dict_get(ak, "timeout", exc)
        if exc[0]:
            return None
        if to is not MISSING and to is not None:
            stream_kwargs["timeout"] = jcopy(to)

    eb = dict_pop(agg_runtime, "extra_body", exc)
    if exc[0]:
        return None
    ceb = dict_get(ak, "extra_body", exc)
    if exc[0]:
        return None
    a = {}
    arg_j(a, "slot_extra_body", eb)
    arg_j(a, "caller_extra_body", ceb)
    agg_extra_body = io.call("_merge_slot_extra_body", a, exc)
    if exc[0]:
        return None

    a = {}
    arg_j(a, "runtime", agg_runtime)
    destination = io.call("destination_key", a, exc)
    if exc[0]:
        return None

    remembered = self_get_def(state, "_merge_same_role_destinations")
    if is_null(remembered):
        remembered = []
        state["_merge_same_role_destinations"] = remembered
    bad = None
    outer = None
    if is_dict(destination):
        bad = destination
        outer = "dict"
    elif is_arr(destination):
        outer = "tuple"
        for el in destination:
            if is_arr(el) or is_dict(el):
                bad = el
                break
    if bad is not None:
        raise_(exc, "TypeError",
               "cannot use '%s' as a set element (unhashable type: '%s')"
               % (outer, type_name(bad)))
        return None
    merged = arr_contains(remembered, destination)
    if merged:
        a = {}
        arg_j(a, "messages", agg_messages)
        mm = io.call("merge_same_role_messages", a, exc)
        if exc[0]:
            return None
        if mm is not None and is_obj(mm) and "__echo__" in mm:
            pass   # identity: agg_messages unchanged
        else:
            agg_messages = mm if mm is not None else None

    at = subscript(prepared, "aggregator_temperature", exc)
    if exc[0]:
        return None
    mt = dict_get(ak, "max_tokens", exc)
    if exc[0]:
        return None
    a = {}
    arg_j(a, "aggregator", aggregator)
    reasoning = io.call("_aggregator_reasoning_config", a, exc)
    if exc[0]:
        return None
    send_kwargs = {
        "task": "moa_aggregator",
        "temperature": jcopy(at) if at is not MISSING else None,
        "max_tokens": jcopy(mt) if mt is not MISSING else None,
        "tools": jcopy(tools),
        "extra_body": jcopy(agg_extra_body) if agg_extra_body is not None else None,
        "reasoning_config": reasoning if reasoning is not None else None,
    }
    for k in stream_kwargs:
        if k in send_kwargs:
            raise_(exc, "TypeError",
                   "functools.partial() got multiple values for keyword "
                   "argument '%s'" % k)
            return None
        send_kwargs[k] = jcopy(stream_kwargs[k])
    for k in agg_runtime:
        if k in send_kwargs:
            raise_(exc, "TypeError",
                   "functools.partial() got multiple values for keyword "
                   "argument '%s'" % k)
            return None
        send_kwargs[k] = jcopy(agg_runtime[k])

    p = {"messages": jcopy(agg_messages), "tools": jcopy(tools),
         "extra_body": jcopy(agg_extra_body) if agg_extra_body is not None else None}
    a = {}
    arg_j(a, "payload", p)
    attempt = io.call("attempt_facts", a, exc)
    if exc[0]:
        return None
    state["last_aggregator_attempt"] = attempt if attempt is not None else None

    agg_response = io.call("call_llm", send_args(send_kwargs, agg_messages), exc)
    if exc[0]:
        if not is_exception(exc[0].get("__raise__")):
            return None
        orig = exc[0]
        exc[0] = None
        retry = None
        echo_same = False
        if not merged:
            a = {}
            arg_j(a, "messages", agg_messages)
            retry = io.call("merge_same_role_messages", a, exc)
            if exc[0]:
                return None
            if retry is not None and is_obj(retry) and "__echo__" in retry:
                echo_same = True
        rethrow = (retry is None or echo_same)
        if not rethrow:
            a = {}
            arg_s(a, "exc", orig.get("msg", ""))
            arg_j(a, "runtime", agg_runtime)
            rej = io.call("is_role_alternation_rejection", a, exc)
            if exc[0]:
                return None
            rethrow = not truth(rej)
        if rethrow:
            exc[0] = orig
            return None
        set_add(remembered, destination)
        a = {}
        arg_j(a, "slot", aggregator)
        io.call("_slot_label", a, exc)
        if exc[0]:
            return None
        agg_messages = retry
        p = {"messages": jcopy(agg_messages), "tools": jcopy(tools),
             "extra_body": jcopy(agg_extra_body) if agg_extra_body is not None else None}
        a = {}
        arg_j(a, "payload", p)
        attempt = io.call("attempt_facts", a, exc)
        if exc[0]:
            return None
        state["last_aggregator_attempt"] = attempt if attempt is not None else None
        agg_response = io.call("call_llm", send_args(send_kwargs, agg_messages), exc)
        if exc[0]:
            return None

    if not is_null(trace):
        pmode = self_get_def(state, "_privacy_mode")
        if truth(pmode):
            copies = msg_copies(agg_messages, exc)
            if exc[0]:
                return None
            a = {}
            arg_j(a, "messages", copies)
            aim = io.call("_redact_trace_messages", a, exc)
            if exc[0]:
                return None
            if aim is None:
                aim = None
        else:
            aim = jcopy(agg_messages)
        trace_set(trace, "aggregator_input_messages", aim, exc)
        if exc[0]:
            return None
        trace["aggregator_streamed"] = stream
        output = None
        if not stream:
            e3 = [None]
            a = {}
            arg_j(a, "response", agg_response)
            et = io.call("_extract_text", a, e3)
            if e3[0]:
                if not swallow(e3):
                    exc[0] = e3[0]
                    return None
            else:
                output = et if et is not None else None
        trace["aggregator_output"] = output

    if stream and hasattr_(agg_response, "choices"):
        a = {}
        arg_j(a, "response", agg_response)
        chunk = io.call("_completed_response_as_stream_chunk", a, exc)
        if exc[0]:
            return None
        return {"__iter__": [chunk if chunk is not None else None]}
    return agg_response if agg_response is not None else None


def ml3_mcc_call_prepared(inp, io, state, exc):
    prepared = inp.get("prepared", MISSING)
    akj = inp.get("api_kwargs", MISSING)
    ak = akj
    if is_null(ak):
        ak = {}
    return ml3_mcc_call_prepared_core(prepared, ak, io, state, exc)


# ---- 11-15. MoAClient ----

def ml3_client_completions(state, exc):
    chat = state.get("chat", MISSING)
    if chat is MISSING:
        ns_attr_err(exc, "chat")
        return MISSING
    if is_null(chat):
        raise_(exc, "AttributeError",
               "'NoneType' object has no attribute 'completions'")
        return MISSING
    if not is_obj(chat):
        attr_err(exc, chat, "completions")
        return MISSING
    comp = chat.get("completions", MISSING)
    if comp is MISSING:
        ns_attr_err(exc, "completions")
        return MISSING
    return comp


def ml3_client_method_carrier(comp, name, exc):
    if is_null(comp) or not is_obj(comp):
        attr_err(exc, comp, name)
        return None
    return comp


def ml3_client_init(inp, io, state, exc):
    comp = {}
    ml3_mcc_init(inp, io, comp, exc)
    if exc[0]:
        return None
    state["chat"] = {"completions": comp}
    return None


def ml3_client_consume_reference_usage(inp, io, state, exc):
    comp = ml3_client_completions(state, exc)
    if exc[0]:
        return None
    carrier = ml3_client_method_carrier(comp, "consume_reference_usage", exc)
    if exc[0]:
        return None
    return ml3_mcc_consume_reference_usage(inp, io, carrier, exc)


def ml3_client_last_aggregator_slot(inp, io, state, exc):
    comp = ml3_client_completions(state, exc)
    if exc[0]:
        return None
    if is_null(comp) or not is_obj(comp):
        return None
    v = comp.get("last_aggregator_slot", MISSING)
    return jcopy(v) if v is not MISSING else None


def ml3_client_consume_and_save_trace(inp, io, state, exc):
    comp = ml3_client_completions(state, exc)
    if exc[0]:
        return None
    carrier = ml3_client_method_carrier(comp, "consume_and_save_trace", exc)
    if exc[0]:
        return None
    return ml3_mcc_consume_and_save_trace(inp, io, carrier, exc)


def ml3_client_last_reference_metrics(inp, io, state, exc):
    comp = ml3_client_completions(state, exc)
    if exc[0]:
        return None
    carrier = ml3_client_method_carrier(comp, "last_reference_metrics", exc)
    if exc[0]:
        return None
    return ml3_mcc_last_reference_metrics(inp, io, carrier, exc)


DISPATCH = {
    "MoAChatCompletions.__init__": ml3_mcc_init,
    "MoAChatCompletions.consume_reference_usage": ml3_mcc_consume_reference_usage,
    "MoAChatCompletions.last_reference_metrics": ml3_mcc_last_reference_metrics,
    "MoAChatCompletions._record_late_reference_accounting": ml3_mcc_record_late,
    "MoAChatCompletions.consume_and_save_trace": ml3_mcc_consume_and_save_trace,
    "MoAChatCompletions._emit": ml3_mcc_emit,
    "MoAChatCompletions.prepare": ml3_mcc_prepare,
    "MoAChatCompletions.rebase_prepared_request": ml3_mcc_rebase,
    "MoAChatCompletions.create": ml3_mcc_create,
    "MoAChatCompletions._call_prepared_aggregator": ml3_mcc_call_prepared,
    "MoAClient.__init__": ml3_client_init,
    "MoAClient.consume_reference_usage": ml3_client_consume_reference_usage,
    "MoAClient.last_aggregator_slot": ml3_client_last_aggregator_slot,
    "MoAClient.consume_and_save_trace": ml3_client_consume_and_save_trace,
    "MoAClient.last_reference_metrics": ml3_client_last_reference_metrics,
}


def mirror_run(func, inp):
    """The harness run_case: fresh state for __init__, normalize, envelope."""
    io = Seam(inp)
    if func in ("MoAChatCompletions.__init__", "MoAClient.__init__"):
        state = {}
    else:
        sj = inp.get("state")
        state = copy.deepcopy(sj) if isinstance(sj, dict) else {}
    exc = [None]
    fn = DISPATCH.get(func)
    ret = fn(inp, io, state, exc) if fn else None
    if func.startswith("MoAClient."):
        chat = state.get("chat", MISSING)
        if is_obj(chat):
            comp = chat.get("completions", MISSING)
            if is_obj(comp) and "__attr__" not in comp:
                norm_facade(comp)
    else:
        norm_facade(state)
    return {"ret": ret, "raise": exc[0], "trace": io.trace, "state": state}


def load_oracle():
    spec = importlib.util.spec_from_file_location("oml3_shadow", ORACLE)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["oml3_shadow"] = mod
    spec.loader.exec_module(mod)
    return mod


def live_envelope(oml3, func, inp):
    oml3.setup_case(inp)
    try:
        ret, state = oml3.drive(func, inp)
        want = {"ret": oml3.proj_ret(ret), "raise": None,
                "state": oml3.norm_ret(state)}
    except BaseException as e:
        st = oml3.CUR["state_out"]
        if st is None:
            st = inp.get("state") or {}
        want = {"ret": None, "raise": oml3.exc_spec(e),
                "state": oml3.norm_ret(st)}
    want["trace"] = oml3.CUR["trace"]
    return want


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "both"
    cases = [json.loads(l) for l in open(FIX, encoding="utf-8") if l.strip()]
    rc = 0
    if mode in ("diff", "both"):
        oml3 = load_oracle()
        oml3.extract()
        oml3.install_module_stubs()
        mism = 0
        for c in cases:
            func, inp = c["func"], c["in"]
            got = mirror_run(func, inp)
            want = live_envelope(oml3, func, inp)
            if oml3.canon(got) != oml3.canon(want):
                mism += 1
                print("SHADOW MISMATCH %s" % func)
                print("  MIRROR: %s" % json.dumps(got, ensure_ascii=False, default=str))
                print("  LIVE  : %s" % json.dumps(want, ensure_ascii=False, default=str))
        print("SHADOW moa_loop_flow3 diff: %d cases, %d mismatches"
              % (len(cases), mism))
        rc |= 1 if mism else 0
    if mode in ("proxy", "both"):
        env = []
        for c in cases:
            out = mirror_run(c["func"], c["in"])
            env.append(json.dumps({"func": c["func"], "in": c["in"], "out": out},
                                  ensure_ascii=True))
        p = subprocess.run([PYBIN, str(ORACLE)], input="\n".join(env) + "\n",
                           capture_output=True, text=True, encoding="utf-8")
        sys.stdout.write(p.stdout)
        if p.stderr:
            sys.stderr.write(p.stderr)
        print("SHADOW proxy rc=%d" % p.returncode)
        rc |= 1 if p.returncode else 0
    return rc


if __name__ == "__main__":
    sys.exit(main())
