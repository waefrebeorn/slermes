#!/usr/bin/env python3
"""Oracle for the Discord adapter FLOW6 remainder port
(src/gateway/platforms/discord_adapter_flow6.py).

Drives the LIVE plugins/platforms/discord/adapter.py defs with SCRIPTED seams:
module helpers, claimed siblings, config gate readers, discord.py-adjacent
imports and the translation/logger calls are recording stubs fed the SAME
scripts the C harness feeds its universal seam.  Compares the DECISION SEQUENCE
(ordered seam events {"name","args","ret"} in "trace") plus the final return
("ret") and the (unchanged) state echo.

The live defs are AST-extracted (module/nested/class bodies, decorators
stripped) into a registered sys.modules namespace; the heavy quarry import
chain is never imported.  Fail-closed: missing defs or 0 cases => exit 2.
Per-case wrapper catches BaseException (SystemExit included).  Summary:
'ORACLE discord_adapter_flow6: N cases, M mismatches'.

Representation contract (shared with the C port): see the port header.  None
== JSON null; a raise is {"__raise__": cls, "message": str(exc)} (OSError
adds errno).  Real builtin exception classes are used so except clause
matching is exact.  Attribute doubles are SimpleNamespace (object always
truthy); getattr-default reads by KEY PRESENCE.  Set returns are JSON arrays.
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
MODNAME = "discord_adapter_flow6_oracle"

# The 21 UNCLAIMED defs ported here.
FREE_FUNCS = [
    "_csv", "_t_discord", "_text", "_choice_label",
    "_default_voice_ack_phrases", "_unauthorized",
    "_standalone_warn_missing_media", "_env_bool",
]
INSTANCE_FUNCS = [
    "_EA_REASON_LABEL", "_EA_HEADER", "_EA_SMART_DENY_LINE",
    "_discord_channel_ids_allowed", "_is_pairing_approved_user",
    "_missed_message_backfill_enabled", "_get_effective_topic",
    "_approval_mention_content", "_missed_message_backfill_number",
    "_missed_message_backfill_limit", "_missed_message_backfill_window_seconds",
    "_missed_message_backfill_max_dispatches", "_missed_message_backfill_max_attempts",
]
PORTED = FREE_FUNCS + INSTANCE_FUNCS
# EXACTLY the C da6_call seam names (set-equality is enforced in cross_check).
SEAM_NAMES = [
    "t", "_truncate_discord_component_text", "_t_discord", "_text",
    "unauthorized_action_notice", "logger.warning", "_scoped_gate_env",
    "_get_allowed_channels", "PairingStore.is_approved", "_is_forum_parent",
    "_extra_or_env_flag", "_missed_message_backfill_number",
]


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


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


def find_def(tree, name, cls_name=None):
    if cls_name is None:
        for n in tree.body:
            if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name == name:
                return n
        for n in ast.walk(tree):
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
        "asyncio": asyncio, "os": os, "re": re, "json": json,
        "Any": object, "Optional": object,
        "logging": __import__("logging"), "logger": _SilentLogger(),
        "_DISCORD_APP_COMMAND_TEXT_LIMIT": 100,
        "Platform": types.SimpleNamespace(DISCORD="discord"),
    })
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    for nm in FREE_FUNCS:
        node = find_def(tree, nm)
        if node is None:
            print(f"FATAL oracle_discord_adapter_flow6: missing free def {nm}")
            sys.exit(2)
        exec_node(src, node, ns, A_SRC)
    cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "DiscordAdapter")
    for nm in INSTANCE_FUNCS:
        node = find_def(cls, nm)
        if node is None:
            print(f"FATAL oracle_discord_adapter_flow6: missing adapter method {nm}")
            sys.exit(2)
        exec_node(src, node, ns, A_SRC)
    missing = [k for k in PORTED if k not in ns]
    if missing:
        print(f"FATAL oracle_discord_adapter_flow6: missing: {missing}")
        sys.exit(2)
    return ns


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


# ── per-case script backend (mirrors C da6_call exactly) ──
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
    NS["t"] = rec(ctx, "t", lambda key, **kwargs: {"key": key, "kwargs": kwargs})
    NS["_truncate_discord_component_text"] = rec(
        ctx, "_truncate_discord_component_text", lambda text, limit: {"text": text, "limit": limit})
    # _t_discord / _text are themselves ported targets: only stub them when they
    # are called as a sibling rather than being the case target.
    if func != "_t_discord":
        NS["_t_discord"] = rec(ctx, "_t_discord", lambda key, limit: {"key": key, "limit": limit})
    if func != "_text":
        NS["_text"] = rec(ctx, "_text", lambda key: {"key": key})
    NS["_scoped_gate_env"] = rec(
        ctx, "_scoped_gate_env", lambda name, default=None: {"name": name, "default": default})
    NS["unauthorized_action_notice"] = rec(
        ctx, "unauthorized_action_notice", lambda platform: {"platform": getattr(platform, "value", platform)})
    NS["Platform"] = types.SimpleNamespace(DISCORD="discord")

    # fake gateway.pairing so the live `from gateway.pairing import PairingStore` is cheap
    g = types.ModuleType("gateway"); g.__path__ = []
    pm = types.ModuleType("gateway.pairing")
    _ps = rec(ctx, "PairingStore.is_approved",
              lambda platform, user_id: {"platform": platform, "user_id": user_id})

    class PairingStore:
        def is_approved(self, platform, user_id):
            return _ps(platform, user_id)
    pm.PairingStore = PairingStore
    sys.modules["gateway"] = g
    sys.modules["gateway.pairing"] = pm

    state = inp.get("state") or {}
    slf = types.SimpleNamespace()
    slf.config = types.SimpleNamespace(extra=state.get("config_extra", {}))
    slf._allowed_user_ids = state.get("_allowed_user_ids", [])
    if func in INSTANCE_FUNCS:
        setattr(slf, func, types.MethodType(REAL[func], slf))

    _allowed_rec = rec(ctx, "_get_allowed_channels")
    def _allowed():
        v = _allowed_rec()
        return set(v) if isinstance(v, list) else v
    setattr(slf, "_get_allowed_channels", _allowed)
    setattr(slf, "_is_forum_parent",
            rec(ctx, "_is_forum_parent", lambda parent: {"parent": proj(parent)}))
    setattr(slf, "_extra_or_env_flag",
            rec(ctx, "_extra_or_env_flag",
                lambda key, env_key, default, *, truthy: {
                    "key": key, "env_key": env_key, "default": default, "truthy": truthy}))
    if func != "_missed_message_backfill_number":
        setattr(slf, "_missed_message_backfill_number",
                rec(ctx, "_missed_message_backfill_number",
                    lambda key, env_key, default, cast, lo, hi=None: {
                        "key": key, "env_key": env_key, "default": default,
                        "cast": cast.__name__, "lo": lo, "hi": hi}))
    return ctx, slf


def drive(func, inp):
    args = inp.get("args") or {}
    state = copy.deepcopy(inp.get("state") or {})
    ctx, slf = setup_case(func, inp)
    try:
        if func == "_csv":
            ret = NS[func](args.get("value"))
        elif func == "_t_discord":
            ret = NS[func](args.get("key", ""), args.get("limit", 0),
                           **(args.get("kwargs") or {}))
        elif func == "_text":
            ret = NS[func](args.get("key", ""))
        elif func == "_choice_label":
            ret = NS[func](args.get("label", ""))
        elif func == "_default_voice_ack_phrases":
            ret = NS[func]()
        elif func == "_unauthorized":
            ret = NS[func]()
        elif func == "_standalone_warn_missing_media":
            ret = NS[func](args.get("media_path", ""))
        elif func == "_env_bool":
            ret = NS[func](args.get("name", ""), args.get("default", False))
        elif func in ("_EA_REASON_LABEL", "_EA_HEADER", "_EA_SMART_DENY_LINE"):
            ret = getattr(slf, func)()
        elif func == "_discord_channel_ids_allowed":
            ret = slf._discord_channel_ids_allowed(set(args.get("channel_ids") or []))
        elif func == "_is_pairing_approved_user":
            ret = slf._is_pairing_approved_user(args.get("user_id"))
        elif func == "_missed_message_backfill_enabled":
            ret = slf._missed_message_backfill_enabled()
        elif func == "_get_effective_topic":
            ret = slf._get_effective_topic(attr_double(args.get("channel")),
                                           args.get("is_thread", False))
        elif func == "_approval_mention_content":
            ret = slf._approval_mention_content()
        elif func == "_missed_message_backfill_number":
            ret = slf._missed_message_backfill_number(
                args.get("key", ""), args.get("env_key", ""), args.get("default"),
                {"int": int, "float": float}.get(args.get("cast", "int"), int),
                args.get("lo", 0), args.get("hi"))
        elif func in ("_missed_message_backfill_limit", "_missed_message_backfill_window_seconds",
                      "_missed_message_backfill_max_dispatches", "_missed_message_backfill_max_attempts"):
            ret = getattr(slf, func)()
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "discord_adapter_flow6.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    c_names = set(re.findall(r'da6_call\(\s*ctx,\s*"([^"]+)"', csrc))
    missing_c = registry - c_names
    if missing_c:
        print(f"FATAL oracle_discord_adapter_flow6: seams absent from C port: {sorted(missing_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= {"logger.warning"}
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_discord_adapter_flow6: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    unstubbed = c_names - used
    if unstubbed:
        print(f"FATAL oracle_discord_adapter_flow6: C seams without oracle stub: {sorted(unstubbed)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow6.jsonl"
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
            print(f"FATAL oracle_discord_adapter_flow6: fixture io keys without a seam: {sorted(bad)}")
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
    print(f"ORACLE discord_adapter_flow6: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow6.jsonl"
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
    print(f"SELFTEST discord_adapter_flow6: {n} cases, {bad} drive/script failures")
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
        ("_csv", {"args": {"value": ["a", 2, True, None]}}, "a,2,True,None"),
        ("_csv", {"args": {"value": "solo"}}, "solo"),
        ("_choice_label", {"args": {"label": "NoDot"}}, "NoDot"),
        ("_EA_SMART_DENY_LINE", {"args": {}}, "\n\n**Warning**"),
        ("_discord_channel_ids_allowed", {"args": {"channel_ids": []}}, False),
        ("_is_pairing_approved_user", {"args": {"user_id": ""}}, False),
        ("_EA_REASON_LABEL", {"args": {}}, "**Reason:** "),
    ]
    for func, inp, want in HAND:
        # hand checks that need seams use io; skip those lacking a script
        if func in ("_EA_SMART_DENY_LINE", "_EA_REASON_LABEL") and not (inp.get("in") or {}).get("io"):
            continue
        out = drive(func, {"args": inp.get("args") or {}})
        if out["ret"] != canon(want):
            bad += 1
            print(f"SELFTEST HAND CHECK FAILED {func}: want={want!r} got={out['ret']!r}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
