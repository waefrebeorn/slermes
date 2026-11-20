#!/usr/bin/env python3
"""Generate tests/fixtures/turns_flow3.jsonl — input cases for the
remaining-unclaimed turns.py FLOW oracle.

The oracle computes want from the LIVE quarry defs at gate time; this script
only emits {func, in} inputs. Representation contract (shared with C + oracle):
None == JSON null; plain JSON numbers are Python ints; bools are bools. All
text inputs are strings (startswith/find/split domains). A Python tuple return
is transported as a JSON array. No external seams; in.io is always absent.
"""
import copy
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import oracle_turns_flow3 as oracle  # noqa: E402

OUT = HERE / "fixtures" / "turns_flow3.jsonl"
CASES = []

SUM = oracle.SUMMARY_PREFIX
LEG = oracle.LEGACY_SUMMARY_PREFIX
HIST = oracle.HISTORICAL_PREFIXES
DELIM = oracle.MERGED_DELIM
ENDM = oracle.END_MARKER
HEAD = oracle.PRIOR_HEADER
SYNTH = oracle.SYNTH_CONTENTS
PREF = oracle.SYNTH_PREFIXES


def add(func, args):
    oracle.drive(func, {"args": copy.deepcopy(args)})  # fail loudly on a typo
    CASES.append({"func": func, "in": {"args": args}})


def summ(**extra):
    d = {"role": "user", "content": SUM + " body", "_compressed_summary": True}
    d.update(extra); return d


def merged(**extra):
    d = {"role": "user", "content": HEAD + "\nprior tail\n" + DELIM + "\n" + SUM + " x",
         "_compressed_summary": True}
    d.update(extra); return d


# ── _starts_with_summary_prefix ──
add("_starts_with_summary_prefix", {"text": SUM + "hello"})
add("_starts_with_summary_prefix", {"text": LEG + "old"})
add("_starts_with_summary_prefix", {"text": HIST[0] + "z"})
add("_starts_with_summary_prefix", {"text": HIST[-1] + "z"})
add("_starts_with_summary_prefix", {"text": "[CONTEXT COMPACTION — nope]"})
add("_starts_with_summary_prefix", {"text": "plain user text"})
add("_starts_with_summary_prefix", {"text": ""})
add("_starts_with_summary_prefix", {"text": "  " + SUM + "x"})

# ── classify_summary_content ──
add("classify_summary_content", {"content": SUM + "x"})
add("classify_summary_content", {"content": LEG + "x"})
add("classify_summary_content", {"content": "  \n" + SUM + "x"})
add("classify_summary_content", {"content": [{"type": "text", "text": SUM + "x"}]})
add("classify_summary_content", {"content": HEAD + "\nx\n" + DELIM + "\n" + SUM + "y"})
add("classify_summary_content", {"content": HEAD + "\nx\n" + DELIM + "\nnot a summary"})
add("classify_summary_content", {"content": "ordinary text"})
add("classify_summary_content", {"content": None})

# ── _is_context_summary_content ──
add("_is_context_summary_content", {"content": SUM + "x"})
add("_is_context_summary_content", {"content": LEG + "x"})
add("_is_context_summary_content", {"content": HEAD + "\nx\n" + DELIM + "\n" + SUM + "y"})
add("_is_context_summary_content", {"content": "ordinary"})
add("_is_context_summary_content", {"content": None})
add("_is_context_summary_content", {"content": ["a", {"type": "text", "text": SUM + "x"}]})

# ── _has_compressed_summary_metadata ──
add("_has_compressed_summary_metadata", {"message": {"_compressed_summary": True}})
add("_has_compressed_summary_metadata", {"message": {"_compressed_summary": False}})
add("_has_compressed_summary_metadata", {"message": {"_compressed_summary": "x"}})
add("_has_compressed_summary_metadata", {"message": {"_compressed_summary": 0}})
add("_has_compressed_summary_metadata", {"message": {"_compressed_summary": [1]}})
add("_has_compressed_summary_metadata", {"message": {}})
add("_has_compressed_summary_metadata", {"message": None})

# ── _is_context_summary_message ──
add("_is_context_summary_message", {"message": {"_compressed_summary": True}})
add("_is_context_summary_message", {"message": {"content": SUM + "x"}})
add("_is_context_summary_message", {"message": {"content": "hello"}})
add("_is_context_summary_message", {"message": {"_compressed_summary": False, "content": "hello"}})
add("_is_context_summary_message", {"message": {"role": "user", "content": "x"}})
add("_is_context_summary_message", {"message": "a string"})
add("_is_context_summary_message", {"message": None})

# ── _is_synthetic_compression_user_turn ──
add("_is_synthetic_compression_user_turn", {"message": {"role": "user", "content": SYNTH[0]}})
add("_is_synthetic_compression_user_turn", {"message": {"role": "user", "content": SYNTH[2]}})
add("_is_synthetic_compression_user_turn", {"message": {"role": "user", "content": "  " + SYNTH[0] + "  "}})
add("_is_synthetic_compression_user_turn", {"message": {"role": "user", "content": PREF[0] + " job done"}})
add("_is_synthetic_compression_user_turn", {"message": {"role": "user", "content": PREF[1] + "do it"}})
add("_is_synthetic_compression_user_turn", {"message": {"role": "user", "content": " a real question "}})
add("_is_synthetic_compression_user_turn", {"message": {"role": "assistant", "content": SYNTH[0]}})
add("_is_synthetic_compression_user_turn", {"message": summ()})

# ── _is_blank_user_turn ──
add("_is_blank_user_turn", {"message": {"role": "user", "content": None}})
add("_is_blank_user_turn", {"message": {"role": "user", "content": "   "}})
add("_is_blank_user_turn", {"message": {"role": "user", "content": "hello"}})
add("_is_blank_user_turn", {"message": {"role": "user", "content": []}})
add("_is_blank_user_turn", {"message": {"role": "user", "content": [{"type": "text", "text": "  "}]}})
add("_is_blank_user_turn", {"message": {"role": "user", "content": [{"type": "text", "text": "x"}]}})
add("_is_blank_user_turn", {"message": {"role": "user", "content": [{"type": "image_url", "image_url": {"url": "u"}}]}})
add("_is_blank_user_turn", {"message": {"role": "assistant", "content": ""}})
add("_is_blank_user_turn", {"message": summ()})

# ── _is_actionable_user_turn ──
add("_is_actionable_user_turn", {"message": {"role": "user", "content": "hi"}})
add("_is_actionable_user_turn", {"message": {"role": "user", "content": ""}})
add("_is_actionable_user_turn", {"message": {"role": "user", "content": "hi", "display_kind": "steer"}})
add("_is_actionable_user_turn", {"message": {"role": "user", "content": "hi", "display_kind": "hidden"}})
add("_is_actionable_user_turn", {"message": summ()})
add("_is_actionable_user_turn", {"message": {"role": "assistant", "content": "hi"}})
add("_is_actionable_user_turn", {"message": {"role": "user", "content": None}})

# ── _is_real_user_turn ──
add("_is_real_user_turn", {"message": {"role": "user", "content": "hi"}})
add("_is_real_user_turn", {"message": {"role": "user", "content": SYNTH[0]}})
add("_is_real_user_turn", {"message": {"role": "user", "content": ""}})
add("_is_real_user_turn", {"message": {"role": "user", "content": "hi", "display_kind": "steer"}})
add("_is_real_user_turn", {"message": {"role": "user", "content": "hi", "display_kind": "hidden"}})
add("_is_real_user_turn", {"message": {"role": "assistant", "content": "hi"}})
add("_is_real_user_turn", {"message": summ()})

# ── is_compaction_summary_message ──
add("is_compaction_summary_message", {"message": {"_compressed_summary": True}})
add("is_compaction_summary_message", {"message": {"content": SUM + "x"}})
add("is_compaction_summary_message", {"message": SUM + "x"})
add("is_compaction_summary_message", {"message": "hello"})
add("is_compaction_summary_message", {"message": {"content": "hello"}})
add("is_compaction_summary_message", {"message": None})

# ── _strip_context_summary_handoff_message ──
add("_strip_context_summary_handoff_message", {"message": "hello"})
add("_strip_context_summary_handoff_message", {"message": {"role": "user", "content": "hi"}})
add("_strip_context_summary_handoff_message", {"message": summ()})  # standalone -> None
add("_strip_context_summary_handoff_message", {"message": merged()})
add("_strip_context_summary_handoff_message", {"message": summ(content="sum body\n" + ENDM + "\nlive tail")})
add("_strip_context_summary_handoff_message", {"message": summ(content=[
    {"type": "text", "text": HEAD + "\nprior text"},
    {"type": "text", "text": DELIM + "\nsummary"},
])})
add("_strip_context_summary_handoff_message", {"message": summ(content=[
    {"type": "text", "text": "sum" + ENDM + "live tail"},
    {"type": "text", "text": "later"},
])})
add("_strip_context_summary_handoff_message", {"message": summ(content=1234)})

# ── _handoff_only_content ──
add("_handoff_only_content", {"content": SUM + "x"})
add("_handoff_only_content", {"content": "head" + ENDM + "tail"})
add("_handoff_only_content", {"content": HEAD + "x\n" + DELIM + "\n" + SUM + "y\n" + ENDM + "live"})
add("_handoff_only_content", {"content": [{"type": "text", "text": "x"},
                                          {"type": "text", "text": DELIM + "\n" + SUM + "y"}]})
add("_handoff_only_content", {"content": [{"type": "text", "text": "head" + ENDM + "live"},
                                          {"type": "text", "text": "later"}]})
add("_handoff_only_content", {"content": [{"type": "text", "text": "a"}, "b"]})
add("_handoff_only_content", {"content": None})
add("_handoff_only_content", {"content": 7})

# ── split_user_originated_turn ──
add("split_user_originated_turn", {"message": {"role": "assistant", "content": "x"}})
add("split_user_originated_turn", {"message": "not a dict"})
add("split_user_originated_turn", {"message": {"role": "user", "content": "hi"}})
add("split_user_originated_turn", {"message": {"role": "user", "content": "hi", "display_kind": "hidden"}})
add("split_user_originated_turn", {"message": {"role": "user", "content": "hi", "display_kind": "steer"}})
add("split_user_originated_turn", {"message": summ()})
add("split_user_originated_turn", {"message": merged()})
add("split_user_originated_turn", {"message": {"role": "user", "content": "hi", "api_content": "stale"}})
add("split_user_originated_turn", {"message": {"role": "user", "content": "hi",
    "display_metadata": {"reactions": [1], "typed_ask": "ask"}}})
add("split_user_originated_turn", {"message": {"role": "user", "content": SYNTH[0]}})
add("split_user_originated_turn", {"message": {"role": "user", "content": SUM + "x",
    "_compressed_summary": True, "_row_id": 5, "timestamp": 123}})

# ── user_originated_turn_view ──
add("user_originated_turn_view", {"message": {"role": "user", "content": "hi"}})
add("user_originated_turn_view", {"message": {"role": "assistant", "content": "x"}})
add("user_originated_turn_view", {"message": summ()})
add("user_originated_turn_view", {"message": merged()})
add("user_originated_turn_view", {"message": {"role": "user", "content": SYNTH[0]}})
add("user_originated_turn_view", {"message": {"role": "user", "content": "hi", "display_kind": "steer"}})
add("user_originated_turn_view", {"message": "not a dict"})

# ── history_before_user_originated_turn ──
add("history_before_user_originated_turn", {"messages": [{"role": "user", "content": "hi"}], "index": -1})
add("history_before_user_originated_turn", {"messages": [{"role": "user", "content": "hi"}], "index": 3})
add("history_before_user_originated_turn", {"messages": [{"role": "assistant", "content": "x"}], "index": 0})
add("history_before_user_originated_turn", {"messages": [
    {"role": "system", "content": "s"}, {"role": "user", "content": "hi"}], "index": 1})
add("history_before_user_originated_turn", {"messages": [merged()], "index": 0})
add("history_before_user_originated_turn", {"messages": [
    {"role": "user", "content": oracle.PRE_LLM_BLOCK, "display_metadata": {"withheld_prompt": "real ask"}}], "index": 0})
add("history_before_user_originated_turn", {"messages": [
    {"role": "user", "content": "a"}, {"role": "assistant", "content": "b"},
    {"role": "user", "content": "c"}], "index": 2})

# ── project_compaction_message_for_display ──
add("project_compaction_message_for_display", {"message": "hello"})
add("project_compaction_message_for_display", {"message": {"role": "user", "content": "hi"}})
add("project_compaction_message_for_display", {"message": summ()})
add("project_compaction_message_for_display", {"message": merged(tool_calls=[{"id": "c"}],
    reasoning_content="rc", display_kind="hidden")})
add("project_compaction_message_for_display", {"message": merged(display_kind="refused")})
add("project_compaction_message_for_display", {"message": {"content": SUM + "x"}})

# ── user_indices ──
add("user_indices", {"messages": []})
add("user_indices", {"messages": [{"role": "user", "content": "hi"}]})
add("user_indices", {"messages": [{"role": "assistant", "content": "x"}]})
add("user_indices", {"messages": [
    {"role": "system", "content": "s"}, {"role": "user", "content": "hi"},
    {"role": "assistant", "content": "a"}, {"role": "user", "content": "yo"}]})
add("user_indices", {"messages": [
    {"role": "user", "content": "hi"}, {"role": "user", "content": SYNTH[0]}]})
add("user_indices", {"messages": [{"role": "user", "content": "hi", "display_kind": "steer"}]})
add("user_indices", {"messages": [{"role": "user", "content": "hi", "display_kind": "hidden"}]})

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
