#!/usr/bin/env python3
"""Generate tests/fixtures/anthropic_transport_flow.jsonl for the anthropic
transport FLOW + leaf port. >=5 cases per ported func, covering normal paths,
edge cases, implicit raises (AttributeError/TypeError/KeyError), scripted seam
raises (incl. the BaseException trio), fail-closed (missing script) and $seq
queues. Inputs stay in the documented fixture domain (ASCII; hashable block
types; str/null _block_ids keys; strict-JSON _parse_tool_args; present
response.stop_reason). The oracle computes the LIVE reference; the C harness
emits its own output — this generator only emits INPUTS.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "anthropic_transport_flow.jsonl"

CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def io(**scripts):
    return scripts


RAISE_VE = {"__raise__": "ValueError", "msg": "boom"}
RAISE_SE = {"__raise__": "SystemExit", "msg": "1"}
RAISE_KI = {"__raise__": "KeyboardInterrupt", "msg": ""}
RAISE_GE = {"__raise__": "GeneratorExit", "msg": ""}
RAISE_RE = {"__raise__": "RuntimeError", "msg": "x"}

# ── _block_type ─────────────────────────────────────────────────────────────
case("_block_type", b={"type": "text"})
case("_block_type", b={"type": "tool_use", "id": "x"})
case("_block_type", b={})
case("_block_type", b=None)
case("_block_type", b=5)
case("_block_type", b="str")
case("_block_type", b={"type": None})
case("_block_type", b={"type": 3})

# ── _has_block_type ─────────────────────────────────────────────────────────
case("_has_block_type", blocks=[{"type": "text"}], types=["text"])
case("_has_block_type", blocks=[{"type": "image"}], types=["text", "tool_use"])
case("_has_block_type", blocks=[], types=["text"])
case("_has_block_type", blocks=[{"type": "thinking"}, {"type": "tool_use"}], types=["tool_use"])
case("_has_block_type", blocks=[5, {"type": "text"}], types=["text"])
case("_has_block_type", blocks=None, types=["text"])
case("_has_block_type", blocks=[{"type": "text"}], types=[])
case("_has_block_type", blocks=[{"type": None}], types=[None])

# ── _is_blank_text_block ────────────────────────────────────────────────────
case("_is_blank_text_block", b={"type": "text", "text": "hello"})
case("_is_blank_text_block", b={"type": "text", "text": ""})
case("_is_blank_text_block", b={"type": "text", "text": "   "})
case("_is_blank_text_block", b={"type": "text", "text": None})
case("_is_blank_text_block", b={"type": "text", "text": 5})
case("_is_blank_text_block", b={"type": "text"})
case("_is_blank_text_block", b={"type": "image"})
case("_is_blank_text_block", b={"type": "text", "text": "\t\n"})
case("_is_blank_text_block", b={"type": "text", "text": "\x1c"})
case("_is_blank_text_block", b=None)

# ── _cache_control_of ───────────────────────────────────────────────────────
case("_cache_control_of", b={"cache_control": {"type": "ephemeral"}})
case("_cache_control_of", b={"cache_control": "x"})
case("_cache_control_of", b={"cache_control": None})
case("_cache_control_of", b={})
case("_cache_control_of", b=None)
case("_cache_control_of", b={"cache_control": {}})

# ── _text_block ─────────────────────────────────────────────────────────────
case("_text_block", text="hi")
case("_text_block", text="")
case("_text_block", text=None)
case("_text_block", text=5)
case("_text_block", text="a\nb")

# ── _text_block_with_citations ──────────────────────────────────────────────
case("_text_block_with_citations", text="hi", cits=[{"a": 1}])
case("_text_block_with_citations", text="hi", cits=[])
case("_text_block_with_citations", text="hi", cits=None)
case("_text_block_with_citations", text="hi", cits="x")
case("_text_block_with_citations", text="hi", cits=[{"a": 1}, {"b": 2}])

# ── _parse_tool_args ────────────────────────────────────────────────────────
case("_parse_tool_args", raw='{"a": 1}')
case("_parse_tool_args", raw='[1, 2, 3]')
case("_parse_tool_args", raw="bad json")
case("_parse_tool_args", raw="null")
case("_parse_tool_args", raw="42")
case("_parse_tool_args", raw='"str"')
case("_parse_tool_args", raw="true")
case("_parse_tool_args", raw={"a": 1})
case("_parse_tool_args", raw=None)
case("_parse_tool_args", raw="01")
case("_parse_tool_args", raw="[1,]")
case("_parse_tool_args", raw='{"a":1,}')
case("_parse_tool_args", raw="1 2")
case("_parse_tool_args", raw='"a\\tb"')
case("_parse_tool_args", raw='"\\u0041"')
case("_parse_tool_args", raw='{"a":1,"a":2}')
case("_parse_tool_args", raw=" 1 ")
case("_parse_tool_args", raw="1.5")
case("_parse_tool_args", raw="1e3")
case("_parse_tool_args", raw='"\\q"')
case("_parse_tool_args", raw="")

# ── _strip_thinking ─────────────────────────────────────────────────────────
case("_strip_thinking", blocks=[{"type": "thinking"}, {"type": "text"}])
case("_strip_thinking", blocks=[{"type": "redacted_thinking"}, {"type": "tool_use"}])
case("_strip_thinking", blocks=[])
case("_strip_thinking", blocks=[{"type": "text"}, {"type": "image"}])
case("_strip_thinking", blocks=[5, {"type": "thinking"}])
case("_strip_thinking", blocks=None)

# ── _block_ids ──────────────────────────────────────────────────────────────
case("_block_ids", blocks=[{"type": "tool_use", "id": "a"}, {"type": "tool_use", "id": "b"}], btype="tool_use", key="id")
case("_block_ids", blocks=[{"type": "tool_use", "id": "a"}, {"type": "text"}], btype="tool_use", key="id")
case("_block_ids", blocks=[{"type": "tool_use", "id": "a"}, {"type": "tool_use", "id": "a"}], btype="tool_use", key="id")
case("_block_ids", blocks=[{"type": "tool_use"}], btype="tool_use", key="id")
case("_block_ids", blocks=[], btype="tool_use", key="id")
case("_block_ids", blocks=[{"type": "tool_result", "tool_use_id": "x"}], btype="tool_result", key="tool_use_id")
case("_block_ids", blocks=[5], btype=None, key="id")
case("_block_ids", blocks=[{"type": "tool_use", "id": "b"}, {"type": "tool_use", "id": "a"}], btype="tool_use", key="id")

# ── _assistant_block_lists ──────────────────────────────────────────────────
case("_assistant_block_lists", result=[{"role": "assistant", "content": [{"type": "text"}]}, {"role": "user", "content": "hi"}])
case("_assistant_block_lists", result=[{"role": "user", "content": "hi"}])
case("_assistant_block_lists", result=[{"role": "assistant", "content": "str"}])
case("_assistant_block_lists", result=[])
case("_assistant_block_lists", result=[{"role": "assistant", "content": []}, {"role": "assistant", "content": [{"type": "text"}]}])
case("_assistant_block_lists", result=[5])
case("_assistant_block_lists", result=[{"role": "assistant"}])

# ── _tool_use_block ─────────────────────────────────────────────────────────
case("_tool_use_block", tool_id="x.y", name="read", tool_input={"a": 1}, io=io(sanitize_tool_id="x_y"))
case("_tool_use_block", tool_id="", name="t", tool_input={}, io=io(sanitize_tool_id="tool_0"))
case("_tool_use_block", tool_id=None, name="t", tool_input=None, io=io(sanitize_tool_id="tool_0"))
case("_tool_use_block", tool_id="z", name="f", tool_input={"k": "v"}, io=io(sanitize_tool_id=RAISE_VE))
case("_tool_use_block", tool_id="z", name="f", tool_input={}, io=io())
case("_tool_use_block", tool_id="z", name="f", tool_input={}, io=io(sanitize_tool_id=RAISE_SE))
case("_tool_use_block", tool_id="z", name="f", tool_input={}, io=io(sanitize_tool_id=RAISE_KI))

# ── _replay_ordered_blocks ──────────────────────────────────────────────────
case("_replay_ordered_blocks",
     m={"tool_calls": [{"id": "t1", "function": {"name": "f", "arguments": '{"x": 1}'}}]},
     ordered_blocks=[{"type": "tool_use", "id": "t1", "name": "f", "input": {"x": 99}}],
     io=io(sanitize_tool_id="t1",
           sanitize_replay_block={"$seq": [{"type": "tool_use", "id": "t1", "name": "f", "input": {"x": 99}}]},
           apply_assistant_cache_control=None))
case("_replay_ordered_blocks", m={},
     ordered_blocks=[{"type": "text", "text": "  "}],
     io=io(sanitize_replay_block={"$seq": [None]}, apply_assistant_cache_control=None))
case("_replay_ordered_blocks", m={},
     ordered_blocks=[{"type": "image", "source": {}}],
     io=io(sanitize_replay_block={"$seq": [None]}, apply_assistant_cache_control=None))
case("_replay_ordered_blocks", m={},
     ordered_blocks=[{"type": "text", "text": "", "cache_control": {"type": "ephemeral"}},
                     {"type": "tool_use", "id": "t", "name": "f", "input": {}}],
     io=io(sanitize_tool_id="t",
           sanitize_replay_block={"$seq": [None, {"type": "tool_use", "id": "t", "name": "f", "input": {}}]},
           apply_assistant_cache_control=None))
case("_replay_ordered_blocks",
     m={"content": [{"type": "text", "text": "x", "cache_control": {"type": "inline"}}]},
     ordered_blocks=[{"type": "text", "text": "hi"}],
     io=io(sanitize_replay_block={"$seq": [{"type": "text", "text": "hi"}]}, apply_assistant_cache_control=None))
case("_replay_ordered_blocks", m={}, ordered_blocks=[{"type": "text", "text": "hi"}],
     io=io(sanitize_replay_block=RAISE_VE, apply_assistant_cache_control=None))
case("_replay_ordered_blocks", m={}, ordered_blocks=[{"type": "text"}], io=io())
case("_replay_ordered_blocks", m={"tool_calls": [{"id": "t", "function": "x"}]},
     ordered_blocks=[], io=io(sanitize_tool_id="t"))
case("_replay_ordered_blocks", m={}, ordered_blocks=[{"type": "text"}],
     io=io(sanitize_replay_block=RAISE_KI, apply_assistant_cache_control=None))
case("_replay_ordered_blocks",
     m={"cache_control": {"type": "top"}, "tool_calls": [{"id": "a", "function": {"arguments": "null"}}]},
     ordered_blocks=[{"type": "tool_use", "id": "a", "name": "f", "input": {"secret": 1}}],
     io=io(sanitize_tool_id="a",
           sanitize_replay_block={"$seq": [{"type": "tool_use", "id": "a", "name": "f", "input": {"secret": 1}}]},
           apply_assistant_cache_control=None))

# ── assistant_replay_carrier ────────────────────────────────────────────────
case("assistant_replay_carrier", m={"anthropic_content_blocks": [{"type": "text", "text": "hi"}]},
     io=io(sanitize_replay_block={"$seq": [{"type": "text", "text": "hi"}]}, apply_assistant_cache_control=None))
case("assistant_replay_carrier", m={"anthropic_content_blocks": []},
     io=io(extract_preserved_thinking_blocks=[{"type": "thinking", "thinking": "t"}]))
case("assistant_replay_carrier", m={}, io=io(extract_preserved_thinking_blocks=[]))
case("assistant_replay_carrier", m={"anthropic_content_blocks": [{"type": "image"}]},
     io=io(sanitize_replay_block={"$seq": [None]}, extract_preserved_thinking_blocks=[{"type": "thinking"}]))
case("assistant_replay_carrier", m={"anthropic_content_blocks": [{"type": "text"}]}, io=io())
case("assistant_replay_carrier", m={}, io=io(extract_preserved_thinking_blocks=RAISE_RE))
case("assistant_replay_carrier", m={"anthropic_content_blocks": [{"type": "text", "text": "hi"}]},
     io=io(sanitize_replay_block={"$seq": [{"type": "text", "text": "hi"}]},
           apply_assistant_cache_control=None, extract_preserved_thinking_blocks=RAISE_GE))

# ── _tool_result_content ────────────────────────────────────────────────────
case("_tool_result_content", m={"content": "hello"})
case("_tool_result_content", m={"content": ""})
case("_tool_result_content", m={"content": "  "})
case("_tool_result_content", m={"content": {"_multimodal": True, "content": [{"type": "text", "text": "x"}]}},
     io=io(content_parts_to_anthropic_blocks=[{"type": "text", "text": "x"}]))
case("_tool_result_content", m={"content": {"_multimodal": True, "content": [], "text_summary": "sum"}},
     io=io(content_parts_to_anthropic_blocks=[]))
case("_tool_result_content", m={"content": {"_multimodal": True, "content": [], "text_summary": 5}},
     io=io(content_parts_to_anthropic_blocks=[]))
case("_tool_result_content", m={"content": [{"type": "image", "source": {"type": "url", "url": "u"}}]},
     io=io(content_parts_to_anthropic_blocks=[{"type": "image", "source": {"type": "url", "url": "u"}}]))
case("_tool_result_content", m={"content": [{"type": "text", "text": "x"}]},
     io=io(content_parts_to_anthropic_blocks=[{"type": "text", "text": "x"}]))
case("_tool_result_content", m={"content": {"a": 1}})
case("_tool_result_content", m={"content": None})
case("_tool_result_content", m={"content": "x", "_anthropic_content_blocks": [{"type": "image", "source": {}}]})
case("_tool_result_content", m={"content": "", "_anthropic_content_blocks": [{"type": "image"}]})
case("_tool_result_content", m={"content": [{"type": "text"}]}, io=io())
case("_tool_result_content", m={"content": [{"type": "text"}]}, io=io(content_parts_to_anthropic_blocks=RAISE_VE))
case("_tool_result_content", m={"content": [1, 2]})
case("_tool_result_content", m={"content": True})

# ── _concat_content ─────────────────────────────────────────────────────────
case("_concat_content", prev="a", curr="b")
case("_concat_content", prev=[{"type": "text", "text": "a"}], curr="b")
case("_concat_content", prev="a", curr=[{"type": "image"}])
case("_concat_content", prev={"k1": 1, "k2": 2}, curr=[])
case("_concat_content", prev=[], curr=[])
case("_concat_content", prev=None, curr="b")
case("_concat_content", prev="a", curr=5)
case("_concat_content", prev=[1, 2], curr=[3])

# ── _keep_valid_thinking ────────────────────────────────────────────────────
case("_keep_valid_thinking", content=[{"type": "thinking", "thinking": "t", "signature": "s"}], signature_dead=False)
case("_keep_valid_thinking", content=[{"type": "thinking", "thinking": "t"}], signature_dead=False)
case("_keep_valid_thinking", content=[{"type": "thinking", "thinking": "t", "signature": "s"}], signature_dead=True)
case("_keep_valid_thinking", content=[{"type": "redacted_thinking", "data": "d"}], signature_dead=False)
case("_keep_valid_thinking", content=[{"type": "redacted_thinking"}], signature_dead=False)
case("_keep_valid_thinking", content=[{"type": "redacted_thinking", "thinking": "t"}], signature_dead=True)
case("_keep_valid_thinking", content=[{"type": "text", "text": "x"}], signature_dead=False)
case("_keep_valid_thinking", content=[5], signature_dead=False)
case("_keep_valid_thinking", content=None, signature_dead=False)

# ── _current_tool_loop_start ────────────────────────────────────────────────
case("_current_tool_loop_start", result=[{"role": "user", "content": "hi"}, {"role": "assistant", "content": []}])
case("_current_tool_loop_start", result=[{"role": "user", "content": [{"type": "tool_result"}]}, {"role": "assistant", "content": []}])
case("_current_tool_loop_start", result=[{"role": "assistant", "content": []}])
case("_current_tool_loop_start", result=[])
case("_current_tool_loop_start", result=[{"role": "user", "content": "a"}, {"role": "user", "content": "b"}])
case("_current_tool_loop_start", result=[5])
case("_current_tool_loop_start", result=[{"role": "user", "content": [{"type": "text"}]}])

# ── _image_source_data ──────────────────────────────────────────────────────
case("_image_source_data", block={"type": "image", "source": {"type": "base64", "data": "abc"}})
case("_image_source_data", block={"type": "image", "source": {"type": "url", "url": "u"}})
case("_image_source_data", block={"type": "image", "source": {"data": 5}})
case("_image_source_data", block={"type": "image", "source": "x"})
case("_image_source_data", block={"type": "image"})
case("_image_source_data", block=None)
case("_image_source_data", block={"source": {"data": "d"}})

# ── _convert_system_content ─────────────────────────────────────────────────
case("_convert_system_content", content="plain str")
case("_convert_system_content", content=[{"type": "text", "text": "a"}, {"type": "text", "text": "b"}])
case("_convert_system_content", content=[{"type": "text", "text": "a", "cache_control": {"type": "ephemeral"}}])
case("_convert_system_content", content=[{"type": "text", "text": "  ", "cache_control": {"type": "ephemeral"}}])
case("_convert_system_content", content=[{"type": "text", "text": "a"}, {"type": "image"}])
case("_convert_system_content", content=[{"type": "text"}])
case("_convert_system_content", content=[{"type": "text", "text": 5}])
case("_convert_system_content", content=[5])
case("_convert_system_content", content=[{"type": "text", "text": "a"}, 5])
case("_convert_system_content", content=None)
case("_convert_system_content", content=[{"type": "text", "text": "a", "cache_control": {}}, {"type": "text", "text": "b"}])
case("_convert_system_content", content=[{"type": "text", "text": "a", "cache_control": {"x": 1}}, {"type": "text", "text": "  "}])

# ── _unprefix_oauth_tool_name ───────────────────────────────────────────────
case("_unprefix_oauth_tool_name", name="mcp__read_file", io=io(**{"registry.get_entry": {"$seq": [None, None, {"name": "read_file"}]}}))
case("_unprefix_oauth_tool_name", name="mcp__chat_history_lookup", io=io(**{"registry.get_entry": {"$seq": [None, None, None]}}))
case("_unprefix_oauth_tool_name", name="mcp__context_notes", io=io(**{"registry.get_entry": {"$seq": [None, None, None]}}))
case("_unprefix_oauth_tool_name", name="mcp__unknown", io=io(**{"registry.get_entry": {"$seq": [None, None, None]}}))
case("_unprefix_oauth_tool_name", name="mcp__read_file", io=io(**{"registry.get_entry": {"$seq": [{"name": "x"}]}}))
case("_unprefix_oauth_tool_name", name="short", io=io(**{"registry.get_entry": {"$seq": [None, None, None]}}))
case("_unprefix_oauth_tool_name", name=None, io=io())
case("_unprefix_oauth_tool_name", name="mcp__x", io=io())
case("_unprefix_oauth_tool_name", name="mcp__x", io=io(**{"registry.get_entry": RAISE_SE}))
case("_unprefix_oauth_tool_name", name="mcp__memory", io=io(**{"registry.get_entry": {"$seq": [None, None, None]}}))

# ── response_finish_reason ──────────────────────────────────────────────────
case("response_finish_reason", response={"stop_reason": "end_turn"}, io=io(guardrail_intervened=False, map_finish_reason="stop"))
case("response_finish_reason", response={"stop_reason": "tool_use"}, io=io(guardrail_intervened=True, map_finish_reason="tool_calls"))
case("response_finish_reason", response={"stop_reason": "max_tokens"}, io=io(guardrail_intervened=False, map_finish_reason="length"))
case("response_finish_reason", response={"stop_reason": "refusal"}, io=io(guardrail_intervened=False, map_finish_reason="content_filter"))
case("response_finish_reason", response={"stop_reason": "unknown"}, io=io(guardrail_intervened=False, map_finish_reason="stop"))
case("response_finish_reason", response={"stop_reason": "end_turn"}, io=io())
case("response_finish_reason", response={"stop_reason": "end_turn"}, io=io(guardrail_intervened=RAISE_VE))
case("response_finish_reason", response={"stop_reason": "end_turn"}, io=io(guardrail_intervened=False, map_finish_reason=RAISE_KI))
case("response_finish_reason", response={"stop_reason": None}, io=io(guardrail_intervened=False, map_finish_reason="stop"))


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    # per-func count audit (>=5 each)
    from collections import Counter
    cnt = Counter(c["func"] for c in CASES)
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for fn, n in sorted(cnt.items()):
        flag = "" if n >= 5 else "  <-- BELOW 5"
        print(f"  {fn}: {n}{flag}")
    below = [fn for fn, n in cnt.items() if n < 5]
    if below:
        raise SystemExit(f"funcs below 5 cases: {below}")


if __name__ == "__main__":
    main()