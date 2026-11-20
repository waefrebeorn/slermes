#!/usr/bin/env python3
"""Oracle for the FLOW port of the UNCLAIMED remainder of
agent/tool_guardrails.py (src/agent/tool_guardrails_flow3.c, tg3_*).

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"] with scripted universal-seam stubs, and
compares the ordered trace, the return and the state carrier. Live defs are
AST-extracted (quarry venv, py3.14); module constants, dataclasses and the full
builtin exception hierarchy are seeded into the namespace.

Ported (11 UNCLAIMED defs): is_stall_guard_repeatable,
_is_non_interactive_platform, _canonical_json, _int_at_least, _ordinal,
_without_execute_code_metadata, ToolCallGuardrailController.__init__, ._decide,
.observe_call, ._detect_identical_cycle, .record_persisted_result.

Claimed siblings are SEAMED (never re-claimed): ToolCallSignature.from_call,
_coerce_args, _result_hash, _build_result_reference_stub, reset_for_turn,
ToolCallGuardrailConfig, and the int() builtin in _int_at_least.

Summary: 'ORACLE tool_guardrails_flow3: N cases, M mismatches'.
"""
import ast
import builtins
import json
import os
import sys
import types
import textwrap
import __future__
from dataclasses import dataclass, field, asdict, fields
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "tool_guardrails.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

SRC = str(QUARRY / "agent" / "tool_guardrails.py")
ANN_FLAG = __future__.annotations.compiler_flag

FUNCS = [
    "is_stall_guard_repeatable",
    "_is_non_interactive_platform",
    "_canonical_json",
    "_int_at_least",
    "_ordinal",
    "_without_execute_code_metadata",
    "ToolCallGuardrailController.__init__",
    "ToolCallGuardrailController._decide",
    "ToolCallGuardrailController.observe_call",
    "ToolCallGuardrailController._detect_identical_cycle",
    "ToolCallGuardrailController.record_persisted_result",
]
CONTROLLER_METHODS = {
    "ToolCallGuardrailController.__init__": "__init__",
    "ToolCallGuardrailController._decide": "_decide",
    "ToolCallGuardrailController.observe_call": "observe_call",
    "ToolCallGuardrailController._detect_identical_cycle": "_detect_identical_cycle",
    "ToolCallGuardrailController.record_persisted_result": "record_persisted_result",
}
TOP_FUNCS = [f for f in FUNCS if f not in CONTROLLER_METHODS]

CURRENT = {"rec": None}
_MISSING = object()


# ------------------------------------------------------------ value helpers
def norm_value(v):
    if isinstance(v, BaseException):
        return str(v)
    if hasattr(v, "__dataclass_fields__") and not isinstance(v, dict):
        try:
            return norm_value(asdict(v))
        except Exception:
            return str(v)
    if isinstance(v, dict):
        return {k: norm_value(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_value(x) for x in v]
    if isinstance(v, float):
        if v == int(v) and abs(v) < 1e18:
            return int(v)
        return v
    return v


def norm_args(v):
    v = norm_value(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    if isinstance(v, list):
        return [norm_args(x) for x in v]
    return v


# -------------------------------------------------------- full exc hierarchy
_BUILTIN_EXC = {}
for _n in dir(builtins):
    _c = getattr(builtins, _n)
    if isinstance(_c, type) and issubclass(_c, BaseException):
        _BUILTIN_EXC[_n] = _c


def build_exc(spec):
    """Materialize ANY scripted exception kind (full builtin hierarchy)."""
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    if kind == "CancelledError":
        import asyncio
        return asyncio.CancelledError(msg)
    if kind in ("BaseExceptionGroup", "ExceptionGroup"):
        cls = _BUILTIN_EXC[kind]
        subs = ([SystemExit("sub")] if kind == "BaseExceptionGroup" else [ValueError("sub")])
        return cls(msg, subs)
    if kind in _BUILTIN_EXC:
        cls = _BUILTIN_EXC[kind]
        try:
            return cls(msg)
        except Exception:
            e = cls.__new__(cls)
            Exception.__init__(e, msg)
            return e
    return type(kind, (Exception,), {})(msg)


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


class Script:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.handed = []

    def next(self, name):
        entry = self.io.get(name, _MISSING)
        if entry is _MISSING:
            return self._fail(name)
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return self._fail(name)
            self.cursors[name] = idx + 1
            val = entry[idx]
        else:
            val = entry
        return self._unwrap(name, val)

    def _fail(self, name):
        spec = {"__raise__": "RuntimeError", "message": "no script for " + name}
        r = Raise(spec, build_exc(spec))
        self.handed.append(r)
        return r

    def _unwrap(self, name, val):
        if isinstance(val, dict) and "__raise__" in val:
            r = Raise(val, build_exc(val))
            self.handed.append(r)
            return r
        if isinstance(val, dict) and val.get("__script__") is True:
            return val.get("ret")
        return val


# ---------------------------------------------------------- value doubles
@dataclass
class _Sig:
    tool_name: str
    args_hash: str


@dataclass
class ToolGuardrailDecision:
    action: str = "allow"
    code: str = "allow"
    message: str = ""
    tool_name: str = ""
    count: int = 0
    signature: object = None

    @property
    def allows_execution(self):
        return self.action in {"allow", "warn"}

    @property
    def should_halt(self):
        return self.action in {"block", "halt"}


@dataclass
class IdenticalCallObservation:
    notice: object = None
    stub: object = None
    kind: object = None


def as_sig(v):
    if v is None:
        return None
    if isinstance(v, _Sig):
        return v
    if isinstance(v, dict):
        return _Sig(tool_name=v.get("tool_name", ""), args_hash=v.get("args_hash", ""))
    return v


# ------------------------------------------------------ per-seam projections
def proj_seam(name, a, k):
    if name == "coerce_args":
        return {"args": a[0] if len(a) > 0 else None}
    if name == "signature_from_call":
        return {"tool_name": a[0] if len(a) > 0 else None,
                "args": a[1] if len(a) > 1 else None}
    if name == "result_hash":
        return {"result": a[0] if len(a) > 0 else None,
                "tool_name": a[1] if len(a) > 1 else None}
    if name in ("build_result_reference_stub",):
        return {"tool_name": a[0] if len(a) > 0 else None,
                "args": a[1] if len(a) > 1 else None}
    if name == "py_int":
        return {"value": a[0] if len(a) > 0 else None}
    return {**{f"arg{i}": v for i, v in enumerate(a)}, **k}


_CONVERTERS = {
    "signature_from_call": lambda v: as_sig(v),
}


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.script = Script(inp.get("io"))

    def recorder(self, name):
        ctx = self

        def call(*a, **k):
            args = norm_args(proj_seam(name, a, k))
            raw = ctx.script.next(name)
            if isinstance(raw, Raise):
                ctx.trace.append({"ev": name, "args": args, "ret": raw.spec})
                raise raw.exc
            ctx.trace.append({"ev": name, "args": args, "ret": norm_value(raw)})
            conv = _CONVERTERS.get(name)
            return conv(raw) if conv else raw
        return call


# --------------------------------------------------------- live namespace
NS = {}
REAL = {}

def _exec_seg(src, node, ns, filename):
    seg = textwrap.dedent(ast.get_source_segment(src, node))
    code = compile(seg, filename, "exec", flags=ANN_FLAG, dont_inherit=True)
    exec(code, ns)


def load_ns():
    global NS, REAL
    ns = {"__name__": "tg3_live", "Any": object, "Mapping": object,
          "json": json, "dataclass": dataclass, "field": field, "asdict": asdict,
          "fields": fields,
          "ToolGuardrailDecision": ToolGuardrailDecision,
          "IdenticalCallObservation": IdenticalCallObservation,
          "_DECISION_MESSAGES": {}, "_IDENTICAL_CALL_NOTICE": "",
          "_IDENTICAL_CYCLE_NOTICE": "",
          "STALL_GUARD_REPEATABLE_TOOLS": frozenset({"process_manage"}),
          "_STALL_GUARD_REPEATABLE_SUFFIXES": ("_get_result", "_poll"),
          "_ATTENDED_PLATFORMS": frozenset({"cli", "tui", "desktop", "acp", "subagent", "api_server"}),
          "STALL_GUARD_IDENTICAL_CALL_THRESHOLD": 3,
          "_STALL_GUARD_MAX_CYCLE_PERIOD": 4,
          "IDENTICAL_RESULT_STUB_MIN_CHARS": 512,
          "_RESULT_STUB_ARGS_PREVIEW_CHARS": 120}
    NS = ns
    src = Path(SRC).read_text(encoding="utf-8")
    tree = ast.parse(src)

    # seed ALL module constants from real source (authoritative values).
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                _exec_seg(src, node, ns, SRC)
            except Exception:
                pass

    # sanity: the constants the ported funcs read are really bound
    for nm in ("_DECISION_MESSAGES", "_IDENTICAL_CALL_NOTICE", "_IDENTICAL_CYCLE_NOTICE",
               "STALL_GUARD_IDENTICAL_CALL_THRESHOLD", "_STALL_GUARD_MAX_CYCLE_PERIOD",
               "IDENTICAL_RESULT_STUB_MIN_CHARS", "STALL_GUARD_REPEATABLE_TOOLS",
               "_STALL_GUARD_REPEATABLE_SUFFIXES", "_ATTENDED_PLATFORMS"):
        if nm not in ns:
            raise RuntimeError("constant seed missing: " + nm)

    # extract top-level target defs
    found = set()
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in TOP_FUNCS:
            _exec_seg(src, node, ns, SRC)
            found.add(node.name)
    # extract controller methods (plain functions taking self)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "ToolCallGuardrailController":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in CONTROLLER_METHODS.values():
                    _exec_seg(src, sub, ns, SRC)
                    found.add("ToolCallGuardrailController." + sub.name)
    missing = [f for f in FUNCS if f not in found]
    if missing:
        raise RuntimeError("missing target defs: %s" % sorted(missing))

    # live functions callable by other live functions
    ns["is_stall_guard_repeatable"] = ns["is_stall_guard_repeatable"]
    ns["_ordinal"] = ns["_ordinal"]

    # claimed siblings (and int builtin) -> recording seam stubs
    ns["_coerce_args"] = _stub("coerce_args")
    ns["_result_hash"] = _stub("result_hash")
    ns["ToolCallSignature"] = types.SimpleNamespace(from_call=_stub("signature_from_call"))
    ns["ToolCallGuardrailConfig"] = _stub("ToolCallGuardrailConfig")
    ns["int"] = _stub("py_int")

    REAL = {nm: ns[nm if nm in TOP_FUNCS else CONTROLLER_METHODS[nm]] for nm in FUNCS}


# _stub is defined after load_ns but called at runtime via load_ns(); bind late.
def _stub(name):
    def f(*a, **k):
        return CURRENT["rec"](name)(*a, **k)
    return f


class SelfDouble:
    def __init__(self, state, sd_ns):
        cfg = state.get("config", {})
        self.config = types.SimpleNamespace(**(cfg if isinstance(cfg, dict) else {}))
        self._halt_decision = as_sig(state.get("_halt_decision"))
        # halt decision is a decision dict -> rebuild as dataclass
        hd = state.get("_halt_decision")
        self._halt_decision = _as_decision(hd)
        self._identical_streak_sig = as_sig(state.get("_identical_streak_sig"))
        self._identical_streak_result_hash = state.get("_identical_streak_result_hash", "") or ""
        self._identical_streak_count = state.get("_identical_streak_count", 0) or 0
        self._identical_streak_first_call_id = state.get("_identical_streak_first_call_id", "") or ""
        self._call_history = [
            (as_sig(e[0]), e[1], e[2]) for e in (state.get("_call_history") or [])
        ]
        self._persisted_result_paths = dict(state.get("_persisted_result_paths") or {})
        # stateful helpers that are ported here run REAL
        for m in ("_decide", "observe_call", "_detect_identical_cycle", "record_persisted_result"):
            setattr(self, m, types.MethodType(sd_ns[m], self))
        # controller __init__ lives under a distinct name (SelfDouble.__init__
        # itself must keep its own signature)
        self._live_init = types.MethodType(sd_ns["__init__"], self)
        # claimed siblings that are called on self -> seam recorders
        self.reset_for_turn = CURRENT["rec"]("reset_for_turn")
        self._build_result_reference_stub = CURRENT["rec"]("build_result_reference_stub")
        self.before_call = CURRENT["rec"]("before_call")
        self.after_call = CURRENT["rec"]("after_call")
        self._is_idempotent = CURRENT["rec"]("_is_idempotent")
        self._check_loop_cap = CURRENT["rec"]("_check_loop_cap")


def _as_decision(v):
    if v is None:
        return None
    if isinstance(v, ToolGuardrailDecision):
        return v
    if isinstance(v, dict):
        return ToolGuardrailDecision(
            action=v.get("action", "allow"), code=v.get("code", "allow"),
            message=v.get("message", ""), tool_name=v.get("tool_name", ""),
            count=v.get("count", 0) or 0, signature=as_sig(v.get("signature")))
    return None


def project_state(sd):
    if isinstance(sd.config, dict):
        cfg = sd.config
    elif hasattr(sd.config, "__dict__"):
        cfg = sd.config.__dict__
    else:
        cfg = sd.config   # config may be an arbitrary truthy value (string/int)
    hd = asdict(sd._halt_decision) if isinstance(sd._halt_decision, ToolGuardrailDecision) else None
    sig = asdict(sd._identical_streak_sig) if isinstance(sd._identical_streak_sig, _Sig) else None
    hist = []
    for e in sd._call_history:
        s = asdict(e[0]) if isinstance(e[0], _Sig) else e[0]
        hist.append([s, e[1], e[2]])
    return norm_value({
        "config": cfg,
        "_halt_decision": hd,
        "_identical_streak_sig": sig,
        "_identical_streak_result_hash": sd._identical_streak_result_hash,
        "_identical_streak_count": sd._identical_streak_count,
        "_identical_streak_first_call_id": sd._identical_streak_first_call_id,
        "_call_history": hist,
        "_persisted_result_paths": sd._persisted_result_paths,
    })


# ------------------------------------------------------------- drive
def drive(func, inp, sd):
    g = inp.get
    if func == "is_stall_guard_repeatable":
        return NS[func](g("tool_name"))
    if func == "_is_non_interactive_platform":
        return NS[func](g("platform"))
    if func == "_canonical_json":
        return NS[func](g("value"))
    if func == "_int_at_least":
        return NS[func](g("value"), g("default"), g("minimum"))
    if func == "_ordinal":
        return NS[func](g("count"))
    if func == "_without_execute_code_metadata":
        return NS[func](g("parsed"))
    if func == "ToolCallGuardrailController.__init__":
        return sd._live_init(g("config"))
    if func == "ToolCallGuardrailController._decide":
        return sd._decide(
            g("action"), g("code"), g("tool_name"), g("count"),
            as_sig(g("signature")), message=g("message"), **dict(g("fmt") or {}))
    if func == "ToolCallGuardrailController.observe_call":
        return sd.observe_call(
            g("tool_name"), g("args"), g("result"),
            tool_call_id=g("tool_call_id", "") or "", failed=g("failed", False) or False)
    if func == "ToolCallGuardrailController._detect_identical_cycle":
        return sd._detect_identical_cycle()
    if func == "ToolCallGuardrailController.record_persisted_result":
        return sd.record_persisted_result(g("tool_call_id"), g("file_path"))
    raise RuntimeError("unknown func " + func)


def proj_ret(v):
    if isinstance(v, (ToolGuardrailDecision, IdenticalCallObservation, _Sig)):
        return norm_value(asdict(v))
    return norm_value(v)


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["rec"] = ctx.recorder
    sd = SelfDouble(inp.get("self") or {}, NS)
    try:
        ret = drive(func, inp, sd)
        return {"ret": proj_ret(ret), "trace": ctx.trace, "st": project_state(sd)}
    except BaseException as e:
        spec = None
        for r in ctx.script.handed:
            if e is r.exc:
                spec = r.spec
                break
        if spec is None:
            spec = {"__raise__": type(e).__name__, "message": str(e)}
        return {"ret": spec, "trace": ctx.trace, "st": project_state(sd)}


def main():
    load_ns()
    n = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        func = rec.get("func")
        inp = rec.get("in") or {}
        n += 1
        want = run_case(func, inp)
        got = norm_value({"ret": rec.get("ret"), "trace": rec.get("trace"), "st": rec.get("st")})
        if got != want:
            mism += 1
            if mism <= 16:
                print("MISMATCH [%s] case %d" % (func, n))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:1200]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:1200]))
    print("ORACLE tool_guardrails_flow3: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
