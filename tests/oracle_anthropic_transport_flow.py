#!/usr/bin/env python3
"""Oracle for the anthropic Messages-API transport FLOW + leaf port
(src/agent/transports/anthropic_transport_flow.c).

Drives the LIVE quarry defs with SCRIPTED seams: every CLAIMED sibling
(_sanitize_tool_id / _sanitize_replay_block / _extract_preserved_thinking_blocks /
_content_parts_to_anthropic_blocks / _apply_assistant_cache_control_to_last_cacheable_block
in agent/anthropic_message_convert.py, map_finish_reason on the transport, and
bedrock_adapter.anthropic_response_guardrail_intervened) plus the foreign registry
lookup is a recording stub fed the SAME scripts the C harness feeds ant_call.
Compares the DECISION SEQUENCE (ordered "trace" of {ev,args,ret}), the canonical
return ("ret") and out-state ("state", always {} — this set mutates no caller state).
The PORTED functions call each other REAL (AST-extracted into one namespace), so
internal composition (assistant_replay_carrier -> _replay_ordered_blocks ->
_parse_tool_args/_block_type/_cache_control_of/_has_block_type/_text_block,
_tool_use_block, _tool_result_content, _convert_system_content, ...) is exercised.

Live defs are AST-extracted from the quarry (annotations kept strings via the
__future__ flag); the heavy import chain is never imported. The exec namespace is a
registered sys.modules module (py3.14 dataclasses/_is_type needs a real alias).
Fail-closed: 0 cases => exit 1; MISSING seam script => RuntimeError("no script for
<name>"); a scripted null is a real None result. Every per-case exception rides as
{"__raise__": cls, "msg": str(exc)} (BaseException catch, so SystemExit/
KeyboardInterrupt/GeneratorExit propagate). Scripted raise markers round-trip bare
(identity match) so multi-arg constructors (KeyError) do not mangle.
Summary: 'ORACLE anthropic_transport_flow: N cases, M mismatches'.

Representation contract (both sides, see the port header):
- None -> null; tuple/set/generator -> array (sets sorted by str()); integral floats
  collapse to ints; dict key order mirrors live insertion order.
- apply_assistant_cache_control is a STATE-EFFECT seam: recorded AND its documented
  mutation (setdefault cache_control on the last text/tool_use block, reversed) is
  applied identically by the C port and this oracle's ApplyCCSeam; args snapshot blocks
  BEFORE the effect.
- _THINKING_TYPES / _CACHEABLE_TYPES are seeded as TUPLES (== membership, matching the
  C list model; avoids frozenset unhashable edges). Block "type" values are hashable
  scalars; _has_block_type `types` arg is a list; _block_ids key values str/null.
- _parse_tool_args domain excludes NaN/Infinity, lone surrogates, >2^53 ints, depth>64.
- response.stop_reason always present; response is an attribute namespace projected to
  its field dict in seam args (matching the C JSON object).
"""
import __future__
import ast
import builtins
import copy
import json
import os
import re
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
if not (QUARRY / "agent" / "anthropic_message_convert.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
# scrubbed-env safety: the quarry chain can reach Path.home()
os.environ.setdefault("HOME", str(_HERE.parents[2]))
os.environ.setdefault("USERPROFILE", str(_HERE.parents[2]))
os.environ.setdefault("LOCALAPPDATA", str(_HERE.parents[2] / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

AMC_SRC = str(QUARRY / "agent" / "anthropic_message_convert.py")
ANT_SRC = str(QUARRY / "agent" / "transports" / "anthropic.py")
ADP_SRC = str(QUARRY / "agent" / "anthropic_adapter.py")

NAME = "anthropic_transport_flow"

AMC_PORTED = [
    "_block_type", "_has_block_type", "_is_blank_text_block", "_cache_control_of",
    "_text_block", "_text_block_with_citations", "_parse_tool_args", "_strip_thinking",
    "_block_ids", "_assistant_block_lists", "_tool_use_block", "_replay_ordered_blocks",
    "assistant_replay_carrier", "_tool_result_content", "_concat_content",
    "_keep_valid_thinking", "_current_tool_loop_start", "_image_source_data",
    "_convert_system_content",
]
ANT_PORTED = ["_unprefix_oauth_tool_name", "response_finish_reason"]
PORTED = AMC_PORTED + ANT_PORTED
# claimed siblings seamed (live name -> seam name)
SEAMED_SIBLINGS = {
    "_sanitize_tool_id": "sanitize_tool_id",
    "_sanitize_replay_block": "sanitize_replay_block",
    "_extract_preserved_thinking_blocks": "extract_preserved_thinking_blocks",
    "_content_parts_to_anthropic_blocks": "content_parts_to_anthropic_blocks",
    "_apply_assistant_cache_control_to_last_cacheable_block": "apply_assistant_cache_control",
}
WANTED_AMC = set(AMC_PORTED)

_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


# ── AST extraction ──────────────────────────────────────────────────────────
_AMC_TEXT = Path(AMC_SRC).read_text(encoding="utf-8")
_AMC_TREE = ast.parse(_AMC_TEXT)
_ANT_TEXT = Path(ANT_SRC).read_text(encoding="utf-8")
_ANT_TREE = ast.parse(_ANT_TEXT)
_ADP_TEXT = Path(ADP_SRC).read_text(encoding="utf-8")
_ADP_TREE = ast.parse(_ADP_TEXT)

DEF_CODE = {}
for _node in _AMC_TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED_AMC:
        DEF_CODE[_node.name] = _compile_node(_node, AMC_SRC)
# anthropic.py: module fn + the response_finish_reason method out of the ClassDef
for _node in _ANT_TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name == "_unprefix_oauth_tool_name":
        DEF_CODE[_node.name] = _compile_node(_node, ANT_SRC)
    elif isinstance(_node, ast.ClassDef) and _node.name == "AnthropicTransport":
        for _sub in _node.body:
            if isinstance(_sub, ast.FunctionDef) and _sub.name == "response_finish_reason":
                DEF_CODE[_sub.name] = _compile_node(_sub, ANT_SRC)
_missing = set(PORTED) - set(DEF_CODE)
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

# constants
CONST_CODE = {}
for _node in _ADP_TREE.body:
    if isinstance(_node, ast.Assign):
        _tg = _node.targets[0]
        if isinstance(_tg, ast.Name) and _tg.id in ("_OAUTH_TOOL_NAME_ALIASES", "_OAUTH_TOOL_NAME_REVERSE_ALIASES"):
            CONST_CODE[_tg.id] = _compile_node(_node, ADP_SRC)
if "_OAUTH_TOOL_NAME_REVERSE_ALIASES" not in CONST_CODE:
    print(f"ORACLE {NAME}: FATAL missing reverse-alias constant", file=sys.stderr)
    sys.exit(1)
_cns = {"__builtins__": builtins}
exec(CONST_CODE["_OAUTH_TOOL_NAME_ALIASES"], _cns)
exec(CONST_CODE["_OAUTH_TOOL_NAME_REVERSE_ALIASES"], _cns)
REVERSE_ALIASES = _cns["_OAUTH_TOOL_NAME_REVERSE_ALIASES"]


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

BASE_EXC = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}


def make_exc(marker):
    kind = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    cls = BASE_EXC.get(kind)
    if cls is None:
        b = getattr(builtins, kind, None)
        if isinstance(b, type) and issubclass(b, BaseException):
            cls = b
        else:
            cls = type(str(kind), (Exception,), {})
    if cls is KeyError:
        exc = cls(msg)
    else:
        exc = cls(msg)
    CUR["raises"].append((exc, marker))
    return exc


def raise_spec(e):
    for exc, marker in CUR["raises"]:
        if e is exc:
            return marker
    return {"__raise__": type(e).__name__, "msg": str(e)}


def materialize(name, sc):
    # C universal seam returns a fresh json_copy per call; mirror with deepcopy so
    # in-place mutation of a seam result never aliases the fixture script.
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
    result = materialize(name, r)
    trace.append({"ev": name, "args": args, "ret": norm(r)})
    return result


SEAM_ARGS = {
    "sanitize_tool_id": lambda a, k: {"tool_id": a[0]},
    "sanitize_replay_block": lambda a, k: {"b": a[0]},
    "extract_preserved_thinking_blocks": lambda a, k: {"message": a[0]},
    "content_parts_to_anthropic_blocks": lambda a, k: {"parts": a[0]},
    "map_finish_reason": lambda a, k: {"raw_reason": a[0]},
    "guardrail_intervened": lambda a, k: {"response": a[0]},
    "registry.get_entry": lambda a, k: {"name": a[0]},
}


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **kw):
        return record(self.name, SEAM_ARGS[self.name](a, kw))


class ApplyCCSeam:
    """_apply_assistant_cache_control_to_last_cacheable_block: recorded seam PLUS the
    identical state effect the C port applies (ant_apply_cc_effect). Args snapshot
    blocks BEFORE the effect (record norms first)."""
    def __call__(self, blocks, cache_control):
        r = record("apply_assistant_cache_control",
                   {"blocks": blocks, "cache_control": cache_control})
        if isinstance(cache_control, dict):
            for block in reversed(blocks):
                bt = block.get("type") if isinstance(block, dict) else None
                if bt in ("text", "tool_use"):
                    if isinstance(block, dict):
                        block.setdefault("cache_control", dict(cache_control))
                    break
        return r


class RecLogger:
    def debug(self, *a, **k): pass
    def info(self, *a, **k): pass
    def warning(self, *a, **k): pass
    def error(self, *a, **k): pass


# ── stub modules for the in-function imports ────────────────────────────────
def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def install_stubs():
    agent = _mod("agent")
    agent.__path__ = []
    adp = _mod("agent.anthropic_adapter", _OAUTH_TOOL_NAME_REVERSE_ALIASES=REVERSE_ALIASES)
    brd = _mod("agent.bedrock_adapter",
               anthropic_response_guardrail_intervened=SeamFn("guardrail_intervened"))
    agent.anthropic_adapter = adp
    agent.bedrock_adapter = brd
    tools = _mod("tools")
    tools.__path__ = []
    reg = _mod("tools.registry",
               registry=types.SimpleNamespace(get_entry=SeamFn("registry.get_entry")))
    tools.registry = reg


# ── base namespace (registered module) ──────────────────────────────────────
_BASE_NS = {
    "__name__": "ant_live_transport",
    "__builtins__": builtins,
    "json": json, "copy": copy, "re": re,
    "logger": RecLogger(),
    "Any": object, "Dict": dict, "List": list, "Optional": object, "Tuple": tuple,
    # constants (tuples for == membership, matching the C list model)
    "_THINKING_TYPES": ("thinking", "redacted_thinking"),
    "_CACHEABLE_TYPES": ("text", "tool_use"),
    "_EMPTY_TEXT_PLACEHOLDER": "(empty)",
    "_MCP_PREFIX": "mcp__",
}
_MOD = types.ModuleType("ant_live_transport")
_MOD.__dict__.update(_BASE_NS)
sys.modules["ant_live_transport"] = _MOD
_BASE_NS = _MOD.__dict__
for _nm, _code in DEF_CODE.items():
    exec(_code, _BASE_NS)
# install claimed-sibling seams AFTER exec (call-time global lookup)
for _live, _seam in SEAMED_SIBLINGS.items():
    if _seam == "apply_assistant_cache_control":
        _BASE_NS[_live] = ApplyCCSeam()
    else:
        _BASE_NS[_live] = SeamFn(_seam)
install_stubs()


def build_ns(rec):
    inp = rec["in"]
    CUR["scripts"] = inp.get("io") or {}
    CUR["trace"] = []
    CUR["raises"] = []
    CUR["idx"] = {}
    return _BASE_NS


# ── live driver ─────────────────────────────────────────────────────────────
def drive(rec, ns):
    func = rec["func"]
    inp = rec["in"]
    g = inp.get
    if func == "_block_type":
        return ns[func](g("b"))
    if func == "_has_block_type":
        return ns[func](g("blocks"), g("types"))
    if func == "_is_blank_text_block":
        return ns[func](g("b"))
    if func == "_cache_control_of":
        return ns[func](g("b"))
    if func == "_text_block":
        return ns[func](g("text"))
    if func == "_text_block_with_citations":
        return ns[func](g("text"), g("cits"))
    if func == "_parse_tool_args":
        return ns[func](g("raw"))
    if func == "_strip_thinking":
        return ns[func](g("blocks"))
    if func == "_block_ids":
        return ns[func](g("blocks"), g("btype"), g("key", "id"))
    if func == "_assistant_block_lists":
        return list(ns[func](g("result")))
    if func == "_tool_use_block":
        return ns[func](g("tool_id"), g("name"), g("tool_input"))
    if func == "_replay_ordered_blocks":
        return ns[func](g("m"), g("ordered_blocks"))
    if func == "assistant_replay_carrier":
        return ns[func](g("m"))
    if func == "_tool_result_content":
        return ns[func](g("m"))
    if func == "_concat_content":
        return ns[func](g("prev"), g("curr"))
    if func == "_keep_valid_thinking":
        return ns[func](g("content"), g("signature_dead", False))
    if func == "_current_tool_loop_start":
        return ns[func](g("result"))
    if func == "_image_source_data":
        return ns[func](g("block"))
    if func == "_convert_system_content":
        return ns[func](g("content"))
    if func == "_unprefix_oauth_tool_name":
        return ns[func](g("name"))
    if func == "response_finish_reason":
        resp = g("response") or {}
        response = types.SimpleNamespace(**resp)
        slf = types.SimpleNamespace(map_finish_reason=SeamFn("map_finish_reason"))
        return ns[func](slf, response)
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
            out = {"ret": {"__raise__": type(e).__name__, "msg": str(e)}, "trace": [], "state": {}}
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
    seam_names = set(SEAM_ARGS) | set(SEAMED_SIBLINGS.values())
    ported_names = set(PORTED)
    seen_funcs = set()
    for i, rec in enumerate(fixtures):
        seen_funcs.add(rec["func"])
        try:
            ns = build_ns(rec)
            try:
                ret = drive(rec, ns)
                spec = norm(ret)
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
        fx = Path(__file__).resolve().parent / "fixtures" / "anthropic_transport_flow.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").split("\n") if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()