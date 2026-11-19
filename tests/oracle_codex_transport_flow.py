#!/usr/bin/env python3
"""Oracle for the agent/transports/codex.py UNCLAIMED-remainder FLOW port
(src/agent/transports/codex_transport_flow.c).

Drives the LIVE codex.py defs with SCRIPTED seams: every collaborator (claimed
siblings _bounded_prompt_cache_key / _xai_prefers_native_web_search /
_is_opencode_responses_backend / _alias_reserved_tools / _profile_declared_efforts,
foreign-module imports utils.base_url_hostname / base_url_host_matches,
agent.model_metadata.is_grok_46_family / openai_model_rejects_reasoning /
grok_supports_reasoning_effort, agent.web_search_registry.get_active_search_provider,
tools.web_tools._get_search_backend, agent.codex_headers.is_official_codex_base_url,
agent.codex_responses_adapter.openai_reasoning_family / strip_unverified_reasoning_items,
agent.reasoning_effort.clamp_effort / is_astra_model / codex_supported_efforts, and
logger.warning) is a recording stub fed the same scripts the C harness feeds cxt_call.
Compares the DECISION SEQUENCE (ordered "trace" of {ev,args,ret} seam events), the
canonical return ("ret") and the post-call mutation window ("state").

UNCLAIMED targets driven live: 16 top-level defs + the 2 ResponsesApiTransport
methods reject_all_turns / drop_unverified_replay. Ported siblings called by a target
(_is_openai_api_origin, _is_official_openai_responses_route, _codex_efforts_for_route,
_is_perplexity_responses_backend, _reserves_tool_search, _openai_prefers_native_web_search)
stay REAL via AST extraction, matching the C port's internal composition. Claimed
siblings are SEAMED.

Live defs are AST-extracted from the quarry (annotations kept as strings via the
__future__ flag); the needed codex.py + agent.reasoning_effort module constants are
seeded. The heavy quarry import chain is never imported: in-function imports ride
sys.modules stubs (every intermediate package registered). The exec namespace is a
registered sys.modules module (py3.14 dataclass/annotation resolution needs a real
module alias). Fail-closed: 0 cases => exit 1; a MISSING seam script raises
RuntimeError("no script for <name>"); any harness/oracle exception per case rides as
{"__raise__": cls} (BaseException catch, so SystemExit/KeyboardInterrupt/GeneratorExit
pass through as specs, not exits). Summary: 'ORACLE codex_transport_flow: N cases, M mismatches'.

Representation contract (both sides, see src/agent/transports/codex_transport_flow.c):
- scripts: MISSING key -> RuntimeError("no script for <name>") (fail-closed); null ->
  None; {"$seq":[...]} per-call queue (exhaustion raises RuntimeError("script exhausted:
  <name>")); {"__raise__":cls,"msg":m,...} raises (trace ret is the WHOLE marker); any
  other value reused (a bare ARRAY is a LIST RESULT, not a queue). None-valued seam arg
  keys dropped on both sides.
- get_active_search_provider results materialize into SimpleNamespace attribute doubles
  (.name read); _alias_reserved_tools results are 2-tuples (unpack); _profile_declared_efforts
  / codex_supported_efforts results are effort tuples; clamp_effort returns str-or-null;
  strip_unverified_reasoning_items returns an int.
- tuple/set/frozenset -> array (sets sorted); None -> null; integral floats collapse to
  ints; uncaught exception -> {"__raise__": cls} (class only).
- except Exception sites swallow the Exception family (return False); the BaseException
  trio passes through. error (reject_all_turns) materializes to a real exception instance
  carrying .status_code; str(error) is its message.
"""
import __future__
import ast
import builtins
import json
import os
import sys
import types
from pathlib import Path
from typing import Any, Callable, Optional

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
# Scrubbed-env defense: derive home from __file__ (repo is parent.parent, home one up).
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "transports" / "codex.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "transports" / "codex.py")
RE_SRC = str(QUARRY / "agent" / "reasoning_effort.py")

NAME = "codex_transport_flow"

PORTED = [
    "_bound_prompt_cache_key_field", "_merge_extra_headers",
    "_is_perplexity_responses_backend", "_reserves_tool_search",
    "_openai_prefers_native_web_search", "_alias_wire_tools", "_resolve_reasoning",
    "_is_openai_api_origin", "_is_official_openai_responses_route",
    "_codex_efforts_for_route", "_sanitize_astra_request_kwargs", "_is_azure_responses",
    "_newest_reasoning_only", "_coerce_timeout", "_sends_all_turns", "_reasoning_fields",
]
METHODS = ["reject_all_turns", "drop_unverified_replay"]
CODEX_CONSTANTS = [
    "_XAI_CLIENT_WEB_SEARCH_ALIAS", "_OPENCODE_RESERVED_TOOL_NAMES",
    "_PERPLEXITY_RESERVED_TOOL_NAMES", "_XAI_RESERVED_TOOL_NAMES", "_OPENAI_RESPONSES_HOSTS",
]
RE_CONSTANTS = [
    "CODEX_ASTRA_EFFORTS", "CODEX_LEGACY_EFFORTS", "GPT62_MODEL_IDS",
    "XAI_GROK46_EFFORTS", "XAI_LEGACY_EFFORTS",
]
WANTED = set(PORTED)
FUNCS = set(PORTED) | set(METHODS)

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_RE_TEXT = Path(RE_SRC).read_text(encoding="utf-8")
_RE_TREE = ast.parse(_RE_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
METHOD_CODE = {}
CONST_CODE = {}
RE_CONST_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node, SRC)
    elif isinstance(_node, ast.ClassDef) and _node.name == "ResponsesApiTransport":
        for _b in _node.body:
            if isinstance(_b, ast.FunctionDef) and _b.name in METHODS:
                METHOD_CODE[_b.name] = _compile_node(_b, SRC)
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(tg, ast.Name) and tg.id in CODEX_CONSTANTS:
            CONST_CODE[tg.id] = _compile_node(_node, SRC)
for _node in _RE_TREE.body:
    if isinstance(_node, (ast.Assign, ast.AnnAssign)):
        tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(tg, ast.Name) and tg.id in RE_CONSTANTS:
            RE_CONST_CODE[tg.id] = _compile_node(_node, RE_SRC)
_missing = ((WANTED - set(DEF_CODE)) | (set(METHODS) - set(METHOD_CODE))
            | (set(CODEX_CONSTANTS) - set(CONST_CODE)) | (set(RE_CONSTANTS) - set(RE_CONST_CODE)))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

# ── canonical projection ────────────────────────────────────────────────────


def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
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
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    return cls(msg)


# ── scripted seams (fail-closed) ────────────────────────────────────────────
_SCRIPTS = None


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


SEAM_ARGS = {
    "base_url_hostname": lambda a, k: {"base_url": a[0]},
    "base_url_host_matches": lambda a, k: {"base_url": a[0], "host": a[1]},
    "is_grok_46_family": lambda a, k: {"model": a[0]},
    "openai_model_rejects_reasoning": lambda a, k: {"model": a[0]},
    "grok_supports_reasoning_effort": lambda a, k: {"model": a[0]},
    "is_official_codex_base_url": lambda a, k: {"base_url": a[0]},
    "openai_reasoning_family": lambda a, k: {"model": a[0]},
    "strip_unverified_reasoning_items": lambda a, k: {"msgs": a[0], "issuer_kind": k["issuer_kind"],
                                                      "issuer_model": k["issuer_model"]},
    "get_active_search_provider": lambda a, k: {},
    "_get_search_backend": lambda a, k: {},
    "clamp_effort": lambda a, k: {"effort": a[0], "supported": a[1]},
    "is_astra_model": lambda a, k: {"model": a[0]},
    "codex_supported_efforts": lambda a, k: {"model": a[0]},
    "_bounded_prompt_cache_key": lambda a, k: {"value": a[0]},
    "_xai_prefers_native_web_search": lambda a, k: {},
    "_is_opencode_responses_backend": lambda a, k: {"params": a[0]},
    "_alias_reserved_tools": lambda a, k: {"response_tools": a[0], "reserved_names": a[1]},
    "_profile_declared_efforts": lambda a, k: {"provider": a[0], "model": a[1], "base_url": a[2]},
}

# Results consumed via attribute reads / unpacking / tuple iteration need doubles.
BUILD = {
    "get_active_search_provider": lambda r: types.SimpleNamespace(**r) if isinstance(r, dict) else r,
    "_alias_reserved_tools": lambda r: tuple(r) if isinstance(r, list) else r,
    "_profile_declared_efforts": lambda r: tuple(r) if isinstance(r, list) else r,
    "codex_supported_efforts": lambda r: tuple(r) if isinstance(r, list) else r,
}


def record(name, args):
    scripts = _SCRIPTS
    args = {k: norm(v) for k, v in args.items() if v is not None}
    script = scripts.next(name)
    if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
        marker = dict(script)
        scripts.trace.append({"ev": name, "args": args, "ret": marker})
        raise make_exc(marker)
    result = BUILD[name](script) if name in BUILD else script
    scripts.trace.append({"ev": name, "args": args, "ret": norm(script)})
    return result


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **kw):
        return record(self.name, SEAM_ARGS[self.name](a, kw))


class LoggerDouble:
    def warning(self, msg, *args, **kw):
        d = {"msg": msg, "args": list(args)}
        if kw.get("exc_info"):
            d["exc_info"] = True
        return record("logger.warning", d)


_LOGGER = LoggerDouble()


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def install_stubs():
    _mod("utils", base_url_hostname=SeamFn("base_url_hostname"),
         base_url_host_matches=SeamFn("base_url_host_matches"))
    agent = _mod("agent")
    agent.__path__ = []
    mm = _mod("agent.model_metadata",
              is_grok_46_family=SeamFn("is_grok_46_family"),
              openai_model_rejects_reasoning=SeamFn("openai_model_rejects_reasoning"),
              grok_supports_reasoning_effort=SeamFn("grok_supports_reasoning_effort"))
    agent.model_metadata = mm
    wsr = _mod("agent.web_search_registry",
               get_active_search_provider=SeamFn("get_active_search_provider"))
    agent.web_search_registry = wsr
    cra = _mod("agent.codex_responses_adapter",
               openai_reasoning_family=SeamFn("openai_reasoning_family"),
               strip_unverified_reasoning_items=SeamFn("strip_unverified_reasoning_items"))
    agent.codex_responses_adapter = cra
    tools = _mod("tools")
    tools.__path__ = []
    wt = _mod("tools.web_tools", _get_search_backend=SeamFn("_get_search_backend"))
    tools.web_tools = wt


# ── base namespace (defs exec'd once; SeamFn recorders read per-case _SCRIPTS) ──
_BASE_NS = {
    "__name__": "cxt_live_codex_transport",
    "__builtins__": builtins,
    "json": json,
    "Any": Any, "Optional": Optional, "Callable": Callable,
    "logger": _LOGGER,
    "_UNPROJECTABLE_DISABLE_WARNED": set(),
}
_MOD = types.ModuleType("cxt_live_codex_transport")
_MOD.__dict__.update(_BASE_NS)
sys.modules["cxt_live_codex_transport"] = _MOD
_BASE_NS = _MOD.__dict__
for _nm, _code in RE_CONST_CODE.items():
    exec(_code, _BASE_NS)
for _nm, _code in CONST_CODE.items():
    exec(_code, _BASE_NS)
for _nm, _code in DEF_CODE.items():
    exec(_code, _BASE_NS)
for _nm, _code in METHOD_CODE.items():
    exec(_code, _BASE_NS)
_BASE_NS["logger"] = _LOGGER

# Module-top imported collaborators + claimed siblings -> ns-global recorders.
_COLLABS = [
    "clamp_effort", "is_astra_model", "codex_supported_efforts", "is_official_codex_base_url",
    "_bounded_prompt_cache_key", "_xai_prefers_native_web_search",
    "_is_opencode_responses_backend", "_alias_reserved_tools", "_profile_declared_efforts",
]
for _nm in _COLLABS:
    _BASE_NS[_nm] = SeamFn(_nm)
install_stubs()


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    trace = []
    _SCRIPTS = Scripts(inp.get("io") or {}, trace)
    return _BASE_NS, trace, _SCRIPTS


# ── input doubles ───────────────────────────────────────────────────────────


def mk_self(d):
    d = d or {}
    ns = types.SimpleNamespace()
    for k, v in d.items():
        if k == "_all_turns_rejected":
            setattr(ns, k, frozenset(v or []))
        else:
            setattr(ns, k, v)
    return ns


def mk_error(d):
    if d is None:
        return None
    if isinstance(d, str):
        return d
    if isinstance(d, dict):
        e = make_exc(d)
        if "status_code" in d:
            e.status_code = d["status_code"]
        return e
    return d


# ── live driver ─────────────────────────────────────────────────────────────


def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    if func == "_coerce_timeout":
        return ns[func](inp.get("timeout"))
    if func == "_is_openai_api_origin":
        return ns[func](inp.get("base_url"))
    if func == "_is_perplexity_responses_backend":
        return ns[func](inp.get("params") or {})
    if func == "_is_azure_responses":
        return ns[func](inp.get("params") or {})
    if func == "_reserves_tool_search":
        return ns[func](inp.get("params") or {}, inp.get("is_xai_responses", False))
    if func == "_openai_prefers_native_web_search":
        return ns[func]()
    if func == "_is_official_openai_responses_route":
        return ns[func](inp.get("model"), inp.get("base_url"))
    if func == "_codex_efforts_for_route":
        return ns[func](inp.get("model"), inp.get("base_url"),
                        is_codex_backend=inp.get("is_codex_backend", False))
    if func == "_sends_all_turns":
        return ns[func](inp.get("model"), inp.get("params") or {})
    if func == "_reasoning_fields":
        return ns[func](inp.get("model"), inp.get("params") or {},
                        effort=inp.get("effort"), enabled=inp.get("enabled", False),
                        replay_encrypted_reasoning=inp.get("replay_encrypted_reasoning", False),
                        is_xai_responses=inp.get("is_xai_responses", False),
                        profile_reasoning=inp.get("profile_reasoning", False),
                        all_turns=inp.get("all_turns", False))
    if func == "_bound_prompt_cache_key_field":
        c = inp.get("container")
        holder["win"] = ("container", c)
        ns[func](c)
        return None
    if func == "_merge_extra_headers":
        kw = inp.get("kwargs") or {}
        holder["win"] = ("kwargs", kw)
        ns[func](kw, **(inp.get("headers") or {}))
        return None
    if func == "_newest_reasoning_only":
        return ns[func](inp.get("messages") or [])
    if func == "_sanitize_astra_request_kwargs":
        kw = inp.get("kwargs") or {}
        holder["win"] = ("kwargs", kw)
        ns[func](kw, inp.get("model"), inp.get("base_url"))
        return None
    if func == "_resolve_reasoning":
        warned = set(inp.get("warned") or [])
        ns["_UNPROJECTABLE_DISABLE_WARNED"] = warned
        holder["win"] = ("warned", warned)
        return ns[func](inp.get("model"), inp.get("params") or {})
    if func == "_alias_wire_tools":
        return ns[func](inp.get("response_tools"), inp.get("params") or {},
                        inp.get("is_xai_responses", False), inp.get("is_codex_backend", False))
    if func == "reject_all_turns":
        slf = mk_self(inp.get("self"))
        holder["win"] = ("self", slf)
        return ns[func](slf, mk_error(inp.get("error")))
    if func == "drop_unverified_replay":
        slf = mk_self(inp.get("self"))
        return ns[func](slf, *(inp.get("message_lists") or []))
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


# ── self-test: drive every fixture, flag drive failures / suspicious raises ──


def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    bad = 0
    names = FUNCS | set(SEAM_ARGS)
    for i, rec in enumerate(fixtures):
        holder = {}
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                ret = drive(rec, ns, scripts, holder)
                spec = norm(ret)
            except BaseException as e:  # noqa: BLE001
                spec = {"__raise__": type(e).__name__, "msg": str(e)}
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                msg = spec.get("msg", "")
                if any(f"'{nm}'" in msg for nm in names):
                    bad += 1
                    print(f"SELFTEST case {i} {rec['func']}: AttributeError naming ported/seam: {msg}")
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = Path(__file__).resolve().parent / "fixtures" / "codex_transport_flow.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()