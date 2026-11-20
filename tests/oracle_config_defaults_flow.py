#!/usr/bin/env python3
"""Oracle for config_defaults_flow.c — the hermes_cli config-defaults FLOW port.

MODULE RESOLUTION: hermes_cli/config_defaults.py is a pure-data leaf (0 funcs, already
ported as canonical JSON in src/hermes_cli/config_defaults.c). The config-defaults LOGIC
lives in the sibling factory modules it imports, which are the real defining modules:
  hermes_cli/config_defaults_auxiliary.py : _aux
  hermes_cli/config_defaults_env.py       : _env, _base_url, _category.make,
                                            OptionalEnvVars.{arm,_ready,__getitem__,get,
                                            __contains__,__iter__,__len__,keys,items,
                                            values,copy}
_category itself is CLAIMED (learning_graph_helpers.c) and SEAMED here: _base_url calls
the module constant _prov (the claimed _category("provider",...) closure), replaced by a
recording stub. The nested closure body `make` is UNCLAIMED and extracted REAL with its
free variables bound per case. 15 unclaimed functions.

Drives the LIVE defs with SCRIPTED I/O. OptionalEnvVars is extracted REAL (a dict subclass)
and driven on a real instance seeded from the case state; the armed fill / published
callables are recording doubles firing the SAME universal-seam scripts the C harness feeds.
The module threading.local _FILLING and Lock _FILL_LOCK are real (single-threaded domain:
the lock is inert). Compares the ordered DECISION SEQUENCE + return value + post-call state
projection with ordered-dict comparison (insertion order is semantic). Per-case BaseException
catch (SystemExit / KeyboardInterrupt / GeneratorExit propagate — no ported def has an except
clause; _ready is try/finally only). Scripted raises round-trip bare.

Input: stdin JSON lines from t_port_config_defaults_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "state":...}}
Summary: ORACLE config_defaults_flow: N cases, M mismatches. Fail-closed (exit 2) when
extraction misses defs or zero cases were compared.

Script semantics (identical on both sides, FAIL-CLOSED): a MISSING script returns a
RuntimeError raise spec; a seams[name] ARRAY is a per-call queue (last repeats, empty ->
None); a single value is a constant; a scripted null is None (NULL in C);
{"__raise__": cls, "msg": ...} scripts an exception. A seam whose live result is a LIST is
scripted one level deeper ([[...]]).

Fixture domain: ASCII text; POSIX lexical paths; integral JSON numbers read as int;
simple-ASCII dict keys (no quote/backslash) so str(KeyError(key)) == '<key>'; fill() success
results are dicts (a non-dict entries value raises AttributeError on entries.items());
make ABSENT url/help/tools == _OMIT (omitted), PRESENT null written verbatim; password/
advanced ABSENT take the bound default, PRESENT overrides. State projects to
{data, fill, published, filling_active}; fill/published callables project to their markers.
"""
import ast
import builtins
import json
import os
import sys
import threading
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

# Scrubbed-env runner: derive home from __file__ (repo is parent.parent).
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "cd2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
ENV_SRC = os.path.join(QUARRY, "hermes_cli", "config_defaults_env.py")
AUX_SRC = os.path.join(QUARRY, "hermes_cli", "config_defaults_auxiliary.py")
ENV_TEXT = open(ENV_SRC, encoding="utf-8").read()
AUX_TEXT = open(AUX_SRC, encoding="utf-8").read()
ANN_FLAG = __import__("__future__").annotations.compiler_flag

ALL_FUNCS = {"aux", "env", "make", "base_url", "arm", "ready", "getitem", "get",
             "contains", "iter", "len", "keys", "items", "values", "copy"}
DICT_FUNCS = {"arm", "ready", "getitem", "get", "contains", "iter", "len", "keys",
              "items", "values", "copy"}

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "raised": []}


# ── exception construction (REAL builtins; BaseException trio included) ──

def build_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    b = getattr(builtins, cls, None)
    if isinstance(b, type) and issubclass(b, BaseException):
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
    if isinstance(o, (frozenset, set)):
        return sorted(norm(v) for v in o)
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


# ── callable doubles (project to their fixture marker) ──

class FillDouble:
    def __init__(self, marker):
        self.marker = marker

    def __call__(self):
        return seam("fill", {})


class PublishedDouble:
    def __init__(self, marker):
        self.marker = marker

    def __call__(self, entries):
        return seam("published", {"entries": entries})


def prov_stub(description, prompt, url=None, *, password=True):
    # The CLAIMED _category("provider", password=True, advanced=True) closure _prov.
    return seam("prov", {"description": description, "prompt": prompt,
                         "url": url, "password": password})


def proj_marker(obj):
    if obj is None:
        return None
    return getattr(obj, "marker", None)


# ── extraction ──

def _exec_node(node, ns, src):
    exec(compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                 flags=ANN_FLAG), ns)


def extract():
    ns = {"__name__": "config_defaults_flow_extracted", "__builtins__": builtins.__dict__}
    ns.update({"threading": threading, "json": json, "os": os, "sys": sys})
    # auxiliary module: _aux (pure factory)
    for node in ast.parse(AUX_TEXT).body:
        if isinstance(node, ast.FunctionDef) and node.name == "_aux":
            _exec_node(node, ns, AUX_SRC)
    # env module: real module constants used by OptionalEnvVars._ready
    ns["_FILL_LOCK"] = threading.Lock()
    ns["_FILLING"] = threading.local()
    ns["_OMIT"] = object()
    env_tree = ast.parse(ENV_TEXT)
    for node in env_tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "OptionalEnvVars":
            _exec_node(node, ns, ENV_SRC)
    # _env / _base_url REAL; _category is CLAIMED -> never exec'd (only its nested
    # make is extracted per case). _prov (the claimed _category product) is seamed.
    for node in env_tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in ("_env", "_base_url"):
            _exec_node(node, ns, ENV_SRC)
    ns["_prov"] = prov_stub
    missing = [n for n in ("_aux", "_env", "_base_url", "OptionalEnvVars",
                           "_FILLING", "_FILL_LOCK", "_prov") if n not in ns]
    if missing:
        print("ORACLE config_defaults_flow: FATAL missing defs: %s" % sorted(missing),
              file=sys.stderr)
        sys.exit(2)
    return ns


def extract_make(bound):
    """Exec the nested _category.make closure with its free variables bound per case."""
    cat = next(n for n in ast.parse(ENV_TEXT).body
               if isinstance(n, ast.FunctionDef) and n.name == "_category")
    mk = next(n for n in cat.body
              if isinstance(n, ast.FunctionDef) and n.name == "make")
    g = {"__builtins__": builtins.__dict__,
         "category": bound.get("category"), "password": bound.get("password"),
         "advanced": bound.get("advanced"), "_OMIT": object()}
    exec(compile(ast.Module(body=[mk], type_ignores=[]), ENV_SRC, "exec",
                 flags=ANN_FLAG), g)
    return g["make"]


# ── instance state ──

def make_instance(ns, inp):
    s = inp.get("state") or {}
    inst = ns["OptionalEnvVars"](s.get("data") or {})
    fm, pm = s.get("fill"), s.get("published")
    inst._fill = FillDouble(fm) if fm is not None else None
    inst._published = PublishedDouble(pm) if pm is not None else None
    ns["_FILLING"].active = bool(s.get("filling_active", False))
    return inst


def proj_state(ns, inst):
    # dict.items(inst) is the BASE dict view — bypasses the _ready override so the
    # projection never re-fires the fill seam.
    data = {k: norm(v) for k, v in dict.items(inst)}
    return {"data": data, "fill": proj_marker(inst._fill),
            "published": proj_marker(inst._published),
            "filling_active": bool(getattr(ns["_FILLING"], "active", False))}


# ── drive ──

def drive(ns, func, inp):
    state = None
    try:
        if func == "aux":
            kw = {"reasoning_effort": inp["reasoning_effort"]} if "reasoning_effort" in inp else {}
            ret = ns["_aux"](inp.get("timeout"), **kw, **(inp.get("extra") or {}))
        elif func == "env":
            ret = ns["_env"](inp.get("description"), inp.get("prompt"),
                             **(inp.get("keys") or {}))
        elif func == "make":
            bound, call = inp.get("bound") or {}, inp.get("call") or {}
            f = extract_make(bound)
            kw = {k: call[k] for k in ("url", "help", "tools", "password", "advanced")
                  if k in call}
            ret = f(call.get("description"), call.get("prompt"), **kw)
        elif func == "base_url":
            ret = ns["_base_url"](inp.get("name"), inp.get("prompt_name"))
        elif func in DICT_FUNCS:
            inst = make_instance(ns, inp)
            if func == "arm":
                fm, pm = inp.get("fill"), inp.get("published")
                inst.arm(FillDouble(fm) if fm is not None else None,
                         PublishedDouble(pm) if pm is not None else None)
                ret = None
            elif func == "ready":
                inst._ready()
                ret = None
            elif func == "getitem":
                ret = inst[inp.get("key")]
            elif func == "get":
                ret = (inst.get(inp.get("key"), inp["default"]) if "default" in inp
                       else inst.get(inp.get("key")))
            elif func == "contains":
                ret = inp.get("key") in inst
            elif func == "iter":
                ret = list(inst.__iter__())
            elif func == "len":
                ret = len(inst)
            elif func == "keys":
                ret = list(inst.keys())
            elif func == "items":
                ret = [[k, v] for k, v in inst.items()]
            elif func == "values":
                ret = list(inst.values())
            elif func == "copy":
                ret = inst.copy()
            state = proj_state(ns, inst)
        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:  # SystemExit included — fail-closed per case
        ret = None
        for exc, spec in CUR["raised"]:
            if e is exc:
                ret = spec  # scripted raise round-trips bare
                break
        if ret is None:
            ret = {"__raise__": type(e).__name__, "msg": str(e)}
        if func in DICT_FUNCS:
            state = proj_state(ns, inst)
    return ret, state


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
        ret, state = drive(ns, func, inp)
        want = {"ret": norm(ret), "trace": CUR["trace"], "state": norm(state)}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE config_defaults_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE config_defaults_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()