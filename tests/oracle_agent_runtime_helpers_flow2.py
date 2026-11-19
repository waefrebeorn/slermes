#!/usr/bin/env python3
"""Oracle for the agent/agent_runtime_helpers.py UNCLAIMED-remainder FLOW port
(src/agent/agent_runtime_helpers_flow2.c).

Drives the LIVE agent_runtime_helpers.py defs with SCRIPTED seams: every
collaborator (claimed siblings, foreign-module imports, run_agent logger /
AIAgent statics, pool methods, plugin hooks, session-rule id variants, logger)
is a recording stub fed the same scripts the C harness feeds arh2_call.
Compares the DECISION SEQUENCE (ordered "trace" of {ev,args,ret} seam events),
the canonical return ("ret") and the post-call out-state ("state"). The ported
functions call each other REAL (AST-extracted into one namespace).

Live defs are AST-extracted from the quarry (annotations kept as strings via the
__future__ flag); only the needed module constants are seeded (_SWITCH_SNAPSHOT_FIELDS
needs ROUTE_VERDICT_FIELDS from agent/route_verdicts.py; _MISSING sentinel). The
heavy quarry import chain is never imported. Every exec namespace is a registered
sys.modules module (py3.14 dataclasses/NamedTuple _is_type needs a real module
alias). Fail-closed: 0 cases => exit 1; any harness/oracle exception per case rides
as {"__raise__": cls} (BaseException, so SystemExit/KeyboardInterrupt propagate).
Summary: 'ORACLE agent_runtime_helpers_flow2: N cases, M mismatches'.

Representation contract (both sides, see src/agent/agent_runtime_helpers_flow2.c):
- object doubles (agent/pool/entry/error.response) are attribute namespaces;
  tuple/set -> array (sets sorted); None -> null; integral floats collapse to
  ints; the _MISSING sentinel -> {"__missing__": true}; uncaught exception ->
  {"__raise__": cls}; an exception passed as a logger arg projects to
  {"__raise__": cls, "msg": str(e)}.
- scripts: absent key -> None; {"$seq": [...]} per-call queue (exhaustion raises
  RuntimeError("script exhausted: <name>")); {"__raise__": cls, "msg": m} raises
  (trace ret is the WHOLE marker); any other value reused. None-valued arg keys
  dropped on both sides.
- object-valued seam results consumed by `if obj:` truthiness (pool.current) are
  null or a NON-EMPTY record; empty {} is outside that live domain.
- fixture domain: ASCII str operands; .strip() ASCII; tool_call entries are dicts
  with dict-or-null function; function.name / tool_call_id str-or-null; json.dumps
  content ASCII scalars/containers (no non-integral floats); agent doubles are
  plain attribute namespaces (no property setters).
"""
import __future__
import ast
import builtins
import contextlib
import copy
import json
import logging
import math
import os
import re
import sys
import threading
import types
from pathlib import Path, PurePath
from typing import Any, Dict, List, Optional, Tuple

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "agent_runtime_helpers.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "agent_runtime_helpers.py")
RV_SRC = str(QUARRY / "agent" / "route_verdicts.py")

NAME = "agent_runtime_helpers_flow2"

PORTED = [
    "_prepend_corruption_marker", "_find_tool_result", "_drop_invalid_roles",
    "_drop_empty_tool_calls_arrays", "_repair_invalid_tool_call_names",
    "_drop_results_without_ids", "_pair_tool_calls_positionally",
    "_dedupe_tool_call_ids", "_realign_tool_result_names", "tool_results_this_turn",
    "_api_error_debug_info", "sync_credential_pool_entry_id",
    "_failed_credential_identity", "_restore_runtime_capabilities",
    "_apply_primary_runtime_fields", "_revert_credential_rotation",
    "_requeue_pending_steer", "_pre_tool_block_message", "_snapshot_switch_state",
    "_restore_switch_snapshot", "_recover_auth_failure",
]
HELPERS = ["_ra"]
CONSTANTS = ["_SWITCH_SNAPSHOT_FIELDS", "_MISSING"]
RV_CONSTANTS = ["ROUTE_REJECTION_FIELDS", "ROUTE_VERDICT_FIELDS"]
WANTED = set(PORTED) | set(HELPERS)

logging.disable(logging.CRITICAL)

DEFAULT_ROLES = ["system", "user", "assistant", "tool", "function", "developer"]
DEFAULT_MARKER = ("[hermes-agent: tool call arguments were corrupted in this session and "
                  "have been dropped to keep the conversation alive. See issue #15236.]")

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_RV_TEXT = Path(RV_SRC).read_text(encoding="utf-8")
_RV_TREE = ast.parse(_RV_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
CONST_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node, SRC)
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(tg, ast.Name) and tg.id in CONSTANTS:
            CONST_CODE[tg.id] = _compile_node(_node, SRC)
RV_CODE = {}
for _node in _RV_TREE.body:
    if isinstance(_node, (ast.Assign, ast.AnnAssign)):
        tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(tg, ast.Name) and tg.id in RV_CONSTANTS:
            RV_CODE[tg.id] = _compile_node(_node, RV_SRC)
_missing = (WANTED - set(DEF_CODE)) | (set(CONSTANTS) - set(CONST_CODE)) | (set(RV_CONSTANTS) - set(RV_CODE))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

# ── scripted seams ──────────────────────────────────────────────────────────
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


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    return cls(msg)


# ── base namespace (registered module) ──────────────────────────────────────
_BASE_NS = {
    "__name__": "arh2_live_agent_runtime_helpers",
    "__builtins__": builtins,
    "contextlib": contextlib, "copy": copy, "json": json, "logging": logging,
    "math": math, "re": re, "threading": threading, "types": types,
    "Any": Any, "Dict": Dict, "List": List, "Optional": Optional, "Tuple": Tuple,
}
_MOD = types.ModuleType("arh2_live_agent_runtime_helpers")
_MOD.__dict__.update(_BASE_NS)
sys.modules["arh2_live_agent_runtime_helpers"] = _MOD
_BASE_NS = _MOD.__dict__
for _nm in RV_CONSTANTS:
    exec(RV_CODE[_nm], _BASE_NS)
for _nm in CONSTANTS:
    exec(CONST_CODE[_nm], _BASE_NS)
for _nm in HELPERS + PORTED:
    exec(DEF_CODE[_nm], _BASE_NS)
_MISSING_OBJ = _BASE_NS["_MISSING"]


# ── canonical projection ────────────────────────────────────────────────────
def norm(v):
    if v is _MISSING_OBJ:
        return {"__missing__": True}
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, PurePath):
        return str(v)
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, float):
        if math.isfinite(v) and v.is_integer():
            return int(v)
        return v
    return v


# ── seam arg projections + materialization ──────────────────────────────────
SEAM_ARGS = {
    "ra._get_tool_call_id_static": lambda a, k: {"tc": a[0]},
    "ra._get_tool_call_name_static": lambda a, k: {"tc": a[0]},
    "tool_call_id_variants": lambda a, k: {"tc": a[0]},
    "tool_result_id_variants": lambda a, k: {"tool_call_id": a[0]},
    "coalesce_tool_call_id": lambda a, k: {"tc": a[0]},
    "coerce_tool_name": lambda a, k: {"name": a[0]},
    "is_actual_route": lambda a, k: {"provider": a[0], "base_url": a[1]},
    "recover_auth_failure": lambda a, k: {"agent": a[0], "pool": a[1], **k},
    "requeue_drained_steer": lambda a, k: {"agent": a[0], "steer_text": a[1]},
    "_dispatch_pre_tool_call_hooks": lambda a, k: {
        "function_name": a[0], "function_args": a[1],
        "task_id": k["task_id"], "session_id": k["session_id"],
        "tool_call_id": k["tool_call_id"], "turn_id": k["turn_id"],
        "api_request_id": k["api_request_id"], "middleware_trace": k["middleware_trace"]},
    "pool.entry_id_for_api_key": lambda a, k: {"api_key": a[0]},
    "pool.current": lambda a, k: {},
    "pool.reclaim": lambda a, k: {"revert_id": a[0], "model": k["model"]},
    "agent._swap_credential": lambda a, k: {"entry": a[0]},
}
VARIANT_SEAMS = {"tool_call_id_variants", "tool_result_id_variants"}
ENTRY_SEAMS = {"pool.current", "pool.reclaim"}
TUPLE_SEAMS = {"_dispatch_pre_tool_call_hooks"}


def materialize(name, script):
    if script is None:
        return None
    if name in VARIANT_SEAMS:
        return frozenset(script) if isinstance(script, (list, tuple)) else script
    if name in ENTRY_SEAMS:
        return types.SimpleNamespace(**script) if isinstance(script, dict) else script
    if name in TUPLE_SEAMS:
        return tuple(script) if isinstance(script, (list, tuple)) else script
    return script


def record(name, args):
    scripts = _SCRIPTS
    args = {k: norm(v) for k, v in args.items() if v is not None}
    script = scripts.next(name)
    if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
        marker = dict(script)
        scripts.trace.append({"ev": name, "args": args, "ret": marker})
        raise make_exc(marker)
    result = materialize(name, script)
    scripts.trace.append({"ev": name, "args": args, "ret": norm(script)})
    return result


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **kw):
        return record(self.name, SEAM_ARGS[self.name](a, kw))


class LoggerDouble:
    def __init__(self, prefix=""):
        self.prefix = prefix

    def _log(self, level, msg, args, kw):
        d = {"msg": msg, "args": list(args)}
        if kw.get("exc_info"):
            d["exc_info"] = True
        return record(self.prefix + "logger." + level, d)

    def debug(self, msg, *args, **kw):
        return self._log("debug", msg, args, kw)

    def warning(self, msg, *args, **kw):
        return self._log("warning", msg, args, kw)

    def info(self, msg, *args, **kw):
        return self._log("info", msg, args, kw)

    def error(self, msg, *args, **kw):
        return self._log("error", msg, args, kw)


_LOGGER = LoggerDouble("")
_BASE_NS["logger"] = _LOGGER
for _nm in ("tool_call_id_variants", "tool_result_id_variants", "coalesce_tool_call_id",
            "coerce_tool_name"):
    _BASE_NS[_nm] = SeamFn(_nm)


def _rec(name, argmap):
    def f(*a, **kw):
        return record(name, argmap(a, kw))
    return f


# ── stub modules for lazy imports ───────────────────────────────────────────
def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def install_stubs():
    agent = _mod("agent")
    agent.__path__ = []
    hc = _mod("hermes_cli")
    hc.__path__ = []
    cc = _mod("agent.context_compressor", _DB_PERSISTED_MARKER="_db_persisted")
    prov = _mod("hermes_cli.providers", is_actual_route=SeamFn("is_actual_route"))
    car = _mod("agent.credential_auth_recovery", recover_auth_failure=SeamFn("recover_auth_failure"))
    ic = _mod("agent.interrupt_control", requeue_drained_steer=SeamFn("requeue_drained_steer"))
    pl = _mod("hermes_cli.plugins", _dispatch_pre_tool_call_hooks=SeamFn("_dispatch_pre_tool_call_hooks"))
    agent.context_compressor = cc
    agent.credential_auth_recovery = car
    agent.interrupt_control = ic
    hc.providers = prov
    hc.plugins = pl


install_stubs()


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    env = inp.get("env") or {}
    trace = []
    scripts = Scripts(inp.get("io") or {}, trace)
    _SCRIPTS = scripts
    # run_agent double (_ra() lazily imports it); env-backed AIAgent constants.
    ra = types.SimpleNamespace()
    ra.logger = LoggerDouble("ra.")
    ra.AIAgent = types.SimpleNamespace(
        _VALID_API_ROLES=frozenset(env.get("valid_api_roles", DEFAULT_ROLES)),
        _get_tool_call_id_static=SeamFn("ra._get_tool_call_id_static"),
        _get_tool_call_name_static=SeamFn("ra._get_tool_call_name_static"),
        _TOOL_CALL_ARGUMENTS_CORRUPTION_MARKER=env.get("corruption_marker", DEFAULT_MARKER),
    )
    sys.modules["run_agent"] = ra
    sys.modules["agent.context_compressor"]._DB_PERSISTED_MARKER = env.get("db_persisted_marker", "_db_persisted")
    _BASE_NS["logger"] = _LOGGER
    return _BASE_NS, trace, scripts


# ── input doubles ───────────────────────────────────────────────────────────
def mk_pool(d):
    if not isinstance(d, dict):
        return d
    ns = types.SimpleNamespace(**d)
    ns.entry_id_for_api_key = _rec("pool.entry_id_for_api_key", lambda a, k: {"api_key": a[0]})
    ns.current = _rec("pool.current", lambda a, k: {})
    ns.reclaim = _rec("pool.reclaim", lambda a, k: {"revert_id": a[0], "model": k["model"]})
    return ns


def mk_agent(d, pool_methods=False, swap=False, transport_cache=False):
    d = d or {}
    ns = types.SimpleNamespace()
    for k, v in d.items():
        if k == "_credential_pool" and pool_methods and isinstance(v, dict):
            ns._credential_pool = mk_pool(v)
        elif k == "_swap_credential" and swap:
            ns._swap_credential = None if v is None else _rec("agent._swap_credential", lambda a, kk: {"entry": a[0]})
        elif k == "_transport_cache" and transport_cache and isinstance(v, dict):
            ns._transport_cache = dict(v)
        else:
            setattr(ns, k, v)
    return ns


def plain_agent(d):
    return types.SimpleNamespace(**(d or {}))


def mk_error(spec):
    spec = spec or {}
    e = make_exc({"__raise__": spec.get("cls", "Exception"), "msg": spec.get("msg", "")})
    for k, v in (spec.get("attrs") or {}).items():
        setattr(e, k, v)
    resp = spec.get("response")
    if isinstance(resp, dict):
        e.response = types.SimpleNamespace(**resp)
    elif resp is not None:
        e.response = resp
    return e


APPLY_FIELDS = ["model", "provider", "requested_provider", "base_url", "api_mode",
                "api_key", "_reasoning_echo_flag", "request_overrides", "_client_kwargs",
                "_transport_cache"]


# ── live driver ─────────────────────────────────────────────────────────────
def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    if func == "_prepend_corruption_marker":
        tm = copy.deepcopy(inp.get("tool_msg") or {})
        ns[func](tm, inp.get("marker", ""))
        holder["state"] = {"tool_msg": norm(tm)}
        return None
    if func == "_find_tool_result":
        return ns[func](inp.get("messages") or [], inp.get("start", 0), inp.get("tool_call") or {})
    if func == "_drop_invalid_roles":
        return ns[func](inp.get("messages") or [])
    if func == "_drop_empty_tool_calls_arrays":
        return ns[func](inp.get("messages") or [])
    if func == "_repair_invalid_tool_call_names":
        msgs = copy.deepcopy(inp.get("messages") or [])
        ns[func](msgs)
        holder["state"] = {"messages": norm(msgs)}
        return None
    if func == "_drop_results_without_ids":
        return ns[func](inp.get("messages") or [])
    if func == "_pair_tool_calls_positionally":
        return ns[func](inp.get("messages") or [])
    if func == "_dedupe_tool_call_ids":
        return ns[func](inp.get("messages") or [])
    if func == "_realign_tool_result_names":
        return ns[func](inp.get("messages") or [])
    if func == "tool_results_this_turn":
        return ns[func](inp.get("messages"))
    if func == "_api_error_debug_info":
        return ns[func](mk_error(inp.get("error")))
    if func == "sync_credential_pool_entry_id":
        agent = mk_agent(inp.get("agent"), pool_methods=True)
        ns[func](agent)
        holder["state"] = {"_credential_pool_entry_id": norm(getattr(agent, "_credential_pool_entry_id", None))}
        return None
    if func == "_failed_credential_identity":
        return ns[func](plain_agent(inp.get("agent")), mk_pool(inp.get("pool")))
    if func == "_restore_runtime_capabilities":
        agent = plain_agent(inp.get("agent"))
        ns[func](agent, inp.get("rt") or {})
        holder["state"] = {"runtime_capabilities": norm(getattr(agent, "runtime_capabilities", None))}
        return None
    if func == "_apply_primary_runtime_fields":
        agent = mk_agent(inp.get("agent"), transport_cache=True)
        ns[func](agent, inp.get("rt") or {})
        holder["state"] = {f: norm(getattr(agent, f, None)) for f in APPLY_FIELDS}
        return None
    if func == "_revert_credential_rotation":
        agent = mk_agent(inp.get("agent"), pool_methods=True, swap=True)
        ns[func](agent)
        holder["state"] = {"_credential_pool_revert_id": norm(getattr(agent, "_credential_pool_revert_id", None))}
        return None
    if func == "_requeue_pending_steer":
        ns[func](plain_agent(inp.get("agent")), inp.get("steer_text", ""))
        return None
    if func == "_pre_tool_block_message":
        return ns[func](plain_agent(inp.get("agent")), inp.get("function_name"),
                        inp.get("function_args"), inp.get("effective_task_id"),
                        inp.get("tool_call_id"), inp.get("middleware_trace"))
    if func == "_snapshot_switch_state":
        return ns[func](plain_agent(inp.get("agent")))
    if func == "_restore_switch_snapshot":
        agent = plain_agent(inp.get("agent"))
        snap = {k: (_MISSING_OBJ if isinstance(v, dict) and v.get("__missing__") else v)
                for k, v in (inp.get("snapshot") or {}).items()}
        ns[func](agent, snap)
        holder["state"] = {k: norm(getattr(agent, k, None)) for k, v in snap.items() if v is not _MISSING_OBJ}
        return None
    if func == "_recover_auth_failure":
        return ns[func](inp.get("agent"), inp.get("pool"), **(inp.get("kwargs") or {}))
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


# ── self-test: drive every fixture, flag drive failures / suspicious raises ──
def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(60, exit=True)
    bad = 0
    seam_names = set(SEAM_ARGS) | {"logger.debug", "logger.warning", "logger.info", "logger.error",
                                   "ra.logger.debug", "ra.logger.warning", "ra.logger.info", "ra.logger.error"}
    for i, rec in enumerate(fixtures):
        holder = {}
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                ret = drive(rec, ns, scripts, holder)
                spec = norm(ret)
            except BaseException as e:  # noqa: BLE001
                spec = {"__raise__": type(e).__name__}
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                bad += 1
                print(f"SELFTEST case {i} {rec['func']}: AttributeError (check binding/materialization)")
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = Path(__file__).resolve().parent / "fixtures" / "agent_runtime_helpers_flow2.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()