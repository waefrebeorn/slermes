#!/usr/bin/env python3
"""Oracle for the FLOW port of the UNCLAIMED remainder of
agent/nous_rate_guard.py (src/agent/nous_rate_guard_flow2.c, nrg2_*).

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"] with scripted universal-seam stubs, and
compares the ordered trace, the return and the state projection ({} -- every
ported function is pure). Live defs are AST-extracted (quarry venv, py3.14).

Ported: _is_exhausted, is_long_welcome_rate_limit,
welcome_refusal_from_headers. Claimed siblings are SEAMED (never re-claimed):
  _safe_float                 -> safe_float(value, default)
  _parse_buckets_from_headers -> parse_buckets_from_headers(headers)
  lower_headers               -> lower_headers(headers)
  parse_retry_after_seconds   -> parse_retry_after_seconds(value)

Fail-closed: 0 cases => exit 1; a missing/exhausted script RETURNS a Raise spec
(recorded before raising) so the trace matches the C dispatcher. BaseException
caught per case -> __raise__ spec, st={}. The three ported functions have no
except site, so every scripted exception -- including the full builtin
hierarchy's BaseException trio, CancelledError and both *Group kinds -- passes
through unchanged (the original spec round-trips).
Summary: 'ORACLE nous_rate_guard_flow2: N cases, M mismatches'.
"""
import ast
import builtins
import json
import os
import sys
import types
import textwrap
import __future__
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "nous_rate_guard.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

SRC = str(QUARRY / "agent" / "nous_rate_guard.py")
ANN_FLAG = __future__.annotations.compiler_flag

FUNCS = ["_is_exhausted", "is_long_welcome_rate_limit", "welcome_refusal_from_headers"]

# live name -> seam name (all four are dependencies, none ported here)
SEAM_GLOBALS = {
    "_safe_float": "safe_float",
    "_parse_buckets_from_headers": "parse_buckets_from_headers",
    "lower_headers": "lower_headers",
    "parse_retry_after_seconds": "parse_retry_after_seconds",
}
WANT_CONST = ["_MIN_RESET_FOR_BREAKER_SECONDS", "WELCOME_LONG_WAIT_SECONDS"]

CURRENT = {"rec": None}
_MISSING = object()


def _stub_fn(seam):
    def f(*a, **k):
        return CURRENT["rec"](seam)(*a, **k)
    return f


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


# ---- FULL builtin exception hierarchy, discovered from the target interpreter
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
        # Keep the group's own kind: BaseExceptionGroup wants a BaseException-only
        # sub (an Exception sub would auto-return an ExceptionGroup); ExceptionGroup
        # wants Exception subs.
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
    # synthetic Exception subclass (unknown kind)
    return type(kind, (Exception,), {})(msg)


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


def norm_value(v):
    if isinstance(v, BaseException):
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


def proj(name, a, k):
    if name == "safe_float":
        return {"value": a[0], "default": a[1] if len(a) > 1 else None}
    if name == "parse_buckets_from_headers":
        return {"headers": a[0]}
    if name == "lower_headers":
        return {"headers": a[0]}
    if name == "parse_retry_after_seconds":
        return {"value": a[0]}
    return {**{f"arg{i}": v for i, v in enumerate(a)}, **k}


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.script = Script(inp.get("io"))

    def recorder(self, name):
        ctx = self

        def call(*a, **k):
            args = norm_args(proj(name, a, k))
            raw = ctx.script.next(name)
            if isinstance(raw, Raise):
                ctx.trace.append({"ev": name, "args": args, "ret": raw.spec})
                raise raw.exc
            ctx.trace.append({"ev": name, "args": args, "ret": norm_value(raw)})
            return raw
        return call


# ---- live namespace seeding ----
NS = {}
REAL = {}


def _exec_seg(src, node, ns, filename):
    seg = textwrap.dedent(ast.get_source_segment(src, node))
    code = compile(seg, filename, "exec", flags=ANN_FLAG, dont_inherit=True)
    exec(code, ns)


def load_ns():
    global NS, REAL
    mod = types.ModuleType("nrg2_live")
    sys.modules["nrg2_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "nrg2_live",
        "Any": object, "Optional": object, "Mapping": object,
    })
    src = Path(SRC).read_text(encoding="utf-8")
    tree = ast.parse(src)
    # module constants first (the two thresholds the ported funcs read)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            names = {t.id for t in tgts if isinstance(t, ast.Name)}
            if names & set(WANT_CONST):
                _exec_seg(src, node, ns, SRC)
    # target defs
    found = set()
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in FUNCS:
            _exec_seg(src, node, ns, SRC)
            found.add(node.name)
    missing = set(FUNCS) - found
    if missing:
        raise RuntimeError("missing target defs: %s" % sorted(missing))
    # claimed sibling dependencies seamed
    for live, seam in SEAM_GLOBALS.items():
        ns[live] = _stub_fn(seam)
    # audit: every name the drivers read is bound
    for nm in FUNCS + list(SEAM_GLOBALS) + WANT_CONST:
        if nm not in ns:
            raise RuntimeError("seed missing: " + nm)
    REAL = {nm: ns[nm] for nm in FUNCS}


# ---- per-case drive ----
def drive(func, inp):
    g = inp.get
    if func == "_is_exhausted":
        return NS[func](remaining=g("remaining"), reset=g("reset")), {}
    if func == "is_long_welcome_rate_limit":
        return NS[func](g("error_context")), {}
    if func == "welcome_refusal_from_headers":
        return NS[func](g("headers"), body_wait=g("body_wait")), {}
    raise RuntimeError("unknown func " + func)


def run_case(func, inp):
    for nm, fn in REAL.items():
        NS[nm] = fn
    ctx = CaseCtx(inp)
    CURRENT["rec"] = ctx.recorder
    try:
        ret, st = drive(func, inp)
        return {"ret": norm_value(ret), "trace": ctx.trace, "st": norm_value(st)}
    except BaseException as e:
        spec = None
        for r in ctx.script.handed:
            if e is r.exc:
                spec = r.spec
                break
        if spec is None:
            spec = {"__raise__": type(e).__name__, "message": str(e)}
        return {"ret": spec, "trace": ctx.trace, "st": {}}


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
            if mism <= 12:
                print("MISMATCH [%s] case %d" % (func, n))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:800]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:800]))
    print("ORACLE nous_rate_guard_flow2: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
