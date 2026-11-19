#!/usr/bin/env python3
"""Generate tests/fixtures/platforms_helpers_flow.jsonl — input cases for the
platform-helper FLOW oracle (the oracle computes want from LIVE helpers.py at
gate time; this script only emits {func, in} inputs).

Deterministic: handcrafted branch-coverage cases plus seeded-random breadth
cases in the domains verified by the algorithm prototype (reply_file_parts
limits >= 4 for non-ASCII text; ASCII-only strip corpus; int/bool/None choice
str() projections; integer len_fn scripts).
"""
import json
import os
import random
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "platforms_helpers_flow.jsonl"
CASES = []


def add(func, args, io=None, state=None):
    inp = {"args": args}
    if io:
        inp["io"] = io
    if state is not None:
        inp["state"] = state
    CASES.append({"func": func, "in": inp})


def fl(v):
    return {"__float__": True, "v": v}


# ── display_width ──
add("display_width", {"text": ""})
add("display_width", {"text": "hello world"})
add("display_width", {"text": "한국어 라벨"})
add("display_width", {"text": "ａｂｃ"})
add("display_width", {"text": "a中é😀b"})
add("display_width", {"text": "┌─┐│"})
add("display_width", {"text": "\u3000\u4e2d"})

# ── _clip_text ──
add("_clip_text", {"text": "short", "limit": 10, "len_fn": None})
add("_clip_text", {"text": "hello world", "limit": 5, "len_fn": None})
add("_clip_text", {"text": "abc", "limit": 0, "len_fn": None})
add("_clip_text", {"text": "abc", "limit": -2, "len_fn": None})
add("_clip_text", {"text": "中文测试", "limit": 3, "len_fn": None})
add("_clip_text", {"text": "abcdef", "limit": 3, "len_fn": "seam"}, {"len_fn": 100})
add("_clip_text", {"text": "abcdef", "limit": 3, "len_fn": "seam"}, {"len_fn": 2})
add("_clip_text", {"text": "abcde", "limit": 3, "len_fn": "seam"}, {"len_fn": [10, 4, 2]})

# ── _shorten_choice ──
add("_shorten_choice", {"text": "short", "each": 10, "anchor": 0})
add("_shorten_choice", {"text": "abcdefghijklmnop", "each": 8, "anchor": 0})
add("_shorten_choice", {"text": "abcdefghijklmnop", "each": 8, "anchor": 15})
add("_shorten_choice", {"text": "abcdefghijklmnop", "each": 8, "anchor": 8})
add("_shorten_choice", {"text": "abcdefgh", "each": 3, "anchor": 3})
add("_shorten_choice", {"text": "abc", "each": 2, "anchor": 3})
add("_shorten_choice", {"text": "中文测试字符串", "each": 4, "anchor": 3})
add("_shorten_choice", {"text": "abc", "each": 1, "anchor": 1})
add("_shorten_choice", {"text": "ab", "each": 1, "anchor": 1})

# ── _distinct_shortenings ──
def prefix_matrix(texts):
    return [[len(os.path.commonprefix([a, b])) for b in texts] for a in texts]


for texts, each in [
    (["same", "same"], 10),
    (["alpha long option one", "alpha long option two"], 12),
    (["prefix AAA suffix", "prefix BBB suffix", "prefix CCC suffix"], 10),
    (["only one"], 3),
    (["ab", "ab", "cd"], 5),
    (["x common y one", "x common y two", "z different"], 8),
    (["repeated wording here", "repeated wording there"], 1),
]:
    add("_distinct_shortenings", {"texts": texts, "each": each, "prefix": prefix_matrix(texts)})

# ── numbered_clarify_choices ──
add("numbered_clarify_choices", {"choices": ["a", "b"], "max_label_cols": 10,
                                 "escape": None, "max_chars": None, "len_fn": None})
add("numbered_clarify_choices", {"choices": ["short", "a very long choice label"],
                                 "max_label_cols": 5, "escape": None, "max_chars": None, "len_fn": None})
add("numbered_clarify_choices", {"choices": ["  padded  ", "x"], "max_label_cols": 3,
                                 "escape": None, "max_chars": None, "len_fn": None})
add("numbered_clarify_choices", {"choices": ["a long option to shrink", "another long option here"],
                                 "max_label_cols": 0, "escape": None, "max_chars": 20, "len_fn": None})
add("numbered_clarify_choices", {"choices": ["aa", "bb"], "max_label_cols": 0,
                                 "escape": "seam", "max_chars": None, "len_fn": None},
    {"escape": "<x>"})
add("numbered_clarify_choices", {"choices": ["aa", "bb"], "max_label_cols": 0,
                                 "escape": "seam", "max_chars": None, "len_fn": None},
    {"escape": ["<1>", "<2>"]})
add("numbered_clarify_choices", {"choices": ["long one", "long two"], "max_label_cols": 0,
                                 "escape": None, "max_chars": 10, "len_fn": "seam"},
    {"len_fn": 5})
add("numbered_clarify_choices", {"choices": ["a long option to shrink", "another long option here"],
                                 "max_label_cols": 0, "escape": None, "max_chars": 20, "len_fn": "seam"},
    {"len_fn": 100})
add("numbered_clarify_choices", {"choices": [], "max_label_cols": 0,
                                 "escape": None, "max_chars": None, "len_fn": None})
add("numbered_clarify_choices", {"choices": [123, None, True], "max_label_cols": 0,
                                 "escape": None, "max_chars": None, "len_fn": None})
add("numbered_clarify_choices", {"choices": ["long option number one", "long option number two"],
                                 "max_label_cols": 0, "escape": None, "max_chars": 0, "len_fn": None})

# ── clarify_text_with_choices ──
add("clarify_text_with_choices", {"head": "Question?", "choices": ["a", "b"],
                                  "limit": 100, "max_label_cols": 10, "escape": None, "len_fn": None})
add("clarify_text_with_choices", {"head": "Which option do you prefer for the deployment?",
                                  "choices": ["the first long option", "the second long option"],
                                  "limit": 60, "max_label_cols": 5, "escape": None, "len_fn": None})
add("clarify_text_with_choices", {"head": "Pick one please", "choices": ["long option aaa", "long option bbb"],
                                  "limit": 10, "max_label_cols": 5, "escape": None, "len_fn": None})
add("clarify_text_with_choices", {"head": "Pick one please", "choices": ["a very long option"],
                                  "limit": 10, "max_label_cols": 5, "escape": None, "len_fn": "seam"},
    {"len_fn": 8})
add("clarify_text_with_choices", {"head": "", "choices": ["long option one", "long option two"],
                                  "limit": 30, "max_label_cols": 5, "escape": None, "len_fn": None})
add("clarify_text_with_choices", {"head": "A long head that will be clipped", "choices": ["x", "y"],
                                  "limit": -5, "max_label_cols": 10, "escape": None, "len_fn": None})
add("clarify_text_with_choices", {"head": "Pick", "choices": ["option alpha", "option beta"],
                                  "limit": 40, "max_label_cols": 3, "escape": "seam", "len_fn": None},
    {"escape": "E"})

# ── _keep_link_target ──
add("_keep_link_target", {"group1": "label", "group2": "https://x.com"})
add("_keep_link_target", {"group1": "https://x.com", "group2": "https://x.com"})
add("_keep_link_target", {"group1": "mail", "group2": "mailto:a@b.c"})
add("_keep_link_target", {"group1": " padded ", "group2": " https://x.com "})
add("_keep_link_target", {"group1": "x", "group2": "HTTPS://X.COM"})
add("_keep_link_target", {"group1": "rel", "group2": "relative/path"})
add("_keep_link_target", {"group1": " pad ", "group2": "mailto:a"})
add("_keep_link_target", {"group1": "x", "group2": "http://"})

# ── bounded_put ──
add("bounded_put", {"store": {"a": 1, "b": 2}, "key": "c", "value": 3, "cap": 3})
add("bounded_put", {"store": {"a": 1, "b": 2, "c": 3}, "key": "a", "value": 9, "cap": 3})
add("bounded_put", {"store": {"a": 1, "b": 2, "c": 3}, "key": "d", "value": 4, "cap": 3})
add("bounded_put", {"store": {"a": 1, "b": 2}, "key": "c", "value": 3, "cap": 0})
add("bounded_put", {"store": {"a": 1, "b": 2}, "key": "b", "value": 5, "cap": 1})
add("bounded_put", {"store": {}, "key": "k", "value": None, "cap": 2})
add("bounded_put", {"store": {}, "key": "x", "value": {"nested": True}, "cap": 2})
add("bounded_put", {"store": {"a": 1}, "key": "b", "value": 2, "cap": -1})

# ── table_row_start ──
T1 = "| a | b |\n|---|---|\n| 1 | 2 |"
add("table_row_start", {"text": T1, "split_at": 22})
add("table_row_start", {"text": T1, "split_at": 0})
add("table_row_start", {"text": T1, "split_at": 9})
add("table_row_start", {"text": T1, "split_at": len(T1)})
add("table_row_start", {"text": "hello\nworld", "split_at": 8})
add("table_row_start", {"text": T1, "split_at": 3})
add("table_row_start", {"text": T1, "split_at": 12})
add("table_row_start", {"text": "a|b\nc|d", "split_at": 5})

# ── table_rollback_point ──
T2 = "| a | b |\n|---|---|\n| 1 | 2 |\n| 3 | 4 |"
add("table_rollback_point", {"text": T2, "split_at": 22, "fits": "seam"}, {"fits": True})
add("table_rollback_point", {"text": T2, "split_at": 22, "fits": "seam"}, {"fits": False})
add("table_rollback_point", {"text": T2, "split_at": 5, "fits": "seam"}, {"fits": True})
add("table_rollback_point", {"text": "intro\n| a | b |\n|---|---|\n| 1 | 2 |",
                             "split_at": 28, "fits": "seam"}, {"fits": True})
add("table_rollback_point", {"text": "| a | b |\n|---|---|\nplain", "split_at": 12, "fits": "seam"},
    {"fits": True})
add("table_rollback_point", {"text": "plain\ntext", "split_at": 3, "fits": "seam"}, {"fits": True})
add("table_rollback_point", {"text": T2, "split_at": 22, "fits": "seam"},
    {"fits": {"__script__": True, "raise": {"type": "ValueError", "message": "fits blew up"}}})

# ── table_header_to_repeat ──
add("table_header_to_repeat", {"chunk": "| a | b |\n|---|---|\n| 1 | 2 |", "rest": "| 3 | 4 |",
                               "max_units": 100, "len_fn": None})
add("table_header_to_repeat", {"chunk": "| a | b |\n|---|---|\n| 1 | 2 |", "rest": "| 3 | 4 |",
                               "max_units": 5, "len_fn": None})
add("table_header_to_repeat", {"chunk": "plain", "rest": "text", "max_units": 100, "len_fn": None})
add("table_header_to_repeat", {"chunk": "| a | b |\n| 1 | 2 |", "rest": "| 3 |",
                               "max_units": 100, "len_fn": None})
add("table_header_to_repeat", {"chunk": "| a | b |\n|---|---|\n| 1 | 2 |", "rest": "| 3 | 4 |",
                               "max_units": 5, "len_fn": "seam"}, {"len_fn": 5})
add("table_header_to_repeat", {"chunk": "| a | b |\n|---|---|\n| 1 | 2 |", "rest": "| 3 | 4 |",
                               "max_units": 0, "len_fn": "seam"}, {"len_fn": 1})
add("table_header_to_repeat", {"chunk": "| a | b |\n|---|---|\n| 1 |\n", "rest": "| 2 |",
                               "max_units": 100, "len_fn": None})

# ── is_discord_channel_obfuscated ──
add("is_discord_channel_obfuscated", {"channel": {"flags": {"value": 131072}, "name": "general"}})
add("is_discord_channel_obfuscated", {"channel": {"flags": {"value": 0}, "name": "___hidden___"}})
add("is_discord_channel_obfuscated", {"channel": {"flags": {"value": 0}, "name": "general"}})
add("is_discord_channel_obfuscated", {"channel": {"flags": {}, "name": "___hidden___"}})
add("is_discord_channel_obfuscated", {"channel": {"name": "general"}})
add("is_discord_channel_obfuscated", {"channel": {"flags": {"value": fl(131072.0)}, "name": "general"}})
add("is_discord_channel_obfuscated", {"channel": {"flags": {"value": True}, "name": "general"}})
add("is_discord_channel_obfuscated", {"channel": {"flags": {"value": -1}, "name": "x"}})
add("is_discord_channel_obfuscated", {"channel": {"flags": None, "name": "___hidden___"}})
add("is_discord_channel_obfuscated", {"channel": {"flags": {"value": 131072}}})
add("is_discord_channel_obfuscated", {"channel": {"flags": {"value": "131072"}, "name": "___hidden___"}})
add("is_discord_channel_obfuscated", {"channel": {}})

# ── reply_document_sender ──
add("reply_document_sender", {"adapter": {"hooks": {}, "attrs": {}}})
add("reply_document_sender", {"adapter": {"hooks": {"send_reply_document": "own"}, "attrs": {}}})
add("reply_document_sender", {"adapter": {"hooks": {"send_document": "own"}, "attrs": {}}})
add("reply_document_sender", {"adapter": {"hooks": {"send_document": "null"}, "attrs": {}}})
add("reply_document_sender", {"adapter": {"hooks": {"send_reply_document": 5, "send_document": "own"},
                                          "attrs": {}}})
add("reply_document_sender", {"adapter": {"hooks": {"send_reply_document": "own", "send_document": "own"},
                                          "attrs": {}}})
add("reply_document_sender", {"adapter": {"hooks": {"send_reply_document": None}, "attrs": {}}})

# ── reply_message_cap ──
add("reply_message_cap", {"adapter": {"hooks": {"send_document": "own"},
                                      "attrs": {"MAX_SPLIT_MESSAGES": 5}}})
add("reply_message_cap", {"adapter": {"hooks": {}, "attrs": {"MAX_SPLIT_MESSAGES": 5}}})
add("reply_message_cap", {"adapter": {"hooks": {"send_document": "own"},
                                      "attrs": {"MAX_SPLIT_MESSAGES": 1}}})
add("reply_message_cap", {"adapter": {"hooks": {"send_document": "own"},
                                      "attrs": {"MAX_SPLIT_MESSAGES": 0}}})
add("reply_message_cap", {"adapter": {"hooks": {"send_document": "own"},
                                      "attrs": {"MAX_SPLIT_MESSAGES": True}}})
add("reply_message_cap", {"adapter": {"hooks": {"send_document": "own"},
                                      "attrs": {"MAX_SPLIT_MESSAGES": fl(5.0)}}})
add("reply_message_cap", {"adapter": {"hooks": {"send_document": "own"}, "attrs": {}}})
add("reply_message_cap", {"adapter": {"hooks": {"send_reply_document": "own"},
                                      "attrs": {"MAX_SPLIT_MESSAGES": 5}}})
add("reply_message_cap", {"adapter": {"hooks": {"send_document": "own"},
                                      "attrs": {"MAX_SPLIT_MESSAGES": "5"}}})

# ── _over_reply_cap ──
CAP5 = {"hooks": {"send_document": "own", "reply_message_count": "own"},
        "attrs": {"MAX_SPLIT_MESSAGES": 5, "MAX_MESSAGE_LENGTH": 10}}
add("_over_reply_cap", {"adapter": CAP5, "content": "x" * 100, "metadata": None},
    {"reply_message_count": 7})
add("_over_reply_cap", {"adapter": CAP5, "content": "x" * 100, "metadata": {"k": 1}},
    {"reply_message_count": 3})
add("_over_reply_cap", {"adapter": CAP5, "content": "", "metadata": None},
    {"reply_message_count": 7})
add("_over_reply_cap", {"adapter": {"hooks": {"send_document": "own", "reply_message_count": "own"},
                                    "attrs": {"MAX_SPLIT_MESSAGES": 5, "MAX_MESSAGE_LENGTH": 100}},
                        "content": "x" * 5, "metadata": None}, {"reply_message_count": 7})
add("_over_reply_cap", {"adapter": {"hooks": {"send_document": "own"},
                                    "attrs": {"MAX_SPLIT_MESSAGES": 5, "MAX_MESSAGE_LENGTH": 10}},
                        "content": "x" * 100, "metadata": None}, {"reply_message_count": 7})
add("_over_reply_cap", {"adapter": {"hooks": {}, "attrs": {"MAX_MESSAGE_LENGTH": 10}},
                        "content": "x" * 100, "metadata": None}, {"reply_message_count": 7})
add("_over_reply_cap", {"adapter": {"hooks": {"send_document": "own", "reply_message_count": "own"},
                                    "attrs": {"MAX_SPLIT_MESSAGES": 5, "MAX_MESSAGE_LENGTH": "abc"}},
                        "content": "x" * 100, "metadata": None}, {"reply_message_count": 7})
add("_over_reply_cap", {"adapter": {"hooks": {"send_document": "own", "reply_message_count": "own"},
                                    "attrs": {"MAX_SPLIT_MESSAGES": 5, "MAX_MESSAGE_LENGTH": None}},
                        "content": "x" * 100, "metadata": None}, {"reply_message_count": 7})
add("_over_reply_cap", {"adapter": {"hooks": {"send_document": "own", "reply_message_count": "own"},
                                    "attrs": {"MAX_SPLIT_MESSAGES": 5, "MAX_MESSAGE_LENGTH": fl(10.9)}},
                        "content": "x" * 100, "metadata": None}, {"reply_message_count": 7})
add("_over_reply_cap", {"adapter": CAP5, "content": "x" * 100, "metadata": None},
    {"reply_message_count": {"__script__": True,
                             "raise": {"type": "RuntimeError", "message": "count failed"}}})
add("_over_reply_cap", {"adapter": {"hooks": {"send_document": "own", "reply_message_count": "own"},
                                    "attrs": {"MAX_SPLIT_MESSAGES": 5, "MAX_MESSAGE_LENGTH": " 4096 "}},
                        "content": "x" * 100, "metadata": None}, {"reply_message_count": 7})

# ── reply_file_parts ──
add("reply_file_parts", {"text": "hello", "limit_bytes": 10})
add("reply_file_parts", {"text": "", "limit_bytes": 5})
add("reply_file_parts", {"text": "ab\ncdefgh", "limit_bytes": 6})
add("reply_file_parts", {"text": "xxxxx\nyyyyy", "limit_bytes": 8})
add("reply_file_parts", {"text": "abcdefghij", "limit_bytes": 4})
add("reply_file_parts", {"text": "中文中文", "limit_bytes": 4})
add("reply_file_parts", {"text": "a\nb\nc\n", "limit_bytes": 2})
add("reply_file_parts", {"text": "😀😀", "limit_bytes": 4})
add("reply_file_parts", {"text": "中\n中", "limit_bytes": 4})

# ── scaled_upload_deadline ──
add("scaled_upload_deadline", {"deadline": 30, "upload_seconds": 0})
add("scaled_upload_deadline", {"deadline": 30, "upload_seconds": -5})
add("scaled_upload_deadline", {"deadline": 30, "upload_seconds": 10})
add("scaled_upload_deadline", {"deadline": 30, "upload_seconds": 10000})
add("scaled_upload_deadline", {"deadline": 4000, "upload_seconds": 10})
add("scaled_upload_deadline", {"deadline": 60, "upload_seconds": 0.5})
add("scaled_upload_deadline", {"deadline": 0, "upload_seconds": 100})

# ── media_upload_seconds ──
add("media_upload_seconds", {"sources": []})
add("media_upload_seconds", {"sources": ["/a/b"]}, {"os.stat": {"st_size": 131072}})
add("media_upload_seconds", {"sources": ["/a", "/b"]},
    {"os.stat": [{"st_size": 131072}, {"st_size": 65536}]})
add("media_upload_seconds", {"sources": [{"kind": "pathlike", "path": "/p"}]},
    {"os.stat": {"st_size": 262144}})
add("media_upload_seconds", {"sources": [{"kind": "file", "fileno": 3}]},
    {"os.fstat": {"st_size": 131072}})
add("media_upload_seconds", {"sources": [5, None, True, [1], {"kind": "bare"}]})
add("media_upload_seconds", {"sources": ["/x"]},
    {"os.stat": {"__script__": True,
                 "raise": {"type": "FileNotFoundError", "message": "gone", "errno": 2}}})
add("media_upload_seconds", {"sources": ["/x"]},
    {"os.stat": {"__script__": True, "raise": {"type": "ValueError", "message": "embedded null"}}})
add("media_upload_seconds", {"sources": ["/x"]},
    {"os.stat": {"__script__": True, "raise": {"type": "RuntimeError", "message": "boom"}}})
add("media_upload_seconds", {"sources": ["/x"]}, {"os.stat": None})
add("media_upload_seconds", {"sources": [{"kind": "file", "fileno": 3}]}, {"os.fstat": None})
add("media_upload_seconds", {"sources": [{"kind": "file",
                                          "fileno_raise": {"type": "OSError", "message": "bad fd", "errno": 9}}]})
add("media_upload_seconds", {"sources": [{"kind": "file",
                                          "fileno_raise": {"type": "RuntimeError", "message": "x"}}]})
add("media_upload_seconds", {"sources": ["/a", {"kind": "file", "fileno": 7}, {"kind": "bare"}]},
    {"os.stat": {"st_size": 100}, "os.fstat": {"st_size": 200}})
add("media_upload_seconds", {"sources": [{"kind": "file", "fileno": 3}]},
    {"os.fstat": {"__script__": True, "raise": {"type": "OSError", "message": "bad fd", "errno": 9}}})

# ── seeded-random breadth (verified terminating domains) ──
rng = random.Random(20261118)
pool = "abc def ghi jkl mnop qrst uvwxyz 0123456789"
upool = "ab\ncd ef中文😀éx\n"
apool = "ab\ncd ef x\n"
tpool = ["| a | b |", "|---|---|", "| 1 | 2 |", "plain text", "", "| x |", "---|---",
         "|:-|-:|", "  | p | q |  ", "|", "a|b"]


def rand_text(n, p):
    return "".join(rng.choice(p) for _ in range(n))


for _ in range(4):
    add("display_width", {"text": rand_text(rng.randint(1, 20), "abc中文😀한국어ａ")})
for _ in range(4):
    text = rand_text(rng.randint(1, 30), pool)
    add("_shorten_choice", {"text": text, "each": rng.randint(1, 20),
                            "anchor": rng.randint(0, len(text) + 1)})
for _ in range(4):
    limit = rng.randint(2, 20)
    text = rand_text(rng.randint(0, 40), upool if limit >= 4 else apool)
    add("reply_file_parts", {"text": text, "limit_bytes": limit})
for _ in range(4):
    text = "\n".join(rng.choice(tpool) for _ in range(rng.randint(2, 6)))
    split_at = rng.randint(0, len(text))
    add("table_row_start", {"text": text, "split_at": split_at})
    add("table_rollback_point", {"text": text, "split_at": split_at, "fits": "seam"},
        {"fits": rng.choice([True, False])})
for _ in range(3):
    k = rng.randint(2, 4)
    base = rand_text(rng.randint(3, 12), pool)
    choices = [base + rand_text(rng.randint(1, 10), pool) for _ in range(k)]
    add("numbered_clarify_choices", {"choices": choices, "max_label_cols": 0,
                                     "escape": None, "max_chars": rng.randint(5, 30),
                                     "len_fn": None})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")
    thin = [fn for fn, n in counts.items() if n < 5]
    if thin:
        raise SystemExit(f"FUNCS WITH <5 CASES: {thin}")


if __name__ == "__main__":
    main()
