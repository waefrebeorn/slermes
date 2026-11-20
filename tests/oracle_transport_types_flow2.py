#!/usr/bin/env python3
"""Oracle for transport_types_flow2.c — agent/transports/types.py UNCLAIMED remainder
(LIVE authority: the quarry agent/transports/types.py).

Ports the 12 dataclass-GENERATED methods no PoP claims: __init__ / __repr__ / __eq__ /
__replace__ for each of ToolCall / Usage / NormalizedResponse. The CLAIMED accessor
surface (_pd, from_openai, the property getters, build_tool_call, map_finish_reason) is
NOT re-ported. The three classes are AST-extracted from the quarry (no heavy
agent.transports import) and exec'd under a registered sys.modules name so py3.14
dataclasses resolves cls.__module__. The ONLY live collaborator these methods invoke is
the class constructor inside __replace__ (`return self.__class__(**changes)`); it is
replaced by a recording stub (patched onto Cls.__init__) returning the SAME scripted
results the C harness feeds its universal seam. __init__ / __repr__ / __eq__ are pure
(empty trace). Compares the ordered seam trace + return value with ordered-dict
comparison (insertion order is semantic). Per-case BaseException catch (SystemExit /
KeyboardInterrupt / GeneratorExit escape and are compared as raise specs; scripted
raises round-trip bare; the real unknown-kwarg TypeError from the ctor is captured).

Input: stdin JSON lines from t_port_transport_types_flow2:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...]}}
Summary: ORACLE transport_types_flow2: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses classes or zero cases were compared.

Script semantics (identical on both sides, FAIL-CLOSED): a MISSING script returns a
RuntimeError raise spec {"__raise__":"RuntimeError","msg":"no script for seam: <name>"};
a seams[name] ARRAY is a per-call queue (last value repeats, empty array -> None); a
single value is a constant; a scripted null is None (NULL in C);
{"__raise__": cls, "msg": ...} scripts an exception.

Fixture domain (kept identical on both sides): ASCII text; integral JSON numbers read
as int; field values are plain JSON (the dataclass stores them verbatim, no coercion);
a PRESENT null passes through verbatim, an ABSENT field takes its default; DIRECT
__init__ kwargs restricted to declared fields with required fields supplied (no
binding-error cases there); __eq__ inputs represent constructed instances (required
fields present); __replace__ DOES probe the unknown-kwarg TypeError path.
"""
import ast
import builtins
import json
import os
import sys
import types
from dataclasses import dataclass, field, fields
from typing import Any

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("HOME", os.path.dirname(_REPO))
os.environ.setdefault("USERPROFILE", os.path.dirname(_REPO))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "agent", "transports", "types.py")
SRC_TEXT = open(SRC, encoding="utf-8").read()
ANN_FLAG = __import__("__future__").annotations.compiler_flag

CLASSES = ["ToolCall", "Usage", "NormalizedResponse"]
# func key -> (class, method)
FUNCS = {
    "tool_call_init": ("ToolCall", "__init__"),
    "tool_call_repr": ("ToolCall", "__repr__"),
    "tool_call_eq": ("ToolCall", "__eq__"),
    "tool_call_replace": ("ToolCall", "__replace__"),
    "usage_init": ("Usage", "__init__"),
    "usage_repr": ("Usage", "__repr__"),
    "usage_eq": ("Usage", "__eq__"),
    "usage_replace": ("Usage", "__replace__"),
    "normalized_response_init": ("NormalizedResponse", "__init__"),
    "normalized_response_repr": ("NormalizedResponse", "__repr__"),
    "normalized_response_eq": ("NormalizedResponse", "__eq__"),
    "normalized_response_replace": ("NormalizedResponse", "__replace__"),
}
ALL_FUNCS = set(FUNCS)

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "raised": []}


# ── exception construction (REAL builtins; BaseException trio must be genuine) ──

def build_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    b = getattr(builtins, cls, None)
    if isinstance(b, type) and issubclass(b, BaseException):
        # The three Unicode*Error builtins need a 5-arg __init__; bypass via __new__.
        if cls in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
            e = b.__new__(b)
            Exception.__init__(e, msg)
            return e
        return b(msg)
    return type(cls, (Exception,), {})(msg)


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    scripts = CUR["scripts"]
    if name not in scripts:
        # FAIL-CLOSED: missing script -> RuntimeError raise spec (presence test,
        # NOT .get() is None — a scripted null is a valid None result).
        r = {"__raise__": "RuntimeError", "msg": "no script for seam: %s" % name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    r = json.loads(json.dumps(r))  # deep copy: live mutation cannot corrupt script/trace
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        e = build_exc(r)
        CUR["raised"].append((e, r))
        raise e
    return r


# ── normalization / canonical comparison ──

def norm(o):
    if o is None or isinstance(o, (bool, int, float, str)):
        return o
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    return str(o)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    o = _numnorm(o)
    if isinstance(o, dict):
        return ("obj", [(k, canon(v)) for k, v in o.items()])
    if isinstance(o, (list, tuple)):
        return ("arr", [canon(v) for v in o])
    if o is None:
        return ("null",)
    if isinstance(o, bool):
        return ("bool", o)
    if isinstance(o, (int, float)):
        return ("num", float(o))
    return ("str", o)


def project(obj):
    """Instance -> field projection in declaration order (matches the C builders)."""
    return {f.name: norm(getattr(obj, f.name)) for f in fields(obj)}


# ── extraction ──

def extract():
    mod = types.ModuleType("transport_types_flow2_extracted")
    sys.modules["transport_types_flow2_extracted"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "transport_types_flow2_extracted",
        "json": json, "dataclass": dataclass, "field": field, "Any": Any,
    })
    tree = ast.parse(SRC_TEXT)
    # Classes in source order (ToolCall before NormalizedResponse references it).
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in CLASSES:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                         flags=ANN_FLAG), ns)
    missing = [n for n in CLASSES if n not in ns]
    if missing:
        print("ORACLE transport_types_flow2: FATAL missing classes: %s" % sorted(missing),
              file=sys.stderr)
        sys.exit(2)
    for cname in CLASSES:
        for meth in ("__init__", "__repr__", "__eq__", "__replace__"):
            if not callable(getattr(ns[cname], meth, None)):
                print("ORACLE transport_types_flow2: FATAL %s.%s missing" % (cname, meth),
                      file=sys.stderr)
                sys.exit(2)
    return ns


# ── drive ──

def drive(ns, func, inp):
    cname, meth = FUNCS[func]
    Cls = ns[cname]
    try:
        if meth == "__init__":
            obj = Cls(**(inp.get("fields") or {}))
            ret = project(obj)
        elif meth == "__repr__":
            obj = Cls(**(inp.get("fields") or {}))
            ret = repr(obj)
        elif meth == "__eq__":
            self_obj = Cls(**(inp.get("self") or {}))
            ocls = ns[inp.get("other_class") or cname]
            other_obj = ocls(**(inp.get("other") or {}))
            ret = (self_obj == other_obj)
        else:  # __replace__ — seam the constructor call self.__class__(**changes)
            obj = Cls(**(inp.get("self") or {}))   # real init, recorder not yet installed
            orig_init = Cls.__init__
            seam_name = "%s.__init__" % cname

            def rec(self, *a, **kw):
                seam(seam_name, dict(kw))          # records; raises on __raise__/fail-closed
                return orig_init(self, *a, **kw)   # real validation + construction

            Cls.__init__ = rec
            try:
                ret_obj = obj.__replace__(**(inp.get("changes") or {}))
            finally:
                Cls.__init__ = orig_init
            ret = project(ret_obj)
    except BaseException as e:  # SystemExit included — fail-closed per case
        ret = None
        for exc, spec in CUR["raised"]:
            if e is exc:
                ret = spec  # scripted raise round-trips bare
                break
        if ret is None:
            ret = {"__raise__": type(e).__name__, "msg": str(e)}
    return ret


def main():
    ns = extract()
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["raised"] = []
        if func not in ALL_FUNCS:
            mism += 1
            print("PYERR unknown func: %s" % func)
            continue
        ret = drive(ns, func, inp)
        want = {"ret": norm(ret), "trace": CUR["trace"]}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE transport_types_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE transport_types_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()