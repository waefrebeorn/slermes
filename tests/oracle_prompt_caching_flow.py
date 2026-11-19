#!/usr/bin/env python3
"""Oracle for the agent/prompt_caching.py FLOW port
(src/agent/prompt_caching_flow.c).

Drives the LIVE prompt_caching.py defs with SCRIPTED seams: every external
collaborator (find_stable_prefix, the claimed sibling strip_anthropic_cache_control,
session_source_for, finite_turn_required, active_policy, adaptive_start_tier,
_infer_provider_from_url, get_provider_profile, fetch_endpoint_model_metadata,
marker_preference, load_config_readonly) is a recording stub fed the same
scripts the C harness feeds pc_call. Compares the DECISION SEQUENCE (ordered
"trace" of {ev,args,ret} seam events), the canonical return ("ret") and the
post-call mutation window ("state": the agent double for the auto/refresh
flows, the messages list for the split/strip flows, {} otherwise — echoed on
RAISE too, carrying the partial mutation history).

UNCLAIMED targets driven live (15 top-level defs + the _tool_run_end nested
closure + 5 PromptCachePlan dataclass methods). Ported siblings called by a
target (_has_part_marker, _text_part, _scaffold_parts, _route_names,
auto_cache_ttl_for_source, auto_cache_ttl_for_agent, auto_start_cache_ttl,
catalog_cache_markers) stay REAL via AST extraction, matching the C port's
internal transcription. Claimed siblings are SEAMED (strip_anthropic_cache_control).

Live defs are AST-extracted from the quarry (annotations kept as strings via
the __future__ flag); module-level Assign constants are seeded after the
class/def execs. The heavy quarry import chain is never imported: in-function
imports ride per-case sys.modules stubs (every intermediate package registered).
The exec namespace is a registered sys.modules module (py3.14 dataclasses
_is_type needs a real module alias). Fail-closed: 0 cases => exit 1; a MISSING
seam script raises RuntimeError("no script for <name>"); any harness/oracle
exception per case rides as {"__raise__": cls} (BaseException catch, so
SystemExit/KeyboardInterrupt/GeneratorExit pass through as specs, not exits).
Summary: 'ORACLE prompt_caching_flow: N cases, M mismatches'.

Representation contract (both sides, see src/agent/prompt_caching_flow.c):
- scripts: MISSING key -> RuntimeError("no script for <name>") (fail-closed);
  null -> None; {"$seq":[...]} per-call queue (exhaustion raises
  RuntimeError("script exhausted: <name>")); {"__raise__":cls,...} raises
  (trace ret is the WHOLE marker); any other value reused (a bare ARRAY is a
  LIST RESULT, not a queue). None-valued seam arg keys dropped on both sides.
- get_provider_profile / active_policy results materialize into SimpleNamespace
  attribute doubles (getattr / direct .base_url / .request_json / .source reads);
  fetch_endpoint_model_metadata / load_config_readonly / strip results stay raw
  dicts/lists (.get / [0] consumers). agent doubles are SimpleNamespace; writes
  land in the echoed state.
- PromptCachePlan instances project via asdict (field order messages, tools);
  NotImplemented -> {"__notimplemented__": true}; integral floats collapse to
  ints; uncaught exception -> {"__raise__": cls} (class only).
- raise markers use exact builtin class names plus dataclasses.FrozenInstanceError.
- No target catches exceptions: every seam raise propagates (BaseException
  passthrough); the C port carries no exception-parent table.
"""
import __future__
import ast
import builtins
import copy
import json
import sys
import types
from dataclasses import FrozenInstanceError, asdict, dataclass, field, is_dataclass
from pathlib import Path
from typing import Any, Dict, List

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stdin, "reconfigure"):
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "prompt_caching.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "prompt_caching.py")

NAME = "prompt_caching_flow"

PORTED = [
    "is_one_shot_request", "_text_part", "_scaffold_parts", "_split_unmarked_scaffolds",
    "auto_cache_ttl_for_source", "auto_cache_ttl_for_agent", "auto_start_cache_ttl",
    "refresh_auto_cache_ttl", "_route_names", "_catalog_route", "catalog_cache_markers",
    "learned_or_catalog_markers", "route_ttl_ceiling", "_has_part_marker",
    "_strip_prior_decoration",
]
NESTED = ["_tool_run_end"]          # closure of _completed_transaction_endpoint_indexes
CLASSES = ["PromptCachePlan"]
PLAN_METHODS = ["PromptCachePlan.__init__", "PromptCachePlan.__eq__",
                "PromptCachePlan.__setattr__", "PromptCachePlan.__delattr__",
                "PromptCachePlan.__hash__"]
WANTED = set(PORTED)
FUNCS = set(PORTED) | set(NESTED) | set(PLAN_METHODS)

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
NESTED_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef):
        # Nested targets live in CLAIMED parents too — scan every def body.
        for _b in _node.body:
            if isinstance(_b, ast.FunctionDef) and _b.name in NESTED:
                NESTED_CODE[_b.name] = _compile_node(_b, SRC)
        if _node.name in WANTED:
            DEF_CODE[_node.name] = _compile_node(_node, SRC)
    elif isinstance(_node, ast.ClassDef) and _node.name in CLASSES:
        CLASS_CODE[_node.name] = _compile_node(_node, SRC)
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        ASSIGN_NODES.append(_node)
_missing = (WANTED - set(DEF_CODE)) | (set(CLASSES) - set(CLASS_CODE)) | (set(NESTED) - set(NESTED_CODE))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

# ── scripted seams ──────────────────────────────────────────────────────────


class Scripts:
    def __init__(self, io, trace):
        self.io = io or {}
        self.trace = trace
        self.cursors = {}

    def next(self, name):
        # Fail-closed: a MISSING script raises; a scripted null is a valid None.
        if name not in self.io:
            return {"__raise__": "RuntimeError", "msg": "no script for " + name}
        script = self.io[name]
        if isinstance(script, dict) and "$seq" in script:
            seq = script["$seq"]
            i = self.cursors.get(name, 0)
            if i >= len(seq):
                return {"__raise__": "RuntimeError", "msg": "script exhausted: " + name}
            self.cursors[name] = i + 1
            return seq[i]
        return script


def norm(v):
    if v is NotImplemented:
        return {"__notimplemented__": True}
    if is_dataclass(v) and not isinstance(v, type):
        return norm(asdict(v))
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items()}
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    if cls_name == "FrozenInstanceError":
        return FrozenInstanceError(msg)
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    return cls(msg)


ARGMAP = {
    "find_stable_prefix": lambda a, k: {"content": a[0]},
    "strip_anthropic_cache_control": lambda a, k: {"messages": a[0]},
    "session_source_for": lambda a, k: {"platform": a[0]},
    "finite_turn_required": lambda a, k: {},
    "active_policy": lambda a, k: {},
    "adaptive_start_tier": lambda a, k: {"agent": a[0], "resolved": a[1]},
    "_infer_provider_from_url": lambda a, k: {"base_url": a[0]},
    "get_provider_profile": lambda a, k: {"name": a[0]},
    "fetch_endpoint_model_metadata": lambda a, k: {"base_url": a[0], "api_key": k.get("api_key")},
    "marker_preference": lambda a, k: {"provider": a[0], "base_url": a[1], "model": a[2]},
    "load_config_readonly": lambda a, k: {},
}

# Results consumed via attribute reads need attribute doubles; .get/[0]
# consumers stay raw.
BUILD = {
    "get_provider_profile": lambda r: types.SimpleNamespace(**r) if isinstance(r, dict) else r,
    "active_policy": lambda r: types.SimpleNamespace(**r) if isinstance(r, dict) else r,
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
        b = BUILD.get(name)
        return b(script) if b else script
    return rec


def install_module_stubs(scripts):
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    agent = mod("agent")
    agent.__path__ = []
    ss = mod("agent.session_source", session_source_for=recorder("session_source_for", scripts))
    agent.session_source = ss
    mm = mod("agent.model_metadata",
             _infer_provider_from_url=recorder("_infer_provider_from_url", scripts),
             fetch_endpoint_model_metadata=recorder("fetch_endpoint_model_metadata", scripts))
    agent.model_metadata = mm
    pcl = mod("agent.prompt_cache_lifecycle")
    pcl.__path__ = []
    agent.prompt_cache_lifecycle = pcl
    tier = mod("agent.prompt_cache_lifecycle.tier",
               adaptive_start_tier=recorder("adaptive_start_tier", scripts))
    pcl.tier = tier
    pcr = mod("agent.prompt_cache_routes", marker_preference=recorder("marker_preference", scripts))
    agent.prompt_cache_routes = pcr

    gw = mod("gateway")
    gw.__path__ = []
    sf = mod("gateway.session_finite", finite_turn_required=recorder("finite_turn_required", scripts))
    gw.session_finite = sf
    sp = mod("gateway.session_policy", active_policy=recorder("active_policy", scripts))
    gw.session_policy = sp

    prov = mod("providers", get_provider_profile=recorder("get_provider_profile", scripts))
    prov.__path__ = []

    hc = mod("hermes_cli")
    hc.__path__ = []
    cfg = mod("hermes_cli.config", load_config_readonly=recorder("load_config_readonly", scripts))
    hc.config = cfg


# ── base namespace (defs exec'd once; per-case recorders override globals) ──

_BASE_NS = {
    "__name__": "pc_live_prompt_caching",
    "__builtins__": builtins,
    "json": json,
    "copy": copy,
    "dataclass": dataclass,
    "field": field,
    "Any": Any,
    "Dict": Dict,
    "List": List,
    "find_stable_prefix": None,
    "strip_anthropic_cache_control": None,
}
# py3.14 dataclass/annotation resolution wants a registered module BEFORE the
# dataclass ClassDefs exec — run everything in the module's own dict.
_MOD = types.ModuleType("pc_live_prompt_caching")
_MOD.__dict__.update(_BASE_NS)
sys.modules["pc_live_prompt_caching"] = _MOD
_BASE_NS = _MOD.__dict__
for _nm in CLASSES:
    exec(CLASS_CODE[_nm], _BASE_NS)
for _nm, _code in DEF_CODE.items():
    exec(_code, _BASE_NS)
for _nm, _code in NESTED_CODE.items():
    exec(_code, _BASE_NS)
for _node in ASSIGN_NODES:
    exec(_compile_node(_node, SRC), _BASE_NS)


def build_ns(rec):
    inp = rec["in"]
    trace = []
    scripts = Scripts(inp.get("io") or {}, trace)
    install_module_stubs(scripts)
    _BASE_NS["find_stable_prefix"] = recorder("find_stable_prefix", scripts)
    _BASE_NS["strip_anthropic_cache_control"] = recorder("strip_anthropic_cache_control", scripts)
    return _BASE_NS, trace, scripts


# ── live driver ─────────────────────────────────────────────────────────────


def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    if func == "_text_part":
        return ns[func](inp.get("text"), inp.get("cache_marker"))
    if func == "_has_part_marker":
        return ns[func](inp.get("content"))
    if func == "is_one_shot_request":
        return ns[func](inp.get("messages"), inp.get("tools"))
    if func == "_scaffold_parts":
        return ns[func](inp.get("msg"), inp.get("cache_marker"))
    if func == "_split_unmarked_scaffolds":
        msgs = inp.get("messages")
        holder["win"] = ("messages", msgs)
        ns[func](msgs)
        return None
    if func == "_strip_prior_decoration":
        msgs = inp.get("messages")
        holder["win"] = ("messages", msgs)
        return ns[func](msgs)
    if func == "auto_cache_ttl_for_source":
        return ns[func](inp.get("source"))
    if func in ("auto_cache_ttl_for_agent", "auto_start_cache_ttl", "refresh_auto_cache_ttl"):
        ag = inp.get("agent")
        agent = types.SimpleNamespace(**ag) if isinstance(ag, dict) else ag
        holder["win"] = ("agent", agent)
        return ns[func](agent)
    if func == "_route_names":
        return ns[func](inp.get("provider"), inp.get("base_url"))
    if func == "_catalog_route":
        return ns[func](inp.get("provider"), inp.get("base_url"))
    if func == "catalog_cache_markers":
        return ns[func](inp.get("provider"), inp.get("base_url", ""),
                        inp.get("model", ""), inp.get("api_key", ""))
    if func == "learned_or_catalog_markers":
        return ns[func](inp.get("provider"), inp.get("base_url", ""),
                        inp.get("model", ""), inp.get("api_key", ""))
    if func == "route_ttl_ceiling":
        return ns[func](inp.get("provider"), inp.get("base_url", ""))
    if func == "_tool_run_end":
        ns["messages"] = inp.get("messages")
        return ns[func](inp.get("start"))
    if func == "PromptCachePlan.__init__":
        cls = ns["PromptCachePlan"]
        return cls(*(inp.get("args") or []), **(inp.get("kwargs") or {}))
    if func == "PromptCachePlan.__eq__":
        cls = ns["PromptCachePlan"]
        slf = cls(**inp["self"])
        o = inp["other"]
        kind = o.get("kind")
        if kind == "plan":
            other = cls(**(o.get("fields") or {}))
        elif kind == "subclass":
            other = type("Sub", (cls,), {})(**(o.get("fields") or {}))
        else:
            other = object()
        return slf.__eq__(other)
    if func == "PromptCachePlan.__setattr__":
        p = ns["PromptCachePlan"](**inp["fields"])
        return p.__setattr__(inp["name"], inp.get("value"))
    if func == "PromptCachePlan.__delattr__":
        p = ns["PromptCachePlan"](**inp["fields"])
        return p.__delattr__(inp["name"])
    if func == "PromptCachePlan.__hash__":
        p = ns["PromptCachePlan"](**inp["fields"])
        return hash(p)
    raise KeyError(f"unknown func {func}")


def echo_state(holder):
    w = holder.get("win")
    if not w:
        return {}
    kind, obj = w
    return {kind: norm(obj)}


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
                out = {"ret": norm(ret), "trace": trace, "state": echo_state(holder)}
            except BaseException as e:  # noqa: BLE001 - fail-closed per case
                out = {"ret": {"__raise__": type(e).__name__}, "trace": trace,
                       "state": echo_state(holder)}
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
