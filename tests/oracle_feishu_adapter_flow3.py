#!/usr/bin/env python3
"""Oracle for the Feishu adapter inbound-admission FLOW remainder port
(src/gateway/platforms/feishu_adapter_flow3.py C sibling:
 src/gateway/platforms/feishu_adapter_flow3.c).

Drives the LIVE plugins/platforms/feishu/adapter.py admission / mention-gating
cluster with SCRIPTED seams: every SEAMED sibling / foreign call is a recording
stub fed the same scripts the C harness feeds its universal seam fa3_call.
Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace") plus the final return ("ret") and the mutated state ("state").

The live defs are AST-extracted from the FeishuAdapter class body (module
constants seeded, Assign AND AnnAssign, plain-Name targets only; logger re-set
AFTER seeding). Every exec namespace is a registered sys.modules module
(py3.14). The heavy quarry import chain is never imported; the foreign
diagnostics import, the claimed siblings and the logger are stubbed.

Mode table (mirrors the C port): every ported function runs its REAL body when
it is the case target; all sibling / foreign calls are SEAMED (scripted). A
ported sibling called BY the target is scripted everywhere except when it is
itself the target. Logger levels are recorded seams {fmt, args, kwargs}.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included); scripted raises round-trip their spec
bare. Summary: 'ORACLE feishu_adapter_flow3: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- self is a SimpleNamespace double; attribute doubles are SimpleNamespace.
- _bot_identity returns {open_id,user_id,name}; _normalize seam returns
  {mentions:[{is_self:bool},...]}; _sender_identity returns an array of ids.
- Integral floats == ints (canon folds). dict comparison order-insensitive;
  trace event LIST order is semantic.
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
    "_admit", "_require_mention_for", "_allow_group_message", "_mentions_self",
    "_message_mentions_bot", "_post_mentions_bot", "_bot_identity",
    "_warn_once_empty_allowlist_deny",
]
# Ported siblings that are seamed when called by another target.
METHOD_SEAMS = [
    "_require_mention_for", "_allow_group_message", "_mentions_self",
    "_message_mentions_bot", "_post_mentions_bot", "_bot_identity", "_normalize",
]
# Claimed / foreign names installed as module-level recorders.
MODULE_SEAMS = ["_sender_identity", "_is_bot_sender", "empty_allowlist_drop_warning"]
LOGGER_SEAMS = ["logger_warning"]
DIAG_MODULE = "plugins.platforms.feishu.feishu_admission_diagnostics"


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


class _Callable:
    """Callable OBJECT with no __get__, so class-level assignment binds no self."""
    def __init__(self, fn):
        self.fn = fn

    def __call__(self, *a, **k):
        return self.fn(*a, **k)


class _BotIdentityDouble(types.SimpleNamespace):
    """Seed stand-in for the frozen dataclass _FeishuBotIdentity.

    `.matches` is a class-level callable recorder (not an instance field), so
    canon(vars(instance)) yields exactly {open_id,user_id,name} — matching the
    C port's _bot_identity result.
    """
    matches = None

    def __init__(self, open_id="", user_id="", name=""):
        self.open_id = open_id
        self.user_id = user_id
        self.name = name


def extract():
    mod = new_module("fa3_oracle_adapter")
    ns = mod.__dict__
    import collections
    import logging
    import threading
    import time as _time
    from typing import Any, Dict, List, Literal, Optional, Sequence
    from types import SimpleNamespace
    ns.update({"re": re, "json": json, "collections": collections,
               "threading": threading, "logging": logging, "os": os, "time": _time,
               "Any": Any, "Dict": Dict, "List": List, "Literal": Literal,
               "Optional": Optional, "Sequence": Sequence,
               "OrderedDict": collections.OrderedDict, "SimpleNamespace": SimpleNamespace,
               "RejectReason": str, "MessageType": types.SimpleNamespace(
                   PHOTO="photo", AUDIO="audio", VIDEO="video",
                   VOICE="voice", DOCUMENT="document", TEXT="text")})
    # Stub the foreign diagnostics module chain so the function-body import is inert.
    for m in ("plugins", "plugins.platforms", "plugins.platforms.feishu", DIAG_MODULE):
        new_module(m)
    setattr(sys.modules[DIAG_MODULE], "empty_allowlist_drop_warning", lambda **k: None)
    ns["_FeishuBotIdentity"] = _BotIdentityDouble
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
    found = set()
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "FeishuAdapter":
            for ch in node.body:
                if isinstance(ch, (ast.FunctionDef, ast.AsyncFunctionDef)) and ch.name in TARGETS:
                    c = copy.copy(ch)
                    c.decorator_list = []
                    exec_node(src, c, ns, SRC)
                    found.add(ch.name)
    missing_targets = [t for t in TARGETS if t not in found]
    if missing_targets:
        print(f"FATAL oracle_feishu_adapter_flow3: adapter missing targets: {missing_targets}")
        sys.exit(2)
    for nm in MODULE_SEAMS:
        ns.setdefault(nm, None)
    ns["logger"] = _SilentLogger()
    missing = [k for k in TARGETS if k not in ns]
    if missing:
        print(f"FATAL oracle_feishu_adapter_flow3: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()


# ── seam names regex-extracted from the C port ──
_CSRC = open(_REPO / "src" / "gateway" / "platforms" / "feishu_adapter_flow3.c",
             encoding="utf-8").read()
SEAMED = set(re.findall(r'fa3_call\(\s*ctx,\s*"([^"]+)"', _CSRC))
SEAMED |= set(re.findall(r'fa3_log\(\s*ctx,\s*"([^"]+)"', _CSRC))
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
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception hierarchy (full builtin side) ──
EXC_CLASSES = {
    "BaseException": BaseException, "Exception": Exception,
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "OSError": OSError, "IOError": OSError, "StopIteration": StopIteration,
    "IndexError": IndexError, "ZeroDivisionError": ZeroDivisionError,
    "AssertionError": AssertionError, "NotImplementedError": NotImplementedError,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "CancelledError": type("CancelledError", (Exception,), {}),
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── attribute projection for seam args ──
def proj_attr(o):
    if isinstance(o, types.SimpleNamespace):
        return {k: proj_attr(v) for k, v in vars(o).items()}
    if isinstance(o, dict):
        return {k: proj_attr(v) for k, v in o.items()}
    if isinstance(o, (list, tuple, frozenset, set)):
        return [proj_attr(i) for i in o]
    return o


ARGMAP = {
    "logger_warning": lambda fmt, *a, **k: {"fmt": fmt, "args": list(a), "kwargs": k},
    "_sender_identity": lambda sender: {"sender": proj_attr(sender)},
    "_is_bot_sender": lambda sender: {"sender": proj_attr(sender)},
    "matches": lambda *, open_id="", user_id="", name="":
        {"open_id": open_id, "user_id": user_id, "name": name},
    "_normalize": lambda msg_type, raw_content, mentions:
        {"msg_type": msg_type, "raw_content": raw_content, "mentions": proj_attr(mentions)},
    "_require_mention_for": lambda chat_id: {"chat_id": chat_id},
    "_allow_group_message": lambda sender_id, chat_id="", *, is_bot=False:
        {"sender_id": proj_attr(sender_id), "chat_id": chat_id, "is_bot": is_bot},
    "_mentions_self": lambda message: {"message": proj_attr(message)},
    "_message_mentions_bot": lambda mentions: {"mentions": proj_attr(mentions)},
    "_post_mentions_bot": lambda mentions: {"mentions": proj_attr(mentions)},
    "_bot_identity": lambda: {},
    "empty_allowlist_drop_warning": lambda *, chat_id="", group_rules=None,
        default_group_policy=None, allowed_group_users=None:
        {"chat_id": chat_id, "group_rules": proj_attr(group_rules),
         "default_group_policy": default_group_policy,
         "allowed_group_users": proj_attr(allowed_group_users)},
}

# Builders applied to the scripted return BEFORE the live function consumes it.
BUILD = {
    "_sender_identity": lambda r: frozenset(r) if isinstance(r, (list, tuple)) else frozenset(),
    "_bot_identity": lambda r: _BotIdentityDouble(**r) if isinstance(r, dict) else None,
    "_normalize": lambda r: types.SimpleNamespace(**r) if isinstance(r, dict) else r,
}


# ── per-case script backend (mirrors C fa3_call exactly) ──
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
        return build(raw) if build else raw
    return f


class RecLogger:
    def __init__(self, ctx):
        self.warning = make_recorder(ctx, "logger_warning")


def install_recorders(ctx, target):
    for nm in REAL_DEFS:
        NS[nm] = REAL_DEFS[nm]
    for nm in MODULE_SEAMS:
        fn = make_recorder(ctx, nm)
        NS[nm] = fn
        if nm == "empty_allowlist_drop_warning":
            setattr(sys.modules[DIAG_MODULE], nm, fn)
    NS["logger"] = RecLogger(ctx)
    _BotIdentityDouble.matches = _Callable(make_recorder(ctx, "matches"))


# ── argument materialization ──
def mat_attr(d):
    if isinstance(d, dict):
        return types.SimpleNamespace(**{k: mat_attr(v) for k, v in d.items()})
    if isinstance(d, list):
        return [mat_attr(i) for i in d]
    return d


def build_self(sj, target, rec):
    sj = sj or {}
    slf = types.SimpleNamespace()
    slf._bot_open_id = sj.get("_bot_open_id", "")
    slf._bot_user_id = sj.get("_bot_user_id", "")
    slf._bot_name = sj.get("_bot_name", "")
    slf._allow_bots = sj.get("_allow_bots", "")
    slf._allow_all_dm = bool(sj.get("_allow_all_dm", False))
    slf._allowed_group_users = set(sj.get("_allowed_group_users") or [])
    slf._admins = set(sj.get("_admins") or [])
    slf._group_rules = {
        k: types.SimpleNamespace(
            require_mention=(r.get("require_mention") if isinstance(r, dict) else None),
            policy=(r.get("policy", "") if isinstance(r, dict) else ""),
            allowlist=set((r.get("allowlist") or []) if isinstance(r, dict) else []),
            blacklist=set((r.get("blacklist") or []) if isinstance(r, dict) else []))
        for k, r in (sj.get("_group_rules") or {}).items()}
    slf._require_mention = sj.get("_require_mention", False)
    slf._default_group_policy = sj.get("_default_group_policy")
    slf._group_policy = sj.get("_group_policy", "")
    slf._warned_empty_allowlist_deny = bool(sj.get("_warned_empty_allowlist_deny", False))
    for nm in METHOD_SEAMS:
        if nm == target:
            continue
        setattr(slf, nm, rec(nm, slf))
    return slf


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


# Recorder factory: ported siblings bind through self; foreign/module seams ignore self.
def rec_factory(ctx):
    def rec(nm, slf):
        return make_recorder(ctx, nm)
    return rec


# ── per-case live call ──
def live_call(func, args, state_out, ctx, rec=None):
    if rec is None:
        rec = rec_factory(ctx)
    slf = build_self(args.get("self"), func, rec)
    M = lambda nm: types.MethodType(NS[nm], slf)
    if func == "_bot_identity":
        ret = M(func)()
    elif func == "_post_mentions_bot":
        ret = M(func)(mat_attr(args.get("mentions")) or [])
    elif func == "_message_mentions_bot":
        ret = M(func)(mat_attr(args.get("mentions")) or [])
    elif func == "_require_mention_for":
        ret = M(func)(args.get("chat_id", ""))
    elif func == "_allow_group_message":
        ret = M(func)(mat_attr(args.get("sender_id")), args.get("chat_id", ""),
                      is_bot=bool(args.get("is_bot", False)))
    elif func == "_mentions_self":
        ret = M(func)(mat_attr(args.get("message")))
    elif func == "_admit":
        req = args.get("require_mention", None)
        if "require_mention" not in args:
            ret = M(func)(mat_attr(args.get("sender")), mat_attr(args.get("message")))
        else:
            ret = M(func)(mat_attr(args.get("sender")), mat_attr(args.get("message")),
                          require_mention=req)
    elif func == "_warn_once_empty_allowlist_deny":
        try:
            ret = M(func)(args.get("chat_id", ""))
        finally:
            state_out["_warned_empty_allowlist_deny"] = slf._warned_empty_allowlist_deny
    else:
        raise RuntimeError(f"unknown func {func}")
    return ret


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    install_recorders(ctx, func)
    state_out = {}
    try:
        ret = live_call(func, args, state_out, ctx)
    except BaseException as exc:
        ret = exc_spec(exc, ctx)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check ──
def cross_check():
    missing_argmap = sorted(SEAMED - set(ARGMAP))
    if missing_argmap:
        print(f"FATAL oracle_feishu_adapter_flow3: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra = sorted(set(ARGMAP) - SEAMED)
    if extra:
        print(f"FATAL oracle_feishu_adapter_flow3: ARGMAP seams absent from C: {extra}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow3.jsonl"
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
            print(f"FATAL oracle_feishu_adapter_flow3: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        missing_funcs = sorted(set(TARGETS) - fix_funcs)
        if missing_funcs:
            print(f"FATAL oracle_feishu_adapter_flow3: targets without fixture cases: {missing_funcs}")
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
    print(f"ORACLE feishu_adapter_flow3: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow3.jsonl"
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
            for nm in list(TARGETS) + METHOD_SEAMS:
                if f"'{nm}'" in msg:
                    bad += 1
                    print(f"SELFTEST BINDING GAP {func} case {n}: {msg}")
                    break
    print(f"SELFTEST feishu_adapter_flow3: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
