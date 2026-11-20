#!/usr/bin/env python3
"""Oracle for the Discord adapter FLOW5 message-construction/classification
port (src/gateway/platforms/discord_adapter_flow5.c).

Drives the LIVE plugins/platforms/discord/adapter.py defs with SCRIPTED seams:
module helpers, claimed siblings, discord.py ctors, regex calls, the
translation helper and logger calls are recording stubs fed the SAME scripts
the C harness feeds its universal seam. Compares the DECISION SEQUENCE
(ordered seam events {"name","args","ret"} in "trace") plus the final return
("ret") and the (unchanged) state echo.

The live defs are AST-extracted from adapter.py (top-level, DiscordAdapter
methods and the two nested view-class methods), decorators stripped, exec'd
into a registered sys.modules namespace. Only the one needed module constant
is seeded. The heavy quarry import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE discord_adapter_flow5: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}
  (OSError adds errno when set, message "[Errno N] msg"). make_exc builds the
  REAL builtin classes so except (ValueError, TypeError) catches exactly the
  ValueError/TypeError families; BaseException kinds pass through.
- Attribute doubles: JSON objects -> SimpleNamespace (always truthy, even
  empty); hasattr/getattr-default read by KEY PRESENCE (present-null is a
  present attribute whose value is None). Scalars keep value truthiness.
- str() positions carry strings/integral numbers/bools/None; int() parses
  ASCII digits with optional sign; strings are sliced/len'd by codepoint;
  str.strip() folds Unicode White_Space, str.lower() folds ASCII.
- re.sub/t/_truncate seam results are always strings in the fixture corpus
  (the C port reads them as string nodes); a raise descriptor propagates.
- state: only "_reply_to_mode" is read (by _reply_reference_for_send), absent
  -> None. No ported def mutates state; out.state echoes the input copy.
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
from typing import Any, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
A_SRC = os.path.join(Q, "plugins", "platforms", "discord", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag
MODNAME = "discord_adapter_flow5_oracle"

# The 17 UNCLAIMED defs ported here.
TOP_FUNCS = ["_derive_forum_thread_name"]
ADAPTER_METHODS = [
    "_message_channel_ids", "_thread_parent_channel", "_thread_created",
    "_get_parent_channel_id", "_interaction_guild_id", "_derive_auto_thread_name",
    "_forum_thread_parts", "_message_reference_from_ids",
    "_reply_reference_for_send", "format_tool_preview", "format_message",
    "_is_down_notice_content", "_is_reply_reference_rejected",
    "_is_length_overflow_error",
]
# nested-class methods: name -> enclosing class name
NESTED_METHODS = {"_first_embed": "_HermesView", "_config_embed": "ModelPickerView"}
PORTED = TOP_FUNCS + ADAPTER_METHODS + list(NESTED_METHODS)
STATIC_METHODS = {
    "_thread_created", "_forum_thread_parts",
    "_is_reply_reference_rejected", "_is_length_overflow_error",
    "_message_reference_from_ids", "_first_embed",
}
INSTANCE_FUNCS = [f for f in PORTED if f not in STATIC_METHODS and f not in TOP_FUNCS]
# one claimed sibling, seamed when it is not the case target
SEAMED_SIBLINGS = ["_message_reference_from_ids"]


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    node = copy.copy(node)
    node.decorator_list = []
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def find_def(tree, name, cls_name=None):
    """Find a FunctionDef/AsyncFunctionDef by name, optionally inside a class."""
    if cls_name is None:
        for n in tree.body:
            if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name == name:
                return n
    for node in ast.walk(tree):
        if isinstance(node, ast.ClassDef) and (cls_name is None or node.name == cls_name):
            for sub in node.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name == name:
                    return sub
    return None


def extract():
    mod = new_module(MODNAME)
    ns = mod.__dict__
    ns.update({
        "asyncio": asyncio, "os": os, "re": None, "json": json,
        "Any": Any, "Optional": Optional, "logging": __import__("logging"),
    })
    src = open(A_SRC, encoding="utf-8").read()
    # seed only the one module constant the ported defs reference
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "_DISCORD_EMBED_TITLE_LIMIT"
                for t in node.targets):
            try:
                exec(compile(seg(src, node), A_SRC, "exec", flags=_FLAGS, dont_inherit=True), ns)
            except Exception:
                ns["_DISCORD_EMBED_TITLE_LIMIT"] = 256
    tree = ast.parse(src)
    for nm in TOP_FUNCS:
        node = find_def(tree, nm)
        if node is None:
            print(f"FATAL oracle_discord_adapter_flow5: missing top def {nm}")
            sys.exit(2)
        exec_node(src, node, ns, A_SRC)
    cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "DiscordAdapter")
    for nm in ADAPTER_METHODS:
        node = find_def(cls, nm)
        if node is None:
            print(f"FATAL oracle_discord_adapter_flow5: missing adapter method {nm}")
            sys.exit(2)
        exec_node(src, node, ns, A_SRC)
    for nm, cname in NESTED_METHODS.items():
        node = find_def(tree, nm, cls_name=cname)
        if node is None:
            print(f"FATAL oracle_discord_adapter_flow5: missing nested method {nm}")
            sys.exit(2)
        exec_node(src, node, ns, A_SRC)
    ns["logger"] = _SilentLogger()
    missing = [k for k in PORTED + ["_DISCORD_EMBED_TITLE_LIMIT"] if k not in ns]
    if missing:
        print(f"FATAL oracle_discord_adapter_flow5: missing: {missing}")
        sys.exit(2)
    return ns


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


NS = extract()
REAL = {nm: NS[nm] for nm in PORTED}


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if isinstance(x, types.SimpleNamespace):
            return conv({k: val for k, val in vars(x).items() if k != "__spec__"})
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection (REAL builtin classes) ──
EXC_CLASSES = {
    "OSError": OSError, "IOError": OSError, "TimeoutError": TimeoutError,
    "PermissionError": PermissionError, "FileNotFoundError": FileNotFoundError,
    "FileExistsError": FileExistsError, "BlockingIOError": BlockingIOError,
    "ConnectionError": ConnectionError, "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "BrokenPipeError": BrokenPipeError,
    "InterruptedError": InterruptedError, "IsADirectoryError": IsADirectoryError,
    "NotADirectoryError": NotADirectoryError, "ChildProcessError": ChildProcessError,
    "ProcessLookupError": ProcessLookupError,
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "NameError": NameError,
    "IndexError": IndexError, "LookupError": LookupError, "ArithmeticError": ArithmeticError,
    "ZeroDivisionError": ZeroDivisionError, "OverflowError": OverflowError,
    "FloatingPointError": FloatingPointError, "AssertionError": AssertionError,
    "SyntaxError": SyntaxError, "IndentationError": IndentationError, "TabError": TabError,
    "SystemError": SystemError, "ReferenceError": ReferenceError,
    "MemoryError": MemoryError, "BufferError": BufferError, "StopIteration": StopIteration,
    "StopAsyncIteration": StopAsyncIteration, "NotImplementedError": NotImplementedError,
    "RecursionError": RecursionError, "UnboundLocalError": UnboundLocalError,
    "UnicodeError": UnicodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeTranslateError": UnicodeTranslateError,
    "Warning": Warning, "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
    "RuntimeWarning": RuntimeWarning, "SyntaxWarning": SyntaxWarning,
    "FutureWarning": FutureWarning, "PendingDeprecationWarning": PendingDeprecationWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "ResourceWarning": ResourceWarning,
    "EncodingWarning": EncodingWarning, "Exception": Exception,
    "ExceptionGroup": ExceptionGroup, "BaseExceptionGroup": BaseExceptionGroup,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": asyncio.CancelledError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    en = spec.get("errno")
    if en is not None and isinstance(cls, type) and issubclass(cls, OSError):
        return cls(en, msg)
    return cls(msg)


def proj_exc(exc):
    d = {"__raise__": type(exc).__name__, "message": str(exc)}
    if isinstance(exc, OSError) and exc.errno is not None:
        d["errno"] = exc.errno
    return d


# ── attribute doubles ──
def attr_double(spec):
    if isinstance(spec, dict):
        d = types.SimpleNamespace()
        for k, v in spec.items():
            setattr(d, k, attr_double(v))
        d.__spec__ = spec
        return d
    return spec


def proj(x):
    return x.__spec__ if hasattr(x, "__spec__") else x


# ── per-case script backend (mirrors C da5_call exactly) ──
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
        return ctx.call(name, args)
    return f


def method_rec(ctx, name, args_fn=None):
    def f(self, *a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        return ctx.call(name, args)
    return f


class RecLogger:
    def __init__(self, ctx):
        self._ctx = ctx

    def __getattr__(self, level):
        def log(fmt, *a, **k):
            return self._ctx.call(f"logger.{level}", {"fmt": fmt, "args": list(a)})
        return log


def setup_case(func, inp):
    ctx = Ctx(inp.get("io"))
    for nm in PORTED:
        NS[nm] = REAL[nm]
    NS["logger"] = RecLogger(ctx)
    NS["re"] = types.SimpleNamespace(
        search=rec(ctx, "re.search", lambda pattern, string: {"pattern": pattern, "text": string}),
        sub=rec(ctx, "re.sub", lambda pattern, repl, string:
                {"pattern": pattern, "repl": repl, "text": string}),
    )
    NS["t"] = rec(ctx, "t", lambda key: {"key": key})
    NS["_truncate_discord_component_text"] = rec(
        ctx, "_truncate_discord_component_text",
        lambda text, limit: {"text": text, "limit": limit})
    NS["_format_discord_markdown_link"] = rec(
        ctx, "_format_discord_markdown_link", lambda text, url: {"text": text, "url": url})
    NS["convert_table_to_bullets"] = rec(
        ctx, "convert_table_to_bullets", lambda content: {"content": content})
    NS["discord"] = types.SimpleNamespace(
        MessageReference=rec(ctx, "discord.MessageReference",
                             lambda **kw: {"message_id": kw.get("message_id"),
                                           "channel_id": kw.get("channel_id"),
                                           "guild_id": kw.get("guild_id"),
                                           "fail_if_not_exists": kw.get("fail_if_not_exists")}),
        Embed=rec(ctx, "discord.Embed",
                  lambda **kw: {"title": kw.get("title"),
                                "description": kw.get("description"),
                                "color": kw.get("color")}),
        Color=types.SimpleNamespace(blue=rec(ctx, "discord.Color.blue", lambda: {})),
    )
    # adapter self double (only _reply_to_mode is read by a ported def)
    slf = types.SimpleNamespace()
    slf._reply_to_mode = (inp.get("state") or {}).get("_reply_to_mode")
    # bind the case-target instance method to the self double
    if func in INSTANCE_FUNCS:
        setattr(slf, func, types.MethodType(REAL[func], slf))
    # seamed sibling (staticmethod in live; bind unbound on the instance)
    for nm in SEAMED_SIBLINGS:
        if nm != func:
            setattr(slf, nm, rec(ctx, nm, lambda message_id, channel: {
                "message_id": message_id, "channel": proj(channel)}))
    return ctx, slf


def drive(func, inp):
    args = inp.get("args") or {}
    state = copy.deepcopy(inp.get("state") or {})
    ctx, slf = setup_case(func, inp)
    try:
        if func == "_message_channel_ids":
            ret = slf._message_channel_ids(attr_double(args.get("message")))
        elif func == "_thread_parent_channel":
            ret = slf._thread_parent_channel(attr_double(args.get("channel")))
        elif func == "_thread_created":
            ret = NS[func](attr_double(args.get("thread")), args.get("name", ""))
        elif func == "_get_parent_channel_id":
            ret = slf._get_parent_channel_id(attr_double(args.get("channel")))
        elif func == "_interaction_guild_id":
            ret = slf._interaction_guild_id(attr_double(args.get("interaction")))
        elif func == "_derive_auto_thread_name":
            ret = slf._derive_auto_thread_name(args.get("content", ""))
        elif func == "_derive_forum_thread_name":
            ret = NS[func](args.get("message", ""))
        elif func == "_forum_thread_parts":
            ret = NS[func](attr_double(args.get("thread")))
        elif func == "_message_reference_from_ids":
            ret = NS[func](args.get("message_id"), attr_double(args.get("channel")))
        elif func == "_reply_reference_for_send":
            ret = slf._reply_reference_for_send(args.get("reply_to"), attr_double(args.get("channel")))
        elif func == "format_tool_preview":
            ret = slf.format_tool_preview(attr_double(args.get("preview")))
        elif func == "format_message":
            ret = slf.format_message(args.get("content", ""))
        elif func == "_is_down_notice_content":
            ret = slf._is_down_notice_content(args.get("content", ""))
        elif func in ("_is_reply_reference_rejected", "_is_length_overflow_error"):
            ret = NS[func](Exception(args.get("err", "")))
        elif func == "_first_embed":
            ret = NS[func](attr_double(args.get("message")))
        elif func == "_config_embed":
            ret = slf._config_embed(
                args.get("description", ""),
                title=args.get("title"),
                color=args.get("color"))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = [
    "_format_discord_markdown_link", "convert_table_to_bullets", "t",
    "_truncate_discord_component_text", "discord.MessageReference",
    "discord.Embed", "discord.Color.blue", "re.search", "re.sub",
    "logger.debug", "_message_reference_from_ids",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "discord_adapter_flow5.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    c_names = set(re.findall(r'da5_call\(\s*ctx,\s*"([^"]+)"', csrc))
    missing_c = registry - c_names
    if missing_c:
        print(f"FATAL oracle_discord_adapter_flow5: seams absent from C port: {sorted(missing_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'(?:rec|method_rec)\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= {"logger.debug"}
    used |= set(SEAMED_SIBLINGS)  # sibling stub is built with a variable name
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_discord_adapter_flow5: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    unstubbed = c_names - used
    if unstubbed:
        print(f"FATAL oracle_discord_adapter_flow5: C seams without oracle stub: {sorted(unstubbed)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow5.jsonl"
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
            print(f"FATAL oracle_discord_adapter_flow5: fixture io keys without a seam: {sorted(bad)}")
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
    print(f"ORACLE discord_adapter_flow5: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow5.jsonl"
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
        if not case.get("fc") and isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            bad += 1
            print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
    print(f"SELFTEST discord_adapter_flow5: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    missing = [f for f in PORTED if f not in counts]
    if missing:
        print(f"SELFTEST FUNCS WITHOUT CASES: {missing}")
        bad += 1
    low = [f for f in PORTED if counts.get(f, 0) < 6]
    if low:
        print(f"SELFTEST FUNCS WITH <6 CASES: {low}")
        bad += 1
    HAND = [
        ("_message_channel_ids", {"args": {"message": {"channel": {"id": "5", "parent_id": "9"}}}},
         ["5", "5", "9"]),
        ("_message_channel_ids", {"args": {"message": {"channel": {"id": "5"}}}},
         ["5", None, None]),
        ("_thread_parent_channel", {"args": {"channel": {"id": "1", "parent": {"id": "0"}}}},
         {"id": "0"}),
        ("_thread_created", {"args": {"thread": {"id": 7}, "name": "fallback"}},
         {"success": True, "thread_id": "7", "thread_name": "fallback", "thread": {"id": 7}}),
        ("_get_parent_channel_id", {"args": {"channel": {"parent": {"id": 3}}}}, "3"),
        ("_interaction_guild_id", {"args": {"interaction": {"guild_id": 42}}}, "42"),
        ("_is_reply_reference_rejected", {"args": {"err": "error code: 10008"}}, True),
        ("_is_length_overflow_error", {"args": {"err": "Error Code: 50035 fewer in length"}}, True),
        ("_first_embed", {"args": {"message": {"embeds": [{"title": "A"}]}}}, {"title": "A"}),
        ("format_message", {"args": {"content": ""}}, ""),
    ]
    for func, inp, want in HAND:
        out = drive(func, inp)
        if out["ret"] != canon(want):
            bad += 1
            print(f"SELFTEST HAND CHECK FAILED {func}: want={want!r} got={out['ret']!r}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
