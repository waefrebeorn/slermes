#!/usr/bin/env python3
"""Oracle for the Slack plugin-adapter FLOW port, wave 3
(src/gateway/platforms/slack_adapter_flow3.c).

Drives the LIVE plugins/platforms/slack/adapter.py UNCLAIMED decision chains
(13 funcs: pure string/key leaves, boolean gates with scripted secret seams,
set-valued config gates, an exception classifier and the elapsed-status
formatter) with SCRIPTED seams. Every injected callable (t, time.monotonic,
_get_scoped_secret, _extra_or_secret, response.get) is a recording stub fed
the same scripts the C harness feeds its universal seam sa3_call. Compares the
DECISION SEQUENCE (ordered seam events {"name","args","ret"} in "trace") plus
the final return ("ret") and the echoed state ("state").

Live defs are AST-extracted from adapter.py (module constants seeded, Assign
AND AnnAssign, plain-Name targets; class methods dedented WITHOUT decorators
and bound onto a SimpleNamespace self double / as plain functions for
staticmethods). The heavy quarry import chain is never imported. Every exec
namespace is a registered sys.modules module (py3.14).

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError
included; those kinds PASS THROUGH every live except-Exception site).
Summary: 'ORACLE slack_adapter_flow3: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- {"__obj__": attrs} <-> SimpleNamespace; {"__set__": [members]} <-> set
  (canon sorts members); {"__tuple__": items} <-> tuple INPUT; {"__exc__":
  spec} <-> REAL exception instances (with optional callable attributes).
- isinstance/except families walk the FULL builtin exception hierarchy below
  (generated from the quarry CPython 3.14) plus asyncio.CancelledError.
- str()-computed positions carry str/int/bool/None only (no containers, no
  non-integral floats); time.monotonic scripts carry NON-INTEGRAL floats so
  the int() truncation matches libjson number handling.
- logger/translation seams record {"key"/"msg", ...} VERBATIM; None-valued
  seam ARG keys are dropped on both sides.
"""
import ast
import asyncio
import copy
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import Path
from typing import Any, Callable, Collection, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
A_SRC = os.path.join(Q, "plugins", "platforms", "slack", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def seed_constants(src, ns, path):
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, path)
            except Exception:
                pass


STATIC_TARGETS = [
    "_mrkdwn_insensitive", "_workspace_thread_key", "_agent_view_context_key",
    "_thread_cache_key", "_thread_root_message", "_default_status_text",
    "_is_block_payload_rejection",
]
INSTANCE_TARGETS = [
    "_strip_stream_cursor", "_extra_flag", "_slack_require_mention",
    "_extra_or_env_flag", "_extra_or_env_channel_set",
]
TARGETS = STATIC_TARGETS + INSTANCE_TARGETS
CONSTS = ["_TRANSIENT_UPLOAD_MARKERS"]

# class-level constants the ported methods read through self
SELF_CONSTS = {
    "_STREAM_CURSOR_GLYPHS": ("\u2589", "\u258d", "\u258c", "\u2026"),
}


def extract_adapter():
    mod = new_module("sa3_oracle_adapter")
    ns = mod.__dict__
    import contextvars
    import logging
    import time
    import unicodedata
    from dataclasses import dataclass, field
    ns.update({"asyncio": asyncio, "contextvars": contextvars, "logging": logging,
               "os": os, "re": re, "json": json, "time": time,
               "unicodedata": unicodedata, "Path": Path, "_Path": Path,
               "dataclass": dataclass, "field": field,
               "Any": Any, "Optional": Optional, "Callable": Callable,
               "Collection": Collection, "__file__": A_SRC})
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    seed_constants(src, ns, A_SRC)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in STATIC_TARGETS:
            exec_node(src, node, ns, A_SRC)
    cls = next((n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "SlackAdapter"), None)
    if cls is None:
        print("FATAL oracle_slack_adapter_flow3: SlackAdapter class not found")
        sys.exit(2)
    method_names = set(INSTANCE_TARGETS) | set(STATIC_TARGETS)
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in method_names:
            exec_node(src, node, ns, A_SRC)
    missing = [k for k in TARGETS + CONSTS if k not in ns]
    if missing:
        print(f"FATAL oracle_slack_adapter_flow3: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract_adapter()
REAL = {nm: NS[nm] for nm in TARGETS}


# ── canonical projection ──
def project(v):
    if isinstance(v, types.SimpleNamespace):
        return {"__obj__": {k: project(x) for k, x in vars(v).items()}}
    if isinstance(v, (set, frozenset)):
        return {"__set__": sorted(project(x) for x in v)}
    if isinstance(v, tuple):
        return [project(x) for x in v]
    if isinstance(v, dict):
        if any(isinstance(k, tuple) for k in v):
            return [{"key": [project(x) for x in k], "value": project(val)}
                    for k, val in v.items()]
        return {k: project(x) for k, x in v.items()}
    if isinstance(v, list):
        return [project(x) for x in v]
    if isinstance(v, float) and v.is_integer():
        return int(v)
    if isinstance(v, BaseException):
        return str(v)
    return v


def canon(v):
    return json.loads(json.dumps(project(v), ensure_ascii=False, default=str))


# ── full builtin exception hierarchy (CPython 3.14, quarry) ──
EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "OSError": OSError, "TimeoutError": TimeoutError, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": asyncio.CancelledError,
    "LookupError": LookupError, "IndexError": IndexError, "StopIteration": StopIteration,
    "ArithmeticError": ArithmeticError, "ZeroDivisionError": ZeroDivisionError,
    "UnicodeError": UnicodeError, "BufferError": BufferError, "EOFError": EOFError,
    "MemoryError": MemoryError, "NameError": NameError, "ReferenceError": ReferenceError,
    "SyntaxError": SyntaxError, "SystemError": SystemError, "AssertionError": AssertionError,
    "FloatingPointError": FloatingPointError, "OverflowError": OverflowError,
    "ConnectionError": ConnectionError, "ConnectionResetError": ConnectionResetError,
    "BrokenPipeError": BrokenPipeError, "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError, "FileNotFoundError": FileNotFoundError,
    "FileExistsError": FileExistsError, "PermissionError": PermissionError,
    "NotADirectoryError": NotADirectoryError, "IsADirectoryError": IsADirectoryError,
    "InterruptedError": InterruptedError, "ProcessLookupError": ProcessLookupError,
    "ChildProcessError": ChildProcessError, "BlockingIOError": BlockingIOError,
    "RecursionError": RecursionError, "NotImplementedError": NotImplementedError,
    "UnboundLocalError": UnboundLocalError, "ModuleNotFoundError": ModuleNotFoundError,
    "IndentationError": IndentationError, "TabError": TabError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeTranslateError": UnicodeTranslateError, "StopAsyncIteration": StopAsyncIteration,
    "Warning": Warning, "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
    "RuntimeWarning": RuntimeWarning, "SyntaxWarning": SyntaxWarning,
    "FutureWarning": FutureWarning, "ImportWarning": ImportWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning, "ResourceWarning": ResourceWarning,
    "BytesWarning": BytesWarning, "EncodingWarning": EncodingWarning,
    "BaseException": BaseException, "BaseExceptionGroup": BaseExceptionGroup,
    "ExceptionGroup": ExceptionGroup,
}


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind) or type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


# ── per-case script backend (mirrors the C sa3_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret


def rec(ctx, name, args_fn=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        args = {kk: vv for kk, vv in args.items() if vv is not None}
        return ctx.call(name, args)
    return f


def materialize(v, ctx=None):
    if isinstance(v, dict):
        if "__obj__" in v:
            d = {}
            for k, x in (v["__obj__"] or {}).items():
                d[k] = materialize(x, ctx)
            return types.SimpleNamespace(**d)
        if "__set__" in v:
            return set(materialize(x, ctx) for x in v["__set__"])
        if "__tuple__" in v:
            return tuple(materialize(x, ctx) for x in v["__tuple__"])
        if "__callable__" in v:
            name = v["__callable__"]
            def fn(*a, **k):
                return ctx.call(name, {"code": a[0] if a else None})
            return fn
        return {k: materialize(x, ctx) for k, x in v.items()}
    if isinstance(v, list):
        return [materialize(x, ctx) for x in v]
    return v


def make_exc_instance(spec, ctx):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    inst = cls(spec.get("message", ""))
    for k, v in spec.items():
        if k in ("type", "message"):
            continue
        setattr(inst, k, materialize(v, ctx))
    return inst


# ── per-case seam installation ──
def install_case(ctx):
    NS["time"] = types.SimpleNamespace(monotonic=rec(ctx, "time.monotonic", lambda: {}))
    NS["t"] = lambda key, **kw: ctx.call("t", {"key": key, **kw})
    NS["_get_scoped_secret"] = rec(
        ctx, "_get_scoped_secret", lambda name, default: {"name": name, "default": default})
    NS["_extra_or_secret"] = rec(
        ctx, "_extra_or_secret",
        lambda extra, key, env_var, default, *, blank_is_unset: {
            "extra": extra, "key": key, "env_var": env_var,
            "default": default, "blank_is_unset": blank_is_unset})


def mk_self(ctx, state):
    st = copy.deepcopy(state)
    mat = {k: materialize(v, ctx) for k, v in st.items()}
    self = types.SimpleNamespace(**mat)
    installed = set()
    # The ported methods read self.config.extra; state["extra"] is the dict
    # behind config.extra. Bind config as a double and keep "extra" as the
    # echoed state key (config itself is scaffolding, not modeled state).
    self.config = types.SimpleNamespace(extra=mat.get("extra"))
    installed.add("config")
    for nm in INSTANCE_TARGETS:
        setattr(self, nm, types.MethodType(REAL[nm], self))
        installed.add(nm)
    for nm in STATIC_TARGETS:
        setattr(self, nm, REAL[nm])
        installed.add(nm)
    for cname, cval in SELF_CONSTS.items():
        if cname not in mat:
            setattr(self, cname, cval)
            installed.add(cname)
    return self, installed


def project_self(self, installed):
    out = {}
    for k, v in vars(self).items():
        if k in installed:
            continue
        out[k] = project(v)
    return out


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    install_case(ctx)
    state_out = copy.deepcopy(state)
    try:
        if func in STATIC_TARGETS:
            f = NS[func]
            if func == "_mrkdwn_insensitive":
                ret = f(args.get("text", ""))
            elif func == "_workspace_thread_key":
                ret = f(args.get("team_id", ""), args.get("channel_id", ""), args.get("thread_ts", ""))
            elif func == "_agent_view_context_key":
                ret = f(args.get("team_id", ""), args.get("user_id", ""))
            elif func == "_thread_cache_key":
                ret = f(args.get("channel_id", ""), args.get("thread_ts", ""), args.get("team_id", ""))
            elif func == "_thread_root_message":
                ret = f(materialize(args.get("messages") or []), args.get("thread_ts", ""))
            elif func == "_default_status_text":
                if "started" in args and args["started"] is not None:
                    ret = f(args["started"])
                else:
                    ret = f(None)
            elif func == "_is_block_payload_rejection":
                ret = f(make_exc_instance(args["exc"]["__exc__"], ctx))
            else:
                raise RuntimeError(f"unbound static target {func}")
        elif func in INSTANCE_TARGETS:
            self, installed = mk_self(ctx, state)
            try:
                if func == "_strip_stream_cursor":
                    ret = self._strip_stream_cursor(args.get("text", ""))
                elif func == "_extra_flag":
                    if "default" in args:
                        ret = self._extra_flag(args["key"], args["default"])
                    else:
                        ret = self._extra_flag(args["key"])
                elif func == "_slack_require_mention":
                    ret = self._slack_require_mention()
                elif func == "_extra_or_env_flag":
                    kw = {"strip": args["strip"]} if "strip" in args else {}
                    ret = self._extra_or_env_flag(args["key"], args["env_var"], **kw)
                elif func == "_extra_or_env_channel_set":
                    kw = {"coerce_scalar": args["coerce_scalar"]} if "coerce_scalar" in args else {}
                    ret = self._extra_or_env_channel_set(args["key"], args["env_var"], **kw)
                else:
                    raise RuntimeError(f"unbound instance target {func}")
            finally:
                state_out = project_self(self, installed)
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── seam-name cross-check ──
SEAM_NAMES = ["t", "time.monotonic", "_get_scoped_secret", "_extra_or_secret", "response.get"]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "slack_adapter_flow3.c",
                encoding="utf-8").read()
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_slack_adapter_flow3: seams absent from C port: {missing_c}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "slack_adapter_flow3.jsonl"
    if fix.exists():
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if line:
                case = json.loads(line)
                fix_keys |= set((case.get("in") or {}).get("io") or {})
        bad = fix_keys - set(SEAM_NAMES)
        if bad:
            print(f"FATAL oracle_slack_adapter_flow3: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 12:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")
        for k in sorted(keys)[:3]:
            print(f"    {k} want={json.dumps(ws.get(k), ensure_ascii=False)[:200]}")
            print(f"    {k} got ={json.dumps(gs.get(k), ensure_ascii=False)[:200]}")


def main():
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec_in = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec_in.get("func", "")
        inp = rec_in.get("in") or {}
        got = rec_in.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print(f"MISMATCH {func}: harness emitted no result: {got}")
            continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1
            print(f"MISMATCH {func}: oracle drive raised {type(exc).__name__}: {exc}")
            continue
        g = canon(got)
        if g != want:
            m += 1
            report_mismatch(func, want, g, shown)
            shown += 1
    print(f"ORACLE slack_adapter_flow3: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "slack_adapter_flow3.jsonl"
    n = bad = 0
    counts = {}
    for line in open(fix, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            out = drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
            continue
        r = out["ret"]
        expect = case.get("expect_raise")
        if expect:
            if not (isinstance(r, dict) and r.get("__raise__") == expect):
                bad += 1
                print(f"SELFTEST EXPECT {func} case {n}: wanted raise {expect}, got {r}")
        elif isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            bad += 1
            print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
    missing = [f for f in TARGETS if f not in counts]
    if missing:
        bad += 1
        print(f"SELFTEST MISSING FUNCS: {missing}")
    thin = [f for f, c in counts.items() if c < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN FUNCS (<5 cases): {thin}")
    print(f"SELFTEST slack_adapter_flow3: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
