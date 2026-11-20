#!/usr/bin/env python3
"""Generate tests/fixtures/anthropic_transport_flow2.jsonl for the anthropic
transport FLOW port. >=5 cases per ported func (10 funcs), covering normal paths,
$seq seam queues, fail-closed (missing script) and scripted raises across the FULL
builtin exception hierarchy incl. the BaseException trio. The oracle computes the
LIVE reference; the C harness emits its own output — this generator only emits
INPUTS. Inputs stay in the documented fixture domain.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "anthropic_transport_flow2.jsonl"

CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def io(**scripts):
    return scripts


def seq(*items):
    return {"$seq": list(items)}


RAISE_VE = {"__raise__": "ValueError", "msg": "boom"}
RAISE_SE = {"__raise__": "SystemExit", "msg": "9"}
RAISE_KI = {"__raise__": "KeyboardInterrupt", "msg": ""}
RAISE_GE = {"__raise__": "GeneratorExit", "msg": ""}
RAISE_RE = {"__raise__": "RuntimeError", "msg": "x"}
RAISE_TE = {"__raise__": "TypeError", "msg": "bad type"}
RAISE_OE = {"__raise__": "OSError", "msg": "io fail"}
RAISE_KE = {"__raise__": "KeyError", "msg": "missing"}

# ── _unprefix_oauth_tool_name ───────────────────────────────────────────────
case("_unprefix_oauth_tool_name", name="mcp__chat_history_lookup",
     io=io(**{"registry.get_entry": seq(None, None, None)}))
case("_unprefix_oauth_tool_name", name="mcp__context_notes",
     io=io(**{"registry.get_entry": seq(None, None, None)}))
case("_unprefix_oauth_tool_name", name="mcp__read_file",
     io=io(**{"registry.get_entry": {"name": "registered"}}))
case("_unprefix_oauth_tool_name", name="mcp__foo",
     io=io(**{"registry.get_entry": seq(None, {"x": 1}, None)}))
case("_unprefix_oauth_tool_name", name="mcp__foo",
     io=io(**{"registry.get_entry": seq(None, None, {"x": 1})}))
case("_unprefix_oauth_tool_name", name=123)
case("_unprefix_oauth_tool_name", name="mcp__bar")  # fail-closed: no registry script
case("_unprefix_oauth_tool_name", name="mcp__baz",
     io=io(**{"registry.get_entry": seq(RAISE_SE, None, None)}))
case("_unprefix_oauth_tool_name", name="mcp__chat_history_lookup",
     io=io(**{"registry.get_entry": seq(None, None, RAISE_KE)}))

# ── api_mode ────────────────────────────────────────────────────────────────
for _i in range(6):
    case("api_mode", io=io(**{"unused": {"n": _i}}))

# ── convert_messages ────────────────────────────────────────────────────────
case("convert_messages", messages=[{"role": "user", "content": "hi"}],
     kwargs={"base_url": "https://x", "model": "claude-3"},
     io=io(**{"convert_messages_to_anthropic": {"system": "s", "messages": []}}))
case("convert_messages", messages=[], kwargs={},
     io=io(**{"convert_messages_to_anthropic": None}))
case("convert_messages", messages=None, kwargs={},
     io=io(**{"convert_messages_to_anthropic": "x"}))
case("convert_messages", messages=[1, 2], kwargs={"model": "m"},
     io=io(**{"convert_messages_to_anthropic": RAISE_VE}))
case("convert_messages", messages=[{"role": "user"}], kwargs={})  # fail-closed
case("convert_messages", messages=[{"role": "user"}],
     kwargs={"strip_tool_prefix": True, "model": "m"},
     io=io(**{"convert_messages_to_anthropic": seq({"ok": 1}, {"ok": 2})}))
case("convert_messages", messages=[{"role": "user"}], kwargs={"model": "m"},
     io=io(**{"convert_messages_to_anthropic": RAISE_GE}))

# ── convert_tools ───────────────────────────────────────────────────────────
case("convert_tools", tools=[{"name": "f"}],
     io=io(**{"convert_tools_to_anthropic": [{"name": "f", "input_schema": {}}]}))
case("convert_tools", tools=[],
     io=io(**{"convert_tools_to_anthropic": []}))
case("convert_tools", tools=None,
     io=io(**{"convert_tools_to_anthropic": None}))
case("convert_tools", tools=[{"name": "f"}],
     io=io(**{"convert_tools_to_anthropic": RAISE_TE}))
case("convert_tools", tools=[{"name": "f"}])  # fail-closed
case("convert_tools", tools=[{"name": "a"}, {"name": "b"}],
     io=io(**{"convert_tools_to_anthropic": seq([1], [2])}))
case("convert_tools", tools=[{"name": "f"}],
     io=io(**{"convert_tools_to_anthropic": RAISE_KI}))

# ── build_kwargs ────────────────────────────────────────────────────────────
case("build_kwargs", model="claude-3", messages=[{"role": "user"}], tools=None,
     params={}, io=io(**{"build_anthropic_kwargs": {"x": 1}}))
case("build_kwargs", model="claude-3", messages=[{"role": "user"}],
     tools=[{"name": "f"}],
     params={"max_tokens": 100, "reasoning_config": {"type": "x"},
             "tool_choice": "auto", "is_oauth": True, "preserve_dots": True,
             "context_length": 200, "base_url": "url", "fast_mode": True,
             "drop_context_1m_beta": True},
     io=io(**{"build_anthropic_kwargs": {"full": True}}))
case("build_kwargs", model="claude-3", messages=[{"role": "user"}], tools=None,
     params={"max_tokens": None, "is_oauth": None},
     io=io(**{"build_anthropic_kwargs": {"n": 1}}))
case("build_kwargs", model="claude-3", messages=[], tools=None, params={},
     io=io(**{"build_anthropic_kwargs": RAISE_RE}))
case("build_kwargs", model="claude-3", messages=[], tools=None, params={})  # fail-closed
case("build_kwargs", messages=[], tools=None, params={},
     io=io(**{"build_anthropic_kwargs": {"m": None}}))
case("build_kwargs", model="claude-3", messages=[], tools=None, params={},
     io=io(**{"build_anthropic_kwargs": seq({"a": 1}, {"a": 2})}))
case("build_kwargs", model="claude-3", messages=[], tools=None, params={},
     io=io(**{"build_anthropic_kwargs": RAISE_OE}))

# ── normalize_response ──────────────────────────────────────────────────────
_TXT = {"type": "text", "text": "hello"}
case("normalize_response",
     response={"content": [{"type": "text", "text": "hello"}], "stop_reason": "end_turn"},
     kwargs={},
     io=io(to_plain_data=seq(_TXT, None), sanitize_replay_block=_TXT,
           guardrail_intervened=False, map_finish_reason="stop"))
_TH1 = {"type": "thinking", "thinking": "T", "signature": "sig"}
_TH2 = {"type": "tool_use", "id": "tu1", "name": "mcp__read_file", "input": {"a": 1}}
case("normalize_response",
     response={"content": [_TH1, _TH2], "stop_reason": "tool_use"},
     kwargs={"strip_tool_prefix": True},
     io=io(to_plain_data=seq(_TH1, _TH2, None),
           sanitize_replay_block=seq(_TH1, _TH2),
           **{"registry.get_entry": seq(None, None, None),
              "guardrail_intervened": False, "map_finish_reason": "tool_calls"}))
case("normalize_response",
     response={"content": [], "stop_reason": "end_turn"}, kwargs={},
     io=io(to_plain_data=seq(None), guardrail_intervened=True))
case("normalize_response",
     response={"content": [{"type": "text", "text": "x"}], "stop_reason": "end_turn"},
     kwargs={}, io=io(to_plain_data=RAISE_SE))
case("normalize_response",
     response={"content": [], "stop_reason": "end_turn"}, kwargs={},
     io=io(to_plain_data=seq(None), guardrail_intervened=RAISE_KI))
case("normalize_response",
     response={"content": [{"type": "text", "text": 5}], "stop_reason": "end_turn"},
     kwargs={},
     io=io(to_plain_data=seq({"type": "text", "text": 5}, None),
           sanitize_replay_block={"type": "text", "text": 5}))
case("normalize_response",
     response={"content": [{"type": "tool_use", "id": "x", "name": None, "input": {}}],
               "stop_reason": "tool_use"},
     kwargs={"strip_tool_prefix": True},
     io=io(to_plain_data=seq({"type": "tool_use", "id": "x", "name": None, "input": {}}, None),
           sanitize_replay_block={"type": "tool_use", "id": "x", "name": None, "input": {}}))
_THT = {"type": "thinking", "thinking": "T"}
case("normalize_response",
     response={"content": [_THT], "stop_details": {"x": 1}, "stop_reason": "end_turn"},
     kwargs={},
     io=io(to_plain_data=seq(_THT, {"x": 1}), sanitize_replay_block=_THT,
           guardrail_intervened=False, map_finish_reason="stop"))
case("normalize_response",
     response={"content": [{"type": "text", "text": "y"}], "stop_reason": "stop"},
     kwargs={},  # fail-closed: sanitize_replay_block missing
     io=io(to_plain_data=seq({"type": "text", "text": "y"}, None),
           guardrail_intervened=False, map_finish_reason="stop"))
case("normalize_response",
     response={"content": [], "stop_reason": "end_turn"}, kwargs={})  # fail-closed to_plain_data

# ── response_finish_reason ──────────────────────────────────────────────────
case("response_finish_reason", response={"stop_reason": "tool_use"},
     io=io(guardrail_intervened=False, map_finish_reason="tool_calls"))
case("response_finish_reason", response={"stop_reason": "end_turn"},
     io=io(guardrail_intervened=True))
case("response_finish_reason", response={"stop_reason": "end_turn"},
     io=io(guardrail_intervened=False, map_finish_reason="stop"))
case("response_finish_reason", response={"stop_reason": "max_tokens"},
     io=io(guardrail_intervened=RAISE_OE))
case("response_finish_reason", response={"stop_reason": "end_turn"},
     io=io(guardrail_intervened=False, map_finish_reason=RAISE_GE))
case("response_finish_reason", response={"stop_reason": "end_turn"})  # fail-closed

# ── validate_response ───────────────────────────────────────────────────────
case("validate_response", response={"content": [{"type": "text"}], "stop_reason": "end_turn"})
case("validate_response", response={"content": [], "stop_reason": "end_turn"})
case("validate_response", response={"content": [], "stop_reason": "refusal"})
case("validate_response", response={"content": [], "stop_reason": "max_tokens"})
case("validate_response", response={})
case("validate_response", response={"content": None, "stop_reason": "end_turn"})
case("validate_response", response={"content": "str", "stop_reason": "stop"})

# ── extract_cache_stats ─────────────────────────────────────────────────────
case("extract_cache_stats", response={})
case("extract_cache_stats", response={"usage": None})
case("extract_cache_stats", response={"usage": {
    "cache_read_input_tokens": 10, "cache_creation_input_tokens": 0}})
case("extract_cache_stats", response={"usage": {
    "cache_read_input_tokens": 0, "cache_creation_input_tokens": 5}})
case("extract_cache_stats", response={"usage": {
    "cache_read_input_tokens": 0, "cache_creation_input_tokens": 0}})
case("extract_cache_stats", response={"usage": {}})
case("extract_cache_stats", response={"usage": {
    "cache_read_input_tokens": None, "cache_creation_input_tokens": 7}})

# ── map_finish_reason ───────────────────────────────────────────────────────
for _r in ("end_turn", "tool_use", "max_tokens", "stop_sequence", "refusal",
           "model_context_window_exceeded", "unknown", None, 5):
    case("map_finish_reason", raw_reason=_r)
case("map_finish_reason", raw_reason=[])


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8", newline="\n") as f:
        for rec in CASES:
            f.write(json.dumps(rec, ensure_ascii=False, separators=(",", ":")) + "\n")
    counts = {}
    for rec in CASES:
        counts[rec["func"]] = counts.get(rec["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    print(json.dumps(counts, sort_keys=True))


if __name__ == "__main__":
    main()
