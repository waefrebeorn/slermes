#!/usr/bin/env python3
"""Oracle for the Photon adapter FLOW port
(src/gateway/platforms/photon_adapter_flow.c).

Drives the LIVE plugins/platforms/photon/adapter.py decision clusters with
SCRIPTED seams: every SEAMED sibling call (_cache_inbound_attachment,
_guess_mime, _sidecar_call, _record_sent_text_async) and foreign import
(strip_markdown, get_scoped_secret, bounded_put, time.time,
base_is_retryable_error, validate_media_delivery_path) is a recording stub fed
the same scripts the C harness feeds its universal seam ph_call. Compares the
DECISION SEQUENCE (ordered seam events {"name","args","ret"} in "trace") plus
the final return ("ret") and the adapter state echo ("state").

The live defs are AST-extracted from adapter.py (module constants seeded twice
— before and after the defs so _CONTENT_NORMALIZERS binds; the PhotonAdapter
methods stripped of decorators and dedented; PhotonSidecarError exec'd as a real
class). MessageType (gateway/platforms/event.py) and SendResult
(gateway/platforms/base.py) are exec'd as the real enum/dataclass. Every exec
namespace is a registered sys.modules module (py3.14). The heavy quarry import
chain (httpx / gateway.* / pm) is never imported.

All 36 ported functions are REAL (run inline / as real methods on a PhotonAdapter
double class). The async outbound methods are driven with asyncio.run; their
awaited seams (_sidecar_call, _record_sent_text_async) are async recorders.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included); scripted raises round-trip their spec
(PhotonSidecarError raises carry error_class/retryable). Summary:
'ORACLE photon_adapter_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc), ...}.
- MessageType projects to .value; the _Normalized tuple to a 4-element list.
- SendResult projects via dataclasses.asdict (all 8 fields).
- bounded_put is seamed but its store mutation is applied identically on both
  sides AFTER the event (args carry the pre-mutation store snapshot).
- Integral floats == ints (canon folds). dict comparison order-insensitive;
  trace event LIST order is semantic.
- Fixture corpus restrictions are documented in the C port header.
"""
import ast
import asyncio
import copy
import dataclasses
import enum
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
SRC = os.path.join(Q, "plugins", "platforms", "photon", "adapter.py")
EVENT_SRC = os.path.join(Q, "gateway", "platforms", "event.py")
BASE_SRC = os.path.join(Q, "gateway", "platforms", "base.py")
_FLAGS = _fut.annotations.compiler_flag

MODULE_TARGETS = [
    "_normalize_content", "_normalize_group_content", "_normalize_binary_payload",
    "_is_voice_payload", "_cached_media_type", "_attachment_message_type",
    "_format_richlink_content", "_group_item_contents", "_reply_inner",
    "_carries_voice", "_mention_gate_text", "_richlink_url_from_content",
    "_url_only_candidate", "_is_richlink_preview_attachment", "_richlink_preview_label",
    "_is_richlink_preview_content", "_markdown_enabled", "_richlink_candidate",
    "_sidecar_payload_text", "_attachment_label", "_attachment_body",
]
# method name -> decorator kind
KINDS = {
    "_normalize_chat_key": "classmethod",
    "_record_sent_message": "instance",
    "_put_by_chat": "instance",
    "_record_last_inbound": "instance",
    "_record_recent_richlink": "instance",
    "_is_recent_richlink_preview": "instance",
    "_reactions_enabled": "instance",
    "_is_retryable_error": "staticmethod",
    "_is_permanent_sidecar_failure": "staticmethod",
    "_send_retry_is_final": "instance",
    "_sidecar_try": "instance",
    "_post_send": "instance",
    "_sidecar_send_poll": "instance",
    "_sidecar_send_attachment": "instance",
    "_sidecar_send": "instance",
}
ASYNC_METHODS = {"_sidecar_try", "_post_send", "_sidecar_send_poll",
                 "_sidecar_send_attachment", "_sidecar_send"}
TARGETS = MODULE_TARGETS + list(KINDS)
CLASS_CONSTS = ["MAX_MESSAGE_LENGTH", "_SENT_IDS_MAX", "_LAST_INBOUND_CHATS_MAX",
                "_DM_CHAT_GUID_RE", "_OK_EMOJI", "_FAIL_EMOJI"]


def seg(src, node):
    # start at the def line (node.lineno), EXCLUDING decorators, so extracted
    # methods are plain functions that make_double re-wraps per KINDS.
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    start = node.lineno
    if isinstance(node, ast.ClassDef):  # keep @dataclass etc. on classes
        for d in getattr(node, "decorator_list", []):
            start = min(start, d.lineno)
    text = textwrap.dedent("\n".join(src.splitlines()[start - 1:node.end_lineno]))
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


def _seed_constants(src, tree, ns):
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass


def extract():
    mod = new_module("ph_oracle_adapter")
    ns = mod.__dict__
    import logging
    from dataclasses import dataclass, field
    from typing import Any, Optional, Callable, Dict, List, Tuple
    from urllib.parse import urlparse
    ns.update({"re": re, "json": json, "os": os, "logging": logging,
               "dataclass": dataclass, "field": field, "Any": Any,
               "Optional": Optional, "Callable": Callable, "Dict": Dict,
               "List": List, "Tuple": Tuple, "enum": enum, "Enum": enum.Enum,
               "urlparse": urlparse, "logger": _SilentLogger()})
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # real MessageType enum + SendResult dataclass from the quarry
    ev = ast.parse(open(EVENT_SRC, encoding="utf-8").read())
    for node in ev.body:
        if isinstance(node, ast.ClassDef) and node.name == "MessageType":
            exec_node(open(EVENT_SRC, encoding="utf-8").read(), node, ns, EVENT_SRC)
    bsrc = open(BASE_SRC, encoding="utf-8").read()
    for node in ast.parse(bsrc).body:
        if isinstance(node, ast.ClassDef) and node.name == "SendResult":
            exec_node(bsrc, node, ns, BASE_SRC)
    # PhotonSidecarError real class (for isinstance / construction in make_exc)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "PhotonSidecarError":
            exec_node(src, node, ns, SRC)
    _seed_constants(src, tree, ns)
    # module-level functions
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            if node.name in MODULE_TARGETS:
                exec_node(src, node, ns, SRC)
    # PhotonAdapter methods + class constants
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "PhotonAdapter")
    for node in cls.body:
        if isinstance(node, ast.Assign):
            names = [t.id for t in node.targets if isinstance(t, ast.Name)]
            if any(nm in CLASS_CONSTS for nm in names):
                try:
                    exec_node(src, node, ns, SRC)
                except Exception:
                    pass
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in KINDS:
            exec_node(src, node, ns, SRC)
    # re-seed constants so _CONTENT_NORMALIZERS binds the now-defined functions
    _seed_constants(src, tree, ns)
    ns["logger"] = _SilentLogger()
    required = TARGETS + CLASS_CONSTS + [
        "MessageType", "SendResult", "PhotonSidecarError",
        "_PHOTON_RETRYABLE_PATTERNS", "_CONTENT_NORMALIZERS", "_BINARY_CONTENT_TYPES",
        "_SIDECAR_URL_RE", "_RICHLINK_PREVIEW_ATTACHMENT_SUFFIX",
        "_RICHLINK_PREVIEW_SUPPRESS_SECONDS", "_MAX_MESSAGE_LENGTH",
    ]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_photon_adapter_flow: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()


# ── canonical projection ──
def canon(v):
    def conv(x):
        if isinstance(x, enum.Enum):
            return x.value
        if dataclasses.is_dataclass(x) and not isinstance(x, type):
            return conv(dataclasses.asdict(x))
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection ──
BUILTIN = {
    "Exception": Exception, "BaseException": BaseException, "OSError": OSError,
    "TimeoutError": TimeoutError, "ConnectionError": ConnectionError,
    "PermissionError": PermissionError, "FileNotFoundError": FileNotFoundError,
    "AttributeError": AttributeError, "TypeError": TypeError, "ValueError": ValueError,
    "RuntimeError": RuntimeError, "NotImplementedError": NotImplementedError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "KeyError": KeyError, "IndexError": IndexError, "LookupError": LookupError,
    "OverflowError": OverflowError, "StopIteration": StopIteration,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "UnicodeError": UnicodeError,
    "JSONDecodeError": json.JSONDecodeError,
    "CancelledError": type("CancelledError", (Exception,), {}),
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    if kind == "PhotonSidecarError":
        e = NS["PhotonSidecarError"](
            path=spec.get("path", ""), status_code=spec.get("status_code", 0),
            error=spec.get("error", ""), error_class=spec.get("error_class", "sidecar_error"),
            retryable=spec.get("retryable", False))
        e.args = (msg,)  # str(e) == the scripted message (both sides use it)
        return e
    cls = BUILTIN.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    if cls is json.JSONDecodeError:
        return cls(msg, "", 0)
    return cls(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C ph_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.scripted = []

    def _raise(self, name, args, spec, exc):
        self.trace.append({"name": name, "args": canon(args), "ret": spec})
        self.scripted.append((exc, spec))
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            msg = f"no script for {name}"
            self._raise(name, args, {"__raise__": "RuntimeError", "message": msg}, RuntimeError(msg))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                msg = f"script exhausted for {name}"
                self._raise(name, args, {"__raise__": "RuntimeError", "message": msg}, RuntimeError(msg))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                rs = used["raise"]
                spec = {"__raise__": rs.get("type", "RuntimeError"), "message": rs.get("message", "")}
                for k, v in rs.items():
                    if k not in ("type", "message"):
                        spec[k] = v
                self._raise(name, args, spec, make_exc(rs))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return ret


def rec(ctx, name, argfn):
    def f(*a, **k):
        return ctx.call(name, argfn(*a, **k))
    return f


def rec_async(ctx, name, argfn):
    async def f(*a, **k):
        return ctx.call(name, argfn(*a, **k))
    return f


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)
# ── seam argument projections (one per C ph_call name) ──
ARGMAP = {
    "_cache_inbound_attachment": lambda content, name, mime, *, force_audio=False: {
        "content": content, "name": name, "mime": mime, "force_audio": force_audio},
    "_guess_mime": lambda path: {"path": path},
    "strip_markdown": lambda text, *, keep_link_targets=False: {
        "text": text, "keep_link_targets": keep_link_targets},
    "get_scoped_secret": lambda name, default=None, **k: {"name": name, "default": default},
    "bounded_put": lambda store, key, value, cap: {
        "store": canon(store), "key": key, "value": canon(value), "cap": cap},
    "time.time": lambda: {},
    "base_is_retryable_error": lambda error: {"error": error},
    "validate_media_delivery_path": lambda path, session_key="": {
        "path": path, "session_key": session_key},
    "sidecar_call": lambda path, body: {"path": path, "body": body},
    "record_sent_text_async": lambda chat_id, message_id, text: {
        "chat_id": chat_id, "message_id": message_id, "text": text},
}
SEAMED = set(ARGMAP)


def install_recorders(ctx):
    NS["strip_markdown"] = rec(ctx, "strip_markdown", ARGMAP["strip_markdown"])
    NS["_get_scoped_secret"] = rec(ctx, "get_scoped_secret", ARGMAP["get_scoped_secret"])
    NS["_cache_inbound_attachment"] = rec(ctx, "_cache_inbound_attachment",
                                          ARGMAP["_cache_inbound_attachment"])
    NS["_guess_mime"] = rec(ctx, "_guess_mime", ARGMAP["_guess_mime"])
    NS["_record_sent_text_async"] = rec_async(ctx, "record_sent_text_async",
                                              ARGMAP["record_sent_text_async"])
    NS["time"] = types.SimpleNamespace(time=rec(ctx, "time.time", ARGMAP["time.time"]))
    NS["BasePlatformAdapter"] = types.SimpleNamespace(
        _is_retryable_error=rec(ctx, "base_is_retryable_error", ARGMAP["base_is_retryable_error"]))

    def bounded_put_rec(store, key, value, cap):
        raw = ctx.call("bounded_put", ARGMAP["bounded_put"](store, key, value, cap))
        store.pop(key, None)
        store[key] = value
        while len(store) > cap:
            del store[next(iter(store))]
        return raw
    NS["bounded_put"] = bounded_put_rec


def make_double(state, ctx):
    clsdict = {}
    for name, kind in KINDS.items():
        fn = NS[name]
        if kind == "classmethod":
            clsdict[name] = classmethod(fn)
        elif kind == "staticmethod":
            clsdict[name] = staticmethod(fn)
        else:
            clsdict[name] = fn
    for cname in CLASS_CONSTS:
        clsdict[cname] = NS[cname]
    clsdict["_sidecar_call"] = staticmethod(rec_async(ctx, "sidecar_call", ARGMAP["sidecar_call"]))
    clsdict["validate_media_delivery_path"] = staticmethod(rec(
        ctx, "validate_media_delivery_path", ARGMAP["validate_media_delivery_path"]))
    D = type("PhotonAdapter", (), clsdict)
    inst = D()
    for k, v in (state or {}).items():
        setattr(inst, k, copy.deepcopy(v))
    return D, inst


def result_double(v):
    return types.SimpleNamespace(**(v or {}))


# ── per-case live dispatch ──
def live_module(func, args):
    f = NS[func]
    if func in ("_normalize_content", "_normalize_group_content", "_format_richlink_content",
                "_group_item_contents", "_reply_inner", "_carries_voice", "_mention_gate_text",
                "_richlink_url_from_content", "_is_richlink_preview_content",
                "_richlink_preview_label"):
        return f(args["content"])
    if func in ("_normalize_binary_payload", "_is_voice_payload",
                "_is_richlink_preview_attachment"):
        return f(args["payload"])
    if func == "_cached_media_type":
        return f(args["payload"], args.get("mime", ""), args.get("is_voice", False))
    if func == "_attachment_message_type":
        return f(args.get("mime", ""))
    if func == "_url_only_candidate":
        return f(args.get("text", ""))
    if func == "_markdown_enabled":
        return f()
    if func == "_richlink_candidate":
        return f(args.get("text", ""))
    if func == "_sidecar_payload_text":
        return f(args.get("text", ""), args.get("send_markdown", False))
    if func == "_attachment_label":
        return f(args.get("kind", ""), args.get("name", ""))
    if func == "_attachment_body":
        return f(args.get("space_id", ""), args.get("safe_path", ""),
                 kind=args.get("kind", "attachment"), name=args.get("name"),
                 mime_type=args.get("mime_type"), caption=args.get("caption"))
    raise RuntimeError(f"unknown module func {func}")


def live_method(func, args, D, inst, state_out):
    if func == "_normalize_chat_key":
        return D._normalize_chat_key(args.get("chat_id", ""))
    if func == "_record_sent_message":
        return inst._record_sent_message(args.get("message_id"))
    if func == "_put_by_chat":
        store = copy.deepcopy(args.get("store") or {})
        state_out["store"] = store
        return inst._put_by_chat(store, args.get("chat_id", ""), args.get("value"))
    if func == "_record_last_inbound":
        return inst._record_last_inbound(args.get("chat_id"), args.get("message_id"))
    if func == "_record_recent_richlink":
        return inst._record_recent_richlink(args.get("chat_id"), args.get("text", ""))
    if func == "_is_recent_richlink_preview":
        return inst._is_recent_richlink_preview(args.get("chat_id"), args["content"])
    if func == "_reactions_enabled":
        return inst._reactions_enabled()
    if func == "_is_retryable_error":
        return D._is_retryable_error(args.get("error"))
    if func == "_is_permanent_sidecar_failure":
        return D._is_permanent_sidecar_failure(result_double(args.get("result")))
    if func == "_send_retry_is_final":
        return inst._send_retry_is_final(result_double(args.get("result")))
    if func == "_sidecar_try":
        return asyncio.run(inst._sidecar_try(args.get("path", ""), args.get("body"),
                                             args.get("what", "")))
    if func == "_post_send":
        return asyncio.run(inst._post_send(args.get("path", ""), args.get("body"),
                                           structured=args.get("structured", False),
                                           sent_text=args.get("sent_text")))
    if func == "_sidecar_send_poll":
        return asyncio.run(inst._sidecar_send_poll(args.get("space_id", ""),
                                                   args.get("title"), args.get("options")))
    if func == "_sidecar_send_attachment":
        return asyncio.run(inst._sidecar_send_attachment(
            args.get("space_id", ""), args.get("path"), name=args.get("name"),
            mime_type=args.get("mime_type"), caption=args.get("caption"),
            kind=args.get("kind", "attachment")))
    if func == "_sidecar_send":
        return asyncio.run(inst._sidecar_send(args.get("space_id", ""), args.get("text", ""),
                                              richlink=args.get("richlink", True),
                                              markdown=args.get("markdown", True)))
    raise RuntimeError(f"unknown method {func}")


def drive(func, inp):
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    install_recorders(ctx)
    state_out = {}
    if func in MODULE_TARGETS:
        try:
            ret = live_module(func, args)
        except BaseException as exc:
            ret = exc_spec(exc, ctx)
    else:
        D, inst = make_double(state, ctx)
        try:
            ret = live_method(func, args, D, inst, state_out)
        except BaseException as exc:
            ret = exc_spec(exc, ctx)
        # echo the mutated in.state keys (for _put_by_chat this adds to "store")
        state_out.update({k: getattr(inst, k) for k in state})
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check ──
def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "photon_adapter_flow.c",
                encoding="utf-8").read()
    c_seams = set(re.findall(r'ph_call\(\s*ctx,\s*"([^"]+)"', csrc))
    if c_seams != SEAMED:
        print(f"FATAL oracle_photon_adapter_flow: C seams {sorted(c_seams)} != ARGMAP {sorted(SEAMED)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "photon_adapter_flow.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func", ""))
        bad = fix_keys - SEAMED
        if bad:
            print(f"FATAL oracle_photon_adapter_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        unbound = fix_funcs - set(TARGETS)
        if unbound:
            print(f"FATAL oracle_photon_adapter_flow: fixture funcs missing from TARGETS: {sorted(unbound)}")
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
    print(f"ORACLE photon_adapter_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fix = _REPO / "tests" / "fixtures" / "photon_adapter_flow.jsonl"
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
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError" and \
                any(f"'{nm}'" in str(r.get("message", "")) for nm in TARGETS):
            bad += 1
            print(f"SELFTEST BINDING GAP {func} case {n}: {r['message']}")
            continue
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            if not (case.get("in") or {}).get("expect_fail_closed") and not case.get("fc"):
                bad += 1
                print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST photon_adapter_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    missing = set(TARGETS) - set(counts)
    if missing:
        print(f"SELFTEST FUNCS WITHOUT CASES: {sorted(missing)}")
        bad += 1
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()