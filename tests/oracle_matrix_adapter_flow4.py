#!/usr/bin/env python3
"""Oracle for the Matrix-adapter config & classification helper flow port
(src/gateway/platforms/matrix_adapter_flow4.c).

Drives the LIVE plugins/platforms/matrix/adapter.py UNCLAIMED module-level
helpers for message sizing, media-filename classification, E2EE-mode / env /
secret resolution, room-list CSV parsing, recovery-key target resolution, URL
redaction and dependency presence checks, with SCRIPTED seams: every module
collaborator and in-function import is a recording stub fed the same scripts
the C harness feeds its universal seam. Compares the DECISION SEQUENCE (ordered
seam events {"name","args","ret"} in "trace") plus the final return ("ret").

The live defs are AST-extracted from adapter.py (module level, decorators
stripped, dedented) and called with plain Python args. All are synchronous. The
heavy quarry import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit/CancelledError included). Scripted raises round-trip
their spec verbatim; unscripted live raises project {__raise__, message:str(exc)}.
Summary: 'ORACLE matrix_adapter_flow4: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- Plain JSON numbers are ints; integral floats normalize to int (canon).
- Attribute carriers (config, registry entry, event) are {"__attr__": {...}};
  the oracle builds SimpleNamespace doubles and projects them back on seam args.
- Sets return as sorted arrays (Python set -> canon sorted list).
- logger calls are silent. No state is mutated by these helpers.
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
from contextlib import suppress
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

METHOD_FUNCS = [
    "_utf8_len", "_resolve_max_message_length", "_looks_like_transport_filename",
    "_looks_like_matrix_image_filename", "_looks_like_matrix_media_filename",
    "_is_bare_media_filename", "_matrix_event_timestamp_seconds",
    "_normalize_e2ee_mode", "_resolve_e2ee_mode", "_env_truthy", "_env_number",
    "_csv_set", "_thread_root", "_extra_csv_set", "_scoped_recovery_key",
    "_redact_url_for_log", "matrix_deps_present", "check_matrix_requirements",
]

# name -> (args_fn, build)
SIBLING_SPECS = {
    "_get_scoped_secret": (
        lambda name, default="": {"name": name, "default": default}, None),
    "_extra_or_secret": (
        lambda extra, key, env_name, default, blank_is_unset=True: proj({
            "extra": extra, "key": key, "env_name": env_name,
            "default": default, "blank_is_unset": blank_is_unset}), None),
    "platform_registry.get": (
        lambda key: {"key": key}, lambda raw: attr_double(raw)),
    "mimetypes.guess_type": (
        lambda candidate: {"candidate": candidate}, None),
    "urlsplit": (
        lambda url: {"url": url}, lambda raw: attr_double(raw)),
    "urlunsplit": (
        lambda parts: {"parts": proj(parts)}, None),
    "is_available": (
        lambda name: {"name": name}, None),
    "ensure_matrix_deps": (
        lambda: {}, None),
}


def proj(v):
    """Project live Python doubles back to fixture JSON (attr carriers kept)."""
    if isinstance(v, types.SimpleNamespace):
        return {"__attr__": {k: proj(vv) for k, vv in vars(v).items()}}
    if isinstance(v, dict):
        return {k: proj(vv) for k, vv in v.items()}
    if isinstance(v, (list, tuple)):
        return [proj(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted(proj(x) for x in v)
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def proj_val(v):
    if isinstance(v, dict) and v.get("__float__") is True:
        return float(v.get("v", 0))
    return v


def attr_double(raw):
    if not isinstance(raw, dict):
        return raw
    inner = raw.get("__attr__", raw)
    ns = types.SimpleNamespace()
    for k, v in inner.items():
        setattr(ns, k, proj_val(v))
    return ns


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


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


def extract():
    mod = new_module("ma4_oracle_adapter")
    ns = mod.__dict__
    ns.update({
        "re": re, "os": os, "json": json, "Path": Path, "suppress": suppress,
        "typing": typing, "Any": typing.Any,
        "Optional": typing.Optional, "Dict": typing.Dict, "Set": typing.Set,
    })
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in METHOD_FUNCS:
            n2 = copy.copy(node)
            n2.decorator_list = []
            exec_node(src, n2, ns, SRC)
    ns["logger"] = _SilentLogger()
    missing = [k for k in METHOD_FUNCS if k not in ns]
    if missing:
        print(f"FATAL oracle_matrix_adapter_flow4: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()


def canon(v):
    def conv(x):
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, BaseException):
            return str(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "OSError": OSError, "Exception": Exception,
    "StopIteration": StopIteration, "IndexError": IndexError, "LookupError": LookupError,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": __import__("asyncio").CancelledError,
    "TimeoutError": TimeoutError, "AssertionError": AssertionError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.scripted = []

    def _fail(self, name, args, msg):
        exc = RuntimeError(msg)
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        if entry is _MISSING:
            self._fail(name, args, f"no script for {name}")
        used = entry
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._fail(name, args, f"script exhausted for {name}")
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                sp = used["raise"]
                exc = make_exc(sp)
                spec = {"__raise__": sp.get("type", "RuntimeError"),
                        "message": sp.get("message", "")}
                self.scripted.append((exc, spec))
                self.trace.append({"name": name, "args": args, "ret": spec})
                raise exc
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret

    def spec_for(self, exc):
        for e, spec in self.scripted:
            if e is exc:
                return spec
        return None


def rec(ctx, name, args_fn=None, build=None, is_async=False):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


def install_global_seams(ctx):
    for nm, (afn, build) in SIBLING_SPECS.items():
        NS[nm] = rec(ctx, nm, afn, build)
    # in-function import interception: stub parents halt the editable finder.
    pm = new_module("pm")
    pm.available = NS["is_available"]
    if "gateway" not in sys.modules:
        parent = new_module("gateway")
        parent.__path__ = []
    pr = new_module("gateway.platform_registry")
    pr.platform_registry = types.SimpleNamespace(get=NS["platform_registry.get"])
    NS["mimetypes"] = types.SimpleNamespace(guess_type=NS["mimetypes.guess_type"])


def cfg_obj(spec):
    """config double: attrs from __attr__; extra stays a plain dict (fixture)."""
    if spec is None:
        return None
    if not isinstance(spec, dict):
        return spec
    inner = spec.get("__attr__", spec)
    ns = types.SimpleNamespace()
    for k, v in inner.items():
        setattr(ns, k, proj_val(v))
    return ns


def attr_obj(spec):
    return attr_double(spec)


def _invoke(func, args, ctx):
    g = args.get
    fn = NS[func]
    if func == "_utf8_len":
        return fn(g("text"))
    if func == "_resolve_max_message_length":
        return fn(cfg_obj(g("config")))
    if func == "_looks_like_transport_filename":
        # real callers pass a tuple of prefixes (str.startswith(tuple)); a JSON
        # list is the fixture representation of that tuple.
        return fn(g("text"), tuple(g("mime_prefixes") or []), g("exts") or [], bool(g("reject_spaces")))
    if func == "_looks_like_matrix_image_filename":
        return fn(g("text"))
    if func == "_looks_like_matrix_media_filename":
        return fn(g("text"))
    if func == "_is_bare_media_filename":
        return fn(g("msgtype"), g("body"))
    if func == "_matrix_event_timestamp_seconds":
        return fn(attr_obj(g("event")))
    if func == "_normalize_e2ee_mode":
        return fn(g("value"))
    if func == "_resolve_e2ee_mode":
        return fn(g("extra"))
    if func == "_env_truthy":
        return fn(g("name"), g("default", ""))
    if func == "_env_number":
        cast = {"int": int, "float": float}.get(g("cast") or "int", int)
        return fn(g("name"), g("default"), cast)
    if func == "_csv_set":
        return fn(g("raw"))
    if func == "_thread_root":
        return fn(g("relates_to"))
    if func == "_extra_csv_set":
        return fn(cfg_obj(g("config")), g("key"), g("env_name"))
    if func == "_scoped_recovery_key":
        return fn()
    if func == "_redact_url_for_log":
        return fn(g("url"))
    if func == "matrix_deps_present":
        return fn()
    if func == "check_matrix_requirements":
        return fn()
    raise RuntimeError(f"unknown func {func}")


def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    install_global_seams(ctx)
    ret = None
    try:
        ret = _invoke(func, args, ctx)
    except BaseException as exc:
        spec = ctx.spec_for(exc)
        ret = spec if spec is not None else proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": {}}


# ── seam-name set cross-check (C calls must equal oracle stubs) ──
SEAM_NAMES = list(SIBLING_SPECS.keys())


def _c_seams():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "matrix_adapter_flow4.c",
                encoding="utf-8").read()
    return set(re.findall(r'ma4_call\s*\(\s*ctx\s*,\s*"([^"]+)"', csrc))


def cross_check():
    registry = set(SEAM_NAMES)
    cseams = _c_seams()
    if cseams != registry:
        print(f"FATAL oracle_matrix_adapter_flow4: seam set drift C vs registry: "
              f"extra={sorted(cseams - registry)} missing={sorted(registry - cseams)}")
        sys.exit(2)
    used = set(SIBLING_SPECS)
    if used != registry:
        print(f"FATAL oracle_matrix_adapter_flow4: seam set drift oracle vs registry: "
              f"extra={sorted(used - registry)} missing={sorted(registry - used)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "matrix_adapter_flow4.jsonl"
    if fix.exists():
        bad = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            bad |= set((json.loads(line).get("in") or {}).get("io") or {})
        bad -= registry
        if bad:
            print(f"FATAL oracle_matrix_adapter_flow4: fixture io keys without a seam: {sorted(bad)}")
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
    print(f"ORACLE matrix_adapter_flow4: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "matrix_adapter_flow4.jsonl"
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
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            for nm in METHOD_FUNCS:
                if f"'{nm}'" in msg:
                    bad += 1
                    print(f"SELFTEST BINDING {func} case {n}: {msg}")
    print(f"SELFTEST matrix_adapter_flow4: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    low = [f for f, c in counts.items() if c < 6]
    if low:
        print("WARNING funcs with <6 cases:", low)
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
