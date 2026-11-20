#!/usr/bin/env python3
"""Oracle for the FLOW port of the UNCLAIMED remainder of
agent/account_usage_nous.py (src/agent/account_usage_nous_flow2.c, aun2_*).

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"] with scripted universal-seam stubs, and
compares the ordered trace, the return, and the state projection ({} -- every
ported function is pure). Live defs are AST-extracted (py3.14 quarry venv).

The 6 module defs whose only PoPs mis-credit the facade (_fmt_usd,
_is_finite_num, build_nous_credits_snapshot, nous_credits_lines,
_snapshot_from_credits_state, build_credits_view) stay REAL; the frozen
dataclass CreditsView surface (__init__/__repr__/__eq__/__setattr__/__delattr__)
is the generated remainder. Claimed siblings (_usage_window_rows, _nous_snapshot,
_nous_logged_in, _fetch_portal_account, nous_credits_view -- ut_*/au2_*) plus the
facade render_account_usage_lines and foreign nous_portal_topup_url /
format_renews are recording seams fed the SAME scripts the C harness feeds.
Ported siblings compose REAL (build_credits_view -> build_nous_credits_snapshot
-> _is_finite_num + _fmt_usd; -> CreditsView.__init__).

Fail-closed: 0 cases => exit 1; a missing/exhausted script RETURNS a Raise spec
(recorded before raising) so the trace matches the C dispatcher. BaseException
caught per case -> __raise__ spec, st={}. Every `except Exception` /
`except (AttributeError,TypeError)` site passes the BaseException trio
(+CancelledError/BaseException/BaseExceptionGroup) through.
Summary: 'ORACLE account_usage_nous_flow2: N cases, M mismatches'.

Carrier models (documented in the port header): account/access/sub/uw/state are
recursive types.SimpleNamespace doubles; AccountUsageWindow/Snapshot carriers
carry the FULL dataclass field set (fetched_at/reset_at epoch-us int or null);
CreditsView balance_lines is tuple-typed for __repr__/__eq__ (array->tuple on
both sides), __init__ stores present values verbatim. __eq__ projects the
NotImplemented singleton to the string "NotImplemented". NaN/Inf inputs ride a
{"__special__":"nan"|"inf"|"-inf"} marker. Domain: ASCII; str()-computed
positions str-or-null; carriers null or non-empty objects.
"""
import ast
import builtins
import calendar
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
if not (QUARRY / "agent" / "account_usage_nous.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))

AU_SRC = str(QUARRY / "agent" / "account_usage.py")
NOUS_SRC = str(QUARRY / "agent" / "account_usage_nous.py")

ANN_FLAG = __future__.annotations.compiler_flag

FUNCS = [
    "_fmt_usd", "_is_finite_num", "build_nous_credits_snapshot",
    "nous_credits_lines", "_snapshot_from_credits_state", "build_credits_view",
]
CV_METHODS = ["__init__", "__repr__", "__eq__", "__setattr__", "__delattr__"]

# claimed bare-global siblings (live name -> seam name)
SEAM_GLOBALS = {
    "_usage_window_rows": "usage_window_rows",
    "_nous_snapshot": "nous_snapshot",
    "_nous_logged_in": "nous_logged_in",
    "_fetch_portal_account": "fetch_portal_account",
    "nous_credits_view": "nous_credits_view",
}
# in-function imports: module -> {attr: seam name | None=real}
SEAM_IMPORTS = {
    "hermes_cli.nous_account": {"nous_portal_topup_url": "nous_portal_topup_url"},
    "agent.account_usage": {"AccountUsageWindow": None,
                            "render_account_usage_lines": "render_account_usage_lines"},
    "agent.billing_usage": {"format_renews": "format_renews"},
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


# full builtin exception map (probed on quarry CPython 3.14.9)
_BUILTIN_EXC = {}
for _n in dir(builtins):
    _c = getattr(builtins, _n)
    if isinstance(_c, type) and issubclass(_c, BaseException):
        _BUILTIN_EXC[_n] = _c


def build_exc(spec):
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
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
    return v


def norm_args(v):
    v = norm_value(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    if isinstance(v, list):
        return [norm_args(x) for x in v]
    return v


def proj(name, a, k):
    if name == "usage_window_rows":
        return {"gauges": a[0], "paused_reason": k.get("paused_reason"),
                "keep_going_on_credits": k.get("keep_going_on_credits"), "cta": k.get("cta")}
    if name == "nous_snapshot":
        return {"windows": a[0], "details": a[1], "tail": a[2], "source": k.get("source"),
                "plan": k.get("plan"), "usage_window": k.get("usage_window")}
    if name == "nous_portal_topup_url":
        return {"account_info": a[0]}
    if name == "format_renews":
        return {"value": a[0]}
    if name == "render_account_usage_lines":
        return {"snapshot": a[0], "markdown": k.get("markdown")}
    if name == "fetch_portal_account":
        return {"timeout": a[0], "stored_token_only": k.get("stored_token_only")}
    if name == "nous_logged_in":
        return {}
    if name == "nous_credits_view":
        return {"markdown": k.get("markdown"), "timeout": k.get("timeout"),
                "credits_state": k.get("credits_state"), "nous_route": k.get("nous_route")}
    if name.startswith("logger."):
        d = {"fmt": a[0], "args": list(a[1:])}
        if "exc_info" in k:
            d["exc_info"] = k["exc_info"]
        return d
    return {**{f"arg{i}": v for i, v in enumerate(a)}, **k}


# ---- materialization of seam results / inputs into live objects ----
def mat_ns(v):
    """recursive attribute double: dict -> SimpleNamespace, list element-wise."""
    if isinstance(v, dict):
        return SimpleNamespace(**{k: mat_ns(x) for k, x in v.items()})
    if isinstance(v, list):
        return [mat_ns(x) for x in v]
    return v


def mat_special(v):
    if isinstance(v, dict) and "__special__" in v:
        return {"nan": float("nan"), "inf": float("inf"), "-inf": float("-inf")}[v["__special__"]]
    return v


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


def mat_cv(d):
    if not isinstance(d, dict):
        return d
    return NS["CreditsView"](
        logged_in=d.get("logged_in"),
        balance_lines=tuple(d.get("balance_lines", [])),
        identity_line=d.get("identity_line"),
        topup_url=d.get("topup_url"),
        depleted=d.get("depleted", False))


def mat_other(o):
    if isinstance(o, dict) and "__credits_view__" in o:
        return mat_cv(o["__credits_view__"])
    return o


def materialize(name, raw):
    if name == "nous_snapshot":
        return mat_snapshot(raw)
    if name == "fetch_portal_account":
        return mat_ns(raw)
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
    lines = src.splitlines()
    start = (node.decorator_list[0].lineno - 1) if node.decorator_list else (node.lineno - 1)
    seg = textwrap.dedent("\n".join(lines[start:node.end_lineno]))
    code = compile(seg, filename, "exec", flags=ANN_FLAG, dont_inherit=True)
    exec(code, ns)


def load_ns():
    global NS, REAL
    mod = types.ModuleType("aun2_live")
    sys.modules["aun2_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "aun2_live", "math": math, "json": json,
        "dataclass": dataclasses.dataclass, "field": dataclasses.field,
        "replace": dataclasses.replace,
        "datetime": datetime, "UTC": UTC,
        "logging": SimpleNamespace(getLogger=lambda name: RecLogger()),
        "logger": RecLogger(),
        "Any": object, "Optional": object, "Callable": object, "TypeGuard": object,
        "BP_FULL": 10000,
    })
    au_src = Path(AU_SRC).read_text(encoding="utf-8")
    nous_src = Path(NOUS_SRC).read_text(encoding="utf-8")
    au_tree = ast.parse(au_src)
    nous_tree = ast.parse(nous_src)
    # dataclass types FIRST (defs / materialization construct them)
    for node in au_tree.body:
        if isinstance(node, ast.ClassDef) and node.name in ("AccountUsageWindow", "AccountUsageSnapshot"):
            _exec_class(au_src, node, ns, AU_SRC)
    for node in nous_tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "CreditsView":
            _exec_class(nous_src, node, ns, NOUS_SRC)
    for nm in ("AccountUsageWindow", "AccountUsageSnapshot", "CreditsView"):
        if nm not in ns:
            raise RuntimeError("seed missing class: " + nm)
    # module constants (plain-Name string-literal targets only)
    want_const = {
        "_DEPLETED_LINE", "_WINDOW_TOP_TIER_USED_UP", "_WINDOW_PAUSED_LINE",
        "_WINDOW_PAUSED_OTHER_LINE", "_WINDOW_KEEP_GOING_OFF_STATUS",
        "_WINDOW_KEEP_GOING_OFF_LINE", "_WINDOW_KEEP_GOING_ON_LINE",
        "NOUS_OTHER_ROUTE_HEADING",
    }
    for node in nous_tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            names = {t.id for t in tgts if isinstance(t, ast.Name)}
            if names & want_const:
                _exec_seg(nous_src, node, ns, NOUS_SRC)
    # target defs
    found = set()
    for node in nous_tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in FUNCS:
            _exec_seg(nous_src, node, ns, NOUS_SRC)
            found.add(node.name)
    missing = set(FUNCS) - found
    if missing:
        raise RuntimeError("missing target defs: %s" % sorted(missing))
    # claimed bare-global siblings seamed
    for live, seam in SEAM_GLOBALS.items():
        ns[live] = _stub_fn(seam)
    # in-function import stubs (OVERWRITE parent packages)
    for pkg in ("agent", "hermes_cli"):
        p = types.ModuleType(pkg); p.__path__ = []
        sys.modules[pkg] = p
    for modname, attrs in SEAM_IMPORTS.items():
        m = types.ModuleType(modname)
        for attr, seam in attrs.items():
            if seam is None:
                continue
            setattr(m, attr, _stub_fn(seam))
        sys.modules[modname] = m
        parts = modname.split(".")
        if len(parts) == 2:
            setattr(sys.modules[parts[0]], parts[1], m)
    # real class into its stub module
    sys.modules["agent.account_usage"].AccountUsageWindow = ns["AccountUsageWindow"]
    # logger must stay the recorder (the module Assign would clobber it)
    ns["logger"] = RecLogger()
    # audit: every name the drivers read is bound
    for nm in FUNCS + list(SEAM_GLOBALS) + sorted(want_const) + ["CreditsView"]:
        if nm not in ns:
            raise RuntimeError("seed missing: " + nm)
    REAL = {nm: ns[nm] for nm in FUNCS}


# ---- per-case drive ----
def drive(func, inp):
    g = inp.get
    if func == "_fmt_usd":
        return NS[func](mat_special(g("d"))), {}
    if func == "_is_finite_num":
        return NS[func](mat_special(g("v"))), {}
    if func == "build_nous_credits_snapshot":
        return NS[func](mat_ns(g("account")), cta=g("cta")), {}
    if func == "nous_credits_lines":
        return NS[func](markdown=g("markdown"), timeout=g("timeout"),
                        credits_state=g("credits_state"), nous_route=g("nous_route")), {}
    if func == "_snapshot_from_credits_state":
        return NS[func](mat_ns(g("state"))), {}
    if func == "build_credits_view":
        return NS[func](markdown=g("markdown"), timeout=g("timeout")), {}
    if func.startswith("CreditsView."):
        m = func[len("CreditsView."):]
        if m == "__init__":
            return NS["CreditsView"](**(g("kw") or {})), {}
        if m == "__repr__":
            return repr(mat_cv(g("self"))), {}
        if m == "__eq__":
            r = mat_cv(g("self")).__eq__(mat_other(g("other")))
            return ("NotImplemented" if r is NotImplemented else r), {}
        if m == "__setattr__":
            mat_cv(g("self")).__setattr__(g("name"), g("value"))
            return None, {}
        if m == "__delattr__":
            mat_cv(g("self")).__delattr__(g("name"))
            return None, {}
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
    print("ORACLE account_usage_nous_flow2: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()