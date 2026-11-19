#!/usr/bin/env python3
"""Oracle for the Slack plugin-adapter FLOW port, wave 2
(src/gateway/platforms/slack_adapter_flow2.c).

Drives the LIVE plugins/platforms/slack/adapter.py unclaimed decision chains
(26 funcs: module-level classifiers/renderers + SlackAdapter static/class/
instance methods) with SCRIPTED seams: every injected callable and claimed
sibling (_int_or_zero, _first_truthy, _render_inline_elements,
_attachment_label, the seamed _render_slack_table_block,
_slack_ignored_channels, logger.warning, time.monotonic, factory, ts_getter)
is a recording stub fed the same scripts the C harness feeds its universal
seam. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the
mutated state ("state").

The live defs are AST-extracted from adapter.py (module constants seeded,
Assign AND AnnAssign, plain-Name targets only; class methods dedented WITHOUT
decorators and bound onto SimpleNamespace self doubles / a bare cls double).
The heavy quarry import chain is never imported. Every exec namespace is a
registered sys.modules module (py3.14).

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError
included; those kinds PASS THROUGH every live except-Exception site).
Summary: 'ORACLE slack_adapter_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- {"__obj__": attrs} <-> SimpleNamespace; {"__set__": members} <-> set
  (canon sorts members); {"__tuple__": items} <-> tuple INPUT (tuple RESULTS
  canon to plain arrays); {"__exc__": spec} <-> REAL exception instances.
- state.slash_command_contexts: ordered [{"key","value"}] pairs <-> dict with
  tuple keys; state.slash_user_id: the _slash_user_id ContextVar value.
- logger seams record {"msg": literal, "args": [raw args]} VERBATIM.
- None-valued seam ARG keys are dropped on both sides.
- ASCII scope: strip/lower/split fold ASCII whitespace only; str() of
  non-strings covers str/int/bool/None; time.monotonic scripts carry
  non-integral floats; _discard_oldest_by_thread_ts carriers are dicts only
  (set iteration is hash-randomized live); dict-key flows use str keys.
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


MODULE_TARGETS = [
    "_slack_error_is", "_is_transient_transport_error",
    "_collect_slack_table_cell_text", "_render_slack_table_block",
    "_extract_text_from_slack_blocks", "_extract_urls_from_slack_blocks",
    "_apply_slack_proxy",
]
STATIC_TARGETS = [
    "_slack_timestamp_sort_key", "_trim_oldest_dict_entries",
    "_metadata_team_id", "_workspace_event_id", "_workspace_message_marker",
    "_truthy_config", "_task_update_chunk", "_stream_relation",
]
CLASS_TARGETS = ["_discard_oldest_by_thread_ts"]
INSTANCE_TARGETS = [
    "_lazy_attr", "_remember_channel_team", "_pop_slash_context",
    "_purge_stale_slash_contexts", "scope_id_for_chat", "_get_client",
    "_is_ignored_channel", "native_task_cards_enabled",
    "_describe_slack_api_error", "_warn_if_not_bot_token",
]
TARGETS = MODULE_TARGETS + STATIC_TARGETS + CLASS_TARGETS + INSTANCE_TARGETS
CONSTS = [
    "_SLACK_TABLE_MAX_CHARS", "_SLACK_API_ERROR_TEMPLATES",
    "_SLACK_PERMISSION_ERRORS", "_slash_user_id",
]


def extract_adapter():
    mod = new_module("sa2_oracle_adapter")
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
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in MODULE_TARGETS:
            exec_node(src, node, ns, A_SRC)
    cls = next((n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "SlackAdapter"), None)
    if cls is None:
        print("FATAL oracle_slack_adapter_flow2: SlackAdapter class not found")
        sys.exit(2)
    method_names = set(STATIC_TARGETS + CLASS_TARGETS + INSTANCE_TARGETS)
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in method_names:
            exec_node(src, node, ns, A_SRC)
    missing = [k for k in TARGETS + CONSTS if k not in ns]
    if missing:
        print(f"FATAL oracle_slack_adapter_flow2: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract_adapter()
REAL = {nm: NS[nm] for nm in TARGETS}


# ── canonical projection (compare parsed JSON, never raw strings) ──
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
    return v


def canon(v):
    return json.loads(json.dumps(project(v), ensure_ascii=False, default=str))


# ── exception materialization / projection ──
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
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def build_aio_classes(spec):
    """Real exception classes from the per-case aiohttp module double spec."""
    if not isinstance(spec, dict):
        return {}, {}
    cs = spec.get("classes") or {}
    memo = {}
    attrs = {}

    def build(name, depth=0):
        if name in memo:
            return memo[name]
        if depth > 32:
            return None
        parent = cs.get(name)
        cls = None
        if isinstance(parent, str):
            base = build(parent, depth + 1) if parent in cs else EXC_CLASSES.get(parent)
            if base is None:
                base = type(parent, (Exception,), {})
            cls = type(name, (base,), {})
        memo[name] = cls
        return cls

    for nm in cs:
        build(nm)
    for nm, parent in cs.items():
        if memo.get(nm) is not None:
            attrs[nm] = memo[nm]
        elif isinstance(parent, dict) and "const" in parent:
            attrs[nm] = parent["const"]
        else:
            attrs[nm] = None
    return {nm: c for nm, c in memo.items() if c is not None}, attrs


def materialize(v):
    if isinstance(v, dict):
        if "__obj__" in v:
            return types.SimpleNamespace(**{k: materialize(x) for k, x in (v["__obj__"] or {}).items()})
        if "__set__" in v:
            return set(materialize(x) for x in v["__set__"])
        if "__tuple__" in v:
            return tuple(materialize(x) for x in v["__tuple__"])
        return {k: materialize(x) for k, x in v.items()}
    if isinstance(v, list):
        return [materialize(x) for x in v]
    return v


def make_exc_instance(spec, aio_classes):
    kind = spec.get("type", "RuntimeError")
    cls = aio_classes.get(kind) or EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    inst = cls(spec.get("message", ""))
    for k, v in spec.items():
        if k in ("type", "message"):
            continue
        setattr(inst, k, materialize(v))
    return inst


# ── per-case script backend (mirrors the C sa2_call exactly) ──
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


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        args = {kk: vv for kk, vv in args.items() if vv is not None}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


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
def install_case(ctx, func):
    # generic seams first ...
    NS["logger"] = LoggerDouble(ctx)
    NS["time"] = types.SimpleNamespace(monotonic=rec(ctx, "time.monotonic", lambda: {}))
    NS["_int_or_zero"] = rec(ctx, "_int_or_zero", lambda value: {"value": value})
    NS["_first_truthy"] = rec(
        ctx, "_first_truthy", lambda mapping, keys: {"mapping": mapping, "keys": list(keys)})
    NS["_render_inline_elements"] = rec(
        ctx, "_render_inline_elements", lambda elements: {"elements": elements})
    NS["_attachment_label"] = rec(ctx, "_attachment_label", lambda file_obj: {"file_obj": file_obj})
    NS["_render_slack_table_block"] = rec(
        ctx, "_render_slack_table_block", lambda block, max_chars=20000: {"block": block})
    # ... then restore EVERY target to real (a target may also be a seamed
    # sibling of another flow), and keep the current target real.
    for nm, fn in REAL.items():
        NS[nm] = fn
    # the extract flow SEAMS its ported sibling _render_slack_table_block
    if func == "_extract_text_from_slack_blocks":
        NS["_render_slack_table_block"] = rec(
            ctx, "_render_slack_table_block", lambda block, max_chars=20000: {"block": block})


def mk_self(ctx, state):
    mat = {}
    for k, v in state.items():
        if k == "slash_command_contexts":
            # fixture key -> the real self attr (leading underscore)
            mat["_slash_command_contexts"] = {tuple(p["key"]): materialize(p["value"]) for p in v}
        else:
            mat[k] = materialize(v)
    self = types.SimpleNamespace(**mat)
    installed = set()
    for nm in INSTANCE_TARGETS:
        setattr(self, nm, types.MethodType(REAL[nm], self))
        installed.add(nm)
    # static targets called via self (self._trim_oldest_dict_entries,
    # self._truthy_config): plain functions as instance attrs are NOT
    # descriptor-bound, matching staticmethod call semantics.
    for nm in STATIC_TARGETS:
        setattr(self, nm, REAL[nm])
        installed.add(nm)
    for cname, cval in (("_SLASH_CTX_TTL", 120.0), ("_CHANNEL_TEAM_MAX", 10000)):
        if cname not in mat:
            setattr(self, cname, cval)
            installed.add(cname)
    self._slack_ignored_channels = rec(ctx, "_slack_ignored_channels", lambda: {})
    installed.add("_slack_ignored_channels")
    return self, installed


def project_self(self, installed):
    out = {}
    for k, v in vars(self).items():
        if k in installed:
            continue
        if k == "_slash_command_contexts" and isinstance(v, dict):
            out["slash_command_contexts"] = [
                {"key": [project(x) for x in kk], "value": project(vv)}
                for kk, vv in v.items()]
        else:
            out[k] = project(v)
    return out


class _ClsDouble:
    pass


_ClsDouble._slack_timestamp_sort_key = staticmethod(REAL["_slack_timestamp_sort_key"])


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    state = copy.deepcopy(inp.get("state") or {})
    ctx = Ctx(inp.get("io"))
    install_case(ctx, func)
    aio_classes, aio_attrs = build_aio_classes(state.get("aiohttp"))
    if aio_attrs:
        NS["aiohttp"] = types.SimpleNamespace(**aio_attrs)
    else:
        NS.pop("aiohttp", None)
    state_out = state
    try:
        if func == "_slack_error_is":
            ret = NS[func](make_exc_instance(args["exc"]["__exc__"], aio_classes), args["code"])
        elif func == "_is_transient_transport_error":
            ret = NS[func](make_exc_instance(args["exc"]["__exc__"], aio_classes))
        elif func == "_collect_slack_table_cell_text":
            ret = NS[func](materialize(args.get("value")))
        elif func == "_render_slack_table_block":
            block = materialize(args.get("block"))
            ret = NS[func](block, args["max_chars"]) if "max_chars" in args else NS[func](block)
        elif func == "_extract_text_from_slack_blocks":
            ret = NS[func](materialize(args.get("blocks")))
        elif func == "_extract_urls_from_slack_blocks":
            ret = NS[func](materialize(args.get("blocks")))
        elif func == "_apply_slack_proxy":
            client = materialize(state.get("client"))
            if "client" in state:
                state["client"] = client
            ret = NS[func](client, args.get("proxy_url"))
            if "client" in state:
                state["client"] = project(client) if client is not None else None
        elif func == "_slack_timestamp_sort_key":
            ret = NS[func](materialize(args.get("ts")))
        elif func == "_trim_oldest_dict_entries":
            ret = NS[func](state.get("mapping"), args["max_size"])
        elif func == "_discard_oldest_by_thread_ts":
            kw = {}
            if args.get("ts_getter"):
                kw["ts_getter"] = rec(ctx, "ts_getter", lambda e: {"e": e})
            ret = NS[func](_ClsDouble, state.get("entries"), args["count"], **kw)
        elif func == "_metadata_team_id":
            ret = NS[func](materialize(args.get("metadata")))
        elif func == "_workspace_event_id":
            ret = NS[func](args.get("team_id"), args.get("event_id"))
        elif func == "_workspace_message_marker":
            ret = NS[func](args.get("team_id"), args.get("message_id"))
        elif func == "_truthy_config":
            ret = NS[func](materialize(args.get("value")))
        elif func == "_task_update_chunk":
            ret = NS[func](materialize(args.get("task")))
        elif func == "_stream_relation":
            ret = NS[func](args["sent"], args["text"])
        elif func in INSTANCE_TARGETS:
            self, installed = mk_self(ctx, state)
            try:
                if func == "_lazy_attr":
                    ret = self._lazy_attr(args["name"], factory=rec(ctx, "factory", lambda: {}))
                elif func == "_remember_channel_team":
                    ret = self._remember_channel_team(args.get("channel_id"), args.get("team_id"))
                elif func == "_pop_slash_context":
                    cv = NS["_slash_user_id"]
                    token = cv.set(state.get("slash_user_id"))
                    try:
                        ret = self._pop_slash_context(args.get("chat_id"), args.get("team_id", ""))
                    finally:
                        cv.reset(token)
                elif func == "_purge_stale_slash_contexts":
                    ret = self._purge_stale_slash_contexts()
                elif func == "scope_id_for_chat":
                    ret = self.scope_id_for_chat(args.get("chat_id"))
                elif func == "_get_client":
                    ret = self._get_client(args.get("chat_id"), team_id=args.get("team_id"))
                elif func == "_is_ignored_channel":
                    ret = self._is_ignored_channel(args.get("chat_id"))
                elif func == "native_task_cards_enabled":
                    ret = self.native_task_cards_enabled()
                elif func == "_describe_slack_api_error":
                    ret = self._describe_slack_api_error(
                        materialize(args.get("response")), file_obj=materialize(args.get("file_obj")))
                elif func == "_warn_if_not_bot_token":
                    ret = self._warn_if_not_bot_token(
                        materialize(args.get("auth_response")), args.get("team_name"))
                else:
                    raise RuntimeError(f"unbound instance target {func}")
            finally:
                # state echo even on raise: partial mutations persist
                state_out = project_self(self, installed)
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = [
    "_int_or_zero", "_first_truthy", "_render_inline_elements",
    "_render_slack_table_block", "_attachment_label", "_slack_ignored_channels",
    "logger.warning", "time.monotonic", "factory", "ts_getter",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "slack_adapter_flow2.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_slack_adapter_flow2: seams absent from C port: {missing_c}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    if '"logger." + level' in osrc:
        used |= {"logger.warning"}
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_slack_adapter_flow2: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "slack_adapter_flow2.jsonl"
    if fix.exists():
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_slack_adapter_flow2: fixture io keys without a seam: {sorted(bad)}")
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
    print(f"ORACLE slack_adapter_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "slack_adapter_flow2.jsonl"
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
    print(f"SELFTEST slack_adapter_flow2: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
