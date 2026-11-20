#!/usr/bin/env python3
"""Oracle for chat_completion_helpers_flow3: 15 verified-clean defs of
agent/chat_completion_helpers.py (scoped down from 79 after scoping away the
64 defs whose input-dependent branches the C port cannot model).

SCOPED to 15 verified-clean defs (75 cases). Reads harness JSONL on stdin,
AST-extracts the LIVE FunctionDefs from the
read-only quarry, drives them with scripted universal seams, and diffs
{ret, trace, st}.  Full builtin exception hierarchy in build_exc(); all
in-function imports, time/threading/uuid and collaborator calls are seams.
Fail-closed: 0 cases => exit 1.
"""
import ast
import builtins
import contextlib
import json
import math
import os
import sys
import textwrap
import types
from pathlib import Path
from types import SimpleNamespace

QUARRY = Path(os.environ.get("HERMES_QUARRY", "C:/Users/eman5/hermes-agent"))
SRC = str(QUARRY / "agent" / "chat_completion_helpers.py")
ANN_FLAG = getattr(__import__("__future__").annotations, "compiler_flag", 0)

FUNCS = [
    "_accept_chat_chunk",
    "_add",
    "_after_first",
    "_anthropic_summary_attempt",
    "_chat_summary_attempt",
    "_chunk_identity",
    "_close_stream_handle",
    "_codex_summary_attempt",
    "_fire_first",
    "_headed",
    "_log_runaway_cut",
    "_model",
    "_stamp_event",
    "_writer_still_current",
    "set_stream_handle",
]

NS = {}
CURRENT = {}
BY_SITE = {}
FREE_NAMES = {}
_MISSING = object()


# ---- full builtin exception hierarchy --------------------------------------
class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


def build_exc(spec):
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    if kind in ("CancelledError", "asyncio.CancelledError"):
        import asyncio
        cls = asyncio.CancelledError
    else:
        cls = getattr(builtins, kind, None)
        if not (isinstance(cls, type) and issubclass(cls, BaseException)):
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


# ---- object doubles / seam recorders ---------------------------------------
class SeamFn:
    """A callable that fires one universal-seam event and returns the script."""
    def __init__(self, name, ctx):
        self.__cp3_name__ = name
        self.__cp3_ctx__ = ctx

    def __call__(self, *a, **k):
        ctx = self.__cp3_ctx__
        args = {**{f"arg{i}": norm_arg(v) for i, v in enumerate(a)},
                **{key: norm_arg(v) for key, v in k.items()}}
        return ctx.fire(self.__cp3_name__, args)


class D:
    """Attribute double over an encoded {"__obj__":{...}} mapping.
    get: an attribute that exists returns its decoded value; a missing one
    raises AttributeError (so getattr(obj,k,default) works)."""
    def __init__(self, mapping, ctx):
        object.__setattr__(self, "__cp3_ctx__", ctx)
        for key, value in mapping.items():
            object.__setattr__(self, key, decode(value, ctx))

    def __getattr__(self, name):
        if name.startswith("__"):
            raise AttributeError(name)
        return SeamFn(name, self.__cp3_ctx__)


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.script = Script(inp.get("io"))

    def fire(self, name, args):
        raw = self.script.next(name)
        if isinstance(raw, Raise):
            self.trace.append({"ev": name, "ret": raw.spec})
            raise raw.exc
        self.trace.append({"ev": name, "ret": norm_value(raw)})
        return raw


def decode(v, ctx, refs=None):
    if isinstance(v, dict):
        if v.get("__ref__") is not None and refs is not None:
            return refs[v["__ref__"]]
        if v.get("__seam__"):
            return SeamFn(v["__seam__"], ctx)
        if v.get("__obj__") is not None:
            return D(v["__obj__"], ctx)
        return {k: decode(x, ctx, refs) for k, x in v.items()}
    if isinstance(v, list):
        return [decode(x, ctx, refs) for x in v]
    return v


def encode(v):
    if isinstance(v, D):
        return {"__obj__": {k: encode(x) for k, x in vars(v).items()
                            if k != "__cp3_ctx__"}}
    if isinstance(v, SeamFn):
        return {"__seam__": v.__cp3_name__}
    if callable(v) and not isinstance(v, type):
        return {"__callable__": getattr(v, "__name__", "fn")}
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, dict):
        return {k: encode(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [encode(x) for x in v]
    if isinstance(v, set):
        return sorted(encode(x) for x in v)
    if isinstance(v, float) and v == int(v):
        return int(v)
    return v


def norm_value(v):
    return encode(v)


def norm_arg(v):
    return encode(v)


# ---- logger doubles ---------------------------------------------------------
class LoggerFn:
    def __init__(self, name, ctx):
        self.name = name
        self.ctx = ctx

    def __call__(self, fmt, *a, **k):
        d = {"fmt": fmt, "args": list(a)}
        d.update(k)
        return self.ctx.fire(self.name, d)


class RecLogger:
    def __getattr__(self, lvl):
        if lvl in ("info", "warning", "debug", "error", "exception", "critical"):
            return CURRENT["rec"]("logger." + lvl)
        raise AttributeError(lvl)


# ---- live namespace seeding -------------------------------------------------
def _all_funcdefs(node):
    for ch in ast.iter_child_nodes(node):
        if isinstance(ch, (ast.FunctionDef, ast.AsyncFunctionDef)):
            yield ch
        yield from _all_funcdefs(ch)


def _free_names(fdef):
    """Names loaded anywhere in fdef that are not its params / local binds."""
    bound = set()
    for a in fdef.args.args + fdef.args.kwonlyargs + fdef.args.posonlyargs:
        bound.add(a.arg)
    if fdef.args.vararg:
        bound.add(fdef.args.vararg.arg)
    if fdef.args.kwarg:
        bound.add(fdef.args.kwarg.arg)
    for n in ast.walk(fdef):
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef, ast.Lambda)):
            for a in n.args.args + n.args.kwonlyargs + n.args.posonlyargs:
                bound.add(a.arg)
            if n.args.vararg:
                bound.add(n.args.vararg.arg)
            if n.args.kwarg:
                bound.add(n.args.kwarg.arg)
        if isinstance(n, ast.Name) and isinstance(n.ctx, (ast.Store, ast.Del)):
            bound.add(n.id)
        if isinstance(n, (ast.Import, ast.ImportFrom)):
            for al in n.names:
                bound.add(al.asname or al.name.split(".")[0])
    out = []
    for n in ast.walk(fdef):
        if isinstance(n, ast.Name) and isinstance(n.ctx, ast.Load) and n.id not in bound:
            if n.id not in out:
                out.append(n.id)
    return out


def _strip_nonlocal(node):
    for n in ast.walk(node):
        if hasattr(n, "body") and isinstance(n.body, list):
            n.body[:] = [s for s in n.body if not isinstance(s, ast.Nonlocal)]
    return node


def load_ns():
    global NS
    mod = types.ModuleType("cch3f_live")
    sys.modules["cch3f_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "cch3f_live",
        "contextlib": contextlib,
        "math": math,
        "sys": sys,
        "os": os,
        "logger": RecLogger(),
        "logging": RecLogger(),
        "SimpleNamespace": SimpleNamespace,
        "Exception": Exception,
        "BaseException": BaseException,
        "RuntimeError": RuntimeError,
        "ValueError": ValueError,
        "TypeError": TypeError,
        "TimeoutError": TimeoutError,
        "KeyError": KeyError,
        "AttributeError": AttributeError,
        "InterruptedError": InterruptedError,
        "StopIteration": StopIteration,
    })
    src = Path(SRC).read_text(encoding="utf-8")
    tree = ast.parse(src)
    found = set()
    for fdef in _all_funcdefs(tree):
        if fdef.name not in FUNCS:
            continue
        seg = ast.get_source_segment(src, fdef) or ""
        seg = textwrap.dedent(seg)
        lines = seg.splitlines()
        while lines and lines[0].lstrip().startswith("@"):
            lines.pop(0)
        seg = "\n".join(lines)
        node = ast.parse(seg)
        fn_node = next(n for n in ast.walk(node)
                       if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)))
        _strip_nonlocal(fn_node)
        code = compile(ast.Module(body=[fn_node], type_ignores=[]), SRC, "exec",
                       flags=ANN_FLAG, dont_inherit=True)
        exec(code, ns)
        byname = BY_SITE.setdefault(fdef.name, [])
        byname.append((fdef.lineno, ns[fdef.name]))
        FREE_NAMES.setdefault(fdef.name, set()).update(_free_names(fn_node))
        if fdef.name not in found:
            found.add(fdef.name)
    missing = set(FUNCS) - found
    if missing:
        raise RuntimeError("missing target defs: %s" % sorted(missing))
    return ns


def _seed_module(name, ctx):
    """A sys.modules stub whose attributes are seam recorders."""
    m = types.ModuleType(name)
    def ga(attr):
        if attr.startswith("_"):
            if attr.startswith("__"):
                raise AttributeError(attr)
            return lambda *a, **k: None
        return SeamFn(attr, ctx)
    m.__getattr__ = ga
    sys.modules[name] = m
    return m


def install_import_stubs(ctx):
    names = [
        "agent.bedrock_adapter", "agent.codex_responses_adapter", "agent.native_compaction",
        "tools.schema_sanitizer", "providers", "agent.reasoning_carriers",
        "agent.redact_phone_policy", "hermes_cli.models",
        "hermes_cli.runtime_provider_custom", "hermes_cli.providers",
        "agent.fallback_cooldown", "agent.backend_identity", "agent.context_compressor",
        "agent.conversation_loop", "tools.browser_session_cloud", "agent",
        "agent.plugin_stream_hooks", "agent.anthropic_adapter",
        "agent.agent_runtime_helpers", "agent.relay_llm", "copy", "time",
        "threading", "uuid", "httpx",
    ]
    for n in names:
        _seed_module(n, ctx)
    # time/threading/uuid seams use dotted names to avoid collisions
    for mod, attrs in {
        "time": ["time", "monotonic", "sleep", "time_ns"],
        "threading": ["Thread", "Timer", "get_ident"],
        "uuid": ["uuid4"],
    }.items():
        m = sys.modules[mod]
        for a in attrs:
            setattr(m, a, SeamFn(mod + "." + a, ctx))
    # os proxy with a seam environ.get (do NOT replace the real os module globally)
    oproxy = types.SimpleNamespace()
    env = types.SimpleNamespace()
    env.get = SeamFn("os.environ.get", ctx)
    oproxy.environ = env
    NS["os"] = oproxy
    NS["time"] = sys.modules["time"]
    NS["threading"] = sys.modules["threading"]
    NS["uuid"] = sys.modules["uuid"]
    # package parents so 'from agent.x import y' resolves
    for parent in ["agent", "hermes_cli", "tools", "providers"]:
        if parent not in sys.modules:
            _seed_module(parent, ctx)
    for parent, child in [
        ("agent", "bedrock_adapter"), ("agent", "codex_responses_adapter"),
        ("agent", "native_compaction"), ("agent", "reasoning_carriers"),
        ("agent", "redact_phone_policy"), ("agent", "fallback_cooldown"),
        ("agent", "backend_identity"), ("agent", "context_compressor"),
        ("agent", "conversation_loop"), ("agent", "plugin_stream_hooks"),
        ("agent", "anthropic_adapter"), ("agent", "agent_runtime_helpers"),
        ("hermes_cli", "models"), ("hermes_cli", "runtime_provider_custom"),
        ("hermes_cli", "providers"), ("tools", "schema_sanitizer"),
        ("tools", "browser_session_cloud"),
    ]:
        p = sys.modules.get(parent)
        if p is not None:
            setattr(p, child, sys.modules.get(parent + "." + child))


def _resolve_site(func):
    if "@" in func:
        nm, ln = func.rsplit("@", 1)
        for line, fn in BY_SITE.get(nm, []):
            if str(line) == ln:
                return fn
        raise RuntimeError("no def at site " + func)
    return NS[func]


def run_case(rec):
    func = rec.get("func")
    inp = rec.get("in") or {}
    ctx = CaseCtx(inp)
    CURRENT["rec"] = ctx.recorder = lambda name: LoggerFn(name, ctx)
    install_import_stubs(ctx)
    fn = _resolve_site(func)
    # free-name default seams for this def
    fn_key = func.rsplit("@", 1)[0] if "@" in func else func
    for nm in FREE_NAMES.get(fn_key, ()):
        if nm in NS or hasattr(builtins, nm):
            continue
        if nm in ("sys", "os", "json", "re", "math", "contextlib", "copy",
                  "time", "threading", "uuid", "httpx", "logging", "logger"):
            continue
        if nm in ("EmptyStreamError", "ProviderStreamError", "ApiStatusError"):
            NS[nm] = type(nm, (Exception,), {})
        else:
            NS[nm] = SeamFn(nm, ctx)
    # per-case global overrides (free vars / constants)
    for k, v in (inp.get("globals") or {}).items():
        NS[k] = decode(v, ctx, objs) if False else decode(v, ctx)
    trace_mask = inp.get("trace_mask")
    objs = {k: decode(v, ctx) for k, v in (inp.get("objs") or {}).items()}
    args = [decode(v, ctx, objs) for v in (inp.get("args") or [])]
    kwargs = {k: decode(v, ctx, objs) for k, v in (inp.get("kwargs") or {}).items()}
    try:
        ret = fn(*args, **kwargs)
        tr = ctx.trace if trace_mask is None else [e for e in ctx.trace if e["ev"] in trace_mask]
        out = {"ret": encode(ret), "trace": tr, "st": {k: encode(v) for k, v in objs.items()}}
    except BaseException as e:
        spec = None
        for r in ctx.script.handed:
            if e is r.exc:
                spec = r.spec
                break
        if spec is None:
            spec = {"__raise__": type(e).__name__, "message": str(e)}
        tr = ctx.trace if trace_mask is None else [e for e in ctx.trace if e["ev"] in trace_mask]
        out = {"ret": spec, "trace": tr, "st": {k: encode(v) for k, v in objs.items()}}
    return out


def main():
    load_ns()
    n = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        want = run_case(rec)
        got = {"ret": rec.get("ret"), "trace": rec.get("trace"), "st": rec.get("st")}
        got = norm_value(got)
        if got != want:
            mism += 1
            if mism <= 15:
                print("MISMATCH [%s] case %d" % (rec.get("func"), n))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:700]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:700]))
    print("ORACLE chat_completion_helpers_flow3: %d cases, %d mismatches" % (n, mism))
    if n == 0 or mism:
        sys.exit(1)
    sys.exit(0)


if __name__ == "__main__":
    main()
