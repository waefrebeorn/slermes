#!/usr/bin/env python3
"""Oracle for src/tools/delegate_progress_flow2.c — drives the LIVE unclaimed
remainder of tools/delegate_tool_progress.py under IDENTICAL scripted universal
seams. The 6 progress defs claimed by tool_dispatch_flow2 (_quiet,
_safe_progress, describe_subagent_failure, _format_duration,
_print_completion_line, _short) plus the delegate_* / delegate_tool_gaps claims
stay SEAMED (never re-ported). This oracle covers the 11 unclaimed defs:

  _normalize_event, child_task_message,
  _ChildProgressRelay.{__init__, _identity_kwargs, _prefix, _tree_line,
    _flush, _on_spawn_requested, _on_text, _on_start, _on_thinking}

Reads harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes the
LIVE reference from rec["in"], and compares ordered trace / return / state.
Live defs are AST-extracted; claimed siblings (_quiet CM, _short, _batch_prefix,
register_stable_prefix) and the relay's composed methods (_relay/_prefix/
_tree_line) are recording seams fed the SAME scripts the C harness feeds.

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
(recorded as the event ret before propagation); 0 cases => exit 1. A scripted
BaseException (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError)
propagates past every except Exception guard (real builtin classes). Per-case
BaseException catch -> __raise__ spec; scripted raises round-trip BARE via
identity match against Script.handed.

Value model: attribute doubles {"__attr__": ...} <-> SimpleNamespace (interned
by __id__; state echo RE-WRAPS, injected seam methods excluded); callables
project to {"__callable__": seam}; DelegateEvent (str Enum) projects to its
.value string; integral floats normalize to ints on both sides.

Summary: 'ORACLE delegate_progress_flow2: N cases, M mismatches'.
"""
import ast
import builtins
import copy
import enum
import json
import math
import os
import sys
import types
from contextlib import contextmanager
from pathlib import Path
from types import SimpleNamespace
from typing import Any, Callable, Dict, List, Optional

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "tools" / "delegate_tool_progress.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))

FUNCS = [
    "_normalize_event", "child_task_message",
    "_ChildProgressRelay.__init__", "_ChildProgressRelay._identity_kwargs",
    "_ChildProgressRelay._prefix", "_ChildProgressRelay._tree_line",
    "_ChildProgressRelay._flush", "_ChildProgressRelay._on_spawn_requested",
    "_ChildProgressRelay._on_text", "_ChildProgressRelay._on_start",
    "_ChildProgressRelay._on_thinking",
]

CURRENT = {"ctx": None, "rec": None, "ids": {}, "stwin": {}}
NS = {}
RelayClass = None

# ---- FULL builtin exception table (py3.14 gate interpreter) ----
EXC_CLASSES = {}
for _n in dir(builtins):
    _o = getattr(builtins, _n)
    if isinstance(_o, type) and issubclass(_o, BaseException):
        EXC_CLASSES[_n] = _o
try:
    import asyncio as _asyncio
    EXC_CLASSES["CancelledError"] = _asyncio.CancelledError
except Exception:
    pass


def build_exc(spec):
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    if kind == "KeyError":
        try:
            return cls(ast.literal_eval(msg))
        except (ValueError, SyntaxError):
            return cls(msg)
    if kind in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    return cls(msg)


def _stub_fn(seam):
    def f(*a, **k):
        return CURRENT["rec"](seam)(*a, **k)
    f.__dp2_seam__ = seam
    return f


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


_MISSING = object()


def _unwrap_script(val):
    if isinstance(val, dict) and "__raise__" in val:
        return Raise(val, build_exc(val))
    if isinstance(val, dict) and val.get("__script__") is True:
        return val.get("ret")
    return val


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
        r = _unwrap_script(val)
        if isinstance(r, Raise):
            self.handed.append(r)
        return r

    def _fail(self, name):
        spec = {"__raise__": "RuntimeError", "message": "no script for " + name}
        r = Raise(spec, build_exc(spec))
        self.handed.append(r)
        return r


def norm_value(v):
    if isinstance(v, enum.Enum) and isinstance(v, str):
        return v.value
    if isinstance(v, SimpleNamespace):
        inj = getattr(v, "__dp2_injected__", set())
        return {k: norm_value(x) for k, x in vars(v).items()
                if not k.startswith("__dp2") and k not in inj}
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, dict):
        return {k: norm_value(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_value(x) for x in v]
    if isinstance(v, float):
        if math.isfinite(v) and v == int(v):
            return int(v)
        return v
    if callable(v):
        return {"__callable__": getattr(v, "__dp2_seam__", True)}
    return v


def norm_state(v):
    if isinstance(v, SimpleNamespace):
        inj = getattr(v, "__dp2_injected__", set())
        inner = {k: norm_state(x) for k, x in vars(v).items()
                 if not k.startswith("__dp2") and k not in inj}
        out = {"__attr__": inner}
        idv = getattr(v, "__dp2_id__", None)
        if idv is not None:
            out["__id__"] = idv
        return out
    if isinstance(v, enum.Enum) and isinstance(v, str):
        return v.value
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, dict):
        return {k: norm_state(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_state(x) for x in v]
    if isinstance(v, float):
        if math.isfinite(v) and v == int(v):
            return int(v)
        return v
    if callable(v):
        return {"__callable__": getattr(v, "__dp2_seam__", True)}
    return v


norm_ret = norm_state


def norm_args(v):
    v = norm_value(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    if isinstance(v, list):
        return [norm_args(x) for x in v]
    return v


def proj(name, a, k):
    if name.startswith("logger."):
        d = {"msg": a[0] if a else "",
             "args": [str(x) if isinstance(x, BaseException) else x for x in a[1:]]}
        if k.get("exc_info"):
            d["exc_info"] = True
        return d
    if name == "register_stable_prefix":
        return {"prefix": a[0] if a else k.get("prefix")}
    if name == "batch_prefix":
        return {"delegation_id": a[0] if a else k.get("delegation_id"),
                "task_index": a[1] if len(a) > 1 else k.get("task_index"),
                "task_count": a[2] if len(a) > 2 else k.get("task_count"),
                "parent_agent": norm_state(k.get("parent_agent"))}
    if name == "prefix":
        return {}
    if name == "relay":
        return {"event_type": a[0] if a else k.get("event_type"),
                "tool_name": a[1] if len(a) > 1 else k.get("tool_name"),
                "preview": a[2] if len(a) > 2 else k.get("preview"),
                "args": a[3] if len(a) > 3 else k.get("args"),
                "kwargs": {kk: vv for kk, vv in k.items()
                           if kk not in ("event_type", "tool_name", "preview", "args")}}
    if name == "tree_line":
        return {"line": a[0] if a else k.get("line")}
    if name == "short":
        return {"text": a[0] if a else k.get("text"), "n": a[1] if len(a) > 1 else k.get("n")}
    if name == "spinner.print_above":
        return {"line": a[0] if a else k.get("line")}
    return {**{("arg%d" % i): v for i, v in enumerate(a)}, **k}


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
            return materialize_result(name, raw)
        call.__dp2_seam__ = name
        return call


class LoggerDouble:
    def _log(self, level, msg, args, kw):
        return CURRENT["rec"]("logger." + level)(msg, *args, **kw)

    def debug(self, msg, *args, **kw):
        return self._log("debug", msg, args, kw)


def _inject(obj, **kw):
    for kk, vv in kw.items():
        setattr(obj, kk, vv)
    inj = getattr(obj, "__dp2_injected__", None)
    if inj is None:
        inj = set()
        obj.__dp2_injected__ = inj
    inj.update(kw.keys())


def materialize_double(j):
    if isinstance(j, dict):
        if "__attr__" in j:
            idv = j.get("__id__")
            if idv is not None and idv in CURRENT["ids"]:
                return CURRENT["ids"][idv]
            inner = j["__attr__"] or {}
            kids = {kk: materialize_double(vv) for kk, vv in inner.items()}
            obj = SimpleNamespace(**kids)
            obj.__dp2_id__ = idv
            obj.__dp2_injected__ = set()
            if idv is not None:
                CURRENT["ids"][idv] = obj
            return obj
        cb = j.get("__callable__")
        if isinstance(cb, str):
            return CURRENT["rec"](cb)
        return {kk: materialize_double(vv) for kk, vv in j.items()}
    if isinstance(j, list):
        return [materialize_double(vv) for vv in j]
    return j


def materialize_result(name, raw):
    if isinstance(raw, dict) and ("__attr__" in raw or "__callable__" in raw):
        return materialize_double(raw)
    return raw


# ---- live namespace seeding (AST-extract from the target module) ----
def _exec_seg(src, node, ns, path, flags):
    import textwrap
    seg = textwrap.dedent(ast.get_source_segment(src, node))
    exec(compile(seg, path, "exec", flags=flags, dont_inherit=True), ns)


def _exec_class_seg(src, node, ns, path, flags):
    lines = src.splitlines()
    start = (node.decorator_list[0].lineno if node.decorator_list else node.lineno) - 1
    seg = "\n".join(lines[start:node.end_lineno])
    exec(compile(seg, path, "exec", flags=flags, dont_inherit=True), ns)


def _install_stub(modname, attrs):
    parts = modname.split(".")
    for i in range(1, len(parts) + 1):
        name = ".".join(parts[:i])
        if name not in sys.modules or not isinstance(sys.modules[name], types.ModuleType):
            m = types.ModuleType(name)
            m.__path__ = []
            sys.modules[name] = m
        if i > 1:
            setattr(sys.modules[".".join(parts[:i - 1])], parts[i - 1], sys.modules[name])
    mod = sys.modules[modname]
    for kk, vv in attrs.items():
        setattr(mod, kk, vv)
    return mod


def load_ns():
    global NS, RelayClass
    import __future__
    flags = __future__.annotations.compiler_flag
    mod = types.ModuleType("dp2_live")
    sys.modules["dp2_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "dp2_live", "enum": enum, "Any": Any, "Optional": Optional,
        "Callable": Callable, "Dict": dict, "List": list,
        "logger": LoggerDouble(),
        # claimed siblings / foreign I/O -> recording seams
        "_short": _stub_fn("short"),
        "_batch_prefix": _stub_fn("batch_prefix"),
    })

    path = str(QUARRY / "tools" / "delegate_tool_progress.py")
    src = Path(path).read_text(encoding="utf-8")
    tree = ast.parse(src)

    # constants: DelegateEvent, _LEGACY_EVENT_MAP, _LIFECYCLE_EVENTS
    for n in tree.body:
        if isinstance(n, ast.ClassDef) and n.name == "DelegateEvent":
            _exec_class_seg(src, n, ns, path, flags)
        elif isinstance(n, (ast.Assign, ast.AnnAssign)):
            tgts = n.targets if isinstance(n, ast.Assign) else [n.target]
            names = {t.id for t in tgts if isinstance(t, ast.Name)}
            if names & {"_LEGACY_EVENT_MAP", "_LIFECYCLE_EVENTS"}:
                _exec_seg(src, n, ns, path, flags)

    # _quiet is a @contextmanager (decorator stripped by segment extraction)
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name == "_quiet":
            _exec_seg(src, n, ns, path, flags)
    ns["_quiet"] = contextmanager(ns["_quiet"])

    # ported module funcs + the relay class
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in ("_normalize_event", "child_task_message"):
            _exec_seg(src, n, ns, path, flags)
        elif isinstance(n, ast.ClassDef) and n.name == "_ChildProgressRelay":
            _exec_class_seg(src, n, ns, path, flags)

    RelayClass = ns["_ChildProgressRelay"]
    _install_stub("agent.prompt_cache_boundary",
                  {"register_stable_prefix": _stub_fn("register_stable_prefix")})
    return ns


def _win(**kw):
    CURRENT["stwin"] = kw
    return kw


def make_self(inp):
    """Materialize state.relay into a self double with composed-method seams."""
    sj = (inp.get("state") or {}).get("relay")
    self = materialize_double(copy.deepcopy(sj)) if sj is not None else SimpleNamespace()
    for m, seam in (("_relay", "relay"), ("_prefix", "prefix"), ("_tree_line", "tree_line")):
        _inject(self, **{m: CURRENT["rec"](seam)})
    return self


def drive(func, inp):
    g = inp.get
    if func == "_normalize_event":
        _win()
        return NS[func](g("event_type")), CURRENT["stwin"]
    if func == "child_task_message":
        _win()
        return NS[func](g("goal"), g("context")), CURRENT["stwin"]
    if func == "_ChildProgressRelay.__init__":
        self = SimpleNamespace()
        RelayClass.__init__(self, g("task_index"), g("goal"), g("spinner"), g("parent_cb"),
                            g("task_count"), g("subagent_id"), g("parent_id"), g("depth"),
                            g("model"), g("toolsets"), g("session_ref"))
        _win(relay=self)
        return None, CURRENT["stwin"]
    if func == "_ChildProgressRelay._identity_kwargs":
        self = make_self(inp); _win(relay=self)
        return RelayClass._identity_kwargs(self), CURRENT["stwin"]
    if func == "_ChildProgressRelay._prefix":
        self = make_self(inp); _win(relay=self)
        return RelayClass._prefix(self), CURRENT["stwin"]
    if func == "_ChildProgressRelay._tree_line":
        self = make_self(inp); _win(relay=self)
        return RelayClass._tree_line(self, g("text")), CURRENT["stwin"]
    if func == "_ChildProgressRelay._flush":
        self = make_self(inp); _win(relay=self)
        return RelayClass._flush(self), CURRENT["stwin"]
    if func in ("_ChildProgressRelay._on_spawn_requested", "_ChildProgressRelay._on_text",
                "_ChildProgressRelay._on_start", "_ChildProgressRelay._on_thinking"):
        self = make_self(inp); _win(relay=self)
        short = func.rsplit(".", 1)[1]
        return getattr(RelayClass, short)(self, g("tool_name"), g("preview"), g("args"), g("kwargs") or {}), CURRENT["stwin"]
    raise RuntimeError("unknown func " + func)


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    CURRENT["rec"] = ctx.recorder
    CURRENT["stwin"] = {}
    CURRENT["ids"] = {}
    try:
        ret, stwin = drive(func, inp)
        return {"ret": norm_ret(ret), "trace": ctx.trace, "st": norm_state(stwin)}
    except BaseException as e:
        spec = None
        for r in ctx.script.handed:
            if e is r.exc:
                spec = r.spec
                break
        if spec is None:
            spec = {"__raise__": type(e).__name__, "message": str(e)}
        return {"ret": spec, "trace": ctx.trace, "st": norm_state(CURRENT["stwin"])}


def main():
    try:
        load_ns()
    except BaseException as e:
        print("PYERR load_ns: %s: %s" % (type(e).__name__, e))
        return 1
    n = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = rec.get("func")
        inp = rec.get("in") or {}
        n += 1
        want = run_case(func, inp)
        got = {"ret": rec.get("ret"), "trace": rec.get("trace"), "st": rec.get("st")}
        got = norm_value(got)
        if got != want:
            mism += 1
            if mism <= 20:
                print("MISMATCH [%s] case %d" % (func, n))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:900]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:900]))
    print("ORACLE delegate_progress_flow2: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        return 1
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
