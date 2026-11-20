#!/usr/bin/env python3
"""Oracle for the Slack plugin-adapter FLOW port, wave 5
(src/gateway/platforms/slack_adapter_flow5.c).

Drives the LIVE plugins/platforms/slack/adapter.py UNCLAIMED remainder (18 funcs)
with SCRIPTED seams: every injected/unported callable is a recording stub fed
the same scripts the C harness feeds sa5_call. Compares the DECISION SEQUENCE
(ordered seam events {"name","args","ret"} in "trace") plus the final return
("ret") and echoed state ("state"). Live defs are AST-extracted (quarry import
chain never imported); every exec namespace is a registered sys.modules module.

Fail-closed: missing defs or 0 cases => exit 2. BaseException-only kinds PASS
THROUGH every except-Exception site. Summary line:
'ORACLE slack_adapter_flow5: N cases, M mismatches'.

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
    "_session_status_method", "_session_title_method",
    "_standalone_format_mrkdwn", "_standalone_proxy_kwargs",
]
STATIC_METHODS = ["_media_message_type", "_append_block_text"]
INSTANCE_METHODS = [
    "_native_task_card_key", "native_task_card_destination_supported",
    "_suppressed_ignored", "_outbound_blocked", "supports_draft_streaming",
    "_reacting_target", "_channel_prompt_with_identity", "_slack_is_free_channel",
    "_slack_reply_expected", "_slack_message_matches_mention_patterns",
    "_thread_rehydration_key", "_thread_session_source",
]
TARGETS = MODULE_FUNCS + STATIC_METHODS + INSTANCE_METHODS
# Claimed/unported leaves transcribed inline in C (no PoP, no seam) -- seeded
# REAL here so the live reference can run them.
PURE_LEAVES = []
CONSTS = []


class SendResult(dict):
    """Plain-dict SendResult double so project() passes it through verbatim."""
    def __init__(self, success=False, error=None):
        super().__init__(success=success, error=error)


class MessageType:
    TEXT = "TEXT"; PHOTO = "PHOTO"; VIDEO = "VIDEO"; VOICE = "VOICE"; DOCUMENT = "DOCUMENT"


class Pat:
    """Pattern double: .search is a substring test (wake-word seam carrier)."""
    def __init__(self, s):
        self.s = s
    def search(self, text):
        return self.s in text


def extract_adapter():
    mod = new_module("sa5_oracle_adapter")
    ns = mod.__dict__
    import logging
    ns.update({"asyncio": asyncio, "logging": logging, "os": os, "re": re,
               "json": json, "Any": Any, "Optional": Optional,
               "MessageType": MessageType, "SendResult": SendResult,
               "__file__": A_SRC})
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    seed_constants(src, ns, A_SRC)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in MODULE_FUNCS:
            exec_node(src, node, ns, A_SRC)
    cls = next((n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "SlackAdapter"), None)
    if cls is None:
        print("FATAL oracle_slack_adapter_flow5: SlackAdapter class not found")
        sys.exit(2)
    method_names = set(STATIC_METHODS) | set(INSTANCE_METHODS)
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in method_names:
            exec_node(src, node, ns, A_SRC)
    missing = [k for k in TARGETS + CONSTS + PURE_LEAVES if k not in ns]
    if missing:
        print(f"FATAL oracle_slack_adapter_flow5: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract_adapter()
REAL = {nm: NS[nm] for nm in TARGETS}


# ── per-case seam installation ──
# Seam helpers that are also drive targets must NOT be replaced with recorders
# while that target is the one being driven.
SEAM_HELPERS = {
    "_resolve_thread_ts": ("resolve_thread_ts", lambda reply_to, metadata: {"reply_to": reply_to, "metadata": metadata}),
    "_metadata_team_id": ("metadata_team_id", lambda metadata: {"metadata": metadata}),
    "_workspace_thread_key": ("workspace_thread_key", lambda team_id, chat_id, thread_ts: {"team_id": team_id, "chat_id": chat_id, "thread_ts": thread_ts}),
    "_native_task_card_key": ("native_task_card_key", lambda chat_id, reply_to, metadata: {"chat_id": chat_id, "reply_to": reply_to, "metadata": metadata}),
    "_is_ignored_channel": ("is_ignored_channel", lambda chat_id: {"chat_id": chat_id}),
    "_suppressed_ignored": ("suppressed_ignored", lambda chat_id, what: {"chat_id": chat_id, "what": what}),
    "_reactions_enabled": ("reactions_enabled", None),
    "_workspace_message_marker": ("workspace_message_marker", lambda team_id, ts: {"team_id": team_id, "ts": ts}),
    "_build_identity_prompt": ("build_identity_prompt", lambda team_id: {"team_id": team_id}),
    "_slack_require_mention_channels": ("slack_require_mention_channels", None),
    "_slack_free_response_channels": ("slack_free_response_channels", None),
    "_slack_require_mention": ("slack_require_mention", None),
    "_slack_message_mentions_self": ("slack_message_mentions_self", lambda text, self_uids: {"text": text, "self_uids": self_uids}),
    "_slack_message_addressed_to_other_user": ("slack_message_addressed_to_other_user", lambda text, self_uids: {"text": text, "self_uids": self_uids}),
    "_slack_thread_followup_silence": ("slack_thread_followup_silence", None),
    "_slack_is_free_channel": ("slack_is_free_channel", lambda channel_id: {"channel_id": channel_id}),
    "build_source": ("build_source", "KWARGS"),
}


def install_case(ctx, drive_func):
    NS["logger"] = LoggerDouble(ctx)
    NS["_sdk_supports_agent_sessions"] = rec5(ctx, "sdk_supports_agent_sessions")
    NS["_slack_unfurl_kwargs"] = rec5(ctx, "slack_unfurl_kwargs", lambda extra: {"extra": extra})
    NS["_extract_additional_text_from_slack_blocks"] = rec5(
        ctx, "extract_additional_text_from_slack_blocks",
        lambda blocks, text, bot_uid: {"blocks": blocks, "text": text, "bot_uid": bot_uid})
    NS["_serialize_slack_blocks_for_agent"] = rec5(
        ctx, "serialize_slack_blocks_for_agent", lambda blocks: {"blocks": blocks})
    NS["resolve_proxy_url"] = rec5(ctx, "resolve_proxy_url")

    # SlackAdapter.__new__(SlackAdapter).format_message(text) -- lightweight class
    fmt = rec5(ctx, "format_message", lambda text: {"text": text})
    NS["SlackAdapter"] = type("SlackAdapter", (), {"format_message": lambda self, text: fmt(text)})

    # gateway.platforms.base provides two seamed callables (import inside bodies)
    m_base = types.ModuleType("gateway.platforms.base")
    m_base.proxy_kwargs_for_aiohttp = rec5(ctx, "proxy_kwargs_for_aiohttp", lambda url: {"url": url})
    m_base.resolve_channel_prompt = rec5(
        ctx, "resolve_channel_prompt",
        lambda extra, channel_id, _third: {"extra": extra, "channel_id": channel_id})
    m_gw = types.ModuleType("gateway"); m_gw.__path__ = []
    m_plat = types.ModuleType("gateway.platforms"); m_plat.__path__ = []
    for n, m in (("gateway", m_gw), ("gateway.platforms", m_plat), ("gateway.platforms.base", m_base)):
        sys.modules[n] = m


def mk_self(ctx, state, drive_func):
    self = types.SimpleNamespace()
    for nm in STATIC_METHODS:
        setattr(self, nm, REAL[nm])
    for nm in INSTANCE_METHODS:
        setattr(self, nm, types.MethodType(REAL[nm], self))
    # seamed helpers (skip the one currently being driven)
    for nm, (sname, afn) in SEAM_HELPERS.items():
        if nm == drive_func:
            continue
        if afn == "KWARGS":
            def _kw(*a, _s=sname, **k):
                return ctx.call(_s, dict(k))
            fn = _kw
        else:
            fn = rec5(ctx, sname, afn)
        setattr(self, nm, fn)
    # collection-returning seams: materialize {"__set__":..} carriers into real sets
    self._slack_require_mention_channels = lambda: materialize(ctx.call("slack_require_mention_channels", {}))
    self._slack_free_response_channels = lambda: materialize(ctx.call("slack_free_response_channels", {}))
    # _slack_mention_patterns returns pattern doubles (substring search seam)
    def _patterns():
        ret = ctx.call("slack_mention_patterns", {})
        return [Pat(x) for x in (ret or [])]
    self._slack_mention_patterns = _patterns
    # state-backed attributes
    self.config = types.SimpleNamespace(extra=materialize(state.get("extra")) or {})
    self._app = materialize(state.get("app"))
    self._native_stream_unsupported = bool(state.get("native_stream_unsupported", False))
    self._bot_user_id = state.get("_bot_user_id", "")
    self._reacting_message_ids = materialize(state.get("_reacting_message_ids")) or []
    self._session_store = materialize(state.get("_session_store"))
    return self


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    install_case(ctx, func)
    state_out = copy.deepcopy(state)
    try:
        if func in MODULE_FUNCS:
            if func == "_session_status_method":
                ret = NS[func](materialize(args.get("client")))
            elif func == "_session_title_method":
                ret = NS[func](materialize(args.get("client")))
            elif func == "_standalone_format_mrkdwn":
                ret = NS[func](args.get("text"))
            elif func == "_standalone_proxy_kwargs":
                ret = NS[func]()
            else:
                raise RuntimeError(f"unbound module target {func}")
        elif func in STATIC_METHODS:
            if func == "_media_message_type":
                ret = NS[func](args.get("media_types"))
            elif func == "_append_block_text":
                ret = NS[func](args.get("text"), args.get("blocks"), args.get("bot_uid"))
            else:
                raise RuntimeError(f"unbound static target {func}")
        elif func in INSTANCE_METHODS:
            self = mk_self(ctx, state, func)
            if func == "_native_task_card_key":
                ret = self._native_task_card_key(args.get("chat_id"), args.get("reply_to"), args.get("metadata"))
            elif func == "native_task_card_destination_supported":
                ret = self.native_task_card_destination_supported(args.get("chat_id"), reply_to=args.get("reply_to"), metadata=args.get("metadata"))
            elif func == "_suppressed_ignored":
                ret = self._suppressed_ignored(args.get("chat_id"), args.get("what"))
            elif func == "_outbound_blocked":
                ret = self._outbound_blocked(args.get("chat_id"), args.get("what"))
            elif func == "supports_draft_streaming":
                ret = self.supports_draft_streaming()
            elif func == "_reacting_target":
                ret = self._reacting_target(materialize(args.get("event")))
            elif func == "_channel_prompt_with_identity":
                ret = self._channel_prompt_with_identity(args.get("channel_id"), args.get("team_id"))
            elif func == "_slack_is_free_channel":
                ret = self._slack_is_free_channel(args.get("channel_id"))
            elif func == "_slack_reply_expected":
                ret = self._slack_reply_expected(
                    args.get("routing_text"), args.get("bot_uid"),
                    channel_id=args.get("channel_id"), addressed=args.get("addressed"),
                    opens_own_session=args.get("opens_own_session"),
                    channel_thread_followup=args.get("channel_thread_followup", False))
            elif func == "_slack_message_matches_mention_patterns":
                ret = self._slack_message_matches_mention_patterns(args.get("text"))
            elif func == "_thread_rehydration_key":
                ret = self._thread_rehydration_key(args.get("channel_id"), args.get("thread_ts"),
                                                    args.get("user_id"), args.get("team_id", ""))
            elif func == "_thread_session_source":
                ret = self._thread_session_source(args.get("channel_id"), args.get("thread_ts"),
                                                  args.get("user_id"), args.get("team_id"), args.get("chat_type"))
            else:
                raise RuntimeError(f"unbound instance target {func}")
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── seam-name cross-check (SET EQUALITY across C / oracle / fixtures) ──
SEAM_NAMES = [
    "sdk_supports_agent_sessions", "format_message", "logger.debug",
    "resolve_proxy_url", "proxy_kwargs_for_aiohttp", "resolve_thread_ts",
    "metadata_team_id", "workspace_thread_key", "native_task_card_key",
    "is_ignored_channel", "logger.log", "suppressed_ignored", "slack_unfurl_kwargs",
    "reactions_enabled", "workspace_message_marker", "resolve_channel_prompt",
    "build_identity_prompt", "slack_require_mention_channels",
    "slack_free_response_channels", "slack_require_mention",
    "slack_message_mentions_self", "slack_message_addressed_to_other_user",
    "slack_thread_followup_silence", "slack_is_free_channel",
    "slack_mention_patterns", "extract_additional_text_from_slack_blocks",
    "serialize_slack_blocks_for_agent", "build_source",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "slack_adapter_flow5.c", encoding="utf-8").read()
    ss = set(SEAM_NAMES)
    if len(ss) != len(SEAM_NAMES):
        print("FATAL oracle_slack_adapter_flow5: duplicate SEAM_NAMES")
        sys.exit(2)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_slack_adapter_flow5: seams absent from C port: {missing_c}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "slack_adapter_flow5.jsonl"
    if fix.exists():
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if line:
                case = json.loads(line)
                fix_keys |= set((case.get("in") or {}).get("io") or {})
        if fix_keys != ss:
            print(f"FATAL oracle_slack_adapter_flow5: fixture io keys != SEAM_NAMES; missing={sorted(ss-fix_keys)} extra={sorted(fix_keys-ss)}")
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
    print(f"ORACLE slack_adapter_flow5: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "slack_adapter_flow5.jsonl"
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
    print(f"SELFTEST slack_adapter_flow5: {n} cases, {bad} drive/script failures")
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


def rec5(ctx, name, args_fn=None):
    """Like rec() but keeps None-valued args (wave-5 C ports emit them as JSON null)."""
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

    def log(self, level, msg, *a, **k):
        return self._log("log", msg, *a, **k)

    def error(self, msg, *a, **k):
        return self._log("error", msg, *a, **k)

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
