#!/usr/bin/env python3
"""Generate tests/fixtures/anthropic_convert_flow2.jsonl — scripted universal-seam
cases for the 15 UNCLAIMED agent/anthropic_message_convert.py FLOW defs ported in
src/agent/anthropic_convert_flow2.c.

Each emitted case is {"func": <live_name>, "in": {...args..., "io": {seam scripts},
"env": {sibling-world env scripts}}}. >=5 cases per function cover the decision
branches, ordered-trace short-circuits, implicit raises, scripted seam raises
(incl. the BaseException trio), fail-closed (missing-script) propagation and $seq
queues. Inputs stay in the documented fixture domain (ASCII; scalar str()
operands; list-or-falsy tools; flat cache markers; non-negative retire counts).

SCRIPT DERIVATION. The claimed converter siblings are AUTO-SCRIPTED: the generator
drives the AST-extracted ported defs with AutoSeam recorders that call the REAL
quarry sibling (a fully live sibling namespace: every AMC def + the
schema_sanitizer / vision closures) and record each call's normalized result into
a per-seam $seq queue. Foreign policy seams (thinking route / replays, eviction
limit / retire, bedrock reasoning_minted, logger raises) are hand-scripted in io;
the sibling world's own bedrock / rasterizer calls consume in.env. After deriving
scripts, EVERY case is re-driven in pure scripted mode (the verification oracle's
own seams) and the two outputs (ret, trace, state) must be IDENTICAL — a
replayability self-check that catches aliasing / identity / ordering drift before
the fixture is emitted. auto=False cases are hand-scripted (fail-closed probes).

Run: python tests/gen_anthropic_convert_flow2_cases.py
"""
import ast
import builtins
import copy
import importlib.util
import json
import logging
import re
import sys
import types
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "anthropic_convert_flow2.jsonl"
QUARRY = HERE.parents[1] / "hermes-agent"
if not (QUARRY / "agent" / "anthropic_message_convert.py").is_file():
    QUARRY = Path.home() / "hermes-agent"

# ── load the verification oracle (reuse its extraction / norm / seams) ──────
_spec = importlib.util.spec_from_file_location("oracle_acv2", HERE / "oracle_anthropic_convert_flow2.py")
oracle = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(oracle)

import __future__
_FLAGS = __future__.annotations.compiler_flag
AMC_SRC = str(QUARRY / "agent" / "anthropic_message_convert.py")
AMC_TEXT = Path(AMC_SRC).read_text(encoding="utf-8")
AMC_TREE = ast.parse(AMC_TEXT)


def _compile(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


class SilentLogger:
    def debug(self, *a, **k): pass
    def info(self, *a, **k): pass
    def warning(self, *a, **k): pass
    def error(self, *a, **k): pass


# ── sibling world: the FULL live quarry module + pure dependency closures ───
def _register(name, ns):
    m = types.ModuleType(name)
    m.__dict__.update(ns)
    sys.modules[name] = m
    return m.__dict__


SIB = _register("acv2_sibling_world", {
    "__name__": "acv2_sibling_world", "__builtins__": builtins,
    "json": json, "copy": copy, "re": re, "logging": logging, "logger": SilentLogger(),
    "Any": object, "Dict": dict, "List": list, "Optional": object, "Tuple": tuple,
})
for _node in AMC_TREE.body:
    if isinstance(_node, ast.FunctionDef):
        exec(_compile(_node, AMC_SRC), SIB)
for _node in AMC_TREE.body:
    if isinstance(_node, (ast.Assign, ast.AnnAssign)):
        exec(_compile(_node, AMC_SRC), SIB)
# policy names are boundary seams in the ported world; fail-closed here (unused)
for _nm in ("anthropic_thinking_route", "route_replays_prior_thinking",
            "images_all_within_many_image_limit", "outbound_image_retire_count"):
    def _fc(*a, _n=_nm, **k):
        raise RuntimeError("sibling world must not call " + _n)
    SIB[_nm] = _fc

# tools.schema_sanitizer closure (strip_nullable_unions + helpers), live-extracted
SS_SRC = str(QUARRY / "tools" / "schema_sanitizer.py")
SS_TREE = ast.parse(Path(SS_SRC).read_text(encoding="utf-8"))
SS = {"__builtins__": builtins}
for _node in SS_TREE.body:
    if isinstance(_node, (ast.Assign, ast.AnnAssign)):
        _tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(_tg, ast.Name) and _tg.id in ("_UNION_KEYS", "_UNION_META_KEYS"):
            exec(_compile(_node, SS_SRC), SS)
for _node in SS_TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in (
            "strip_nullable_unions", "_rewrite", "_is_null_branch", "_carry_union_meta"):
        exec(_compile(_node, SS_SRC), SS)
_register("tools.schema_sanitizer", SS)

# tools.vision_tools_image_prep: live media-type gate; rasterizer is env-scripted
VI_SRC = str(QUARRY / "tools" / "vision_tools_image_prep.py")
VI_TREE = ast.parse(Path(VI_SRC).read_text(encoding="utf-8"))
VI = {"__builtins__": builtins}
for _node in VI_TREE.body:
    if isinstance(_node, (ast.Assign, ast.AnnAssign)):
        _tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(_tg, ast.Name) and _tg.id == "_ANTHROPIC_SUPPORTED_MEDIA_TYPES":
            exec(_compile(_node, VI_SRC), VI)
for _node in VI_TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name == "unsupported_inline_image_media_type":
        exec(_compile(_node, VI_SRC), VI)

# ── env scripting (sibling-world foreign calls; NOT part of the boundary trace) ─
ENV = {"scripts": {}, "idx": {}}


def env_record(name, args):
    if name not in ENV["scripts"]:
        raise RuntimeError("no env script for " + name)
    sc = ENV["scripts"][name]
    if isinstance(sc, dict) and "$seq" in sc:
        seq = sc["$seq"]
        i = ENV["idx"].get(name, 0)
        if i >= len(seq):
            raise RuntimeError("env script exhausted: " + name)
        ENV["idx"][name] = i + 1
        r = seq[i]
    else:
        r = sc
    if isinstance(r, dict) and isinstance(r.get("__raise__"), str):
        raise oracle.make_exc(r)
    return copy.deepcopy(r) if isinstance(r, (dict, list)) else r


VI["rasterize_svg_data_url"] = lambda url: env_record("rasterize_svg_data_url", {"url": url})
if "tools" not in sys.modules:
    _register("tools", {"__path__": []})
_tools = sys.modules["tools"]
_tools.schema_sanitizer = sys.modules["tools.schema_sanitizer"]
_register("tools.vision_tools_image_prep", VI)
_tools.vision_tools_image_prep = sys.modules["tools.vision_tools_image_prep"]

# bedrock: boundary SeamFn (io-scripted) for the ported world; env-backed for the
# sibling world. AutoSeam swaps the attribute around every live sibling call.
_BEDROCK = sys.modules["agent.bedrock_adapter"]
_BOUNDARY_MINTED = _BEDROCK.reasoning_minted_outside_claude
_ENV_MINTED = lambda msg: env_record("reasoning_minted_outside_claude", {"message": msg})

# ── auto seams ──────────────────────────────────────────────────────────────
AUTO_LIVE = {
    "block_type": ("_block_type", "value"),
    "has_block_type": ("_has_block_type", "value"),
    "text_block": ("_text_block", "value"),
    "strip_thinking": ("_strip_thinking", "value"),
    "block_ids": ("_block_ids", "set"),
    "assistant_block_lists": ("_assistant_block_lists", "abl"),
    "parse_tool_args": ("_parse_tool_args", "value"),
    "tool_use_block": ("_tool_use_block", "value"),
    "concat_content": ("_concat_content", "value"),
    "keep_valid_thinking": ("_keep_valid_thinking", "value"),
    "current_tool_loop_start": ("_current_tool_loop_start", "value"),
    "image_source_data": ("_image_source_data", "value"),
    "convert_system_content": ("_convert_system_content", "value"),
    "assistant_replay_carrier": ("assistant_replay_carrier", "value"),
    "tool_result_content": ("_tool_result_content", "value"),
    "carry_cache_control": ("_carry_cache_control", "carry_cc"),
    "split_blank_text_blocks": ("_split_blank_text_blocks", "value"),
    "sanitize_tool_id": ("_sanitize_tool_id", "value"),
    "normalize_tool_input_schema": ("_normalize_tool_input_schema", "value"),
    "convert_content_part_to_anthropic": ("_convert_content_part_to_anthropic", "value"),
    "apply_assistant_cache_control": ("_apply_assistant_cache_control_to_last_cacheable_block", "apply_cc"),
    "logger.warning": (None, "logger"),
}
AUTO_BUILT = set()


def _append_script(name, ret_n):
    io = oracle.CUR["scripts"]
    sc = io.get(name)
    if sc is None:
        io[name] = {"$seq": [ret_n]}
    elif isinstance(sc, dict) and "$seq" in sc:
        sc["$seq"].append(ret_n)
    else:
        raise RuntimeError("auto seam over explicit script: " + name)
    AUTO_BUILT.add(name)


class AutoSeam:
    def __init__(self, name):
        self.name = name
        self.live, self.kind = AUTO_LIVE[name]

    def __call__(self, *a, **kw):
        name = self.name
        io = oracle.CUR["scripts"]
        if name in io and name not in AUTO_BUILT:
            # explicit hand script: identical semantics to the verification oracle
            return oracle.record(name, oracle.SEAM_ARGS[name](a, kw))
        args_n = {k: oracle.norm(v) for k, v in oracle.SEAM_ARGS[name](a, kw).items()
                  if v is not None}
        if self.kind == "logger":
            _append_script(name, None)
            oracle.CUR["trace"].append({"ev": name, "args": args_n, "ret": None})
            return None
        fn = SIB[self.live]
        _BEDROCK.reasoning_minted_outside_claude = _ENV_MINTED
        try:
            try:
                if self.kind == "abl":
                    r = list(fn(a[0]))
                elif self.kind == "apply_cc":
                    r = fn(a[0], a[1])  # mutates live blocks; returns None
                elif self.kind == "carry_cc":
                    r = fn(a[0], a[1], **kw)  # mutates out, returns out
                else:
                    r = fn(*a, **kw)
            except BaseException as e:  # noqa: BLE001 - record the implicit sibling raise
                marker = {"__raise__": type(e).__name__, "msg": str(e)}
                _append_script(name, marker)
                oracle.CUR["trace"].append({"ev": name, "args": args_n, "ret": marker})
                raise
            ret_n = oracle.norm(r)
            _append_script(name, ret_n)
            oracle.CUR["trace"].append({"ev": name, "args": args_n, "ret": ret_n})
            if self.kind == "abl":
                return r  # LIVE (i, result[i]) pairs — generator identity
            if self.kind == "apply_cc":
                return r
            if self.kind in ("carry_cc", "set"):
                return copy.deepcopy(r)
            return copy.deepcopy(r) if isinstance(r, (dict, list)) else r
        finally:
            _BEDROCK.reasoning_minted_outside_claude = _BOUNDARY_MINTED


# install auto seams over the oracle's ported namespace (keep originals for replay)
_ORIG_SEAMS = {}
for _live, _seam in oracle.SEAMED_SIBLINGS.items():
    _ORIG_SEAMS[_live] = oracle._BASE_NS[_live]
    oracle._BASE_NS[_live] = AutoSeam(_seam)
_ORIG_SEAMS["logger"] = oracle._BASE_NS["logger"]
oracle._BASE_NS["logger"] = types.SimpleNamespace(warning=AutoSeam("logger.warning"))


def _restore_oracle_seams():
    for _live, _obj in _ORIG_SEAMS.items():
        if _live == "logger":
            oracle._BASE_NS["logger"] = _obj
        else:
            oracle._BASE_NS[_live] = _obj


def _install_auto_seams():
    for _live, _seam in oracle.SEAMED_SIBLINGS.items():
        oracle._BASE_NS[_live] = AutoSeam(_seam)
    oracle._BASE_NS["logger"] = types.SimpleNamespace(warning=AutoSeam("logger.warning"))


# ── case table ──────────────────────────────────────────────────────────────
CASES = []


def case(func, auto=True, env=None, **inp):
    CASES.append({"func": func, "auto": auto, "env": env or {}, "in": inp})


def io(**scripts):
    return scripts


RAISE_VE = {"__raise__": "ValueError", "msg": "boom"}
RAISE_SE = {"__raise__": "SystemExit", "msg": "1"}
RAISE_KI = {"__raise__": "KeyboardInterrupt", "msg": ""}
RAISE_GE = {"__raise__": "GeneratorExit", "msg": ""}
RAISE_RE = {"__raise__": "RuntimeError", "msg": "x"}
CC = {"type": "ephemeral"}

# ── 1. _extract_preserved_thinking_blocks ───────────────────────────────────
case("_extract_preserved_thinking_blocks",
     message={"reasoning_details": [{"type": "thinking", "thinking": "t", "signature": "s"},
                                    {"type": "redacted_thinking", "data": "d"},
                                    {"type": "other"}]},
     io=io(reasoning_minted_outside_claude=False))
case("_extract_preserved_thinking_blocks", message={"reasoning_details": "x"}, io=io())
case("_extract_preserved_thinking_blocks", message={}, io=io())
case("_extract_preserved_thinking_blocks",
     message={"reasoning_details": [{"type": " Thinking ", "thinking": "t"}, {"type": None},
                                    {"type": 0}, 5, {"type": "redacted_thinking", "data": "d"}]},
     io=io(reasoning_minted_outside_claude=False))
case("_extract_preserved_thinking_blocks",
     message={"reasoning_details": [{"type": "thinking"}]},
     io=io(reasoning_minted_outside_claude=True))
case("_extract_preserved_thinking_blocks", message=None, io=io())
case("_extract_preserved_thinking_blocks",
     message={"reasoning_details": []}, io=io(reasoning_minted_outside_claude=RAISE_KI))
case("_extract_preserved_thinking_blocks",
     message={"reasoning_details": [{"type": "thinking", "thinking": "t"}]}, io=io())

# ── 2. convert_tools_to_anthropic ───────────────────────────────────────────
case("convert_tools_to_anthropic",
     tools=[{"function": {"name": "a", "description": "d",
                          "parameters": {"type": "object", "properties": {}}}}])
case("convert_tools_to_anthropic", tools=[{"function": {"name": "a"}}, {"function": {"name": "a"}}])
case("convert_tools_to_anthropic", tools=[{"function": {"name": ""}}, {"function": {"name": ""}}])
case("convert_tools_to_anthropic", tools=None)
case("convert_tools_to_anthropic", tools=[])
case("convert_tools_to_anthropic",
     tools=[{"function": {"name": "a", "parameters": {"anyOf": [{"type": "string"}, {"type": "null"}]}}}])
case("convert_tools_to_anthropic",
     tools=[{"function": {"name": "a"}, "cache_control": CC}])
case("convert_tools_to_anthropic", tools=[5])
case("convert_tools_to_anthropic", tools=[{"function": 5}])
case("convert_tools_to_anthropic", tools=[{"function": {"name": "a"}}],
     io=io(normalize_tool_input_schema=RAISE_VE))
case("convert_tools_to_anthropic", tools=5)
case("convert_tools_to_anthropic", auto=False, tools=[{"function": {"name": "a"}}], io=io())
case("convert_tools_to_anthropic",
     tools=[{"function": {"name": "a", "parameters": None}}, {"function": {"name": "b", "description": "x"}}])

# ── 3. _convert_content_to_anthropic ────────────────────────────────────────
case("_convert_content_to_anthropic", content="str")
case("_convert_content_to_anthropic", content=[{"type": "text", "text": "hi"}])
case("_convert_content_to_anthropic", content=[None, {"type": "text", "text": "x"}])
case("_convert_content_to_anthropic", content=[])
case("_convert_content_to_anthropic", content=5)
case("_convert_content_to_anthropic", content=[{"type": "text", "text": "hi"}],
     io=io(convert_content_part_to_anthropic=RAISE_SE))
case("_convert_content_to_anthropic", content=[{"type": "text", "text": "hi"}],
     io=io(convert_content_part_to_anthropic={}))
case("_convert_content_to_anthropic", content=None)

# ── 4. _content_parts_to_anthropic_blocks ───────────────────────────────────
case("_content_parts_to_anthropic_blocks",
     parts=[{"type": "text", "text": "hi"},
            {"type": "image_url", "image_url": {"url": "http://x/y.png"}}])
case("_content_parts_to_anthropic_blocks", parts="x")
case("_content_parts_to_anthropic_blocks", parts=[{"type": "text", "text": ""}])
case("_content_parts_to_anthropic_blocks", parts=[{"type": "image", "source": {}}])
case("_content_parts_to_anthropic_blocks", parts=[{"type": "text", "text": "a"}, None])
case("_content_parts_to_anthropic_blocks", parts=[{"type": "text", "text": "hi"}],
     io=io(convert_content_part_to_anthropic="truthy-str"))
case("_content_parts_to_anthropic_blocks", parts=[{"type": "text", "text": "hi"}],
     io=io(text_block=RAISE_GE))
case("_content_parts_to_anthropic_blocks",
     parts=[{"type": "image_url", "image_url": {"url": "data:image/svg+xml,<svg/>"}}],
     env={"rasterize_svg_data_url": None})
case("_content_parts_to_anthropic_blocks",
     parts=[{"type": "image_url", "image_url": {"url": "data:image/png;base64,QUJD"}}])

# ── 5. _convert_assistant_message ───────────────────────────────────────────
case("_convert_assistant_message", m={"content": "hello"},
     io=io(assistant_replay_carrier=[False, []]))
case("_convert_assistant_message", m={"content": "  "},
     io=io(assistant_replay_carrier=[False, []]))
case("_convert_assistant_message", m={"content": [{"type": "text", "text": "hi"}]},
     io=io(assistant_replay_carrier=[False, []]))
case("_convert_assistant_message",
     m={"content": [{"type": "text", "text": "  ", "cache_control": CC}]},
     io=io(assistant_replay_carrier=[False, []]))
case("_convert_assistant_message",
     m={"anthropic_content_blocks": [{"type": "text", "text": "hi"}]},
     io=io(assistant_replay_carrier=[True, [{"type": "text", "text": "hi"}]]))
case("_convert_assistant_message",
     m={"content": "x", "tool_calls": [{"id": "t.1", "function": {"name": "f", "arguments": '{"a": 1}'}}]},
     io=io(assistant_replay_carrier=[False, []]))
case("_convert_assistant_message", m={"content": "x", "reasoning_content": "think"},
     io=io(assistant_replay_carrier=[False, []]))
case("_convert_assistant_message",
     m={"content": "x", "reasoning_content": "think"},
     io=io(assistant_replay_carrier=[True, [{"type": "thinking", "thinking": "t", "signature": "s"}]]))
case("_convert_assistant_message", m={"content": "x", "cache_control": {"type": "top"}},
     io=io(assistant_replay_carrier=[False, []]))
case("_convert_assistant_message", m={"content": "x", "tool_calls": None},
     io=io(assistant_replay_carrier=[False, []]))
case("_convert_assistant_message", m={"content": "x", "tool_calls": [{"id": "t", "function": 5}]},
     io=io(assistant_replay_carrier=[False, []]))
case("_convert_assistant_message", m={"content": "x"},
     io=io(assistant_replay_carrier=[False, None]))
case("_convert_assistant_message", m={"content": [{"type": "text", "text": "x"}]},
     io=io(assistant_replay_carrier=[False, None]))
case("_convert_assistant_message", m={"content": "x"},
     io=io(assistant_replay_carrier=RAISE_KI))
case("_convert_assistant_message", m=5)
case("_convert_assistant_message", auto=False, m={"content": "x"}, io=io())
case("_convert_assistant_message",
     m={"content": "x", "tool_calls": [None, 0, {"id": "t2", "function": {"name": "g"}}]},
     io=io(assistant_replay_carrier=[False, []]))

# ── 6. _convert_tool_message_to_result ──────────────────────────────────────
case("_convert_tool_message_to_result", result=[], m={"tool_call_id": "t1", "content": "out"})
case("_convert_tool_message_to_result",
     result=[{"role": "user", "content": [{"type": "tool_result", "tool_use_id": "t0", "content": "x"}]}],
     m={"tool_call_id": "t1", "content": "y"})
case("_convert_tool_message_to_result", result=[{"role": "user", "content": "str"}],
     m={"tool_call_id": "t1", "content": "out"})
case("_convert_tool_message_to_result", result=[{"role": "assistant", "content": []}],
     m={"tool_call_id": "t1", "content": "out"})
case("_convert_tool_message_to_result", result=[],
     m={"tool_call_id": "t.1", "content": "out", "cache_control": CC})
case("_convert_tool_message_to_result", result=[],
     m={"tool_call_id": "t1", "content": {"_multimodal": True, "content": [{"type": "text", "text": "x"}]}})
case("_convert_tool_message_to_result", result=[5], m={"tool_call_id": "t1", "content": "out"})
case("_convert_tool_message_to_result", result=[{"role": "user", "content": [5]}],
     m={"tool_call_id": "t1", "content": "out"})
case("_convert_tool_message_to_result", result=[], m={"tool_call_id": "t1", "content": "x"},
     io=io(sanitize_tool_id=RAISE_VE))
case("_convert_tool_message_to_result", result=[], m=5)
case("_convert_tool_message_to_result", auto=False, result=[],
     m={"tool_call_id": "t", "content": "x"}, io=io())

# ── 7. _convert_user_message ────────────────────────────────────────────────
case("_convert_user_message", content=[{"type": "text", "text": "hi"}])
case("_convert_user_message", content=[{"type": "text", "text": "  "}])
case("_convert_user_message", content="hello")
case("_convert_user_message", content="   ")
case("_convert_user_message", content=None)
case("_convert_user_message", content=0)
case("_convert_user_message", content={"a": 1})
case("_convert_user_message", content=[{"type": "text", "text": "hi"}],
     io=io(convert_content_part_to_anthropic=RAISE_GE))

# ── 8. _strip_orphaned_tool_blocks ──────────────────────────────────────────
case("_strip_orphaned_tool_blocks",
     result=[{"role": "assistant", "content": [{"type": "tool_use", "id": "t1", "name": "f", "input": {}}]},
             {"role": "user", "content": [{"type": "tool_result", "tool_use_id": "t1", "content": "ok"}]}])
case("_strip_orphaned_tool_blocks",
     result=[{"role": "assistant", "content": [{"type": "tool_use", "id": "t1", "name": "f", "input": {}}]},
             {"role": "user", "content": "hi"}])
case("_strip_orphaned_tool_blocks",
     result=[{"role": "assistant", "content": [{"type": "thinking", "thinking": "t", "signature": "s"},
                                               {"type": "tool_use", "id": "t1", "name": "f", "input": {}}]},
             {"role": "user", "content": "hi"}])
case("_strip_orphaned_tool_blocks",
     result=[{"role": "assistant", "content": [{"type": "tool_use", "id": "t1", "name": "f", "input": {}}]},
             {"role": "user", "content": "x"},
             {"role": "user", "content": [{"type": "tool_result", "tool_use_id": "t1", "content": "ok"}]}])
case("_strip_orphaned_tool_blocks", result=[])
case("_strip_orphaned_tool_blocks",
     result=[{"role": "user", "content": [{"type": "tool_result", "tool_use_id": "ghost", "content": "x"}]}])
case("_strip_orphaned_tool_blocks",
     result=[{"role": "assistant", "content": [{"type": "text", "text": "hi"}]}])
case("_strip_orphaned_tool_blocks",
     result=[{"role": "assistant", "content": [{"type": "tool_use", "id": "t1", "name": "f", "input": {}}]},
             {"role": "user", "content": [{"type": "tool_result", "tool_use_id": "t1", "content": "ok"}]}],
     io=io(block_ids=RAISE_SE))
case("_strip_orphaned_tool_blocks", result=[5])

# ── 9. _merge_consecutive_roles ─────────────────────────────────────────────
case("_merge_consecutive_roles",
     result=[{"role": "user", "content": "a"}, {"role": "assistant", "content": "b"}])
case("_merge_consecutive_roles",
     result=[{"role": "user", "content": "a"}, {"role": "user", "content": "b"}])
case("_merge_consecutive_roles",
     result=[{"role": "assistant", "content": [{"type": "thinking", "thinking": "t"}, {"type": "text", "text": "a"}]},
             {"role": "assistant", "content": [{"type": "thinking", "thinking": "t2"}, {"type": "text", "text": "b"}]}])
case("_merge_consecutive_roles",
     result=[{"role": "assistant", "content": "a", "_thinking_signature_invalidated": True},
             {"role": "assistant", "content": "b"}])
case("_merge_consecutive_roles",
     result=[{"role": "user", "content": "a"}, {"role": "user", "content": "b"}, {"role": "user", "content": "c"}])
case("_merge_consecutive_roles", result=[{"content": "a"}, {"role": "user", "content": "b"}])
case("_merge_consecutive_roles", result=[{"role": "user"}, {"role": "user", "content": "b"}])
case("_merge_consecutive_roles",
     result=[{"role": "assistant", "content": "a"}, {"role": "assistant", "content": "b"}],
     io=io(concat_content=RAISE_VE))
case("_merge_consecutive_roles", result=[5])
case("_merge_consecutive_roles", result=[{"role": "user", "content": "a"}, 5])

# ── 10. _manage_thinking_signatures ─────────────────────────────────────────
case("_manage_thinking_signatures",
     result=[{"role": "assistant", "content": [{"type": "thinking", "thinking": "t", "signature": "s", "cache_control": CC},
                                               {"type": "text", "text": "x"}]}],
     base_url=None, model=None,
     io=io(anthropic_thinking_route="native", route_replays_prior_thinking=True))
case("_manage_thinking_signatures",
     result=[{"role": "assistant", "content": [{"type": "thinking", "thinking": "t", "signature": "s", "cache_control": CC},
                                               {"type": "text", "text": "x"}], "_thinking_signature_invalidated": True}],
     io=io(anthropic_thinking_route="kimi", route_replays_prior_thinking=True))
case("_manage_thinking_signatures",
     result=[{"role": "assistant", "content": [{"type": "thinking", "thinking": "t", "signature": "s"},
                                               {"type": "redacted_thinking", "data": "d"},
                                               {"type": "thinking", "thinking": "u"}]}],
     io=io(anthropic_thinking_route="deepseek", route_replays_prior_thinking=False))
case("_manage_thinking_signatures",
     result=[{"role": "assistant", "content": [{"type": "thinking", "thinking": "t", "signature": "s"}]}],
     io=io(anthropic_thinking_route="third_party", route_replays_prior_thinking=False))
case("_manage_thinking_signatures",
     result=[{"role": "user", "content": "hi"},
             {"role": "assistant", "content": [{"type": "thinking", "thinking": "t", "signature": "s"}]}],
     io=io(anthropic_thinking_route="native", route_replays_prior_thinking=False))
case("_manage_thinking_signatures",
     result=[{"role": "assistant", "content": [{"type": "thinking", "thinking": "t", "signature": "s"}]},
             {"role": "user", "content": "hi"}],
     io=io(anthropic_thinking_route="native", route_replays_prior_thinking=False))
case("_manage_thinking_signatures",
     result=[{"role": "assistant", "content": [{"type": "thinking", "thinking": "t"}], "_thinking_signature_invalidated": True}],
     io=io(anthropic_thinking_route="native", route_replays_prior_thinking=True))
case("_manage_thinking_signatures",
     result=[{"role": "assistant", "content": [{"type": "redacted_thinking"}]}],
     io=io(anthropic_thinking_route="native", route_replays_prior_thinking=True))
case("_manage_thinking_signatures", result=[{"role": "assistant", "content": []}],
     io=io(anthropic_thinking_route="native", route_replays_prior_thinking=True))
case("_manage_thinking_signatures",
     result=[{"role": "assistant", "content": [{"type": "thinking", "thinking": "t", "signature": "s"}]}],
     io=io(anthropic_thinking_route=RAISE_KI))
case("_manage_thinking_signatures", auto=False,
     result=[{"role": "assistant", "content": [{"type": "thinking", "thinking": "t", "signature": "s"}]}],
     io=io(anthropic_thinking_route="native"))
case("_manage_thinking_signatures",
     result=[{"role": "assistant", "content": [{"type": "thinking", "thinking": "t", "signature": "s"}]}],
     io=io(anthropic_thinking_route="deepseek", route_replays_prior_thinking=False))

# ── 11. _evict_old_screenshots ──────────────────────────────────────────────
case("_evict_old_screenshots",
     result=[{"role": "user", "content": [{"type": "image", "source": {"type": "base64", "media_type": "image/png", "data": "abc"}}]}],
     io=io(images_all_within_many_image_limit=True, outbound_image_retire_count=0))
case("_evict_old_screenshots",
     result=[{"role": "user", "content": [{"type": "tool_result", "tool_use_id": "t",
                                           "content": [{"type": "image", "source": {"data": "aa"}}, {"type": "text", "text": "x"}]}]}],
     io=io(images_all_within_many_image_limit=True, outbound_image_retire_count=1))
case("_evict_old_screenshots",
     result=[{"role": "user", "content": [{"type": "tool_result", "tool_use_id": "t",
                                           "content": [{"type": "image", "source": {"data": "aa"}}]}]}],
     io=io(images_all_within_many_image_limit=True, outbound_image_retire_count=0))
case("_evict_old_screenshots",
     result=[{"role": "user", "content": [{"type": "tool_result", "content": [{"type": "image", "source": {"data": "aa"}}]},
                                          {"type": "tool_result", "content": [{"type": "image", "source": {"data": "bb"}}]}]}],
     io=io(images_all_within_many_image_limit=False, outbound_image_retire_count=1))
case("_evict_old_screenshots",
     result=[{"role": "user", "content": [{"type": "image", "source": {"type": "url", "url": "u"}}]}],
     io=io(images_all_within_many_image_limit=True, outbound_image_retire_count=0))
case("_evict_old_screenshots", result=[5], io=io())
case("_evict_old_screenshots",
     result=[{"role": "user", "content": [{"type": "tool_result", "content": [{"type": "image", "source": {"data": "aa"}}]}]}],
     io=io(images_all_within_many_image_limit=RAISE_VE))
case("_evict_old_screenshots", result=[],
     io=io(images_all_within_many_image_limit=True, outbound_image_retire_count=0))
case("_evict_old_screenshots",
     result=[{"role": "user", "content": [{"type": "tool_result",
                                           "content": [{"type": "image", "source": {"data": "aa"}},
                                                       {"type": "image", "source": {"data": "bbb"}}]}]}],
     io=io(images_all_within_many_image_limit=True, outbound_image_retire_count=1))

# ── 12. _ensure_leading_user_turn ───────────────────────────────────────────
case("_ensure_leading_user_turn", result=[])
case("_ensure_leading_user_turn", result=[{"role": "user", "content": "x"}])
case("_ensure_leading_user_turn", result=[{"role": "assistant", "content": "x"}])
case("_ensure_leading_user_turn", result=[{"role": "system", "content": "x"}])
case("_ensure_leading_user_turn", result=[5])
case("_ensure_leading_user_turn", result=[{"role": "assistant", "content": "x"}],
     io=io(text_block=RAISE_SE))
case("_ensure_leading_user_turn", result=[{"content": "x"}])

# ── 13. _fix_blank_text_blocks_in_list ──────────────────────────────────────
case("_fix_blank_text_blocks_in_list", blocks=[{"type": "text", "text": "hi"}],
     placeholder_text="(empty)", msg_index=0, role="assistant", location="content")
case("_fix_blank_text_blocks_in_list", blocks=[{"type": "text", "text": "  "}, {"type": "text", "text": "hi"}],
     placeholder_text="(empty)", msg_index=1, role="user", location="content")
case("_fix_blank_text_blocks_in_list", blocks=[{"type": "text", "text": "  ", "cache_control": CC}],
     placeholder_text="(empty)", msg_index=0, role="assistant", location="content")
case("_fix_blank_text_blocks_in_list", blocks=[{"type": "image", "source": {}}],
     placeholder_text="(empty message)", msg_index=2, role="user", location="content")
case("_fix_blank_text_blocks_in_list", blocks=[],
     placeholder_text="(no output)", msg_index=0, role="user", location="tool_result")
case("_fix_blank_text_blocks_in_list", blocks=[{"type": "text", "text": "  "}],
     placeholder_text="(empty)", msg_index=0, role="assistant", location="content",
     io=io(split_blank_text_blocks=RAISE_VE))
case("_fix_blank_text_blocks_in_list", auto=False, blocks=[{"type": "text", "text": "  "}],
     placeholder_text="(empty)", msg_index=0, role="assistant", location="content",
     io=io(split_blank_text_blocks=[[], None, [0]]))
case("_fix_blank_text_blocks_in_list",
     blocks=[{"type": "text", "text": "hi"}, {"type": "text", "text": "  ", "cache_control": CC}],
     placeholder_text="(empty)", msg_index=3, role=None, location="content")

# ── 14. _scrub_blank_text_blocks ────────────────────────────────────────────
case("_scrub_blank_text_blocks", result=[{"role": "assistant", "content": [{"type": "text", "text": "  "}]}])
case("_scrub_blank_text_blocks", result=[{"role": "user", "content": [{"type": "text", "text": "  "}]}])
case("_scrub_blank_text_blocks",
     result=[{"role": "user", "content": [{"type": "tool_result", "tool_use_id": "t",
                                           "content": [{"type": "text", "text": "  "}]}]}])
case("_scrub_blank_text_blocks", result=[5, {"role": "user", "content": [{"type": "text", "text": "  "}]}])
case("_scrub_blank_text_blocks", result=[{"role": "user", "content": []}])
case("_scrub_blank_text_blocks", result=[{"role": "user", "content": "str"}])
case("_scrub_blank_text_blocks",
     result=[{"role": "user", "content": [{"type": "tool_result", "content": [{"type": "text", "text": "hi"}]}]}])
case("_scrub_blank_text_blocks", result=[{"role": "assistant", "content": [{"type": "text", "text": "  "}]}],
     io=io(split_blank_text_blocks=RAISE_GE))

# ── 15. convert_messages_to_anthropic ───────────────────────────────────────
case("convert_messages_to_anthropic",
     messages=[{"role": "system", "content": "sys"}, {"role": "user", "content": "hi"}],
     base_url=None, model=None,
     io=io(anthropic_thinking_route="native", route_replays_prior_thinking=True,
           images_all_within_many_image_limit=True, outbound_image_retire_count=0))
case("convert_messages_to_anthropic",
     messages=[{"role": "user", "content": "a"},
               {"role": "assistant", "content": "b", "tool_calls": [{"id": "t1", "function": {"name": "f", "arguments": "{}"}}]},
               {"role": "tool", "tool_call_id": "t1", "content": "ok"}],
     io=io(anthropic_thinking_route="native", route_replays_prior_thinking=True,
           images_all_within_many_image_limit=True, outbound_image_retire_count=0))
case("convert_messages_to_anthropic", messages=[],
     io=io(anthropic_thinking_route="native", route_replays_prior_thinking=True,
           images_all_within_many_image_limit=True, outbound_image_retire_count=0))
case("convert_messages_to_anthropic", messages=[{"role": "assistant", "content": "x"}],
     io=io(anthropic_thinking_route="native", route_replays_prior_thinking=True,
           images_all_within_many_image_limit=True, outbound_image_retire_count=0))
case("convert_messages_to_anthropic",
     messages=[{"role": "user", "content": "a"}, {"role": "user", "content": "b"}],
     io=io(anthropic_thinking_route="native", route_replays_prior_thinking=True,
           images_all_within_many_image_limit=True, outbound_image_retire_count=0))
case("convert_messages_to_anthropic",
     messages=[{"role": "system", "content": [{"type": "text", "text": "a", "cache_control": CC}]}],
     io=io(anthropic_thinking_route="native", route_replays_prior_thinking=True,
           images_all_within_many_image_limit=True, outbound_image_retire_count=0))
case("convert_messages_to_anthropic", messages=[{"role": "user", "content": "hi"}],
     io=io(anthropic_thinking_route=RAISE_VE))
case("convert_messages_to_anthropic", messages=[5])
case("convert_messages_to_anthropic",
     messages=[{"role": "assistant", "content": "x", "reasoning_content": "r"}, {"role": "user", "content": "q"}],
     io=io(anthropic_thinking_route="kimi", route_replays_prior_thinking=True,
           images_all_within_many_image_limit=True, outbound_image_retire_count=0))
case("convert_messages_to_anthropic",
     messages=[{"role": "user", "content": [{"type": "image_url", "image_url": {"url": "http://x/y.png"}}]}],
     io=io(anthropic_thinking_route="native", route_replays_prior_thinking=True,
           images_all_within_many_image_limit=True, outbound_image_retire_count=0))


# ── derivation + replayability self-check ───────────────────────────────────
def derive_and_check():
    emitted = []
    failures = 0
    for i, c in enumerate(CASES):
        rec = {"func": c["func"], "in": dict(c["in"])}
        rec["in"]["io"] = copy.deepcopy(c["in"].get("io") or {})
        rec["in"]["env"] = copy.deepcopy(c["env"])
        AUTO_BUILT.clear()
        if c["auto"]:
            # Auto drive runs the REAL siblings, whose results share objects with
            # the inputs (kept blocks, carrier lists); later state-effect seams
            # then mutate the fixture input in place. Drive on a THROWAWAY copy
            # so the emitted fixture stays pristine; the derived scripts are
            # norm snapshots taken at call time (pre-mutation), so replaying them
            # against the pristine input reproduces the auto drive exactly.
            drive_rec = copy.deepcopy(rec)
            ENV["scripts"] = drive_rec["in"]["env"]
            ENV["idx"] = {}
            _install_auto_seams()
            out_auto = oracle.run_case(drive_rec)
            # build_ns's `io or {}` may have replaced an EMPTY io with a fresh
            # dict; capture the scripts dict the auto drive actually wrote to.
            rec["in"]["io"] = oracle.CUR["scripts"]
            # replay with the verification oracle's own seams and the derived scripts
            _restore_oracle_seams()
            ENV["scripts"] = rec["in"]["env"]
            ENV["idx"] = {}
            out_replay = oracle.run_case(rec)
            if out_auto != out_replay:
                failures += 1
                print(f"REPLAY DIVERGENCE case {i} {c['func']}:")
                print(f"  AUTO  ={json.dumps(out_auto, ensure_ascii=False)[:700]}")
                print(f"  REPLAY={json.dumps(out_replay, ensure_ascii=False)[:700]}")
                continue
        else:
            ENV["scripts"] = rec["in"]["env"]
            ENV["idx"] = {}
            _restore_oracle_seams()
            out_replay = oracle.run_case(rec)
            # hand-scripted cases must not fail-closed unexpectedly unless intended
            _ = out_replay
        emitted.append({"func": c["func"], "in": rec["in"]})
    return emitted, failures


def main():
    emitted, failures = derive_and_check()
    if failures:
        raise SystemExit(f"{failures} replay divergences — fixtures NOT written")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in emitted:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    from collections import Counter
    cnt = Counter(c["func"] for c in emitted)
    print(f"wrote {len(emitted)} cases -> {OUT}")
    for fn, n in sorted(cnt.items()):
        flag = "" if n >= 5 else "  <-- BELOW 5"
        print(f"  {fn}: {n}{flag}")
    below = [fn for fn, n in cnt.items() if n < 5]
    if below:
        raise SystemExit(f"funcs below 5 cases: {below}")
    missing = set(oracle.PORTED) - set(cnt)
    if missing:
        raise SystemExit(f"funcs with no cases: {sorted(missing)}")


if __name__ == "__main__":
    main()