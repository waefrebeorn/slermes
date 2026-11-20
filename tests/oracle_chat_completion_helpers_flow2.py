#!/usr/bin/env python3
"""Oracle for chat_completion_helpers_flow2 (the UNCLAIMED remainder of
agent/chat_completion_helpers.py: _StreamingCall shared small helpers).

Reads harness JSONL on stdin, drives the LIVE FunctionDefs AST-extracted from
the read-only quarry with scripted universal seams, and diffs {ret, trace, st}.
Full builtin exception hierarchy in build_exc(); contextlib.suppress(Exception)
is exercised by the _quiet cases (Exception subclasses suppressed, BaseException
family propagated). Fail-closed: 0 cases => exit 1.
"""
import ast
import builtins
import contextlib
import json
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
    "_live_route", "_visible_text_delivered", "_route_switched_under_request",
    "_set_managed_stream", "_start_stream_attempt", "_cancel_current_stream_attempt",
    "_stream_attempt_is_active", "_stream_attempt_was_cancelled",
    "_mark_finish_seen", "_quiet",
]

NS = {}
CURRENT = {}
_MISSING = object()


# ---- full builtin exception hierarchy --------------------------------------
class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


def build_exc(spec):
    """Resolve any builtin exception name; unknown names subclass Exception
    (matching the C side, which only treats the BaseException family as
    escaping contextlib.suppress(Exception))."""
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


def norm_value(v):
    if isinstance(v, SimpleNamespace):
        return {k: norm_value(x) for k, x in vars(v).items()}
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, set):
        return sorted(norm_value(x) for x in v)
    if isinstance(v, dict):
        return {k: norm_value(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_value(x) for x in v]
    if isinstance(v, float) and v == int(v):
        return int(v)
    return v


def norm_args(v):
    v = norm_value(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    if isinstance(v, list):
        return [norm_args(x) for x in v]
    return v


def proj(name, a, k):
    if name == "fn":
        return list(a)
    if name.startswith("logger."):
        return {"fmt": a[0], "args": list(a[1:])}
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


class RecLogger:
    def _log(self, lvl):
        def f(fmt, *args):
            return CURRENT["rec"]("logger." + lvl)(fmt, *args)
        return f

    def __getattr__(self, name):
        if name in ("info", "warning", "debug", "error"):
            return self._log(name)
        raise AttributeError(name)


def ns_recursive(v):
    if isinstance(v, dict):
        return SimpleNamespace(**{k: ns_recursive(x) for k, x in v.items()})
    if isinstance(v, list):
        return [ns_recursive(x) for x in v]
    return v


def make_self(inp):
    """SimpleNamespace self from the case's `self` dict. agent becomes an
    attribute object; stream_attempt_state.cancelled becomes a real set;
    _live_route is bound to the extracted live method."""
    sd = inp.get("self") or {}
    self = SimpleNamespace()
    for k, v in sd.items():
        if k == "agent" and isinstance(v, dict):
            v = ns_recursive(v)
        elif k == "_request_route" and isinstance(v, list):
            v = tuple(v)  # live _live_route() returns a tuple, not a list
        elif k == "stream_attempt_state" and isinstance(v, dict):
            v = dict(v)
            v["cancelled"] = set(v.get("cancelled") or [])
        self.__dict__[k] = v
    if "_live_route" not in self.__dict__:
        self.__dict__["_live_route"] = lambda: NS["_live_route"](self)
    if "stream_attempt_lock" not in self.__dict__:
        # threading.Lock is only a no-op `with` context in these helpers.
        self.__dict__["stream_attempt_lock"] = _NoopLock()
    return self


class _NoopLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


# ---- live namespace seeding -------------------------------------------------
def _all_funcdefs(node):
    for ch in ast.iter_child_nodes(node):
        if isinstance(ch, (ast.FunctionDef, ast.AsyncFunctionDef)):
            yield ch
        yield from _all_funcdefs(ch)


def load_ns():
    global NS
    mod = types.ModuleType("cch2f2_live")
    sys.modules["cch2f2_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "cch2f2_live",
        "contextlib": contextlib,
        "logger": RecLogger(),
        "SimpleNamespace": SimpleNamespace,
    })
    src = Path(SRC).read_text(encoding="utf-8")
    tree = ast.parse(src)
    found = set()
    for fdef in _all_funcdefs(tree):
        if fdef.name not in FUNCS or fdef.name in found:
            continue
        seg = ast.get_source_segment(src, fdef) or ""
        seg = textwrap.dedent(seg)
        lines = seg.splitlines()
        while lines and lines[0].lstrip().startswith("@"):
            lines.pop(0)
        seg = "\n".join(lines)
        code = compile(seg, SRC, "exec", flags=ANN_FLAG, dont_inherit=True)
        exec(code, ns)
        found.add(fdef.name)
    missing = set(FUNCS) - found
    if missing:
        raise RuntimeError("missing target defs: %s" % sorted(missing))
    return ns


def drive(func, inp):
    """Return (ret, st); raise on live exception (caught by run_case)."""
    st = {}
    if func == "_live_route":
        return NS[func](make_self(inp)), st
    if func == "_visible_text_delivered":
        return NS[func](make_self(inp)), st
    if func == "_route_switched_under_request":
        return NS[func](make_self(inp)), st
    if func == "_set_managed_stream":
        self = make_self(inp)
        stream = inp.get("stream")
        ret = NS[func](self, stream)
        st["stream"] = self.managed_stream_holder["stream"]
        return ret, st
    if func == "_start_stream_attempt":
        self = make_self(inp)
        ret = NS[func](self)
        st["current"] = self.stream_attempt_state["current"]
        st["provider_tool_in_flight"] = self.provider_tool_in_flight["yes"]
        st["prompt_progress"] = self.prompt_progress["progress"]
        st["partial_tool_names"] = self.result["partial_tool_names"]
        return ret, st
    if func == "_cancel_current_stream_attempt":
        self = make_self(inp)
        ret = NS[func](self, inp.get("reason"))
        st["cancelled"] = self.stream_attempt_state["cancelled"]
        return ret, st
    if func == "_stream_attempt_is_active":
        return NS[func](make_self(inp), inp.get("stream_attempt_id")), st
    if func == "_stream_attempt_was_cancelled":
        return NS[func](make_self(inp), inp.get("stream_attempt_id")), st
    if func == "_mark_finish_seen":
        diag = inp.get("diag")
        ret = NS[func](diag, inp.get("finish_reason"))
        st["diag"] = diag
        return ret, st
    if func == "_quiet":
        fn = CURRENT["rec"]("fn")
        ret = NS[func](fn, *(inp.get("fn_args") or []))
        return ret, st
    raise RuntimeError("unknown func " + func)


def run_case(func, inp):
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
    print("ORACLE chat_completion_helpers_flow2: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
