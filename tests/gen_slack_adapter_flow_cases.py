#!/usr/bin/env python3
"""Generate tests/fixtures/slack_adapter_flow.jsonl for the Slack
plugin-adapter FLOW port (src/gateway/platforms/slack_adapter_flow.c).

Deterministic (seeded) corpus: >=5 cases per ported function, covering the
pure-leaf edges and the seamed-flow branches (scripted results, ordered
traces, fail-closed missing scripts, BaseException passthrough). Case shape:
{"func", "in": {"args", "io", "state"}, "expect_raise"?}.

Domain guards (ASCII scope, shared with the port header):
- str()-computed positions carry str/int/bool/None only (no containers, no
  non-integral floats).
- float() raw strings parse finite or nan (no inf results).
- malformed saved-token JSON is "" or whitespace-only.
- int() string magnitudes stay < 2^53.
"""
import json
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "slack_adapter_flow.jsonl")

CASES = []


def case(func, args, io=None, state=None, expect_raise=None):
    inp = {"args": args}
    if io is not None:
        inp["io"] = io
    if state is not None:
        inp["state"] = state
    rec = {"func": func, "in": inp}
    if expect_raise:
        rec["expect_raise"] = expect_raise
    CASES.append(rec)


def script(ret=None, raise_spec=None):
    d = {"__script__": True}
    if raise_spec is not None:
        d["raise"] = raise_spec
    else:
        d["ret"] = ret
    return d


def raise_spec(kind, msg):
    return {"type": kind, "message": msg}


# ── _slack_unfurl_kwargs ──
case("_slack_unfurl_kwargs", {"extra": None})
case("_slack_unfurl_kwargs", {"extra": {}})
case("_slack_unfurl_kwargs", {"extra": {"unfurl_links": True, "unfurl_media": False}})
case("_slack_unfurl_kwargs", {"extra": {"unfurl_links": "TRUE ", "unfurl_media": "no"}})
case("_slack_unfurl_kwargs", {"extra": {"unfurl_links": "yes", "unfurl_media": "0"}})
case("_slack_unfurl_kwargs", {"extra": {"unfurl_links": "junk", "unfurl_media": 1}})
case("_slack_unfurl_kwargs", {"extra": {"unfurl_links": " On\n", "unfurl_media": "OFF"}})
case("_slack_unfurl_kwargs", {"extra": {"unfurl_links": None, "unfurl_media": "false"}})

# ── _is_table_row ──
for line in ["| a | b |", "  ", "a|b", "abc", "", " | ", "\t|x|\t"]:
    case("_is_table_row", {"line": line})

# ── _disp_width ──
for s in ["", "abc", "你好", "a你b", "ＡＢ", "😀", "é", "x😀y", "ｦﾅ", "🇺🇸"]:
    case("_disp_width", {"s": s})

# ── _pad ──
for cell, width in [("a", 5), ("你", 4), ("abc", 2), ("", 3), ("Ａ", 3), ("ab", 2), ("你好", 4)]:
    case("_pad", {"cell": cell, "width": width})

# ── _split_table_row ──
for line in ["| a | b | c |", "a|b", "||", "|", "| a |", " a | b ", "|  | x |"]:
    case("_split_table_row", {"line": line})

# ── _align_table ──
case("_align_table", {"rows": ["| a | b |", "|---|---|", "| c | d |"]})
case("_align_table", {"rows": ["| a | b |"]})
case("_align_table", {"rows": []})
case("_align_table", {"rows": ["| a | b | c |", "|---|---|", "| d |"]})
case("_align_table", {"rows": ["| 你 | x |", "|---|---|", "| a | bb |"]})
case("_align_table", {"rows": ["a|bbbb", "---|---", "cc|d", "| e | f | g |"]})
case("_align_table", {"rows": ["| :--- | ---: |", "|:---:|---|", "| x | y |"]})

# ── _wrap_markdown_tables ──
case("_wrap_markdown_tables", {"text": ""})
case("_wrap_markdown_tables", {"text": "no pipes here"})
case("_wrap_markdown_tables", {"text": "a|b without dash"})
case("_wrap_markdown_tables", {"text": "h1|h2\n---|---\na|b"})
case("_wrap_markdown_tables", {"text": "```\nh1|h2\n---|---\na|b\n```"})
case("_wrap_markdown_tables", {"text": "before\nh1|h2\n---|---\na|b\nafter"})
case("_wrap_markdown_tables", {"text": "| h1 | h2 |\n| --- | --- |\n| a | b |\n| cc | d |"})
case("_wrap_markdown_tables", {"text": "x\n---|---\ny"})
case("_wrap_markdown_tables", {"text": "t1|t2\n---|---\na|b\n\nu1|u2\n---|---\nc|d"})
case("_wrap_markdown_tables", {"text": "```\nfence\n```\nh|k\n---|---\n1|2"})
case("_wrap_markdown_tables", {"text": "a|b\n-|-"})
case("_wrap_markdown_tables", {"text": "a|b\n---"})

# ── _collect_slack_block_mentions ──
case("_collect_slack_block_mentions", {"blocks": None})
case("_collect_slack_block_mentions", {"blocks": []})
case("_collect_slack_block_mentions", {"blocks": [{"type": "user", "user_id": "U1"}]})
case("_collect_slack_block_mentions",
     {"blocks": [{"type": "rich_text_quote", "elements": [{"type": "user", "user_id": "U2"}]}]})
case("_collect_slack_block_mentions",
     {"blocks": [{"type": "rich_text_section",
                  "elements": [{"type": "user", "user_id": "U1"}, {"type": "text", "text": "hi"}]}]})
case("_collect_slack_block_mentions",
     {"blocks": [{"type": "user", "user_id": ""}, {"type": "user", "user_id": 0}]})
case("_collect_slack_block_mentions",
     {"blocks": [{"type": "user", "user_id": 123}]})
case("_collect_slack_block_mentions",
     {"blocks": [{"type": "rich_text", "element": {"type": "user", "user_id": "U9"},
                  "elements": [{"type": "user", "user_id": "U8"}]}]})
case("_collect_slack_block_mentions",
     {"blocks": [{"type": "rich_text_quote",
                  "elements": [{"type": "rich_text_section",
                                "elements": [{"type": "user", "user_id": "UQ"}]}]},
                 {"type": "user", "user_id": "UO"}]})

# ── _slack_mention_detection_text ──
case("_slack_mention_detection_text", {"event": {"text": "hi"}})
case("_slack_mention_detection_text",
     {"event": {"text": " hi ", "blocks": [{"type": "user", "user_id": "U1"}]}})
case("_slack_mention_detection_text",
     {"event": {"text": "<@U1> hi", "blocks": [{"type": "user", "user_id": "U1"}]}})
case("_slack_mention_detection_text",
     {"event": {"text": "", "blocks": [{"type": "user", "user_id": "U1"},
                                       {"type": "user", "user_id": "U2"}]}})
case("_slack_mention_detection_text", {"event": {"text": "x", "blocks": []}})
case("_slack_mention_detection_text", {"event": {"text": None}})
case("_slack_mention_detection_text",
     {"event": {"text": "q", "blocks": [{"type": "rich_text_quote",
                                        "elements": [{"type": "user", "user_id": "UQ"}]}]}})

# ── _slack_permalink_path ──
for ch, ts in [("C1", "1234.5678"), (None, "1"), ("C1", None), ("", "1"),
               ("C1", ""), ("C1", "1234"), ("C2", "0.0")]:
    case("_slack_permalink_path", {"channel_id": ch, "message_ts": ts})

# ── _str_or_empty ──
for v in ["x", "", None, 0, 123, True, False, "0"]:
    case("_str_or_empty", {"value": v})

# ── _int_or_zero ──
for v in ["123", "abc", "", " 42 ", "-7", "1_000", "+5", "007"]:
    case("_int_or_zero", {"value": v})
case("_int_or_zero", {"value": None}, expect_raise="TypeError")
case("_int_or_zero", {"value": []}, expect_raise="TypeError")
case("_int_or_zero", {"value": True})
case("_int_or_zero", {"value": 3.7})
case("_int_or_zero", {"value": "1_"}, expect_raise=None)  # ValueError -> 0

# ── _first_truthy ──
case("_first_truthy", {"mapping": {"a": 0, "b": "", "c": "x"}, "keys": ["a", "b", "c"]})
case("_first_truthy", {"mapping": {"a": 0, "b": False}, "keys": ["a", "b"]})
case("_first_truthy", {"mapping": {"a": "first", "b": "second"}, "keys": ["a", "b"]})
case("_first_truthy", {"mapping": {"a": 1}, "keys": ["zz", "a"]})
case("_first_truthy", {"mapping": {"a": 1}, "keys": []})
case("_first_truthy", {"mapping": {"a": "0", "b": 0}, "keys": ["b", "a"]})
case("_first_truthy", {"mapping": {}, "keys": ["a"]})

# ── _slack_str_field ──
case("_slack_str_field", {"el": {"text": "hi"}, "name": "text"})
case("_slack_str_field", {"el": {"text": 123}, "name": "text"})
case("_slack_str_field", {"el": {}, "name": "text"})
case("_slack_str_field", {"el": {"text": None}, "name": "text"})
case("_slack_str_field", {"el": {"value": "v"}, "name": "value"})
case("_slack_str_field", {"el": {"text": "hi"}, "name": "value"})

# ── _render_slack_inline_element ──
case("_render_slack_inline_element", {"el": {"type": "text", "text": "hello"}})
case("_render_slack_inline_element", {"el": {"type": "color", "value": "#ff0000"}})
case("_render_slack_inline_element", {"el": {"type": "channel", "channel_id": "C1"}})
case("_render_slack_inline_element", {"el": {"type": "user", "user_id": "U1"}})
case("_render_slack_inline_element", {"el": {"type": "usergroup", "usergroup_id": "S1"}})
case("_render_slack_inline_element", {"el": {"type": "team", "team_id": "T1"}})
case("_render_slack_inline_element", {"el": {"type": "emoji", "name": "wave"}})
case("_render_slack_inline_element", {"el": {"type": "broadcast"}})
case("_render_slack_inline_element", {"el": {"type": "broadcast", "range": "everyone"}})
case("_render_slack_inline_element", {"el": {"type": "date", "fallback": "Jan 1"}})
case("_render_slack_inline_element",
     {"el": {"type": "date", "url": "https://x.com", "fallback": ""}})
case("_render_slack_inline_element",
     {"el": {"type": "link", "url": "https://x.com", "text": "X"}})
case("_render_slack_inline_element",
     {"el": {"type": "link", "url": "https://x.com", "text": "https://x.com"}})
case("_render_slack_inline_element", {"el": {"type": "link", "url": "https://x.com"}})
case("_render_slack_inline_element",
     {"el": {"type": "message_mention", "channel_id": "C1", "message_ts": "12.34"}})
case("_render_slack_inline_element",
     {"el": {"type": "message_mention", "channel_id": "C1", "message_ts": "12.34",
             "text": "label"}})
case("_render_slack_inline_element", {"el": {"type": "mystery", "text": "lbl"}})
case("_render_slack_inline_element", {"el": {"type": "user", "user_id": 42}})
case("_render_slack_inline_element", {"el": {"type": "user"}})
case("_render_slack_inline_element", {"el": {"type": "link", "text": "only"}})

# ── _render_inline_elements ──
case("_render_inline_elements", {"elements": []})
case("_render_inline_elements",
     {"elements": [{"type": "text", "text": "hi "}, {"type": "user", "user_id": "U1"}]})
case("_render_inline_elements",
     {"elements": [{"type": "link", "url": "https://x", "text": "X"},
                   {"type": "emoji", "name": "smile"}]})
case("_render_inline_elements", {"elements": [{"type": "text", "text": "solo"}]})
case("_render_inline_elements",
     {"elements": [{"type": "date", "fallback": "d"}, {"type": "text", "text": " "},
                   {"type": "broadcast"}]})

# ── _unescape_slack_entities ──
for t in ["&amp;", "&lt;&gt;", "a &amp; b &lt; c &gt; d", "&amp;amp;", "", None,
          "&unknown; &amp", "&gt;&gt;"]:
    case("_unescape_slack_entities", {"text": t})

# ── _resolve_slack_audio_ext ──
case("_resolve_slack_audio_ext", {"file_obj": {"name": "clip.mp3"}, "mimetype": "video/mp4"})
case("_resolve_slack_audio_ext", {"file_obj": {"name": "clip.ogg"}, "mimetype": "audio/mp4"})
case("_resolve_slack_audio_ext", {"file_obj": {"name": "clip.xyz"}, "mimetype": "audio/mpeg"})
case("_resolve_slack_audio_ext", {"file_obj": {"name": None}, "mimetype": "audio/mp4; codecs=mp4a"})
case("_resolve_slack_audio_ext", {"file_obj": {"name": "clip"}, "mimetype": "audio/opus"})
case("_resolve_slack_audio_ext", {"file_obj": {"name": "clip.MP4"}, "mimetype": ""})
case("_resolve_slack_audio_ext", {"file_obj": {"name": " clip.wav "}, "mimetype": None})
case("_resolve_slack_audio_ext", {"file_obj": {}, "mimetype": "audio/unknown"})
case("_resolve_slack_audio_ext", {"file_obj": {"name": ".bashrc"}, "mimetype": "audio/aac"})
case("_resolve_slack_audio_ext", {"file_obj": {"name": "a.b.m4a"}, "mimetype": "video/mp4"})

# ── _is_slack_voice_clip ──
case("_is_slack_voice_clip", {"file_obj": {"subtype": "slack_audio"}})
case("_is_slack_voice_clip", {"file_obj": {"subtype": " Slack_Audio "}})
case("_is_slack_voice_clip", {"file_obj": {"name": "audio_message_1.mp4"}})
case("_is_slack_voice_clip", {"file_obj": {"subtype": "slack_video"}})
case("_is_slack_voice_clip", {"file_obj": {"name": "video.mp4"}})
case("_is_slack_voice_clip", {"file_obj": {}})
case("_is_slack_voice_clip", {"file_obj": {"name": " AUDIO_MESSAGE_x "}})

# ── _attachment_label ──
case("_attachment_label", {"file_obj": {"name": "f.png"}})
case("_attachment_label", {"file_obj": {"id": "F1"}})
case("_attachment_label", {"file_obj": {}})
case("_attachment_label", {"file_obj": None})
case("_attachment_label", {"file_obj": {"name": "", "id": "F2"}})
case("_attachment_label", {"file_obj": {"name": 123}})

# ── _standalone_post_kwargs ──
case("_standalone_post_kwargs",
     {"chat_id": "C1", "text": "hi", "unfurl_kwargs": {}, "thread_id": None})
case("_standalone_post_kwargs",
     {"chat_id": "C1", "text": "hi", "unfurl_kwargs": {}, "thread_id": "12.34"})
case("_standalone_post_kwargs",
     {"chat_id": "C1", "text": "hi", "unfurl_kwargs": {"mrkdwn": False}, "thread_id": None})
case("_standalone_post_kwargs",
     {"chat_id": "C1", "text": "hi", "unfurl_kwargs": {"unfurl_links": True}, "thread_id": "1"})
case("_standalone_post_kwargs",
     {"chat_id": "C1", "text": "hi", "unfurl_kwargs": {}, "thread_id": ""})
case("_standalone_post_kwargs",
     {"chat_id": "C1", "text": None, "unfurl_kwargs": {"channel": "C2", "text": "x"},
      "thread_id": None})

# ── _slack_response_payload ──
case("_slack_response_payload", {"response": {"ok": True, "ts": "1"}})
case("_slack_response_payload", {"response": {"__obj__": {"data": {"ok": True}}}})
case("_slack_response_payload", {"response": {"__obj__": {}}})
case("_slack_response_payload", {"response": {"__obj__": {"data": "x"}}})
case("_slack_response_payload", {"response": None})
case("_slack_response_payload", {"response": "str"})
case("_slack_response_payload", {"response": []})

# ── _model_picker_expired_notice (seam: t) ──
case("_model_picker_expired_notice", {}, io={"t": "The model picker expired."})
case("_model_picker_expired_notice", {}, io={"t": ""})
case("_model_picker_expired_notice", {},
     io={"t": script(raise_spec=raise_spec("ValueError", "bad key"))},
     expect_raise="ValueError")
case("_model_picker_expired_notice", {},
     io={"t": script(raise_spec=raise_spec("SystemExit", "bye"))},
     expect_raise="SystemExit")
case("_model_picker_expired_notice", {}, io={}, expect_raise="RuntimeError")

# ── _rewrite_known_bang_command (seam: is_gateway_known_command) ──
case("_rewrite_known_bang_command", {"text": "hello"})
case("_rewrite_known_bang_command", {"text": "!help"},
     io={"is_gateway_known_command": True})
case("_rewrite_known_bang_command", {"text": "!help"},
     io={"is_gateway_known_command": False})
case("_rewrite_known_bang_command", {"text": "!Help@bot arg"},
     io={"is_gateway_known_command": True})
case("_rewrite_known_bang_command", {"text": "!"})
case("_rewrite_known_bang_command", {"text": "! cmd"},
     io={"is_gateway_known_command": True})
case("_rewrite_known_bang_command", {"text": "!a/b"})
case("_rewrite_known_bang_command", {"text": "!help"}, io={})  # fail-closed swallowed
case("_rewrite_known_bang_command", {"text": "!help"},
     io={"is_gateway_known_command": script(raise_spec=raise_spec("SystemExit", "x"))},
     expect_raise="SystemExit")
case("_rewrite_known_bang_command", {"text": "!help"},
     io={"is_gateway_known_command": script(raise_spec=raise_spec("ImportError", "no mod"))})
case("_rewrite_known_bang_command", {"text": "!   "})
case("_rewrite_known_bang_command", {"text": "!HELP"},
     io={"is_gateway_known_command": [False, True]})

# ── _slack_dedup_ttl_seconds (seams: get_scoped_secret, logger.warning) ──
case("_slack_dedup_ttl_seconds", {}, io={"get_scoped_secret": "12.5"})
case("_slack_dedup_ttl_seconds", {}, io={"get_scoped_secret": "0"})
case("_slack_dedup_ttl_seconds", {}, io={"get_scoped_secret": "-3"})
case("_slack_dedup_ttl_seconds", {},
     io={"get_scoped_secret": "abc", "logger.warning": None})
case("_slack_dedup_ttl_seconds", {}, io={"get_scoped_secret": ""})
case("_slack_dedup_ttl_seconds", {}, io={"get_scoped_secret": None})
case("_slack_dedup_ttl_seconds", {}, io={"get_scoped_secret": "nan"})
case("_slack_dedup_ttl_seconds", {}, io={"get_scoped_secret": " 60 "})
case("_slack_dedup_ttl_seconds", {}, io={"get_scoped_secret": 45})
case("_slack_dedup_ttl_seconds", {}, io={"get_scoped_secret": True})
case("_slack_dedup_ttl_seconds", {}, io={"get_scoped_secret": script(ret=[1])},
     expect_raise="TypeError")
case("_slack_dedup_ttl_seconds", {}, io={}, expect_raise="RuntimeError")
case("_slack_dedup_ttl_seconds", {},
     io={"get_scoped_secret": script(raise_spec=raise_spec("KeyboardInterrupt", ""))},
     expect_raise="KeyboardInterrupt")
case("_slack_dedup_ttl_seconds", {}, io={"get_scoped_secret": "1_000"})
case("_slack_dedup_ttl_seconds", {},
     io={"get_scoped_secret": "abc",
         "logger.warning": script(raise_spec=raise_spec("SystemExit", "log"))},
     expect_raise="SystemExit")

# ── _resolve_slack_proxy_url ──
NO_PROXY = "is_host_excluded_by_no_proxy"
case("_resolve_slack_proxy_url", {}, io={"resolve_proxy_url": None})
case("_resolve_slack_proxy_url", {}, io={"resolve_proxy_url": ""})
case("_resolve_slack_proxy_url", {},
     io={"resolve_proxy_url": "http://p:8080", NO_PROXY: [False, False, False]})
case("_resolve_slack_proxy_url", {},
     io={"resolve_proxy_url": "HTTP://P", NO_PROXY: [False, False, False]})
case("_resolve_slack_proxy_url", {},
     io={"resolve_proxy_url": "socks5://p", "safe_proxy_url_for_log": "socks5://p",
         "logger.info": None})
case("_resolve_slack_proxy_url", {},
     io={"resolve_proxy_url": "http://p", NO_PROXY: [True], "logger.info": None})
case("_resolve_slack_proxy_url", {},
     io={"resolve_proxy_url": "http://p", NO_PROXY: [False, True], "logger.info": None})
case("_resolve_slack_proxy_url", {},
     io={"resolve_proxy_url": "http://p", NO_PROXY: [False, False, True],
         "logger.info": None})
case("_resolve_slack_proxy_url", {}, io={}, expect_raise="RuntimeError")
case("_resolve_slack_proxy_url", {},
     io={"resolve_proxy_url": "https://p",
         NO_PROXY: script(raise_spec=raise_spec("GeneratorExit", ""))},
     expect_raise="GeneratorExit")
case("_resolve_slack_proxy_url", {},
     io={"resolve_proxy_url": "ftp://x",
         "safe_proxy_url_for_log": script(raise_spec=raise_spec("ValueError", "bad"))},
     expect_raise="ValueError")

# ── _trim_slack_dm_cache (state flow) ──
def dm_state(n):
    return {"dm_cache": {f"k{i:05d}": f"D{i:05d}" for i in range(n)}}


case("_trim_slack_dm_cache", {}, state=dm_state(5002))
case("_trim_slack_dm_cache", {}, state=dm_state(5000))
case("_trim_slack_dm_cache", {}, state=dm_state(0))
case("_trim_slack_dm_cache", {}, state=dm_state(1))
case("_trim_slack_dm_cache", {}, state=dm_state(5001))
case("_trim_slack_dm_cache", {}, state={})

# ── _load_slack_bot_tokens ──
HOME = "/h/.hermes"
PATH = HOME + "/slack_tokens.json"
GH = "get_hermes_home"
EX = "tokens_file.exists"
RD = "tokens_file.read_text"
WN = "warn_if_credential_file_broadly_readable"
LI = "logger.info"
LW = "logger.warning"
SAVED = json.dumps({"T1": {"token": "xoxb-3", "team_name": "Acme"},
                    "T2": {"token": "xoxb-4"}})
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1,xoxb-2", "quiet": True},
     io={GH: HOME, EX: False})
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1,xoxb-2", "quiet": False},
     io={GH: HOME, EX: True, WN: None, RD: SAVED, LI: [None, None]})
case("_load_slack_bot_tokens", {"raw_token": "xoxb-3", "quiet": False},
     io={GH: HOME, EX: True, WN: None, RD: SAVED, LI: [None]})
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1", "quiet": True},
     io={GH: HOME, EX: True, RD: SAVED})
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1", "quiet": True},
     io={GH: script(raise_spec=raise_spec("ValueError", "no home"))})
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1", "quiet": False},
     io={GH: script(raise_spec=raise_spec("ValueError", "no home"))},
     expect_raise="ValueError")
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1", "quiet": False},
     io={GH: None}, expect_raise="TypeError")
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1", "quiet": True},
     io={GH: None})
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1", "quiet": False},
     io={GH: HOME, EX: True, WN: None,
         RD: script(raise_spec=raise_spec("OSError", "disk")), LW: None})
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1", "quiet": False},
     io={GH: HOME, EX: True, WN: None, RD: "", LW: None})
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1", "quiet": False},
     io={GH: HOME, EX: True, WN: None, RD: "[]", LW: None})
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1", "quiet": False},
     io={GH: HOME, EX: True, WN: None, RD: "\ufeff" + SAVED, LI: [None, None]})
case("_load_slack_bot_tokens", {"raw_token": "", "quiet": False},
     io={GH: HOME, EX: True, WN: None,
         RD: json.dumps({"T1": "notdict", "T2": {"token": ""}, "T3": {"token": None},
                         "T4": {"token": "xoxb-9"}}), LI: [None]})
case("_load_slack_bot_tokens", {"raw_token": " a , , b ", "quiet": True},
     io={GH: HOME, EX: False})
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1", "quiet": False},
     io={GH: HOME, EX: True, WN: None, RD: "  ", LW: None})
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1", "quiet": False},
     io={GH: HOME, EX: True, WN: None,
         RD: script(raise_spec=raise_spec("SystemExit", "stop"))},
     expect_raise="SystemExit")
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1", "quiet": False},
     io={GH: HOME, EX: True,
         WN: script(raise_spec=raise_spec("RuntimeError", "warn")), LW: None})
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1", "quiet": True},
     io={GH: HOME, EX: True, RD: "null", "logger.warning": None})
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1", "quiet": False},
     io={GH: "/", EX: True, WN: None, RD: SAVED, LI: [None, None]})
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1", "quiet": False},
     io={GH: HOME, EX: True, WN: None, RD: SAVED,
         LI: script(raise_spec=raise_spec("RuntimeError", "log fail")), LW: None})
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1", "quiet": False},
     io={GH: script(raise_spec=raise_spec("SystemExit", "stop"))},
     expect_raise="SystemExit")
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1", "quiet": True},
     io={GH: HOME, EX: script(raise_spec=raise_spec("GeneratorExit", ""))},
     expect_raise="GeneratorExit")
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1", "quiet": False},
     io={}, expect_raise="RuntimeError")
case("_load_slack_bot_tokens", {"raw_token": "xoxb-1", "quiet": True}, io={})

# ── seeded-random broadening (tables / width / unescape) ──
rng = random.Random(20261118)
ALPHA = "abc| -:```\\\n\t"
for _ in range(14):
    n = rng.randint(1, 6)
    lines = ["".join(rng.choice(ALPHA) for _ in range(rng.randint(0, 12)))
             for _ in range(n)]
    case("_wrap_markdown_tables", {"text": "\n".join(lines)})
WALPHA = "abcXYZ你好ＡＢ😀éｦ🇺🇸 123"
for _ in range(8):
    s = "".join(rng.choice(WALPHA) for _ in range(rng.randint(0, 10)))
    case("_disp_width", {"s": s})
    case("_pad", {"cell": s, "width": rng.randint(0, 12)})
EALPHA = "&;amp<>ltgt xyz"
for _ in range(8):
    s = "".join(rng.choice(EALPHA) for _ in range(rng.randint(0, 16)))
    case("_unescape_slack_entities", {"text": s})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for rec in CASES:
            counts[rec["func"]] = counts.get(rec["func"], 0) + 1
            f.write(json.dumps(rec, ensure_ascii=False) + "\n")
    print(f"GEN slack_adapter_flow: {len(CASES)} cases, {len(counts)} funcs")
    thin = [f for f, c in counts.items() if c < 5]
    if thin:
        print(f"GEN THIN (<5): {thin}")
        sys.exit(1)
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")


if __name__ == "__main__":
    main()