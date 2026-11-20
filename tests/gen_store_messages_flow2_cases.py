#!/usr/bin/env python3
"""Generate tests/fixtures/store_messages_flow2.jsonl — input cases for the
unclaimed pure-leaf FLOW oracle (store_messages_flow2).

The oracle computes want from the LIVE quarry defs at gate time; this script
only emits {func, in} inputs (and validates each through oracle.drive once so a
typo fails loudly at generation time).

Representation contract (shared with the C port and the oracle):
- None == JSON null; plain JSON numbers are Python ints; JSON bools are bools.
- A Python float travels as {"__float__": true, "v": <number>}; a tuple as
  {"__tuple__": true, "v": [...]}.
- A Python string beginning with the reserved "\x00json:" marker travels as
  {"__prefixed__": <rest>} (libjson cannot carry a NUL byte in a C string).
- Fixture domain for these pure leaves: str / dict / list / int / bool / null,
  ASCII text only; json-text strings use ordinary JSON escaping.
"""
import copy
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import oracle_store_messages_flow2 as oracle  # noqa: E402

OUT = HERE / "fixtures" / "store_messages_flow2.jsonl"
CASES = []


def add(func, args):
    oracle.drive(func, {"args": copy.deepcopy(args)})
    CASES.append({"func": func, "in": {"args": args}})


# ── _reasoning_json_text ──
add("_reasoning_json_text", {"value": "raw reasoning text"})
add("_reasoning_json_text", {"value": ""})
add("_reasoning_json_text", {"value": None})
add("_reasoning_json_text", {"value": {"kind": "summary", "n": 2}})
add("_reasoning_json_text", {"value": [1, "two", False]})
add("_reasoning_json_text", {"value": True})
add("_reasoning_json_text", {"value": 0})

# ── _encode_content ──
add("_encode_content", {"content": "plain text"})
add("_encode_content", {"content": None})
add("_encode_content", {"content": 42})
add("_encode_content", {"content": {"role": "user", "name": "a"}})
add("_encode_content", {"content": ["part", 7, True]})
add("_encode_content", {"content": True})
add("_encode_content", {"content": {"__prefixed__": "payload"}})  # reserved-prefix escape

# ── _encode_display_metadata ──
add("_encode_display_metadata", {"display_metadata": None})
add("_encode_display_metadata", {"display_metadata": {}})
add("_encode_display_metadata", {"display_metadata": ""})
add("_encode_display_metadata", {"display_metadata": {"a": 1, "b": "two"}})
add("_encode_display_metadata", {"display_metadata": '{"b": 2}'})     # pre-serialized JSON
add("_encode_display_metadata", {"display_metadata": "not json"})
add("_encode_display_metadata", {"display_metadata": "[1,2]"})       # parses to a list
add("_encode_display_metadata", {"display_metadata": 42})

# ── _decode_display_metadata ──
add("_decode_display_metadata", {"raw": None})
add("_decode_display_metadata", {"raw": '{"a": 1}'})
add("_decode_display_metadata", {"raw": "not json"})
add("_decode_display_metadata", {"raw": json.dumps({"inner": json.dumps({"x": 1})})})  # double-encoded
add("_decode_display_metadata", {"raw": 42})
add("_decode_display_metadata", {"raw": "[1,2]"})
add("_decode_display_metadata", {"raw": "null"})

# ── _reaction_list ──
add("_reaction_list", {"meta": None})
add("_reaction_list", {"meta": {"reactions": [{"emoji": "x"}, "bad", 5, {"emoji": "y"}]}})
add("_reaction_list", {"meta": {"reactions": "not-a-list"}})
add("_reaction_list", {"meta": {}})
add("_reaction_list", {"meta": {"other": 1}})
add("_reaction_list", {"meta": [1, 2]})  # non-dict truthy -> AttributeError
add("_reaction_list", {"meta": {"reactions": None}})

# ── _fold_summary_carrier (mutates first_by_uid) ──
add("_fold_summary_carrier", {"row": {"message_uid": None, "_compressed_summary": False},
                              "key": "K", "first_by_uid": {}})
add("_fold_summary_carrier", {"row": {"message_uid": "u1", "_compressed_summary": False},
                              "key": "K", "first_by_uid": {}})
add("_fold_summary_carrier", {"row": {"message_uid": "u1", "_compressed_summary": True},
                              "key": "K2", "first_by_uid": {"u1": "K"}})
add("_fold_summary_carrier", {"row": {"message_uid": "u2", "_compressed_summary": True},
                              "key": "K3", "first_by_uid": {}})
add("_fold_summary_carrier", {"row": {"message_uid": 0, "_compressed_summary": False},
                              "key": 5, "first_by_uid": {}})
add("_fold_summary_carrier", {"row": {"message_uid": "u3", "_compressed_summary": False},
                              "key": {"k": 1}, "first_by_uid": {"other": 2}})

# ── _active_clause ──
add("_active_clause", {"include_inactive": False, "include_compacted": False})
add("_active_clause", {"include_inactive": False, "include_compacted": True})
add("_active_clause", {"include_inactive": True, "include_compacted": False})
add("_active_clause", {"include_inactive": True, "include_compacted": True})
add("_active_clause", {"include_inactive": False, "include_compacted": 0})
add("_active_clause", {"include_inactive": 0, "include_compacted": 1})

# ── _exact_replayed_user_clone_key ──
add("_exact_replayed_user_clone_key", {"timestamp": None, "content": "x"})
add("_exact_replayed_user_clone_key", {"timestamp": 100, "content": None})
add("_exact_replayed_user_clone_key", {"timestamp": 100, "content": ""})
add("_exact_replayed_user_clone_key", {"timestamp": 100, "content": []})
add("_exact_replayed_user_clone_key", {"timestamp": 100, "content": "hello"})
add("_exact_replayed_user_clone_key", {"timestamp": 100, "content": {"b": 2, "a": 1}})
add("_exact_replayed_user_clone_key", {"timestamp": 100, "content": [1, 2]})

# ── _is_model_only_row ──
add("_is_model_only_row", {"row": {"display_metadata": '{"model_only": true}'}})
add("_is_model_only_row", {"row": {"display_metadata": '{"model_only": false}'}})
add("_is_model_only_row", {"row": {"display_metadata": '{"other": 1}'}})
add("_is_model_only_row", {"row": {"display_metadata": None}})
add("_is_model_only_row", {"row": {"display_metadata": json.dumps({"inner": json.dumps({"model_only": True})})}})
add("_is_model_only_row", {"row": {}})  # missing key -> KeyError

# ── _is_explicit_fork_child_row ──
add("_is_explicit_fork_child_row", {"session": {"source": "tool"}})
add("_is_explicit_fork_child_row", {"session": {}})
add("_is_explicit_fork_child_row", {"session": {"model_config": {"_branched_from": "p"}}})
add("_is_explicit_fork_child_row", {"session": {"model_config": {"_branched_from": "p"},
                                                "parent_session_id": "q"}})
add("_is_explicit_fork_child_row", {"session": {"model_config": {"_branched_from": "p"},
                                                "parent_session_id": "p"}})
add("_is_explicit_fork_child_row", {"session": {"model_config": '{"_delegate_from":"p"}',
                                                "parent_session_id": "p"}})
add("_is_explicit_fork_child_row", {"session": {"model_config": "not json"}})
add("_is_explicit_fork_child_row", {"session": {"model_config": {"_reset_from": "p"},
                                                "parent_session_id": "p"},
                                     "include_reset": True})
add("_is_explicit_fork_child_row", {"session": {"model_config": {"_reset_from": "p"},
                                                "parent_session_id": "p"},
                                     "include_reset": False})


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    missing = [k for k in oracle.KINDS if counts.get(k, 0) < 5]
    if missing:
        print(f"FATAL gen_store_messages_flow2: funcs with <5 cases: {missing}")
        sys.exit(2)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "wb") as f:
        for c in CASES:
            line = json.dumps(c, ensure_ascii=False)
            f.write(line.encode("utf-8", errors="surrogatepass") + b"\n")
    print(f"gen_store_messages_flow2: {len(CASES)} cases -> {OUT}")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")


if __name__ == "__main__":
    main()
