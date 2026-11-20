#!/usr/bin/env python3
"""Oracle for the Slack plugin-adapter FLOW port, wave 6
(src/gateway/platforms/slack_adapter_flow6.c).

Drives the LIVE plugins/platforms/slack/adapter.py UNCLAIMED remainder (18 funcs)
with SCRIPTED seams: every injected/unported callable is a recording stub fed
the same scripts the C harness feeds sa6_call. Compares the DECISION SEQUENCE
(ordered seam events {"name","args","ret"} in "trace") plus the final return
("ret") and echoed state ("state"). Live defs are AST-extracted (quarry import
chain never imported); every exec namespace is a registered sys.modules module.

Fail-closed: missing defs or 0 cases => exit 2. BaseException-only kinds PASS
THROUGH every except-Exception site. Summary line:
'ORACLE slack_adapter_flow6: N cases, M mismatches'.

Representation contract (shared with the C port): see the C port header.
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


MODULE_FUNCS = [
    "_standalone_post_text", "slack_deps_present",
]
CLASS_METHODS = [
    "_retry_after_from_exc", "_stream_key", "_evict_oldest_by_ts", "_trim_bot_message_timestamps",
    "_trim_mentioned_threads", "_EA_HEADER", "_EA_SMART_DENY_LINE",
    "_slash_channel_gated", "_format_chunks", "_team_in_install_set",
    "_build_thread_session_key", "_warn_if_inchannel_without_flat_reply",
]
CLASSMETHODS = ["_is_slack_cdn_url"]
NESTED_FUNCS = ["_esc", "_unseen", "_append_line"]
TARGETS = MODULE_FUNCS + CLASS_METHODS + CLASSMETHODS + NESTED_FUNCS

# Seam names that MUST match the C port and the fixture io-key union exactly.
SEAM_NAMES = [
    "parse_retry_after_seconds", "standalone_post_kwargs", "chat_postMessage",
    "slack_response_payload", "metadata_team_id", "scope_id_for_chat",
    "workspace_thread_key", "resolve_thread_ts", "discard_oldest_by_thread_ts",
    "evict_oldest_by_ts", "t", "is_ignored_channel", "slack_allowed_channels",
    "urlparse", "format_message", "truncate_message", "thread_session_source",
    "source_session_key", "cron_continuable_surface", "logger.warning",
]

CLS_DOUBLE = types.SimpleNamespace(
    _SLACK_CDN_EXACT_HOSTS={"slack.com", "slack-files.com"},
    _SLACK_CDN_HOST_SUFFIXES=(".slack.com", ".slack-files.com"),
)


def extract_adapter():
    mod = new_module("sa6_oracle_adapter")
    ns = mod.__dict__
    import logging
    ns.update({"asyncio": asyncio, "logging": logging, "os": os, "re": re,
               "json": json, "Any": Any, "Optional": Optional,
               "Callable": Callable, "__file__": A_SRC})
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    seed_constants(src, ns, A_SRC)
    # Module-level availability flag lives in a try/except (not seeded above).
    ns["SLACK_AVAILABLE"] = False
    # SlackAdapter class constants (methods read them off self/cls in the port).
    ns.update({"MAX_MESSAGE_LENGTH": 39000, "_BOT_TS_MAX": 5000,
               "_MENTIONED_THREADS_MAX": 5000, "_THREAD_REHYDRATION_CHECKED_MAX": 5000})
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in MODULE_FUNCS:
            exec_node(src, node, ns, A_SRC)
    cls = next((n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "SlackAdapter"), None)
    if cls is None:
        print("FATAL oracle_slack_adapter_flow6: SlackAdapter class not found")
        sys.exit(2)
    wanted = set(CLASS_METHODS) | set(CLASSMETHODS)
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in wanted:
            exec_node(src, node, ns, A_SRC)
    # Nested helpers: pull the first def of each name from anywhere in the tree.
    seen = set()
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in NESTED_FUNCS \
                and node.name not in seen:
            exec_node(src, node, ns, A_SRC)
            seen.add(node.name)
    missing = [k for k in TARGETS if k not in ns]
    if missing:
        print(f"FATAL oracle_slack_adapter_flow6: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract_adapter()
REAL = {nm: NS[nm] for nm in TARGETS}


# ── per-case seam installation ──
def install_case(ctx):
    NS["parse_retry_after_seconds"] = rec6(
        ctx, "parse_retry_after_seconds", lambda headers: {"headers": headers})
    NS["_standalone_post_kwargs"] = rec6(
        ctx, "standalone_post_kwargs",
        lambda chat_id, text, unfurl_kwargs, thread_id: {
            "chat_id": chat_id, "text": text,
            "unfurl_kwargs": unfurl_kwargs, "thread_id": thread_id})
    NS["_slack_response_payload"] = rec6(
        ctx, "slack_response_payload", lambda resp: {"response": resp})
    NS["t"] = rec6(ctx, "t", lambda key: {"key": key})
    NS["logger"] = LoggerDouble6(ctx)
    # urllib.parse.urlparse is an import-inside-body seam.
    m_up = types.ModuleType("urllib.parse")
    def _urlparse(url):
        ret = ctx.call("urlparse", {"url": url})
        return materialize(ret)
    m_up.urlparse = _urlparse
    sys.modules["urllib.parse"] = m_up


def mk_self(ctx, state, drive_func=None):
    self = types.SimpleNamespace()
    for nm in CLASS_METHODS:
        setattr(self, nm, types.MethodType(REAL[nm], self))
    for nm in CLASSMETHODS:
        setattr(self, nm, (lambda url, f=REAL[nm]: f(CLS_DOUBLE, url)))
    # seamed sibling methods
    self._metadata_team_id = rec6(ctx, "metadata_team_id", lambda md: {"metadata": md})
    self.scope_id_for_chat = rec6(ctx, "scope_id_for_chat", lambda cid: {"chat_id": cid})
    self._workspace_thread_key = rec6(
        ctx, "workspace_thread_key",
        lambda tid, cid, ts: {"team_id": tid, "chat_id": cid, "thread_ts": ts})
    self._resolve_thread_ts = rec6(
        ctx, "resolve_thread_ts",
        lambda reply_to, md: {"reply_to": reply_to, "metadata": md})
    self._discard_oldest_by_thread_ts = rec6(
        ctx, "discard_oldest_by_thread_ts", lambda entries, n, *a: {"entries": entries, "n": n})
    if drive_func != "_evict_oldest_by_ts":
        self._evict_oldest_by_ts = rec6(
            ctx, "evict_oldest_by_ts", lambda entries, cap, *a: {"entries": entries, "cap": cap})
    self._is_ignored_channel = rec6(
        ctx, "is_ignored_channel", lambda cid: {"chat_id": cid})
    self._slack_allowed_channels = rec6(ctx, "slack_allowed_channels", lambda: {})
    self.format_message = rec6(ctx, "format_message", lambda text: {"text": text})
    self.truncate_message = rec6(
        ctx, "truncate_message", lambda text, mx: {"text": text, "max": mx})
    self._thread_session_source = rec6(
        ctx, "thread_session_source",
        lambda cid, ts, uid, tid, ct: {
            "channel_id": cid, "thread_ts": ts, "user_id": uid,
            "team_id": tid, "chat_type": ct})
    self._source_session_key = rec6(
        ctx, "source_session_key", lambda source: {"source": source})
    self._cron_continuable_surface = rec6(ctx, "cron_continuable_surface", lambda: {})
    # state-backed attributes (constants overridable per case for small fixtures)
    self._BOT_TS_MAX = state.get("_BOT_TS_MAX", 5000)
    self._MENTIONED_THREADS_MAX = state.get("_MENTIONED_THREADS_MAX", 5000)
    self.MAX_MESSAGE_LENGTH = state.get("MAX_MESSAGE_LENGTH", 39000)
    self._bot_message_ts = state.get("_bot_message_ts", [])
    self._mentioned_threads = state.get("_mentioned_threads", [])
    self._team_clients = state.get("_team_clients") or {}
    self._session_store = materialize(state.get("_session_store"))
    self.config = types.SimpleNamespace(extra=state.get("extra") or {})
    return self


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    install_case(ctx)
    state_out = copy.deepcopy(state)
    NS["parts"] = (state_out.setdefault("parts", []) if func == "_append_line"
                   else state_out.get("parts", []))
    NS["extras"] = state_out.get("extras", [])
    self = mk_self(ctx, state_out, func)
    try:
        if func == "_retry_after_from_exc":
            ret = NS[func](materialize(args.get("e")))
        elif func == "_standalone_post_text":
            async def _cpm(**kw):
                return ctx.call("chat_postMessage", dict(kw))
            client = types.SimpleNamespace(chat_postMessage=_cpm)
            ret = asyncio.run(NS[func](
                client, args.get("chat_id"), args.get("text"),
                args.get("unfurl_kwargs"), args.get("thread_id")))
        elif func == "slack_deps_present":
            ret = NS[func]()
        elif func == "_stream_key":
            ret = self._stream_key(args.get("chat_id"), args.get("metadata"))
        elif func == "_evict_oldest_by_ts":
            ret = self._evict_oldest_by_ts(materialize(args.get("entries")), args.get("cap"))
        elif func == "_trim_bot_message_timestamps":
            ret = self._trim_bot_message_timestamps()
        elif func == "_trim_mentioned_threads":
            ret = self._trim_mentioned_threads()
        elif func == "_EA_HEADER":
            ret = self._EA_HEADER()
        elif func == "_EA_SMART_DENY_LINE":
            ret = self._EA_SMART_DENY_LINE()
        elif func == "_slash_channel_gated":
            ret = self._slash_channel_gated(args.get("channel_id"))
        elif func == "_is_slack_cdn_url":
            ret = NS[func](CLS_DOUBLE, args.get("url"))
        elif func == "_format_chunks":
            ret = self._format_chunks(args.get("content"))
        elif func == "_team_in_install_set":
            ret = self._team_in_install_set(args.get("team_id"))
        elif func == "_build_thread_session_key":
            ret = self._build_thread_session_key(
                args.get("channel_id"), args.get("thread_ts"), args.get("user_id"),
                args.get("team_id", ""), chat_type=args.get("chat_type", "group"))
        elif func == "_warn_if_inchannel_without_flat_reply":
            ret = self._warn_if_inchannel_without_flat_reply(args.get("team_name"))
        elif func == "_esc":
            ret = NS[func](args.get("text"))
        elif func == "_unseen":
            ret = NS[func](args.get("piece"), args.get("base"))
        elif func == "_append_line":
            ret = NS[func](args.get("text"), args.get("quote_depth", 0), args.get("bullet", ""))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── seam-name cross-check (SET EQUALITY across C / oracle / fixtures) ──
def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "slack_adapter_flow6.c",
                encoding="utf-8").read()
    ss = set(SEAM_NAMES)
    if len(ss) != len(SEAM_NAMES):
        print("FATAL oracle_slack_adapter_flow6: duplicate SEAM_NAMES")
        sys.exit(2)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_slack_adapter_flow6: seams absent from C port: {missing_c}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "slack_adapter_flow6.jsonl"
    if fix.exists():
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if line:
                fix_keys |= set((json.loads(line).get("in") or {}).get("io") or {})
        if fix_keys != ss:
            print(f"FATAL oracle_slack_adapter_flow6: fixture io keys != SEAM_NAMES; "
                  f"missing={sorted(ss - fix_keys)} extra={sorted(fix_keys - ss)}")
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
    print(f"ORACLE slack_adapter_flow6: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "slack_adapter_flow6.jsonl"
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
    thin = [f for f, c in counts.items() if c < 6]
    if thin:
        bad += 1
        print(f"SELFTEST THIN FUNCS (<6 cases): {thin}")
    print(f"SELFTEST slack_adapter_flow6: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


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


# ── per-case script backend (mirrors the C sa6_call exactly) ──
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


def rec6(ctx, name, args_fn=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
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
        return {k: materialize(x, ctx) for k, x in v.items()}
    if isinstance(v, list):
        return [materialize(x, ctx) for x in v]
    return v


class LoggerDouble6:
    def __init__(self, ctx):
        self.ctx = ctx

    def warning(self, msg, *args, **kwargs):
        # Decision sequence only: the fixed log text is not part of the trace;
        # the variable argument (team_name) is.
        return self.ctx.call("logger.warning", {"args": list(args)})

    def info(self, msg, *args, **kwargs):
        return self.ctx.call("logger.warning", {"args": list(args)})


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
