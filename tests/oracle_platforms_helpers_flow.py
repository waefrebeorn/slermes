#!/usr/bin/env python3
"""Oracle for the platform-helper FLOW port
(src/gateway/platforms/platforms_helpers_flow.c).

Drives the LIVE gateway/platforms/helpers.py decision chains with SCRIPTED
seams: every escape/len_fn/fits callable, the reply_message_count hook and the
os.stat/os.fstat media probes are recording stubs fed the same scripts the C
harness feeds its universal seam. Compares the DECISION SEQUENCE (ordered seam
events {"name","args","ret"} in "trace") plus the final return ("ret") and the
mutated state ("state", bounded_put's store).

The live defs are AST-extracted from helpers.py (module constants seeded,
Assign AND AnnAssign, plain-Name targets only; logger re-set to silent AFTER
seeding). BasePlatformAdapter.send_document is AST-extracted from
gateway/platforms/base.py for the reply_document_sender identity gate; every
exec namespace is a registered sys.modules module (py3.14). The heavy quarry
import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE platforms_helpers_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}
  (OSError adds errno when set, message "[Errno N] msg").
- A Python float in an isinstance/type-sensitive slot travels as
  {"__float__": true, "v": <number>}; plain JSON numbers are ints, JSON
  booleans are bools (bool IS an int instance, but type() is not int).
- bytes return values (reply_file_parts) project to latin-1 strings.
- Bound-method returns (reply_document_sender) project to the function name.
- len_fn/escape/fits callable params: null -> builtin len / identity / absent;
  the literal "seam" -> the recording stub named "len_fn"/"escape"/"fits".
- Adapter doubles: {"hooks": {send_reply_document: "own"|<non-callable>,
  send_document: "own"|"base"|"null", reply_message_count: "own"}, "attrs":
  {MAX_SPLIT_MESSAGES, MAX_MESSAGE_LENGTH, ...}}; the class always inherits
  the extracted BasePlatformAdapter.send_document unless overridden.
- Media sources: a JSON string is a path; {"kind":"pathlike","path":...} is
  an os.PathLike double; {"kind":"file","fileno":N} / {"kind":"file",
  "fileno_raise":spec} is an open-file double; anything else has no fileno.
  os.stat/os.fstat scripts return {"st_size": N}; a null return models the
  real AttributeError on .st_size (propagates for paths, caught for files).
- logger calls are silent on both sides (not traced).
- Fixture corpus: strip/lstrip/rstrip fold ASCII whitespace only; str() of
  non-string choices covers ints/bools/None only; reply_file_parts limits are
  >= 4 for non-ASCII text (live loops forever when a forced cut backs to 0).
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
from typing import Any, Iterator, MutableMapping, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
H_SRC = os.path.join(Q, "gateway", "platforms", "helpers.py")
BASE_SRC = os.path.join(Q, "gateway", "platforms", "base.py")

_FLAGS = _fut.annotations.compiler_flag


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


# ── helpers.py namespace (live defs + seeded constants) ──
def extract_helpers():
    mod = new_module("phf_oracle_helpers")
    ns = mod.__dict__
    import asyncio
    import contextlib
    import contextvars
    import hashlib
    import hmac
    import itertools
    import logging
    import secrets
    import threading
    import time
    import unicodedata
    ns.update({"asyncio": asyncio, "contextlib": contextlib, "contextvars": contextvars,
               "hashlib": hashlib, "hmac": hmac, "itertools": itertools,
               "logging": logging, "os": os, "re": re, "secrets": secrets,
               "threading": threading, "time": time, "unicodedata": unicodedata,
               "json": json, "Path": Path, "Any": Any, "Iterator": Iterator,
               "MutableMapping": MutableMapping, "Optional": Optional})
    src = open(H_SRC, encoding="utf-8").read()
    seed_constants(src, ns, H_SRC)
    for node in ast.parse(src).body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            exec_node(src, node, ns, H_SRC)
    # the logger Assign ran during seeding; re-set the silent recorder AFTER
    ns["logger"] = _SilentLogger()
    required = ["display_width", "_clip_text", "_shorten_choice", "_distinct_shortenings",
                "numbered_clarify_choices", "clarify_text_with_choices", "_keep_link_target",
                "bounded_put", "table_row_start", "table_rollback_point",
                "table_header_to_repeat", "is_discord_channel_obfuscated",
                "reply_document_sender", "reply_message_cap", "_over_reply_cap",
                "reply_file_parts", "scaled_upload_deadline", "media_upload_seconds",
                "_on_disk_size", "_table_header_above", "_split_inside_table",
                "_is_pipe_row", "is_table_row", "TABLE_SEPARATOR_RE", "_MD_LINK_RE",
                "_HTTP_TARGET_RE", "MIN_MEDIA_UPLOAD_RATE", "MEDIA_UPLOAD_DEADLINE_MAX",
                "DISCORD_CHANNEL_OBFUSCATED_FLAG", "DISCORD_OBFUSCATED_CHANNEL_NAME"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_platforms_helpers_flow: helpers missing: {missing}")
        sys.exit(2)
    return ns


NS = extract_helpers()


# ── BasePlatformAdapter.send_document (identity gate only, never called) ──
def extract_base():
    for name in ("gateway", "gateway.platforms", "gateway.platforms.base"):
        if name not in sys.modules:
            new_module(name)
    bmod = sys.modules["gateway.platforms.base"]
    ns = new_module("phf_oracle_base").__dict__
    src = open(BASE_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "BasePlatformAdapter")
    fn = next(n for n in cls.body
              if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))
              and n.name == "send_document")
    fn = copy.copy(fn)
    fn.decorator_list = []
    exec_node(src, fn, ns, BASE_SRC)
    BaseStub = type("BasePlatformAdapter", (), {"send_document": ns["send_document"]})
    bmod.BasePlatformAdapter = BaseStub
    return BaseStub


BaseStub = extract_base()


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if isinstance(x, bytes):
            return x.decode("latin-1")
        if isinstance(x, types.MethodType):
            return x.__func__.__name__
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
EXC_CLASSES = {
    "OSError": OSError, "TimeoutError": TimeoutError, "PermissionError": PermissionError,
    "FileNotFoundError": FileNotFoundError, "FileExistsError": FileExistsError,
    "BlockingIOError": BlockingIOError, "ConnectionError": ConnectionError,
    "ConnectionResetError": ConnectionResetError, "BrokenPipeError": BrokenPipeError,
    "InterruptedError": InterruptedError, "ValueError": ValueError, "TypeError": TypeError,
    "RuntimeError": RuntimeError, "KeyError": KeyError, "AttributeError": AttributeError,
    "ImportError": ImportError, "Exception": Exception, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    en = spec.get("errno")
    if en is not None and issubclass(cls, OSError):
        return cls(en, msg)
    return cls(msg)


def proj_exc(exc):
    d = {"__raise__": type(exc).__name__, "message": str(exc)}
    if isinstance(exc, OSError) and exc.errno is not None:
        d["errno"] = exc.errno
    return d


# ── per-case script backend (mirrors the C phf_call exactly) ──
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


# ── attribute doubles ──
def pyval(v):
    """{"__float__": true, "v": x} -> float(x); anything else verbatim."""
    if isinstance(v, dict) and v.get("__float__") is True:
        return float(v.get("v", 0))
    return v


class PathDouble(os.PathLike):
    def __init__(self, path):
        self._path = path

    def __fspath__(self):
        return self._path


class FileDouble:
    def __init__(self, spec):
        self._spec = spec

    def fileno(self):
        if "fileno_raise" in self._spec:
            raise make_exc(self._spec["fileno_raise"])
        return self._spec["fileno"]


def make_source(spec):
    if isinstance(spec, str):
        return spec
    if isinstance(spec, dict):
        kind = spec.get("kind")
        if kind == "pathlike":
            return PathDouble(spec["path"])
        if kind == "file":
            return FileDouble(spec)
        return object()  # no .fileno -> AttributeError -> 0
    return spec  # ints/bools/None/lists: no .fileno -> AttributeError -> 0


def make_adapter(spec, ctx):
    spec = spec or {}
    hooks = spec.get("hooks") or {}
    attrs = spec.get("attrs") or {}
    clsdict = {}
    srd = hooks.get("send_reply_document")
    if srd == "own":
        async def send_reply_document(self, *a, **k):
            raise AssertionError("seam must not be called")
        clsdict["send_reply_document"] = send_reply_document
    elif srd is not None:
        clsdict["send_reply_document"] = pyval(srd)  # non-callable class attribute
    sd = hooks.get("send_document", "base")
    if sd == "own":
        async def send_document(self, *a, **k):
            raise AssertionError("seam must not be called")
        clsdict["send_document"] = send_document
    elif sd == "null":
        clsdict["send_document"] = None
    # "base" (default): inherit the extracted BasePlatformAdapter.send_document
    if hooks.get("reply_message_count") == "own":
        def reply_message_count(self, content, metadata):
            return ctx.call("reply_message_count", {"content": content, "metadata": metadata})
        clsdict["reply_message_count"] = reply_message_count
    cls = type("AdapterCase", (BaseStub,), clsdict)
    inst = cls()
    for k, v in attrs.items():
        setattr(inst, k, pyval(v))
    return inst


def chan_double(spec):
    spec = spec or {}
    d = {}
    for k, v in spec.items():
        if k == "flags" and isinstance(v, dict):
            d[k] = types.SimpleNamespace(**{fk: pyval(fv) for fk, fv in v.items()})
        else:
            d[k] = pyval(v) if k == "name" else v
    return types.SimpleNamespace(**d)


def seam_fn(ctx, args, key, name):
    """null -> None (caller substitutes builtin/identity); "seam" -> recorder."""
    if args.get(key) == "seam":
        return rec(ctx, name, lambda t: {"text": t})
    return None


def stat_build(raw):
    return types.SimpleNamespace(**raw) if isinstance(raw, dict) else raw


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    state = {}
    ctx = Ctx(inp.get("io"))
    try:
        if func == "display_width":
            ret = NS["display_width"](args["text"])
        elif func == "_clip_text":
            lf = seam_fn(ctx, args, "len_fn", "len_fn") or len
            ret = NS["_clip_text"](args["text"], args["limit"], lf)
        elif func == "_shorten_choice":
            ret = NS["_shorten_choice"](args["text"], args["each"], args["anchor"])
        elif func == "_distinct_shortenings":
            ret = NS["_distinct_shortenings"](args["texts"], args["each"], args["prefix"])
        elif func == "numbered_clarify_choices":
            esc = seam_fn(ctx, args, "escape", "escape")
            lf = seam_fn(ctx, args, "len_fn", "len_fn") or len
            ret = NS["numbered_clarify_choices"](
                args["choices"], max_label_cols=args["max_label_cols"], escape=esc,
                max_chars=args.get("max_chars"), len_fn=lf)
        elif func == "clarify_text_with_choices":
            esc = seam_fn(ctx, args, "escape", "escape")
            lf = seam_fn(ctx, args, "len_fn", "len_fn") or len
            ret = NS["clarify_text_with_choices"](
                args["head"], args["choices"], limit=args["limit"],
                max_label_cols=args["max_label_cols"], escape=esc, len_fn=lf)
        elif func == "_keep_link_target":
            g1, g2 = args["group1"], args["group2"]
            m = NS["_MD_LINK_RE"].search(f"[{g1}]({g2})")
            if m is None or m.group(1) != g1 or m.group(2) != g2:
                raise ValueError("fixture groups do not round-trip through _MD_LINK_RE")
            ret = NS["_keep_link_target"](m)
        elif func == "bounded_put":
            store = dict(args.get("store") or {})
            state["store"] = store
            NS["bounded_put"](store, args["key"], args.get("value"), args["cap"])
            ret = None
        elif func == "table_row_start":
            ret = NS["table_row_start"](args["text"], args["split_at"])
        elif func == "table_rollback_point":
            fits = rec(ctx, "fits", lambda t: {"text": t})
            ret = NS["table_rollback_point"](args["text"], args["split_at"], fits)
        elif func == "table_header_to_repeat":
            lf = seam_fn(ctx, args, "len_fn", "len_fn") or len
            ret = NS["table_header_to_repeat"](args["chunk"], args["rest"],
                                               args["max_units"], lf)
        elif func == "is_discord_channel_obfuscated":
            ret = NS["is_discord_channel_obfuscated"](chan_double(args.get("channel")))
        elif func == "reply_document_sender":
            ret = NS["reply_document_sender"](make_adapter(args.get("adapter"), ctx))
        elif func == "reply_message_cap":
            ret = NS["reply_message_cap"](make_adapter(args.get("adapter"), ctx))
        elif func == "_over_reply_cap":
            ret = NS["_over_reply_cap"](make_adapter(args.get("adapter"), ctx),
                                        args["content"], args.get("metadata"))
        elif func == "reply_file_parts":
            ret = NS["reply_file_parts"](args["text"], args["limit_bytes"])
        elif func == "scaled_upload_deadline":
            ret = NS["scaled_upload_deadline"](args["deadline"], args["upload_seconds"])
        elif func == "media_upload_seconds":
            sources = [make_source(s) for s in args["sources"]]
            stat_stub = rec(ctx, "os.stat", lambda v: {"path": os.fspath(v)}, stat_build)
            fstat_stub = rec(ctx, "os.fstat", lambda fd: {"fd": fd}, stat_build)
            old_stat, old_fstat = os.stat, os.fstat
            os.stat, os.fstat = stat_stub, fstat_stub
            try:
                ret = NS["media_upload_seconds"](sources)
            finally:
                os.stat, os.fstat = old_stat, old_fstat
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = ["escape", "len_fn", "fits", "reply_message_count", "os.stat", "os.fstat"]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "platforms_helpers_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_platforms_helpers_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'seam_fn\(ctx, args, "[^"]+", "([^"]+)"\)', osrc))
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_platforms_helpers_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "platforms_helpers_flow.jsonl"
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
            print(f"FATAL oracle_platforms_helpers_flow: fixture io keys without a seam: {sorted(bad)}")
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
    print(f"ORACLE platforms_helpers_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "platforms_helpers_flow.jsonl"
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
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            bad += 1
            print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
    print(f"SELFTEST platforms_helpers_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
