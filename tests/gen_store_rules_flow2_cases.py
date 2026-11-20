#!/usr/bin/env python3
"""Generate tests/fixtures/store_rules_flow2.jsonl — input cases for the
unclaimed turn-policy leaves FLOW oracle.

The oracle computes want from the LIVE quarry defs at gate time; this script
only emits {func, in} inputs. This cluster has no external collaborators, so
in.io is always absent. Override/sequence machinery is intentionally unused.

Representation contract (shared with the C port and the oracle):
- None == JSON null; plain JSON numbers are Python ints; JSON bools are bools.
- A Python float travels as {"__float__": true, "v": <number>}; a tuple as
  {"__tuple__": true, "v": [...]}.
- Fixture domain for these pure functions: str / dict / list / int / bool / null
  values only; text parts of type "text" carry str values.
"""
import copy
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import oracle_store_rules_flow2 as oracle  # noqa: E402

OUT = HERE / "fixtures" / "store_rules_flow2.jsonl"
CASES = []


def add(func, args):
    # validate the live drive once at generation time (fail loudly on a typo)
    oracle.drive(func, {"args": copy.deepcopy(args)})
    CASES.append({"func": func, "in": {"args": args}})


# ── _field ──
add("_field", {"value": {"text": "hello", "type": "text"}, "key": "text"})
add("_field", {"value": {"type": "text"}, "key": "missing"})
add("_field", {"value": "a string", "key": "text"})
add("_field", {"value": None, "key": "text"})
add("_field", {"value": [1, 2], "key": "text"})
add("_field", {"value": {"type": "image", "url": "u"}, "key": "type"})
add("_field", {"value": 42, "key": "text"})
add("_field", {"value": {"text": None}, "key": "text"})

# ── _part_text ──
add("_part_text", {"item": "plain string"})
add("_part_text", {"item": {"type": "text", "text": "body"}})
add("_part_text", {"item": {"type": "image"}})
add("_part_text", {"item": {"text": 5}})
add("_part_text", {"item": None})
add("_part_text", {"item": [1, 2]})
add("_part_text", {"item": {"text": ""}})

# ── _with_part_text ──
add("_with_part_text", {"item": {"type": "text", "text": "old"}, "text": "new"})
add("_with_part_text", {"item": {"type": "text"}, "text": "new"})
add("_with_part_text", {"item": {"type": "text", "text": "old", "extra": 1}, "text": "x"})
add("_with_part_text", {"item": "old", "text": "new"})
add("_with_part_text", {"item": None, "text": "new"})
add("_with_part_text", {"item": 5, "text": "new"})
add("_with_part_text", {"item": {}, "text": ""})

# ── _content_text_for_contains ──
add("_content_text_for_contains", {"content": "plain text"})
add("_content_text_for_contains", {"content": None})
add("_content_text_for_contains", {"content": ""})
add("_content_text_for_contains", {"content": [{"text": "a"}, {"text": "b"}]})
add("_content_text_for_contains", {"content": [{"text": "a"}, "b", {"type": "image"}]})
add("_content_text_for_contains", {"content": []})
add("_content_text_for_contains", {"content": [{"text": ""}, {"text": "x"}]})
add("_content_text_for_contains", {"content": ["first", {"text": "second"}, 5]})

# ── is_content_blank ──
add("is_content_blank", {"content": None})
add("is_content_blank", {"content": ""})
add("is_content_blank", {"content": "   \t\n "})
add("is_content_blank", {"content": "x"})
add("is_content_blank", {"content": " \x1c "})
add("is_content_blank", {"content": [{"type": "text", "text": "   "}]})
add("is_content_blank", {"content": [{"type": "text", "text": "hi"}]})
add("is_content_blank", {"content": []})
add("is_content_blank", {"content": [{"type": "image"}]})
add("is_content_blank", {"content": {"a": 1}})
add("is_content_blank", {"content": 5})
add("is_content_blank", {"content": [{"type": "text", "text": ""}]})

# ── drop_stale_api_content ──
add("drop_stale_api_content", {"msg": {"role": "user", "api_content": [1]}})
add("drop_stale_api_content", {"msg": {"role": "user"}})
add("drop_stale_api_content", {"msg": {}})
add("drop_stale_api_content", {"msg": {"api_content": None, "content": "x"}})
add("drop_stale_api_content", {"msg": None})
add("drop_stale_api_content", {"msg": 42})
add("drop_stale_api_content", {"msg": "a string"})

# ── _display_metadata_str ──
add("_display_metadata_str", {"message": {"display_metadata": {"withheld_prompt": "real ask"}}, "key": "withheld_prompt"})
add("_display_metadata_str", {"message": {"display_metadata": {"withheld_prompt": 5}}, "key": "withheld_prompt"})
add("_display_metadata_str", {"message": {"display_metadata": {"other": "x"}}, "key": "withheld_prompt"})
add("_display_metadata_str", {"message": {"display_metadata": "not-a-dict"}, "key": "withheld_prompt"})
add("_display_metadata_str", {"message": {"role": "user"}, "key": "withheld_prompt"})
add("_display_metadata_str", {"message": None, "key": "withheld_prompt"})
add("_display_metadata_str", {"message": [1], "key": "withheld_prompt"})

# ── withheld_prompt_text ──
add("withheld_prompt_text", {"message": {"display_metadata": {"withheld_prompt": "real ask"}}})
add("withheld_prompt_text", {"message": {"display_metadata": {"withheld_prompt": ""}}})
add("withheld_prompt_text", {"message": {"display_metadata": {"withheld_prompt": 5}}})
add("withheld_prompt_text", {"message": {"display_metadata": {"other": "x"}}})
add("withheld_prompt_text", {"message": None})
add("withheld_prompt_text", {"message": {"role": "user"}})

# ── typed_prompt_content ──
PH = oracle.PRE_LLM_BLOCK_WITHHELD_PROMPT
add("typed_prompt_content", {"content": "before " + PH + " after",
                             "message": {"display_metadata": {"withheld_prompt": "REAL"}}})
add("typed_prompt_content", {"content": "no placeholder",
                             "message": {"display_metadata": {"withheld_prompt": "REAL"}}})
add("typed_prompt_content", {"content": PH + PH,
                             "message": {"display_metadata": {"withheld_prompt": "X"}}})
add("typed_prompt_content", {"content": [{"type": "text", "text": "p " + PH}, "tail"],
                             "message": {"display_metadata": {"withheld_prompt": "REAL"}}})
add("typed_prompt_content", {"content": [{"type": "image"}, {"text": PH}],
                             "message": {"display_metadata": {"withheld_prompt": "REAL"}}})
add("typed_prompt_content", {"content": "unchanged",
                             "message": {"display_metadata": {}}})
add("typed_prompt_content", {"content": 5,
                             "message": {"display_metadata": {"withheld_prompt": "REAL"}}})
add("typed_prompt_content", {"content": None,
                             "message": {"display_metadata": {"withheld_prompt": "REAL"}}})
add("typed_prompt_content", {"content": PH,
                             "message": {"display_metadata": {"withheld_prompt": ""}}})

# ── retryable_user_text ──
add("retryable_user_text", {"content": "hello"})
add("retryable_user_text", {"content": "  leading and trailing  "})
add("retryable_user_text", {"content": "   "})
add("retryable_user_text", {"content": ""})
add("retryable_user_text", {"content": ["a", "b", "c"]})
add("retryable_user_text", {"content": [{"type": "text", "text": "one"},
                                         {"type": "input_text", "text": "two"}]})
add("retryable_user_text", {"content": [{"type": "image", "image_url": "u"}]})
add("retryable_user_text", {"content": [{"type": "text", "text": "x", "extra": 1}]})
add("retryable_user_text", {"content": [{"type": "text", "text": 5}]})
add("retryable_user_text", {"content": 5})
add("retryable_user_text", {"content": ["a", 5]})
add("retryable_user_text", {"content": []})
add("retryable_user_text", {"content": [{"type": "output_text", "text": "ok"}]})


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    missing = [k for k in oracle.KINDS if counts.get(k, 0) < 5]
    if missing:
        print(f"FATAL gen_store_rules_flow2: funcs with <5 cases: {missing}")
        sys.exit(2)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "wb") as f:
        for c in CASES:
            line = json.dumps(c, ensure_ascii=False)
            f.write(line.encode("utf-8", errors="surrogatepass") + b"\n")
    print(f"gen_store_rules_flow2: {len(CASES)} cases -> {OUT}")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")


if __name__ == "__main__":
    main()
