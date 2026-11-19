#!/usr/bin/env python3
"""Oracle for the agent/prompt_cache_clock.py FLOW port
(src/agent/prompt_cache_clock_flow.c).

Drives the LIVE prompt_cache_clock.py defs with SCRIPTED seams: every clock /
storage / endpoint dependency is a recording stub fed the same scripts the C
harness feeds pccf_call. Compares the DECISION SEQUENCE (ordered "trace" of
{ev,args,ret} seam events), the canonical return ("ret") and the post-call out
set ("state", _block_marker_ttls only). Internal support leaves (_list_field,
_refresh_restart, _refresh_openai, _managed_unload_end, _field, _storage_errors)
stay REAL via AST extraction so nested resolution logic is exercised, matching
the C port's internal transcription.

Live defs are AST-extracted from the quarry (annotations kept as strings via
the __future__ flag); module-level Assign/AnnAssign constants are seeded after
the class/def execs; TTL_SECONDS comes from the live agent.prompt_caching
Assign. The heavy quarry import chain is never imported. Every exec namespace
is a registered sys.modules module (py3.14 dataclasses _is_type needs a real
module alias). Fail-closed: 0 cases => exit 1; any harness/oracle exception per
case rides as {"__raise__": cls} (BaseException, so SystemExit propagates).
Summary: 'ORACLE prompt_cache_clock_flow: N cases, M mismatches'.

Representation contract (both sides, see src/agent/prompt_cache_clock_flow.c):
- CacheEvent/_Ctx/_Verdict/CacheWindow doubles are attribute dicts; dataclass
  results project via asdict (field order); tuple/set -> array (sets sorted);
  None -> null; integral floats collapse to ints; uncaught exception ->
  {"__raise__": cls}.
- scripts: absent key -> None; {"$seq": [...]} consumed per call (exhaustion
  raises RuntimeError("script exhausted: <name>")); {"__raise__": cls, ...}
  raises (trace ret is the WHOLE marker); any other value reused. None-valued
  arg keys dropped on both sides.
- time rides a singleton double whose time() dispatches to the current case's
  scripts. is_local_endpoint / is_managed_endpoint ride lazy-imported module
  stubs; IDLE_UNLOAD_S / _SWEEP_EVERY_S come from env. current_generation's
  reader is a getattr double gated by env.has_reader; its scripted results are
  int/bool/str/null only (libjson cannot distinguish int from float, so the
  isinstance(value, int) branch is exercised with ints/bools vs str/null).
- raise markers use exact builtin class names plus sqlite3.Error,
  PersistenceError and WorkerPersistenceError (real exception classes).
"""
import __future__
import ast
import builtins
import json
import logging
import re
import sqlite3
import sys
import types
from dataclasses import asdict, dataclass, field, is_dataclass, replace
from pathlib import Path
from typing import Any, Callable, Optional

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stdin, "reconfigure"):
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "prompt_cache_clock.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "prompt_cache_clock.py")
PC_SRC = str(QUARRY / "agent" / "prompt_caching.py")

NAME = "prompt_cache_clock_flow"

PORTED = [
    "_block_marker_ttls", "_marker_ttl", "_wire_field", "attempt_facts",
    "classify_route", "_written_tiers", "_rule_anthropic_spec", "_rule_openai_spec",
    "_fold_coverage", "_ends", "_estimated_end", "_confidence", "_invalid_reason",
    "current_generation", "event_from_json", "cache_certainly_warm",
    "cache_certainly_cold", "_duration",
]
PURE = ["_list_field", "_refresh_restart", "_refresh_openai", "_managed_unload_end",
        "_field", "_storage_errors"]
CLASSES = ["CacheEvent", "CacheWindow", "_Ctx", "_Verdict"]
WANTED = set(PORTED) | set(PURE)

logging.disable(logging.CRITICAL)

# ── AST extraction ──────────────────────────────────────────────────────────

_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
CLASS_CODE = {}
ASSIGN_NODES = []
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node, SRC)
    elif isinstance(_node, ast.ClassDef) and _node.name in CLASSES:
        CLASS_CODE[_node.name] = _compile_node(_node, SRC)
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        ASSIGN_NODES.append(_node)
_missing = (WANTED - set(DEF_CODE)) | (set(CLASSES) - set(CLASS_CODE))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)


def _extract_assign(src, name):
    tree = ast.parse(Path(src).read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == name for t in node.targets):
            ns = {"__builtins__": builtins}
            exec(_compile_node(node, src), ns)
            return ns[name]
    print(f"ORACLE {NAME}: FATAL missing constant {name} in {src}", file=sys.stderr)
    sys.exit(1)


TTL_SECONDS = _extract_assign(PC_SRC, "TTL_SECONDS")


# Real exception classes for the storage-error catch set.
class PersistenceError(Exception):
    pass


class WorkerPersistenceError(Exception):
    pass


# ── scripted seams ──────────────────────────────────────────────────────────

_SCRIPTS = None  # current case's Scripts (singleton time double dispatches here)


class Scripts:
    def __init__(self, io, trace):
        self.io = io or {}
        self.trace = trace
        self.cursors = {}

    def next(self, name):
        script = self.io.get(name)
        if isinstance(script, dict) and "$seq" in script:
            seq = script["$seq"]
            i = self.cursors.get(name, 0)
            if i >= len(seq):
                return {"__raise__": "RuntimeError", "msg": f"script exhausted: {name}"}
            self.cursors[name] = i + 1
            return seq[i]
        return script


def norm(v):
    if is_dataclass(v) and not isinstance(v, type):
        return norm(asdict(v))
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    if cls_name == "sqlite3.Error":
        return sqlite3.Error(msg)
    if cls_name == "PersistenceError":
        return PersistenceError(msg)
    if cls_name == "WorkerPersistenceError":
        return WorkerPersistenceError(msg)
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    return cls(msg)


ARGMAP = {
    "time": lambda a, k: {},
    "is_local_endpoint": lambda a, k: {"base_url": a[0]},
    "is_managed_endpoint": lambda a, k: {"base_url": a[0]},
    "prompt_cache_generation": lambda a, k: {"session_id": a[0]},
}


def recorder(name, scripts):
    def rec(*a, **kw):
        args = {k: norm(v) for k, v in ARGMAP[name](a, kw).items() if v is not None}
        script = scripts.next(name)
        if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
            marker = dict(script)
            scripts.trace.append({"ev": name, "args": args, "ret": marker})
            raise make_exc(marker)
        scripts.trace.append({"ev": name, "args": args, "ret": norm(script)})
        return script
    return rec


class _TimeD:
    """time module double: time() records the time seam."""
    def time(self):
        return recorder("time", _SCRIPTS)()


_TIME_SINGLETON = _TimeD()


def install_module_stubs(scripts, env):
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    agent = mod("agent")
    agent.__path__ = []
    mm = mod("agent.model_metadata", is_local_endpoint=recorder("is_local_endpoint", scripts))
    agent.model_metadata = mm
    rss = mod("agent.runtime_session_store", WorkerPersistenceError=WorkerPersistenceError)
    agent.runtime_session_store = rss

    hc = mod("hermes_cli")
    hc.__path__ = []
    lr = mod("hermes_cli.local_runtime")
    lr.__path__ = []
    hc.local_runtime = lr
    growth = mod("hermes_cli.local_runtime.growth",
                 is_managed_endpoint=recorder("is_managed_endpoint", scripts))
    lr.growth = growth
    bootstrap = mod("hermes_cli.local_runtime.bootstrap",
                    _SWEEP_EVERY_S=env.get("sweep_every_s", 0))
    lr.bootstrap = bootstrap
    supervisor = mod("hermes_cli.local_runtime.supervisor",
                     LlamaServerSupervisor=types.SimpleNamespace(
                         IDLE_UNLOAD_S=env.get("idle_unload_s", 0)))
    lr.supervisor = supervisor

    persistence = mod("persistence")
    persistence.__path__ = []
    core = mod("persistence.core")
    core.__path__ = []
    persistence.core = core
    errors = mod("persistence.core.errors", PersistenceError=PersistenceError)
    core.errors = errors


# ── base namespace (defs exec'd once; time singleton captures default args) ──

_BASE_NS = {
    "__name__": "pcc_live_prompt_cache_clock",
    "__builtins__": builtins,
    "logging": logging,
    "logger": logging.getLogger("pcc_live_prompt_cache_clock"),
    "re": re,
    "json": json,
    "sqlite3": sqlite3,
    "time": _TIME_SINGLETON,
    "dataclass": dataclass,
    "field": field,
    "asdict": asdict,
    "replace": replace,
    "Any": Any,
    "Optional": Optional,
    "Callable": Callable,
    "TTL_SECONDS": TTL_SECONDS,
}
# py3.14 dataclass/annotation resolution wants a registered module BEFORE the
# dataclass ClassDefs exec — run everything in the module's own dict.
_MOD = types.ModuleType("pcc_live_prompt_cache_clock")
_MOD.__dict__.update(_BASE_NS)
sys.modules["pcc_live_prompt_cache_clock"] = _MOD
_BASE_NS = _MOD.__dict__
for _nm in CLASSES:
    exec(CLASS_CODE[_nm], _BASE_NS)
for _nm, _code in DEF_CODE.items():
    exec(_code, _BASE_NS)
for _node in ASSIGN_NODES:
    exec(_compile_node(_node, SRC), _BASE_NS)
_BASE_NS["logger"] = logging.getLogger("pcc_live_prompt_cache_clock")
CacheEvent = _BASE_NS["CacheEvent"]
_Verdict = _BASE_NS["_Verdict"]
_Ctx = _BASE_NS["_Ctx"]


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    env = inp.get("env") or {}
    trace = []
    scripts = Scripts(inp.get("io") or {}, trace)
    _SCRIPTS = scripts
    install_module_stubs(scripts, env)
    return _BASE_NS, trace, scripts


# ── input doubles ───────────────────────────────────────────────────────────


def mk_event(d):
    d = dict(d)
    if "route" in d:
        d["route"] = tuple(d["route"])
    if "destination" in d:
        d["destination"] = tuple(d["destination"])
    return CacheEvent(**d)


def mk_event_opt(d):
    return mk_event(d) if d is not None else None


def mk_verdict(d):
    d = d or {}
    return _Verdict(tuple(d.get("tiers") or []), d.get("confidence"), d.get("reason"))


def mk_ctx(d):
    d = d or {}
    return _Ctx(d.get("route_class"), d.get("requested_ttl"), d.get("retention"),
                d.get("write_tiers") or {}, d.get("write_tokens", 0), d.get("ceiling"))


# ── live driver ─────────────────────────────────────────────────────────────


def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    env = inp.get("env") or {}
    if func == "_block_marker_ttls":
        out = set(inp.get("out") or [])
        ns[func](inp.get("block"), out)
        holder["state"] = norm(out)
        return None
    if func == "_marker_ttl":
        return ns[func](inp.get("api_kwargs"))
    if func == "_wire_field":
        return ns[func](inp.get("api_kwargs"), inp.get("key", ""))
    if func == "attempt_facts":
        return ns[func](inp.get("api_kwargs"))
    if func == "classify_route":
        return ns[func](provider=inp.get("provider", ""), base_url=inp.get("base_url", ""),
                        api_mode=inp.get("api_mode", ""), model=inp.get("model", ""),
                        has_markers=inp.get("has_markers", False), retention=inp.get("retention"))
    if func == "_written_tiers":
        return ns[func](inp.get("tiers") or {})
    if func == "_rule_anthropic_spec":
        return ns[func](mk_ctx(inp.get("ctx")))
    if func == "_rule_openai_spec":
        return ns[func](mk_ctx(inp.get("ctx")))
    if func == "_fold_coverage":
        return ns[func](mk_event_opt(inp.get("previous")), mk_verdict(inp.get("verdict")),
                        inp.get("route_class", ""), inp.get("at", 0),
                        inp.get("read", 0), inp.get("write", 0))
    if func == "_ends":
        return ns[func](inp.get("coverage") or {})
    if func == "_estimated_end":
        return ns[func](inp.get("coverage") or {}, base_url=inp.get("base_url", ""),
                        retention=inp.get("retention"), at=inp.get("at", 0),
                        previous=mk_event_opt(inp.get("previous")),
                        carried=inp.get("carried", False), idle_from=inp.get("idle_from", 0))
    if func == "_confidence":
        return ns[func](mk_verdict(inp.get("verdict")), mk_event_opt(inp.get("previous")),
                        inp.get("carried", False), inp.get("write", 0))
    if func == "_invalid_reason":
        route = tuple(inp["route"]) if inp.get("route") is not None else None
        return ns[func](mk_event(inp["event"]), session_id=inp.get("session_id", ""),
                        generation=inp.get("generation"), route=route)
    if func == "current_generation":
        if env.get("has_reader", False):
            db = types.SimpleNamespace(
                prompt_cache_generation=recorder("prompt_cache_generation", scripts))
        else:
            db = types.SimpleNamespace()
        return ns[func](db, inp.get("session_id"))
    if func == "event_from_json":
        return ns[func](inp.get("raw"))
    if func == "cache_certainly_warm":
        window = inp.get("window")
        if env.get("as_object") and isinstance(window, dict):
            window = types.SimpleNamespace(**window)
        return ns[func](window, now=inp.get("now"))
    if func == "cache_certainly_cold":
        window = inp.get("window")
        if env.get("as_object") and isinstance(window, dict):
            window = types.SimpleNamespace(**window)
        return ns[func](window, now=inp.get("now"))
    if func == "_duration":
        return ns[func](inp.get("seconds"))
    raise KeyError(f"unknown func {func}")


def main():
    n = mism = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        holder = {}
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                ret = drive(rec, ns, scripts, holder)
                out = {"ret": norm(ret), "trace": trace, "state": holder.get("state", {})}
            except BaseException as e:  # noqa: BLE001 - fail-closed per case
                out = {"ret": {"__raise__": type(e).__name__}, "trace": trace,
                       "state": holder.get("state", {})}
        except BaseException as e:  # noqa: BLE001 - seeding failure is a mismatch
            out = {"ret": {"__raise__": type(e).__name__}, "trace": [], "state": {}}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:800]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:800]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()