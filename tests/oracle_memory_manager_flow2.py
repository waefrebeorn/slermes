#!/usr/bin/env python3
"""Oracle for src/agent/memory_manager_flow2.c — agent/memory_manager.py UNCLAIMED
remainder (LIVE authority: C:/Users/eman5/hermes-agent/agent/memory_manager.py).

The 14 ported defs are AST-extracted from the quarry (no heavy
agent.memory_manager import). Collaborators that other PoP sets own, plus the
foreign-module calls (inspect.signature, redact_for_egress, caller_scope,
provider callbacks, the background gate, logger, flush_pending/shutdown_all),
are recording stubs fed the SAME scripted results the C harness feeds its
universal seam. Internal composition (accepts_require_checkpoint ->
signature_params; supports_pre_compress_checkpoint -> checkpoint_api_version;
call_unless_stalled -> registry_key; release_soft -> claim_teardown) runs REAL
on both sides. Compares the ordered decision sequence + return + state window
with order-sensitive canon(). Per-case BaseException catch (SystemExit /
KeyboardInterrupt / GeneratorExit escape every `except Exception` /
`(TypeError, ValueError)` and are compared as bare raise specs).

Input: stdin JSON lines from t_port_memory_manager_flow2:
    {"func":..., "in":{...,"seams":{...}}, "out":{"ret":...,"trace":[...],"state":{...}}}
Summary: ORACLE memory_manager_flow2: N cases, M mismatches. Fail-closed (2)
on missing defs or zero cases.

Script semantics (identical on both sides, FAIL-CLOSED): a MISSING script
returns a RuntimeError raise spec (presence test, NOT .get() is None — a
scripted null is a valid None); a seams[name] ARRAY is a per-call queue (last
value repeats, empty -> None); a single value is a constant; a scripted null
is None; {"__raise__":cls,"msg":...} scripts an exception (built by a REAL
builtin class so `except` binds by hierarchy identity).

Fixture domain: ASCII text; integral numbers read as int (integral floats ==
ints); inspect Parameter kinds are the six spelling strings; provider doubles
are dicts with "name" (and optional pre_compress_checkpoint_api_version);
state window always carries all five keys; scripted exception kinds are
builtin classes listed in MM2's full hierarchy table.
"""
import ast
import builtins
import contextlib
import copy
import inspect
import json
import os
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "mm2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "agent", "memory_manager.py")
SRC_TEXT = open(SRC, encoding="utf-8").read().replace("\r", "")
ANN_FLAG = __import__("__future__").annotations.compiler_flag

# func key -> live def name
PY = {
    "signature_params": "_signature_params",
    "has_var_kwargs": "_has_var_kwargs",
    "accepts_require_checkpoint": "_accepts_require_checkpoint",
    "scrub": "_scrub",
    "owner_allows": "_owner_allows",
    "fork_turn_hooks_suppressed": "_fork_turn_hooks_suppressed",
    "provider_sync_accepts": "_provider_sync_accepts",
    "registry_key": "_registry_key",
    "checkpoint_api_version": "_checkpoint_api_version",
    "supports_pre_compress_checkpoint": "supports_pre_compress_checkpoint",
    "each_provider": "_each_provider",
    "call_unless_stalled": "_call_unless_stalled",
    "claim_teardown": "claim_teardown",
    "release_soft": "release_soft",
}
ALL_FUNCS = set(PY)
TOP_DEFS = list(PY.values())
STATIC_DEFS = {"_provider_sync_accepts", "_checkpoint_api_version"}
STATE_KEYS = ("bound_scope", "home_key", "agent_context", "teardown_claim", "shutting_down")

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "raised": []}
REAL = {}

# ── exception construction (real builtin classes => except binds by identity) ──
_UNICODE_ERRS = ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError")


def build_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    b = getattr(builtins, cls, None)
    if isinstance(b, type) and issubclass(b, BaseException):
        if cls in _UNICODE_ERRS:
            e = b.__new__(b)
            Exception.__init__(e, msg)
            return e
        return b(msg)
    return type(cls, (Exception,), {})(msg)


def seam(name, args):
    args = norm({k: v for k, v in args.items() if v is not None})
    scripts = CUR["scripts"]
    if name not in scripts:
        # FAIL-CLOSED: missing script -> RuntimeError raise spec (presence test).
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
    if isinstance(o, types.SimpleNamespace) and hasattr(o, "_spec"):
        return norm(o._spec)
    if isinstance(o, BaseException):
        return str(o)
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


# ── doubles ──

_KIND = {
    "POSITIONAL_ONLY": inspect.Parameter.POSITIONAL_ONLY,
    "POSITIONAL_OR_KEYWORD": inspect.Parameter.POSITIONAL_OR_KEYWORD,
    "VAR_POSITIONAL": inspect.Parameter.VAR_POSITIONAL,
    "KEYWORD_ONLY": inspect.Parameter.KEYWORD_ONLY,
    "VAR_KEYWORD": inspect.Parameter.VAR_KEYWORD,
}


def mat_params(r):
    """Materialize a scripted params mapping into objects with real Parameter kinds."""
    if not isinstance(r, dict):
        return r
    out = {}
    for k, v in r.items():
        if isinstance(v, dict):
            o = types.SimpleNamespace()
            o._spec = v              # norm() unwraps attribute doubles to raw JSON
            o.kind = _KIND.get(v.get("kind"))
            out[k] = o
        else:
            out[k] = v
    return out


def mat_provider(spec):
    if spec is None or not isinstance(spec, dict):
        return spec
    o = types.SimpleNamespace()
    o._spec = spec
    o.name = spec.get("name", "")
    if "handle_tool_call" in spec:
        o.handle_tool_call = spec["handle_tool_call"]
    if "sync_turn" in spec:
        o.sync_turn = spec["sync_turn"]
    if "pre_compress_checkpoint_api_version" in spec:
        o.pre_compress_checkpoint_api_version = spec["pre_compress_checkpoint_api_version"]
    return o


class InspectDouble:
    Parameter = inspect.Parameter  # real enum: `is` / `in` identity in the live defs

    @staticmethod
    def signature(fn):
        r = seam("signature", {"fn": fn})
        return types.SimpleNamespace(parameters=mat_params(r))


class LoggerDouble:
    def log(self, level, fmt, *args, **kw):
        return seam("logger.log", {"level": level, "fmt": fmt, "args": list(args), **kw})

    def warning(self, fmt, *args, **kw):
        return seam("logger.warning", {"fmt": fmt, "args": list(args), **kw})

    def debug(self, fmt, *args, **kw):
        return seam("logger.debug", {"fmt": fmt, "args": list(args), **kw})


class GateDouble:
    def enter(self, key, token, timeout):
        return seam("gate.enter", {"key": key, "timeout": timeout})

    def leave(self, key, token, *, ok):
        return seam("gate.leave", {"key": key, "ok": ok})


class SelfDouble:
    """Backs the extracted instance methods. State keys live in `st` (echoed
    on every exit, including raise); methods are bound ported defs or stubs."""

    _MAP = {
        "_bound_scope": "bound_scope",
        "_home_key": "home_key",
        "_agent_context": "agent_context",
        "_teardown_claim": "teardown_claim",
        "_shutting_down": "shutting_down",
    }

    def __init__(self, st, ns, inp):
        object.__setattr__(self, "_st", st)
        object.__setattr__(self, "_providers", [mat_provider(p) for p in (inp.get("providers") or [])])
        object.__setattr__(self, "_sync_executor_lock", contextlib.nullcontext())
        object.__setattr__(self, "_BACKGROUND_GATE", GateDouble())
        object.__setattr__(self, "_each_provider", types.MethodType(ns["_each_provider"], self))
        object.__setattr__(self, "_registry_key", types.MethodType(ns["_registry_key"], self))
        object.__setattr__(self, "_checkpoint_api_version", ns["_checkpoint_api_version"])
        object.__setattr__(self, "supports_pre_compress_checkpoint",
                           types.MethodType(ns["supports_pre_compress_checkpoint"], self))
        object.__setattr__(self, "claim_teardown", types.MethodType(ns["claim_teardown"], self))
        object.__setattr__(self, "release_soft", types.MethodType(ns["release_soft"], self))

        def flush(*, timeout=None):
            return seam("flush_pending", {"timeout": timeout})

        def shutdown():
            return seam("shutdown_all", {})

        object.__setattr__(self, "flush_pending", flush)
        object.__setattr__(self, "shutdown_all", shutdown)

    def __getattr__(self, name):
        if name in self._MAP:
            return self._st[self._MAP[name]]
        raise AttributeError(name)

    def __setattr__(self, name, val):
        if name in self._MAP:
            self._st[self._MAP[name]] = val
        else:
            object.__setattr__(self, name, val)


# ── extraction ──

def extract():
    ns = {
        "__name__": "memory_manager_flow2_extracted",
        "__file__": SRC,
        "inspect": InspectDouble(),
        "logging": __import__("logging"),  # default args evaluated at def time
        "logger": LoggerDouble(),
        "caller_scope": lambda scope, label: seam("caller_scope", {"scope": scope, "label": label}),
        "redact_for_egress": lambda text, *, urls=True: seam(
            "redact_for_egress", {"text": text, "urls": urls}),
        "_BACKGROUND_GATE": GateDouble(),
        "_BACKGROUND_CALL_STALL_S": 30.0,
        "_LEGACY_PRE_COMPRESS_API_VERSION": 1,
        "PRE_COMPRESS_CHECKPOINT_API_VERSION": 2,
        "Optional": object,
        "Callable": object,
        "Any": object,
    }
    tree = ast.parse(SRC_TEXT)
    nodes = {}

    def exec_node(node):
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                     flags=ANN_FLAG), ns)

    # Module-level leaves + instance/static methods on MemoryManager.
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in TOP_DEFS:
            nodes[node.name] = node
        elif isinstance(node, ast.ClassDef):
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in TOP_DEFS:
                    nodes[sub.name] = sub
    for name in TOP_DEFS:
        if name not in nodes:
            print("ORACLE memory_manager_flow2: FATAL AST node missing: %s" % name,
                  file=sys.stderr)
            sys.exit(2)
        nd = copy.copy(nodes[name])
        nd.decorator_list = []  # @staticmethod on two defs; bind plain
        try:
            exec_node(nd)
        except Exception as exc:
            print("ORACLE memory_manager_flow2: FATAL exec %s: %s" %
                  (name, exc), file=sys.stderr)
            sys.exit(2)
    ns["logger"] = LoggerDouble()  # re-set after any Assign-seeding
    missing = [n for n in TOP_DEFS if n not in ns]
    if missing:
        print("ORACLE memory_manager_flow2: FATAL missing defs: %s" % sorted(missing),
              file=sys.stderr)
        sys.exit(2)
    for n in TOP_DEFS:
        REAL[n] = ns[n]
    return ns


def install_stubs(ns, inp, func, st):
    ns["logger"] = LoggerDouble()
    ns["inspect"] = InspectDouble()
    ns["_BACKGROUND_GATE"] = GateDouble()


def drive(ns, func, inp, st):
    slf = SelfDouble(st, ns, inp)
    try:
        if func == "signature_params":
            ret = ns["_signature_params"](inp.get("fn"))
        elif func == "has_var_kwargs":
            ret = ns["_has_var_kwargs"](mat_params(inp.get("params")))
        elif func == "accepts_require_checkpoint":
            ret = ns["_accepts_require_checkpoint"](inp.get("fn"))
        elif func == "scrub":
            ret = ns["_scrub"](inp.get("text"), urls=inp.get("urls", True))
        elif func == "owner_allows":
            ret = ns["_owner_allows"](slf, inp.get("hook", ""))
        elif func == "fork_turn_hooks_suppressed":
            ret = ns["_fork_turn_hooks_suppressed"](slf)
        elif func == "provider_sync_accepts":
            prov = types.SimpleNamespace(sync_turn=inp.get("sync_fn"))
            ret = ns["_provider_sync_accepts"](prov, inp.get("keyword", ""))
        elif func == "registry_key":
            ret = slf._registry_key(mat_provider({"name": inp.get("provider_name", "")}))
        elif func == "checkpoint_api_version":
            ret = ns["_checkpoint_api_version"](mat_provider(inp.get("provider")))
        elif func == "supports_pre_compress_checkpoint":
            ret = slf.supports_pre_compress_checkpoint(api_version=inp.get("api_version", 2))
        elif func == "each_provider":
            cn = inp.get("call_name", "")

            def call(p):
                return seam(cn, {"provider": p})

            ret = ns["_each_provider"](slf, inp.get("label", ""), call,
                                       level=inp.get("level", 10),
                                       exc_info=inp.get("exc_info", False))
        elif func == "call_unless_stalled":
            cn = inp.get("call_name", "")
            prov = mat_provider(inp.get("provider"))

            def call():
                return seam(cn, {})

            ret = ns["_call_unless_stalled"](slf, prov, call)
        elif func == "claim_teardown":
            ret = slf.claim_teardown(session_end=inp.get("session_end", False))
        elif func == "release_soft":
            ret = slf.release_soft()
        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:
        ret = None
        for exc, spec in CUR["raised"]:
            if e is exc:
                ret = spec
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
        st = copy.deepcopy(inp.get("state") or {})
        for k in STATE_KEYS:
            if k not in st:
                st[k] = None
        install_stubs(ns, inp, func, st)
        ret = drive(ns, func, inp, st)
        want = {"ret": norm(ret), "trace": CUR["trace"], "state": norm(st)}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE memory_manager_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE memory_manager_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
