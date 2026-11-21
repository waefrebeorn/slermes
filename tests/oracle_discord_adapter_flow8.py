#!/usr/bin/env python3
"""Oracle for the Discord adapter FLOW8 remainder port
(src/gateway/platforms/discord_adapter_flow8.py C source: discord_adapter_flow8.c).

Drives the LIVE plugins/platforms/discord/adapter.py defs with SCRIPTED seams:
claimed siblings, injected callables and attribute doubles are recording stubs
fed the SAME scripts the C harness feeds its universal seam.  Compares the
DECISION SEQUENCE (ordered seam events {"name","args","ret"} in "trace") plus
the final return ("ret") and the state echo.

The live defs are AST-extracted (module/class bodies, decorators stripped) into
a registered sys.modules namespace; the heavy quarry import chain is never
imported.  Fail-closed: missing defs or 0 cases => exit 2.  Per-case wrapper
catches BaseException (SystemExit included).  Summary:
'ORACLE discord_adapter_flow8: N cases, M mismatches'.

Representation contract (shared with the C port): see the port header.  None
== JSON null; a raise is {"__raise__": cls, "message": str(exc)}.  Real builtin
exception classes are used so except clause matching is exact.  Attribute
doubles are SimpleNamespace (object always truthy); getattr-default reads by
KEY PRESENCE; a truthy slot at a callable key fires the recorded seam.
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
MODNAME = "discord_adapter_flow8_oracle"

FREE_FUNCS = [
    "_abort_discord_websocket_transport",
]
INSTANCE_FUNCS = [
    "__contains__", "_snapshot", "_secret_key", "_dave_session",
    "_dave_protocol_version", "_dave_downgraded", "_cancel_voice_timeout",
    "voice_mixer_active", "is_in_voice_channel", "discard_pending_voice_input",
    "_disable_all", "_write_command_sync_state", "_record_command_sync_attempt",
    "_in_bot_thread", "_check_auth", "_check_slash_authorization",
]
PORTED = FREE_FUNCS + INSTANCE_FUNCS
# EXACTLY the C da8_call seam names (set-equality is enforced in cross_check).
SEAM_NAMES = [
    "task.cancel", "vc.is_connected", "receiver.discard_pending",
    "_command_sync_state_path", "atomic_json_write", "_update_command_sync_entry",
    "_discord_thread_require_mention", "current_gates", "_component_check_auth",
    "abort", "_evaluate_slash_authorization", "_reject_slash",
]


class _Thread:
    """Stand-in for discord.Thread so isinstance() is controllable per case."""


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    node = copy.copy(node)
    node.decorator_list = []
    text = textwrap.dedent(seg(src, node))
    text = re.sub(r'\bsuper\(\)', '__super__', text)
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def find_def(tree, name):
    for n in tree.body:
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name == name:
            return n
    for n in ast.walk(tree):
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name == name:
            return n
    return None


def extract():
    mod = new_module(MODNAME)
    ns = mod.__dict__
    ns.update({
        "asyncio": asyncio, "contextlib": contextlib, "suppress": contextlib.suppress,
        "os": os, "re": re, "json": json,
        "Any": object, "Optional": object, "dict": dict, "set": set, "list": list,
        "str": str, "bool": bool, "int": int, "bytes": bytes, "float": float,
        "logging": __import__("logging"), "logger": None,
        "discord": types.SimpleNamespace(Thread=_Thread),
        "atomic_json_write": None, "__super__": None,
    })
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    for nm in PORTED:
        node = find_def(tree, nm)
        if node is None:
            print(f"FATAL oracle_discord_adapter_flow8: missing def {nm}")
            sys.exit(2)
        exec_node(src, node, ns, A_SRC)
    missing = [k for k in PORTED if k not in ns]
    if missing:
        print(f"FATAL oracle_discord_adapter_flow8: missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()
REAL = {nm: NS[nm] for nm in PORTED}


# --- canonical projection (compare parsed JSON, never raw strings) ---
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


# --- exception materialization / projection (REAL builtin classes) ---
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


# --- attribute doubles ---
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


def seam_double(spec, ctx, seam_map):
    """Recursively build SimpleNamespace doubles; truthy keys in seam_map are callables."""
    if isinstance(spec, dict):
        d = types.SimpleNamespace()
        for k, v in spec.items():
            if k in seam_map and bool(v):
                setattr(d, k, rec(ctx, seam_map[k]))
            else:
                setattr(d, k, seam_double(v, ctx, seam_map))
        d.__spec__ = spec
        return d
    if isinstance(spec, (list, tuple)):
        return [seam_double(i, ctx, seam_map) for i in spec]
    return spec


def make_message(spec, ctx):
    d = types.SimpleNamespace()
    ch = (spec or {}).get("channel") or {}
    obj = _Thread() if bool(ch.get("__thread__")) else types.SimpleNamespace()
    obj.id = ch.get("id")
    d.channel = obj
    return d


def proj(x):
    return x.__spec__ if hasattr(x, "__spec__") else x


# --- per-case script backend (mirrors C da8_call exactly) ---
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

    NS["atomic_json_write"] = rec(
        ctx, "atomic_json_write",
        lambda path, state, indent=None, separators=None: {
            "path": proj(path), "state": state, "indent": indent,
            "separators": separators})

    # _check_auth's local import binds this module's stub via `from ... import`.
    for pkg in ("plugins", "plugins.platforms", "plugins.platforms.discord"):
        m = types.ModuleType(pkg)
        m.__path__ = []
        sys.modules[pkg] = m
    cam = types.ModuleType("plugins.platforms.discord.adapter_component_auth")
    cam._component_check_auth = rec(
        ctx, "_component_check_auth",
        lambda interaction, users, roles, live_auth=False: {
            "interaction": proj(interaction), "users": users,
            "roles": roles, "live_auth": live_auth})
    sys.modules["plugins.platforms.discord.adapter_component_auth"] = cam

    slf = types.SimpleNamespace()
    slf._ids = state.get("_ids", {})
    slf._max_tracked = state.get("_max_tracked", 3)
    slf._creds = state.get("_creds", [])
    slf._voice_timeout_tasks = {
        gid: seam_double(v, ctx, {"cancel": "task.cancel"})
        for gid, v in state.get("_voice_timeout_tasks", {}).items()}
    slf._voice_clients = {
        gid: seam_double(v, ctx, {"is_connected": "vc.is_connected"})
        for gid, v in state.get("_voice_clients", {}).items()}
    slf._voice_receivers = {
        gid: seam_double(v, ctx, {"discard_pending": "receiver.discard_pending"})
        for gid, v in state.get("_voice_receivers", {}).items()}
    slf._voice_mixers = state.get("_voice_mixers", None)
    slf.children = None
    slf._threads = state.get("_threads", [])
    slf.allowed_user_ids = state.get("allowed_user_ids", [])
    slf.allowed_role_ids = state.get("allowed_role_ids", [])
    slf.live_auth = state.get("live_auth", None)
    slf.current_gates = rec(ctx, "current_gates") if state.get("current_gates_callable") else None
    slf._command_sync_state_path = rec(ctx, "_command_sync_state_path")
    slf._update_command_sync_entry = rec(
        ctx, "_update_command_sync_entry",
        lambda app_id, fingerprint, *, keep_existing, drop=(): {
            "app_id": app_id, "fingerprint": fingerprint,
            "keep_existing": keep_existing, "drop": list(drop)})
    slf._discord_thread_require_mention = rec(ctx, "_discord_thread_require_mention")

    _ev = rec(ctx, "_evaluate_slash_authorization")
    _rj = rec(ctx, "_reject_slash",
              lambda interaction, command_text, reason: {
                  "interaction": proj(interaction), "command_text": command_text,
                  "reason": reason})

    async def _reject_slash(interaction, command_text, reason):
        return _rj(interaction, command_text, reason)
    slf._evaluate_slash_authorization = _ev
    slf._reject_slash = _reject_slash

    for nm in INSTANCE_FUNCS:
        setattr(slf, nm, types.MethodType(REAL[nm], slf))
    return ctx, slf


def drive(func, inp):
    args = inp.get("args") or {}
    state = copy.deepcopy(inp.get("state") or {})
    ctx, slf = setup_case(func, inp, state)
    try:
        if func == "__contains__":
            ret = slf.__contains__(args.get("message_id"))
        elif func == "_snapshot":
            ret = slf._snapshot()
            if "_ids" in state:
                state["_ids"] = slf._ids
        elif func in ("_secret_key", "_dave_session",
                      "_dave_protocol_version", "_dave_downgraded"):
            ret = getattr(slf, func)()
        elif func == "_cancel_voice_timeout":
            ret = slf._cancel_voice_timeout(args.get("guild_id"))
            if "_voice_timeout_tasks" in state:
                state["_voice_timeout_tasks"] = {
                    gid: proj(v) for gid, v in slf._voice_timeout_tasks.items()}
        elif func == "voice_mixer_active":
            ret = slf.voice_mixer_active(args.get("guild_id"))
        elif func == "is_in_voice_channel":
            ret = slf.is_in_voice_channel(args.get("guild_id"))
        elif func == "discard_pending_voice_input":
            ret = slf.discard_pending_voice_input(args.get("guild_id"))
        elif func == "_disable_all":
            slf.children = [types.SimpleNamespace(**dict(c)) for c in state.get("children", [])]
            ret = slf._disable_all()
            if "children" in state:
                state["children"] = [vars(c) for c in slf.children]
        elif func == "_write_command_sync_state":
            ret = slf._write_command_sync_state(args.get("state", {}))
        elif func == "_record_command_sync_attempt":
            ret = slf._record_command_sync_attempt(args.get("app_id"), args.get("fingerprint", ""))
        elif func == "_in_bot_thread":
            ret = slf._in_bot_thread(make_message(args.get("message"), ctx))
        elif func == "_check_auth":
            ret = slf._check_auth(attr_double(args.get("interaction")))
        elif func == "_abort_discord_websocket_transport":
            ret = NS[func](seam_double(args.get("websocket"), ctx, {"abort": "abort"}))
        elif func == "_check_slash_authorization":
            ret = slf._check_slash_authorization(
                attr_double(args.get("interaction")), args.get("command_text", ""))
        else:
            raise RuntimeError(f"unknown func {func}")
        if asyncio.iscoroutine(ret):
            ret = asyncio.run(ret)
    except BaseException as exc:
        ret = proj_exc(exc)
    finally:
        # Live state mutations happen before/independently of a seam raising;
        # mirror them even on the exception path (C mutates ctx.state in place).
        if func == "_cancel_voice_timeout" and "_voice_timeout_tasks" in state:
            state["_voice_timeout_tasks"] = {
                gid: proj(v) for gid, v in slf._voice_timeout_tasks.items()}
        elif func == "_snapshot" and "_ids" in state:
            state["_ids"] = slf._ids
        elif func == "_disable_all" and "children" in state and slf.children is not None:
            state["children"] = [vars(c) for c in slf.children]
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# --- three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ---
def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "discord_adapter_flow8.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    c_names = set(re.findall(r'da8_call\(\s*ctx,\s*"([^"]+)"', csrc))
    missing_c = registry - c_names
    if missing_c:
        print(f"FATAL oracle_discord_adapter_flow8: seams absent from C port: {sorted(missing_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    # seam_double wires these via a variable name, so add them explicitly.
    used |= {"abort", "task.cancel", "vc.is_connected", "receiver.discard_pending"}
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_discord_adapter_flow8: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    unstubbed = c_names - used
    if unstubbed:
        print(f"FATAL oracle_discord_adapter_flow8: C seams without oracle stub: {sorted(unstubbed)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow8.jsonl"
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
            print(f"FATAL oracle_discord_adapter_flow8: fixture io keys without a seam: {sorted(bad)}")
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
    print(f"ORACLE discord_adapter_flow8: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow8.jsonl"
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
    print(f"SELFTEST discord_adapter_flow8: {n} cases, {bad} drive/script failures")
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
