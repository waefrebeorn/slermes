#!/usr/bin/env python3
"""Oracle for the Discord adapter FLOW7 remainder port
(src/gateway/platforms/discord_adapter_flow7.c).

Drives the LIVE plugins/platforms/discord/adapter.py defs with SCRIPTED seams:
local imports, claimed siblings, config readers and the response/attachment
attribute doubles are recording stubs fed the SAME scripts the C harness feeds
its universal seam.  Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the state
echo (two ported defs mutate the forum-probe cache in state).

The live defs are AST-extracted (module/nested/class bodies, decorators
stripped) into a registered sys.modules namespace; the heavy quarry import
chain is never imported.  Fail-closed: missing defs or 0 cases => exit 2.
Per-case wrapper catches BaseException (SystemExit included).  Summary:
'ORACLE discord_adapter_flow7: N cases, M mismatches'.

Representation contract (shared with the C port): see the port header.  None
== JSON null; a raise is {"__raise__": cls, "message": str(exc)} (OSError
adds errno).  Real builtin exception classes are used so except clause
matching is exact.  Attribute doubles are SimpleNamespace (object always
truthy); getattr-default reads by KEY PRESENCE.  MessageType members are
represented by their NAME.  "super()" is rewritten to a recording stub.
"""
import ast
import asyncio
import contextlib
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
MODNAME = "discord_adapter_flow7_oracle"

# The 18 UNCLAIMED defs ported here.
FREE_FUNCS = [
    "_standalone_response_encoding", "_standalone_close_response",
    "_remember_channel_is_forum", "_probe_is_forum_cached",
    "_build_allowed_mentions",
]
INSTANCE_FUNCS = [
    "_load_discord_int_config", "_load_voice_timeout", "_load_playback_timeout",
    "_voice_timeout_limit", "_playback_timeout_limit",
    "_exec_approval_cmd_budget", "_text_batch_delay_for", "_resolve_channel_prompt",
    "_playback_timeout_for_audio", "_attachment_message_type",
    "_guild_member_has_role", "_reply_target", "_stamp_auto_thread_name",
]
PORTED = FREE_FUNCS + INSTANCE_FUNCS
# EXACTLY the C da7_call seam names (set-equality is enforced in cross_check).
SEAM_NAMES = [
    "resp.get_encoding", "resp.close", "resp.release",
    "_extra_or_secret", "discord.AllowedMentions",
    "read_raw_config", "logger.debug", "_load_discord_int_config",
    "_approval_mention_content", "super._exec_approval_cmd_budget",
    "super._text_batch_delay_for", "resolve_channel_prompt",
    "_playback_timeout_limit", "_probe_audio_duration_seconds",
    "_is_discord_voice_message_attachment", "guild.get_member", "_Snowflake",
]


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


class SuperRewrite(ast.NodeTransformer):
    """Rewrite zero-arg super() into the recorded __super__ stub namespace."""
    def visit_Call(self, node):
        self.generic_visit(node)
        if (isinstance(node.func, ast.Name) and node.func.id == "super"
                and not node.args and not node.keywords):
            return ast.copy_location(ast.Name(id="__super__", ctx=ast.Load()), node)
        return node


def exec_node(src, node, ns, path):
    node = copy.copy(node)
    node.decorator_list = []
    text = textwrap.dedent(seg(src, node))
    # zero-arg super() needs a __class__ cell that a detached function lacks;
    # bind it to the recorded __super__ stub namespace instead.
    text = re.sub(r'\bsuper\(\)', '__super__', text)
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


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
        "asyncio": asyncio, "contextlib": contextlib, "suppress": contextlib.suppress,
        "os": os, "re": re, "json": json,
        "Any": object, "Optional": object, "dict": dict, "set": set, "str": str,
        "logging": __import__("logging"), "logger": None,
        # MessageType members are represented by their NAME strings.
        "MessageType": types.SimpleNamespace(
            PHOTO="PHOTO", VIDEO="VIDEO", VOICE="VOICE", AUDIO="AUDIO",
            DOCUMENT="DOCUMENT"),
        "discord": types.SimpleNamespace(), "DISCORD_AVAILABLE": True,
        "_extra_or_secret": None, "_Snowflake": None, "__super__": None,
    })
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    for nm in FREE_FUNCS:
        node = find_def(tree, nm)
        if node is None:
            print(f"FATAL oracle_discord_adapter_flow7: missing free def {nm}")
            sys.exit(2)
        exec_node(src, node, ns, A_SRC)
    cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "DiscordAdapter")
    for nm in INSTANCE_FUNCS:
        node = find_def(cls, nm)
        if node is None:
            print(f"FATAL oracle_discord_adapter_flow7: missing adapter method {nm}")
            sys.exit(2)
        exec_node(src, node, ns, A_SRC)
    missing = [k for k in PORTED if k not in ns]
    if missing:
        print(f"FATAL oracle_discord_adapter_flow7: missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()
REAL = {nm: NS[nm] for nm in PORTED}


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if isinstance(x, types.SimpleNamespace):
            return conv({k: val for k, val in vars(x).items() if k != "__spec__"})
        if isinstance(x, (set, frozenset)):
            return [conv(i) for i in x]
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
    if isinstance(spec, (list, tuple)):
        return [attr_double(i) for i in spec]
    return spec


def make_resp(spec, ctx):
    """Response double: truthy close/release/get_encoding values are callables."""
    if not isinstance(spec, dict):
        return spec
    d = types.SimpleNamespace()
    for k, v in spec.items():
        if k in ("close", "release", "get_encoding") and bool(v):
            if k == "get_encoding":
                setattr(d, k, lambda: ctx.call("resp.get_encoding", {}))
            elif k == "close":
                setattr(d, k, lambda: ctx.call("resp.close", {}))
            else:
                setattr(d, k, lambda: ctx.call("resp.release", {}))
        else:
            setattr(d, k, attr_double(v))
    d.__spec__ = spec
    return d


def make_guild(spec, ctx):
    """Guild double whose get_member() fires the recorded seam."""
    if not isinstance(spec, dict):
        return spec
    d = types.SimpleNamespace()
    for k, v in spec.items():
        if k != "get_member":
            setattr(d, k, attr_double(v))
    def _get_member(user_id):
        v = ctx.call("guild.get_member", {"user_id": user_id})
        return attr_double(v) if isinstance(v, dict) else v
    d.get_member = _get_member
    d.__spec__ = spec
    return d


def proj(x):
    return x.__spec__ if hasattr(x, "__spec__") else x


# ── per-case script backend (mirrors C da7_call exactly) ──
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


def setup_case(func, inp, state):
    ctx = Ctx(inp.get("io"))
    for nm in PORTED:
        NS[nm] = REAL[nm]
    NS["logger"] = types.SimpleNamespace(debug=rec(
        ctx, "logger.debug", lambda fmt, *a, **k: {"fmt": fmt, "args": list(a)}))
    NS["DISCORD_AVAILABLE"] = state.get("DISCORD_AVAILABLE", True)
    NS["_extra_or_secret"] = rec(
        ctx, "_extra_or_secret",
        lambda configured, key, name, default=None: {
            "configured": configured, "key": key, "name": name, "default": default})
    NS["discord"] = types.SimpleNamespace(AllowedMentions=rec(
        ctx, "discord.AllowedMentions", lambda **kw: dict(kw)))
    NS["_Snowflake"] = rec(ctx, "_Snowflake", lambda id: {"id": id})
    # The forum-probe cache is a module global in the live code; bind it to the
    # echoed state dict. Only the mutator adds the key when it is absent.
    if func == "_remember_channel_is_forum":
        NS["_DISCORD_CHANNEL_TYPE_PROBE_CACHE"] = state.setdefault("_channel_type_probe_cache", {})
    else:
        NS["_DISCORD_CHANNEL_TYPE_PROBE_CACHE"] = state.get("_channel_type_probe_cache", {})

    # register lightweight local-import modules (the live defs import inside body)
    g = types.ModuleType("gateway"); g.__path__ = []
    gp = types.ModuleType("gateway.platforms"); gp.__path__ = []
    gpb = types.ModuleType("gateway.platforms.base")
    gpb.resolve_channel_prompt = rec(
        ctx, "resolve_channel_prompt",
        lambda extra, channel_id, parent_id=None: {
            "extra": extra, "channel_id": channel_id, "parent_id": parent_id})
    sys.modules["gateway"] = g
    sys.modules["gateway.platforms"] = gp
    sys.modules["gateway.platforms.base"] = gpb
    hc = types.ModuleType("hermes_cli"); hc.__path__ = []
    hcc = types.ModuleType("hermes_cli.config")
    hcc.read_raw_config = rec(ctx, "read_raw_config")
    sys.modules["hermes_cli"] = hc
    sys.modules["hermes_cli.config"] = hcc

    NS["__super__"] = types.SimpleNamespace(
        _exec_approval_cmd_budget=rec(
            ctx, "super._exec_approval_cmd_budget",
            lambda description, smart_denied: {
                "description": description, "smart_denied": smart_denied}),
        _text_batch_delay_for=rec(
            ctx, "super._text_batch_delay_for", lambda pending: {"pending": proj(pending)}))

    slf = types.SimpleNamespace()
    slf.config = types.SimpleNamespace(extra=state.get("config_extra", {}))
    slf.VOICE_TIMEOUT = state.get("VOICE_TIMEOUT", 300)
    slf.PLAYBACK_TIMEOUT = state.get("PLAYBACK_TIMEOUT", 120)
    slf.PLAYBACK_TIMEOUT_PADDING = state.get("PLAYBACK_TIMEOUT_PADDING", 30)
    for k in ("_voice_timeout_seconds", "_playback_timeout_seconds"):
        if k in state:
            setattr(slf, k, state[k])
    # C falls back to 0.0 when the split delay is absent from state.
    slf._text_batch_split_delay_seconds = state.get("_text_batch_split_delay_seconds", 0.0)
    for nm in INSTANCE_FUNCS:
        setattr(slf, nm, types.MethodType(REAL[nm], slf))

    # claimed siblings that are seams when the target is a different func
    if func != "_load_discord_int_config":
        slf._load_discord_int_config = rec(
            ctx, "_load_discord_int_config",
            lambda key, default, minimum=0: {"key": key, "default": default, "minimum": minimum})
    if func != "_playback_timeout_limit":
        slf._playback_timeout_limit = rec(ctx, "_playback_timeout_limit")
    # _approval_mention_content is flow6-ported, so it is never a live def here:
    # always the recorded sibling seam.
    slf._approval_mention_content = rec(ctx, "_approval_mention_content")
    slf._probe_audio_duration_seconds = rec(
        ctx, "_probe_audio_duration_seconds", lambda audio_path: {"audio_path": audio_path})
    setattr(slf, "_is_discord_voice_message_attachment", rec(
        ctx, "_is_discord_voice_message_attachment", lambda att: {"att": proj(att)}))
    return ctx, slf


def drive(func, inp):
    args = inp.get("args") or {}
    state = copy.deepcopy(inp.get("state") or {})
    ctx, slf = setup_case(func, inp, state)
    try:
        if func == "_standalone_response_encoding":
            ret = NS[func](make_resp(args.get("resp"), ctx))
        elif func == "_standalone_close_response":
            ret = NS[func](make_resp(args.get("resp"), ctx))
        elif func == "_remember_channel_is_forum":
            ret = NS[func](args.get("chat_id"), args.get("is_forum"))
        elif func == "_probe_is_forum_cached":
            ret = NS[func](args.get("chat_id"))
        elif func == "_build_allowed_mentions":
            ret = NS[func](args.get("extra"))
        elif func == "_load_discord_int_config":
            ret = slf._load_discord_int_config(
                args.get("key", ""), args.get("default", 0), minimum=args.get("minimum", 0))
        elif func in ("_load_voice_timeout", "_load_playback_timeout",
                      "_voice_timeout_limit", "_playback_timeout_limit"):
            ret = getattr(slf, func)()
        elif func == "_exec_approval_cmd_budget":
            ret = slf._exec_approval_cmd_budget(
                args.get("description", ""), args.get("smart_denied", False))
        elif func == "_text_batch_delay_for":
            ret = slf._text_batch_delay_for(attr_double(args.get("pending")))
        elif func == "_resolve_channel_prompt":
            ret = slf._resolve_channel_prompt(args.get("channel_id", ""), args.get("parent_id"))
        elif func == "_playback_timeout_for_audio":
            ret = slf._playback_timeout_for_audio(args.get("audio_path", ""))
        elif func == "_attachment_message_type":
            ret = slf._attachment_message_type(attr_double(args.get("att")))
        elif func == "_guild_member_has_role":
            ret = NS[func](make_guild(args.get("guild"), ctx), args.get("user_id"),
                           set(args.get("allowed_roles") or []))
        elif func == "_reply_target":
            ret = NS[func](attr_double(args.get("reference")))
        elif func == "_stamp_auto_thread_name":
            ret = NS[func](attr_double(args.get("thread")), args.get("thread_name"))
        else:
            raise RuntimeError(f"unknown func {func}")
        if asyncio.iscoroutine(ret):
            ret = asyncio.run(ret)
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "discord_adapter_flow7.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    c_names = set(re.findall(r'da7_call\(\s*ctx,\s*"([^"]+)"', csrc))
    missing_c = registry - c_names
    if missing_c:
        print(f"FATAL oracle_discord_adapter_flow7: seams absent from C port: {sorted(missing_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    # response callables are driven through make_resp's ctx.call, not rec()
    used |= {"resp.get_encoding", "resp.close", "resp.release", "guild.get_member"}
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_discord_adapter_flow7: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    unstubbed = c_names - used
    if unstubbed:
        print(f"FATAL oracle_discord_adapter_flow7: C seams without oracle stub: {sorted(unstubbed)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow7.jsonl"
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
            print(f"FATAL oracle_discord_adapter_flow7: fixture io keys without a seam: {sorted(bad)}")
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
    if want.get("state") != got.get("state"):
        print(f"  state want={json.dumps(want.get('state'), ensure_ascii=False)[:300]}")
        print(f"  state got ={json.dumps(got.get('state'), ensure_ascii=False)[:300]}")
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
    print(f"ORACLE discord_adapter_flow7: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow7.jsonl"
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
    print(f"SELFTEST discord_adapter_flow7: {n} cases, {bad} drive/script failures")
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
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
