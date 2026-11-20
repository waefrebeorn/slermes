#!/usr/bin/env python3
"""Oracle for the agent/display.py FLOW2 port (src/agent/display_flow2.py).

Drives the LIVE agent/display.py presentation flows with SCRIPTED I/O: every
foreign sub-call (i18n t, skin engine, shell sanitizer/summarizer, tool-label
registry, prompt_toolkit import, clock, thread factory) and every claimed
sibling (_tail_trunc, _primary_arg_preview) is a recording stub fed the same
scripts the C harness feeds its seams. Compares the DECISION SEQUENCE (ordered
seam calls {"name","args"} in "trace"), the return ("ret") and the echoed
globals ("state").

Live defs are AST-extracted (module constants seeded, Assign AND AnnAssign);
the heavy quarry import chain is never imported. Fail-closed: missing defs or
0 cases => exit 2; a missing/exhausted seam script raises RuntimeError on both
sides. Catch BaseException (scripted SystemExit/KeyboardInterrupt/GeneratorExit/
CancelledError propagate through `except Exception` sites). The exception
factory covers the FULL builtin hierarchy and forces str(exc) == scripted
message so the C side's verbatim {"__raise__", "message"} matches every type.

Representation contract (shared with the C port header):
- tool label double -> {"name","emoji","text","preview"}.
- skin double -> {"spinner": {key: [...]}} (or null / raise).
- _out double -> {"__proxy__": bool} for the patch_stdout proxy check.
- start() state carries running/start_time/thread; mutated in place.
Summary: 'ORACLE display_flow2: N cases, M mismatches'.
"""
import ast
import asyncio
import copy
import json
import math
import os
import re
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
os.environ.setdefault("HOME", str(_REPO.parent.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "display.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "display.py")

PY = {
    "_preview_shell": "_preview_shell",
    "_preview_bridge_call": "_preview_bridge_call",
    "_skin_spinner_list": "_skin_spinner_list",
    "_catalog_thinking_verbs": "_catalog_thinking_verbs",
    "_is_patch_stdout_proxy": "_is_patch_stdout_proxy",
    "start": "start",
    "_clear_line_blanks": "_clear_line_blanks",
}
METHODS = {
    "_skin_spinner_list", "_catalog_thinking_verbs", "_is_patch_stdout_proxy",
    "start", "_clear_line_blanks",
}

import __future__ as _fut
_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    exec(compile(seg(src, node), path, "exec", flags=_FLAGS, dont_inherit=True), ns)


class StdoutProxy:
    """Marker class standing in for prompt_toolkit's StdoutProxy."""


class _ThreadStub:
    def __init__(self, tok, ctx):
        self.tok = tok
        self._ctx = ctx

    def start(self):
        self._ctx.call("thread.start", {"thread": self.tok})


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {
        "__name__": "agent.display",
        "re": re, "os": os, "sys": sys, "json": json, "math": math,
        "logging": __import__("logging"), "threading": __import__("threading"),
        "time": __import__("time"), "types": types,
        # imported collaborators / claimed siblings (seamed per case; placeholders
        # so the class/func exec itself never fails)
        "t": lambda *a, **k: "",
        "_get_skin": lambda *a, **k: None,
        "sanitize_command_for_display": lambda *a, **k: "",
        "summarize_shell_command": lambda *a, **k: "",
        "bridge_tool_labels": lambda *a, **k: [],
        "_primary_arg_preview": lambda *a, **k: None,
        "_tail_trunc": lambda *a, **k: None,
        "logger": _SilentLogger(),
    }
    tree = ast.parse(src)

    def consts():
        for node in tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
                if not all(isinstance(t, ast.Name) for t in tgts):
                    continue
                try:
                    exec_node(src, node, ns, SRC)
                except Exception:
                    pass

    consts()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "KawaiiSpinner":
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    consts()  # re-seed lambda-valued constants after function exec
    missing = []
    for nm in ("_preview_shell", "_preview_bridge_call"):
        if nm not in ns:
            missing.append(nm)
    kaw = ns.get("KawaiiSpinner")
    for m in METHODS:
        if not hasattr(kaw, m):
            missing.append("KawaiiSpinner." + m)
    if missing:
        print(f"FATAL oracle_display_flow2: missing defs: {missing}")
        sys.exit(2)
    return ns


NS = extract()
KAWAII = NS["KawaiiSpinner"]

# ── full builtin exception hierarchy ──────────────────────────────────────
EXC_CLASSES = {"BaseException": BaseException}


def _walk_exc(cls):
    for sub in cls.__subclasses__():
        EXC_CLASSES.setdefault(sub.__name__, sub)
        _walk_exc(sub)


_walk_exc(BaseException)
EXC_CLASSES.setdefault("CancelledError", asyncio.CancelledError)


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    base = EXC_CLASSES.get(kind)
    if not isinstance(base, type) or not issubclass(base, BaseException):
        base = Exception
    # Subclass so type(exc).__name__ == kind while preserving the `except`
    # hierarchy; a custom __new__/#__str__ lets every builtin (including the
    # two-arg ExceptionGroup / asyncio exceptions) take exactly (message),
    # and forces str(exc) == message (KeyError et al. quote by default).
    def _new(cls, message="", *a, **k):
        obj = BaseException.__new__(cls)
        BaseException.__init__(obj, message)
        return obj

    cls = type(kind, (base,), {
        "__new__": _new,
        "__str__": lambda self: (self.args[0] if self.args else ""),
    })
    try:
        return cls(msg)
    except Exception:
        # CPython refuses BaseException.__new__ for ExceptionGroup classes;
        # a plain (correctly named/messaged) Exception still satisfies every
        # scripted-exception comparison (no `except ExceptionGroup` site here).
        plain = type(kind, (Exception,), {
            "__str__": lambda self: (self.args[0] if self.args else ""),
        })
        return plain(msg)


class Ctx:
    def __init__(self, inp):
        self.io = inp.get("io") or {}
        self.cursors = {}
        self.trace = []

    def next_entry(self, name):
        if name not in self.io:
            raise RuntimeError(f"no script for {name}")
        entry = self.io[name]
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                raise RuntimeError(f"script exhausted for {name}")
            self.cursors[name] = idx + 1
            return entry[idx]
        return entry

    def call(self, name, args):
        self.trace.append({"name": name, "args": args})
        entry = self.next_entry(name)
        if isinstance(entry, dict) and entry.get("__script__"):
            if isinstance(entry.get("raise"), dict):
                raise make_exc(entry["raise"])
            return entry.get("ret")
        return entry


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── result materialization (script dict -> live object double) ───────────
def mat_labels(r):
    if isinstance(r, list):
        return [types.SimpleNamespace(**d) if isinstance(d, dict) else d for d in r]
    return r


def mat_skin(r):
    if isinstance(r, dict):
        return types.SimpleNamespace(**r)
    return r


def install_seams(ctx):
    ns = NS
    ns["KawaiiSpinner"] = KAWAII

    # _preview_shell seams
    ns["sanitize_command_for_display"] = lambda s: ctx.call("sanitize_command_for_display", {"s": s})
    ns["summarize_shell_command"] = lambda s: ctx.call("summarize_shell_command", {"s": s})
    # _preview_bridge_call seams
    ns["bridge_tool_labels"] = lambda tool_name, args: mat_labels(
        ctx.call("bridge_tool_labels", {"tool_name": tool_name, "args": args}))
    ns["_primary_arg_preview"] = lambda tool_name, args, max_len: ctx.call(
        "_primary_arg_preview", {"tool_name": tool_name, "args": args, "max_len": max_len})
    # shared
    ns["_tail_trunc"] = lambda text, limit: ctx.call("_tail_trunc", {"text": text, "limit": limit})
    # _skin_spinner_list / _catalog_thinking_verbs seams
    ns["_get_skin"] = lambda: mat_skin(ctx.call("_get_skin", {}))
    ns["t"] = lambda key, **kw: ctx.call("t", {"key": key, **kw})
    # _is_patch_stdout_proxy seam: the from-import fires __getattr__, which
    # records the trace and either raises (ImportError -> False) or binds the
    # marker class.
    def _import():
        ctx.call("StdoutProxy_import", {})
        return StdoutProxy

    pkg = types.ModuleType("prompt_toolkit")
    pkg.__path__ = []
    pm = types.ModuleType("prompt_toolkit.patch_stdout")

    def _patch_getattr(name):
        if name == "StdoutProxy":
            return _import()
        raise AttributeError(name)

    pm.__getattr__ = _patch_getattr
    pkg.patch_stdout = pm
    sys.modules["prompt_toolkit"] = pkg
    sys.modules["prompt_toolkit.patch_stdout"] = pm
    # start() seams
    ns["time"] = types.SimpleNamespace(time=lambda: ctx.call("time.time", {}))

    def thread_factory(target, daemon):
        tname = getattr(getattr(target, "__func__", target), "__name__", str(target))
        tok = ctx.call("threading.Thread", {"target": tname, "daemon": daemon})
        return _ThreadStub(tok, ctx)

    ns["threading"] = types.SimpleNamespace(Thread=thread_factory)


def run_case(func, inp):
    ctx = Ctx(inp)
    install_seams(ctx)
    st = copy.deepcopy(inp.get("state") or {})
    out = {}
    ret = None
    try:
        if func == "_preview_shell":
            build = NS["_preview_shell"](inp.get("key", ""))
            ret = build(inp.get("args") or {}, inp.get("max_len"))
        elif func == "_preview_bridge_call":
            build = NS["_preview_bridge_call"](inp.get("tool_name", ""))
            ret = build(inp.get("args") or {}, inp.get("max_len"))
        elif func == "_skin_spinner_list":
            ret = KAWAII._skin_spinner_list(inp.get("key", ""), inp.get("fallback"))
        elif func == "_catalog_thinking_verbs":
            ret = KAWAII._catalog_thinking_verbs()
        elif func == "_is_patch_stdout_proxy":
            outval = st.get("_out")
            slf = types.SimpleNamespace(_out=(StdoutProxy() if (isinstance(outval, dict)
                                                                  and outval.get("__proxy__"))
                                              else object()))
            ret = KAWAII._is_patch_stdout_proxy(slf)
        elif func == "start":
            def _animate():
                return None
            slf = types.SimpleNamespace(running=st.get("running", False), _animate=_animate)
            try:
                KAWAII.start(slf)
                ret = None
            except BaseException as exc:  # noqa: BLE001 - project, then mirror state
                ret = proj_exc(exc)
            # start() mutates the self double before any seam can raise; mirror
            # those fields even on the exception paths (C mutates io->state).
            st["running"] = slf.running
            if hasattr(slf, "start_time"):
                st["start_time"] = slf.start_time
            if hasattr(slf, "thread"):
                th = slf.thread
                st["thread"] = th.tok if isinstance(th, _ThreadStub) else th
        elif func == "_clear_line_blanks":
            slf = types.SimpleNamespace(last_line_len=st.get("last_line_len", 0))
            ret = KAWAII._clear_line_blanks(slf)
        else:
            out["error"] = "unknown func"
            return out
    except BaseException as exc:  # noqa: BLE001 - raise sentinel parity
        ret = proj_exc(exc)
    out["ret"] = ret
    out["trace"] = ctx.trace
    out["state"] = st
    return out


def canon(v):
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, float):
        if math.isnan(v):
            return {"__float__": "nan"}
        if math.isinf(v):
            return {"__float__": "-inf" if v < 0 else "inf"}
        if v == int(v) and abs(v) < 1e16:
            return int(v)
        return v
    if isinstance(v, dict):
        return {k: canon(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [canon(x) for x in v]
    return v


def deep_eq(a, b):
    return canon(a) == canon(b)


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or not line.startswith("{"):
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = rec.get("func")
        inp = rec.get("in", {}) or {}
        c_out = rec.get("out")
        cases += 1
        if func not in PY:
            mism += 1
            print(f"PYERR {func}: unknown func")
            continue
        try:
            py_out = run_case(func, inp)
        except BaseException as exc:  # noqa: BLE001
            mism += 1
            print(f"PYERR {func}: {type(exc).__name__}: {exc}")
            continue
        if c_out is None:
            mism += 1
            print(f"MISMATCH {func}: missing C out")
            continue
        bad = []
        for key, want in py_out.items():
            got = c_out.get(key)
            if not deep_eq(got, want):
                bad.append(key)
        if bad:
            mism += 1
            print(f"MISMATCH {func} keys={bad}")
            for key in bad:
                print(f"  C.{key}={json.dumps(canon(c_out.get(key)), default=str)[:600]}")
                print(f"  PY.{key}={json.dumps(canon(py_out.get(key)), default=str)[:600]}")
    if cases == 0:
        print("FATAL oracle_display_flow2: 0 cases")
        print("ORACLE display_flow2: 0 cases, 0 mismatches")
        return 2
    print(f"ORACLE display_flow2: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


def self_test(fixpath):
    """Drive every fixture through run_case; flag drive failures and suspicious
    uniform exception families naming a ported method or seam (binding bugs)."""
    names = set(PY.values()) | set(seam_names())
    seen_funcs = set()
    problems = []
    n = 0
    for line in open(fixpath, encoding="utf-8"):
        line = line.strip()
        if not line.startswith("{"):
            continue
        rec = json.loads(line)
        func = rec["func"]
        seen_funcs.add(func)
        n += 1
        try:
            out = run_case(func, rec.get("in") or {})
        except BaseException as exc:  # noqa: BLE001
            problems.append(f"DRIVE-FAIL {func}: {type(exc).__name__}: {exc}")
            continue
        ret = out.get("ret")
        if isinstance(ret, dict) and ret.get("__raise__") == "AttributeError":
            msg = ret.get("message", "")
            for nm in names:
                if f"'{nm}'" in msg:
                    problems.append(f"SUSPECT-ATTR {func}: {msg}")
                    break
    missing = set(PY) - seen_funcs
    if missing:
        problems.append(f"FIXTURE missing funcs: {sorted(missing)}")
    print(f"SELF-TEST display_flow2: {n} cases, {len(problems)} problems")
    for p in problems[:40]:
        print("  " + p)
    return 0 if not problems else 1


def seam_names():
    return ["sanitize_command_for_display", "summarize_shell_command", "bridge_tool_labels",
            "_primary_arg_preview", "_tail_trunc", "_get_skin", "t", "StdoutProxy_import",
            "time.time", "threading.Thread", "thread.start"]


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        fix = sys.argv[2] if len(sys.argv) > 2 else str(_HERE.parent / "fixtures" / "display_flow2.jsonl")
        sys.exit(self_test(fix))
    sys.exit(main())
