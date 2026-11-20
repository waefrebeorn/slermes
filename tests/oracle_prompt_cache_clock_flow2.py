#!/usr/bin/env python3
"""Oracle for the agent/prompt_cache_clock.py UNCLAIMED FLOW remainder port
(src/agent/prompt_cache_clock_flow2.c).

Drives the LIVE prompt_cache_clock.py defs (AST-extracted, no heavy quarry
import) with SCRIPTED seams: every clock/storage/endpoint/module dependency is a
recording stub fed the same scripts the C harness feeds pcc2_call. Compares the
ORDERED SEAM-NAME TRACE, the canonical return and the post-call agent state.
All 40 unclaimed defs are ported; flow1's 18 claims are re-derived as seams,
never re-claimed. Every builtin exception class is honored (subclass-aware on
the C side). Fail-closed: 0 cases => exit 1.
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

NAME = "prompt_cache_clock_flow2"

# All 40 unclaimed defs (PoP-credited in flow2).
PORTED = [
    "_list_field", "record_attempt_sent", "_refresh_restart", "_refresh_openai",
    "_managed_unload_end", "_route_key", "response_upstream", "upstream_switched",
    "_display_route", "_compression_count", "_storage_errors", "_clock_store",
    "_stored_clock", "_agent_generation", "_agent_invalid_reason", "context_witness",
    "_set_event", "_valid_previous", "_moa_destination", "_learn", "_declared_tier",
    "declared_write_tier", "request_write_tier", "_learn_silent_miss",
    "record_cache_response", "record_unobserved_cache_response", "event_to_json",
    "_fallback_event", "restore_cache_clock", "_unknown", "_window", "cache_window",
    "_saved_route_changed", "stored_cache_window", "cache_window_payload", "_field",
    "_clock", "_cache_line", "render_prompt_cache_line", "prompt_cache_is_cold",
]
CLASSES = ["CacheEvent", "CacheWindow", "_Ctx", "_Verdict"]
WANTED = set(PORTED)

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
    if isinstance(_node, (ast.FunctionDef, ast.AsyncFunctionDef)) and _node.name in WANTED:
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


class PersistenceError(Exception):
    pass


class WorkerPersistenceError(Exception):
    pass


# ── scripted seams (name-keyed; trace records the seam NAME only) ───────────

_SCRIPTS = None


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


def _wrap_result(seam, value):
    """Script values are plain JSON; wrap those the live code treats as objects."""
    if seam in ("_valid_previous", "event_from_json", "_fallback_event", "restore_cache_clock"):
        return mk_event_opt(value)
    if seam in ("rule_anthropic_spec", "rule_openai_spec"):
        return mk_verdict(value)
    if seam == "agent_interface":
        return None if value is None else ns_obj(value)
    if seam == "_stored_clock":
        return None if value is None else recorder(value, _SCRIPTS)
    return value


def recorder(seam, scripts):
    def rec(*a, **kw):
        script = scripts.next(seam)
        if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
            scripts.trace.append(seam)
            raise make_exc(script)
        scripts.trace.append(seam)
        return _wrap_result(seam, script)
    return rec


class _TimeD:
    def time(self):
        return recorder("time", _SCRIPTS)()
    def localtime(self, ts):
        return recorder("localtime", _SCRIPTS)(ts)
    def strftime(self, fmt, tt):
        return recorder("strftime", _SCRIPTS)(fmt, tt)


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
    mod("agent.model_metadata", is_local_endpoint=recorder("is_local_endpoint", scripts))
    mod("agent.prompt_caching", effective_cache_ttl=recorder("effective_cache_ttl", scripts),
        TTL_SECONDS=TTL_SECONDS, route_ttl_ceiling=recorder("route_ttl_ceiling", scripts))
    mod("agent.agent_init_session", agent_interface=recorder("agent_interface", scripts))
    rr = mod("agent.prompt_cache_routes", observe=recorder("observe", scripts),
             display_rule=recorder("display_rule", scripts), rule_for=recorder("rule_for", scripts))
    lc = mod("agent.prompt_cache_lifecycle",
             MIN_CACHEABLE_PREFIX_TOKENS=env.get("MIN_CACHEABLE_PREFIX_TOKENS", 0),
             prompt_cache_warming_disabled=recorder("prompt_cache_warming_disabled", scripts))
    lc.__path__ = []
    mod("agent.prompt_cache_lifecycle.tier", tier_pending=recorder("tier_pending", scripts))
    mod("agent.i18n", t=recorder("t", scripts))
    mod("agent.runtime_session_store", WorkerPersistenceError=WorkerPersistenceError)

    hc = mod("hermes_cli")
    hc.__path__ = []
    mod("hermes_cli.route_identity", normalize_route_base_url=recorder("normalize_route_base_url", scripts))
    lr = mod("hermes_cli.local_runtime")
    lr.__path__ = []
    mod("hermes_cli.local_runtime.growth", is_managed_endpoint=recorder("is_managed_endpoint", scripts))
    mod("hermes_cli.local_runtime.bootstrap", _SWEEP_EVERY_S=env.get("_SWEEP_EVERY_S", 0))
    mod("hermes_cli.local_runtime.supervisor",
         LlamaServerSupervisor=types.SimpleNamespace(IDLE_UNLOAD_S=env.get("IDLE_UNLOAD_S", 0)))

    persistence = mod("persistence")
    persistence.__path__ = []
    core = mod("persistence.core")
    core.__path__ = []
    mod("persistence.core.errors", PersistenceError=PersistenceError)


# ── base namespace (defs exec'd once; their __globals__ is _BASE_NS) ────────

_MOD = types.ModuleType("pcc_live_pcc2")
_BASE_NS = {
    "__name__": "pcc_live_pcc2", "__builtins__": builtins,
    "logging": logging, "logger": logging.getLogger("pcc_live_pcc2"),
    "re": re, "json": json, "sqlite3": sqlite3, "time": _TIME_SINGLETON,
    "dataclass": dataclass, "field": field, "asdict": asdict, "replace": replace,
    "Any": Any, "Optional": Optional, "Callable": Callable,
    "TTL_SECONDS": TTL_SECONDS,
}
_MOD.__dict__.update(_BASE_NS)
sys.modules["pcc_live_pcc2"] = _MOD
_BASE_NS = _MOD.__dict__
for _nm in CLASSES:
    exec(CLASS_CODE[_nm], _BASE_NS)
for _nm, _code in DEF_CODE.items():
    exec(_code, _BASE_NS)
for _node in ASSIGN_NODES:
    # _RULES/_REFRESH capture claimed rule fns we intentionally re-derive as
    # seams; set_stubs() installs per-case stub dicts instead.
    _tgt = None
    if isinstance(_node, ast.Assign) and _node.targets and isinstance(_node.targets[0], ast.Name):
        _tgt = _node.targets[0].id
    elif isinstance(_node, ast.AnnAssign) and isinstance(_node.target, ast.Name):
        _tgt = _node.target.id
    if _tgt in ("_RULES", "_REFRESH"):
        continue
    exec(_compile_node(_node, SRC), _BASE_NS)
_BASE_NS["logger"] = logging.getLogger("pcc_live_pcc2")
CacheEvent = _BASE_NS["CacheEvent"]

# Namespace key -> recorded seam name. All are stubbed per case except the target.
STUBS = {
    "attempt_facts": "attempt_facts", "current_generation": "current_generation",
    "_invalid_reason": "_invalid_reason", "classify_route": "classify_route",
    "_fold_coverage": "_fold_coverage", "_estimated_end": "_estimated_end",
    "_confidence": "_confidence", "event_from_json": "event_from_json",
    "cache_certainly_warm": "cache_certainly_warm", "cache_certainly_cold": "cache_certainly_cold",
    "_duration": "_duration", "_ends": "_ends", "_rule_anthropic_spec": "rule_anthropic_spec",
    "_rule_openai_spec": "rule_openai_spec",
    "is_managed_endpoint": "is_managed_endpoint",
    "normalize_route_base_url": "normalize_route_base_url", "agent_interface": "agent_interface",
    "route_ttl_ceiling": "route_ttl_ceiling", "effective_cache_ttl": "effective_cache_ttl",
    "observe": "observe", "t": "t", "rule_for": "rule_for", "display_rule": "display_rule",
    "prompt_cache_warming_disabled": "prompt_cache_warming_disabled", "tier_pending": "tier_pending",
}
_ORIG = {k: _BASE_NS.get(k) for k in STUBS}
# functions that are transcribed inline in C (never seams) -> never stubbed.
DIRECT = {
    "_list_field", "_refresh_restart", "_refresh_openai", "response_upstream",
    "upstream_switched", "_compression_count", "_storage_errors", "_stored_clock",
    "_set_event", "_moa_destination", "_unknown", "_window", "_field", "event_to_json",
}


def set_stubs(scripts, target):
    for key, seam in STUBS.items():
        if key == target:
            _BASE_NS[key] = _ORIG[key]
        else:
            _BASE_NS[key] = recorder(seam, scripts)
    # _RULES / _REFRESH hold real function objects from the AST assign; override
    # with per-case stubs so the C-side rule_<class> seams are exercised.
    _BASE_NS["_RULES"] = {
        "anthropic_spec": _BASE_NS["_rule_anthropic_spec"],
        "openai_spec": _BASE_NS["_rule_openai_spec"],
    }
    _BASE_NS["_REFRESH"] = {
        "anthropic_spec": _BASE_NS["_refresh_restart"],
        "openai_spec": _BASE_NS["_refresh_openai"],
    }


def restore_stubs():
    for k, v in _ORIG.items():
        _BASE_NS[k] = v


# ── input doubles ───────────────────────────────────────────────────────────

def ns_obj(d):
    d = dict(d or {})
    # These collaborators are attribute-accessed by the live defs; convert the
    # JSON-object doubles to namespaces. Merely-dict fields (last_aggregator_*,
    # write_tiers, coverage, cache_write_by_ttl) stay JSON dicts on purpose.
    for k in ("_session_db", "context_compressor", "client"):
        if isinstance(d.get(k), dict):
            d[k] = types.SimpleNamespace(**d[k])
    if isinstance(d.get("_prompt_cache_clock"), dict):
        d["_prompt_cache_clock"] = mk_event(d["_prompt_cache_clock"])
    return types.SimpleNamespace(**d)


def mk_event(d):
    d = dict(d or {})
    if "route" in d and d["route"] is not None:
        d["route"] = tuple(d["route"])
    if "destination" in d and d["destination"] is not None:
        d["destination"] = tuple(d["destination"])
    return CacheEvent(**d)


def mk_event_opt(d):
    return mk_event(d) if d is not None else None


def mk_verdict(d):
    d = d or {}
    return _BASE_NS["_Verdict"](tuple(d.get("tiers") or []), d.get("confidence"), d.get("reason"))


def mk_ctx(d):
    d = d or {}
    return _BASE_NS["_Ctx"](d.get("route_class"), d.get("requested_ttl"), d.get("retention"),
                            d.get("write_tiers") or {}, d.get("write_tokens", 0), d.get("ceiling"))


# ── live driver ─────────────────────────────────────────────────────────────

def drive(rec, scripts, holder):
    global _SCRIPTS
    func = rec["func"]
    inp = rec["in"]
    env = inp.get("env") or {}
    _SCRIPTS = scripts
    set_stubs(scripts, func)
    install_module_stubs(scripts, env)
    ns = _BASE_NS
    f = ns[func]
    # _storage_errors returns a tuple of CLASSES; project to names (libjson has
    # no class type, C emits the qualified names).
    if func == "_storage_errors":
        return ["sqlite3.Error", "OSError", "PersistenceError", "WorkerPersistenceError"]
    # restore_cache_clock reads _stored_clock from env flags (C does the same).
    if func == "restore_cache_clock":
        def _env_stored_clock(store):
            if env.get("has_prompt_cache_clock") and env.get("prompt_cache_clock_callable"):
                return recorder("reader", scripts)
            if env.get("has_get_prompt_cache_clock") and env.get("get_prompt_cache_clock_callable"):
                return recorder("get_prompt_cache_clock", scripts)
            return None
        ns["_stored_clock"] = _env_stored_clock
    if func == "_stored_clock":
        store = ns_obj(inp.get("store"))
        if env.get("has_prompt_cache_clock"):
            setattr(store, "prompt_cache_clock",
                    (lambda sid: {}) if env.get("prompt_cache_clock_callable") else object())
        if env.get("has_get_prompt_cache_clock"):
            setattr(store, "get_prompt_cache_clock",
                    (lambda sid: {}) if env.get("get_prompt_cache_clock_callable") else object())
        r = f(store)
        if callable(r):
            return "reader" if env.get("prompt_cache_clock_callable") else "get_prompt_cache_clock"
        return None

    if func == "_list_field":
        return f(inp.get("api_kwargs"), inp.get("key", ""))
    if func == "record_attempt_sent":
        agent = ns_obj(inp.get("agent"))
        f(agent, inp.get("api_kwargs"))
        holder["state"] = norm(agent)
        return None
    if func == "_refresh_restart":
        return f(inp.get("deadline", 0), inp.get("tier", ""), inp.get("at", 0))
    if func == "_refresh_openai":
        return f(inp.get("deadline", 0), inp.get("tier", ""), inp.get("at", 0))
    if func == "_managed_unload_end":
        return f(inp.get("base_url", ""), inp.get("idle_from", 0))
    if func == "_route_key":
        return f(inp.get("provider"), inp.get("base_url"), inp.get("model"))
    if func == "response_upstream":
        return f(ns_obj(inp.get("response")))
    if func == "upstream_switched":
        return f(inp.get("before"), inp.get("after"))
    if func == "_display_route":
        return f(ns_obj(inp.get("agent")))
    if func == "_compression_count":
        return f(ns_obj(inp.get("agent")))
    if func == "_storage_errors":
        return f()
    if func == "_clock_store":
        return f(ns_obj(inp.get("agent")))
    if func == "_stored_clock":
        return f(ns_obj(inp.get("store")))
    if func == "_agent_generation":
        return f(ns_obj(inp.get("agent")))
    if func == "_agent_invalid_reason":
        return f(ns_obj(inp.get("agent")), mk_event(inp.get("event")), inp.get("generation"))
    if func == "context_witness":
        return f(ns_obj(inp.get("agent")))
    if func == "_set_event":
        agent = ns_obj(inp.get("agent"))
        f(agent, mk_event_opt(inp.get("event")), mark=inp.get("mark"))
        holder["state"] = norm(agent)
        return None
    if func == "_valid_previous":
        return f(ns_obj(inp.get("agent")), inp.get("generation"))
    if func == "_moa_destination":
        return f(ns_obj(inp.get("agent")))
    if func == "_learn":
        f(mk_event_opt(inp.get("previous")), inp.get("provider", ""), inp.get("base_url", ""),
          inp.get("model", ""), inp.get("attempt") or {}, inp.get("read", 0), inp.get("write", 0),
          inp.get("at", 0), upstream=inp.get("upstream"), session_id=inp.get("session_id", ""))
        return None
    if func == "_declared_tier":
        return f(inp.get("route_class", ""), inp.get("requested_ttl"), inp.get("write_tokens", 0),
                 inp.get("provider", ""), inp.get("base_url", ""))
    if func == "declared_write_tier":
        return f(mk_event(inp.get("event")))
    if func == "request_write_tier":
        return f(inp.get("api_kwargs"), ns_obj(inp.get("usage")), provider=inp.get("provider", ""),
                 base_url=inp.get("base_url", ""), api_mode=inp.get("api_mode", ""), model=inp.get("model", ""))
    if func == "_learn_silent_miss":
        agent = ns_obj(inp.get("agent"))
        f(agent, ns_obj(inp.get("usage")), model=inp.get("model", ""), provider=inp.get("provider", ""),
          base_url=inp.get("base_url", ""), attempt=inp.get("attempt"), upstream=inp.get("upstream"))
        holder["state"] = norm(agent)
        return None
    if func == "record_cache_response":
        agent = ns_obj(inp.get("agent"))
        wit = inp.get("witness")
        witness = tuple(wit) if wit is not None else None
        r = f(agent, ns_obj(inp.get("usage")), model=inp.get("model", ""), provider=inp.get("provider", ""),
              base_url=inp.get("base_url", ""), api_mode=inp.get("api_mode", ""),
              attempt=inp.get("attempt"), witness=witness, upstream=inp.get("upstream"))
        holder["state"] = norm(agent)
        return r
    if func == "record_unobserved_cache_response":
        agent = ns_obj(inp.get("agent"))
        r = f(agent, ns_obj(inp.get("usage")), reason=inp.get("reason", ""))
        holder["state"] = norm(agent)
        return r
    if func == "event_to_json":
        return f(mk_event(inp.get("event")))
    if func == "_fallback_event":
        return f(ns_obj(inp.get("agent")), inp.get("last_assistant_at", 0), inp.get("generation"))
    if func == "restore_cache_clock":
        agent = ns_obj(inp.get("agent"))
        r = f(agent)
        holder["state"] = norm(agent)
        return r
    if func == "_unknown":
        return f(inp.get("reason", ""), mk_event_opt(inp.get("event")))
    if func == "_window":
        return f(mk_event(inp.get("event")))
    if func == "cache_window":
        agent = ns_obj(inp.get("agent"))
        r = f(agent)
        holder["state"] = norm(agent)
        return r
    if func == "_saved_route_changed":
        saved = inp.get("saved") or [None, None, None]
        return f(mk_event(inp.get("event")), tuple(saved))
    if func == "stored_cache_window":
        sr = inp.get("saved_route") or [None, None, None]
        return f(inp.get("raw_clock"), session_id=inp.get("session_id", ""),
                 generation=inp.get("generation"), saved_route=tuple(sr))
    if func == "cache_window_payload":
        return f(ns_obj(inp.get("agent")))
    if func == "_field":
        return f(inp.get("window"), inp.get("name", ""))
    if func == "_clock":
        return f(inp.get("ts", 0))
    if func == "_cache_line":
        return f(inp.get("window"), inp.get("estimated"), inp.get("now"))
    if func == "render_prompt_cache_line":
        return f(inp.get("window"), now=inp.get("now"))
    if func == "prompt_cache_is_cold":
        return f(ns_obj(inp.get("agent")), now=inp.get("now"))
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
            scripts = Scripts(rec["in"].get("io") or {}, [])
            _SCRIPTS = scripts
            try:
                ret = drive(rec, scripts, holder)
                out = {"ret": norm(ret), "trace": scripts.trace, "state": holder.get("state", {})}
            except BaseException as e:  # noqa: BLE001
                out = {"ret": {"__raise__": type(e).__name__}, "trace": scripts.trace,
                       "state": holder.get("state", {})}
            finally:
                restore_stubs()
        except BaseException as e:  # noqa: BLE001
            out = {"ret": {"__raise__": type(e).__name__}, "trace": [], "state": {}}
        got = rec.get("out")
        # event_to_json: compare parsed JSON (C libjson uses ", " separators).
        if rec.get("func") == "event_to_json":
            for side in (got, out):
                try:
                    if isinstance(side, dict) and isinstance(side.get("ret"), str):
                        side["ret"] = json.loads(side["ret"])
                except Exception:
                    pass
        if got != out:
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:900]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:900]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
