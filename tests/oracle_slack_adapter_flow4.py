#!/usr/bin/env python3
"""Oracle for the Slack plugin-adapter FLOW port, wave 4
(src/gateway/platforms/slack_adapter_flow4.c).

Drives the LIVE plugins/platforms/slack/adapter.py UNCLAIMED remainder (25 funcs:
SDK-probe global with except-Exception, static tuple/dict leaves, a claimed
sibling compositor, scoped policy gates and an exception classifier) with
SCRIPTED seams. Every injected callable (sdk_agent_sessions_probe,
is_slack_voice_clip, _extra_or_secret, logger.warning, _is_retryable_error)
is a recording stub fed the same scripts the C harness feeds sa4_call. Compares
the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in "trace")
plus the final return ("ret") and the echoed state ("state").

Live defs are AST-extracted from adapter.py; the heavy quarry import chain is
never imported. Every exec namespace is a registered sys.modules module.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError
included; those kinds PASS THROUGH every except-Exception site).
Summary: 'ORACLE slack_adapter_flow4: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- {"__obj__": attrs} <-> SimpleNamespace; {"__set__": [members]} <-> set;
  {"__tuple__": items} <-> tuple INPUT; {"__exc__": spec} <-> REAL exception
  instances (with optional callable/attribute fields).
- isinstance/except families walk the FULL builtin exception hierarchy below
  (CPython 3.14 quarry) plus asyncio.CancelledError.
- str()-computed positions carry str/int/bool/None only; status_code is an
  integral number; exc kinds are non-KeyError so str(exc) == message.
- logger seams record {"msg": literal, "args": [raw args]} VERBATIM.
- SDK probe scripts are JSON bools: truthy -> a callable, falsy -> None; a
  scripted raise fires from the stubbed AsyncWebClient metaclass getattr.
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
from typing import Any, Callable, Optional

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


MODULE_FUNCS = ["_sdk_supports_agent_sessions"]
STATIC_METHODS = ["_interaction_fields", "_section_text",
                  "_parse_users_info", "_slack_file_kind",
                  "_button", "_event_team_id", "_context_channel_id", "_first_file_share"]
INSTANCE_METHODS = ["_cron_continuable_surface", "_slack_allow_bots",
                    "_reactions_enabled", "_is_retryable_upload_error",
                    "_markdown_block_payload", "_feedback_block",
                    "_append_feedback_block", "_build_identity_prompt",
                    "_is_foreign_author", "_agent_view_event_fields",
                    "_extract_assistant_thread_metadata", "_event_declares_bot_sender",
                    "_resolve_thread_ts", "_session_thread_ts",
                    "_normalize_changed_message", "_assistant_thread_title_enabled"]
TARGETS = MODULE_FUNCS + STATIC_METHODS + INSTANCE_METHODS
# Claimed pure leaves transcribed inline in C (no PoP, no seam) -- seeded REAL
# here so the live reference can run them.
PURE_LEAVES = ["_str_or_empty", "_first_truthy"]
INLINE_METHODS = ["_extra_flag", "_team_in_install_set",
                  "_dm_top_level_threads_as_sessions"]
CONSTS = ["_TRANSIENT_UPLOAD_MARKERS"]


def extract_adapter():
    mod = new_module("sa4_oracle_adapter")
    ns = mod.__dict__
    import logging
    ns.update({"asyncio": asyncio, "logging": logging, "os": os, "re": re,
               "json": json, "Any": Any, "Optional": Optional,
               "__file__": A_SRC})
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    seed_constants(src, ns, A_SRC)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and \
                node.name in (MODULE_FUNCS + PURE_LEAVES):
            exec_node(src, node, ns, A_SRC)
    cls = next((n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "SlackAdapter"), None)
    if cls is None:
        print("FATAL oracle_slack_adapter_flow4: SlackAdapter class not found")
        sys.exit(2)
    method_names = set(STATIC_METHODS) | set(INSTANCE_METHODS) | set(INLINE_METHODS)
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in method_names:
            exec_node(src, node, ns, A_SRC)
    missing = [k for k in TARGETS + CONSTS + PURE_LEAVES + INLINE_METHODS if k not in ns]
    if missing:
        print(f"FATAL oracle_slack_adapter_flow4: adapter missing: {missing}")
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


# ── per-case script backend (mirrors the C sa4_call exactly) ──
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
    cls = EXC_CLASSES.get(kind) or type(kind, (Exception,), {})
    inst = cls(spec.get("message", ""))
    for k, v in spec.items():
        if k in ("type", "message"):
            continue
        if k == "exc_message_attr":
            setattr(inst, "message", materialize(v, ctx))
            continue
        setattr(inst, k, materialize(v, ctx))
    return inst


class LoggerDouble:
    def __init__(self, ctx):
        self.ctx = ctx

    def _log(self, level, msg, *args, **kwargs):
        return self.ctx.call("logger." + level, {"msg": msg, "args": list(args)})

    def warning(self, msg, *a, **k):
        return self._log("warning", msg, *a, **k)

    def info(self, msg, *a, **k):
        return self._log("info", msg, *a, **k)

    def debug(self, msg, *a, **k):
        return self._log("debug", msg, *a, **k)

    def error(self, msg, *a, **k):
        return self._log("error", msg, *a, **k)


# ── per-case seam installation ──
def install_case(ctx):
    NS["logger"] = LoggerDouble(ctx)
    NS["t"] = rec(ctx, "t", lambda key: {"key": key})
    NS["_extra_or_secret"] = rec(
        ctx, "_extra_or_secret",
        lambda extra, key, env_var, default: {
            "extra": extra, "key": key, "env_var": env_var, "default": default})
    NS["_is_slack_voice_clip"] = rec(ctx, "is_slack_voice_clip", lambda f: {"f": f})

    # SDK-probe: the LIVE function does `from slack_sdk.web.async_client import
    # AsyncWebClient; callable(getattr(AsyncWebClient, "agents_sessions_setStatus", None))`.
    # A metaclass __getattr__ fires the seam; truthy script -> a callable,
    # falsy -> None; a scripted raise propagates from getattr into the live
    # except-Exception (or past it for BaseException-only kinds).
    class ProbeMeta(type):
        def __getattr__(cls, name):
            v = ctx.call("sdk_agent_sessions_probe", {})
            return (lambda: None) if v else None
    probe_cls = ProbeMeta("AsyncWebClient", (), {})
    m_async = types.ModuleType("slack_sdk.web.async_client")
    m_async.AsyncWebClient = probe_cls
    m_web = types.ModuleType("slack_sdk.web")
    m_web.__path__ = []
    m_web.async_client = m_async
    m_sdk = types.ModuleType("slack_sdk")
    m_sdk.__path__ = []
    m_sdk.web = m_web
    for n, m in (("slack_sdk", m_sdk), ("slack_sdk.web", m_web),
                 ("slack_sdk.web.async_client", m_async)):
        sys.modules[n] = m


def mk_self(ctx, state):
    extra = materialize(state.get("extra"))
    self = types.SimpleNamespace(config=types.SimpleNamespace(extra=extra))
    # Static targets resolve via self as plain functions (no descriptor binding).
    for nm in STATIC_METHODS:
        setattr(self, nm, REAL[nm])
    # Instance targets bind as methods.
    for nm in INSTANCE_METHODS:
        setattr(self, nm, types.MethodType(REAL[nm], self))
    # Claimed helpers transcribed inline in C run REAL on the live side.
    for nm in INLINE_METHODS:
        setattr(self, nm, types.MethodType(NS[nm], self))
    self._MARKDOWN_BLOCK_MAX = 11500
    self._team_bot_names = materialize(state.get("_team_bot_names")) or {}
    self._bot_display_name = materialize(state.get("_bot_display_name")) or ""
    self._processed_message_ts = materialize(state.get("_processed_message_ts")) or set()
    if "_team_clients" in state:
        self._team_clients = materialize(state["_team_clients"])
    self._is_retryable_error = rec(ctx, "_is_retryable_error", lambda body: {"body": body})
    self._slack_api_human_users = rec(ctx, "slack_api_human_users")
    return self


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    install_case(ctx)
    state_out = copy.deepcopy(state)
    try:
        if func in MODULE_FUNCS:
            gk = "_AGENT_SESSIONS_SUPPORTED"
            NS[gk] = state.get(gk) if gk in state else None
            try:
                ret = NS[func]()
            finally:
                state_out[gk] = NS[gk]
        elif func in STATIC_METHODS:
            if func == "_interaction_fields":
                ret = NS[func](args.get("body"), args.get("action"))
            elif func == "_section_text":
                if "limit" in args:
                    ret = NS[func](args.get("message"), args["limit"])
                else:
                    ret = NS[func](args.get("message"))
            elif func == "_parse_users_info":
                ret = NS[func](args.get("payload"), args.get("user_id"))
            elif func == "_slack_file_kind":
                ret = NS[func](args.get("f"), args.get("mimetype"))
            elif func == "_button":
                ret = NS[func](args.get("text"), args.get("action_id"), args.get("value"),
                               style=args.get("style", ""), emoji=args.get("emoji", False))
            elif func == "_event_team_id":
                ret = NS[func](args.get("event"), args.get("body"),
                               install_set=args.get("install_set"))
            elif func == "_context_channel_id":
                ret = NS[func](args.get("context"))
            elif func == "_first_file_share":
                ret = NS[func](args.get("file_obj"), args.get("channel_id"))
            else:
                raise RuntimeError(f"unbound static target {func}")
        elif func in INSTANCE_METHODS:
            self = mk_self(ctx, state)
            if func == "_cron_continuable_surface":
                ret = self._cron_continuable_surface()
            elif func == "_slack_allow_bots":
                ret = self._slack_allow_bots()
            elif func == "_reactions_enabled":
                ret = self._reactions_enabled()
            elif func == "_is_retryable_upload_error":
                ret = self._is_retryable_upload_error(
                    make_exc_instance(args["exc"]["__exc__"], ctx))
            elif func == "_markdown_block_payload":
                ret = self._markdown_block_payload(args.get("content"))
            elif func == "_feedback_block":
                ret = self._feedback_block()
            elif func == "_append_feedback_block":
                ret = self._append_feedback_block(args.get("blocks"))
            elif func == "_build_identity_prompt":
                ret = self._build_identity_prompt(args.get("team_id", ""))
            elif func == "_is_foreign_author":
                ret = self._is_foreign_author(args.get("author_team"), args.get("resolved_team"))
            elif func == "_agent_view_event_fields":
                ret = self._agent_view_event_fields(args.get("event"), args.get("body"))
            elif func == "_extract_assistant_thread_metadata":
                ret = self._extract_assistant_thread_metadata(args.get("event"), args.get("body"))
            elif func == "_event_declares_bot_sender":
                ret = self._event_declares_bot_sender(args.get("event"))
            elif func == "_resolve_thread_ts":
                ret = self._resolve_thread_ts(args.get("reply_to"), args.get("metadata"))
            elif func == "_session_thread_ts":
                ret = self._session_thread_ts(args.get("event"), args.get("ts"),
                                              args.get("is_dm"), args.get("assistant_meta") or {})
            elif func == "_normalize_changed_message":
                ret = self._normalize_changed_message(args.get("event"))
            elif func == "_assistant_thread_title_enabled":
                ret = self._assistant_thread_title_enabled()
            else:
                raise RuntimeError(f"unbound instance target {func}")
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── seam-name cross-check ──
SEAM_NAMES = ["sdk_agent_sessions_probe", "is_slack_voice_clip",
              "_extra_or_secret", "logger.warning", "_is_retryable_error",
              "t", "slack_api_human_users"]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "slack_adapter_flow4.c",
                encoding="utf-8").read()
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_slack_adapter_flow4: seams absent from C port: {missing_c}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "slack_adapter_flow4.jsonl"
    if fix.exists():
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if line:
                case = json.loads(line)
                fix_keys |= set((case.get("in") or {}).get("io") or {})
        bad = fix_keys - set(SEAM_NAMES)
        if bad:
            print(f"FATAL oracle_slack_adapter_flow4: fixture io keys without a seam: {sorted(bad)}")
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
    print(f"ORACLE slack_adapter_flow4: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "slack_adapter_flow4.jsonl"
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
        elif isinstance(r, dict) and r.get("__raise__") == "AttributeError" and \
                any(t in str(r.get("message", "")) for t in TARGETS):
            bad += 1
            print(f"SELFTEST MISSING-BINDING {func} case {n}: {r}")
    missing = [f for f in TARGETS if f not in counts]
    if missing:
        bad += 1
        print(f"SELFTEST MISSING FUNCS: {missing}")
    thin = [f for f, c in counts.items() if c < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN FUNCS (<5 cases): {thin}")
    print(f"SELFTEST slack_adapter_flow4: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
