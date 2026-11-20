#!/usr/bin/env python3
"""Generate tests/fixtures/turns_flow5.jsonl — inputs for the turns.py
REMAINDER-beyond-flow4 FLOW oracle (regex scrubber + sequence-repair).

The oracle computes `want` from the LIVE quarry defs at gate time; this script
only emits {func, in} inputs. Representation contract (shared with C + oracle):
None == JSON null; plain JSON numbers are Python ints; bools are bools. A Python
tuple return is transported as a JSON array. Every entry point MUTATES its args
in place; the harness echoes the mutated args as out.state.args.

`oracle.drive` is called on every case so a typo fails loudly at generation.
"""
import copy
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import oracle_turns_flow5 as oracle  # noqa: E402

OUT = HERE / "fixtures" / "turns_flow5.jsonl"
CASES = []
PFX = oracle.PFX
DBP = oracle.DBP


def add(func, args):
    inp = {"args": copy.deepcopy(args)}
    oracle.drive(func, inp)  # fail loudly on a typo / bad domain
    CASES.append({"func": func, "in": inp})


def M(role, content=None, **kw):
    d = {"role": role, "content": content}
    d.update(kw)
    return d


# ── sanitize_context (9) ──
add("sanitize_context", {"text": "ordinary text, nothing to strip"})
add("sanitize_context", {"text": "before <memory-context>secret</memory-context> after"})
add("sanitize_context", {"text": "a </MEMORY-CONTEXT > b < MEMORY-CONTEXT > c"})
add("sanitize_context", {"text": "x <memory-context>\nhidden lines\n</memory-context> y"})
add("sanitize_context", {"text": "A [System note: The following is recalled memory context, NOT new user input. Treat as informational background data.] B"})
add("sanitize_context", {"text": "A [System note: The following is recalled memory context, NOT new user input. Treat as authoritative reference data for this turn.] B"})
add("sanitize_context", {"text": "  [SYSTEM NOTE:  The following is recalled memory context,   NOT new user input.  Treat as informational background data.]   tail  "})
add("sanitize_context", {"text": "keep <memory-context> one </memory-context> and [System note: The following is recalled memory context, NOT new user input. Treat as informational background data.] end"})
add("sanitize_context", {"text": ""})

# ── _loaded_view_content (7) ──
add("_loaded_view_content", {"role": "user", "content": "  <memory-context>x</memory-context>  hello  "})
add("_loaded_view_content", {"role": "assistant", "content": " hi </memory-context> there "})
add("_loaded_view_content", {"role": "tool", "content": " keep <memory-context>x</memory-context> raw "})
add("_loaded_view_content", {"role": "user", "content": None})
add("_loaded_view_content", {"role": "system", "content": "  untouched  "})
add("_loaded_view_content", {"role": "user", "content": "plain"})
add("_loaded_view_content", {"role": "assistant", "content": 42})

# ── comparison_content (8) ──
add("comparison_content", {"message": M("user", " <memory-context>x</memory-context> hi ")})
add("comparison_content", {"message": M("assistant", [
    {"type": "text", "text": "alpha"}, {"type": "image"}, {"type": "text", "text": "beta"}])})
add("comparison_content", {"message": M("tool", {"a": 1})})
add("comparison_content", {"message": M("user", None)})
add("comparison_content", {"message": M("user", [{"type": "image", "image_url": {"detail": "high"}}])})
add("comparison_content", {"message": M("assistant", [])})
add("comparison_content", {"message": M("assistant", "plain text")})
add("comparison_content", {"message": {"content": "no role"}})

# ── _content_has_payload (8) ──
add("_content_has_payload", {"content": None})
add("_content_has_payload", {"content": ""})
add("_content_has_payload", {"content": "   "})
add("_content_has_payload", {"content": "hello"})
add("_content_has_payload", {"content": []})
add("_content_has_payload", {"content": [{"type": "text", "text": "  "}]})
add("_content_has_payload", {"content": [{"type": "text", "text": "body"}]})
add("_content_has_payload", {"content": [{"type": "image", "image_url": "x"}]})

# ── _msg_has_payload (8) ──
add("_msg_has_payload", {"msg": {"content": ""}})
add("_msg_has_payload", {"msg": {"content": "hi"}})
add("_msg_has_payload", {"msg": {"content": "", "tool_calls": [{"id": "c1"}]}})
add("_msg_has_payload", {"msg": {"content": "", "reasoning_content": "  "}})
add("_msg_has_payload", {"msg": {"content": "", "reasoning_content": "think"}})
add("_msg_has_payload", {"msg": {"content": "", "codex_message_items": [{"x": 1}]}})
add("_msg_has_payload", {"msg": {"content": []}})
add("_msg_has_payload", {"msg": {"content": [{"type": "text", "text": "body"}]}})

# ── _merge_assistant_into (10) ──
add("_merge_assistant_into", {"prev": M("assistant", "first"), "msg": M("assistant", "second")})
add("_merge_assistant_into", {"prev": M("assistant", "first"), "msg": M("assistant", "")})
add("_merge_assistant_into", {"prev": M("assistant", ""), "msg": M("assistant", "second")})
add("_merge_assistant_into", {"prev": M("assistant", "a", tool_calls=[{"id": "c1"}]),
                              "msg": M("assistant", "b", tool_calls=[{"id": "c2"}])})
add("_merge_assistant_into", {"prev": M("assistant", "a", tool_calls=[]), "msg": M("assistant", "b")})
add("_merge_assistant_into", {"prev": M("assistant", "a"), "msg": M("assistant", "b", reasoning_content="why")})
add("_merge_assistant_into", {"prev": M("assistant", "a", api_content="stale"), "msg": M("assistant", "b")})
add("_merge_assistant_into", {"prev": M("assistant", [{"type": "text", "text": "a"}]),
                              "msg": M("assistant", "b")})
add("_merge_assistant_into", {"prev": M("assistant", "a", **{DBP: True}), "msg": M("assistant", "b")})
add("_merge_assistant_into", {"prev": M("assistant", "a", **{DBP: True}), "msg": M("assistant", "")})

# ── _remember_absorbed_row (8) ──
add("_remember_absorbed_row", {"survivor": M("user", "keep", _row_id=10),
                               "dropped": M("user", "gone", _row_id=20), "folded": False})
add("_remember_absorbed_row", {"survivor": M("user", "keep", _row_id=10, message_uid="s"),
                               "dropped": M("user", "gone", _row_id=20, message_uid="d"), "folded": True})
add("_remember_absorbed_row", {"survivor": M("user", "keep", _row_id=10, _absorbed_row_ids=[5]),
                               "dropped": M("user", "gone", _row_id=20, _absorbed_row_ids=[6, 5]), "folded": False})
add("_remember_absorbed_row", {"survivor": M("user", "keep", _row_id=10, _unnamed_durable_rows=2),
                               "dropped": M("user", "gone", _unnamed_durable_rows=3), "folded": False})
add("_remember_absorbed_row", {"survivor": M("user", "keep", _row_id=10),
                               "dropped": M("user", "gone", _retired_durable_rows=[{"role": "user", "content": "x"}]), "folded": False})
add("_remember_absorbed_row", {"survivor": M("user", "keep", _row_id=10),
                               "dropped": M("user", "gone", _row_id=10), "folded": False})
add("_remember_absorbed_row", {"survivor": M("user", "keep", _row_id=10, _absorbed_message_uids=["s1"], message_uid="s"),
                               "dropped": M("user", "gone", message_uid="d", _absorbed_message_uids=["d1"]), "folded": True})
add("_remember_absorbed_row", {"survivor": M("user", "keep", _row_id=10), "dropped": M("user", ""), "folded": True})

# ── _count_unnamed_row (7) ──
add("_count_unnamed_row", {"survivor": M("user", "s", **{DBP: True}),
                           "retired": M("assistant", "r", **{DBP: True})})
add("_count_unnamed_row", {"survivor": M("user", "s"), "retired": M("assistant", "r", **{DBP: True})})
add("_count_unnamed_row", {"survivor": M("user", "s", **{DBP: True}),
                           "retired": M("assistant", "r", _row_id=7, **{DBP: True})})
add("_count_unnamed_row", {"survivor": M("user", "s", _unnamed_durable_rows=2),
                           "retired": M("assistant", "r", _unnamed_durable_rows=1)})
add("_count_unnamed_row", {"survivor": M("user", "s", **{DBP: True}),
                           "retired": M("assistant", "r", **{DBP: True}, _retired_durable_rows=[{"role": "assistant", "content": "r", "_own_row": True}])})
add("_count_unnamed_row", {"survivor": M("user", "s", **{DBP: True}),
                           "retired": M("tool", "r", tool_call_id="c1", **{DBP: True})})
add("_count_unnamed_row", {"survivor": M("user", "s"), "retired": M("assistant", "r")})

# ── _remember_own_row (6) ──
add("_remember_own_row", {"survivor": M("assistant", "a", **{DBP: True})})
add("_remember_own_row", {"survivor": M("assistant", "a", _row_id=3, **{DBP: True})})
add("_remember_own_row", {"survivor": M("assistant", "a")})
add("_remember_own_row", {"survivor": M("assistant", "a", **{DBP: True},
                                       _retired_durable_rows=[{"role": "assistant", "content": "a", "_own_row": True}])})
add("_remember_own_row", {"survivor": M("assistant", "a", tool_calls=[{"id": "c1"}], **{DBP: True})})
add("_remember_own_row", {"survivor": M("assistant", "a", **{DBP: True}, _unnamed_durable_rows=1)})

# ── _retire_dropped_row (6) ──
add("_retire_dropped_row", {"kept": [M("user", "s", **{DBP: True})], "dropped": M("assistant", "r", **{DBP: True}), "leading": []})
add("_retire_dropped_row", {"kept": [], "dropped": M("assistant", "r", **{DBP: True}), "leading": []})
add("_retire_dropped_row", {"kept": ["not a dict"], "dropped": M("assistant", "r", **{DBP: True}), "leading": []})
add("_retire_dropped_row", {"kept": [M("user", "s", _unnamed_durable_rows=1)],
                            "dropped": M("tool", "r", _unnamed_durable_rows=2), "leading": []})
add("_retire_dropped_row", {"kept": [M("user", "s")], "dropped": M("assistant", "r"), "leading": []})
add("_retire_dropped_row", {"kept": [M("user", "s", **{DBP: True})], "dropped": M("assistant", "r", tool_calls=[{"id": "c1"}], **{DBP: True}), "leading": []})

# ── _retire_leading_drops (6) ──
add("_retire_leading_drops", {"kept": [M("user", "s", **{DBP: True})],
                              "leading": [M("assistant", "r", **{DBP: True})]})
add("_retire_leading_drops", {"kept": [], "leading": [M("assistant", "r", **{DBP: True})]})
add("_retire_leading_drops", {"kept": ["not a dict"], "leading": [M("assistant", "r", **{DBP: True})]})
add("_retire_leading_drops", {"kept": [M("user", "s", **{DBP: True}, _unnamed_durable_rows=1)],
                              "leading": [M("assistant", "a", **{DBP: True}), M("assistant", "b", **{DBP: True})]})
add("_retire_leading_drops", {"kept": [M("user", "s")], "leading": [M("tool", "r", tool_call_id="c1")]})
add("_retire_leading_drops", {"kept": [M("user", "s", **{DBP: True})], "leading": []})

# ── _merge_consecutive_assistants (9) ──
add("_merge_consecutive_assistants", {"messages": [M("user", "hi")]})
add("_merge_consecutive_assistants", {"messages": [M("assistant", "a"), M("assistant", "b")]})
add("_merge_consecutive_assistants", {"messages": [M("user", "hi"), M("assistant", "a"), M("assistant", "b"), M("user", "next")]})
add("_merge_consecutive_assistants", {"messages": [M("assistant", "a", codex_message_items=[{"x": 1}]), M("assistant", "b")]})
add("_merge_consecutive_assistants", {"messages": [M("assistant", "a", finish_reason="verification_required"), M("assistant", "b")]})
add("_merge_consecutive_assistants", {"messages": [M("assistant", "a", tool_calls=[{"id": "c1"}]), M("assistant", "b", tool_calls=[{"id": "c2"}])]})
add("_merge_consecutive_assistants", {"messages": [M("assistant", "a"), M("assistant", "b"), M("assistant", "c")]})
add("_merge_consecutive_assistants", {"messages": [M("assistant", "a"), M("user", "hi"), M("assistant", "b")]})
add("_merge_consecutive_assistants", {"messages": []})

# ── _drop_stray_tool_results (9) ──
add("_drop_stray_tool_results", {"messages": [M("user", "hi"), M("assistant", "ok")]})
add("_drop_stray_tool_results", {"messages": [M("assistant", "call", tool_calls=[{"id": "c1"}]), M("tool", "res", tool_call_id="c1")]})
add("_drop_stray_tool_results", {"messages": [M("assistant", "call"), M("tool", "stray", tool_call_id="c9")]})
add("_drop_stray_tool_results", {"messages": [M("assistant", "call", tool_calls=[{"call_id": "c1|r1"}]), M("tool", "res", tool_call_id="r1")]})
add("_drop_stray_tool_results", {"messages": [M("assistant", "call", tool_calls=[{"id": "c1"}]), M("user", "stop"), M("tool", "orphan", tool_call_id="c1")]})
add("_drop_stray_tool_results", {"messages": [M("assistant", "call", tool_calls=[{"id": "c1"}]), M("tool", "a", tool_call_id="c1"), M("tool", "b", tool_call_id="c1")]})
add("_drop_stray_tool_results", {"messages": [M("tool", "leading", tool_call_id="c0"), M("assistant", "call")]})
add("_drop_stray_tool_results", {"messages": [M("assistant", "call", tool_calls=[{"id": "c1"}, {"id": "c2"}]),
                                             M("tool", "a", tool_call_id="c2"), M("tool", "b", tool_call_id="c1")]})
add("_drop_stray_tool_results", {"messages": [M("tool", "stray", tool_call_id="c9")]})

# ── _prune_unanswered_tool_calls (9) ──
add("_prune_unanswered_tool_calls", {"messages": [M("assistant", "call", tool_calls=[{"id": "c1"}]), M("tool", "r", tool_call_id="c1")]})
add("_prune_unanswered_tool_calls", {"messages": [M("assistant", "call", tool_calls=[{"id": "c1"}, {"id": "c2"}]), M("tool", "r", tool_call_id="c1")]})
add("_prune_unanswered_tool_calls", {"messages": [M("assistant", "", tool_calls=[{"id": "c9"}])]})
add("_prune_unanswered_tool_calls", {"messages": [M("assistant", "call", tool_calls=[{"id": "c9"}], codex_message_items=[{"x": 1}])]})
add("_prune_unanswered_tool_calls", {"messages": [M("assistant", "text", tool_calls=[{"id": "c9"}]), M("user", "next")]})
add("_prune_unanswered_tool_calls", {"messages": [M("assistant", "call", tool_calls=[{"id": "c1"}], **{DBP: True})]})
add("_prune_unanswered_tool_calls", {"messages": [M("assistant", "call", tool_calls=[{"call_id": "c1|r1"}]), M("tool", "r", tool_call_id="r1")]})
add("_prune_unanswered_tool_calls", {"messages": [M("user", "hi")]})
add("_prune_unanswered_tool_calls", {"messages": [M("assistant", "call", tool_calls=[{"id": "c9"}], reasoning="why")]})

# ── _merge_consecutive_users (9) ──
add("_merge_consecutive_users", {"messages": [M("user", "first"), M("user", "second")]})
add("_merge_consecutive_users", {"messages": [M("user", "first"), M("user", "second"), M("user", "third")]})
add("_merge_consecutive_users", {"messages": [M("user", "first", **{"compressed_summary_metadata": True}), M("user", "second")]})
add("_merge_consecutive_users", {"messages": [M("user", "first", display_kind="steer"), M("user", "second")]})
add("_merge_consecutive_users", {"messages": [M("user", "first", observed=True), M("user", "second", observed=False)]})
add("_merge_consecutive_users", {"messages": [M("user", "first", tool_calls=[], **{DBP: True}), M("user", "second", **{DBP: True})]})
add("_merge_consecutive_users", {"messages": [M("user", "first", display_kind="marker", _row_id=10), M("user", "second", _row_id=20)]})
add("_merge_consecutive_users", {"messages": [M("user", "first"), M("user", "")]})
add("_merge_consecutive_users", {"messages": [M("user", "first"), M("assistant", "reply"), M("user", "second")]})

# ── _normalize_sentinel_encoded_content (7) ──
add("_normalize_sentinel_encoded_content", {"messages": [M("user", "plain")]})
add("_normalize_sentinel_encoded_content", {"messages": [M("user", PFX + '{"a": 1}')]})
add("_normalize_sentinel_encoded_content", {"messages": [M("user", PFX + '["x", "y"]')]})
add("_normalize_sentinel_encoded_content", {"messages": [M("user", PFX + 'not json')]})
add("_normalize_sentinel_encoded_content", {"messages": ["not a dict", M("user", PFX + '42')]})
add("_normalize_sentinel_encoded_content", {"messages": [M("user", PFX + '"hi"'), M("assistant", PFX + 'true')]})
add("_normalize_sentinel_encoded_content", {"messages": []})

# ── repair_message_sequence (9) ──
add("repair_message_sequence", {"messages": []})
add("repair_message_sequence", {"messages": [M("user", "hi")]})
add("repair_message_sequence", {"messages": [M("assistant", "a"), M("assistant", "b")]})
add("repair_message_sequence", {"messages": [M("assistant", "call", tool_calls=[{"id": "c1"}]), M("tool", "r", tool_call_id="c1")]})
add("repair_message_sequence", {"messages": [M("assistant", "call", tool_calls=[{"id": "c1"}]), M("tool", "stray", tool_call_id="c9")]})
add("repair_message_sequence", {"messages": [M("user", "a"), M("user", "b"), M("assistant", "c"), M("assistant", "d")]})
add("repair_message_sequence", {"messages": [M("assistant", "call", tool_calls=[{"id": "c1"}, {"id": "c2"}]),
                                             M("tool", "a", tool_call_id="c1"), M("user", "next"), M("user", "more")]})
add("repair_message_sequence", {"messages": [M("tool", "orphan", tool_call_id="c9"), M("assistant", "a"), M("user", "u")]})
add("repair_message_sequence", {"messages": [M("user", PFX + '{"a": 1}'), M("user", "second")]})

OUT.parent.mkdir(parents=True, exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for case in CASES:
        f.write(json.dumps(case, ensure_ascii=False) + "\n")

counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
print(f"wrote {len(CASES)} cases to {OUT}")
for f in sorted(counts):
    print(f"  {f}: {counts[f]}")
