#!/usr/bin/env python3
"""Generate tests/fixtures/feishu_adapter_flow.jsonl — input cases for the
Feishu adapter normalization FLOW oracle.

For each case the io scripts are DERIVED from the live reference by running it
with depth-filtered delegating recorders (a seamed sibling called at depth 0
records {name, args, ret} and delegates to the real function; nested seamed
calls delegate silently). The recorded per-name result queues are exactly what
the C universal seam and the oracle's scripted stubs replay. List-valued
results are wrapped {"__script__": true, "ret": [...]} (queue semantics).

A derive-vs-drive self-consistency check runs every case through BOTH the
delegating live run and the oracle's scripted drive() and asserts identical
ret / trace / state — catching ARGMAP/projection bugs before the gate.

Deterministic handcrafted branch-coverage cases plus a few seeded-random
text-leaf cases. Propagation cases hand-script BaseException raises.
"""
import ast
import copy
import json
import os
import random
import re
import sys
import textwrap
import types
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import oracle_feishu_adapter_flow as o  # noqa: E402

OUT = Path(__file__).resolve().parent / "fixtures" / "feishu_adapter_flow.jsonl"
Q = os.path.expanduser("~/hermes-agent")
HELPERS_SRC = os.path.join(Q, "gateway", "platforms", "helpers.py")

CASES = []
CHECK = []


# ── real strip_markdown for delegation (foreign seam) ──
def extract_strip():
    mod = o.new_module("fa_gen_helpers")
    ns = mod.__dict__
    ns["re"] = re
    src = open(HELPERS_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    want_const = {"_STRIP_RULES", "_MD_LINK_RE", "_NEWLINE_SQUEEZE_RE"}
    want_fn = {"strip_markdown", "_keep_link_target"}
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if any(isinstance(t, ast.Name) and t.id in want_const for t in tgts):
                try:
                    o.exec_node(src, node, ns, HELPERS_SRC)
                except Exception:
                    pass
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in want_fn:
            o.exec_node(src, node, ns, HELPERS_SRC)
    return ns["strip_markdown"]


REAL_STRIP = extract_strip()


def wrap(r):
    return {"__script__": True, "ret": r} if isinstance(r, list) else r


def derive_io(func, args):
    events = []
    depth = [0]
    for nm in o.SEAMED:
        if nm in o.REAL_DEFS:
            o.NS[nm] = o.REAL_DEFS[nm]

    def deleg(nm):
        real = REAL_STRIP if nm == "strip_markdown" else o.REAL_DEFS[nm]
        argfn = o.ARGMAP[nm]

        def f(*a, **k):
            ap = o.canon(argfn(*a, **k))
            d = depth[0]
            depth[0] += 1
            try:
                result = real(*a, **k)
            finally:
                depth[0] -= 1
            if d == 0:
                events.append({"name": nm, "args": ap, "ret": o.canon(result)})
            return result
        return f

    for nm in o.SEAMED:
        if nm == func:
            continue
        if nm == "strip_markdown":
            sys.modules["gateway.platforms.helpers"].strip_markdown = deleg(nm)
        elif nm in o.NS:
            o.NS[nm] = deleg(nm)
    state_out = {}
    ret = o.live_call(func, copy.deepcopy(args), state_out)
    io = {}
    for ev in events:
        io.setdefault(ev["name"], []).append(wrap(ev["ret"]))
    return io, o.canon(ret), events, o.canon(state_out)


def add(func, args, io=None):
    args = copy.deepcopy(args)
    if io is None:
        io, dret, dtrace, dstate = derive_io(func, args)
        # self-consistency: scripted drive must reproduce the delegating run
        got = o.drive(func, {"args": copy.deepcopy(args), "io": io})
        want = {"ret": dret, "trace": dtrace, "state": dstate}
        if got != want:
            CHECK.append((func, args, want, got))
    CASES.append({"func": func, "in": {"args": args, "io": io}})


def rais(kind, msg):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


# ── leaves ──
for st in ["bot", "app", "user", "", "Bot"]:
    add("_is_bot_sender", {"sender": {"sender_type": st}})
add("_is_bot_sender", {"sender": {}})

add("_sender_identity", {"sender": {"sender_id": None}})
add("_sender_identity", {"sender": {"sender_id": {"open_id": "ou_1"}}})
add("_sender_identity", {"sender": {"sender_id": {"open_id": "ou_1", "user_id": "u_1", "union_id": "on_1"}}})
add("_sender_identity", {"sender": {"sender_id": {"open_id": "x", "user_id": "x", "union_id": ""}}})
add("_sender_identity", {"sender": {}})

for t in ["", "plain text", "a*b_c[d]", "back\\slash `code`", "# head > quote | pipe", "é中*"]:
    add("_escape_markdown_text", {"text": t})

for v in [True, 1, "true", "True", 0, False, "1", 1.0, None, "false", 2]:
    add("_to_boolean", {"value": v})

add("_is_style_enabled", {"style": None, "key": "bold"})
add("_is_style_enabled", {"style": {}, "key": "bold"})
add("_is_style_enabled", {"style": {"code": True}, "key": "code"})
add("_is_style_enabled", {"style": {"bold": 1}, "key": "bold"})
add("_is_style_enabled", {"style": {"italic": "true"}, "key": "italic"})
add("_is_style_enabled", {"style": {"bold": False}, "key": "bold"})

for t in ["", "code", "`x`", "a``b```c", "```", "x`", "`"]:
    add("_wrap_inline_code", {"text": t})

for lang in ["py", "  py  ", "p\ny", "p\ry", "", "\tpython\n"]:
    add("_sanitize_fence_language", {"language": lang})

for v, d, mv in [("5", None, 0), (" 7 ", None, 0), ("+5", None, 0), ("-5", None, 0),
                 ("1_0", None, 0), ("bad", 9, 0), (None, 9, 0), (True, None, 0),
                 (5.7, None, 0), (-5.7, None, 0), ([], 3, 0), ("2", 9, 5),
                 ("1__0", 4, 0), ("_1", 4, 0), ("0x10", 4, 0), (False, None, 0),
                 ("1\x1c", 9, 0), ("1\xa0", None, 0), ("\x1c5\x1d", 9, 0), (" 5\u3000", None, 0)]:
    add("_coerce_int", {"value": v, "default": d, "min_value": mv})

add("_to_post_payload", {"candidate": "notdict"})
add("_to_post_payload", {"candidate": {"title": "t"}})
add("_to_post_payload", {"candidate": {"content": "notlist"}})
add("_to_post_payload", {"candidate": {"title": "t", "content": [[{"tag": "text", "text": "x"}]]}})
add("_to_post_payload", {"candidate": {"content": [], "files": [{"file_key": "k"}]}})
add("_to_post_payload", {"candidate": {"content": [], "files": "notlist", "title": 5}})

add("_load_feishu_payload", {"raw_content": ""})
add("_load_feishu_payload", {"raw_content": '{"text": "hi"}'})
add("_load_feishu_payload", {"raw_content": '[1, 2, 3]'})
add("_load_feishu_payload", {"raw_content": '"just a string"'})
add("_load_feishu_payload", {"raw_content": 'not json at all'})
add("_load_feishu_payload", {"raw_content": '42'})
add("_load_feishu_payload", {"raw_content": '{"a": 1, "b": [true, null]}'})

add("_normalize_feishu_text", {"text": "  hello   world  \n\n foo ", "mentions_map": None})
add("_normalize_feishu_text", {"text": "a\r\nb\rc", "mentions_map": None})
add("_normalize_feishu_text", {"text": "@_user_1 hi @_user_2", "mentions_map": {"@_user_1": {"name": "Al", "open_id": "ou_1", "is_all": False, "is_self": False}}})
add("_normalize_feishu_text", {"text": "@_all meet", "mentions_map": None})
add("_normalize_feishu_text", {"text": "\t tabbed \u00a0 space \u3000", "mentions_map": None})
add("_normalize_feishu_text", {"text": "", "mentions_map": None})
add("_normalize_feishu_text", {"text": "line1\n\n\nline2", "mentions_map": None})

add("_unique_lines", {"lines": []})
add("_unique_lines", {"lines": ["a", "b", "a", "c", "b"]})
add("_unique_lines", {"lines": ["", "a", "", "a"]})
add("_unique_lines", {"lines": ["x"]})
add("_unique_lines", {"lines": ["a", "a", "a"]})

add("_extract_mention_ids", {"mention": {"id": "ou_1", "id_type": "open_id"}})
add("_extract_mention_ids", {"mention": {"id": "u_1", "id_type": "user_id"}})
add("_extract_mention_ids", {"mention": {"id": "x", "id_type": "union_id"}})
add("_extract_mention_ids", {"mention": {"id": None}})
add("_extract_mention_ids", {"mention": {"id": {"open_id": "ou_9", "user_id": "u_9"}}})
add("_extract_mention_ids", {"mention": {}})

add("_build_mention_hint", {"mentions": []})
add("_build_mention_hint", {"mentions": [{"name": "Bot", "open_id": "ou_b", "is_all": False, "is_self": True}]})
add("_build_mention_hint", {"mentions": [{"name": "", "open_id": "", "is_all": True, "is_self": False}]})
add("_build_mention_hint", {"mentions": [{"name": "Al", "open_id": "ou_1", "is_all": False, "is_self": False}]})
add("_build_mention_hint", {"mentions": [{"name": "Bo", "open_id": "", "is_all": False, "is_self": False}]})
add("_build_mention_hint", {"mentions": [{"name": "Al", "open_id": "ou_1", "is_all": False, "is_self": False},
                                          {"name": "Al", "open_id": "ou_1", "is_all": False, "is_self": False}]})

add("_strip_edge_self_mentions", {"text": "", "mentions": []})
add("_strip_edge_self_mentions", {"text": "@Bot hello", "mentions": [{"name": "Bot", "open_id": "", "is_all": False, "is_self": True}]})
add("_strip_edge_self_mentions", {"text": "hello @Bot", "mentions": [{"name": "Bot", "open_id": "", "is_all": False, "is_self": True}]})
add("_strip_edge_self_mentions", {"text": "hello @Bot again", "mentions": [{"name": "Bot", "open_id": "", "is_all": False, "is_self": True}]})
add("_strip_edge_self_mentions", {"text": "@Bot @Bot hi", "mentions": [{"name": "Bot", "open_id": "", "is_all": False, "is_self": True}]})
add("_strip_edge_self_mentions", {"text": "@Bot你好", "mentions": [{"name": "Bot", "open_id": "", "is_all": False, "is_self": True}]})
add("_strip_edge_self_mentions", {"text": "hi @机器人。", "mentions": [{"name": "机器人", "open_id": "", "is_all": False, "is_self": True}]})
add("_strip_edge_self_mentions", {"text": "no self here", "mentions": [{"name": "Al", "open_id": "", "is_all": False, "is_self": False}]})

add("_build_markdown_post_rows", {"content": ""})
add("_build_markdown_post_rows", {"content": "just prose"})
add("_build_markdown_post_rows", {"content": "```py\ncode\n```"})
add("_build_markdown_post_rows", {"content": "before\n```py\ncode\n```\nafter"})
add("_build_markdown_post_rows", {"content": "```\na\n```\n```\nb\n```"})
add("_build_markdown_post_rows", {"content": "unclosed\n```py\ncode"})
add("_build_markdown_post_rows", {"content": "  \n``` \nx\n```  "})

add("matches", {"bot": {"open_id": "ou_b", "user_id": "u_b", "name": "Bot"}, "open_id": "ou_b", "user_id": "", "name": ""})
add("matches", {"bot": {"open_id": "ou_b", "user_id": "u_b", "name": "Bot"}, "open_id": "ou_x", "user_id": "", "name": ""})
add("matches", {"bot": {"open_id": "", "user_id": "u_b", "name": "Bot"}, "open_id": "ou_x", "user_id": "u_b", "name": ""})
add("matches", {"bot": {"open_id": "", "user_id": "", "name": "Bot"}, "open_id": "", "user_id": "", "name": "Bot"})
add("matches", {"bot": {"open_id": "", "user_id": "", "name": "Bot"}, "open_id": "", "user_id": "", "name": "Other"})
add("matches", {"bot": {"open_id": "", "user_id": "", "name": ""}, "open_id": "", "user_id": "", "name": "Bot"})

add("_walk_nodes", {"value": {"a": 1, "b": {"c": 2}}})
add("_walk_nodes", {"value": [1, {"x": 1}, [2, 3]]})
add("_walk_nodes", {"value": "scalar"})
add("_walk_nodes", {"value": {}})
add("_walk_nodes", {"value": {"header": {"title": {"content": "T"}}, "elements": [{"tag": "div"}]}})

# ── renderers ──
add("_render_text_element", {"element": {"text": "plain"}})
add("_render_text_element", {"element": {"text": "code", "style": {"code": True}}})
add("_render_text_element", {"element": {"text": "bold", "style": {"bold": True}}})
add("_render_text_element", {"element": {"text": "both", "style": {"bold": True, "italic": True}}})
add("_render_text_element", {"element": {"text": ""}})
add("_render_text_element", {"element": {"text": "x", "style": "notdict"}})
add("_render_text_element", {"element": {"text": "all", "style": {"bold": True, "italic": True, "underline": True, "strikethrough": True}}})

add("_render_code_block_element", {"element": {"language": "py", "text": "x=1"}})
add("_render_code_block_element", {"element": {"lang": "js", "text": "y=2"}})
add("_render_code_block_element", {"element": {"text": "a\r\nb"}})
add("_render_code_block_element", {"element": {"content": "c\n"}})
add("_render_code_block_element", {"element": {}})
add("_render_code_block_element", {"element": {"language": "  py\n ", "text": "z"}})

add("_render_post_element", {"element": "raw string", "image_keys": [], "media_refs": [], "mentions_map": None})
add("_render_post_element", {"element": 42, "image_keys": [], "media_refs": [], "mentions_map": None})
add("_render_post_element", {"element": {"tag": "br"}, "image_keys": [], "media_refs": [], "mentions_map": None})
add("_render_post_element", {"element": {"tag": "hr"}, "image_keys": [], "media_refs": [], "mentions_map": None})
add("_render_post_element", {"element": {"tag": "text", "text": "hi", "style": {"bold": True}}, "image_keys": [], "media_refs": [], "mentions_map": None})
add("_render_post_element", {"element": {"tag": "a", "href": "http://x", "text": "label"}, "image_keys": [], "media_refs": [], "mentions_map": None})
add("_render_post_element", {"element": {"tag": "a", "text": "nolink"}, "image_keys": [], "media_refs": [], "mentions_map": None})
add("_render_post_element", {"element": {"tag": "at", "user_id": "@_all"}, "image_keys": [], "media_refs": [], "mentions_map": {}})
add("_render_post_element", {"element": {"tag": "at", "user_id": "@_user_1"}, "image_keys": [], "media_refs": [], "mentions_map": {"@_user_1": {"name": "Al", "open_id": "ou_1", "is_all": False, "is_self": False}}})
add("_render_post_element", {"element": {"tag": "at", "user_id": "@_user_2", "user_name": "Fallback"}, "image_keys": [], "media_refs": [], "mentions_map": None})
add("_render_post_element", {"element": {"tag": "img", "image_key": "img_1", "text": "alt"}, "image_keys": [], "media_refs": [], "mentions_map": None})
add("_render_post_element", {"element": {"tag": "img", "image_key": "img_1"}, "image_keys": ["img_1"], "media_refs": [], "mentions_map": None})
add("_render_post_element", {"element": {"tag": "file", "file_key": "f1", "file_name": "doc.pdf"}, "image_keys": [], "media_refs": [], "mentions_map": None})
add("_render_post_element", {"element": {"tag": "audio", "file_key": "a1"}, "image_keys": [], "media_refs": [{"file_key": "a1", "file_name": "", "resource_type": "audio"}], "mentions_map": None})
add("_render_post_element", {"element": {"tag": "media", "file_key": ""}, "image_keys": [], "media_refs": [], "mentions_map": None})
add("_render_post_element", {"element": {"tag": "emoji", "text": "smile"}, "image_keys": [], "media_refs": [], "mentions_map": None})
add("_render_post_element", {"element": {"tag": "emoji"}, "image_keys": [], "media_refs": [], "mentions_map": None})
add("_render_post_element", {"element": {"tag": "code", "text": "x`y"}, "image_keys": [], "media_refs": [], "mentions_map": None})
add("_render_post_element", {"element": {"tag": "div", "text": "nested", "title": "T"}, "image_keys": [], "media_refs": [], "mentions_map": None})
add("_render_post_element", {"element": {"tag": "text", "text": "x"}, "image_keys": [], "media_refs": [], "mentions_map": None},
    io={"_render_text_element": [rais("SystemExit", "boom")]})

add("_join_nested_posts", {"values": ["a", "b"], "image_keys": [], "media_refs": [], "mentions_map": None})
add("_join_nested_posts", {"values": ["a", "", "c"], "image_keys": [], "media_refs": [], "mentions_map": None})
add("_join_nested_posts", {"values": [{"tag": "text", "text": "x"}], "image_keys": [], "media_refs": [], "mentions_map": None})
add("_join_nested_posts", {"values": [["a", "b"], "c"], "image_keys": [], "media_refs": [], "mentions_map": None})
add("_join_nested_posts", {"values": [], "image_keys": [], "media_refs": [], "mentions_map": None})

add("_render_nested_post", {"value": "a*b", "image_keys": [], "media_refs": [], "mentions_map": None})
add("_render_nested_post", {"value": ["a", "b"], "image_keys": [], "media_refs": [], "mentions_map": None})
add("_render_nested_post", {"value": {"tag": "text", "text": "x"}, "image_keys": [], "media_refs": [], "mentions_map": None})
add("_render_nested_post", {"value": {"foo": "bar", "text": "x"}, "image_keys": [], "media_refs": [], "mentions_map": None})
add("_render_nested_post", {"value": 42, "image_keys": [], "media_refs": [], "mentions_map": None})

# ── parse / resolve ──
add("parse_feishu_post_payload", {"payload": {}, "mentions_map": None})
add("parse_feishu_post_payload", {"payload": {"title": "T", "content": [[{"tag": "text", "text": "hello"}]]}, "mentions_map": None})
add("parse_feishu_post_payload", {"payload": {"content": [[{"tag": "img", "image_key": "i1"}, {"tag": "text", "text": "x"}]]}, "mentions_map": {}})
add("parse_feishu_post_payload", {"payload": {"content": [], "files": [{"file_key": "f1", "file_name": "a.pdf"}, {"file_key": "f2", "is_folder": True}, {"file_key": "  "}]}, "mentions_map": None})
add("parse_feishu_post_payload", {"payload": {"post": {"zh_cn": {"title": "标题", "content": [[{"tag": "text", "text": "中文"}]]}}}, "mentions_map": None})
add("parse_feishu_post_payload", {"payload": {"content": [[{"tag": "at", "user_id": "@_all"}]]}, "mentions_map": {}})
add("parse_feishu_post_payload", {"payload": {"content": "notlist"}, "mentions_map": None})
add("parse_feishu_post_payload", {"payload": {"title": "T", "content": [[{"tag": "text", "text": "x"}]]}, "mentions_map": None},
    io={"_resolve_post_payload": [rais("SystemExit", "stop")]})

add("_resolve_post_payload", {"payload": {"content": [[{"tag": "text", "text": "x"}]]}})
add("_resolve_post_payload", {"payload": {"post": {"content": [[{"tag": "text", "text": "y"}]]}}})
add("_resolve_post_payload", {"payload": {"zh_cn": {"content": [[{"tag": "text", "text": "z"}]]}}})
add("_resolve_post_payload", {"payload": "notdict"})
add("_resolve_post_payload", {"payload": {"post": {"en_us": {"content": []}}}})

add("_resolve_locale_payload", {"payload": {"content": [[{"tag": "text", "text": "x"}]]}})
add("_resolve_locale_payload", {"payload": {"zh_cn": {"content": [[{"tag": "text", "text": "zh"}]]}}})
add("_resolve_locale_payload", {"payload": {"en_us": {"content": [[{"tag": "text", "text": "en"}]]}}})
add("_resolve_locale_payload", {"payload": {"fr_fr": {"content": [[{"tag": "text", "text": "fr"}]]}}})
add("_resolve_locale_payload", {"payload": "notdict"})
add("_resolve_locale_payload", {"payload": {"zh_cn": {"content": "notlist"}, "en_us": {"content": [[{"tag": "text", "text": "e"}]]}}})

for c in ["", "hi", "a\nb", "```py\ncode\n```", "prose\n```\nx\n```\ntail"]:
    add("_build_markdown_post_payload", {"content": c})

add("_coerce_required_int", {"value": "5", "default": 9, "min_value": 0})
add("_coerce_required_int", {"value": "bad", "default": 9, "min_value": 0})
add("_coerce_required_int", {"value": "2", "default": 9, "min_value": 5})
add("_coerce_required_int", {"value": None, "default": 9, "min_value": 0})
add("_coerce_required_int", {"value": "-3", "default": 9, "min_value": 0})
add("_coerce_required_int", {"value": "5", "default": 9, "min_value": 0},
    io={"_coerce_int": [rais("KeyboardInterrupt", "int")]})

for t in ["[label](http://x)", "> quoted", "  ---  ", "~~strike~~", "<u>under</u>", "plain", "a\r\nb\n> q\n~~s~~",
          "  ---  \n", "before\n  ---  \nafter", "\n  ---  \n", "x\n---\n\n---\ny"]:
    add("_strip_markdown_to_plain_text", {"text": t})
add("_strip_markdown_to_plain_text", {"text": "hello"},
    io={"strip_markdown": [rais("SystemExit", "exit")]})

add("_attachment_placeholder", {"file_name": "doc.pdf"})
add("_attachment_placeholder", {"file_name": ""})
add("_attachment_placeholder", {"file_name": "  spaced  "})
add("_attachment_placeholder", {"file_name": "  "})
add("_attachment_placeholder", {"file_name": "中文.pdf"})

add("_first_non_empty_text", {"values": ["first", "second"]})
add("_first_non_empty_text", {"values": [None, {}, [], "real"]})
add("_first_non_empty_text", {"values": [5, "x"]})
add("_first_non_empty_text", {"values": ["", "  ", "ok"]})
add("_first_non_empty_text", {"values": [None, {}]})
add("_first_non_empty_text", {"values": [True, "x"]})

add("_find_header_title", {"payload": "notdict"})
add("_find_header_title", {"payload": {"header": "notdict"}})
add("_find_header_title", {"payload": {"header": {"title": "Simple"}}})
add("_find_header_title", {"payload": {"header": {"title": {"content": "C", "text": "T"}}}})
add("_find_header_title", {"payload": {"header": {"title": None}}})
add("_find_header_title", {"payload": {"header": {"title": {"name": "N"}}}})

add("_find_first_text", {"payload": {"title": "T"}, "keys": ["title", "summary"]})
add("_find_first_text", {"payload": {"elements": [{"text": "deep"}]}, "keys": ["text"]})
add("_find_first_text", {"payload": {"title": "  "}, "keys": ["title"]})
add("_find_first_text", {"payload": {"a": {"b": {"summary": "S"}}}, "keys": ["summary", "title"]})
add("_find_first_text", {"payload": [1, 2], "keys": ["title"]})

add("_first_text_field", {"payload": {"name": "N"}, "keys": ["name", "title"], "deep": []})
add("_first_text_field", {"payload": {"x": {"title": "Deep"}}, "keys": ["name"], "deep": ["title"]})
add("_first_text_field", {"payload": {}, "keys": ["name"], "deep": []})
add("_first_text_field", {"payload": {"name": "  "}, "keys": ["name"], "deep": ["title"]})
add("_first_text_field", {"payload": {"title": "T", "name": "N"}, "keys": ["name", "title"], "deep": []})

add("_build_media_ref_from_payload", {"payload": {"file_key": "k1", "file_name": "a.mp3"}, "resource_type": "audio"})
add("_build_media_ref_from_payload", {"payload": {"file_key": "k2", "title": "v.mp4"}, "resource_type": "video"})
add("_build_media_ref_from_payload", {"payload": {"file_key": "k3", "text": "d.pdf"}, "resource_type": "file"})
add("_build_media_ref_from_payload", {"payload": {"file_key": "  "}, "resource_type": "media"})
add("_build_media_ref_from_payload", {"payload": {}, "resource_type": "audio"})

# ── collect walkers ──
add("_collect_text_segments", {"value": "plain", "in_rich_block": False})
add("_collect_text_segments", {"value": "rich", "in_rich_block": True})
add("_collect_text_segments", {"value": ["a", "b"], "in_rich_block": True})
add("_collect_text_segments", {"value": {"tag": "div", "text": "hello"}, "in_rich_block": False})
add("_collect_text_segments", {"value": {"tag": "div", "text": "x", "url": "http://skip"}, "in_rich_block": False})
add("_collect_text_segments", {"value": {"tag": "button", "text": "Go", "value": "v"}, "in_rich_block": False})
add("_collect_text_segments", {"value": {"elements": [{"tag": "note", "text": "n"}]}, "in_rich_block": False})

add("_collect_card_lines", {"payload": {"tag": "div", "text": "Hello"}})
add("_collect_card_lines", {"payload": {}})
add("_collect_card_lines", {"payload": {"elements": [{"tag": "div", "text": "A"}, {"tag": "div", "text": "A"}]}})
add("_collect_card_lines", {"payload": {"tag": "markdown", "content": "Line"}})
add("_collect_card_lines", {"payload": {"title": "T", "text": "body"}})

add("_collect_action_labels", {"payload": {"elements": [{"tag": "button", "text": "Go"}]}})
add("_collect_action_labels", {"payload": {"elements": [{"tag": "select_static", "text": "Pick"}]}})
add("_collect_action_labels", {"payload": {"elements": [{"tag": "div", "text": "not action"}]}})
add("_collect_action_labels", {"payload": {"elements": [{"tag": "button", "text": "Go"}, {"tag": "button", "text": "Go"}]}})
add("_collect_action_labels", {"payload": {}})

add("_collect_forward_entries", {"payload": {"messages": [{"sender_name": "Al", "text": "hi"}]}})
add("_collect_forward_entries", {"payload": {"messages": ["plain entry", "plain entry"]}})
add("_collect_forward_entries", {"payload": {"messages": [{"sender_name": "Al", "message_type": "post", "content": {"content": [[{"tag": "text", "text": "p"}]]}}]}})
add("_collect_forward_entries", {"payload": {"items": [{"text": "a"}, {"summary": "b"}]}})
add("_collect_forward_entries", {"payload": {}})
add("_collect_forward_entries", {"payload": {"messages": [{"sender_name": "Al", "text": "x"}, {"sender_name": "Al", "text": "x"}]}})

# ── normalizer sub-flows ──
add("_normalize_merge_forward_message", {"payload": {"title": "Forward", "messages": [{"sender_name": "Al", "text": "hi"}]}})
add("_normalize_merge_forward_message", {"payload": {}})
add("_normalize_merge_forward_message", {"payload": {"messages": [{"text": f"m{i}"} for i in range(10)]}})
add("_normalize_merge_forward_message", {"payload": {"summary": "Sum", "messages": ["a"]}})
add("_normalize_merge_forward_message", {"payload": {"messages": []}})

add("_normalize_share_chat_message", {"payload": {"chat_name": "Room", "chat_id": "oc_1"}})
add("_normalize_share_chat_message", {"payload": {"chat_name": "Room"}})
add("_normalize_share_chat_message", {"payload": {"chat_id": "oc_1"}})
add("_normalize_share_chat_message", {"payload": {}})
add("_normalize_share_chat_message", {"payload": {"name": "N", "open_chat_id": "oc_2"}})

add("_normalize_interactive_message", {"message_type": "interactive", "payload": {"header": {"title": {"content": "Card"}}}})
add("_normalize_interactive_message", {"message_type": "card", "payload": {"elements": [{"tag": "button", "text": "Go"}]}})
add("_normalize_interactive_message", {"message_type": "interactive", "payload": {"card": {"header": {"title": "Inner"}, "elements": [{"tag": "div", "text": "body"}]}}})
add("_normalize_interactive_message", {"message_type": "interactive", "payload": {}})
add("_normalize_interactive_message", {"message_type": "interactive", "payload": {"title": "T", "header": {"title": "H"}, "elements": [{"tag": "div", "text": "H"}]}})
add("_normalize_interactive_message", {"message_type": "interactive", "payload": {"elements": [{"tag": "div", "text": f"line{i}"} for i in range(15)]}})

# ── mentions map ──
add("_build_mentions_map", {"mentions": [], "bot": {"open_id": "ou_b", "user_id": "", "name": "Bot"}})
add("_build_mentions_map", {"mentions": [{"key": "@_all"}], "bot": {"open_id": "", "user_id": "", "name": ""}})
add("_build_mentions_map", {"mentions": [{"key": "@_user_1", "name": "Al", "id": "ou_1", "id_type": "open_id"}], "bot": {"open_id": "ou_b", "user_id": "", "name": "Bot"}})
add("_build_mentions_map", {"mentions": [{"key": "@_user_1", "name": "Bot", "id": "ou_b", "id_type": "open_id"}], "bot": {"open_id": "ou_b", "user_id": "", "name": "Bot"}})
add("_build_mentions_map", {"mentions": [{"key": ""}, {"key": "@_user_1", "name": "Al", "id": {"open_id": "ou_1"}}], "bot": {"open_id": "", "user_id": "", "name": ""}})
add("_build_mentions_map", {"mentions": None, "bot": {"open_id": "", "user_id": "", "name": ""}})

# ── top-level normalize ──
add("normalize_feishu_message", {"message_type": "text", "raw_content": '{"text": "hello"}', "mentions": None, "bot": None})
add("normalize_feishu_message", {"message_type": "text", "raw_content": '{"text": "@_all hi"}', "mentions": [], "bot": None})
add("normalize_feishu_message", {"message_type": "text", "raw_content": '{"text": "@_user_1 hey"}', "mentions": [{"key": "@_user_1", "name": "Al", "id": "ou_1", "id_type": "open_id"}], "bot": {"open_id": "ou_b", "user_id": "", "name": "Bot"}})
add("normalize_feishu_message", {"message_type": "post", "raw_content": '{"title": "T", "content": [[{"tag": "text", "text": "body"}]]}', "mentions": None, "bot": None})
add("normalize_feishu_message", {"message_type": "post", "raw_content": '{"content": [[{"tag": "img", "image_key": "i1"}, {"tag": "at", "user_id": "@_all"}]]}', "mentions": [], "bot": None})
add("normalize_feishu_message", {"message_type": "image", "raw_content": '{"image_key": "img_1", "text": "alt"}', "mentions": None, "bot": None})
add("normalize_feishu_message", {"message_type": "image", "raw_content": '{"image_key": "img_1"}', "mentions": None, "bot": None})
add("normalize_feishu_message", {"message_type": "file", "raw_content": '{"file_key": "f1", "file_name": "doc.pdf"}', "mentions": None, "bot": None})
add("normalize_feishu_message", {"message_type": "audio", "raw_content": '{"file_key": "a1", "file_name": "v.mp3"}', "mentions": None, "bot": None})
add("normalize_feishu_message", {"message_type": "media", "raw_content": '{"file_key": "m1"}', "mentions": None, "bot": None})
add("normalize_feishu_message", {"message_type": "merge_forward", "raw_content": '{"title": "F", "messages": [{"sender_name": "Al", "text": "hi"}]}', "mentions": None, "bot": None})
add("normalize_feishu_message", {"message_type": "share_chat", "raw_content": '{"chat_name": "Room", "chat_id": "oc_1"}', "mentions": None, "bot": None})
add("normalize_feishu_message", {"message_type": "interactive", "raw_content": '{"header": {"title": {"content": "Card"}}}', "mentions": None, "bot": None})
add("normalize_feishu_message", {"message_type": "card", "raw_content": '{"elements": [{"tag": "button", "text": "Go"}]}', "mentions": None, "bot": None})
add("normalize_feishu_message", {"message_type": "sticker", "raw_content": '{}', "mentions": None, "bot": None})
add("normalize_feishu_message", {"message_type": " TEXT ", "raw_content": 'not json', "mentions": None, "bot": None})
add("normalize_feishu_message", {"message_type": "text", "raw_content": '{"text": "x"}', "mentions": None, "bot": None},
    io={"_load_feishu_payload": [rais("SystemExit", "bye")]})
add("normalize_feishu_message", {"message_type": "text", "raw_content": '{"text": "x"}', "mentions": None, "bot": None},
    io={"_load_feishu_payload": [{"text": "x"}], "_build_mentions_map": [rais("GeneratorExit", "gen")]})

# ── seeded-random breadth for text leaves ──
rng = random.Random(20261118)
WS = [" ", "\t", "\n", "\r", "\u00a0", "\u3000", ""]
WORDS = ["hello", "world", "@_user_1", "@_all", "foo", "中文", "a*b", ""]
for _ in range(8):
    txt = "".join(rng.choice(WORDS) + rng.choice(WS) for _ in range(rng.randint(1, 5)))
    add("_normalize_feishu_text", {"text": txt, "mentions_map": None})
for _ in range(6):
    txt = "".join(rng.choice(["a", "*", "_", "`", "[", "]", "(", ")", "#", " ", "é"]) for _ in range(rng.randint(0, 12)))
    add("_escape_markdown_text", {"text": txt})


def main():
    if CHECK:
        print(f"GENERATOR SELF-CONSISTENCY FAILURES: {len(CHECK)}")
        for func, args, want, got in CHECK[:8]:
            print(f"  {func} args={json.dumps(args, ensure_ascii=False)[:200]}")
            print(f"    want={json.dumps(want, ensure_ascii=False)[:300]}")
            print(f"    got ={json.dumps(got, ensure_ascii=False)[:300]}")
        sys.exit(1)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"WROTE {OUT}: {len(CASES)} cases, {len(counts)} funcs")
    low = [f for f, n in counts.items() if n < 5]
    print("funcs with <5 cases:", low)
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")


if __name__ == "__main__":
    main()