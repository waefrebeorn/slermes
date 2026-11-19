#!/usr/bin/env python3
"""Oracle for the OpenAI Chat Completions transport UNCLAIMED-remainder FLOW port
(src/agent/transports/openai_transport_flow.c).

The task names agent/transports/openai.py; in this quarry snapshot the OpenAI
transport IS agent/transports/chat_completions.py ("OpenAI Chat Completions
transport (default api_mode for OpenAI-compatible providers)"), so live defs are
AST-extracted from there. Three unclaimed transport-type leaves (ToolCall._pd,
NormalizedResponse._pd, Usage.from_openai) come from agent/transports/types.py.

Drives the LIVE defs with SCRIPTED seams: every collaborator (claimed siblings
_add_prompt_cache_key / _raise_gemini_thinking_max_tokens, foreign-module imports
agent.transports.codex._alias_reserved_tools, providers.get_provider_profile,
agent.message_sanitization.route_reasoning_carriers, utils.base_url_host_matches,
session_rules.turns.coalesce_tool_call_id, agent.moonshot_schema.is_moonshot_model /
sanitize_moonshot_tools, the params["max_tokens_param_fn"] callback, and the
extra.model_dump object method) is a recording stub fed the same scripts the C
harness feeds oai_call. Compares the DECISION SEQUENCE (ordered "trace" of
{ev,args,ret} seam events), the canonical return ("ret") and the post-call
mutation window ("state").

UNCLAIMED targets driven live: 14 chat_completions.py top-level defs + the
ChatCompletionsTransport._normalize_tool_call method + ToolCall._pd /
NormalizedResponse._pd / Usage.from_openai. Ported siblings called by a target
(_mistral_tool_id, _has_replayable_thought_signature, _attr_or_model_extra,
_dump_extra_content) stay REAL via AST extraction, matching the C port's internal
composition. Claimed siblings are SEAMED.

Live defs are AST-extracted from the quarry; the needed chat_completions.py
module constants are seeded (MESSAGE_UID / DATA_USE from agent.message_metadata,
DEVELOPER_ROLE_MODELS from agent.prompt_builder). The heavy quarry import chain is
never imported: in-function imports ride sys.modules stubs (every intermediate
package registered). The exec namespace is a registered sys.modules module
(py3.14 dataclass/annotation resolution needs a real module alias). Fail-closed:
0 cases => exit 1; a MISSING seam script raises RuntimeError("no script for
<name>"); any harness/oracle exception per case rides as {"__raise__": cls}
(BaseException catch, so SystemExit/KeyboardInterrupt/GeneratorExit pass through
as specs, not exits). Summary: 'ORACLE openai_transport_flow: N cases, M mismatches'.

Representation contract (both sides, see src/agent/transports/openai_transport_flow.c):
- scripts: MISSING key -> RuntimeError("no script for <name>") (fail-closed); null ->
  None; {"$seq":[...]} per-call queue (exhaustion raises RuntimeError("script exhausted:
  <name>")); {"__raise__":cls,"msg":m,...} raises (trace ret is the WHOLE marker); any
  other value reused (a bare ARRAY is a LIST RESULT, not a queue). None-valued seam arg
  keys dropped on both sides.
- get_provider_profile results materialize into SimpleNamespace attribute doubles;
  _alias_reserved_tools results are 2-tuples (returned verbatim); route_reasoning_carriers
  results are frozensets (sorted-array projection); _add_prompt_cache_key is a void
  mutator: a {"__update__":{...}} script result merges into api_kwargs on BOTH sides.
- tuple/set/frozenset -> array (sets sorted); None -> null; integral floats collapse to
  ints; uncaught exception -> {"__raise__": cls} (class only).
- except TypeError / except Exception sites (_dump_extra_content) swallow the matching
  family; the BaseException trio passes through. Scripted raises materialize to REAL
  builtin exception instances (str(e) == the scripted msg).
- OMIT_TEMPERATURE (providers.base object() sentinel) is modeled by the string marker
  "__OMIT_TEMPERATURE__" in profile.fixed_temperature, materialized to the real sentinel.
- max_tokens_param_fn presence is a TRUTHY non-empty dict marker ({"fn":1}); the live
  params entry is replaced by the recording SeamFn so the callback is callable.
"""
import __future__
import ast
import builtins
import dataclasses
import hashlib
import json
import math
import os
import re
import string
import sys
import types
from pathlib import Path
from typing import Any

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
if not (QUARRY / "agent" / "transports" / "chat_completions.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "transports" / "chat_completions.py")
TYPES_SRC = str(QUARRY / "agent" / "transports" / "types.py")
MM_SRC = str(QUARRY / "agent" / "message_metadata.py")
PB_SRC = str(QUARRY / "agent" / "prompt_builder.py")
PORT_SRC = str(_REPO / "src" / "agent" / "transports" / "openai_transport_flow.c")

NAME = "openai_transport_flow"

PORTED = [
    "_rename_tool_search_bridge_for_xai", "_profile_replays_tool_call_signatures",
    "_wire_reasoning_carriers", "_has_replayable_thought_signature",
    "_attr_or_model_extra", "_dump_extra_content", "_pareto_score",
    "_swap_developer_role", "_apply_max_tokens", "_base_kwargs", "_finish_kwargs",
    "_route_requires_mistral_tool_ids", "_mistral_tool_id", "_sanitize_message",
]
METHODS = ["_normalize_tool_call"]
CC_CONSTANTS = [
    "_XAI_TOOL_SEARCH_ALIAS", "_STRIP_MSG_KEYS", "_STRIP_TC_KEYS",
    "_MISTRAL_TOOL_ID_RE", "_MISTRAL_ID_ALPHABET", "_MISTRAL_MODEL_MARKERS",
]
MM_CONSTANTS = ["MESSAGE_UID", "DATA_USE"]
PB_CONSTANTS = ["DEVELOPER_ROLE_MODELS"]
TYPE_CLASSES = ["ToolCall", "Usage", "NormalizedResponse"]
WANTED = set(PORTED)
FUNCS = set(PORTED) | set(METHODS) | {"ToolCall._pd", "NormalizedResponse._pd", "Usage.from_openai"}

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_TYPES_TEXT = Path(TYPES_SRC).read_text(encoding="utf-8")
_TYPES_TREE = ast.parse(_TYPES_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src, flags=0):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=flags, dont_inherit=True)


DEF_CODE = {}
METHOD_CODE = {}
CONST_CODE = {}
MM_CONST_CODE = {}
PB_CONST_CODE = {}
TYPE_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node, SRC)
    elif isinstance(_node, ast.ClassDef) and _node.name == "ChatCompletionsTransport":
        for _b in _node.body:
            if isinstance(_b, ast.FunctionDef) and _b.name in METHODS:
                METHOD_CODE[_b.name] = _compile_node(_b, SRC)
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(tg, ast.Name) and tg.id in CC_CONSTANTS:
            CONST_CODE[tg.id] = _compile_node(_node, SRC)
for _node in ast.parse(Path(MM_SRC).read_text(encoding="utf-8")).body:
    if isinstance(_node, (ast.Assign, ast.AnnAssign)):
        tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(tg, ast.Name) and tg.id in MM_CONSTANTS:
            MM_CONST_CODE[tg.id] = _compile_node(_node, MM_SRC)
for _node in ast.parse(Path(PB_SRC).read_text(encoding="utf-8")).body:
    if isinstance(_node, (ast.Assign, ast.AnnAssign)):
        tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(tg, ast.Name) and tg.id in PB_CONSTANTS:
            PB_CONST_CODE[tg.id] = _compile_node(_node, PB_SRC)
for _node in _TYPES_TREE.body:
    if isinstance(_node, ast.ClassDef) and _node.name in TYPE_CLASSES:
        TYPE_CODE[_node.name] = _compile_node(_node, TYPES_SRC, flags=_FLAGS)
_missing = ((WANTED - set(DEF_CODE)) | (set(METHODS) - set(METHOD_CODE))
            | (set(CC_CONSTANTS) - set(CONST_CODE)) | (set(MM_CONSTANTS) - set(MM_CONST_CODE))
            | (set(PB_CONSTANTS) - set(PB_CONST_CODE)) | (set(TYPE_CLASSES) - set(TYPE_CODE)))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

# ── canonical projection ────────────────────────────────────────────────────


def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items()}
    if dataclasses.is_dataclass(v) and not isinstance(v, type):
        return {k: norm(x) for k, x in vars(v).items()}
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, float) and math.isfinite(v) and v.is_integer():
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
    "_alias_reserved_tools": lambda a, k: {"tools": a[0], "reserved_names": list(a[1])},
    "get_provider_profile": lambda a, k: {"name": a[0]},
    "route_reasoning_carriers": lambda a, k: {"provider": a[0], "base_url": a[1]},
    "base_url_host_matches": lambda a, k: {"base_url": a[0], "host": a[1]},
    "coalesce_tool_call_id": lambda a, k: {"tc": a[0]},
    "is_moonshot_model": lambda a, k: {"model": a[0]},
    "sanitize_moonshot_tools": lambda a, k: {"tools": a[0]},
    "_raise_gemini_thinking_max_tokens": lambda a, k: {"model": a[0], "reasoning_config": a[1],
                                                       "requested": a[2]},
    "max_tokens_param_fn": lambda a, k: {"value": a[0]},
    "_add_prompt_cache_key": lambda a, k: {"api_kwargs": a[0], "messages": k["messages"],
                                            "tools": k["tools"],
                                            "supports_prompt_cache_key": k["supports_prompt_cache_key"],
                                            "session_id": k.get("session_id"),
                                            "cache_scope_id": k.get("cache_scope_id")},
    "model_dump": lambda a, k: {"kwargs": k},
}

# Results consumed via attribute reads / tuple / frozenset need doubles.
BUILD = {
    "get_provider_profile": lambda r: types.SimpleNamespace(**r) if isinstance(r, dict) else r,
    "_alias_reserved_tools": lambda r: tuple(r) if isinstance(r, list) else r,
    "route_reasoning_carriers": lambda r: frozenset(r) if isinstance(r, list) else r,
}

# Post-call effects of void mutator seams (applied AFTER the event is recorded).
POST = {
    "_add_prompt_cache_key": lambda a, k, r: (
        a[0].update(r["__update__"])
        if isinstance(r, dict) and isinstance(r.get("__update__"), dict) else None),
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
        r = record(self.name, SEAM_ARGS[self.name](a, kw))
        if self.name in POST:
            POST[self.name](a, kw, r)
        return r


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# ── base namespace (defs exec'd once; SeamFn recorders read per-case _SCRIPTS) ──
_OMIT_TEMPERATURE = object()   # providers.base.OMIT_TEMPERATURE sentinel double
_BASE_NS = {
    "__name__": "oai_live_openai_transport",
    "__builtins__": builtins,
    "json": json, "re": re, "string": string, "hashlib": hashlib,
    "Any": Any,
    "dataclass": dataclasses.dataclass, "field": dataclasses.field,
    "OMIT_TEMPERATURE": _OMIT_TEMPERATURE,
}
_MOD = types.ModuleType("oai_live_openai_transport")
_MOD.__dict__.update(_BASE_NS)
sys.modules["oai_live_openai_transport"] = _MOD
_BASE_NS = _MOD.__dict__
for _nm, _code in MM_CONST_CODE.items():
    exec(_code, _BASE_NS)
for _nm, _code in PB_CONST_CODE.items():
    exec(_code, _BASE_NS)
for _nm, _code in CONST_CODE.items():
    exec(_code, _BASE_NS)
for _nm, _code in TYPE_CODE.items():
    exec(_code, _BASE_NS)
for _nm, _code in DEF_CODE.items():
    exec(_code, _BASE_NS)
for _nm, _code in METHOD_CODE.items():
    exec(_code, _BASE_NS)

# Module-top imported collaborators + claimed siblings -> ns-global recorders.
_COLLABS = [
    "coalesce_tool_call_id", "is_moonshot_model", "sanitize_moonshot_tools",
    "_raise_gemini_thinking_max_tokens", "_add_prompt_cache_key",
]
for _nm in _COLLABS:
    _BASE_NS[_nm] = SeamFn(_nm)


def install_stubs():
    agent = _mod("agent")
    agent.__path__ = []
    transports = _mod("agent.transports")
    transports.__path__ = []
    agent.transports = transports
    codex = _mod("agent.transports.codex",
                 _alias_reserved_tools=SeamFn("_alias_reserved_tools"))
    transports.codex = codex
    msz = _mod("agent.message_sanitization",
               route_reasoning_carriers=SeamFn("route_reasoning_carriers"))
    agent.message_sanitization = msz
    moon = _mod("agent.moonshot_schema",
                is_moonshot_model=SeamFn("is_moonshot_model"),
                sanitize_moonshot_tools=SeamFn("sanitize_moonshot_tools"))
    agent.moonshot_schema = moon
    providers = _mod("providers", get_provider_profile=SeamFn("get_provider_profile"))
    providers.__path__ = []
    pbase = _mod("providers.base", OMIT_TEMPERATURE=_OMIT_TEMPERATURE)
    providers.base = pbase
    utils = _mod("utils", base_url_host_matches=SeamFn("base_url_host_matches"))
    utils.__path__ = []
    sr = _mod("session_rules")
    sr.__path__ = []
    turns = _mod("session_rules.turns", coalesce_tool_call_id=SeamFn("coalesce_tool_call_id"))
    sr.turns = turns


install_stubs()


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    trace = []
    _SCRIPTS = Scripts(inp.get("io") or {}, trace)
    return _BASE_NS, trace, _SCRIPTS


# ── input doubles ───────────────────────────────────────────────────────────


def mk_attr_double(d):
    return types.SimpleNamespace(**d) if isinstance(d, dict) else d


def mk_profile(d):
    if not isinstance(d, dict):
        return d
    dd = dict(d)
    if dd.get("fixed_temperature") == "__OMIT_TEMPERATURE__":
        dd["fixed_temperature"] = _OMIT_TEMPERATURE
    return types.SimpleNamespace(**dd)


def mk_dump_extra(extra, flag):
    if flag and isinstance(extra, dict):
        d = types.SimpleNamespace(**extra)
        d.model_dump = SeamFn("model_dump")
        return d
    return extra


def mk_tc(d):
    dd = dict(d or {})
    fn = dd.get("function")
    if isinstance(fn, dict):
        dd["function"] = types.SimpleNamespace(**fn)
    return types.SimpleNamespace(**dd)


# ── live driver ─────────────────────────────────────────────────────────────


def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    if func == "_rename_tool_search_bridge_for_xai":
        return ns[func](inp.get("tools"))
    if func == "_profile_replays_tool_call_signatures":
        return ns[func](inp.get("provider"))
    if func == "_wire_reasoning_carriers":
        return ns[func](inp.get("provider"), inp.get("base_url"))
    if func == "_has_replayable_thought_signature":
        return ns[func](inp.get("extra_content"))
    if func == "_attr_or_model_extra":
        return ns[func](mk_attr_double(inp.get("obj")), inp.get("name", ""))
    if func == "_dump_extra_content":
        extra_in = inp.get("extra")
        d = mk_dump_extra(extra_in, inp.get("has_model_dump", False))
        ret = ns[func](d)
        # live returns the double itself on the no-method / break paths; the C
        # port returns the original extra data (the method is not data).
        if ret is d:
            return extra_in
        return ret
    if func == "_pareto_score":
        return ns[func](inp.get("raw"))
    if func == "_swap_developer_role":
        return ns[func](inp.get("sanitized"), inp.get("model_lower", ""))
    if func == "_apply_max_tokens":
        kw = inp.get("api_kwargs") or {}
        holder["win"] = ("api_kwargs", kw)
        params = dict(inp.get("params") or {})
        mk = params.get("max_tokens_param_fn")
        if isinstance(mk, dict) and mk:   # truthy marker -> callable recorder
            params["max_tokens_param_fn"] = SeamFn("max_tokens_param_fn")
        return ns[func](kw, inp.get("model"), inp.get("reasoning_config"), params,
                        profile_max=inp.get("profile_max"))
    if func == "_base_kwargs":
        return ns[func](inp.get("model"), inp.get("sanitized"), inp.get("tools"),
                        dict(inp.get("params") or {}), profile=mk_profile(inp.get("profile")))
    if func == "_finish_kwargs":
        kw = inp.get("api_kwargs") or {}
        holder["win"] = ("api_kwargs", kw)
        return ns[func](kw, inp.get("sanitized"), dict(inp.get("params") or {}),
                        supports_prompt_cache_key=inp.get("supports_prompt_cache_key", False))
    if func == "_route_requires_mistral_tool_ids":
        return ns[func](inp.get("provider"), inp.get("model"), inp.get("base_url"))
    if func == "_mistral_tool_id":
        return ns[func](inp.get("key"))
    if func == "_sanitize_message":
        return ns[func](inp.get("msg"), inp.get("strip_extra_content", False),
                        frozenset(inp.get("strip_reasoning") or []),
                        inp.get("native_reasoning_details_type"),
                        mistral_tool_ids=inp.get("mistral_tool_ids", False))
    if func == "_normalize_tool_call":
        slf = types.SimpleNamespace(**(inp.get("self") or {}))
        if not hasattr(slf, "_last_wire_aliases"):
            slf._last_wire_aliases = None
        return ns[func](slf, mk_tc(inp.get("tc")))
    if func == "ToolCall._pd":
        tc = ns["ToolCall"](id=None, name="x", arguments="{}",
                            provider_data=inp.get("provider_data"))
        return tc._pd(inp.get("key", ""))
    if func == "NormalizedResponse._pd":
        nr = ns["NormalizedResponse"](content=None, tool_calls=None, finish_reason="stop",
                                      provider_data=inp.get("provider_data"))
        return nr._pd(inp.get("key", ""))
    if func == "Usage.from_openai":
        return ns["Usage"].from_openai(mk_attr_double(inp.get("u")))
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


def cross_check(fixtures):
    """Three-way seam-name cross-check + live-constant assertions."""
    bad = 0
    ctext = Path(PORT_SRC).read_text(encoding="utf-8")
    c_names = set(re.findall(r'oai_call\(ctx,\s*"([^"]+)"', ctext))
    if c_names != set(SEAM_ARGS):
        bad += 1
        print(f"CROSSCHECK seam-name drift: C-only={sorted(c_names - set(SEAM_ARGS))} "
              f"oracle-only={sorted(set(SEAM_ARGS) - c_names)}")
    fx_io = set()
    for rec in fixtures:
        fx_io |= set((rec["in"].get("io") or {}).keys())
    if fx_io - c_names:
        bad += 1
        print(f"CROSSCHECK fixture io keys with no C seam: {sorted(fx_io - c_names)}")
    fx_funcs = {rec["func"] for rec in fixtures}
    if fx_funcs - FUNCS:
        bad += 1
        print(f"CROSSCHECK fixture funcs not extracted: {sorted(fx_funcs - FUNCS)}")
    if FUNCS - fx_funcs:
        bad += 1
        print(f"CROSSCHECK extracted funcs with no fixture: {sorted(FUNCS - fx_funcs)}")
    checks = [
        ("DEVELOPER_ROLE_MODELS", _BASE_NS["DEVELOPER_ROLE_MODELS"], ("gpt-5", "codex")),
        ("MESSAGE_UID", _BASE_NS["MESSAGE_UID"], "message_uid"),
        ("DATA_USE", _BASE_NS["DATA_USE"], "data_use"),
        ("_XAI_TOOL_SEARCH_ALIAS", _BASE_NS["_XAI_TOOL_SEARCH_ALIAS"], "hermes_tool_search"),
        ("_STRIP_TC_KEYS", _BASE_NS["_STRIP_TC_KEYS"], ("call_id", "response_item_id")),
        ("_MISTRAL_MODEL_MARKERS", _BASE_NS["_MISTRAL_MODEL_MARKERS"],
         ("mistral", "magistral", "mixtral", "devstral", "codestral", "ministral", "pixtral", "voxtral")),
        ("_MISTRAL_ID_ALPHABET", _BASE_NS["_MISTRAL_ID_ALPHABET"],
         string.digits + string.ascii_uppercase + string.ascii_lowercase),
        ("_STRIP_MSG_KEYS", _BASE_NS["_STRIP_MSG_KEYS"],
         ("codex_reasoning_items", "codex_message_items", "tool_name", "effect_disposition",
          "timestamp", "platform_message_id", "message_id", "api_content",
          "anthropic_content_blocks", "bedrock_content_blocks", "message_uid", "data_use")),
    ]
    for nm, got, want in checks:
        if got != want:
            bad += 1
            print(f"CROSSCHECK constant {nm} drifted: {got!r} != {want!r}")
    return bad


def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    bad = cross_check(fixtures)
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
            if isinstance(spec, dict) and spec.get("__raise__") == "NameError":
                bad += 1
                print(f"SELFTEST case {i} {rec['func']}: NameError (seeding gap): {spec.get('msg', '')}")
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = Path(__file__).resolve().parent / "fixtures" / "openai_transport_flow.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()