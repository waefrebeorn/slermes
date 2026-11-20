#!/usr/bin/env python3
"""Oracle for the FLOW + leaf port of the UNCLAIMED route-identity remainder
(src/cli/route_identity_flow.c, rid_* entries):
  hermes_cli/route_identity.py  normalize_route_base_url,
                              should_clear_context_pin,
                              should_clear_context_pin_async
  utils/urls.py                 _parse_base_url, _hostname_of,
                              base_url_hostname, base_url_path,
                              base_url_origin, same_base_url_origin,
                              base_url_host_matches

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}),
recomputes the LIVE reference from rec["in"] with scripted universal-seam
stubs, and compares the ordered trace, the return, and the state projection
({} everywhere -- all ten entries are stateless). Live defs are AST-extracted
(py3.14 quarry venv). The claimed route_identity siblings
(provider_owns_route, drop_stale_model_route, drop_credentials_of_other_route,
src/agent/fallback_cooldown_flow.c) are NOT called by any entry here, so they
need no seam. The one seamed collaborator is agent.agent_init.
_context_route_mismatch (in-function import; import_ok=false halts the import
via sys.modules None -> ImportError swallowed by `except Exception` -> True).
Ported siblings stay REAL so composition is exercised (async -> sync pin;
hostname/path/origin -> _parse_base_url (+ _hostname_of); same_origin ->
origin x2 with short-circuit; host_matches -> hostname). asyncio.to_thread
runs its callable synchronously with no trace of its own (the
gateway_inbound_flow2 convention); the stdlib urlsplit/urlunsplit/urlparse
stay REAL on the live side -- the C transcription is what the diff checks.

Fail-closed: 0 cases => exit 1; a missing/exhausted script RETURNS a Raise
spec (recorded before raising) so the trace matches the C dispatcher; the
pin's `except Exception` swallows it into True. BaseException caught per case
-> __raise__ spec, st={}; the BaseException-only kinds (SystemExit,
KeyboardInterrupt, GeneratorExit, CancelledError) PASS THROUGH the pin's
`except Exception` site.
Summary: 'ORACLE route_identity_flow: N cases, M mismatches'.

Domain (documented in the port header): ASCII text, no NUL bytes (libjson
C-string storage); str()-positions carry strings/null/bools/integral numbers;
utils strip()-positions carry any JSON scalar (truthy non-strings raise the
live AttributeError); bracketed hosts / port strings carry no quotes or
backslashes (repr == "'<x>'"); _hostname_of.parsed is null or a 6-string
ParseResult projection; async.args is an array.

Self-test: python tests/oracle_route_identity_flow.py --self-test [fixture]
drives every fixture case through the LIVE extraction (no C involved) and
flags script-gap / unbound-method AttributeError families.
"""
import ast
import asyncio
import copy
import faulthandler
import json
import math
import os
import sys
import types
import textwrap
import __future__
from pathlib import Path
from types import SimpleNamespace
from urllib.parse import urlsplit, urlunsplit, urlparse, ParseResult

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "hermes_cli" / "route_identity.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))

RI_SRC = str(QUARRY / "hermes_cli" / "route_identity.py")
UU_SRC = str(QUARRY / "utils" / "urls.py")

ANN_FLAG = __future__.annotations.compiler_flag

FUNCS = [
    "normalize_route_base_url", "should_clear_context_pin",
    "should_clear_context_pin_async",
    "_parse_base_url", "_hostname_of", "base_url_hostname", "base_url_path",
    "base_url_origin", "same_base_url_origin", "base_url_host_matches",
]

# in-function imports: module -> {attr: seam name}
SEAM_IMPORTS = {
    "agent.agent_init": {"_context_route_mismatch": "context_route_mismatch"},
}

CURRENT = {"rec": None}
_MISSING = object()
_REAL_CANCELLED = asyncio.CancelledError


def _stub_fn(seam):
    def f(*a, **k):
        return CURRENT["rec"](seam)(*a, **k)
    return f


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


def build_exc(spec):
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    base = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}
    builtin = {"TypeError": TypeError, "ValueError": ValueError,
               "RuntimeError": RuntimeError, "AttributeError": AttributeError,
               "KeyError": KeyError, "TimeoutError": TimeoutError,
               "Exception": Exception, "OSError": OSError,
               "StopIteration": StopIteration, "ArithmeticError": ArithmeticError,
               "LookupError": LookupError, "ImportError": ImportError,
               "NotImplementedError": NotImplementedError}
    if kind in base:
        cls = base[kind]
    elif kind in builtin:
        cls = builtin[kind]
    elif kind == "CancelledError":
        cls = _REAL_CANCELLED
    else:
        cls = type(kind, (Exception,), {})
    return cls(msg)


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
    if isinstance(v, SimpleNamespace):
        return {k: norm_value(x) for k, x in vars(v).items()}
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, dict):
        return {k: norm_value(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_value(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted(norm_value(x) for x in v)
    if isinstance(v, float):
        if math.isfinite(v) and v == int(v):
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
    if name == "context_route_mismatch":
        return {"configured_base_url": a[0], "active_base_url": a[1],
                "configured_provider": a[2], "active_provider": a[3]}
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
    seg = ast.get_source_segment(src, node)
    seg = textwrap.dedent(seg)
    code = compile(seg, filename, "exec", flags=ANN_FLAG, dont_inherit=True)
    exec(code, ns)


class _AsyncioStub:
    """sys.modules['asyncio'] double: to_thread runs its callable
    synchronously (no trace); CancelledError stays the REAL class so
    build_exc and the live except-site identity agree."""
    CancelledError = _REAL_CANCELLED

    @staticmethod
    async def to_thread(fn, *a, **k):
        return fn(*a, **k)


def load_ns():
    global NS, REAL
    mod = types.ModuleType("rid_live")
    sys.modules["rid_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "rid_live",
        "urlsplit": urlsplit, "urlunsplit": urlunsplit, "urlparse": urlparse,
        "Any": object, "Optional": object,
    })
    ri_src = Path(RI_SRC).read_text(encoding="utf-8")
    uu_src = Path(UU_SRC).read_text(encoding="utf-8")
    ri_tree = ast.parse(ri_src)
    uu_tree = ast.parse(uu_src)
    found = set()
    for node in ri_tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in FUNCS:
            _exec_seg(ri_src, node, ns, RI_SRC)
            found.add(node.name)
    for node in uu_tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in FUNCS:
            _exec_seg(uu_src, node, ns, UU_SRC)
            found.add(node.name)
    missing = set(FUNCS) - found
    if missing:
        raise RuntimeError("missing target defs: %s" % sorted(missing))
    # in-function import stubs. OVERWRITE the parent packages (not setdefault):
    # the quarry is importable in the venv, so a stale real package would let
    # live submodule imports leak past the stubs.
    agent_pkg = types.ModuleType("agent")
    agent_pkg.__path__ = []
    sys.modules["agent"] = agent_pkg
    for modname, attrs in SEAM_IMPORTS.items():
        m = types.ModuleType(modname)
        for attr, seam in attrs.items():
            setattr(m, attr, _stub_fn(seam))
        sys.modules[modname] = m
        parts = modname.split(".")
        if len(parts) == 2:
            setattr(sys.modules[parts[0]], parts[1], m)
    # asyncio double for the async wrapper's in-function import
    sys.modules["asyncio"] = _AsyncioStub
    for nm in FUNCS:
        if nm not in ns:
            raise RuntimeError("seed missing: " + nm)
    REAL = {nm: ns[nm] for nm in FUNCS}


def _setup_pin_imports(inp):
    if inp.get("import_ok", True):
        m = types.ModuleType("agent.agent_init")
        m._context_route_mismatch = _stub_fn("context_route_mismatch")
        sys.modules["agent.agent_init"] = m
        sys.modules["agent"].agent_init = m
    else:
        sys.modules["agent.agent_init"] = None


def _drive_coro(coro):
    """The to_thread double never suspends: one send completes the coroutine.
    A suspension would be outside the fixture domain."""
    try:
        coro.send(None)
    except StopIteration as e:
        return e.value
    raise RuntimeError("coroutine suspended outside fixture domain")


def drive(func, inp):
    """Return (ret, st). Raises on live exception (caught by run_case)."""
    if func == "normalize_route_base_url":
        return NS[func](inp.get("base_url")), {}
    if func == "should_clear_context_pin":
        _setup_pin_imports(inp)
        return NS[func](inp.get("configured_model"), inp.get("active_model"),
                        inp.get("configured_base_url"), inp.get("active_base_url"),
                        inp.get("configured_provider"), inp.get("active_provider")), {}
    if func == "should_clear_context_pin_async":
        _setup_pin_imports(inp)
        args = inp.get("args") or []
        return _drive_coro(NS[func](*args)), {}
    if func == "_parse_base_url":
        return NS[func](inp.get("base_url")), {}
    if func == "_hostname_of":
        parsed = inp.get("parsed")
        if isinstance(parsed, list):
            parsed = ParseResult(*parsed)
        return NS[func](parsed), {}
    if func in ("base_url_hostname", "base_url_path", "base_url_origin"):
        return NS[func](inp.get("base_url")), {}
    if func == "same_base_url_origin":
        return NS[func](inp.get("a"), inp.get("b")), {}
    if func == "base_url_host_matches":
        return NS[func](inp.get("base_url"), inp.get("domain")), {}
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


def self_test(path):
    """Drive every fixture case through the LIVE extraction (no C). Flag
    script-gap / unbound-method AttributeError families: an AttributeError
    whose message names a ported def or seam stub is a binding bug, not a
    legitimate fixture raise."""
    faulthandler.dump_traceback_later(120, exit=True)
    load_ns()
    names = set(FUNCS) | set(SEAM_IMPORTS) | {"context_route_mismatch"}
    cases = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                cases.append(json.loads(line))
    bad = 0
    per_func = {}
    for i, rec in enumerate(cases):
        func = rec.get("func")
        inp = rec.get("in") or {}
        per_func[func] = per_func.get(func, 0) + 1
        if func not in FUNCS:
            print("SELFTEST case %d: func %r not in extraction map" % (i + 1, func))
            bad += 1
            continue
        try:
            out = run_case(func, inp)
        except BaseException as e:
            print("SELFTEST case %d [%s]: drive escaped: %s: %s"
                  % (i + 1, func, type(e).__name__, e))
            bad += 1
            continue
        ret = out["ret"]
        if isinstance(ret, dict) and ret.get("__raise__") == "AttributeError":
            msg = ret.get("message", "")
            if any(("'" + nm + "'") in msg for nm in names):
                print("SELFTEST case %d [%s]: binding AttributeError: %s"
                      % (i + 1, func, msg))
                bad += 1
    missing_funcs = set(FUNCS) - set(per_func)
    if missing_funcs:
        print("SELFTEST: funcs absent from fixtures: %s" % sorted(missing_funcs))
        bad += 1
    for fn in FUNCS:
        if per_func.get(fn, 0) < 5:
            print("SELFTEST: func %s has %d cases (<5)" % (fn, per_func.get(fn, 0)))
            bad += 1
    print("SELFTEST route_identity_flow: %d cases, %d problems, per-func %s"
          % (len(cases), bad, json.dumps(per_func, sort_keys=True)))
    faulthandler.cancel_dump_traceback_later()
    sys.exit(1 if bad else 0)


def main():
    if len(sys.argv) >= 2 and sys.argv[1] == "--self-test":
        path = sys.argv[2] if len(sys.argv) > 2 else str(
            _HERE.parent / "fixtures" / "route_identity_flow.jsonl")
        self_test(path)
        return
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
        got = {"ret": rec.get("ret"), "trace": rec.get("trace"), "st": rec.get("st")}
        got = norm_value(got)
        if got != want:
            mism += 1
            if mism <= 12:
                print("MISMATCH [%s] case %d" % (func, n))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:600]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:600]))
    print("ORACLE route_identity_flow: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()