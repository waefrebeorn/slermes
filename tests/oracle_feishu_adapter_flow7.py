#!/usr/bin/env python3
"""Oracle for the Feishu adapter proxy/onboarding/reaction/batch FLOW port
(C sibling: src/gateway/platforms/feishu_adapter_flow7.c).

Drives the LIVE plugins/platforms/feishu/adapter.py defs with SCRIPTED seams:
every collaborator is a recording stub fed the same scripts the C harness feeds
its universal seam fa7_call. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the
(always empty) state.

The live defs are AST-extracted from adapter.py (future-annotations compiler
flag; decorators stripped); the module constants _ONBOARD_ACCOUNTS_URLS and
_ONBOARD_OPEN_URLS are seeded from the source. The heavy quarry import chain is
never imported.

Mode table (mirrors the C port): each ported function runs its REAL body when
it is the case target; a ported sibling is real only when it is itself the
target and a recorder in every other case. All other collaborators are
recorders in every case.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included); scripted raises round-trip their spec
bare. Summary: 'ORACLE feishu_adapter_flow7: N cases, M mismatches'.
"""
import ast
import asyncio
import copy
import inspect
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import PurePosixPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = __import__("pathlib").Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "plugins", "platforms", "feishu", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

TARGETS = [
    "__getattr__", "__repr__",
    "_accounts_base_url", "_onboard_open_base_url", "_parse_bot_response",
    "_reactions_enabled", "_text_batch_is_compatible",
    "_begin_registration", "_init_registration", "_apply_yaml_config",
    "_build_onboard_client", "probe_bot", "_probe_bot_sdk",
    "_add_reaction", "_remove_reaction",
    "_media_batch_key", "_media_batch_is_compatible", "_get_cached_sender_name",
]


def _lit_constant(path, name):
    tree = ast.parse(open(path, encoding="utf-8").read())
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            for t in tgts:
                if isinstance(t, ast.Name) and t.id == name:
                    return ast.literal_eval(node.value)
    raise KeyError(name)


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def extract():
    mod = types.ModuleType("fa7_oracle_adapter")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    from typing import Any, Dict, List, Literal, Optional, Sequence
    ns.update({
        "Path": PurePosixPath,
        "SimpleNamespace": types.SimpleNamespace,
        "Any": Any, "Dict": Dict, "List": List, "Literal": Literal,
        "Optional": Optional, "Sequence": Sequence,
        "_ONBOARD_ACCOUNTS_URLS": _lit_constant(SRC, "_ONBOARD_ACCOUNTS_URLS"),
        "_ONBOARD_OPEN_URLS": _lit_constant(SRC, "_ONBOARD_OPEN_URLS"),
        "FeishuAdapter": None,
    })
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    found = set()
    for node in tree.body:
        if isinstance(node, ast.ClassDef):
            for ch in node.body:
                if isinstance(ch, (ast.FunctionDef, ast.AsyncFunctionDef)) and ch.name in TARGETS:
                    c = copy.copy(ch)
                    c.decorator_list = []
                    exec_node(src, c, ns, SRC)
                    found.add(ch.name)
        elif isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in TARGETS:
            c = copy.copy(node)
            c.decorator_list = []
            exec_node(src, c, ns, SRC)
            found.add(node.name)
    missing = [t for t in TARGETS if t not in ns]
    if missing:
        print(f"FATAL oracle_feishu_adapter_flow7: adapter missing targets: {missing}")
        sys.exit(2)
    return ns


NS = extract()

# ── seam names regex-extracted from the C port (SEAM-NAME SET EQUALITY) ──
_CSRC = open(_REPO / "src" / "gateway" / "platforms" / "feishu_adapter_flow7.c",
             encoding="utf-8").read()
SEAMED = set(re.findall(r'fa7_call\(\s*ctx,\s*"([^"]+)"', _CSRC))
REAL_DEFS = {nm: NS[nm] for nm in SEAMED if nm in NS}

# Ported siblings (targets that are also seams).
PORTED_SIBLINGS = set(SEAMED) & set(TARGETS)

# ── canonical projection ──
def canon(v):
    def conv(x):
        if isinstance(x, (frozenset, set)):
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
EXC_CLASSES = {
    "Exception": Exception, "BaseException": BaseException,
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "OSError": OSError, "IOError": OSError,
    "LookupError": LookupError, "StopIteration": StopIteration, "IndexError": IndexError,
    "ZeroDivisionError": ZeroDivisionError, "AssertionError": AssertionError,
    "NotImplementedError": NotImplementedError, "SyntaxError": SyntaxError,
    "UnicodeError": UnicodeError, "Warning": Warning, "ArithmeticError": ArithmeticError,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "CancelledError": type("CancelledError", (BaseException,), {}),
}
BASE_EXC = {"BaseException", "SystemExit", "KeyboardInterrupt", "GeneratorExit", "CancelledError"}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = (BaseException,) if kind in BASE_EXC else (Exception,)
        cls = type(kind, cls, {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── seam argument projections (MUST match fa7_call args in the C port) ──
ARGMAP = {
    "_accounts_base_url": lambda domain: {"domain": domain},
    "_apply_yaml_bridge": lambda feishu_cfg, specs: {"feishu_cfg": feishu_cfg},
    "_build_lark_client": lambda app_id, app_secret, sdk_domain: {
        "app_id": app_id, "app_secret": app_secret, "sdk_domain": sdk_domain},
    "_build_onboard_client": lambda app_id, app_secret, domain: {
        "app_id": app_id, "app_secret": app_secret, "domain": domain},
    "_get_scoped_secret": lambda key, default="true": {"key": key},
    "_load_lark_oapi": lambda: {},
    "_parse_bot_response": lambda data: {"data": data},
    "_post_registration": lambda base_url, body: {"base_url": base_url, "body": body},
    "_probe_bot_http": lambda app_id, app_secret, domain: {
        "app_id": app_id, "app_secret": app_secret, "domain": domain},
    "_probe_bot_sdk": lambda app_id, app_secret, domain: {
        "app_id": app_id, "app_secret": app_secret, "domain": domain},
    "_reaction_call": lambda verb, message_id, ident, build_request, api: {
        "verb": verb, "message_id": message_id, "ident": ident,
        "build_request": "<build>", "api": "<api>"},
    "_sdk_domain": lambda domain_name: {"domain_name": domain_name},
    "_target": lambda: {},
    "_tenant_get_request": lambda uri: {"uri": uri},
    "_text_batch_is_compatible": lambda existing, incoming: {
        "existing": existing, "incoming": incoming},
    "_text_batch_key": lambda event: {"event": event},
    "cache_get": lambda key: {"sender_id": key},
    "cache_pop": lambda key, default=None: {"sender_id": key},
    "client_request": lambda req: {"request": req},   # client injected by wrapper
    "getattr_content": lambda raw, name="content": {"raw": raw, "name": name},
    "getattr_raw": lambda resp, name="raw": {"resp": resp, "name": name},
    "getattr_reaction": lambda data, name="reaction_id": {"data": data, "name": name},
    "getattr_target": lambda target, name: {"target": target, "name": name},
    "json_loads": lambda text: {"text": text},
    "logger_debug": lambda message, *a, **k: {"message": (message % a) if a else message},
    "repr_target": lambda target: {"target": target},
    "time_now": lambda: {},
}

# ── per-case script backend (mirrors C fa7_call exactly) ──
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


# ── carrier doubles (mirror the C "read seam" projections) ──
class _AttrDouble:
    def __init__(self, raw, ctx, seam, key):
        object.__setattr__(self, "_raw", raw)
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "_seam", seam)
        object.__setattr__(self, "_key", key)

    def __getattr__(self, name):
        r = self._ctx.call(self._seam, {self._key: canon(self._raw), "name": name})
        return wrap_result(self._seam, r, self._ctx, _ACTIVE_TARGET[0])


class _ReprDouble:
    def __init__(self, raw, ctx):
        object.__setattr__(self, "_raw", raw)
        object.__setattr__(self, "_ctx", ctx)

    def __repr__(self):
        return self._ctx.call("repr_target", {"target": canon(self._raw)})


class _ClientDouble:
    def __init__(self, raw, ctx):
        object.__setattr__(self, "_raw", raw)
        object.__setattr__(self, "_ctx", ctx)

    def request(self, req):
        r = self._ctx.call("client_request", {
            "client": canon(self._raw), "request": canon(req)})
        return wrap_result("client_request", r, self._ctx, _ACTIVE_TARGET[0])


class _Chain:
    def __getattr__(self, name):
        return self

    def __bool__(self):
        return True


class _CacheDouble:
    def __init__(self, ctx):
        object.__setattr__(self, "_ctx", ctx)

    def get(self, key):
        return self._ctx.call("cache_get", {"sender_id": key})

    def pop(self, key, default=None):
        return self._ctx.call("cache_pop", {"sender_id": key})


def wrap_result(name, r, ctx, target):
    if r is None:
        return None
    if name == "_target":
        if target == "__getattr__":
            return _AttrDouble(r, ctx, "getattr_target", "target")
        if target == "__repr__":
            return _ReprDouble(r, ctx)
    if name == "_reaction_call":
        return _AttrDouble(r, ctx, "getattr_reaction", "data")
    if name == "_build_onboard_client":
        return _ClientDouble(r, ctx)
    if name == "client_request":
        return _AttrDouble(r, ctx, "getattr_raw", "resp")
    if name == "getattr_raw":
        return _AttrDouble(r, ctx, "getattr_content", "raw")
    return r


_ACTIVE_CTX = [None]
_ACTIVE_TARGET = [None]
SELF = types.SimpleNamespace()


def _core_call(nm, ctx, argfn):
    def f(*a, **k):
        return ctx.call(nm, argfn(*a, **k))
    return f


def make_recorder(ctx, nm):
    argfn = ARGMAP[nm]

    def wrapped(r):
        return wrap_result(nm, r, ctx, _ACTIVE_TARGET[0])

    if nm == "_reaction_call":
        core = _core_call(nm, ctx, argfn)
        async def af(*a, **k):
            return wrapped(core(*a, **k))
        return af

    def sf(*a, **k):
        return wrapped(ctx.call(nm, argfn(*a, **k)))
    return sf


def install_recorders(ctx, target):
    for nm, real in REAL_DEFS.items():
        NS[nm] = real if nm == target else make_recorder(ctx, nm)
    for nm in SEAMED - set(REAL_DEFS):
        NS[nm] = make_recorder(ctx, nm)
    NS["time"] = types.SimpleNamespace(time=make_recorder(ctx, "time_now"))
    NS["json"] = types.SimpleNamespace(loads=make_recorder(ctx, "json_loads"))
    NS["logger"] = types.SimpleNamespace(debug=make_recorder(ctx, "logger_debug"))
    # Self double: ported siblings + foreign method seams used via self./FeishuAdapter.
    SELF.__dict__.clear()
    for nm in PORTED_SIBLINGS:
        if nm in NS:
            setattr(SELF, nm, NS[nm])
    for nm in ("_target", "_text_batch_key"):
        if nm in NS:
            setattr(SELF, nm, NS[nm])
    NS["FeishuAdapter"] = SELF


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


def mat_event(raw):
    if isinstance(raw, dict):
        return types.SimpleNamespace(**{k: mat_event(v) for k, v in raw.items()})
    if isinstance(raw, list):
        return [mat_event(i) for i in raw]
    return raw


def _run(x):
    if inspect.isawaitable(x):
        return asyncio.run(x)
    return x


# ── per-case live call ──
def live_call(func, args, ctx):
    _ACTIVE_CTX[0] = ctx
    g = args.get
    if func == "__getattr__":
        return NS[func](SELF, g("name", ""))
    if func == "__repr__":
        return NS[func](SELF)
    if func == "_accounts_base_url" or func == "_onboard_open_base_url":
        return NS[func](g("domain", ""))
    if func == "_parse_bot_response":
        return NS[func](g("data") or {})
    if func == "_reactions_enabled":
        return NS[func](SELF)
    if func == "_text_batch_is_compatible":
        return NS[func](mat_event(g("existing")), mat_event(g("incoming")))
    if func == "_begin_registration":
        return NS[func](g("domain", "feishu"))
    if func == "_init_registration":
        return NS[func](g("domain", "feishu"))
    if func == "_apply_yaml_config":
        return NS[func](g("yaml_cfg"), g("feishu_cfg"))
    if func == "_build_onboard_client":
        return NS[func](g("app_id", ""), g("app_secret", ""), g("domain", ""))
    if func == "probe_bot":
        return NS[func](g("app_id", ""), g("app_secret", ""), g("domain", ""))
    if func == "_add_reaction":
        SELF._client = _Chain() if g("client") else g("client")
        SELF._reaction_call = NS["_reaction_call"]
        return NS[func](SELF, g("message_id", ""), g("emoji_type", ""))
    if func == "_remove_reaction":
        SELF._client = _Chain() if g("client") else g("client")
        SELF._reaction_call = NS["_reaction_call"]
        return NS[func](SELF, g("message_id", ""), g("reaction_id", ""))
    if func == "_media_batch_key":
        return NS[func](SELF, mat_event(g("event")))
    if func == "_media_batch_is_compatible":
        return NS[func](mat_event(g("existing")), mat_event(g("incoming")))
    if func == "_get_cached_sender_name":
        SELF._sender_name_cache = _CacheDouble(ctx)
        return NS[func](SELF, g("sender_id"))
    if func == "_probe_bot_sdk":
        return NS[func](g("app_id", ""), g("app_secret", ""), g("domain", ""))
    raise RuntimeError(f"unknown func {func}")


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    _ACTIVE_TARGET[0] = func
    install_recorders(ctx, func)
    try:
        ret = _run(live_call(func, args, ctx))
    except BaseException as exc:
        ret = exc_spec(exc, ctx)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": {}}


# ── cross-checks ──
def cross_check():
    missing_argmap = sorted(SEAMED - set(ARGMAP))
    if missing_argmap:
        print(f"FATAL oracle_feishu_adapter_flow7: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra = sorted(set(ARGMAP) - SEAMED)
    if extra:
        print(f"FATAL oracle_feishu_adapter_flow7: ARGMAP seams absent from C: {extra}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow7.jsonl"
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
            print(f"FATAL oracle_feishu_adapter_flow7: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        missing_funcs = sorted(set(TARGETS) - fix_funcs)
        if missing_funcs:
            print(f"FATAL oracle_feishu_adapter_flow7: targets without fixture cases: {missing_funcs}")
            sys.exit(2)
        unknown_funcs = sorted(fix_funcs - set(TARGETS))
        if unknown_funcs:
            print(f"FATAL oracle_feishu_adapter_flow7: fixture funcs not in TARGETS: {unknown_funcs}")
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
    print(f"ORACLE feishu_adapter_flow7: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow7.jsonl"
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
        if case.get("_note") == "fail-closed":
            continue
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
    print(f"SELFTEST feishu_adapter_flow7: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
