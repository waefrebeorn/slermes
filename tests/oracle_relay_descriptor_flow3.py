#!/usr/bin/env python3
"""Oracle for relay_descriptor_flow3.c — standalone FLOW differential check of
the gateway/relay/descriptor.py remainder (the frozen CapabilityDescriptor).

The CRLF-aware PoP grep shows the three CONCRETE methods (from_json,
supports_op, to_json) are claimed by the base port relay_descriptor.c; there is
no relay_descriptor_flow2 file. The genuinely unclaimed remainder is the
frozen-dataclass generated surface (__init__, __repr__, __eq__). Per
flow2-remainder-scoping.md this standalone oracle ports the WHOLE module with
qualified PoPs on the unclaimed generated methods and RE-DERIVES the claimed
siblings inside the C TU for the differential check; older claims stay in force.

Drives the LIVE functions with SCRIPTED I/O: the CapabilityDescriptor ClassDef
is AST-extracted from the quarry and exec'd as a real frozen dataclass, and the
one foreign effect (int() inside from_json's try/except (TypeError, ValueError))
is replaced by a recording stub returning the SAME scripted result/raise the C
harness feeds its universal `int_cast` seam.

Full builtin exception hierarchy: except (TypeError, ValueError) swallows
TypeError/ValueError and subclasses (UnicodeError, ...) but passes every other
Exception and the BaseException trio (SystemExit/KeyboardInterrupt/GeneratorExit).

Input (stdin, from t_port_relay_descriptor_flow3):
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...]}}
Summary: ORACLE relay_descriptor_flow3: N cases, M mismatches. Exit 2 on seed
failure, 1 on mismatch. `--self-test` runs the literal-C shadow mirror against
the live drive over the fixture plus a gate proxy (both must be 0).
"""
import ast
import copy
import json
import os
import sys
import types
from dataclasses import asdict, dataclass, field

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("HOME", os.path.dirname(_REPO))
os.environ.setdefault("USERPROFILE", os.path.dirname(_REPO))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "relay", "descriptor.py")
FIXTURE = os.path.join(_REPO, "tests", "fixtures", "relay_descriptor_flow3.jsonl")

# harness func -> live attribute on the extracted class
PY = {
    "init": "__init__",
    "supports_op": "supports_op",
    "to_json": "to_json",
    "from_json": "from_json",
    "repr": "__repr__",
    "eq": "__eq__",
}

CUR = {"trace": [], "scripts": {}, "idx": {}}

BASE_EXC = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}
BUILTIN_EXC = {
    "Exception": Exception, "BaseException": BaseException,
    "RuntimeError": RuntimeError, "ValueError": ValueError, "TypeError": TypeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "NameError": NameError,
    "LookupError": LookupError, "IndexError": IndexError,
    "StopIteration": StopIteration, "StopAsyncIteration": StopAsyncIteration,
    "OSError": OSError, "FileNotFoundError": FileNotFoundError,
    "PermissionError": PermissionError, "NotADirectoryError": NotADirectoryError,
    "IsADirectoryError": IsADirectoryError, "FileExistsError": FileExistsError,
    "TimeoutError": TimeoutError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "AssertionError": AssertionError,
    "ArithmeticError": ArithmeticError, "ZeroDivisionError": ZeroDivisionError,
    "OverflowError": OverflowError, "FloatingPointError": FloatingPointError,
    "UnicodeError": UnicodeError, "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeEncodeError": UnicodeEncodeError, "UnicodeTranslateError": UnicodeTranslateError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "MemoryError": MemoryError, "BufferError": BufferError, "EOFError": EOFError,
    "ReferenceError": ReferenceError, "SystemError": SystemError,
    "SyntaxError": SyntaxError, "IndentationError": IndentationError,
    "TabError": TabError, "UnboundLocalError": UnboundLocalError,
    "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "BlockingIOError": BlockingIOError,
    "ChildProcessError": ChildProcessError, "ProcessLookupError": ProcessLookupError,
    "InterruptedError": InterruptedError,
    "Warning": Warning, "UserWarning": UserWarning,
    "DeprecationWarning": DeprecationWarning, "PendingDeprecationWarning": PendingDeprecationWarning,
    "SyntaxWarning": SyntaxWarning, "RuntimeWarning": RuntimeWarning,
    "FutureWarning": FutureWarning, "ImportWarning": ImportWarning,
    "UnicodeWarning": UnicodeWarning, "BytesWarning": BytesWarning,
    "ResourceWarning": ResourceWarning, "EncodingWarning": EncodingWarning,
    "JSONDecodeError": json.JSONDecodeError,
}

KNOWN = ("contract_version", "platform", "label", "max_message_length",
         "supports_draft_streaming", "supports_edit", "supports_threads",
         "markdown_dialect", "len_unit", "emoji", "platform_hint", "pii_safe",
         "supports_context", "supports_inchannel_continuable",
         "supports_block_formatting", "supported_ops")

_SENTINEL = object()


def make_exc(kind, msg):
    cls = BASE_EXC.get(kind) or BUILTIN_EXC.get(kind)
    if cls is None:
        cls = type(str(kind), (Exception,), {})
    if cls is json.JSONDecodeError:
        return cls(msg, "", 0)
    if cls is UnicodeDecodeError:
        return UnicodeDecodeError("utf-8", b"", 0, 1, msg)
    return cls(msg)


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r["__raise__"], r.get("msg", ""))
    return r


def seam(name, args):
    args = {k: v for k, v in args.items()}
    scripts = CUR["scripts"]
    if name not in scripts:
        r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
            if not sc:
                r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
            else:
                i = CUR["idx"].get(name, 0)
                CUR["idx"][name] = i + 1
                r = sc[min(i, len(sc) - 1)]
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return maybe_raise(r)


def norm(o):
    if o is None or isinstance(o, (bool, int, str, float)):
        return o
    if isinstance(o, BaseException):
        return str(o)
    if is_dataclass_obj(o):
        return norm(asdict(o))
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if isinstance(o, (set, frozenset)):
        return sorted(norm(v) for v in o)
    return str(o)


def is_dataclass_obj(o):
    return hasattr(type(o), "__dataclass_fields__")


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False, default=str))


# ── extraction ──
NS = None
CD = None


def extract():
    global NS, CD
    mod = types.ModuleType("relay.descriptor")
    ns = mod.__dict__
    ns["__name__"] = "relay.descriptor"
    sys.modules["relay.descriptor"] = mod
    relay = types.ModuleType("relay")
    relay.__path__ = []
    relay.descriptor = mod
    sys.modules["relay"] = relay

    ns["dataclass"] = dataclass
    ns["field"] = field
    ns["asdict"] = asdict
    ns["json"] = json

    tree = ast.parse(open(SRC, encoding="utf-8").read())
    cls = next((n for n in tree.body if isinstance(n, ast.ClassDef)
                and n.name == "CapabilityDescriptor"), None)
    if cls is None:
        print("FATAL CapabilityDescriptor ClassDef not found")
        sys.exit(2)
    # Annotations are builtin names (int/str/bool/tuple) evaluated at exec time;
    # no typing imports are needed. Keep the decorator and the real field defaults.
    exec(compile(ast.Module(body=[cls], type_ignores=[]), SRC, "exec"), ns)
    CD = ns.get("CapabilityDescriptor")
    if CD is None:
        print("FATAL CapabilityDescriptor exec failed")
        sys.exit(2)
    NS = ns
    return ns


def install_stubs(ns):
    ns["int"] = lambda v: seam("int_cast", {"value": v})


REAL = {}


def restore():
    for k, v in REAL.items():
        NS[k] = v


def drive(ns, func, inp):
    if func == "init":
        return asdict(CD(**inp["fields"]))
    if func == "supports_op":
        return CD(**inp["fields"]).supports_op(inp["op"])
    if func == "to_json":
        return CD(**inp["fields"]).to_json()
    if func == "from_json":
        return asdict(CD.from_json(inp["data"]))
    if func == "repr":
        return repr(CD(**inp["fields"]))
    if func == "eq":
        return CD(**inp["a"]) == CD(**inp["b"])
    raise ValueError("unknown func: " + str(func))


def compute_live(rec):
    func = rec.get("func")
    inp = rec.get("in") or {}
    CUR["trace"] = []
    CUR["scripts"] = inp.get("seams") or {}
    CUR["idx"] = {}
    restore()
    install_stubs(NS)
    try:
        ret = drive(NS, func, inp)
    except BaseException as ex:
        ret = {"__raise__": type(ex).__name__, "msg": str(ex)}
    return {"ret": norm(ret), "trace": CUR["trace"]}


def check(rec):
    want = compute_live(rec)
    got = rec.get("out") or {}
    return (canon(want) != canon(got)), want, got


# ── literal-C shadow mirror (transcribes relay_descriptor_flow3.c) ──
SHAD = {"trace": [], "scripts": {}, "idx": {}}


def s_type_name(v):
    if v is None: return "NoneType"
    if isinstance(v, bool): return "bool"
    if isinstance(v, int): return "int"
    if isinstance(v, float): return "int" if float(v).is_integer() else "float"
    if isinstance(v, str): return "str"
    if isinstance(v, list): return "list"
    if isinstance(v, dict): return "dict"
    return "object"


def s_exc_parent(c):
    return {
        "BaseException": None,
        "SystemExit": "BaseException", "KeyboardInterrupt": "BaseException",
        "GeneratorExit": "BaseException", "Exception": "BaseException",
        "RuntimeError": "Exception", "NotImplementedError": "RuntimeError",
        "RecursionError": "RuntimeError", "ValueError": "Exception", "TypeError": "Exception",
        "AttributeError": "Exception", "NameError": "Exception", "UnboundLocalError": "NameError",
        "LookupError": "Exception", "IndexError": "LookupError", "KeyError": "LookupError",
        "ArithmeticError": "Exception", "ZeroDivisionError": "ArithmeticError",
        "OverflowError": "ArithmeticError", "FloatingPointError": "ArithmeticError",
        "OSError": "Exception", "FileNotFoundError": "OSError", "PermissionError": "OSError",
        "IsADirectoryError": "OSError", "NotADirectoryError": "OSError",
        "FileExistsError": "OSError", "InterruptedError": "OSError",
        "BlockingIOError": "OSError", "ChildProcessError": "OSError",
        "ProcessLookupError": "OSError", "TimeoutError": "OSError",
        "ConnectionError": "OSError", "BrokenPipeError": "ConnectionError",
        "ConnectionAbortedError": "ConnectionError", "ConnectionRefusedError": "ConnectionError",
        "ConnectionResetError": "ConnectionError",
        "JSONDecodeError": "ValueError", "UnicodeError": "ValueError",
        "UnicodeDecodeError": "UnicodeError", "UnicodeEncodeError": "UnicodeError",
        "UnicodeTranslateError": "UnicodeError",
        "StopIteration": "Exception", "StopAsyncIteration": "Exception",
        "AssertionError": "Exception", "SystemError": "Exception",
        "ReferenceError": "Exception", "MemoryError": "Exception",
        "BufferError": "Exception", "EOFError": "Exception",
        "ImportError": "Exception", "ModuleNotFoundError": "ImportError",
        "SyntaxError": "Exception", "IndentationError": "SyntaxError",
        "TabError": "IndentationError", "Warning": "Exception", "UserWarning": "Warning",
        "DeprecationWarning": "Warning", "PendingDeprecationWarning": "Warning",
        "SyntaxWarning": "Warning", "RuntimeWarning": "Warning", "FutureWarning": "Warning",
        "ImportWarning": "Warning", "UnicodeWarning": "Warning", "BytesWarning": "Warning",
        "ResourceWarning": "Warning", "EncodingWarning": "Warning",
    }.get(c)


def s_exc_matches(spec, classes):
    cls = spec.get("__raise__") if isinstance(spec, dict) else None
    if not cls: return False
    for _ in range(32):
        if cls in classes: return True
        cls = s_exc_parent(cls)
        if not cls: break
    return False


def s_call(name, args):
    sc = SHAD["scripts"].get(name)
    if isinstance(sc, list):
        if not sc:
            r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
        else:
            i = SHAD["idx"].get(name, 0)
            SHAD["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)]
    elif sc is not None:
        r = sc
    else:
        r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
    cp = copy.deepcopy(r)
    SHAD["trace"].append({"ev": name, "args": copy.deepcopy(args), "ret": cp})
    return copy.deepcopy(r)


def s_defaults():
    return {"emoji": "🔌", "platform_hint": "", "pii_safe": False,
            "supports_context": False, "supports_inchannel_continuable": False,
            "supports_block_formatting": False, "supported_ops": []}


def s_init(fields):
    d = s_defaults()
    if isinstance(fields, dict):
        for k, v in fields.items():
            if k in KNOWN:
                d[k] = copy.deepcopy(v)
    return d


def s_supports_op(desc, op):
    if op is None: return False
    ops = desc.get("supported_ops")
    if isinstance(ops, list) and ops:
        return any(isinstance(x, str) and x == op for x in ops)
    return op in ("send", "edit", "typing", "follow_up")


def s_dumps(v):
    # Mirrors rd3_dumps_rec: Python's json.dumps with ', ' / ': ' separators.
    return json.dumps(v, sort_keys=True, ensure_ascii=False, separators=(", ", ": "))


def s_pystr(s):
    # Mirrors rd3_pystr (single-quoted repr with escapes; raw non-ASCII).
    out = ["'"]
    for ch in s or "":
        o = ord(ch)
        if ch in ("'", "\\"):
            out.append("\\" + ch)
        elif ch == "\n": out.append("\\n")
        elif ch == "\r": out.append("\\r")
        elif ch == "\t": out.append("\\t")
        elif o < 0x20 or o == 0x7f: out.append("\\x%02x" % o)
        else: out.append(ch)
    out.append("'")
    return "".join(out)


def s_pyval(v):
    if v is None: return "None"
    if isinstance(v, bool): return "True" if v else "False"
    if isinstance(v, str): return s_pystr(v)
    if isinstance(v, float) and v.is_integer(): return "%.0f" % v
    return str(v)


def s_repr(desc, ops_tuple):
    parts = []
    for k in KNOWN:
        if k == "supported_ops":
            arr = desc.get("supported_ops") or []
            n = len(arr)
            inner = ", ".join(s_pystr(x) for x in arr)
            if ops_tuple and n == 1:
                inner += ","
            parts.append(k + "=" + ("(" if ops_tuple else "[") + inner + (")" if ops_tuple else "]"))
        else:
            parts.append(k + "=" + s_pyval(desc.get(k)))
    return "CapabilityDescriptor(" + ", ".join(parts) + ")"


def s_eq(a, b):
    return canon(a) == canon(b)


def s_from_json(data):
    raw = json.loads(data)
    if not isinstance(raw, dict):
        return None, {"__raise__": "AttributeError",
                      "msg": "'%s' object has no attribute 'items'" % s_type_name(raw)}
    filtered = {k: copy.deepcopy(v) for k, v in raw.items() if k in KNOWN}
    if "max_message_length" in filtered:
        r = s_call("int_cast", {"value": copy.deepcopy(filtered["max_message_length"])})
        if isinstance(r, dict) and "__raise__" in r:
            if s_exc_matches(r, ("TypeError", "ValueError")):
                filtered["max_message_length"] = 4096
            else:
                return None, r
        else:
            nv = r if isinstance(r, (int, float)) and not isinstance(r, bool) else 0
            if nv <= 0:
                filtered["max_message_length"] = 4096
    if "supported_ops" in filtered:
        so = filtered["supported_ops"]
        filtered["supported_ops"] = [x for x in so if isinstance(x, str) and x] if isinstance(so, list) else []
    return s_init(filtered), None


def shadow_run(func, inp):
    SHAD["trace"] = []
    SHAD["scripts"] = inp.get("seams") or {}
    SHAD["idx"] = {}
    pending = None
    ret = None
    if func == "init":
        ret = s_init(inp.get("fields"))
    elif func == "supports_op":
        ret = s_supports_op(s_init(inp.get("fields")), inp.get("op"))
    elif func == "to_json":
        ret = s_dumps(s_init(inp.get("fields")))
    elif func == "from_json":
        ret, pending = s_from_json(inp.get("data", ""))
    elif func == "repr":
        f = inp.get("fields")
        ops_tuple = not (isinstance(f, dict) and "supported_ops" in f)
        ret = s_repr(s_init(f), ops_tuple)
    elif func == "eq":
        ret = s_eq(s_init(inp.get("a")), s_init(inp.get("b")))
    else:
        ret = None
    out = {}
    out["ret"] = pending if pending is not None else (ret if ret is not None else None)
    out["trace"] = SHAD["trace"]
    return out


def self_test():
    recs = []
    with open(FIXTURE, encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if line.startswith("{"):
                recs.append(json.loads(line))
    diff = 0
    gate = 0
    for rec in recs:
        inp = rec.get("in") or {}
        sh = shadow_run(rec.get("func"), inp)
        srec = {"func": rec.get("func"), "in": inp, "out": sh}
        _, want, _ = check(srec)
        if canon(sh) != canon(want):
            diff += 1
            print("SHADOW MISMATCH %s\n  C : %s\n  PY: %s" %
                  (rec.get("func"), json.dumps(sh, ensure_ascii=False),
                   json.dumps(want, ensure_ascii=False)))
        bad, _, _ = check(srec)  # gate proxy: shadow output fed to the real comparator
        if bad:
            gate += 1
    print("SELFTEST relay_descriptor_flow3: %d cases, %d shadow mismatches, %d gate-proxy mismatches"
          % (len(recs), diff, gate))
    sys.exit(1 if (diff or gate) else 0)


def main():
    ns = extract()
    global REAL
    REAL = {"int": None}
    for k in REAL:
        REAL[k] = ns.get(k, _SENTINEL)
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        cases += 1
        bad, want, got = check(rec)
        if bad:
            mism += 1
            print("MISMATCH %s" % rec.get("func"))
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE relay_descriptor_flow3: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE relay_descriptor_flow3: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    if "--self-test" in sys.argv:
        extract()
        self_test()
    else:
        main()
