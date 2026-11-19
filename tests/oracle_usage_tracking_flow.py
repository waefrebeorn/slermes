#!/usr/bin/env python3
"""Oracle for the FLOW + leaf port of the /usage account-usage tracking cluster
(src/agent/usage_tracking_flow.c, ut_* entries). The task named agent/
usage_tracking.py, which does not exist; it resolves to the REAL defining
modules agent/account_usage.py (the /usage facade) and agent/account_usage_nous.py
(the Nous credits / usage-window views). Each PoP credits the defining module.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"] with scripted universal-seam stubs, and
compares the ordered trace, the return, and the state projection ({} -- every
ported function is pure). Live defs are AST-extracted (py3.14 quarry venv);
claimed siblings (_utc_now, _parse_dt, _format_reset, _title_case_slug, _plural,
_get_json, _codex_backend_urls, _resolve_codex_usage_credentials,
render_account_usage_lines, fetch_account_usage, _snapshot_from_credits_state,
build_nous_credits_snapshot, nous_credits_lines, _dev_fixture_usage_model,
usage_model_from_account) and all I/O are recording seams fed the SAME scripts
the C harness feeds. The UNCLAIMED _fetch_portal_account (thread-pool mechanics)
is deferred and seamed. Ported siblings stay REAL so composition is exercised.

Fail-closed: 0 cases => exit 1; a missing/exhausted script RETURNS a Raise spec
(recorded before raising) so the trace matches the C dispatcher. BaseException
caught per case -> __raise__ spec, st={}. Every `except Exception` site passes
the BaseException trio (+CancelledError/BaseException/BaseExceptionGroup)
through; the one `except httpx.HTTPStatusError` site matches exactly that kind.
Summary: 'ORACLE usage_tracking_flow: N cases, M mismatches'.

Datetime model: a datetime travels as INTEGER epoch-MICROSECONDS (UTC). The
oracle materializes it to a real datetime (exact integer arithmetic) and
projects it back via calendar.timegm+microsecond; the C reads the integer.
Domain (documented in the port header): ASCII inputs; used_bp/resets_at_ms/
available_count INT-typed (non-integral floats exercise the isinstance-int
rejection); snapshot/gauge/credits_state scripts null or NON-EMPTY carriers;
_get_json/payload dict-or-raise; HTTPStatusError carries status_code;
resolve_codex_usage_credentials a 3-array; codex_principal_identity null or a
2-array; {body!r} repr ASCII str/int/bool/null/nested (no floats).
"""
import ast
import builtins
import calendar
import copy
import dataclasses
import enum
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
GAUGES_SRC = str(QUARRY / "agent" / "usage_gauges.py")

ANN_FLAG = __future__.annotations.compiler_flag

# live target names, grouped by defining module
AU_FUNCS = [
    "_is_num", "_codex_banked_resets", "_codex_window_labels", "_codex_headers",
    "_codex_snapshot_identity", "_unavailable", "_codex_reset_guard",
    "_codex_reset_outcome", "_snapshot", "_usage_windows", "account_limit_lines",
    "_reset_phrase", "_render_usage_window_lines", "_call_plugin_usage_hook",
    "_fetch_codex_account_usage_impl", "_fetch_codex_account_usage_read_only",
]
NOUS_FUNCS = [
    "_header_gauge_percent", "_bp_window", "_window_top_tier_line",
    "_nous_snapshot", "_usage_window_rows", "apply_header_gauge",
    "_nous_logged_in", "_nous_account_view", "nous_credits_view",
    "nous_credits_usage_fields", "nous_usage_lines", "nous_usage_view",
]
FUNCS = AU_FUNCS + NOUS_FUNCS

# claimed bare-global siblings (live name -> seam name)
SEAM_GLOBALS = {
    "_utc_now": "utc_now",
    "_parse_dt": "parse_dt",
    "_format_reset": "format_reset",
    "_title_case_slug": "title_case_slug",
    "_plural": "plural",
    "_get_json": "get_json",
    "_codex_backend_urls": "codex_backend_urls",
    "_resolve_codex_usage_credentials": "resolve_codex_usage_credentials",
    "render_account_usage_lines": "render_account_usage_lines",
    "fetch_account_usage": "fetch_account_usage",
    "_snapshot_from_credits_state": "snapshot_from_credits_state",
    "build_nous_credits_snapshot": "build_nous_credits_snapshot",
    "nous_credits_lines": "nous_credits_lines",
    "_fetch_portal_account": "fetch_portal_account",
}
# in-function imports: module -> {attr: seam name | None=real | "REAL:<fn>"=ported}
SEAM_IMPORTS = {
    "hermes_cli.model_picker_usage": {"format_resets_in": "format_resets_in",
                                      "format_percent_used": "format_percent_used",
                                      "render_bar": "render_bar"},
    "agent.deadline": {"run_bounded_sync": "run_bounded_sync"},
    "providers.base": {"ProviderProfile": None},
    "agent.codex_headers": {"codex_account_headers": "codex_account_headers"},
    "agent.credential_pool": {"_codex_principal_identity": "codex_principal_identity"},
    "hermes_cli.auth": {"clear_codex_pool_quota_cooldowns": "clear_codex_pool_quota_cooldowns",
                        "get_provider_auth_state": "get_provider_auth_state"},
    "agent.account_usage": {"_snapshot": "REAL:_snapshot",
                            "AccountUsageWindow": None,
                            "render_account_usage_lines": "render_account_usage_lines"},
    "agent.usage_gauges": {"SCOPE_TOP_TIER": None, "all_scope": "all_scope",
                           "gauge_title": "gauge_title", "parse_gauges": "parse_gauges",
                           "paused_for_payment": "paused_for_payment"},
    "agent.credits_tracker": {"dev_fixture_credits_state": "dev_fixture_credits_state"},
    "agent.billing_usage": {"_dev_fixture_usage_model": "dev_fixture_usage_model",
                            "usage_model_from_account": "usage_model_from_account"},
}

CURRENT = {"rec": None}
_MISSING = object()


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


# full builtin exception map (probed on quarry CPython 3.14.9)
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
        import asyncio
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


# ---- datetime model (epoch-microseconds, UTC) ----
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
    if isinstance(v, enum.Enum):
        return v.value
    return v


def norm_args(v):
    v = norm_value(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    if isinstance(v, list):
        return [norm_args(x) for x in v]
    return v


def proj(name, a, k):
    if name in ("utc_now", "clear_codex_pool_quota_cooldowns", "dev_fixture_credits_state",
                "dev_fixture_usage_model", "time.time"):
        return {}
    if name == "parse_dt": return {"value": a[0]}
    if name == "format_reset": return {"dt": a[0]}
    if name == "title_case_slug": return {"value": a[0]}
    if name == "plural": return {"count": a[0]}
    if name == "get_json": return {"url": a[0], "headers": a[1], "timeout": k.get("timeout")}
    if name == "codex_backend_urls": return {"base_url": a[0]}
    if name == "resolve_codex_usage_credentials":
        d = {"base_url": a[0], "api_key": a[1]}
        if "force_refresh" in k: d["force_refresh"] = k["force_refresh"]
        return d
    if name == "render_account_usage_lines": return {"snapshot": a[0], "markdown": k.get("markdown")}
    if name == "fetch_account_usage":
        return {"provider": a[0], "base_url": k.get("base_url"), "api_key": k.get("api_key")}
    if name == "snapshot_from_credits_state": return {"state": a[0]}
    if name == "build_nous_credits_snapshot": return {"account": a[0], "cta": k.get("cta")}
    if name == "nous_credits_lines":
        return {"markdown": k.get("markdown"), "credits_state": k.get("credits_state"),
                "nous_route": k.get("nous_route")}
    if name == "fetch_portal_account": return {"timeout": a[0], "stored_token_only": k.get("stored_token_only")}
    if name == "usage_model_from_account": return {"account": a[0]}
    if name == "codex_account_headers": return {"token": a[0]}
    if name == "codex_principal_identity": return {"token": a[0]}
    if name == "get_provider_auth_state": return {"provider": a[0]}
    if name == "format_resets_in": return {"ms": a[0]}
    if name == "render_bar": return {"used": a[0]}
    if name == "format_percent_used": return {"used": a[0]}
    if name == "run_bounded_sync":
        return {"fn": {"__callable__": "profile.fetch_account_usage"},
                "timeout": a[1], "label": k.get("label")}
    if name == "parse_gauges": return {"gauges": a[0]}
    if name == "all_scope": return {"gauges": list(a[0])}
    if name == "gauge_title": return {"gauge": a[0]}
    if name == "paused_for_payment": return {"reason": a[0]}
    if name.startswith("logger."):
        d = {"fmt": a[0], "args": list(a[1:])}
        if "exc_info" in k: d["exc_info"] = k["exc_info"]
        return d
    return {**{f"arg{i}": v for i, v in enumerate(a)}, **k}


# ---- materialization of seam results / inputs into live objects ----
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


def mat_gauge(g):
    if not isinstance(g, dict):
        return g
    return NS["WindowGauge"](id=g["id"], scope=g["scope"], label=g["label"],
                             used_bp=g["used_bp"], resets_at_ms=g.get("resets_at_ms"))


def mat_credits_state(cs):
    if not isinstance(cs, dict):
        return cs
    d = dict(cs)
    if isinstance(d.get("gauges"), list):
        d["gauges"] = tuple(mat_gauge(g) for g in d["gauges"])
    if isinstance(d.get("top_tier_gauge"), dict):
        d["top_tier_gauge"] = mat_gauge(d["top_tier_gauge"])
    return SimpleNamespace(**d)


def mat_agent(a):
    if not isinstance(a, dict):
        return a
    d = dict(a)
    if "_credits_state" in d:
        d["_credits_state"] = mat_credits_state(d["_credits_state"])
    return SimpleNamespace(**d)


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


def materialize(name, raw):
    if name in ("utc_now", "parse_dt"):
        return us_to_dt(raw)
    if name in ("snapshot_from_credits_state", "build_nous_credits_snapshot"):
        return mat_snapshot(raw)
    if name == "parse_gauges":
        if isinstance(raw, list):
            return tuple(mat_gauge(g) for g in raw)
        return raw
    if name == "all_scope":
        if isinstance(raw, list):
            return tuple(mat_gauge(g) for g in raw)
        return raw
    if name == "run_bounded_sync":
        if isinstance(raw, dict):
            return SimpleNamespace(**raw)
        return raw
    return raw


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


class RecLogger:
    def __getattr__(self, name):
        if name in ("info", "warning", "debug", "error"):
            def f(fmt, *args, **kw):
                return CURRENT["rec"]("logger." + name)(fmt, *args, **kw)
            return f
        raise AttributeError(name)


# ---- live namespace seeding ----
NS = {}
REAL = {}


def _exec_seg(src, node, ns, filename):
    seg = ast.get_source_segment(src, node)
    seg = textwrap.dedent(seg)
    code = compile(seg, filename, "exec", flags=ANN_FLAG, dont_inherit=True)
    exec(code, ns)


def _exec_class(src, node, ns, filename):
    # ast.get_source_segment EXCLUDES decorators; rebuild from the first
    # decorator line so @dataclass actually generates __init__.
    lines = src.splitlines()
    start = (node.decorator_list[0].lineno - 1) if node.decorator_list else (node.lineno - 1)
    seg = textwrap.dedent("\n".join(lines[start:node.end_lineno]))
    code = compile(seg, filename, "exec", flags=ANN_FLAG, dont_inherit=True)
    exec(code, ns)


def load_ns():
    global NS, REAL
    mod = types.ModuleType("ut_live")
    sys.modules["ut_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "ut_live", "math": math, "json": json, "enum": enum,
        "dataclass": dataclasses.dataclass, "field": dataclasses.field,
        "replace": dataclasses.replace,
        "datetime": datetime, "UTC": UTC,
        "time": SimpleNamespace(time=_stub_fn("time.time")),
        "logging": SimpleNamespace(getLogger=lambda name: RecLogger()),
        "logger": RecLogger(),
        "Any": object, "Optional": object, "Callable": object,
        "TypeGuard": object, "Iterable": object,
        "httpx": SimpleNamespace(HTTPStatusError=_HTTPStatusError),
        "BP_FULL": 10000, "SCOPE_ALL": "all", "SCOPE_TOP_TIER": "top_tier",
    })
    au_src = Path(AU_SRC).read_text(encoding="utf-8")
    nous_src = Path(NOUS_SRC).read_text(encoding="utf-8")
    gauges_src = Path(GAUGES_SRC).read_text(encoding="utf-8")
    au_tree = ast.parse(au_src)
    nous_tree = ast.parse(nous_src)
    gauges_tree = ast.parse(gauges_src)
    # dataclass types FIRST (constants / defs construct them)
    for node in gauges_tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "WindowGauge":
            _exec_class(gauges_src, node, ns, GAUGES_SRC)
    for node in au_tree.body:
        if isinstance(node, ast.ClassDef) and node.name in (
                "AccountUsageWindow", "AccountUsageSnapshot", "CodexResetRedeemResult"):
            _exec_class(au_src, node, ns, AU_SRC)
    for nm in ("WindowGauge", "AccountUsageWindow", "AccountUsageSnapshot", "CodexResetRedeemResult"):
        if nm not in ns:
            raise RuntimeError("seed missing class: " + nm)
    # module constants (plain-Name targets only)
    want_const = {
        "_CODEX_WINDOW_LABELS_BY_SECONDS", "_CODEX_WINDOW_POSITIONAL_LABELS",
        "_CODEX_WINDOW_EXHAUSTED_PERCENT", "PLUGIN_USAGE_HOOK_DEADLINE_S",
        "_WINDOW_TOP_TIER_USED_UP", "_WINDOW_PAUSED_LINE", "_WINDOW_PAUSED_OTHER_LINE",
        "_WINDOW_KEEP_GOING_OFF_STATUS", "_WINDOW_KEEP_GOING_OFF_LINE",
        "_WINDOW_KEEP_GOING_ON_LINE", "NOUS_OTHER_ROUTE_HEADING",
    }
    for tree, src, fn in ((au_tree, au_src, AU_SRC), (nous_tree, nous_src, NOUS_SRC)):
        for node in tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
                names = {t.id for t in tgts if isinstance(t, ast.Name)}
                if names & want_const:
                    _exec_seg(src, node, ns, fn)
    # target defs
    found = set()
    for node in au_tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in AU_FUNCS:
            _exec_seg(au_src, node, ns, AU_SRC)
            found.add(node.name)
    for node in nous_tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in NOUS_FUNCS:
            _exec_seg(nous_src, node, ns, NOUS_SRC)
            found.add(node.name)
    missing = set(FUNCS) - found
    if missing:
        raise RuntimeError("missing target defs: %s" % sorted(missing))
    # claimed bare-global siblings seamed
    for live, seam in SEAM_GLOBALS.items():
        ns[live] = _stub_fn(seam)
    # in-function import stubs (OVERWRITE parent packages)
    for pkg in ("agent", "hermes_cli", "providers", "tools"):
        p = types.ModuleType(pkg); p.__path__ = []
        sys.modules[pkg] = p
    for modname, attrs in SEAM_IMPORTS.items():
        m = types.ModuleType(modname)
        for attr, seam in attrs.items():
            if seam is None:
                continue
            if isinstance(seam, str) and seam.startswith("REAL:"):
                setattr(m, attr, ns[seam[5:]])
            else:
                setattr(m, attr, _stub_fn(seam))
        sys.modules[modname] = m
        parts = modname.split(".")
        if len(parts) == 2:
            setattr(sys.modules[parts[0]], parts[1], m)
    # real constants / classes into their stub modules
    sys.modules["agent.usage_gauges"].SCOPE_TOP_TIER = "top_tier"
    sys.modules["agent.account_usage"].AccountUsageWindow = ns["AccountUsageWindow"]

    # ProviderProfile base no-op hook (the identity check compares against it)
    class ProviderProfile:
        def fetch_account_usage(self, base_url=None, api_key=None):
            return None
    ns["ProviderProfile"] = ProviderProfile
    sys.modules["providers.base"].ProviderProfile = ProviderProfile
    # audit: every name the drivers read is bound
    for nm in FUNCS + list(SEAM_GLOBALS) + sorted(want_const):
        if nm not in ns:
            raise RuntimeError("seed missing: " + nm)
    REAL = {nm: ns[nm] for nm in FUNCS}


# ---- per-case drive ----
def drive(func, inp):
    g = inp.get
    if func == "_is_num":
        return NS[func](g("v")), {}
    if func == "_codex_banked_resets":
        return NS[func](g("payload")), {}
    if func == "_codex_window_labels":
        return NS[func](g("rate_limit")), {}
    if func == "_codex_headers":
        return NS[func](g("token"), g("account_id")), {}
    if func == "_codex_snapshot_identity":
        return NS[func](g("token")), {}
    if func == "_unavailable":
        return NS[func](g("message")), {}
    if func == "_codex_reset_guard":
        return NS[func](g("payload"), g("available"), g("force")), {}
    if func == "_codex_reset_outcome":
        return NS[func](g("body"), g("available")), {}
    if func == "_snapshot":
        kw = g("kw") or {}
        windows = [mat_window(w) for w in (g("windows") or [])]
        return NS[func](g("provider"), g("source"), windows, g("details") or [], **kw), {}
    if func == "_usage_windows":
        mapping = [tuple(p) for p in (g("mapping") or [])]
        ms = frozenset(g("model_scoped") or [])
        return NS[func](g("source"), mapping, g("used_key"), g("reset_key"),
                        fraction=g("fraction"), model_scoped=ms), {}
    if func == "account_limit_lines":
        return NS[func](g("provider"), base_url=g("base_url"), api_key=g("api_key"),
                        markdown=g("markdown")), {}
    if func == "_reset_phrase":
        return NS[func](us_to_dt(g("dt"))), {}
    if func == "_render_usage_window_lines":
        return NS[func](mat_snapshot(g("snapshot"))), {}
    if func == "_call_plugin_usage_hook":
        return NS[func](mat_profile(g("profile")), g("base_url"), g("api_key")), {}
    if func in ("_fetch_codex_account_usage_impl", "_fetch_codex_account_usage_read_only"):
        if func == "_fetch_codex_account_usage_read_only":
            return NS[func](g("base_url"), g("api_key")), {}
        return NS[func](g("base_url"), g("api_key"), read_only=g("read_only")), {}
    if func == "_header_gauge_percent":
        return NS[func](g("bp")), {}
    if func == "_bp_window":
        return NS[func](g("label"), g("used_bp"), g("resets_at_ms")), {}
    if func == "_window_top_tier_line":
        return NS[func](us_to_dt(g("reset_at")), g("keep_going_on_credits")), {}
    if func == "_nous_snapshot":
        windows = [mat_window(w) for w in (g("windows") or [])]
        return NS[func](windows, g("details") or [], g("tail") or [],
                        source=g("source"), plan=g("plan"), usage_window=g("usage_window")), {}
    if func == "_usage_window_rows":
        return NS[func](g("gauges"), paused_reason=g("paused_reason"),
                        keep_going_on_credits=g("keep_going_on_credits"), cta=g("cta")), {}
    if func == "apply_header_gauge":
        return NS[func](mat_snapshot(g("snapshot")), mat_credits_state(g("credits_state"))), {}
    if func == "_nous_logged_in":
        return NS[func](), {}
    if func in ("_nous_account_view", "nous_credits_view"):
        return NS[func](markdown=g("markdown"), timeout=g("timeout"),
                        credits_state=mat_credits_state(g("credits_state")),
                        nous_route=g("nous_route")), {}
    if func == "nous_credits_usage_fields":
        return NS[func](mat_agent(g("agent")), nous_route=g("nous_route")), {}
    if func == "nous_usage_lines":
        return NS[func](nous_route=g("nous_route"), markdown=g("markdown"),
                        credits_state=g("credits_state")), {}
    if func == "nous_usage_view":
        return NS[func](nous_route=g("nous_route"), credits_state=mat_credits_state(g("credits_state"))), {}
    raise RuntimeError("unknown func " + func)


def run_case(func, inp):
    for nm, fn in REAL.items():
        NS[nm] = fn
    ctx = CaseCtx(inp)
    CURRENT["rec"] = ctx.recorder
    try:
        ret, st = drive(func, inp)
        return {"ret": norm_value(ret), "trace": ctx.trace, "st": norm_value(st)}
    except BaseException as e:
        spec = None
        for r in ctx.script.handed:
            if e is r.exc:
                spec = r.spec
                break
        if spec is None:
            spec = {"__raise__": type(e).__name__, "message": str(e)}
        return {"ret": spec, "trace": ctx.trace, "st": {}}


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
    print("ORACLE usage_tracking_flow: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()