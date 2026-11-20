#!/usr/bin/env python3
"""Oracle for the FLOW port of the UNCLAIMED remainder of agent/usage_pricing.py
(src/agent/usage_pricing_flow.c, up2_* entries).

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"] with scripted universal-seam stubs, and
compares the ordered trace, the return, and the state projection. Live defs are
AST-extracted (py3.14 quarry venv): the 13 unclaimed top-level defs plus the 3
nested closures host (inside resolve_billing_route), rate / per_million (inside
_pricing_entry_from_metadata). The whole module body (minus imports) is exec'd
so the constant tables (_OFFICIAL_DOCS_PRICING / _OPENAI_ULTRAFAST_PRICING /
_ANTHROPIC_FAST_MODE_PRICING / _MODELS_DEV_DIRECT_HOSTS / _CACHE_WRITE_TIER_PATHS)
are built by the REAL _snap. Claimed siblings (_to_decimal,
_normalize_anthropic_model_name) and all foreign I/O are recording seams fed the
SAME scripts the C harness feeds. Ported siblings stay REAL so composition is
exercised (_usage_field / rate).

Fail-closed: 0 cases => exit 1; a missing/exhausted script RETURNS a Raise spec
(recorded before raising) so the trace matches the C dispatcher. BaseException
caught per case -> __raise__ spec. _usage_field's `except Exception` int-coercion
guard passes the BaseException trio through (FULL builtin hierarchy in C).
rate / per_million echo {"rejected":[sorted]} on success AND raise.
Summary: 'ORACLE usage_pricing_flow: N cases, M mismatches'.

Decimal model: {"__dec__": str(Decimal)} (exact text). datetime: integer epoch-us
(UTC). Carriers carry the full dataclass field set; the oracle materializes them
to real dataclasses and projects back. ASCII inputs; integral floats excluded
from str()-computed positions.
"""
import ast
import builtins
import calendar
import copy
import dataclasses
import json
import math
import os
import sys
import types
import textwrap
import typing
import __future__
from datetime import datetime, UTC, timezone
from decimal import Decimal
from pathlib import Path
from types import SimpleNamespace

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "usage_pricing.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))

SRC = str(QUARRY / "agent" / "usage_pricing.py")
RE_SRC = str(QUARRY / "agent" / "reasoning_effort.py")
ANN_FLAG = __future__.annotations.compiler_flag

FUNCS = [
    "_snap", "_usage_field", "_first_nonzero", "_strip_prefix",
    "with_served_service_tier", "_served_openai_tier", "_openai_served_tier_entry",
    "_served_fast", "_anthropic_fast_mode_entry", "_models_dev_pricing_entry",
    "cache_write_tiers", "_cache_write_buckets", "_unknown_cost",
    "host", "rate", "per_million",
]
STATEFUL = {"rate", "per_million"}
# claimed siblings seamed (live name -> seam name)
SEAMED_SIBLINGS = {"_to_decimal": "to_decimal",
                   "_normalize_anthropic_model_name": "normalize_anthropic_model_name"}
SEAM_NAMES = {"to_decimal", "normalize_anthropic_model_name", "base_url_host_matches",
              "base_url_origin", "get_model_info", "utc_now", "logger.debug"}

CURRENT = {"rec": None, "ctx": None}
_MISSING = object()
NS = {}
REAL = {}


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


_BUILTIN_EXC = {}
for _n in dir(builtins):
    _c = getattr(builtins, _n)
    if isinstance(_c, type) and issubclass(_c, BaseException):
        _BUILTIN_EXC[_n] = _c


def build_exc(spec):
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    if kind in _BUILTIN_EXC:
        cls = _BUILTIN_EXC[kind]
        if kind in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
            e = cls.__new__(cls)
            Exception.__init__(e, msg)
            return e
        try:
            return cls(msg)
        except Exception:
            e = cls.__new__(cls)
            Exception.__init__(e, msg)
            return e
    # synthetic Exception subclass (InvalidOperation, custom kinds, ...)
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


# ---- datetime model (epoch microseconds, UTC) ----
def us_to_dt(us):
    if us is None:
        return None
    us = int(us)
    return datetime.fromtimestamp(us // 1_000_000, tz=UTC).replace(microsecond=us % 1_000_000)


def dt_to_us(dt):
    if dt is None:
        return None
    return calendar.timegm(dt.utctimetuple()) * 1_000_000 + dt.microsecond


# ---- Decimal projection / materialization ----
def dec_norm(d):
    return {"__dec__": str(d)}


def mat_dec(v):
    if isinstance(v, dict) and "__dec__" in v:
        return Decimal(v["__dec__"])
    return v


def norm_value(v):
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, Decimal):
        return dec_norm(v)
    if isinstance(v, datetime):
        return dt_to_us(v)
    if isinstance(v, ModelInfoDouble):
        return norm_value(v._d)
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
    if name == "to_decimal":
        return {"value": a[0] if a else k.get("value")}
    if name == "normalize_anthropic_model_name":
        return {"model": a[0] if a else k.get("model")}
    if name == "base_url_host_matches":
        return {"url": a[0], "name": a[1]}
    if name == "base_url_origin":
        return {"base_url": a[0]}
    if name == "get_model_info":
        return {"provider": a[0], "model": a[1]}
    if name == "utc_now":
        return {}
    return {**{f"arg{i}": v for i, v in enumerate(a)}, **k}


# ---- carrier doubles ----
class ModelInfoDouble:
    def __init__(self, d):
        self._d = d if isinstance(d, dict) else {}

    def has_cost_data(self):
        return bool(self._d.get("has_cost_data", False))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("'ModelInfoDouble' object has no attribute '%s'" % name)


class LoggerDouble:
    def _log(self, level, msg, args, kw):
        d = {"msg": msg, "args": list(args)}
        if kw.get("exc_info"):
            d["exc_info"] = True
        return CURRENT["rec"]("logger." + level)(**d)

    def debug(self, msg, *args, **kw):
        return self._log("debug", msg, args, kw)

    def info(self, msg, *args, **kw):
        return self._log("info", msg, args, kw)

    def warning(self, msg, *args, **kw):
        return self._log("warning", msg, args, kw)

    def error(self, msg, *args, **kw):
        return self._log("error", msg, args, kw)


def materialize(name, raw):
    if name == "to_decimal":
        return mat_dec(raw)
    if name == "utc_now":
        return us_to_dt(raw)
    if name == "get_model_info":
        return ModelInfoDouble(raw) if isinstance(raw, dict) else raw
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


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **k):
        return CURRENT["rec"](self.name)(*a, **k)


# ---- live namespace seeding ----
def _exec_seg(src, node, ns, filename):
    seg = textwrap.dedent(ast.get_source_segment(src, node))
    exec(compile(seg, filename, "exec", flags=ANN_FLAG, dont_inherit=True), ns)


def _exec_class(src, node, ns, filename):
    lines = src.splitlines()
    start = (node.decorator_list[0].lineno - 1) if node.decorator_list else (node.lineno - 1)
    seg = textwrap.dedent("\n".join(lines[start:node.end_lineno]))
    exec(compile(seg, filename, "exec", flags=ANN_FLAG, dont_inherit=True), ns)


def _gpt62_ids():
    src = Path(RE_SRC).read_text(encoding="utf-8")
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if any(isinstance(t, ast.Name) and t.id == "GPT62_MODEL_IDS" for t in tgts):
                ns = {}
                exec(compile(ast.get_source_segment(src, node), RE_SRC, "exec"), ns)
                return ns["GPT62_MODEL_IDS"]
    return frozenset({"gpt-6.2-astra", "gpt-6.2-sol"})


def load_ns():
    global NS, REAL
    mod = types.ModuleType("up2_live")
    sys.modules["up2_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns["__name__"] = "up2_live"
    ns.update({
        "Decimal": Decimal, "dataclass": dataclasses.dataclass,
        "fields": dataclasses.fields, "replace": dataclasses.replace,
        "datetime": datetime, "UTC": UTC, "timezone": timezone,
        "Any": typing.Any, "Dict": typing.Dict, "Literal": typing.Literal,
        "Optional": typing.Optional,
        "re": __import__("re"),
        "logging": SimpleNamespace(getLogger=lambda n: SimpleNamespace(
            debug=lambda *a, **k: None, info=lambda *a, **k: None)),
        "GPT62_MODEL_IDS": _gpt62_ids(),
        "base_url_host_matches": lambda *a, **k: False,
        "base_url_hostname": lambda *a, **k: "",
        "base_url_origin": lambda *a, **k: ("https", "", 443),
        "fetch_endpoint_model_metadata": lambda *a, **k: {},
        "fetch_model_metadata": lambda *a, **k: {},
        "strip_codex_context_variant_suffix": lambda s: s,
    })
    src = Path(SRC).read_text(encoding="utf-8")
    tree = ast.parse(src)
    # exec the whole module body (minus imports) in source order: dataclasses,
    # constants, the table-building loops (real _snap), and every FunctionDef.
    for node in tree.body:
        if isinstance(node, (ast.Import, ast.ImportFrom)):
            continue
        if isinstance(node, ast.ClassDef):
            _exec_class(src, node, ns, SRC)
        else:
            _exec_seg(src, node, ns, SRC)
    # nested closures: scan EVERY top-level def body (prompt_caching doctrine)
    nested = {}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef):
            for sub in ast.walk(node):
                if isinstance(sub, ast.FunctionDef) and sub is not node and sub.name in ("rate", "per_million", "host"):
                    nested[sub.name] = sub
    for nm, sub in nested.items():
        _exec_seg(src, sub, ns, SRC)
    missing = set(FUNCS) - set(ns)
    if missing:
        raise RuntimeError("missing target defs: %s" % sorted(missing))
    # stub module for the in-function `from agent.models_dev import get_model_info`
    agent = types.ModuleType("agent")
    agent.__path__ = []
    sys.modules["agent"] = agent
    md = types.ModuleType("agent.models_dev")
    md.get_model_info = lambda *a, **k: None
    sys.modules["agent.models_dev"] = md
    agent.models_dev = md
    REAL = {nm: ns[nm] for nm in FUNCS}


# ---- carrier materialization ----
def mat_usage(u):
    if not isinstance(u, dict):
        return u
    return NS["CanonicalUsage"](
        input_tokens=u.get("input_tokens", 0), output_tokens=u.get("output_tokens", 0),
        cache_read_tokens=u.get("cache_read_tokens", 0), cache_write_tokens=u.get("cache_write_tokens", 0),
        reasoning_tokens=u.get("reasoning_tokens", 0), request_count=u.get("request_count", 1),
        raw_usage=u.get("raw_usage"),
        cache_write_by_ttl=tuple(tuple(p) for p in u.get("cache_write_by_ttl", [])))


def mat_entry(e):
    if not isinstance(e, dict):
        return e
    kw = {}
    for f in dataclasses.fields(NS["PricingEntry"]):
        v = e.get(f.name)
        if f.name == "fetched_at":
            kw[f.name] = us_to_dt(v)
        elif "cost_per_million" in f.name or f.name == "request_cost":
            kw[f.name] = mat_dec(v)
        else:
            kw[f.name] = v
    return NS["PricingEntry"](**kw)


def mat_route(r):
    if not isinstance(r, dict):
        return r
    return NS["BillingRoute"](provider=r.get("provider", ""), model=r.get("model", ""),
                              base_url=r.get("base_url", ""), billing_mode=r.get("billing_mode", "unknown"))


def mat_response(r):
    if isinstance(r, dict):
        return SimpleNamespace(**r)
    return r


# ---- per-case drive ----
def drive(func, inp):
    g = inp.get
    if func == "_snap":
        tiers = {k: mat_dec(v) for k, v in (g("tiers") or {}).items()}
        return NS[func](g("inp"), g("out"), g("cache_read"), g("cache_write"),
                        version=g("version"), url=g("url"), **tiers)
    if func == "_usage_field":
        return NS[func](g("obj"), *g("path"))
    if func == "_first_nonzero":
        return NS[func](g("obj"), *[tuple(p) for p in g("paths")])
    if func == "_strip_prefix":
        return NS[func](g("name"), tuple(g("prefixes")))
    if func == "with_served_service_tier":
        return NS[func](mat_usage(g("usage")), mat_response(g("response")))
    if func == "_served_openai_tier":
        return NS[func](mat_usage(g("usage")))
    if func == "_openai_served_tier_entry":
        return NS[func](g("model"), g("tier"))
    if func == "_served_fast":
        return NS[func](mat_usage(g("usage")))
    if func == "_anthropic_fast_mode_entry":
        return NS[func](g("model"))
    if func == "_models_dev_pricing_entry":
        return NS[func](mat_route(g("route")))
    if func == "cache_write_tiers":
        return NS[func](g("response_usage"))
    if func == "_cache_write_buckets":
        return NS[func](mat_usage(g("usage")), mat_entry(g("entry")))
    if func == "_unknown_cost":
        return NS[func](g("source"), *(g("notes") or []))
    if func == "host":
        NS["url"] = g("url")
        return NS[func](g("name"))
    if func in ("rate", "per_million"):
        NS["pricing"] = g("pricing") or {}
        NS["rejected"] = set((inp.get("state") or {}).get("rejected") or [])
        NS["model_id"] = g("model_id")
        NS["logger"] = LoggerDouble()
        return NS[func](g("key"), *g("aliases"))
    raise RuntimeError("unknown func " + func)


def project_state():
    if CURRENT.get("stateful"):
        return {"rejected": sorted(NS["rejected"])}
    return {}


def install_seams():
    for live, seam in SEAMED_SIBLINGS.items():
        NS[live] = SeamFn(seam)
    NS["base_url_host_matches"] = SeamFn("base_url_host_matches")
    NS["base_url_origin"] = SeamFn("base_url_origin")
    NS["_UTC_NOW"] = SeamFn("utc_now")
    NS["logger"] = LoggerDouble()
    sys.modules["agent.models_dev"].get_model_info = SeamFn("get_model_info")


def run_case(func, inp):
    for nm, fn in REAL.items():
        NS[nm] = fn
    ctx = CaseCtx(inp)
    CURRENT["rec"] = ctx.recorder
    CURRENT["ctx"] = ctx
    CURRENT["stateful"] = func in STATEFUL
    install_seams()
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
    print("ORACLE usage_pricing_flow: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
