#!/usr/bin/env python3
"""Oracle for the Feishu adapter SDK-builder / dependency-gate FLOW remainder
port (C sibling: src/gateway/platforms/feishu_adapter_flow4.c).

Drives the LIVE plugins/platforms/feishu/adapter.py builder/gate cluster with
SCRIPTED seams: every seamed sibling / foreign call is a recording stub fed the
same scripts the C harness feeds its universal seam fa4_call. Compares the
DECISION SEQUENCE (ordered seam events {"name","args","ret"} in "trace") plus
the final return ("ret") and the (always empty) state.

The live defs are AST-extracted from adapter.py (module-level functions and the
two FeishuAdapter static methods; future-annotations compiler flag; decorators
stripped). The heavy quarry import chain is never imported; the two in-function
package imports (pm.extras.available / pm.ensure_import) and the seamed sibling
_sdk_build are stubbed per case.

Mode table (mirrors the C port): each ported function runs its REAL body when it
is the case target; seamed calls are scripted. _sdk_build is real only as the
target and a recorder when a wrapper calls it. Logger is unused here.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included); scripted raises round-trip their spec
bare. Summary: 'ORACLE feishu_adapter_flow4: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- LARK_DOMAIN / FEISHU_DOMAIN and request-class values are marker strings.
- `fields` travels as an ordered array of {"name","value"} pairs and projects
  to **fields on the live side.
- Integral floats == ints (canon folds). dict comparison order-insensitive;
  trace event LIST order is semantic.
- full builtin exception hierarchy: Exception subclasses are swallowed by the
  two dependency gates (-> False); the BaseException family propagates.
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

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "plugins", "platforms", "feishu", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

TARGETS = [
    "_sdk_domain", "_card_button", "_card", "_sdk_build",
    "feishu_deps_present", "check_feishu_requirements",
    "_build_get_chat_request", "_build_file_upload_body",
]


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


def extract():
    mod = new_module("fa4_oracle_adapter")
    ns = mod.__dict__
    from typing import Any, Dict, List, Literal, Optional, Sequence
    from types import SimpleNamespace
    ns.update({
        "SimpleNamespace": SimpleNamespace,
        "Any": Any, "Dict": Dict, "List": List, "Literal": Literal,
        "Optional": Optional, "Sequence": Sequence,
        # marker module constants (shared with the C port)
        "LARK_DOMAIN": "LARK_DOMAIN", "FEISHU_DOMAIN": "FEISHU_DOMAIN",
        "CreateFileRequestBody": "CreateFileRequestBody",
        "GetChatRequest": "GetChatRequest",
        "FEISHU_AVAILABLE": False,
    })
    # in-function import stubs: every intermediate package must exist.
    pm = new_module("pm"); pm.__path__ = []
    pmextras = new_module("pm.extras"); pmextras.__path__ = []
    setattr(pmextras, "available", lambda *a, **k: None)
    setattr(pm, "ensure_import", lambda *a, **k: None)
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    found = set()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in TARGETS:
            exec_node(src, node, ns, SRC)
            found.add(node.name)
        elif isinstance(node, ast.ClassDef) and node.name == "FeishuAdapter":
            for ch in node.body:
                if isinstance(ch, (ast.FunctionDef, ast.AsyncFunctionDef)) and ch.name in TARGETS:
                    c = copy.copy(ch)
                    c.decorator_list = []
                    exec_node(src, c, ns, SRC)
                    found.add(ch.name)
    missing_targets = [t for t in TARGETS if t not in found]
    if missing_targets:
        print(f"FATAL oracle_feishu_adapter_flow4: adapter missing targets: {missing_targets}")
        sys.exit(2)
    return ns


NS = extract()


# ── seam names regex-extracted from the C port ──
_CSRC = open(_REPO / "src" / "gateway" / "platforms" / "feishu_adapter_flow4.c",
             encoding="utf-8").read()
SEAMED = set(re.findall(r'fa4_call\(\s*ctx,\s*"([^"]+)"', _CSRC))
REAL_DEFS = {nm: NS[nm] for nm in SEAMED if nm in NS and nm in TARGETS}


# ── canonical projection ──
def canon(v):
    def conv(x):
        if isinstance(x, frozenset):
            return sorted(conv(i) for i in x)
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
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


# ── exception hierarchy (full builtin side) ──
# Exception side -> swallowed by `except Exception` in the two gates.
# BaseException side -> propagates. C fa4_is_base_exc() mirrors the second set.
EXC_CLASSES = {
    "Exception": Exception, "BaseException": BaseException,
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "OSError": OSError, "IOError": OSError, "LookupError": LookupError,
    "StopIteration": StopIteration, "IndexError": IndexError,
    "ZeroDivisionError": ZeroDivisionError, "AssertionError": AssertionError,
    "NotImplementedError": NotImplementedError, "SyntaxError": SyntaxError,
    "UnicodeError": UnicodeError, "Warning": Warning,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "CancelledError": type("CancelledError", (BaseException,), {}),
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── seam argument projections ──
def pairs(**fields):
    return [{"name": k, "value": v} for k, v in fields.items()]


ARGMAP = {
    "builder_new": lambda cls: {"cls": cls},
    "builder_set": lambda name, value: {"name": name, "value": value},
    "builder_build": lambda: {},
    "_sdk_build": lambda cls, **fields: {"cls": cls, "fields": pairs(**fields)},
    "available": lambda extra: {"extra": extra},
    "ensure_import": lambda extra: {"extra": extra},
}


class BuilderDouble:
    """Chained builder: any setter records builder_set; .build records
    builder_build. Scripted builder_new/set returns are ignored (per the C)."""
    def __init__(self, ctx):
        object.__setattr__(self, "_ctx", ctx)

    def __getattr__(self, name):
        ctx = object.__getattribute__(self, "_ctx")

        def setter(value):
            return make_recorder(ctx, "builder_set")(name, value)
        return setter

    def build(self):
        ctx = object.__getattribute__(self, "_ctx")
        return make_recorder(ctx, "builder_build")()


def make_cls(ctx, marker):
    cls = type("ClsDouble", (), {})
    cls.builder = lambda: make_recorder(ctx, "builder_new")(marker)
    return cls


# Builders applied to a seam's scripted return BEFORE the live body consumes it.
def _builder_wrap(ctx, raw):
    return BuilderDouble(ctx)


BUILD = {
    "builder_new": _builder_wrap,
    "builder_set": _builder_wrap,
}


# ── per-case script backend (mirrors C fa4_call exactly) ──
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
                self._raise(name, args, spec, make_exc(rs))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return ret


def make_recorder(ctx, nm):
    argfn = ARGMAP[nm]
    build = BUILD.get(nm)

    def f(*a, **k):
        raw = ctx.call(nm, argfn(*a, **k))
        return build(ctx, raw) if build else raw
    return f


def install_recorders(ctx, target):
    for nm, real in REAL_DEFS.items():
        NS[nm] = real if nm == target else make_recorder(ctx, nm)
    setattr(sys.modules["pm.extras"], "available", make_recorder(ctx, "available"))
    setattr(sys.modules["pm"], "ensure_import", make_recorder(ctx, "ensure_import"))


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


# ── per-case live call ──
def live_call(func, args, ctx):
    if func == "_sdk_domain":
        return NS[func](args.get("domain_name", ""))
    if func == "_card_button":
        return NS[func](args.get("label", ""), args.get("btn_type", ""), args.get("value"))
    if func == "_card":
        title, template, markdown = args.get("title", ""), args.get("template", ""), args.get("markdown", "")
        acts = args.get("actions")
        if acts is None:
            return NS[func](title, template, markdown)
        return NS[func](title, template, markdown, actions=acts)
    if func == "_sdk_build":
        cls = args.get("request_cls")
        fields = {p["name"]: p["value"] for p in (args.get("fields") or [])}
        rc = make_cls(ctx, cls) if cls is not None else None
        return NS[func](rc, **fields)
    if func in ("feishu_deps_present", "check_feishu_requirements"):
        NS["FEISHU_AVAILABLE"] = bool(args.get("FEISHU_AVAILABLE", False))
        return NS[func]()
    if func == "_build_get_chat_request":
        return NS[func](args.get("chat_id", ""))
    if func == "_build_file_upload_body":
        # signature is keyword-only: (*, file_type, file_name, file, duration=0)
        return NS[func](file_type=args.get("file_type", ""), file_name=args.get("file_name", ""),
                        file=args.get("file"), duration=args.get("duration", 0))
    raise RuntimeError(f"unknown func {func}")


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    install_recorders(ctx, func)
    try:
        ret = live_call(func, args, ctx)
    except BaseException as exc:
        ret = exc_spec(exc, ctx)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": {}}


# ── cross-checks ──
def cross_check():
    missing_argmap = sorted(SEAMED - set(ARGMAP))
    if missing_argmap:
        print(f"FATAL oracle_feishu_adapter_flow4: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra = sorted(set(ARGMAP) - SEAMED)
    if extra:
        print(f"FATAL oracle_feishu_adapter_flow4: ARGMAP seams absent from C: {extra}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow4.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func"))
        bad = fix_keys - SEAMED
        if bad:
            print(f"FATAL oracle_feishu_adapter_flow4: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        missing_funcs = sorted(set(TARGETS) - fix_funcs)
        if missing_funcs:
            print(f"FATAL oracle_feishu_adapter_flow4: targets without fixture cases: {missing_funcs}")
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
    print(f"ORACLE feishu_adapter_flow4: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow4.jsonl"
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
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            for nm in TARGETS:
                if f"'{nm}'" in msg:
                    bad += 1
                    print(f"SELFTEST BINDING GAP {func} case {n}: {msg}")
                    break
    print(f"SELFTEST feishu_adapter_flow4: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
