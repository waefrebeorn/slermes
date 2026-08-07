"""Oracle for gateway/platforms/helpers.py markdown chunking pure functions."""
import json, os, sys

DEV_ROOT = os.environ.get("HERMES_DEV_ROOT")
if DEV_ROOT and DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

from gateway.platforms.helpers import (
    text_has_unclosed_fence,
    text_ends_with_table_row,
    is_fence_atom,
    is_table_atom,
    split_at_paragraph_boundary,
    split_markdown_atoms,
    infer_block_separator,
    merge_streaming_fences,
    balance_fences_across_chunks,
    split_text_fence_aware,
)

cases_file = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(__file__),
    "oracle", "fixtures", "platforms_helpers_md", "cases.in",
)

with open(cases_file) as f:
    cases = json.load(f)

for c in cases:
    op = c.get("op", "")
    val = c.get("value", "")

    if op == "text_has_unclosed_fence":
        print(text_has_unclosed_fence(val))
    elif op == "text_ends_with_table_row":
        print(text_ends_with_table_row(val))
    elif op == "is_fence_atom":
        print(is_fence_atom(val))
    elif op == "is_table_atom":
        print(is_table_atom(val))
    elif op == "split_at_paragraph_boundary":
        max_chars = c.get("max_chars", 50)
        head, tail = split_at_paragraph_boundary(val, max_chars)
        print(json.dumps({"head": head, "tail": tail}))
    elif op == "split_markdown_atoms":
        print(json.dumps(split_markdown_atoms(val)))
    elif op == "infer_block_separator":
        nxt = c.get("next", "")
        print(repr(infer_block_separator(val, nxt)))
    elif op == "merge_streaming_fences":
        chunks = c.get("chunks", [])
        print(json.dumps(merge_streaming_fences(chunks)))
    elif op == "balance_fences_across_chunks":
        chunks = c.get("chunks", [])
        print(json.dumps(balance_fences_across_chunks(chunks)))
    elif op == "split_text_fence_aware":
        limit = c.get("limit", 50)
        pref = c.get("prefer_paragraphs", True)
        bal = c.get("balance_fences", False)
        print(json.dumps(split_text_fence_aware(val, limit, prefer_paragraphs=pref, balance_fences=bal)))
