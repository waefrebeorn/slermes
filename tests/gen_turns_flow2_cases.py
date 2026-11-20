#!/usr/bin/env python3
"""Generate tests/fixtures/turns_flow2.jsonl — input cases for the
unclaimed turns.py pure-helper FLOW oracle.

The oracle computes want from the LIVE quarry defs at gate time; this script
only emits {func, in} inputs. This cluster has no external collaborators, so
in.io is always absent. Representation contract (shared with C + oracle):
None == JSON null; plain JSON numbers are Python ints; bools are bools; a
Python float travels as {"__float__": true, "v": n}. Deterministic frozensets
are compared as sorted string arrays on both sides.
"""
import copy
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import oracle_turns_flow2 as oracle  # noqa: E402

OUT = HERE / "fixtures" / "turns_flow2.jsonl"
CASES = []


def add(func, args):
    oracle.drive(func, {"args": copy.deepcopy(args)})  # fail loudly on a typo
    CASES.append({"func": func, "in": {"args": args}})


# ── _uid_occurrences ──
add("_uid_occurrences", {"value": "u1"})
add("_uid_occurrences", {"value": ["u1", "u2"]})
add("_uid_occurrences", {"value": 5})
add("_uid_occurrences", {"value": None})
add("_uid_occurrences", {"value": {"a": 1}})
add("_uid_occurrences", {"value": []})
add("_uid_occurrences", {"value": True})
add("_uid_occurrences", {"value": ["a", 2, None]})

# ── merge_tool_call_uids ──
add("merge_tool_call_uids", {"into": {}, "extra": {}})
add("merge_tool_call_uids", {"into": {"c1": "u1"}, "extra": {"c2": "u2"}})
add("merge_tool_call_uids", {"into": {"c1": "u1"}, "extra": {"c1": "u2"}})
add("merge_tool_call_uids", {"into": {"c1": ["u0", "u1"]}, "extra": {"c1": "u2"}})
add("merge_tool_call_uids", {"into": {"c1": "u1"}, "extra": {"c1": ["u2", "u3"]}})
add("merge_tool_call_uids", {"into": None, "extra": {"c1": "u1"}})
add("merge_tool_call_uids", {"into": {"c1": None}, "extra": {"c1": "u2"}})
add("merge_tool_call_uids", {"into": {"c1": "u1"}, "extra": None})

# ── _tc_field ──
add("_tc_field", {"tc": {"call_id": "c1"}, "key": "call_id"})
add("_tc_field", {"tc": {"call_id": "c1"}, "key": "id"})
add("_tc_field", {"tc": {"call_id": None}, "key": "call_id"})
add("_tc_field", {"tc": "a string", "key": "call_id"})
add("_tc_field", {"tc": 5, "key": "call_id"})
add("_tc_field", {"tc": None, "key": "call_id"})
add("_tc_field", {"tc": {"x": [1, 2]}, "key": "x"})

# ── _expand_tool_id_variants ──
add("_expand_tool_id_variants", {"values": ["c1"]})
add("_expand_tool_id_variants", {"values": [" c1 ", "c2"]})
add("_expand_tool_id_variants", {"values": ["a|b"]})
add("_expand_tool_id_variants", {"values": [""]})
add("_expand_tool_id_variants", {"values": [5, None, "x"]})
add("_expand_tool_id_variants", {"values": ["  x|y  "]})
add("_expand_tool_id_variants", {"values": ["dup", "dup"]})
add("_expand_tool_id_variants", {"values": []})

# ── tool_call_id_variants ──
add("tool_call_id_variants", {"tc": {"call_id": "c1", "id": "i1"}})
add("tool_call_id_variants", {"tc": {"id": "i1", "response_item_id": "r1"}})
add("tool_call_id_variants", {"tc": {"call_id": "a|b"}})
add("tool_call_id_variants", {"tc": {"call_id": " c ", "id": ""}})
add("tool_call_id_variants", {"tc": {}})
add("tool_call_id_variants", {"tc": "not a dict"})
add("tool_call_id_variants", {"tc": {"call_id": 5, "id": None}})

# ── tool_result_id_variants ──
add("tool_result_id_variants", {"tool_call_id": "c1"})
add("tool_result_id_variants", {"tool_call_id": "a|b"})
add("tool_result_id_variants", {"tool_call_id": " c "})
add("tool_result_id_variants", {"tool_call_id": ""})
add("tool_result_id_variants", {"tool_call_id": None})
add("tool_result_id_variants", {"tool_call_id": 5})

# ── per_occurrence_tool_call_uids ──
add("per_occurrence_tool_call_uids", {"uids": {}, "tool_calls": []})
add("per_occurrence_tool_call_uids", {"uids": {"c1": "u1"}, "tool_calls": [{"call_id": "c1"}]})
add("per_occurrence_tool_call_uids", {"uids": {"c1": "u1"},
                                      "tool_calls": [{"call_id": "c1"}, {"id": "c1"}]})
add("per_occurrence_tool_call_uids", {"uids": {"c1": "u1"},
                                      "tool_calls": [{"call_id": "c1"}, {"call_id": "c1"},
                                                     {"call_id": "c2"}]})
add("per_occurrence_tool_call_uids", {"uids": {"c1": ["u0", "u1"]},
                                      "tool_calls": [{"call_id": "c1"}, {"call_id": "c1"}]})
add("per_occurrence_tool_call_uids", {"uids": {},
                                      "tool_calls": [{"call_id": "c1"}, {"call_id": "c1"}]})
add("per_occurrence_tool_call_uids", {"uids": {"c1": "u1"},
                                      "tool_calls": [{"call_id": " c1|r "}, {"id": "c1"}]})
add("per_occurrence_tool_call_uids", {"uids": {"c1": "u1"}, "tool_calls": [{}]})

# ── index_tool_call_uids ──
add("index_tool_call_uids", {"index": {}, "assistant": {}})
add("index_tool_call_uids", {"index": {},
    "assistant": {"tool_calls": [{"call_id": "c1", "id": "i1"}], "_tool_call_uids": {"c1": "u1"}}})
add("index_tool_call_uids", {"index": {"c1": "old"},
    "assistant": {"tool_calls": [{"call_id": "c1"}]}})
add("index_tool_call_uids", {"index": {"keep": "v"},
    "assistant": {"tool_calls": [{"call_id": "c1"}], "_tool_call_uids": {}}})
add("index_tool_call_uids", {"index": {},
    "assistant": {"tool_calls": [{"call_id": "c1", "id": "i1"}, {"call_id": "c1", "id": "i2"}],
                  "_tool_call_uids": {"c1": ["u0", "u1"]}}})
add("index_tool_call_uids", {"index": {},
    "assistant": {"tool_calls": [{"call_id": "a|b", "id": "r"}],
                  "_tool_call_uids": {"a": "ua"}}})
add("index_tool_call_uids", {"index": {"c1": "stale"},
    "assistant": {"_tool_call_uids": {"c1": "u1"}}})

# ── resolve_tool_call_uid ──
add("resolve_tool_call_uid", {"index": {"c1": "u1"}, "tool_call_id": "c1"})
add("resolve_tool_call_uid", {"index": {"i1": "u1"}, "tool_call_id": "c1|i1"})
add("resolve_tool_call_uid", {"index": {"c1": "u1"}, "tool_call_id": "a|b"})
add("resolve_tool_call_uid", {"index": {}, "tool_call_id": "c1"})
add("resolve_tool_call_uid", {"index": {"c1": "u1"}, "tool_call_id": ""})
add("resolve_tool_call_uid", {"index": {"c1": "u1"}, "tool_call_id": 5})
add("resolve_tool_call_uid", {"index": {"c1": "u1"}, "tool_call_id": None})
add("resolve_tool_call_uid", {"index": {"c1": "u1", "i1": "u1"}, "tool_call_id": "c1|i1"})

# ── retired_row_payload ──
add("retired_row_payload", {"message": {"role": "user", "content": "hi"}})
add("retired_row_payload", {"message": {"role": "tool", "content": None, "tool_call_id": "tc1"}})
add("retired_row_payload", {"message": {"role": "assistant", "content": "",
                                        "tool_calls": [{"id": "c1"}, {"id": "c2"}]}})
add("retired_row_payload", {"message": {"tool_call_id": ""}})
add("retired_row_payload", {"message": {}})
add("retired_row_payload", {"message": None})
add("retired_row_payload", {"message": {"tool_calls": [{"no": "id"}]}})
add("retired_row_payload", {"message": {"tool_calls": "not a list"}})

# ── _is_multimodal_tool_result ──
add("_is_multimodal_tool_result", {"value": {"_multimodal": True, "content": []}})
add("_is_multimodal_tool_result", {"value": {"_multimodal": True, "content": "x"}})
add("_is_multimodal_tool_result", {"value": {"_multimodal": 1, "content": []}})
add("_is_multimodal_tool_result", {"value": {"_multimodal": False, "content": []}})
add("_is_multimodal_tool_result", {"value": {}})
add("_is_multimodal_tool_result", {"value": None})
add("_is_multimodal_tool_result", {"value": {"_multimodal": True}})

# ── _is_text_part ──
add("_is_text_part", {"p": {"type": "text"}})
add("_is_text_part", {"p": {"type": "image"}})
add("_is_text_part", {"p": {"type": "text", "text": "x"}})
add("_is_text_part", {"p": {}})
add("_is_text_part", {"p": "text"})
add("_is_text_part", {"p": {"type": 5}})
add("_is_text_part", {"p": None})

# ── _multimodal_text_summary ──
add("_multimodal_text_summary", {"value": "plain"})
add("_multimodal_text_summary", {"value": {"_multimodal": True, "content": [], "text_summary": "sum"}})
add("_multimodal_text_summary", {"value": {"_multimodal": True, "content": [], "text_summary": 5}})
add("_multimodal_text_summary", {"value": {"_multimodal": True, "content": [
    {"type": "text", "text": "a"}, {"type": "image"}, {"type": "text", "text": "b"}]}})
add("_multimodal_text_summary", {"value": {"_multimodal": True, "content": [{"type": "image"}]}})
add("_multimodal_text_summary", {"value": {"_multimodal": True, "content": []}})
add("_multimodal_text_summary", {"value": 5})
add("_multimodal_text_summary", {"value": [1, 2]})
add("_multimodal_text_summary", {"value": {"a": 1}})
add("_multimodal_text_summary", {"value": True})
add("_multimodal_text_summary", {"value": None})

# ── _is_ephemeral_scaffolding ──
add("_is_ephemeral_scaffolding", {"msg": {}})
add("_is_ephemeral_scaffolding", {"msg": {"_thinking_prefill": True}})
add("_is_ephemeral_scaffolding", {"msg": {"_thinking_prefill": False}})
add("_is_ephemeral_scaffolding", {"msg": {"_empty_recovery_synthetic": "x"}})
add("_is_ephemeral_scaffolding", {"msg": {"_verification_stop_synthetic": 1}})
add("_is_ephemeral_scaffolding", {"msg": {"other": True}})
add("_is_ephemeral_scaffolding", {"msg": None})
add("_is_ephemeral_scaffolding", {"msg": {"_dropped_toolcall_nudge": []}})

# ── _image_marker ──
add("_image_marker", {"part": {}})
add("_image_marker", {"part": {"image_url": {"url": "u"}}})
add("_image_marker", {"part": {"detail": "low"}})
add("_image_marker", {"part": {"detail": "high", "image_url": {}}})
add("_image_marker", {"part": {"image_url": {"detail": "auto"}}})
add("_image_marker", {"part": {"detail": "weird"}})
add("_image_marker", {"part": {"detail": "low", "image_url": {"detail": "high"}}})
add("_image_marker", {"part": None})

# ── _durable_content ──
add("_durable_content", {"content": None})
add("_durable_content", {"content": "text"})
add("_durable_content", {"content": {"_multimodal": True, "content": [], "text_summary": "sum"}})
add("_durable_content", {"content": []})
add("_durable_content", {"content": [{"type": "text", "text": "a"},
                                     {"type": "image_url", "image_url": {"url": "u"}}]})
add("_durable_content", {"content": [{"type": "image", "image_url": {"detail": "high"}}]})
add("_durable_content", {"content": [{"type": "text"}]})
add("_durable_content", {"content": [{"type": "audio"}]})
add("_durable_content", {"content": {"a": 1}})

# ── _content_has_payload ──
add("_content_has_payload", {"content": None})
add("_content_has_payload", {"content": ""})
add("_content_has_payload", {"content": "  "})
add("_content_has_payload", {"content": "x"})
add("_content_has_payload", {"content": 0})
add("_content_has_payload", {"content": []})
add("_content_has_payload", {"content": [{}]})
add("_content_has_payload", {"content": [{"type": "text", "text": ""}]})
add("_content_has_payload", {"content": [{"type": "text", "text": "x"}]})
add("_content_has_payload", {"content": [{"type": "image"}]})
add("_content_has_payload", {"content": [5]})
add("_content_has_payload", {"content": [None]})
add("_content_has_payload", {"content": [{"type": "text"}]})

# ── _msg_has_payload ──
add("_msg_has_payload", {"msg": {}})
add("_msg_has_payload", {"msg": {"content": "x"}})
add("_msg_has_payload", {"msg": {"content": ""}})
add("_msg_has_payload", {"msg": {"content": None, "tool_calls": [{}]}})
add("_msg_has_payload", {"msg": {"content": None, "reasoning_content": "  "}})
add("_msg_has_payload", {"msg": {"content": None, "reasoning_content": "x"}})
add("_msg_has_payload", {"msg": {"content": None, "reasoning_details": [1]}})
add("_msg_has_payload", {"msg": {"content": None, "codex_message_items": []}})
add("_msg_has_payload", {"msg": {"content": None, "codex_reasoning_items": [1]}})
add("_msg_has_payload", {"msg": {"content": [{"type": "image"}]}})

# ── _is_codex_interim ──
add("_is_codex_interim", {"m": {}})
add("_is_codex_interim", {"m": {"finish_reason": "incomplete"}})
add("_is_codex_interim", {"m": {"finish_reason": "stop"}})
add("_is_codex_interim", {"m": {"codex_message_items": [1]}})
add("_is_codex_interim", {"m": {"codex_reasoning_items": []}})
add("_is_codex_interim", {"m": None})
add("_is_codex_interim", {"m": {"finish_reason": "INCOMPLETE"}})

# ── _json_or ──
add("_json_or", {"raw": '{"a":1}', "fallback": "raw", "warning": "w"})
add("_json_or", {"raw": "5", "fallback": None, "warning": "w"})
add("_json_or", {"raw": '"hi"', "fallback": None, "warning": "w"})
add("_json_or", {"raw": "[1,2]", "fallback": None, "warning": "w"})
add("_json_or", {"raw": "true", "fallback": None, "warning": "w"})
add("_json_or", {"raw": "null", "fallback": None, "warning": "w"})
add("_json_or", {"raw": "not json", "fallback": "raw", "warning": "w"})
add("_json_or", {"raw": 5, "fallback": "raw", "warning": "w"})
add("_json_or", {"raw": None, "fallback": "raw", "warning": "w"})
add("_json_or", {"raw": "", "fallback": "raw", "warning": "w"})
add("_json_or", {"raw": '{"a": 1}', "fallback": None, "warning": "w"})


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    missing = [k for k in oracle.KINDS if counts.get(k, 0) < 5]
    if missing:
        print(f"FATAL gen_turns_flow2: funcs with <5 cases: {missing}")
        sys.exit(2)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "wb") as f:
        for c in CASES:
            line = json.dumps(c, ensure_ascii=False)
            f.write(line.encode("utf-8", errors="surrogatepass") + b"\n")
    print(f"gen_turns_flow2: {len(CASES)} cases -> {OUT}")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")


if __name__ == "__main__":
    main()
