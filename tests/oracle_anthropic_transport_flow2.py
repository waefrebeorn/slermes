#!/usr/bin/env python3
"""Oracle for the anthropic Messages-API transport FLOW port
(src/agent/transports/anthropic_transport_flow2.c).

Drives the LIVE quarry defs with SCRIPTED seams: every CLAIMED sibling
(convert_messages_to_anthropic / convert_tools_to_anthropic / build_anthropic_kwargs
in agent/anthropic_adapter.py + agent/anthropic_message_convert.py, _to_plain_data /
_sanitize_replay_block in agent/anthropic_message_convert.py,
anthropic_response_guardrail_intervened in agent/bedrock_adapter.py, registry.get_entry
in tools.registry) plus the transport's own map_finish_reason is a recording stub
fed the SAME scripts the C harness feeds at2_call. Compares the DECISION SEQUENCE
(ordered "trace" of {ev,args,ret}), the canonical return ("ret") and out-state
("state", always {} — this set mutates no caller-held state). Ported functions call
each other REAL (AST-extracted into one registered sys.modules namespace):
normalize_response -> _unprefix_oauth_tool_name / response_finish_reason, so the
registry / guardrail / map seams are exercised in their true evaluation order.

Live defs are AST-extracted from the quarry (annotations kept strings via the
__future__ flag, decorators stripped so @property api_mode is a plain function);
the heavy import chain is never imported. Fail-closed: 0 cases => exit 1; MISSING
seam script => RuntimeError("no script for <name>"); a scripted null is a real
None result. Every per-case exception rides as
{"__raise__": cls, "msg": str(exc)} (BaseException catch, so the
SystemExit/KeyboardInterrupt/GeneratorExit trio propagates). FULL BUILTIN
EXCEPTION HIERARCHY: classes resolve through getattr(builtins, ...) with an
issubclass(BaseException) check, no hand-rolled parent table. Scripted raise
markers round-trip bare (identity match) so multi-arg constructors (KeyError)
do not mangle.

Summary: 'ORACLE anthropic_transport_flow2: N cases, M mismatches'.

Representation contract (both sides, see the port header):
- None -> null; tuple/set -> array; integral floats collapse to ints; dict key
  order mirrors live insertion order.
- ToolCall / NormalizedResponse are seeded as field-faithful SimpleNamespace
  factories (id,name,arguments,provider_data / content,tool_calls,finish_reason,
  reasoning,usage,provider_data) so norm() projects the same JSON shape the C emits.
- response is an attribute dict projected identically on both sides; content is a
  list of block objects (each an attribute dict for the live def, a JSON object
  for C); the to_plain_data seam sees the block projected back to a dict.
- None-valued seam arg keys dropped on both sides; "" kept.
"""
import __future__
import ast
import builtins
import copy
import json
import os
import sys
import types
from pathlib import Path

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parents[1]
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "transports" / "anthropic.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
# scrubbed-env safety: the quarry chain can reach Path.home()
os.environ.setdefault("HOME", str(_HERE.parents[2]))
os.environ.setdefault("USERPROFILE", str(_HERE.parents[2]))
os.environ.setdefault("LOCALAPPDATA", str(_HERE.parents[2] / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

ANT_SRC = str(QUARRY / "agent" / "transports" / "anthropic.py")
BASE_SRC = str(QUARRY / "agent" / "transports" / "base.py")
ADP_SRC = str(QUARRY / "agent" / "anthropic_adapter.py")

NAME = "anthropic_transport_flow2"

PORTED = [
    "_unprefix_oauth_tool_name", "api_mode", "convert_messages", "convert_tools",
    "build_kwargs", "normalize_response", "response_finish_reason",
    "validate_response", "extract_cache_stats", "map_finish_reason",
]
_SEAM_NAMES = [
    "convert_messages_to_anthropic", "convert_tools_to_anthropic",
    "build_anthropic_kwargs", "to_plain_data", "sanitize_replay_block",
    "guardrail_intervened", "map_finish_reason", "registry.get_entry",
]
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


def _extract_fn(node, src, store, name=None):
    node = copy.deepcopy(node)
    node.decorator_list = []  # @property api_mode -> plain function
    store[name or node.name] = _compile_node(node, src)


def _extract_assign(tree, src, names):
    """Compile top-level (or class-body) assignments for the named constants."""
    out = {}
    for node in tree.body:
        if isinstance(node, ast.Assign):
            for t in node.targets:
                if isinstance(t, ast.Name) and t.id in names:
                    out[t.id] = _compile_node(node, src)
    return out


# ── AST extraction from live quarry ─────────────────────────────────────────
_ANT_TEXT = Path(ANT_SRC).read_text(encoding="utf-8")
_ANT_TREE = ast.parse(_ANT_TEXT)
_BASE_TREE = ast.parse(Path(BASE_SRC).read_text(encoding="utf-8"))
_ADP_TREE = ast.parse(Path(ADP_SRC).read_text(encoding="utf-8"))

ANT_FUNCS = {
    "_unprefix_oauth_tool_name", "api_mode", "convert_messages", "convert_tools",
    "build_kwargs", "normalize_response", "response_finish_reason",
    "validate_response", "extract_cache_stats",
}
DEF_CODE = {}
for _node in _ANT_TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in ANT_FUNCS:
        _extract_fn(_node, ANT_SRC, DEF_CODE)
    elif isinstance(_node, ast.ClassDef) and _node.name == "AnthropicTransport":
        for _sub in _node.body:
            if isinstance(_sub, ast.FunctionDef) and _sub.name in ANT_FUNCS:
                _extract_fn(_sub, ANT_SRC, DEF_CODE)
# base.ProviderTransport.map_finish_reason (inherited by the transport)
for _node in _BASE_TREE.body:
    if isinstance(_node, ast.ClassDef) and _node.name == "ProviderTransport":
        for _sub in _node.body:
            if isinstance(_sub, ast.FunctionDef) and _sub.name == "map_finish_reason":
                _extract_fn(_sub, BASE_SRC, DEF_CODE)
_missing = set(PORTED) - set(DEF_CODE)
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

# constants
const_src = {"__builtins__": builtins}

def _grab(nodes, key):
    ns = {"__builtins__": builtins}
    exec(nodes[key], ns)
    return ns[key]

_ANT_CONSTS = _extract_assign(_ANT_TREE, ANT_SRC,
    {"_MCP_PREFIX", "_THINKING_TYPES", "_BUILD_KWARG_DEFAULTS"})
_ADP_CONSTS = _extract_assign(_ADP_TREE, ADP_SRC,
    {"_OAUTH_TOOL_NAME_ALIASES", "_OAUTH_TOOL_NAME_REVERSE_ALIASES"})
MCP_PREFIX = _grab(_ANT_CONSTS, "_MCP_PREFIX")
THINKING_TYPES = _grab(_ANT_CONSTS, "_THINKING_TYPES")
BUILD_DEFAULTS = _grab(_ANT_CONSTS, "_BUILD_KWARG_DEFAULTS")
_ADP_NS = {"__builtins__": builtins}
for _k in ("_OAUTH_TOOL_NAME_ALIASES", "_OAUTH_TOOL_NAME_REVERSE_ALIASES"):
    exec(_ADP_CONSTS[_k], _ADP_NS)   # reverse derived from the aliases dict
REVERSE_ALIASES = _ADP_NS["_OAUTH_TOOL_NAME_REVERSE_ALIASES"]
# _STOP_REASON_MAP lives on the AnthropicTransport class body
STOP_REASON_MAP = None
for _node in _ANT_TREE.body:
    if isinstance(_node, ast.ClassDef) and _node.name == "AnthropicTransport":
        for _sub in _node.body:
            if isinstance(_sub, ast.Assign):
                for _t in _sub.targets:
                    if isinstance(_t, ast.Name) and _t.id == "_STOP_REASON_MAP":
                        STOP_REASON_MAP = _grab(
                            {"_STOP_REASON_MAP": _compile_node(_sub, ANT_SRC)},
                            "_STOP_REASON_MAP")
if STOP_REASON_MAP is None:
    print(f"ORACLE {NAME}: FATAL missing _STOP_REASON_MAP", file=sys.stderr)
    sys.exit(1)


# ── canonical projection ────────────────────────────────────────────────────
def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
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


# ── per-case context ────────────────────────────────────────────────────────
CUR = {"scripts": {}, "trace": [], "raises": [], "idx": {}}


def make_exc(marker):
    """Full builtin exception hierarchy (no hand-rolled parent table)."""
    kind = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    cls = getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(str(kind), (Exception,), {})
    exc = cls(msg)
    CUR["raises"].append((exc, marker))
    return exc


def raise_spec(e):
    for exc, marker in CUR["raises"]:
        if e is exc:
            return marker
    return {"__raise__": type(e).__name__, "msg": str(e)}


def materialize(sc):
    if isinstance(sc, (dict, list)):
        return copy.deepcopy(sc)
    return sc


def record(name, args):
    args = {k: norm(v) for k, v in args.items() if v is not None}
    io = CUR["scripts"]
    trace = CUR["trace"]
    if name not in io:
        r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
    else:
        sc = io[name]
        if isinstance(sc, dict) and "$seq" in sc:
            seq = sc["$seq"]
            i = CUR["idx"].get(name, 0)
            if i >= len(seq):
                r = {"__raise__": "RuntimeError", "msg": "script exhausted: " + name}
            else:
                CUR["idx"][name] = i + 1
                r = seq[i]
        else:
            r = sc
    if isinstance(r, dict) and isinstance(r.get("__raise__"), str):
        trace.append({"ev": name, "args": args, "ret": r})
        raise make_exc(r)
    result = materialize(r)
    trace.append({"ev": name, "args": args, "ret": norm(r)})
    return result


SEAM_ARGS = {
    "convert_messages_to_anthropic":
        lambda a, k: {"messages": a[0], "base_url": k.get("base_url"), "model": k.get("model")},
    "convert_tools_to_anthropic": lambda a, k: {"tools": a[0]},
    "build_anthropic_kwargs": lambda a, k: dict(k),
    "to_plain_data": lambda a, k: {"value": a[0]},
    "sanitize_replay_block": lambda a, k: {"b": a[0]},
    "guardrail_intervened": lambda a, k: {"response": a[0]},
    "map_finish_reason": lambda a, k: {"raw_reason": a[0]},
    "registry.get_entry": lambda a, k: {"name": a[0]},
}


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **kw):
        return record(self.name, SEAM_ARGS[self.name](a, kw))


class RecLogger:
    def debug(self, *a, **k): pass
    def info(self, *a, **k): pass
    def warning(self, *a, **k): pass
    def error(self, *a, **k): pass


# field-faithful constructor stubs (dataclass shapes from agent/transports/types.py)
def ToolCall(id, name, arguments, provider_data=None):
    return types.SimpleNamespace(id=id, name=name, arguments=arguments,
                                 provider_data=provider_data)


def NormalizedResponse(content, tool_calls, finish_reason, reasoning=None,
                       usage=None, provider_data=None):
    return types.SimpleNamespace(content=content, tool_calls=tool_calls,
                                 finish_reason=finish_reason, reasoning=reasoning,
                                 usage=usage, provider_data=provider_data)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def install_stubs():
    agent = _mod("agent")
    agent.__path__ = []
    adp = _mod("agent.anthropic_adapter",
               _OAUTH_TOOL_NAME_REVERSE_ALIASES=REVERSE_ALIASES,
               build_anthropic_kwargs=SeamFn("build_anthropic_kwargs"))
    amc = _mod("agent.anthropic_message_convert",
               convert_messages_to_anthropic=SeamFn("convert_messages_to_anthropic"),
               convert_tools_to_anthropic=SeamFn("convert_tools_to_anthropic"),
               _to_plain_data=SeamFn("to_plain_data"),
               _sanitize_replay_block=SeamFn("sanitize_replay_block"))
    brd = _mod("agent.bedrock_adapter",
               anthropic_response_guardrail_intervened=SeamFn("guardrail_intervened"))
    agent.anthropic_adapter = adp
    agent.anthropic_message_convert = amc
    agent.bedrock_adapter = brd
    tools = _mod("tools")
    tools.__path__ = []
    reg = _mod("tools.registry",
               registry=types.SimpleNamespace(get_entry=SeamFn("registry.get_entry")))
    tools.registry = reg


# ── base namespace (registered module) ──────────────────────────────────────
_BASE_NS = {
    "__name__": "ant_live_transport2",
    "__builtins__": builtins,
    "json": json, "copy": copy,
    "logger": RecLogger(),
    "ToolCall": ToolCall, "NormalizedResponse": NormalizedResponse,
    "Any": object, "Dict": dict, "List": list, "Optional": object, "Tuple": tuple,
    "_MCP_PREFIX": MCP_PREFIX,
    "_THINKING_TYPES": THINKING_TYPES,
    "_BUILD_KWARG_DEFAULTS": BUILD_DEFAULTS,
    "_STOP_REASON_MAP": STOP_REASON_MAP,
}
_MOD = types.ModuleType("ant_live_transport2")
_MOD.__dict__.update(_BASE_NS)
sys.modules["ant_live_transport2"] = _MOD
_BASE_NS = _MOD.__dict__
for _nm, _code in DEF_CODE.items():
    exec(_code, _BASE_NS)
install_stubs()


def build_ns(rec):
    inp = rec["in"]
    CUR["scripts"] = inp.get("io") or {}
    CUR["trace"] = []
    CUR["raises"] = []
    CUR["idx"] = {}
    return _BASE_NS


def resp_ns(d):
    """Attribute view of a response dict: content -> list of block attribute dicts,
    usage -> attribute dict; other fields pass through unchanged."""
    if not isinstance(d, dict):
        return d
    kw = {}
    for k, v in d.items():
        if k == "content" and isinstance(v, list):
            kw[k] = [types.SimpleNamespace(**b) if isinstance(b, dict) else b for b in v]
        elif k == "usage" and isinstance(v, dict):
            kw[k] = types.SimpleNamespace(**v)
        else:
            kw[k] = v
    return types.SimpleNamespace(**kw)


# ── live driver ─────────────────────────────────────────────────────────────
def drive(rec, ns):
    func = rec["func"]
    inp = rec["in"]
    g = inp.get
    if func == "_unprefix_oauth_tool_name":
        return ns[func](g("name"))
    if func == "api_mode":
        return ns[func](None)
    if func == "convert_messages":
        return ns[func](None, g("messages"), **(g("kwargs") or {}))
    if func == "convert_tools":
        return ns[func](None, g("tools"))
    if func == "build_kwargs":
        return ns[func](None, g("model"), g("messages"), tools=g("tools"),
                        **(g("params") or {}))
    if func == "normalize_response":
        # self.response_finish_reason is a method on the transport: bind the live
        # extracted def to a self that carries the map_finish_reason seam.
        slf = types.SimpleNamespace(
            response_finish_reason=lambda r: ns["response_finish_reason"](slf, r),
            map_finish_reason=SeamFn("map_finish_reason"))
        return ns[func](slf, resp_ns(g("response")), **(g("kwargs") or {}))
    if func == "response_finish_reason":
        slf = types.SimpleNamespace(map_finish_reason=SeamFn("map_finish_reason"))
        return ns[func](slf, resp_ns(g("response")))
    if func == "validate_response":
        return ns[func](None, resp_ns(g("response")))
    if func == "extract_cache_stats":
        return ns[func](None, resp_ns(g("response")))
    if func == "map_finish_reason":
        slf = types.SimpleNamespace(_STOP_REASON_MAP=STOP_REASON_MAP)
        return ns[func](slf, g("raw_reason"))
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
        try:
            ns = build_ns(rec)
            try:
                ret = drive(rec, ns)
                out = {"ret": norm(ret), "trace": CUR["trace"], "state": {}}
            except BaseException as e:  # noqa: BLE001 - fail-closed per case
                out = {"ret": raise_spec(e), "trace": CUR["trace"], "state": {}}
        except BaseException as e:  # noqa: BLE001 - seeding failure is a mismatch
            out = {"ret": {"__raise__": type(e).__name__, "msg": str(e)},
                   "trace": [], "state": {}}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 12:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:900]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:900]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


# ── self-test: drive every fixture, flag binding/seeding failures ───────────
def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(90, exit=True)
    bad = 0
    seam_names = set(_SEAM_NAMES)
    ported_names = set(PORTED)
    seen_funcs = set()
    for i, rec in enumerate(fixtures):
        seen_funcs.add(rec["func"])
        try:
            ns = build_ns(rec)
            try:
                spec = norm(drive(rec, ns))
            except BaseException as e:  # noqa: BLE001
                spec = raise_spec(e)
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                msg = spec.get("msg", "")
                if any(("'" + nm + "'") in msg for nm in (ported_names | seam_names)):
                    bad += 1
                    print(f"SELFTEST case {i} {rec['func']}: AttributeError naming ported/seam: {msg}")
            if isinstance(spec, dict) and spec.get("__raise__") == "NameError":
                bad += 1
                print(f"SELFTEST case {i} {rec['func']}: NameError (seeding gap): {spec.get('msg')}")
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    faulthandler.cancel_dump_traceback_later()
    missing_funcs = (set(PORTED) - seen_funcs)
    if missing_funcs:
        bad += 1
        print(f"SELFTEST: funcs with no fixture: {sorted(missing_funcs)}")
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = Path(__file__).resolve().parent / "fixtures" / "anthropic_transport_flow2.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").split("\n") if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()
