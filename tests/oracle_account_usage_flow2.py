#!/usr/bin/env python3
"""Oracle for the FLOW port of the UNCLAIMED remainder of the
agent/account_usage*.py family (src/agent/account_usage_flow2.c, au2_* entries).

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"] with scripted universal-seam stubs, and
compares the ordered trace, the return, and the state projection. Live defs are
AST-extracted (py3.14 quarry venv): _get_json/_plural/_data (agent/account_usage.py,
_data is the nested closure inside _fetch_openrouter_account_usage),
_fetch_portal_account (agent/account_usage_nous.py), and the 12 defs of
agent/account_usage_cache.py. Claimed siblings (fetch_account_usage,
_codex_principal_identity, hermes_home_key, the Nous facade defs) and all I/O are
recording seams fed the SAME scripts the C harness feeds. Ported siblings stay
REAL so composition is exercised (_key/_claim_refresh/_release_refresh).

Fail-closed: 0 cases => exit 1; a missing/exhausted script RETURNS a Raise spec
(recorded before raising) so the trace matches the C dispatcher. BaseException
caught per case -> __raise__ spec. Every `except Exception` site passes the
BaseException trio through; snapshot_is_stale's 5-tuple except is subclass-aware;
_fetch_portal_account's `except BaseException` catches EVERY kind (callback fires
even for the trio) then re-raises, and its `finally: pool.shutdown` always runs.
Stateful cache entries echo the FULL state projection on success AND raise.
Summary: 'ORACLE account_usage_flow2: N cases, M mismatches'.

State model: slot key = 4-tuple (home, provider, identity_id, base_url) encoded
length-prefixed "".join(f"{len(s)}:{s}"); _inflight a set -> sorted array both
sides. Snapshot carriers carry the full dataclass field set. fetched_at is
integer epoch-MICROSECONDS (UTC) or a {__raise__,message} spec modelling a
.timestamp() that raises. threading.Lock (_lock) is inert scaffolding (no seam).
"""
import ast
import asyncio  # imported BEFORE stdlib stubs so CancelledError stays bound
import builtins
import calendar
import contextlib
import copy
import dataclasses
import json
import math
import os
import sys
import types
import textwrap
import __future__
from datetime import datetime, UTC
from pathlib import Path
from types import SimpleNamespace

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "account_usage.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))

AU_SRC = str(QUARRY / "agent" / "account_usage.py")
NOUS_SRC = str(QUARRY / "agent" / "account_usage_nous.py")
CACHE_SRC = str(QUARRY / "agent" / "account_usage_cache.py")

ANN_FLAG = __future__.annotations.compiler_flag

AU_FUNCS = ["_get_json", "_plural", "_data"]
NOUS_FUNCS = ["_fetch_portal_account"]
CACHE_FUNCS = [
    "_key", "has_account_usage", "_identity_id_for", "remember_account_usage",
    "cached_account_usage", "snapshot_is_stale", "_claim_refresh", "_release_refresh",
    "refresh_account_usage_async", "_refresh_legacy",
    "refresh_account_usage_entries_async", "_refresh_entries",
]
FUNCS = AU_FUNCS + NOUS_FUNCS + CACHE_FUNCS
# Only the entries that mutate the four module dicts emit a state projection;
# _key / has_account_usage / _identity_id_for / snapshot_is_stale are PURE (st={}).
STATEFUL = {
    "remember_account_usage", "cached_account_usage", "_claim_refresh", "_release_refresh",
    "refresh_account_usage_async", "_refresh_legacy",
    "refresh_account_usage_entries_async", "_refresh_entries",
}

CURRENT = {"rec": None, "ctx": None, "stateful": False, "thread_construction": None}
_MISSING = object()
NS = {}
REAL = {}


def _stub_fn(seam):
    def f(*a, **k):
        return CURRENT["rec"](seam)(*a, **k)
    return f


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


class _HTTPStatusError(Exception):
    def __init__(self, msg, status_code=0):
        super().__init__(msg)
        self.response = SimpleNamespace(status_code=status_code)


_BUILTIN_EXC = {}
for _n in dir(builtins):
    _c = getattr(builtins, _n)
    if isinstance(_c, type) and issubclass(_c, BaseException):
        _BUILTIN_EXC[_n] = _c


def build_exc(spec):
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    if kind == "HTTPStatusError":
        return _HTTPStatusError(msg, spec.get("status_code", 0))
    if kind == "CancelledError":
        return asyncio.CancelledError(msg)
    if kind in _BUILTIN_EXC:
        cls = _BUILTIN_EXC[kind]
        try:
            return cls(msg)
        except Exception:
            e = cls.__new__(cls)
            Exception.__init__(e, msg)
            return e
    return type(kind, (Exception,), {})(msg)


class Script:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.handed = []

    def next(self, name):
        entry = self.io.get(name, _MISSING)
        if entry is _MISSING:
            return self._fail(name)
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return self._fail(name)
            self.cursors[name] = idx + 1
            val = entry[idx]
        else:
            val = entry
        return self._unwrap(name, val)

    def _fail(self, name):
        spec = {"__raise__": "RuntimeError", "message": "no script for " + name}
        r = Raise(spec, build_exc(spec))
        self.handed.append(r)
        return r

    def _unwrap(self, name, val):
        if isinstance(val, dict) and "__raise__" in val:
            r = Raise(val, build_exc(val))
            self.handed.append(r)
            return r
        if isinstance(val, dict) and val.get("__script__") is True:
            return val.get("ret")
        return val


# ---- key encoding (length-prefixed printable ASCII) ----
def keystr(key):
    return "".join(f"{len(s)}:{s}" for s in key)


def decode_keystr(ks):
    parts = []
    i = 0
    while i < len(ks):
        j = ks.index(":", i)
        n = int(ks[i:j])
        parts.append(ks[j + 1:j + 1 + n])
        i = j + 1 + n
    return tuple(parts)


# ---- carrier doubles (attribute access fires seams) ----
class ClientDouble:
    def __init__(self, marker):
        self.marker = marker

    def __enter__(self):
        return CURRENT["rec"]("client.__enter__")(client=self.marker)

    def __exit__(self, et, ev, tb):
        return CURRENT["rec"]("client.__exit__")(client=self.marker, exc=CURRENT["exc_proj"](ev))

    def get(self, url, headers=None):
        return CURRENT["rec"]("client.get")(client=self.marker, url=url, headers=headers)


class ResponseDouble:
    def __init__(self, marker):
        self.marker = marker

    def raise_for_status(self):
        return CURRENT["rec"]("response.raise_for_status")(response=self.marker)

    def json(self):
        return CURRENT["rec"]("response.json")(response=self.marker)


def _make_run(marker):
    def run(*a, **k):
        pass
    run.__au2_ctx__ = marker
    return run


class ContextDouble:
    def __init__(self, marker):
        self.marker = marker
        self.run = _make_run(marker)


class PoolDouble:
    def __init__(self, marker):
        self.marker = marker

    def submit(self, fn, read):
        ctx = getattr(fn, "__au2_ctx__", None)
        return CURRENT["rec"]("pool.submit")(context=ctx, read=read)

    def shutdown(self, wait=False):
        return CURRENT["rec"]("pool.shutdown")(wait=wait)


class FutureDouble:
    def __init__(self, marker):
        self.marker = marker

    def result(self, timeout=None):
        return CURRENT["rec"]("future.result")(timeout=timeout)

    def add_done_callback(self, cb):
        return CURRENT["rec"]("future.add_done_callback")(
            callback={"__callable__": "future.exception_drain"})


class ThreadDouble:
    def __init__(self, marker, construction):
        self.marker = marker
        self.construction = construction

    def start(self):
        return CURRENT["rec"]("thread.start")(thread=self.marker)


class RaiseTimestamp:
    def __init__(self, spec):
        self.spec = spec

    def timestamp(self):
        raise build_exc(self.spec)


# ---- datetime model ----
def us_to_dt(us):
    if us is None:
        return None
    us = int(us)
    return datetime.fromtimestamp(us // 1_000_000, tz=UTC).replace(microsecond=us % 1_000_000)


def dt_to_us(dt):
    if dt is None:
        return None
    return calendar.timegm(dt.utctimetuple()) * 1_000_000 + dt.microsecond


def norm_value(v):
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, (ClientDouble, ResponseDouble, ContextDouble, PoolDouble, FutureDouble)):
        return norm_value(v.marker)
    if isinstance(v, ThreadDouble):
        return norm_value(v.construction)
    if isinstance(v, datetime):
        return dt_to_us(v)
    if dataclasses.is_dataclass(v) and not isinstance(v, type):
        return {f.name: norm_value(getattr(v, f.name)) for f in dataclasses.fields(v)}
    if isinstance(v, SimpleNamespace):
        return {k: norm_value(x) for k, x in vars(v).items()}
    if isinstance(v, dict):
        return {k: norm_value(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_value(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted(norm_value(x) for x in v)
    if isinstance(v, float):
        if math.isfinite(v) and v == int(v):
            return int(v)
        return v
    return v


def norm_args(v):
    v = norm_value(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    if isinstance(v, list):
        return [norm_args(x) for x in v]
    return v


def _thread_proj_from_k(k):
    at = k.get("args") or ()
    fn = at[0] if at else None
    if fn is REAL.get("_refresh_legacy"):
        return {"target": "context.run", "fn": "_refresh_legacy",
                "provider": at[1], "key": list(at[2]),
                "name": k.get("name"), "daemon": k.get("daemon")}
    return {"target": "context.run", "fn": "_refresh_entries",
            "due": at[1], "name": k.get("name"), "daemon": k.get("daemon")}


def proj(name, a, k):
    if name in ("hermes_home_key", "time.monotonic", "time.time", "usage_fetchers",
                "contextvars.copy_context"):
        return {}
    if name == "get_provider_profile":
        return {"slug": a[0]}
    if name == "codex_principal_identity":
        return {"token": a[0]}
    if name == "sha256_hexdigest":
        return {"text": k.get("text")}
    if name == "fetch_account_usage":
        return {"provider": a[0], "base_url": k.get("base_url"), "api_key": k.get("api_key"),
                "read_only": k.get("read_only"), "identity_id": k.get("identity_id")}
    if name == "httpx.Client":
        return {"timeout": k.get("timeout")}
    if name == "client.__enter__":
        return {"client": k.get("client")}
    if name == "client.get":
        return {"client": k.get("client"), "url": k.get("url"), "headers": k.get("headers")}
    if name == "response.raise_for_status":
        return {"response": k.get("response")}
    if name == "client.__exit__":
        return {"client": k.get("client"), "exc": k.get("exc")}
    if name == "response.json":
        return {"response": k.get("response")}
    if name == "DaemonThreadPoolExecutor":
        return {"max_workers": k.get("max_workers")}
    if name == "partial":
        return {"fn": "get_nous_portal_account_info", "force_fresh": k.get("force_fresh")}
    if name == "pool.submit":
        return {"context": k.get("context"), "read": k.get("read")}
    if name == "future.result":
        return {"timeout": k.get("timeout")}
    if name == "future.add_done_callback":
        return {"callback": k.get("callback")}
    if name == "pool.shutdown":
        return {"wait": k.get("wait")}
    if name == "threading.Thread":
        return _thread_proj_from_k(k)
    if name == "thread.start":
        return {"thread": k.get("thread")}
    return {**{f"arg{i}": v for i, v in enumerate(a)}, **k}


# ---- materialization ----
def mat_window(w):
    if not isinstance(w, dict):
        return w
    return NS["AccountUsageWindow"](
        label=w.get("label"), used_percent=w.get("used_percent"),
        reset_at=us_to_dt(w.get("reset_at")), detail=w.get("detail"),
        scope=w.get("scope", "account"))


def mat_snapshot(s):
    if not isinstance(s, dict):
        return s
    return NS["AccountUsageSnapshot"](
        provider=s.get("provider"), source=s.get("source"),
        fetched_at=us_to_dt(s.get("fetched_at")),
        title=s.get("title", "Account limits"), plan=s.get("plan"),
        windows=tuple(mat_window(w) for w in s.get("windows", [])),
        details=tuple(s.get("details", [])),
        unavailable_reason=s.get("unavailable_reason"),
        identity=s.get("identity"), raw=s.get("raw"),
        usage_window=s.get("usage_window", False))


def mat_snapshot_stale(s):
    if not isinstance(s, dict):
        return s
    fa = s.get("fetched_at")
    if isinstance(fa, dict) and "__raise__" in fa:
        fa_obj = RaiseTimestamp(fa)
    elif fa is None:
        fa_obj = None
    else:
        fa_obj = us_to_dt(fa)
    return NS["AccountUsageSnapshot"](
        provider=s.get("provider"), source=s.get("source"), fetched_at=fa_obj,
        title=s.get("title", "Account limits"), plan=s.get("plan"),
        windows=tuple(mat_window(w) for w in s.get("windows", [])),
        details=tuple(s.get("details", [])),
        unavailable_reason=s.get("unavailable_reason"),
        identity=s.get("identity"), raw=s.get("raw"),
        usage_window=s.get("usage_window", False))


def mat_profile(p):
    if p is None:
        return None
    hook = p.get("hook") if isinstance(p, dict) else None
    base = NS["ProviderProfile"]
    if hook == "custom":
        class _Custom(base):
            def fetch_account_usage(self, base_url=None, api_key=None):
                return None
        return _Custom()
    class _Plain(base):
        pass
    return _Plain()


def mat_entry(e):
    if isinstance(e, dict):
        return SimpleNamespace(**e)
    return e


def materialize(name, raw):
    if name in ("httpx.Client", "client.__enter__"):
        return ClientDouble(raw) if isinstance(raw, dict) else raw
    if name == "client.get":
        return ResponseDouble(raw) if isinstance(raw, dict) else raw
    if name == "contextvars.copy_context":
        return ContextDouble(raw) if isinstance(raw, dict) else raw
    if name == "DaemonThreadPoolExecutor":
        return PoolDouble(raw) if isinstance(raw, dict) else raw
    if name == "pool.submit":
        return FutureDouble(raw) if isinstance(raw, dict) else raw
    if name == "threading.Thread":
        return ThreadDouble(raw, CURRENT.get("thread_construction")) if isinstance(raw, dict) else raw
    if name == "get_provider_profile":
        return mat_profile(raw)
    return raw


def exc_proj(ev):
    if ev is None:
        return None
    ctx = CURRENT.get("ctx")
    if ctx:
        for r in ctx.script.handed:
            if ev is r.exc:
                return r.spec
    return {"__raise__": type(ev).__name__, "message": str(ev)}


CURRENT["exc_proj"] = exc_proj


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.script = Script(inp.get("io"))

    def recorder(self, name):
        ctx = self

        def call(*a, **k):
            args = norm_args(proj(name, a, k))
            raw = ctx.script.next(name)
            if isinstance(raw, Raise):
                ctx.trace.append({"ev": name, "args": args, "ret": raw.spec})
                raise raw.exc
            ctx.trace.append({"ev": name, "args": args, "ret": norm_value(raw)})
            return materialize(name, raw)
        return call


def _thread_stub(*a, **k):
    pargs = _thread_proj_from_k(k)
    if pargs["fn"] == "_refresh_legacy":
        CURRENT["thread_construction"] = {"fn": "_refresh_legacy",
                                          "provider": pargs["provider"], "key": pargs["key"]}
    else:
        CURRENT["thread_construction"] = {"fn": "_refresh_entries", "due": pargs["due"]}
    return CURRENT["rec"]("threading.Thread")(*a, **k)


def _hashlib_sha256(b):
    return SimpleNamespace(hexdigest=lambda: CURRENT["rec"]("sha256_hexdigest")(text=b.decode("utf-8")))


# ---- live namespace seeding ----
def _exec_seg(src, node, ns, filename):
    seg = ast.get_source_segment(src, node)
    seg = textwrap.dedent(seg)
    code = compile(seg, filename, "exec", flags=ANN_FLAG, dont_inherit=True)
    exec(code, ns)


def _exec_class(src, node, ns, filename):
    lines = src.splitlines()
    start = (node.decorator_list[0].lineno - 1) if node.decorator_list else (node.lineno - 1)
    seg = textwrap.dedent("\n".join(lines[start:node.end_lineno]))
    code = compile(seg, filename, "exec", flags=ANN_FLAG, dont_inherit=True)
    exec(code, ns)


def load_ns():
    global NS, REAL
    mod = types.ModuleType("au2_live")
    sys.modules["au2_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "au2_live", "math": math, "json": json,
        "dataclass": dataclasses.dataclass, "field": dataclasses.field,
        "replace": dataclasses.replace,
        "datetime": datetime, "UTC": UTC,
        "time": SimpleNamespace(monotonic=_stub_fn("time.monotonic"), time=_stub_fn("time.time")),
        "threading": SimpleNamespace(Thread=_thread_stub, Lock=lambda: contextlib.nullcontext()),
        "copy_context": _stub_fn("contextvars.copy_context"),
        "logging": SimpleNamespace(getLogger=lambda name: SimpleNamespace(
            debug=lambda *a, **k: None, info=lambda *a, **k: None)),
        "Any": object, "Optional": object, "Callable": object, "Iterable": object,
        "TypeGuard": object,
        "httpx": SimpleNamespace(Client=_stub_fn("httpx.Client"), HTTPStatusError=_HTTPStatusError),
        "_lock": contextlib.nullcontext(),
        "REFRESH_AFTER_S": 120.0, "SNAPSHOT_STALE_AFTER_S": 900.0,
        "_snapshots": {}, "_started": {}, "_last_try": {}, "_inflight": set(),
    })
    au_src = Path(AU_SRC).read_text(encoding="utf-8")
    nous_src = Path(NOUS_SRC).read_text(encoding="utf-8")
    cache_src = Path(CACHE_SRC).read_text(encoding="utf-8")
    au_tree = ast.parse(au_src)
    nous_tree = ast.parse(nous_src)
    cache_tree = ast.parse(cache_src)
    # dataclass types FIRST
    for node in au_tree.body:
        if isinstance(node, ast.ClassDef) and node.name in ("AccountUsageWindow", "AccountUsageSnapshot"):
            _exec_class(au_src, node, ns, AU_SRC)
    for nm in ("AccountUsageWindow", "AccountUsageSnapshot"):
        if nm not in ns:
            raise RuntimeError("seed missing class: " + nm)
    # cache constants
    for node in cache_tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            names = {t.id for t in tgts if isinstance(t, ast.Name)}
            if names & {"REFRESH_AFTER_S", "SNAPSHOT_STALE_AFTER_S"}:
                _exec_seg(cache_src, node, ns, CACHE_SRC)
    # target defs
    found = set()
    for node in au_tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in ("_get_json", "_plural"):
            _exec_seg(au_src, node, ns, AU_SRC)
            found.add(node.name)
    # nested _data closure inside _fetch_openrouter_account_usage
    for node in ast.walk(au_tree):
        if isinstance(node, ast.FunctionDef) and node.name == "_data":
            _exec_seg(au_src, node, ns, AU_SRC)
            found.add("_data")
            break
    for node in nous_tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in NOUS_FUNCS:
            _exec_seg(nous_src, node, ns, NOUS_SRC)
            found.add(node.name)
    for node in cache_tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in CACHE_FUNCS:
            _exec_seg(cache_src, node, ns, CACHE_SRC)
            found.add(node.name)
    missing = set(FUNCS) - found
    if missing:
        raise RuntimeError("missing target defs: %s" % sorted(missing))

    # ProviderProfile base no-op hook (real identity check in has_account_usage)
    class ProviderProfile:
        def fetch_account_usage(self, base_url=None, api_key=None):
            return None
    ns["ProviderProfile"] = ProviderProfile

    # ---- stub modules for in-function imports ----
    for pkg in ("agent", "hermes_cli", "providers", "tools"):
        p = types.ModuleType(pkg)
        p.__path__ = []
        sys.modules[pkg] = p

    def link(modname, m):
        sys.modules[modname] = m
        parts = modname.split(".")
        if len(parts) == 2:
            setattr(sys.modules[parts[0]], parts[1], m)

    # agent.account_usage: module __getattr__ fires usage_fetchers on _USAGE_FETCHERS
    au_mod = types.ModuleType("agent.account_usage")

    def au_getattr(name):
        if name == "_USAGE_FETCHERS":
            return CURRENT["rec"]("usage_fetchers")()
        if name == "fetch_account_usage":
            return _stub_fn("fetch_account_usage")
        if name == "AccountUsageSnapshot":
            return ns["AccountUsageSnapshot"]
        if name == "AccountUsageWindow":
            return ns["AccountUsageWindow"]
        raise AttributeError(name)
    au_mod.__getattr__ = au_getattr
    link("agent.account_usage", au_mod)

    link("agent.credential_pool", SimpleNamespace(_codex_principal_identity=_stub_fn("codex_principal_identity")))
    link("hermes_constants", SimpleNamespace(hermes_home_key=_stub_fn("hermes_home_key")))
    link("providers", SimpleNamespace(get_provider_profile=_stub_fn("get_provider_profile")))
    link("providers.base", SimpleNamespace(ProviderProfile=ProviderProfile))
    link("hermes_cli.nous_account", SimpleNamespace(
        get_nous_portal_account_info=object(),
        stored_token_account_info={"fn": "stored_token_account_info"},
        nous_portal_topup_url=_stub_fn("nous_portal_topup_url")))
    link("tools.daemon_pool", SimpleNamespace(DaemonThreadPoolExecutor=_stub_fn("DaemonThreadPoolExecutor")))
    # stdlib in-function imports (asyncio already imported above)
    sys.modules["contextvars"] = SimpleNamespace(copy_context=_stub_fn("contextvars.copy_context"))
    sys.modules["functools"] = SimpleNamespace(partial=_stub_fn("partial"))
    sys.modules["hashlib"] = SimpleNamespace(sha256=_hashlib_sha256)

    # audit: every target name bound
    for nm in FUNCS:
        if nm not in ns:
            raise RuntimeError("seed missing: " + nm)
    REAL = {nm: ns[nm] for nm in FUNCS}


# ---- state projection ----
def seed_state(inp):
    st = inp.get("state") or {}
    NS["_snapshots"] = {decode_keystr(k): mat_snapshot(v) for k, v in (st.get("snapshots") or {}).items()}
    NS["_started"] = {decode_keystr(k): v for k, v in (st.get("started") or {}).items()}
    NS["_last_try"] = {decode_keystr(k): v for k, v in (st.get("last_try") or {}).items()}
    NS["_inflight"] = set(decode_keystr(k) for k in (st.get("inflight") or []))


def project_state():
    if not CURRENT["stateful"]:
        return {}
    return {
        "snapshots": {keystr(k): norm_value(v) for k, v in NS["_snapshots"].items()},
        "started": {keystr(k): norm_value(v) for k, v in NS["_started"].items()},
        "last_try": {keystr(k): norm_value(v) for k, v in NS["_last_try"].items()},
        "inflight": sorted(keystr(k) for k in NS["_inflight"]),
    }


# ---- per-case drive ----
def drive(func, inp):
    g = inp.get
    if func == "_plural":
        return NS[func](g("count"))
    if func == "_get_json":
        return NS[func](g("url"), g("headers"), timeout=g("timeout"))
    if func == "_data":
        c = g("client")
        NS["client"] = ClientDouble(c) if isinstance(c, dict) else c
        NS["normalized"] = g("normalized")
        NS["headers"] = g("headers")
        return NS[func](g("path"))
    if func == "_fetch_portal_account":
        return NS[func](g("timeout"), stored_token_only=g("stored_token_only"))
    if func == "_key":
        return NS[func](g("provider"), identity_id=g("identity_id"), base_url=g("base_url"))
    if func == "has_account_usage":
        return NS[func](g("provider"))
    if func == "_identity_id_for":
        return NS[func](g("provider"), mat_entry(g("entry")))
    if func == "remember_account_usage":
        return NS[func](g("provider"), mat_snapshot(g("snapshot")),
                        identity_id=g("identity_id"), base_url=g("base_url"),
                        started_monotonic=g("started_monotonic"))
    if func == "cached_account_usage":
        return NS[func](g("provider"), identity_id=g("identity_id"), base_url=g("base_url"))
    if func == "snapshot_is_stale":
        return NS[func](mat_snapshot_stale(g("snapshot")))
    if func == "_claim_refresh":
        return NS[func](tuple(g("key")), g("now"))
    if func == "_release_refresh":
        return NS[func](tuple(g("key")))
    if func == "refresh_account_usage_async":
        return NS[func](g("providers"))
    if func == "_refresh_legacy":
        return NS[func](g("provider"), tuple(g("key")))
    if func == "refresh_account_usage_entries_async":
        return NS[func](g("requests"))
    if func == "_refresh_entries":
        return NS[func](g("requests"))
    raise RuntimeError("unknown func " + func)


def run_case(func, inp):
    for nm, fn in REAL.items():
        NS[nm] = fn
    ctx = CaseCtx(inp)
    CURRENT["rec"] = ctx.recorder
    CURRENT["ctx"] = ctx
    CURRENT["stateful"] = func in STATEFUL
    CURRENT["thread_construction"] = None
    seed_state(inp)
    try:
        ret = drive(func, inp)
        return {"ret": norm_value(ret), "trace": ctx.trace, "st": project_state()}
    except BaseException as e:
        spec = None
        for r in ctx.script.handed:
            if e is r.exc:
                spec = r.spec
                break
        if spec is None:
            spec = {"__raise__": type(e).__name__, "message": str(e)}
        return {"ret": spec, "trace": ctx.trace, "st": project_state()}


def main():
    load_ns()
    n = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        func = rec.get("func")
        inp = rec.get("in") or {}
        n += 1
        want = run_case(func, inp)
        got = {"ret": rec.get("ret"), "trace": rec.get("trace"), "st": rec.get("st")}
        got = norm_value(got)
        if got != want:
            mism += 1
            if mism <= 12:
                print("MISMATCH [%s] case %d" % (func, n))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:600]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:600]))
    print("ORACLE account_usage_flow2: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()