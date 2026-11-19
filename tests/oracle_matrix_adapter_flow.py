#!/usr/bin/env python3
"""Oracle for the Matrix-adapter FLOW port
(src/gateway/platforms/matrix_adapter_flow.c).

Drives the LIVE plugins/platforms/matrix/adapter.py decision chains with
SCRIPTED seams: every scoped-secret / config reader, the bang-command
dispatch lookups, the platform-registry probe, mimetypes.guess_type, the
urlsplit/urlunsplit pair and time.monotonic are recording stubs fed the same
scripts the C harness feeds its universal seam. Compares the DECISION SEQUENCE
(ordered seam events {"name","args","ret"} in "trace") plus the final return
("ret") and the mutated state ("state").

The live defs are AST-extracted from adapter.py (module constants seeded,
Assign AND AnnAssign, plain-Name targets only; logger re-set to silent AFTER
seeding). MatrixAdapter methods are extracted from the ClassDef (decorators
stripped, dedented) and bound onto SimpleNamespace self doubles. Every exec
namespace is a registered sys.modules module (py3.14). The heavy quarry import
chain is never imported; in-function imports are satisfied with sys.modules
stubs whose attributes are the case's recorders.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE matrix_adapter_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- Plain JSON numbers are ints; JSON booleans are bools. A Python float in an
  isinstance/type-sensitive slot travels as {"__float__": true, "v": x}.
  Integral floats normalize to int on both sides (canon).
- SendResult returns (_media_too_large) project to
  {"success","message_id","error"}.
- Attribute doubles (exc/event/device_keys/config/prompt/self) are JSON objects
  wrapped in SimpleNamespace; dict-vs-attr access reads the same key.
- set returns (_csv_set/_extra_csv_set) and the dedup set normalize to sorted
  lists on both sides.
- str.strip()/lower() fold ASCII whitespace/case only on the fixture corpus;
  int()/float() parsers cover the verified no-underscore domain.
- logger calls are silent on both sides (not traced).
"""
import ast
import copy
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import Path
from collections import deque
from contextlib import suppress
from urllib.parse import urlsplit as _real_urlsplit, urlunsplit as _real_urlunsplit
import mimetypes as _real_mimetypes
import time as _real_time
import logging
import typing

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "plugins", "platforms", "matrix", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

MODULE_FUNCS = [
    "_utf8_len", "_content_bytes", "_csv_set", "_thread_root", "_normalize_e2ee_mode",
    "_extract_reply_fallback", "_strip_reply_fallback", "_split_reply_fallback",
    "_is_permanent_matrix_auth_error", "_matrix_event_timestamp_seconds",
    "_redact_url_for_log", "_looks_like_transport_filename",
    "_looks_like_matrix_image_filename", "_looks_like_matrix_media_filename",
    "_is_bare_media_filename", "_env_truthy", "_env_number", "_scoped_recovery_key",
    "_extra_csv_set", "_resolve_e2ee_mode", "_resolve_matrix_bang_command",
    "_normalize_matrix_bang_command", "_resolve_max_message_length",
]
METHOD_FUNCS = [
    "_is_self_sender", "_is_system_or_bridge_sender", "_user_localpart", "_mxc_to_http",
    "_sanitize_link_url", "_state_event_value", "_extract_server_ed25519",
    "_configured_bool", "_media_too_large", "_matrix_prompt_expired",
    "_apply_relation_metadata", "_is_duplicate_event", "_invalidate_room_identities",
]
STATIC_METHODS = {
    "_is_system_or_bridge_sender", "_sanitize_link_url", "_state_event_value",
    "_extract_server_ed25519", "_configured_bool",
}


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


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


class SendResult:
    def __init__(self, success, message_id=None, error=None, **k):
        self.success = success
        self.message_id = message_id
        self.error = error


def extract():
    mod = new_module("ma_oracle_adapter")
    ns = mod.__dict__
    ns.update({
        "re": re, "os": os, "json": json, "Path": Path, "suppress": suppress,
        "mimetypes": _real_mimetypes, "time": _real_time, "logging": logging,
        "urlsplit": _real_urlsplit, "urlunsplit": _real_urlunsplit,
        "Any": typing.Any, "Optional": typing.Optional, "Dict": typing.Dict,
        "Set": typing.Set, "deque": deque,
    })
    src = open(SRC, encoding="utf-8").read()
    seed_constants(src, ns, SRC)
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in MODULE_FUNCS:
            exec_node(src, node, ns, SRC)
    cls = next(n for n in ast.parse(src).body
               if isinstance(n, ast.ClassDef) and n.name == "MatrixAdapter")
    for n in cls.body:
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name in METHOD_FUNCS:
            n2 = copy.copy(n)
            n2.decorator_list = []
            exec_node(src, n2, ns, SRC)
    ns["logger"] = _SilentLogger()
    ns["SendResult"] = SendResult
    required = MODULE_FUNCS + METHOD_FUNCS + [
        "_MATRIX_BANG_COMMAND_RE", "_MATRIX_REPLY_FALLBACK_PILL_RE",
        "_MATRIX_PERMANENT_ERRCODES", "_MATRIX_IMAGE_FILENAME_EXTS",
        "_MATRIX_MEDIA_FILENAME_EXTS", "_MAX_CONTENT_BYTES",
        "MATRIX_MAX_MESSAGE_LENGTH_CEILING", "DEFAULT_MAX_MESSAGE_LENGTH",
    ]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_matrix_adapter_flow: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()

# ── sys.modules stubs for the in-function imports (attributes re-pointed per case) ──
for _pkg in ("hermes_cli", "agent", "gateway"):
    if _pkg not in sys.modules:
        new_module(_pkg)
for _leaf in ("hermes_cli.commands", "agent.skill_commands", "gateway.platform_registry"):
    new_module(_leaf)
sys.modules["hermes_cli"].commands = sys.modules["hermes_cli.commands"]
sys.modules["agent"].skill_commands = sys.modules["agent.skill_commands"]
sys.modules["gateway"].platform_registry = sys.modules["gateway.platform_registry"]


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if isinstance(x, SendResult):
            return {"success": x.success, "message_id": x.message_id, "error": x.error}
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, deque):
            return [conv(i) for i in x]
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection ──
import asyncio
EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "OSError": OSError, "Exception": Exception, "StopIteration": StopIteration,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": asyncio.CancelledError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    return cls(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C ma_call exactly) ──
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


def pyval(v):
    if isinstance(v, dict) and v.get("__float__") is True:
        return float(v.get("v", 0))
    return v


def attr_double(spec):
    if spec is None:
        return None
    if isinstance(spec, dict):
        return types.SimpleNamespace(**{k: pyval(v) for k, v in spec.items()})
    return spec


def _ns_wrap(raw):
    return types.SimpleNamespace(**raw) if isinstance(raw, dict) else raw


def _tuple_wrap(raw):
    return tuple(raw) if isinstance(raw, list) else raw


def install_seams(ctx):
    NS["_get_scoped_secret"] = rec(
        ctx, "get_scoped_secret", lambda name, default=None, **k: {"name": name, "default": default})
    NS["_extra_or_secret"] = rec(
        ctx, "extra_or_secret",
        lambda extra, key, env, default="", *, blank_is_unset=True: {
            "extra": extra, "key": key, "env": env, "default": default,
            "blank_is_unset": blank_is_unset})
    NS["mimetypes"] = types.SimpleNamespace(
        guess_type=rec(ctx, "mimetypes.guess_type", lambda fn: {"filename": fn}, _tuple_wrap))
    NS["urlsplit"] = rec(ctx, "urlsplit", lambda url: {"url": url}, _ns_wrap)
    NS["urlunsplit"] = rec(ctx, "urlunsplit", lambda parts: {"parts": list(parts)})
    NS["time"] = types.SimpleNamespace(
        monotonic=rec(ctx, "time.monotonic", lambda: {}), time=_real_time.time)
    sys.modules["hermes_cli.commands"].is_gateway_known_command = rec(
        ctx, "is_gateway_known_command", lambda candidate: {"candidate": candidate})
    sys.modules["agent.skill_commands"].get_skill_commands = rec(
        ctx, "get_skill_commands", lambda: {})
    sys.modules["gateway.platform_registry"].platform_registry = types.SimpleNamespace(
        get=rec(ctx, "platform_registry.get", lambda name: {"name": name}, _ns_wrap))


def mk_self(attrs, methods):
    slf = types.SimpleNamespace(**{k: pyval(v) for k, v in (attrs or {}).items()})
    for m in methods:
        if m in NS and m not in STATIC_METHODS:
            setattr(slf, m, types.MethodType(NS[m], slf))
    return slf


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    state_in = inp.get("state") or {}
    state = {}
    ctx = Ctx(inp.get("io"))
    install_seams(ctx)
    try:
        if func == "_utf8_len":
            ret = NS["_utf8_len"](args["text"])
        elif func == "_content_bytes":
            ret = NS["_content_bytes"](args["content"])
        elif func == "_csv_set":
            ret = NS["_csv_set"](args["raw"])
        elif func == "_thread_root":
            ret = NS["_thread_root"](args["relates_to"])
        elif func == "_normalize_e2ee_mode":
            ret = NS["_normalize_e2ee_mode"](pyval(args.get("value")))
        elif func == "_extract_reply_fallback":
            ret = NS["_extract_reply_fallback"](args["body"])
        elif func == "_strip_reply_fallback":
            ret = NS["_strip_reply_fallback"](args["body"])
        elif func == "_split_reply_fallback":
            ret = NS["_split_reply_fallback"](args["body"])
        elif func == "_is_permanent_matrix_auth_error":
            ret = NS["_is_permanent_matrix_auth_error"](attr_double(args.get("exc")))
        elif func == "_matrix_event_timestamp_seconds":
            ret = NS["_matrix_event_timestamp_seconds"](attr_double(args.get("event")))
        elif func == "_redact_url_for_log":
            ret = NS["_redact_url_for_log"](args["url"])
        elif func == "_looks_like_transport_filename":
            prefixes = args["mime_prefixes"]
            prefixes = tuple(prefixes) if isinstance(prefixes, list) else prefixes
            ret = NS["_looks_like_transport_filename"](
                args["text"], prefixes, frozenset(args["exts"]), args.get("reject_spaces", False))
        elif func == "_looks_like_matrix_image_filename":
            ret = NS["_looks_like_matrix_image_filename"](args["text"])
        elif func == "_looks_like_matrix_media_filename":
            ret = NS["_looks_like_matrix_media_filename"](args["text"])
        elif func == "_is_bare_media_filename":
            ret = NS["_is_bare_media_filename"](args["msgtype"], args["body"])
        elif func == "_env_truthy":
            ret = NS["_env_truthy"](args["name"], args.get("default", ""))
        elif func == "_env_number":
            cast = int if args.get("cast") == "int" else float
            ret = NS["_env_number"](args["name"], pyval(args.get("default")), cast)
        elif func == "_scoped_recovery_key":
            ret = NS["_scoped_recovery_key"]()
        elif func == "_extra_csv_set":
            cfg = types.SimpleNamespace(extra=args.get("extra"))
            ret = NS["_extra_csv_set"](cfg, args["key"], args["env_name"])
        elif func == "_resolve_e2ee_mode":
            ret = NS["_resolve_e2ee_mode"](args.get("extra"))
        elif func == "_resolve_matrix_bang_command":
            ret = NS["_resolve_matrix_bang_command"](args["name"])
        elif func == "_normalize_matrix_bang_command":
            ret = NS["_normalize_matrix_bang_command"](args["text"])
        elif func == "_resolve_max_message_length":
            cfg = types.SimpleNamespace(extra=args.get("extra"))
            ret = NS["_resolve_max_message_length"](cfg)
        elif func == "_is_self_sender":
            slf = mk_self(args.get("self"), ["_is_self_sender"])
            ret = slf._is_self_sender(args["sender"])
        elif func == "_is_system_or_bridge_sender":
            ret = NS["_is_system_or_bridge_sender"](args["sender"])
        elif func == "_user_localpart":
            slf = mk_self(args.get("self"), ["_user_localpart"])
            ret = slf._user_localpart()
        elif func == "_mxc_to_http":
            slf = mk_self(args.get("self"), ["_mxc_to_http"])
            ret = slf._mxc_to_http(args["mxc_url"])
        elif func == "_sanitize_link_url":
            ret = NS["_sanitize_link_url"](args["url"])
        elif func == "_state_event_value":
            ev = args.get("event")
            if args.get("event_attr") and isinstance(ev, dict):
                ev = types.SimpleNamespace(**ev)
            ret = NS["_state_event_value"](ev, args["key"])
        elif func == "_extract_server_ed25519":
            ret = NS["_extract_server_ed25519"](attr_double(args.get("device_keys")))
        elif func == "_configured_bool":
            cfg = types.SimpleNamespace(extra=args.get("extra"))
            ret = NS["_configured_bool"](cfg, args["key"])
        elif func == "_media_too_large":
            slf = mk_self(args.get("self"), ["_media_too_large"])
            ret = slf._media_too_large(args["size"])
        elif func == "_matrix_prompt_expired":
            slf = mk_self(args.get("self"), ["_matrix_prompt_expired"])
            ret = slf._matrix_prompt_expired(attr_double(args.get("prompt")))
        elif func == "_apply_relation_metadata":
            slf = mk_self(args.get("self"), ["_apply_relation_metadata"])
            mc = dict(args.get("msg_content") or {})
            slf._apply_relation_metadata(mc, reply_to=args.get("reply_to"),
                                         metadata=args.get("metadata"))
            state["msg_content"] = mc
            ret = None
        elif func == "_is_duplicate_event":
            maxlen = args.get("maxlen", 1000)
            init = list(state_in.get("processed_events") or [])
            slf = mk_self({}, ["_is_duplicate_event"])
            slf._processed_events = deque(init, maxlen=maxlen)
            slf._processed_events_set = set(init)
            ret = slf._is_duplicate_event(args.get("event_id"))
            state["processed_events"] = list(slf._processed_events)
            state["processed_events_set"] = sorted(slf._processed_events_set)
        elif func == "_invalidate_room_identities":
            slf = mk_self({}, ["_invalidate_room_identities"])
            slf._room_identities = dict(state_in.get("room_identities") or {})
            slf._room_identity_cached_at = dict(state_in.get("room_identity_cached_at") or {})
            slf._invalidate_room_identities(args.get("room_id"))
            state["room_identities"] = slf._room_identities
            state["room_identity_cached_at"] = slf._room_identity_cached_at
            ret = None
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = [
    "get_scoped_secret", "extra_or_secret", "is_gateway_known_command",
    "get_skill_commands", "platform_registry.get", "mimetypes.guess_type",
    "urlsplit", "urlunsplit", "time.monotonic",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "matrix_adapter_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_matrix_adapter_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_matrix_adapter_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "matrix_adapter_flow.jsonl"
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
            print(f"FATAL oracle_matrix_adapter_flow: fixture io keys without a seam: {sorted(bad)}")
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
    print(f"ORACLE matrix_adapter_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "matrix_adapter_flow.jsonl"
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
    print(f"SELFTEST matrix_adapter_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()