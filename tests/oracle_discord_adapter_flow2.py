#!/usr/bin/env python3
"""Oracle for the Discord adapter FLOW remainder port
(src/gateway/platforms/discord_adapter_flow2.c).

Drives the LIVE plugins/platforms/discord/adapter.py gate / debounce /
mention / channel-key / command-sync-canonicalization methods with SCRIPTED
seams: every sibling method, module helper, injected callable, logger call
and time.monotonic read is a recording stub fed the same scripts the C
harness feeds its universal seam. Compares the DECISION SEQUENCE (ordered
seam events {"name","args","ret"} in "trace") plus the final return ("ret")
and the state echo ("state" — _snapshot_gate_env / _record_bot_tag_debounce
/ _is_bot_tag_debounce_continuation mutate adapter state).

The live defs are AST-extracted from adapter.py (module constants seeded,
Assign AND AnnAssign, plain-Name targets only; logger re-set to the recorder
AFTER seeding). Every exec namespace is a registered sys.modules module
(py3.14). The heavy quarry import chain is never imported; the module
helpers (_scoped_gate_env / _extra_or_secret / _decode_json_list_literal /
is_automated_message) are NS-level recorders.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE discord_adapter_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}
  (OSError adds errno when set, message "[Errno N] msg"). BaseException kinds
  (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) pass through
  every except-Exception site; make_exc builds the REAL builtin classes so
  except Exception swallows exactly the Exception family.
- Attribute doubles (message/channel/author/parent/att/command/config
  param): JSON objects -> SimpleNamespace (always truthy, even empty);
  scalars keep value truthiness. config.extra and the config PARAM's extra
  stay RAW dicts (isinstance dict gates). client.user / mentions entries are
  UserDoubles compared by id equality (discord.py User semantics).
- Set returns project to SORTED deduped lists on both sides; set-returning
  seams are scripted as lists (membership-only consumers).
- state mutation echo: _gate_env_snapshot / _bot_tag_debounce_until appear
  in out.state iff present in input OR written non-empty (snapshot: written
  non-None). Absent debounce dict is created on first write.
- is_voice_message marker: null/absent -> duration+waveform probe; "seam" ->
  callable recording stub; any other value -> raw non-callable attribute.
- Seam args keep explicit nulls; logger records {"fmt","args"} verbatim (no
  %-formatting); the voice-failure logger arg projects str(exc).
- Fixture corpus: str() positions carry strings/integral numbers/bools/None;
  int() string parsing folds ASCII digits + Unicode White_Space strip;
  lower() folds ASCII; mention regex \\d folds ASCII; debounce values and
  batch-delay fields are numbers; _username_resolved_ids elements are str;
  to_dict / _canonicalize_* seam results are dicts; contexts /
  integration_types / options / choices / mentions are lists or falsy
  (str/dict carriers iterate); client.user and mention entries carry id.
"""
import ast
import asyncio
import copy
import json
import math
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
MODNAME = "discord_adapter_flow2_oracle"


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


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


# All 36 ported defs are DiscordAdapter methods (4 staticmethods).
METHOD_FUNCS = [
    "_extra_or_env_flag", "_discord_require_mention",
    "_discord_free_response_auto_thread", "_discord_thread_require_mention",
    "_discord_history_backfill", "_discord_max_attachment_bytes",
    "_is_discord_voice_message_attachment", "_snapshot_gate_env",
    "_gate_env", "_gate_raw", "_gate_csv_set",
    "_get_allowed_channels", "_get_ignored_channels", "_get_no_thread_channels",
    "resolved_allowlist_user_ids", "_discord_allow_all_users",
    "_gateway_allow_all_users", "_get_allow_bots",
    "_bot_tag_debounce_key", "_bot_tag_window_seconds",
    "_record_bot_tag_debounce", "_is_bot_tag_debounce_continuation",
    "_discord_free_response_channels", "_raw_mentioned_user_ids",
    "_self_is_explicitly_mentioned", "_self_is_raw_mentioned",
    "_discord_bots_require_inline_mention", "_discord_channel_keys",
    "_discord_channel_keys_from_channel", "_discord_history_backfill_limit",
    "_get_discord_command_sync_policy", "_normalize_permissions",
    "_canonicalize_app_command_option", "_canonicalize_app_command_payload",
    "_existing_command_to_payload", "_patchable_app_command_payload",
]
STATIC_METHODS = {
    "_is_discord_voice_message_attachment", "_gate_csv_set",
    "_bot_tag_debounce_key", "_normalize_permissions",
}


def extract():
    mod = new_module(MODNAME)
    ns = mod.__dict__
    ns.update({
        "asyncio": asyncio, "math": math, "os": os, "re": re, "json": json,
        "Any": Any, "Optional": Optional, "logging": __import__("logging"),
    })
    src = open(A_SRC, encoding="utf-8").read()
    seed_constants(src, ns, A_SRC)
    tree = ast.parse(src)
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "DiscordAdapter")
    for sub in cls.body:
        if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in METHOD_FUNCS:
            sub = copy.copy(sub)
            sub.decorator_list = []
            exec_node(src, sub, ns, A_SRC)
    ns["logger"] = _SilentLogger()
    required = METHOD_FUNCS + ["_GATE_ENV_KEYS", "_DISCORD_COMMAND_SYNC_POLICIES"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_discord_adapter_flow2: missing: {missing}")
        sys.exit(2)
    return ns


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


NS = extract()
REAL = {nm: NS[nm] for nm in METHOD_FUNCS}


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
    "SyntaxError": SyntaxError, "IndentationError": IndentationError,
    "TabError": TabError, "SystemError": SystemError, "ReferenceError": ReferenceError,
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
_MISS = object()


class UserDouble(types.SimpleNamespace):
    """discord.py User semantics: equality by id (str/int corpus)."""
    def __eq__(self, other):
        if isinstance(other, UserDouble):
            return getattr(self, "id", _MISS) == getattr(other, "id", _MISS)
        return NotImplemented

    def __hash__(self):
        return hash(getattr(self, "id", None))


def attr_double(spec):
    if isinstance(spec, dict):
        d = types.SimpleNamespace()
        for k, v in spec.items():
            setattr(d, k, attr_double(v))
        d.__spec__ = spec
        return d
    return spec


def proj(x):
    if hasattr(x, "__spec__"):
        return x.__spec__
    return x


def message_double(spec):
    d = attr_double(spec)
    if isinstance(d, types.SimpleNamespace):
        m = getattr(d, "mentions", None)
        if isinstance(m, list):
            out = []
            for e in m:
                if isinstance(e, dict):
                    out.append(UserDouble(**{k: attr_double(v) for k, v in e.items()}))
                elif isinstance(e, types.SimpleNamespace):
                    out.append(UserDouble(**{k: v for k, v in vars(e).items() if k != "__spec__"}))
                else:
                    out.append(e)
            d.mentions = out
    return d


def config_double(spec):
    """config PARAM carrier: extra stays a RAW dict (isinstance gate)."""
    if isinstance(spec, dict):
        d = types.SimpleNamespace(extra=spec.get("extra"))
        d.__spec__ = spec
        return d
    return spec


def user_double(spec):
    if isinstance(spec, dict):
        return UserDouble(**{k: attr_double(v) for k, v in spec.items()})
    return spec


# ── per-case script backend (mirrors the C da2_call exactly) ──
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
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


def method_rec(ctx, name, args_fn=None, build=None):
    def f(self, *a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


class RecLogger:
    def __init__(self, ctx):
        self._ctx = ctx

    def __getattr__(self, level):
        def log(fmt, *a, **k):
            return self._ctx.call(f"logger.{level}", {"fmt": fmt, "args": list(a)})
        return log


METHOD_ARGMAP = {
    "_extra_or_env_flag": lambda key, env_key, env_default, *, truthy: {
        "key": key, "env_key": env_key, "env_default": env_default, "truthy": truthy},
    "_gate_env": lambda name, default="": {"name": name, "default": default},
    "_gate_raw": lambda extra_key, env_key, config=None: {
        "extra_key": extra_key, "env_key": env_key, "config": proj(config)},
    "_gate_csv_set": lambda raw: {"raw": raw},
    "_bot_tag_debounce_key": lambda message: {"message": proj(message)},
    "_bot_tag_window_seconds": lambda: {},
    "_self_is_explicitly_mentioned": lambda message: {"message": proj(message)},
    "_raw_mentioned_user_ids": lambda message: {"message": proj(message)},
    "_discord_channel_keys_from_channel": lambda channel, parent_channel_id=None: {
        "channel": proj(channel), "parent_channel_id": parent_channel_id},
    "_normalize_permissions": lambda value: {"value": value},
    "_canonicalize_app_command_payload": lambda payload: {"payload": payload},
    "_canonicalize_app_command_option": lambda payload: {"payload": payload},
}


def setup_case(func, inp, state):
    ctx = Ctx(inp.get("io"))
    for nm, fn in REAL.items():
        NS[nm] = fn
    NS["logger"] = RecLogger(ctx)
    NS["time"] = types.SimpleNamespace(monotonic=rec(ctx, "time.monotonic", lambda: {}))
    NS["_scoped_gate_env"] = rec(
        ctx, "_scoped_gate_env", lambda name, default="": {"name": name, "default": default})
    NS["_extra_or_secret"] = rec(
        ctx, "_extra_or_secret",
        lambda extra, key, env, default="", *, blank_is_unset=True: {
            "extra": extra, "key": key, "env": env, "default": default})
    NS["_decode_json_list_literal"] = rec(
        ctx, "_decode_json_list_literal", lambda raw: {"raw": raw})
    NS["is_automated_message"] = rec(
        ctx, "is_automated_message", lambda message: {"message": proj(message)})
    # adapter self double
    slf = types.SimpleNamespace()
    slf.name = state.get("name", "Discord")
    cfg = state.get("config")
    slf.config = types.SimpleNamespace(extra=cfg.get("extra")) if isinstance(cfg, dict) else None
    slf._gate_env_snapshot = state.get("_gate_env_snapshot")
    slf._username_resolved_ids = state.get("_username_resolved_ids")
    slf._text_batch_delay_seconds = state.get("_text_batch_delay_seconds", 0.0)
    slf._text_batch_split_delay_seconds = state.get("_text_batch_split_delay_seconds", 0.0)
    slf._bot_tag_debounce_until = (
        state["_bot_tag_debounce_until"] if "_bot_tag_debounce_until" in state else {})
    cl = state.get("_client")
    if isinstance(cl, dict):
        slf._client = types.SimpleNamespace(user=user_double(cl.get("user")))
    else:
        slf._client = cl
    for nm in METHOD_FUNCS:
        if nm == func:
            fn = REAL[nm]
        elif nm in STATIC_METHODS:
            fn = rec(ctx, nm, METHOD_ARGMAP.get(nm))
        else:
            fn = method_rec(ctx, nm, METHOD_ARGMAP.get(nm))
        if nm in STATIC_METHODS:
            setattr(slf, nm, fn)
        else:
            setattr(slf, nm, types.MethodType(fn, slf))
    return ctx, slf


def drive(func, inp):
    args = inp.get("args") or {}
    state = copy.deepcopy(inp.get("state") or {})
    ctx, slf = setup_case(func, inp, state)
    try:
        if func == "_extra_or_env_flag":
            ret = slf._extra_or_env_flag(args["key"], args["env_key"], args["env_default"],
                                         truthy=args["truthy"])
        elif func in ("_discord_require_mention", "_discord_free_response_auto_thread",
                      "_discord_thread_require_mention", "_discord_history_backfill",
                      "_discord_max_attachment_bytes", "_snapshot_gate_env",
                      "_get_allowed_channels", "_get_ignored_channels",
                      "_get_no_thread_channels", "resolved_allowlist_user_ids",
                      "_discord_allow_all_users", "_gateway_allow_all_users",
                      "_get_allow_bots", "_bot_tag_window_seconds",
                      "_discord_free_response_channels",
                      "_discord_bots_require_inline_mention",
                      "_discord_history_backfill_limit",
                      "_get_discord_command_sync_policy"):
            ret = getattr(slf, func)()
        elif func == "_is_discord_voice_message_attachment":
            att = attr_double(args.get("att"))
            if isinstance(att, types.SimpleNamespace) and \
                    getattr(att, "is_voice_message", None) == "seam":
                att.is_voice_message = rec(ctx, "is_voice_message", lambda: {})
            ret = NS[func](att)
        elif func == "_gate_env":
            ret = slf._gate_env(args["name"], args.get("default", ""))
        elif func == "_gate_raw":
            cfg = args.get("config")
            ret = slf._gate_raw(args["extra_key"], args["env_key"],
                                config=config_double(cfg) if cfg is not None else None)
        elif func == "_gate_csv_set":
            ret = NS[func](args.get("raw"))
        elif func == "_bot_tag_debounce_key":
            ret = NS[func](message_double(args.get("message")))
        elif func in ("_record_bot_tag_debounce", "_is_bot_tag_debounce_continuation",
                      "_raw_mentioned_user_ids", "_self_is_explicitly_mentioned",
                      "_self_is_raw_mentioned"):
            ret = getattr(slf, func)(message_double(args.get("message")))
        elif func == "_discord_channel_keys":
            ret = slf._discord_channel_keys(message_double(args.get("message")),
                                            args.get("parent_channel_id"))
        elif func == "_discord_channel_keys_from_channel":
            ret = slf._discord_channel_keys_from_channel(
                attr_double(args.get("channel")), args.get("parent_channel_id"))
        elif func == "_normalize_permissions":
            ret = NS[func](args.get("value"))
        elif func in ("_canonicalize_app_command_option", "_canonicalize_app_command_payload",
                      "_patchable_app_command_payload"):
            ret = getattr(slf, func)(args.get("payload") or {})
        elif func == "_existing_command_to_payload":
            spec = args.get("command") or {}
            cmd = types.SimpleNamespace()
            cmd.to_dict = rec(ctx, "to_dict", lambda: {})
            for k in ("nsfw", "guild_only", "default_member_permissions"):
                if k in spec:
                    setattr(cmd, k, attr_double(spec[k]))
            ret = slf._existing_command_to_payload(cmd)
        else:
            raise RuntimeError(f"unknown func {func}")
        if isinstance(ret, set):
            ret = sorted(ret)
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    echo = dict(state)
    if "_gate_env_snapshot" in state or slf._gate_env_snapshot is not None:
        echo["_gate_env_snapshot"] = slf._gate_env_snapshot
    if "_bot_tag_debounce_until" in state or (
            isinstance(slf._bot_tag_debounce_until, dict) and slf._bot_tag_debounce_until):
        echo["_bot_tag_debounce_until"] = slf._bot_tag_debounce_until
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(echo)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = [
    "_scoped_gate_env", "_extra_or_secret", "_decode_json_list_literal",
    "is_automated_message", "time.monotonic", "logger.warning", "logger.debug",
    "to_dict", "is_voice_message",
    "_extra_or_env_flag", "_gate_env", "_gate_raw", "_gate_csv_set",
    "_bot_tag_debounce_key", "_bot_tag_window_seconds",
    "_self_is_explicitly_mentioned", "_raw_mentioned_user_ids",
    "_discord_channel_keys_from_channel", "_normalize_permissions",
    "_canonicalize_app_command_payload", "_canonicalize_app_command_option",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "discord_adapter_flow2.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    c_names = set(re.findall(r'da2_call\(\s*ctx,\s*"([^"]+)"', csrc))
    missing_c = registry - c_names
    if missing_c:
        print(f"FATAL oracle_discord_adapter_flow2: seams absent from C port: {sorted(missing_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'(?:rec|method_rec)\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= {"logger.warning", "logger.debug", "is_voice_message", "to_dict"}
    used |= set(METHOD_ARGMAP)
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_discord_adapter_flow2: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    unstubbed = c_names - used
    if unstubbed:
        print(f"FATAL oracle_discord_adapter_flow2: C seams without oracle stub: {sorted(unstubbed)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow2.jsonl"
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
            print(f"FATAL oracle_discord_adapter_flow2: fixture io keys without a seam: {sorted(bad)}")
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
    print(f"ORACLE discord_adapter_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow2.jsonl"
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
    print(f"SELFTEST discord_adapter_flow2: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    missing = [f for f in METHOD_FUNCS if f not in counts]
    if missing:
        print(f"SELFTEST FUNCS WITHOUT CASES: {missing}")
        bad += 1
    HAND = [
        ("_extra_or_env_flag",
         {"args": {"key": "k", "env_key": "E", "env_default": "true", "truthy": False},
          "io": {"_extra_or_secret": "off"}}, False),
        ("_extra_or_env_flag",
         {"args": {"key": "k", "env_key": "E", "env_default": "false", "truthy": True},
          "io": {"_extra_or_secret": None}}, False),
        ("_discord_require_mention", {"io": {"_extra_or_env_flag": True}}, True),
        ("_gate_csv_set", {"args": {"raw": "a, b ,,a"},
                           "io": {"_decode_json_list_literal": "a, b ,,a"}}, ["a", "b"]),
        ("_gate_csv_set", {"args": {"raw": None}}, []),
        ("_gate_env", {"args": {"name": "X", "default": "d"},
                       "state": {"_gate_env_snapshot": {"X": ""}}}, "d"),
        ("_gate_env", {"args": {"name": "X", "default": "d"},
                       "io": {"_scoped_gate_env": "v"}}, "v"),
        ("_discord_max_attachment_bytes",
         {"state": {"config": {"extra": {"max_attachment_bytes": "10"}}}}, 10),
        ("_discord_max_attachment_bytes",
         {"state": {"config": {"extra": {}}},
          "io": {"_scoped_gate_env": "0"}}, 0),
        ("_raw_mentioned_user_ids",
         {"args": {"message": {"content": "hi <@!12> and <@34> <@12>"}}}, ["12", "34"]),
        ("_normalize_permissions", {"args": {"value": 8}}, "8"),
        ("_normalize_permissions", {"args": {"value": None}}, None),
        ("_get_discord_command_sync_policy", {"io": {"_scoped_gate_env": "BULK"}}, "bulk"),
        ("_bot_tag_window_seconds",
         {"state": {"_text_batch_delay_seconds": 1.5, "_text_batch_split_delay_seconds": 2.0}}, 2),
        ("_discord_channel_keys_from_channel",
         {"args": {"channel": {"id": 5, "name": " general "}}}, ["#general", "5", "general"]),
    ]
    for func, inp, want in HAND:
        out = drive(func, inp)
        if out["ret"] != canon(want):
            bad += 1
            print(f"SELFTEST HAND CHECK FAILED {func}: want={want!r} got={out['ret']!r}")
    # one happy-path _record trace shape check (5 ordered seam events)
    out = drive("_record_bot_tag_debounce", {
        "args": {"message": {"channel": {"id": "c"}, "author": {"id": "a"}}},
        "state": {"_text_batch_delay_seconds": 1.0},
        "io": {"is_automated_message": True, "_self_is_explicitly_mentioned": True,
               "time.monotonic": 100.0, "_bot_tag_window_seconds": 2.0,
               "_bot_tag_debounce_key": "c:a"}})
    names = [e["name"] for e in out["trace"]]
    if names != ["is_automated_message", "_self_is_explicitly_mentioned",
                 "time.monotonic", "_bot_tag_window_seconds", "_bot_tag_debounce_key"]:
        bad += 1
        print(f"SELFTEST HAND CHECK FAILED _record trace: {names}")
    if out["state"].get("_bot_tag_debounce_until") != {"c:a": 102}:
        bad += 1
        print(f"SELFTEST HAND CHECK FAILED _record state: {out['state']}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()