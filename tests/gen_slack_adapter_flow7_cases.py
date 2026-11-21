#!/usr/bin/env python3
"""Generate tests/fixtures/slack_adapter_flow7.jsonl for the Slack
plugin-adapter FLOW port, wave 7 (src/gateway/platforms/slack_adapter_flow7.c).

Deterministic corpus: >=6 cases per ported function (16 funcs), covering pure
logic, chained seams with ORDERED traces, fail-closed missing scripts and
BaseException passthrough. Case shape:
{"func", "in": {"args","io"?,"state"}, "expect_raise"?}.

Representation markers (shared with the C port + oracle):
- {"__obj__": {attrs}} -- attribute carrier (SimpleNamespace double)
- io values: a plain JSON value, a LIST consumed in order, or
  {"__script__":true,"ret":v} / {"__script__":true,"raise":{...}}.

Domain guards (ASCII scope): str() positions carry str/int/bool/None only.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "slack_adapter_flow7.jsonl")
CASES = []

SDT = {
    ".txt": "text/plain", ".md": "text/markdown", ".csv": "text/csv",
    ".json": "application/json", ".pdf": "application/pdf", ".log": "text/plain",
    ".py": "text/plain", ".ts": "text/plain", ".sh": "text/plain",
}
TEXT_EXT = [".txt", ".md", ".csv", ".log", ".json", ".py", ".ts", ".sh"]


def case(func, args=None, io=None, state=None, expect_raise=None):
    args = dict(args or {})
    st = dict(state) if state is not None else {}
    inp = {"args": args, "state": st}
    if io is not None:
        inp["io"] = io
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


def rais(kind, message):
    return script(raise_spec={"type": kind, "message": message})


def obj(**attrs):
    return {"__obj__": attrs}


# ------------------------------- __init__
for i, cfg in enumerate([
    obj(name="slack", token="xoxb-1"),
    obj(name="slack", token="xoxb-2"),
    {"name": "slack", "extra": {"x": 1}},
    obj(name="primary"),
    None,
    obj(name="secondary", team="T1"),
]):
    case("__init__", {"config": cfg},
         io={"super_init": None, "slack_dedup_ttl": 60 + i,
             "new_deduplicator": obj(ttl=60 + i), "new_slash_lanes": obj(lanes=[]),
             "new_async_lock": obj(locked=False)})

# ------------------------------- _add_reaction
for i, (ch, ts, em, tid) in enumerate([
    ("C1", "111.1", "thumbsup", ""),
    ("C2", "222.2", "eyes", "T1"),
    ("D3", "333.3", "fire", ""),
    ("C4", "444.4", "ok", "T2"),
    ("C5", "555.5", "heart", ""),
]):
    case("_add_reaction", {"channel": ch, "timestamp": ts, "emoji": em, "team_id": tid},
         io={"react": True})
case("_add_reaction", {"channel": "C1", "timestamp": "1", "emoji": "x", "team_id": ""},
     io={"react": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _agent_view_context_for_event
def av_ctx(team, user, ccid):
    return {"context_channel_id": ccid, "team_id": team, "user_id": user}

case("_agent_view_context_for_event",
     {"event": {"app_context": {"channel": "C9"}}, "team_id": "T1", "user_id": "U1"},
     state={"_agent_view_contexts": {"T1:U1": av_ctx("T1", "U1", "CACHE")}},
     io={"context_channel_id": "C9", "agent_view_context_key": "T1:U1"})
case("_agent_view_context_for_event",
     {"event": {"context": {"channel": "C8"}}, "team_id": "T1", "user_id": "U1"},
     state={"_agent_view_contexts": {"T1:U1": av_ctx("T1", "U1", "CACHE")}},
     io={"context_channel_id": "", "agent_view_context_key": "T1:U1"})
case("_agent_view_context_for_event",
     {"event": {}, "team_id": "T2", "user_id": "U2"},
     state={"_agent_view_contexts": {"T2:U2": av_ctx("T2", "U2", "CC7")}},
     io={"context_channel_id": None, "agent_view_context_key": "T2:U2"})
case("_agent_view_context_for_event",
     {"event": {"app_context": "A"}, "team_id": "T3", "user_id": "U3"},
     io={"context_channel_id": "C3", "agent_view_context_key": "T3:U3"})
case("_agent_view_context_for_event",
     {"event": {"app_context": "A"}, "team_id": "T4", "user_id": "U4"},
     io={"context_channel_id": rais("KeyboardInterrupt", "base"),
         "agent_view_context_key": "T4:U4"}, expect_raise="KeyboardInterrupt")
case("_agent_view_context_for_event",
     {"event": {"app_context": "A"}, "team_id": "T5", "user_id": "U5"},
     io={"context_channel_id": "C5"}, expect_raise="RuntimeError")

# ------------------------------- _append_link_unfurls
case("_append_link_unfurls",
     {"text": "", "slack_attachments": [
         {"title": "T", "title_link": "http://x", "text": "body"}]},
     io={"is_forward": False, "extract_text_from_slack_blocks": "",
         "elide": "", "logger.debug": None})
case("_append_link_unfurls",
     {"text": "intro", "slack_attachments": [
         {"title": "T", "from_url": "http://y", "text": "body", "footer": "note"}]},
     io={"is_forward": False, "extract_text_from_slack_blocks": "",
         "elide": "", "logger.debug": None})
case("_append_link_unfurls",
     {"text": "", "slack_attachments": [
         {"is_msg_unfurl": True, "text": "skip"}]},
     io={"is_forward": False, "extract_text_from_slack_blocks": "", "elide": ""})
case("_append_link_unfurls",
     {"text": "", "slack_attachments": [
         {"title": "F", "title_link": "http://f", "text": "skip"}]},
     io={"is_forward": True, "extract_text_from_slack_blocks": "", "elide": ""})
case("_append_link_unfurls",
     {"text": "", "slack_attachments": [
         {"title": "N", "title_link": "http://n", "text": "base",
          "blocks": [{"type": "section"}]}]},
     io={"is_forward": False, "extract_text_from_slack_blocks": "table text",
         "elide": "table text", "logger.debug": None})
case("_append_link_unfurls",
     {"text": "", "slack_attachments": [{"text": "x" * 600}]},
     io={"is_forward": False, "extract_text_from_slack_blocks": "", "elide": "",
         "logger.debug": None})
case("_append_link_unfurls",
     {"text": "", "slack_attachments": [{"title": "Z", "title_link": "http://z"}]},
     io={"is_forward": rais("SystemExit", "base"),
         "extract_text_from_slack_blocks": "", "elide": ""},
     expect_raise="SystemExit")

# ------------------------------- _apply_bot_mention
case("_apply_bot_mention",
     {"text": "  hi <@U1> there  ", "original_text": "  hi <@U1> there  ",
      "command_probe_text": "old", "is_command_text": False, "bot_uid": "U1",
      "thread_ts": None, "team_id": "T1"},
     io={"rewrite_known_bang_command": "plain"})
case("_apply_bot_mention",
     {"text": "<@U1> !go now", "original_text": "<@U1> !go now",
      "command_probe_text": "", "is_command_text": False, "bot_uid": "U1",
      "thread_ts": "111.2", "team_id": "T1"},
     io={"rewrite_known_bang_command": "/go now", "slack_strict_mention": False,
         "slack_thread_require_mention": False, "register_mentioned_thread": None})
case("_apply_bot_mention",
     {"text": "<@U1> /cmd x", "original_text": "<@U1> /cmd x",
      "command_probe_text": "", "is_command_text": False, "bot_uid": "U1",
      "thread_ts": "222.2", "team_id": "T2"},
     io={"slack_strict_mention": False, "slack_thread_require_mention": False,
         "register_mentioned_thread": None})
case("_apply_bot_mention",
     {"text": "<@U1> hi", "original_text": "<@U1> hi", "command_probe_text": "p",
      "is_command_text": True, "bot_uid": "U1", "thread_ts": "333.3", "team_id": "T3"},
     io={"rewrite_known_bang_command": "hi", "slack_strict_mention": True})
case("_apply_bot_mention",
     {"text": "text <@U2>", "original_text": "text <@U2>", "command_probe_text": "p",
      "is_command_text": False, "bot_uid": "U2", "thread_ts": "444.4", "team_id": "T4"},
     io={"rewrite_known_bang_command": "text", "slack_strict_mention": False,
         "slack_thread_require_mention": True})
case("_apply_bot_mention",
     {"text": "x", "original_text": "x", "command_probe_text": "p",
      "is_command_text": False, "bot_uid": "U3", "thread_ts": "555.5", "team_id": "T5"},
     io={"rewrite_known_bang_command": rais("ValueError", "bang"),
         "slack_strict_mention": False}, expect_raise="ValueError")

# ------------------------------- _apply_yaml_config
BRIDGE = [["require_mention", "SLACK_REQUIRE_MENTION", "lower"],
          ["allowed_channels", "SLACK_ALLOWED_CHANNELS", "csv"]]
for i, sc in enumerate([{"require_mention": "true"},
                        {"allowed_channels": "C1,C2"},
                        {}, {"strict_mention": "false"},
                        {"reactions": "yes"}, {"x": 1}]):
    case("_apply_yaml_config",
         {"yaml_cfg": {"slack": sc}, "slack_cfg": sc, "bridge": BRIDGE},
         io={"apply_yaml_bridge": {"ok": True, "i": i}})
case("_apply_yaml_config",
     {"yaml_cfg": {}, "slack_cfg": {}, "bridge": BRIDGE},
     io={"apply_yaml_bridge": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _assistant_suggested_prompts
case("_assistant_suggested_prompts",
     {"extra": {"suggested_prompts": {"title": "  Ideas  ", "prompts": [
         {"title": " A ", "message": " do a "},
         {"title": "B", "message": " do b "},
         {"title": "", "message": "skip"}]}}})
case("_assistant_suggested_prompts",
     {"extra": {"suggested_prompts": {"title": "T", "prompts": [
         {"title": f"P{i}", "message": "m"} for i in range(7)]}}})
case("_assistant_suggested_prompts",
     {"extra": {"suggested_prompts": [{"title": "A", "message": "b"}]}})
case("_assistant_suggested_prompts",
     {"extra": {"suggested_prompts": {"title": "NoPrompts"}}})
case("_assistant_suggested_prompts",
     {"extra": {"suggested_prompts": "not a list"}})
case("_assistant_suggested_prompts",
     {"extra": {"suggested_prompts": {"title": "X", "prompts": [
         {"title": "L" * 100, "message": "msg"},
         {"title": "N", "message": "n"}]}}})
case("_assistant_suggested_prompts", {"extra": {}})

# ------------------------------- _authenticate_workspace
for i, (tok, proxy) in enumerate([("xoxb-1", None), ("xoxb-2", "http://p"),
                                  ("xoxb-3", None), ("xoxb-4", "http://q"),
                                  ("xoxb-5", None)]):
    case("_authenticate_workspace", {"token": tok, "proxy_url": proxy},
         io={"new_web_client": obj(ok=True),
             "auth_test": {"team_id": f"T{i}", "user_id": f"U{i}",
                           "user": f"bot{i}", "team": f"Team{i}"},
             "logger.info": None, "warn_group_dm_scopes": None,
             "warn_not_bot_token": None, "warn_inchannel": None})
case("_authenticate_workspace", {"token": "x", "proxy_url": None},
     io={"new_web_client": obj(),
         "auth_test": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _begin_interaction
FIELDS = ["act1", "val1", {"text": "msg"}, "111.2", "C1", "alice", "U1"]
case("_begin_interaction",
     {"body": {"channel": "C1"}, "action": {"action_id": "a"}, "kind": "button",
      "team_scoped": True},
     io={"ack": None, "event_team_id": "T1",
         "interaction_fields": script(ret=FIELDS),
         "is_interactive_user_authorized": True})
case("_begin_interaction",
     {"body": {"channel": "C2"}, "action": {"action_id": "b"}, "kind": "button",
      "team_scoped": False},
     io={"ack": None, "event_team_id": "T2",
         "interaction_fields": script(ret=["a2", "v2", "m2", "t2", "C2", "bob", "U2"]),
         "is_interactive_user_authorized": True})
case("_begin_interaction",
     {"body": {}, "action": {}, "kind": "select", "team_scoped": True},
     io={"ack": None, "event_team_id": "T3",
         "interaction_fields": script(ret=["a3", "v3", "m3", "t3", "C3", "carol", "U3"]),
         "is_interactive_user_authorized": False, "logger.warning": None})
case("_begin_interaction",
     {"body": {}, "action": {}, "kind": "button", "team_scoped": True},
     io={"ack": None, "event_team_id": "T4",
         "interaction_fields": script(ret=["a4", "v4", "m4", "t4", "C4", "dave", "U4"]),
         "is_interactive_user_authorized": rais("RuntimeError", "auth")},
     expect_raise="RuntimeError")
case("_begin_interaction",
     {"body": {}, "action": {}, "kind": "button"},
     io={"ack": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_begin_interaction",
     {"body": {}, "action": {}, "kind": "button"},
     io={"ack": None, "event_team_id": "T6"}, expect_raise="RuntimeError")

# ------------------------------- _bot_authored_thread_root
CACHE_HIT = {"C1:111.1:x": obj(parent_user_id="UBOT")}
CACHE_MISS = {"C2:222.2:x": obj(parent_user_id="OTHER")}
case("_bot_authored_thread_root", {"channel_id": "C1", "thread_ts": "111.1", "team_id": "T1"},
     state={"_team_bot_user_ids": {"T1": "UBOT"}, "_thread_context_cache": CACHE_HIT},
     io={"fetch_thread_context": None})
case("_bot_authored_thread_root", {"channel_id": "C2", "thread_ts": "222.2", "team_id": "T1"},
     state={"_team_bot_user_ids": {"T1": "UBOT"}, "_thread_context_cache": CACHE_MISS},
     io={"fetch_thread_context": None})
case("_bot_authored_thread_root", {"channel_id": "C3", "thread_ts": "333.3", "team_id": "T2"},
     state={"_team_bot_user_ids": {"T2": "UBOT"}, "_thread_context_cache": {}},
     io={"fetch_thread_context": None})
case("_bot_authored_thread_root", {"channel_id": "C4", "thread_ts": "", "team_id": "T1"},
     state={"_team_bot_user_ids": {"T1": "UBOT"}})
case("_bot_authored_thread_root", {"channel_id": "C5", "thread_ts": "555.5", "team_id": "T9"},
     state={"_team_bot_user_ids": {}, "_bot_user_id": None})
case("_bot_authored_thread_root", {"channel_id": "C6", "thread_ts": "666.6", "team_id": "T1"},
     state={"_team_bot_user_ids": {"T1": "UBOT"}, "_thread_context_cache": {}},
     io={"fetch_thread_context": rais("KeyboardInterrupt", "base")},
     expect_raise="KeyboardInterrupt")

# ------------------------------- _build_message_event
def bme(**over):
    d = dict(event={"channel_type": "channel"}, text="hello", original_text="hello",
             command_probe_text="hello", is_command_text=False, channel_id="C1",
             team_id="T1", ts="111.1", user_id="U1", thread_ts="222.2", is_dm=True,
             media_urls=[], media_types=[], media_text_inlined=[], channel_context=None,
             reply_expected=None, extra={})
    d.update(over)
    return d

BME_IO = {
    "media_message_type": "text", "resolve_user_name": "alice",
    "resolve_channel_name": "general", "set_assistant_thread_title": None,
    "event_declares_bot_sender": False, "resolve_user_is_bot": False,
    "build_source": obj(chat_id="C1", user_name="alice"),
    "humanize_user_mentions": "hello @bob", "channel_prompt_with_identity": "prompt",
    "resolve_channel_skills": [], "MessageEvent": {"message_id": "111.1"},
}
case("_build_message_event", bme(), io=dict(BME_IO))
case("_build_message_event", bme(is_command_text=True, command_probe_text="/go",
                                 text="hello", is_dm=False, thread_ts=None),
     io={k: v for k, v in BME_IO.items() if k != "media_message_type"})
case("_build_message_event", bme(event={"channel_type": "mpim"}), io=dict(BME_IO))
case("_build_message_event", bme(event={"channel_type": "channel"}, is_dm=False, thread_ts=None),
     io=dict(BME_IO))
case("_build_message_event", bme(event={"channel_type": "channel"}),
     io={**BME_IO, "event_declares_bot_sender": True})
case("_build_message_event", bme(),
     io={**BME_IO, "resolve_user_name": rais("SystemExit", "base")},
     expect_raise="SystemExit")

# ------------------------------- _build_model_picker_provider_blocks
PROVS = [
    {"name": "OpenAI", "slug": "openai", "models": ["m1", "m2"], "total_models": 2},
    {"name": "Anthropic", "slug": "anthropic", "models": ["m3"]},
]
case("_build_model_picker_provider_blocks",
     {"providers": PROVS, "current_model": "m1", "provider_label": "OpenAI"},
     io={"t": ["", "", "Header", "Choose", "Cancel"]})
case("_build_model_picker_provider_blocks",
     {"providers": PROVS, "current_model": "", "provider_label": "P"},
     io={"t": ["Unknown", "Header", "Choose", "Cancel"]})
case("_build_model_picker_provider_blocks",
     {"providers": [{"name": "P", "slug": "p", "models": ["x"], "total_models": 5}],
      "current_model": "x", "provider_label": "P"},
     io={"t": ["", "Header", "Choose", "Cancel"]})
case("_build_model_picker_provider_blocks",
     {"providers": [{"name": f"P{i}", "slug": f"p{i}", "models": ["x"]} for i in range(101)],
      "current_model": "x", "provider_label": "P0"},
     io={"t": ["more", "", "Header", "Choose", "Cancel"]})
case("_build_model_picker_provider_blocks",
     {"providers": [], "current_model": "", "provider_label": ""},
     io={"t": ["Unknown", "Header", "Choose", "Cancel"]})
case("_build_model_picker_provider_blocks",
     {"providers": PROVS, "current_model": "m1", "provider_label": "OpenAI"},
     io={"t": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _build_model_picker_model_blocks
case("_build_model_picker_model_blocks",
     {"providers": PROVS, "provider_slug": "openai"},
     io={"picker_row_suffix": ["", ""], "t": ["", "Header", "Choose", "Back", "Cancel"]})
case("_build_model_picker_model_blocks",
     {"providers": PROVS, "provider_slug": "anthropic"},
     io={"picker_row_suffix": ["Free"], "t": ["Header", "Choose", "Back", "Cancel"]})
case("_build_model_picker_model_blocks",
     {"providers": PROVS, "provider_slug": ""},
     io={"picker_row_suffix": [], "t": ["Header", "Choose", "Cancel"]})
case("_build_model_picker_model_blocks",
     {"providers": [{"name": "P", "slug": "p", "models": ["a/b"], "total_models": 9,
                     "plan_line": "Pro plan"}], "provider_slug": "p"},
     io={"picker_row_suffix": ["Large"], "t": ["more", "Header", "Choose", "Back", "Cancel"]})
case("_build_model_picker_model_blocks",
     {"providers": PROVS, "provider_slug": "missing"},
     io={"picker_row_suffix": [], "t": ["Header", "Choose", "Back", "Cancel"]})
case("_build_model_picker_model_blocks",
     {"providers": PROVS, "provider_slug": "openai"},
     io={"picker_row_suffix": rais("ValueError", "suffix")}, expect_raise="ValueError")

# ------------------------------- _cache_agent_view_context
case("_cache_agent_view_context",
     {"metadata": {"team_id": "T1", "user_id": "U1", "channel_id": "C1",
                   "context_channel_id": "CC1", "ignored": "x"}},
     io={"agent_view_context_key": "T1:U1", "trim_oldest": None})
case("_cache_agent_view_context",
     {"metadata": {"team_id": "T2", "user_id": "U2", "channel_id": ""}},
     io={"agent_view_context_key": "T2:U2", "trim_oldest": None})
case("_cache_agent_view_context",
     {"metadata": {"team_id": "", "user_id": ""}},
     io={"agent_view_context_key": ""})
case("_cache_agent_view_context",
     {"metadata": {"team_id": "T4", "user_id": "U4", "channel_id": "C4"}},
     io={"agent_view_context_key": "T4:U4", "trim_oldest": rais("SystemExit", "base")},
     expect_raise="SystemExit")
case("_cache_agent_view_context",
     {"metadata": {"team_id": "T5", "user_id": "U5", "channel_id": "C5"}},
     io={"agent_view_context_key": rais("ValueError", "key")}, expect_raise="ValueError")
case("_cache_agent_view_context",
     {"metadata": {"team_id": "T6", "user_id": "U6"}},
     io={"agent_view_context_key": "T6:U6"}, expect_raise="RuntimeError")

# ------------------------------- _cache_assistant_thread_metadata
case("_cache_assistant_thread_metadata",
     {"metadata": {"team_id": "T1", "channel_id": "C1", "thread_ts": "111.1",
                   "session_key": "s1", "empty": ""}},
     state={"_assistant_threads": {"old": {"session_key": "old"}}},
     io={"workspace_thread_key": "T1:C1:111.1", "trim_oldest": None,
         "remember_channel_team": None})
case("_cache_assistant_thread_metadata",
     {"metadata": {"team_id": "T2", "channel_id": "C2", "thread_ts": "222.2"}},
     io={"workspace_thread_key": "T2:C2:222.2", "trim_oldest": None,
         "remember_channel_team": None})
case("_cache_assistant_thread_metadata",
     {"metadata": {"team_id": "", "channel_id": "C3", "thread_ts": "333.3"}},
     io={"workspace_thread_key": "", "trim_oldest": None, "remember_channel_team": None})
case("_cache_assistant_thread_metadata",
     {"metadata": {"team_id": "T4", "channel_id": "", "thread_ts": "444.4"}},
     io={"workspace_thread_key": "T4::444.4", "trim_oldest": None})
case("_cache_assistant_thread_metadata",
     {"metadata": {"team_id": "T5", "channel_id": "C5", "thread_ts": "555.5"}},
     io={"workspace_thread_key": "T5:C5:555.5", "trim_oldest": None,
         "remember_channel_team": rais("GeneratorExit", "base")},
     expect_raise="GeneratorExit")
case("_cache_assistant_thread_metadata",
     {"metadata": {"team_id": "T6", "channel_id": "C6", "thread_ts": "666.6"}},
     io={"workspace_thread_key": "T6:C6:666.6"}, expect_raise="RuntimeError")

# ------------------------------- _cache_slack_document
DOC_STATE = {"SUPPORTED_DOCUMENT_TYPES": SDT, "_TEXT_INJECT_EXTENSIONS": TEXT_EXT}
case("_cache_slack_document",
     {"f": {"name": "report.pdf", "size": 100}, "url": "http://x", "mimetype": "", "team_id": "T1"},
     state=DOC_STATE,
     io={"download_slack_file_bytes": "pdfdata",
         "cache_document_from_bytes_async": "/cache/report.pdf", "logger.debug": None})
case("_cache_slack_document",
     {"f": {"name": "notes.txt", "size": 50}, "url": "http://y", "mimetype": "text/plain", "team_id": "T2"},
     state=DOC_STATE,
     io={"download_slack_file_bytes": "hello world",
         "cache_document_from_bytes_async": "/cache/notes.txt", "logger.debug": None,
         "re.sub": "notes_txt"})
case("_cache_slack_document",
     {"f": {"name": "data", "size": 10}, "url": "http://z", "mimetype": "application/pdf", "team_id": "T3"},
     state=DOC_STATE,
     io={"download_slack_file_bytes": "rawdata",
         "cache_document_from_bytes_async": "/cache/document.pdf", "logger.debug": None})
case("_cache_slack_document",
     {"f": {"name": "big.pdf", "size": 21 * 1024 * 1024}, "url": "http://x", "mimetype": "", "team_id": "T4"},
     state=DOC_STATE, io={"logger.warning": None})
case("_cache_slack_document",
     {"f": {"name": "unknown", "size": 0}, "url": "http://x", "mimetype": "", "team_id": "T5"},
     state=DOC_STATE, io={"logger.warning": None})
case("_cache_slack_document",
     {"f": {"name": "a.txt", "size": 5}, "url": "http://x", "mimetype": "text/plain", "team_id": "T6"},
     state=DOC_STATE,
     io={"download_slack_file_bytes": rais("SystemExit", "base")},
     expect_raise="SystemExit")


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for rec in CASES:
            f.write(json.dumps(rec, ensure_ascii=False) + "\n")
    from collections import Counter
    c = Counter(r["func"] for r in CASES)
    keys = set()
    for r in CASES:
        keys |= set((r["in"]).get("io") or {})
    print(f"wrote {os.path.getsize(OUT)} bytes, {len(CASES)} cases, {len(c)} funcs")
    print("min per func:", min(c.values()), "seam io keys:", len(keys))
    for fn in sorted(c):
        print(f"  {fn}: {c[fn]}")


if __name__ == "__main__":
    main()
