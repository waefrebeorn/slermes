#!/usr/bin/env python3
"""Generate tests/fixtures/slack_adapter_flow5.jsonl for the Slack
plugin-adapter FLOW port, wave 5 (src/gateway/platforms/slack_adapter_flow5.c).

Deterministic corpus: >=6 cases per ported function (18 funcs), covering pure
leaves, chained seams with ORDERED traces, state echoes, fail-closed missing
scripts and BaseException passthrough. Case shape:
{"func", "in": {"args","io"?,"state"}, "expect_raise"?}.

Representation markers (shared with the C port + oracle):
- {"__obj__": {attrs}} -- attribute carrier (SimpleNamespace double)
- {"__set__": [members]} -- set carrier
- io values: a plain JSON value, a LIST consumed in order, or
  {"__script__":true,"ret":v} / {"__script__":true,"raise":{...}}.

Domain guards (ASCII scope): str() positions carry str/int/bool/None only;
every case's state carries an "extra" object so args echoes match; wake-word
pattern doubles are non-empty substrings.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "slack_adapter_flow5.jsonl")
CASES = []


def case(func, args=None, io=None, state=None, expect_raise=None):
    args = dict(args or {})
    st = dict(state) if state is not None else {}
    st.setdefault("extra", {})
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


def client(**attrs):
    return {"__obj__": attrs}


def store(per_user):
    return client(config=client(thread_sessions_per_user=per_user))


# ------------------------------- _session_status_method
case("_session_status_method", {"client": client(agents_sessions_setStatus="M_NEW", assistant_threads_setStatus="M_OLD")},
     io={"sdk_supports_agent_sessions": True})
case("_session_status_method", {"client": client(assistant_threads_setStatus="M_OLD")},
     io={"sdk_supports_agent_sessions": True})
case("_session_status_method", {"client": client(agents_sessions_setStatus="M_NEW", assistant_threads_setStatus="M_OLD")},
     io={"sdk_supports_agent_sessions": False})
case("_session_status_method", {"client": client()},
     io={"sdk_supports_agent_sessions": True}, expect_raise="AttributeError")
case("_session_status_method", {"client": client()},
     io={"sdk_supports_agent_sessions": False}, expect_raise="AttributeError")
case("_session_status_method", {"client": client(assistant_threads_setStatus="M_OLD")},
     io={"sdk_supports_agent_sessions": rais("RuntimeError", "boom")}, expect_raise="RuntimeError")
case("_session_status_method", {"client": client(assistant_threads_setStatus="M_OLD")},
     io={"sdk_supports_agent_sessions": rais("SystemExit", "bye")}, expect_raise="SystemExit")

# ------------------------------- _session_title_method
case("_session_title_method", {"client": client(agents_sessions_rename="R_NEW", assistant_threads_setTitle="R_OLD")},
     io={"sdk_supports_agent_sessions": True})
case("_session_title_method", {"client": client(assistant_threads_setTitle="R_OLD")},
     io={"sdk_supports_agent_sessions": True})
case("_session_title_method", {"client": client(agents_sessions_rename="R_NEW", assistant_threads_setTitle="R_OLD")},
     io={"sdk_supports_agent_sessions": False})
case("_session_title_method", {"client": client()},
     io={"sdk_supports_agent_sessions": True}, expect_raise="AttributeError")
case("_session_title_method", {"client": client()},
     io={"sdk_supports_agent_sessions": False}, expect_raise="AttributeError")
case("_session_title_method", {"client": client(assistant_threads_setTitle="R_OLD")},
     io={"sdk_supports_agent_sessions": rais("ValueError", "bad")}, expect_raise="ValueError")
case("_session_title_method", {"client": client(assistant_threads_setTitle="R_OLD")},
     io={"sdk_supports_agent_sessions": rais("CancelledError", "cx")}, expect_raise="CancelledError")

# ------------------------------- _standalone_format_mrkdwn
case("_standalone_format_mrkdwn", {"text": ""})
case("_standalone_format_mrkdwn", {"text": None})
case("_standalone_format_mrkdwn", {"text": "hi"}, io={"format_message": "**hi**"})
case("_standalone_format_mrkdwn", {"text": "hi"},
     io={"format_message": rais("RuntimeError", "fmt"), "logger.debug": None})
case("_standalone_format_mrkdwn", {"text": "hi"},
     io={"format_message": rais("SystemExit", "bye")}, expect_raise="SystemExit")
case("_standalone_format_mrkdwn", {"text": "hi"},
     io={"format_message": rais("RuntimeError", "f"), "logger.debug": rais("RuntimeError", "log")},
     expect_raise="RuntimeError")
case("_standalone_format_mrkdwn", {"text": "x"}, io={"format_message": None})

# ------------------------------- _standalone_proxy_kwargs
case("_standalone_proxy_kwargs", io={"resolve_proxy_url": "http://p:8080",
                                      "proxy_kwargs_for_aiohttp": [{"proxy": "http://p:8080"}, {"trust_env": False}]})
case("_standalone_proxy_kwargs", io={"resolve_proxy_url": None,
                                      "proxy_kwargs_for_aiohttp": [[], {}]})
case("_standalone_proxy_kwargs", io={"resolve_proxy_url": rais("RuntimeError", "res")}, expect_raise="RuntimeError")
case("_standalone_proxy_kwargs", io={"resolve_proxy_url": "x",
                                      "proxy_kwargs_for_aiohttp": rais("RuntimeError", "p")}, expect_raise="RuntimeError")
case("_standalone_proxy_kwargs", io={"resolve_proxy_url": "x", "proxy_kwargs_for_aiohttp": script(ret=[])})
case("_standalone_proxy_kwargs", io={"resolve_proxy_url": "x",
                                      "proxy_kwargs_for_aiohttp": [{"a": 1}, {"b": 2}]})
case("_standalone_proxy_kwargs", io={}, expect_raise="RuntimeError")  # fail-closed resolve_proxy_url

# ------------------------------- _media_message_type
case("_media_message_type", {"media_types": []})
case("_media_message_type", {"media_types": ["image/png"]})
case("_media_message_type", {"media_types": ["video/mp4"]})
case("_media_message_type", {"media_types": ["audio/ogg"]})
case("_media_message_type", {"media_types": ["application/pdf"]})
case("_media_message_type", {"media_types": ["text/plain", "image/jpeg"]})
case("_media_message_type", {"media_types": None})

# ------------------------------- _native_task_card_key
case("_native_task_card_key", {"chat_id": "C1", "reply_to": "111.1", "metadata": {"thread_id": "T.1"}},
     io={"resolve_thread_ts": "111.1", "metadata_team_id": "T1", "workspace_thread_key": "K1"})
case("_native_task_card_key", {"chat_id": "C1", "reply_to": "111.1", "metadata": {}},
     io={"resolve_thread_ts": None})
case("_native_task_card_key", {"chat_id": "C1", "reply_to": "111.1", "metadata": {}},
     io={"resolve_thread_ts": "111.1", "metadata_team_id": None, "workspace_thread_key": "K2"})
case("_native_task_card_key", {"chat_id": "C1", "reply_to": "111.1", "metadata": {}},
     io={"resolve_thread_ts": rais("RuntimeError", "r")}, expect_raise="RuntimeError")
case("_native_task_card_key", {"chat_id": "C1", "reply_to": "111.1", "metadata": {}},
     io={"resolve_thread_ts": "111.1", "metadata_team_id": rais("RuntimeError", "m")}, expect_raise="RuntimeError")
case("_native_task_card_key", {"chat_id": "C1", "reply_to": "111.1", "metadata": {}},
     io={"resolve_thread_ts": "111.1", "metadata_team_id": "T1",
         "workspace_thread_key": rais("RuntimeError", "w")}, expect_raise="RuntimeError")
case("_native_task_card_key", {"chat_id": "C2", "reply_to": 123, "metadata": None},
     io={"resolve_thread_ts": 123, "metadata_team_id": "T9", "workspace_thread_key": "K3"})

# ------------------------------- native_task_card_destination_supported
case("native_task_card_destination_supported", {"chat_id": "C1", "reply_to": "111.1", "metadata": {}},
     io={"native_task_card_key": "K1"})
case("native_task_card_destination_supported", {"chat_id": "C1", "reply_to": "111.1", "metadata": {}},
     io={"native_task_card_key": None})
case("native_task_card_destination_supported", {"chat_id": "C1", "reply_to": "111.1", "metadata": {}},
     io={"native_task_card_key": rais("RuntimeError", "k")}, expect_raise="RuntimeError")
case("native_task_card_destination_supported", {"chat_id": "C1", "reply_to": "111.1", "metadata": {}},
     io={"native_task_card_key": {}})
case("native_task_card_destination_supported", {"chat_id": "C1", "reply_to": "111.1", "metadata": {}},
     io={"native_task_card_key": False})
case("native_task_card_destination_supported", {"chat_id": "C1", "reply_to": "111.1", "metadata": {}},
     io={"native_task_card_key": 0})
case("native_task_card_destination_supported", {"chat_id": "C1", "reply_to": "111.1", "metadata": {}},
     io={"native_task_card_key": "K9", "resolve_thread_ts": "unused"})

# ------------------------------- _suppressed_ignored
case("_suppressed_ignored", {"chat_id": "C1", "what": "send"},
     io={"is_ignored_channel": True, "logger.log": None})
case("_suppressed_ignored", {"chat_id": "C2", "what": "react"},
     io={"is_ignored_channel": False})
case("_suppressed_ignored", {"chat_id": "C3", "what": "send"},
     io={"is_ignored_channel": rais("RuntimeError", "i")}, expect_raise="RuntimeError")
case("_suppressed_ignored", {"chat_id": "C4", "what": "send"},
     io={"is_ignored_channel": True, "logger.log": rais("RuntimeError", "log")}, expect_raise="RuntimeError")
case("_suppressed_ignored", {"chat_id": "C5", "what": "edit"},
     io={"is_ignored_channel": 1, "logger.log": None})
case("_suppressed_ignored", {"chat_id": "C6", "what": "send"},
     io={"is_ignored_channel": None})
case("_suppressed_ignored", {"chat_id": "C7", "what": "post"},
     io={"is_ignored_channel": True, "logger.log": None})

# ------------------------------- _outbound_blocked
case("_outbound_blocked", {"chat_id": "C1", "what": "send"},
     io={"suppressed_ignored": True})
case("_outbound_blocked", {"chat_id": "C2", "what": "send"},
     io={"suppressed_ignored": False})
case("_outbound_blocked", {"chat_id": "C3", "what": "send"},
     io={"suppressed_ignored": False}, state={"app": {}})
case("_outbound_blocked", {"chat_id": "C4", "what": "send"},
     io={"suppressed_ignored": False}, state={"app": {"x": 1}})
case("_outbound_blocked", {"chat_id": "C5", "what": "send"},
     io={"suppressed_ignored": rais("RuntimeError", "s")}, expect_raise="RuntimeError")
case("_outbound_blocked", {"chat_id": "C6", "what": "send"},
     io={"suppressed_ignored": False}, state={"app": None})
case("_outbound_blocked", {"chat_id": "C7", "what": "send"},
     io={"suppressed_ignored": False}, state={"app": "yes"})

# ------------------------------- supports_draft_streaming
case("supports_draft_streaming", state={"native_stream_unsupported": True})
case("supports_draft_streaming", state={"native_stream_unsupported": False, "extra": {"unfurl_links": True}},
     io={"slack_unfurl_kwargs": True})
case("supports_draft_streaming", state={"app": {"x": 1}}, io={"slack_unfurl_kwargs": False})
case("supports_draft_streaming", io={"slack_unfurl_kwargs": False})
case("supports_draft_streaming", state={"app": None}, io={"slack_unfurl_kwargs": False})
case("supports_draft_streaming", io={"slack_unfurl_kwargs": rais("RuntimeError", "u")}, expect_raise="RuntimeError")
case("supports_draft_streaming", state={"app": {}}, io={"slack_unfurl_kwargs": False})

# ------------------------------- _reacting_target
EV = client(message_id="111.1", source=client(scope_id="T1"))
case("_reacting_target", {"event": EV}, state={"_reacting_message_ids": ["m1"]},
     io={"reactions_enabled": True, "workspace_message_marker": "m1"})
case("_reacting_target", {"event": EV}, io={"reactions_enabled": False})
case("_reacting_target", {"event": EV}, state={"_reacting_message_ids": ["m1"]},
     io={"reactions_enabled": True, "workspace_message_marker": "mX"})
case("_reacting_target", {"event": EV}, io={"reactions_enabled": rais("RuntimeError", "r")}, expect_raise="RuntimeError")
case("_reacting_target", {"event": EV}, io={"reactions_enabled": True,
     "workspace_message_marker": rais("RuntimeError", "m")}, expect_raise="RuntimeError")
case("_reacting_target", {"event": client(source=client(scope_id="T1"))},
     io={"reactions_enabled": True})
case("_reacting_target", {"event": client(message_id="111.1", source=client())},
     state={"_reacting_message_ids": {"__set__": ["m1"]}},
     io={"reactions_enabled": True, "workspace_message_marker": "m1"})

# ------------------------------- _channel_prompt_with_identity
case("_channel_prompt_with_identity", {"channel_id": "C1", "team_id": "T1"},
     io={"resolve_channel_prompt": "Hello", "build_identity_prompt": ""})
case("_channel_prompt_with_identity", {"channel_id": "C1", "team_id": "T1"},
     io={"resolve_channel_prompt": "Hello", "build_identity_prompt": "I am bot"})
case("_channel_prompt_with_identity", {"channel_id": "C1", "team_id": "T1"},
     io={"resolve_channel_prompt": None, "build_identity_prompt": "I am bot"})
case("_channel_prompt_with_identity", {"channel_id": "C1", "team_id": "T1"},
     io={"resolve_channel_prompt": rais("RuntimeError", "c")}, expect_raise="RuntimeError")
case("_channel_prompt_with_identity", {"channel_id": "C1", "team_id": "T1"},
     io={"resolve_channel_prompt": "Hi", "build_identity_prompt": rais("RuntimeError", "i")}, expect_raise="RuntimeError")
case("_channel_prompt_with_identity", {"channel_id": "C1", "team_id": "T1"},
     io={"resolve_channel_prompt": "", "build_identity_prompt": ""})
case("_channel_prompt_with_identity", {"channel_id": "C1", "team_id": "T1"},
     io={"resolve_channel_prompt": "  Hi  ", "build_identity_prompt": "  X  "})

# ------------------------------- _slack_is_free_channel
case("_slack_is_free_channel", {"channel_id": "C1"},
     io={"slack_require_mention_channels": script(ret=["C1"])})
case("_slack_is_free_channel", {"channel_id": "C1"},
     io={"slack_require_mention_channels": script(ret=[]), "slack_free_response_channels": script(ret=["C1"])})
case("_slack_is_free_channel", {"channel_id": "C1"},
     io={"slack_require_mention_channels": script(ret=[]), "slack_free_response_channels": script(ret=[]),
         "slack_require_mention": False})
case("_slack_is_free_channel", {"channel_id": "C1"},
     io={"slack_require_mention_channels": script(ret=[]), "slack_free_response_channels": script(ret=[]),
         "slack_require_mention": True})
case("_slack_is_free_channel", {"channel_id": "C1"},
     io={"slack_require_mention_channels": rais("RuntimeError", "r")}, expect_raise="RuntimeError")
case("_slack_is_free_channel", {"channel_id": "C1"},
     io={"slack_require_mention_channels": script(ret=[]), "slack_free_response_channels": rais("RuntimeError", "f")}, expect_raise="RuntimeError")
case("_slack_is_free_channel", {"channel_id": "C1"},
     io={"slack_require_mention_channels": script(ret=[]), "slack_free_response_channels": script(ret=[]),
         "slack_require_mention": rais("RuntimeError", "q")}, expect_raise="RuntimeError")
case("_slack_is_free_channel", {"channel_id": "C1"},
     io={"slack_require_mention_channels": {"__set__": ["C1"]}})

# ------------------------------- _slack_reply_expected
def reply(text="hi", uid="U1", **over):
    d = {"routing_text": text, "bot_uid": uid, "channel_id": "C1",
         "addressed": False, "opens_own_session": False, "channel_thread_followup": False}
    d.update(over)
    return d

case("_slack_reply_expected", reply(addressed=True))
case("_slack_reply_expected", reply(),
     io={"slack_message_mentions_self": True})
case("_slack_reply_expected", reply(),
     io={"slack_message_mentions_self": False, "slack_message_addressed_to_other_user": True})
case("_slack_reply_expected", reply(channel_thread_followup=True),
     io={"slack_message_mentions_self": False, "slack_message_addressed_to_other_user": False,
         "slack_thread_followup_silence": True})
case("_slack_reply_expected", reply(opens_own_session=True),
     io={"slack_message_mentions_self": False, "slack_message_addressed_to_other_user": False,
         "slack_is_free_channel": True})
case("_slack_reply_expected", reply(opens_own_session=True),
     io={"slack_message_mentions_self": False, "slack_message_addressed_to_other_user": False,
         "slack_is_free_channel": False})
case("_slack_reply_expected", reply(),
     io={"slack_message_mentions_self": False, "slack_message_addressed_to_other_user": False})
case("_slack_reply_expected", reply(),
     io={"slack_message_mentions_self": rais("RuntimeError", "m")}, expect_raise="RuntimeError")

# ------------------------------- _slack_message_matches_mention_patterns
case("_slack_message_matches_mention_patterns", {"text": "hello world"},
     io={"slack_mention_patterns": script(ret=["world"])})
case("_slack_message_matches_mention_patterns", {"text": "hello"},
     io={"slack_mention_patterns": script(ret=["xyz"])})
case("_slack_message_matches_mention_patterns", {"text": ""})
case("_slack_message_matches_mention_patterns", {"text": "abc"},
     io={"slack_mention_patterns": script(ret=[])})
case("_slack_message_matches_mention_patterns", {"text": "abc"},
     io={"slack_mention_patterns": rais("RuntimeError", "p")}, expect_raise="RuntimeError")
case("_slack_message_matches_mention_patterns", {"text": "AB"},
     io={"slack_mention_patterns": script(ret=["A", "B"])})
case("_slack_message_matches_mention_patterns", {"text": "abc"},
     io={"slack_mention_patterns": script(ret=["z", "b"])})

# ------------------------------- _append_block_text
case("_append_block_text", {"text": "orig", "blocks": [{"type": "section"}], "bot_uid": "U1"},
     io={"extract_additional_text_from_slack_blocks": "more",
         "logger.debug": None, "serialize_slack_blocks_for_agent": "[payload]"})
case("_append_block_text", {"text": "orig", "blocks": [], "bot_uid": "U1"},
     io={"extract_additional_text_from_slack_blocks": "", "serialize_slack_blocks_for_agent": ""})
case("_append_block_text", {"text": "orig", "blocks": [], "bot_uid": "U1"},
     io={"extract_additional_text_from_slack_blocks": rais("RuntimeError", "e")}, expect_raise="RuntimeError")
case("_append_block_text", {"text": "orig", "blocks": []},
     io={"extract_additional_text_from_slack_blocks": "", "serialize_slack_blocks_for_agent": rais("RuntimeError", "s")}, expect_raise="RuntimeError")
case("_append_block_text", {"text": "", "blocks": []},
     io={"extract_additional_text_from_slack_blocks": "x", "logger.debug": None,
         "serialize_slack_blocks_for_agent": ""})
case("_append_block_text", {"text": "orig", "blocks": []},
     io={"extract_additional_text_from_slack_blocks": None, "serialize_slack_blocks_for_agent": "p"})
case("_append_block_text", {"text": "orig", "blocks": []},
     io={"extract_additional_text_from_slack_blocks": " spaced ", "logger.debug": None,
         "serialize_slack_blocks_for_agent": ""})

# ------------------------------- _thread_rehydration_key
case("_thread_rehydration_key", {"channel_id": "C1", "thread_ts": "111.1", "user_id": "U1", "team_id": "T1"})
case("_thread_rehydration_key", {"channel_id": "C1", "thread_ts": "111.1", "user_id": "U1", "team_id": "T1"},
     state={"_session_store": store(True)})
case("_thread_rehydration_key", {"channel_id": "C1", "thread_ts": "111.1", "user_id": "U1", "team_id": "T1"},
     state={"_session_store": store(False)})
case("_thread_rehydration_key", {"channel_id": "C1", "thread_ts": "111.1", "user_id": "U1", "team_id": ""})
case("_thread_rehydration_key", {"channel_id": "C1", "thread_ts": "111.1", "user_id": "U1", "team_id": "T1"},
     state={"_session_store": client()})
case("_thread_rehydration_key", {"channel_id": "C1", "thread_ts": "111.1", "user_id": "U1", "team_id": "T1"},
     state={"_session_store": store("yes")})
case("_thread_rehydration_key", {"channel_id": "C2", "thread_ts": "222.2", "user_id": "U2", "team_id": "T2"},
     state={"_session_store": store(True)})

# ------------------------------- _thread_session_source
case("_thread_session_source", {"channel_id": "C1", "thread_ts": "111.1", "user_id": "U1", "team_id": "T1", "chat_type": "channel"},
     io={"build_source": {"chat_id": "C1", "chat_type": "channel"}})
case("_thread_session_source", {"channel_id": "C1", "thread_ts": "111.1", "user_id": "U1", "team_id": "", "chat_type": "channel"},
     io={"build_source": {}})
case("_thread_session_source", {"channel_id": "C1", "thread_ts": "111.1", "user_id": "U1", "team_id": None, "chat_type": "im"},
     io={"build_source": {"scope_id": None}})
case("_thread_session_source", {"channel_id": "C1", "thread_ts": "111.1", "user_id": "U1", "team_id": "T1", "chat_type": "channel"},
     io={"build_source": rais("RuntimeError", "b")}, expect_raise="RuntimeError")
case("_thread_session_source", {"channel_id": "C2", "thread_ts": "222.2", "user_id": "U2", "team_id": "T2", "chat_type": "group"},
     io={"build_source": {"ok": True}})
case("_thread_session_source", {"channel_id": "C3", "thread_ts": "333.3", "user_id": "U3", "team_id": "T3", "chat_type": "channel"},
     io={"build_source": "SRC"})
case("_thread_session_source", {"channel_id": "C1", "thread_ts": "111.1", "user_id": "U1", "team_id": "T1", "chat_type": "channel"},
     io={}, expect_raise="RuntimeError")  # fail-closed build_source


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    keys = set()
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
        keys |= set((c["in"].get("io") or {}).keys())
    print(f"wrote {len(CASES)} cases to {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")
    print(f"seam keys covered ({len(keys)}): {sorted(keys)}")


if __name__ == "__main__":
    main()
