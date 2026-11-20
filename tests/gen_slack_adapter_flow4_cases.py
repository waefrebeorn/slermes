#!/usr/bin/env python3
"""Generate tests/fixtures/slack_adapter_flow4.jsonl for the Slack
plugin-adapter FLOW port, wave 4 (src/gateway/platforms/slack_adapter_flow4.c).

Deterministic corpus: >=5 cases per ported function (9 UNCLAIMED funcs),
covering pure-leaf edges, chained dict.get defaults, exception families,
seamed decision chains (scripted results + ordered traces), state echoes,
fail-closed missing scripts and BaseException passthrough through the one
except-Exception site. Case shape:
{"func", "in": {"args","io"?,"state"}, "expect_raise"?}.

Representation markers (shared with the C port + oracle):
- {"__obj__": {attrs}} -- attribute carrier (SimpleNamespace double)
- {"__set__": [members]} -- set carrier (C keeps members sorted)
- {"__exc__": {type,message,...attrs}} -- exception instance double; the
  "exc_message_attr" key becomes the live `exc.message` attribute.
- io values: a plain JSON value (every call), a LIST consumed in call order,
  or {"__script__":true,"ret":v} / {"__script__":true,"raise":{...}}.

Domain guards (ASCII scope, shared with the port header):
- str() positions carry str/int/bool/None only (no containers, no non-integral
  floats). status_code fixtures are integral numbers. exc kinds are non-
  KeyError so str(exc) == message. On the body-join path exc.response is
  absent/null (else str(SimpleNamespace) is untranscribable). SDK probe
  scripts and clip/classifier scripts are JSON bools.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "slack_adapter_flow4.jsonl")
CASES = []

_UNSET = object()


def case(func, args=None, io=None, state=None, expect_raise=None, install_set_arg=_UNSET):
    args = dict(args or {})
    if install_set_arg is not _UNSET:
        args["install_set"] = install_set_arg
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


def exc(kind, message, **attrs):
    d = {"type": kind, "message": message}
    d.update(attrs)
    return {"__exc__": d}


def obj(**attrs):
    return {"__obj__": attrs}


# ------------------------------------------------ _sdk_supports_agent_sessions
case("_sdk_supports_agent_sessions", state={"_AGENT_SESSIONS_SUPPORTED": None},
     io={"sdk_agent_sessions_probe": True})
case("_sdk_supports_agent_sessions", state={"_AGENT_SESSIONS_SUPPORTED": None},
     io={"sdk_agent_sessions_probe": False})
case("_sdk_supports_agent_sessions", state={"_AGENT_SESSIONS_SUPPORTED": None},
     io={"sdk_agent_sessions_probe": None})
case("_sdk_supports_agent_sessions", state={"_AGENT_SESSIONS_SUPPORTED": None},
     io={"sdk_agent_sessions_probe": script(raise_spec={"type": "RuntimeError", "message": "boom"})})
case("_sdk_supports_agent_sessions", state={"_AGENT_SESSIONS_SUPPORTED": None},
     io={"sdk_agent_sessions_probe": script(raise_spec={"type": "ValueError", "message": "bad"})})
case("_sdk_supports_agent_sessions", state={"_AGENT_SESSIONS_SUPPORTED": None},
     io={"sdk_agent_sessions_probe": script(raise_spec={"type": "SystemExit", "message": "bye"})},
     expect_raise="SystemExit")
case("_sdk_supports_agent_sessions", state={"_AGENT_SESSIONS_SUPPORTED": None},
     io={"sdk_agent_sessions_probe": script(raise_spec={"type": "CancelledError", "message": "cx"})},
     expect_raise="CancelledError")
case("_sdk_supports_agent_sessions", state={"_AGENT_SESSIONS_SUPPORTED": True})
case("_sdk_supports_agent_sessions", state={"_AGENT_SESSIONS_SUPPORTED": False})
case("_sdk_supports_agent_sessions", state={"_AGENT_SESSIONS_SUPPORTED": None},
     io={"sdk_agent_sessions_probe": script(raise_spec={"type": "TypeError", "message": "t"})})

# ------------------------------------------------ _interaction_fields
case("_interaction_fields", {"body": {"message": {"ts": "111.1"}, "channel": {"id": "C1"},
                                      "user": {"name": "alice", "id": "U1"}},
                             "action": {"action_id": "approve", "value": "yes"}})
case("_interaction_fields", {"body": {}, "action": {}})
case("_interaction_fields", {"body": {"user": {"id": "U2"}}, "action": {"action_id": "click"}})
case("_interaction_fields", {"body": {"channel": {"id": 7}}, "action": {"value": 5}})
case("_interaction_fields", {"body": 5, "action": {}}, expect_raise="AttributeError")
case("_interaction_fields", {"body": {}, "action": None}, expect_raise="AttributeError")
case("_interaction_fields", {"body": {"channel": 3}, "action": {}}, expect_raise="AttributeError")
case("_interaction_fields", {"body": {"message": "hi"}, "action": {}}, expect_raise="AttributeError")
case("_interaction_fields", {"body": {"message": None}, "action": {}}, expect_raise="AttributeError")
case("_interaction_fields", {"body": {"user": "bob"}, "action": {}}, expect_raise="AttributeError")

# ------------------------------------------------ _section_text
case("_section_text", {"message": {"blocks": []}})
case("_section_text", {"message": {"blocks": [{"type": "section", "text": {"text": "hello"}}]}})
case("_section_text", {"message": {"blocks": [{"type": "divider"},
                                              {"type": "section", "text": {"text": "second"}},
                                              {"type": "section", "text": {"text": "third"}}]}})
case("_section_text", {"message": {"blocks": [{"type": "section", "text": {"text": "abcdef"}}]},
                       "limit": 3})
case("_section_text", {"message": {"blocks": [{"type": "section", "text": {"text": "abcdef"}}]},
                       "limit": None})
case("_section_text", {"message": {"blocks": [{"type": "section", "text": {"text": "abcdef"}}]},
                       "limit": 0})
case("_section_text", {"message": {"blocks": [{"type": "section"}]}})
case("_section_text", {"message": {"blocks": [{"type": "section", "text": None}]}})
case("_section_text", {"message": {"blocks": ["oops"]}}, expect_raise="AttributeError")
case("_section_text", {"message": {"blocks": [{"type": "section", "text": "notadict"}]}},
     expect_raise="AttributeError")

# ------------------------------------------------ _parse_users_info
case("_parse_users_info", {"payload": {"user": {"is_bot": False,
                                                "profile": {"display_name": "Alice", "real_name": "A"}}},
                           "user_id": "U1"})
case("_parse_users_info", {"payload": {"user": {"profile": {"real_name": "Real"}}}, "user_id": "U2"})
case("_parse_users_info", {"payload": {"user": {"real_name": "UserReal"}}, "user_id": "U3"})
case("_parse_users_info", {"payload": {"user": {"name": "uname"}}, "user_id": "U4"})
case("_parse_users_info", {"payload": {"user": {}}, "user_id": "U5"})
case("_parse_users_info", {"payload": {"user": {"is_bot": True, "profile": {}}}, "user_id": "U6"})
case("_parse_users_info", {"payload": {"user": {"is_workflow_bot": True, "profile": {}}}, "user_id": "U7"})
case("_parse_users_info", {"payload": {"user": {"profile": {"bot_id": "B1"}}}, "user_id": "U8"})
case("_parse_users_info", {"payload": {"user": None}, "user_id": "U9"}, expect_raise="AttributeError")
case("_parse_users_info", {"payload": {"user": {"profile": "bad"}}, "user_id": "U10"},
     expect_raise="AttributeError")
case("_parse_users_info", {"payload": {"user": {"profile": None}}, "user_id": "U11"},
     expect_raise="AttributeError")

# ------------------------------------------------ _slack_file_kind
case("_slack_file_kind", {"f": {"id": "F1"}, "mimetype": "image/png"})
case("_slack_file_kind", {"f": {"id": "F2"}, "mimetype": "audio/ogg"})
case("_slack_file_kind", {"f": {"id": "F3"}, "mimetype": "video/mp4"},
     io={"is_slack_voice_clip": True})
case("_slack_file_kind", {"f": {"id": "F4"}, "mimetype": "video/quicktime"},
     io={"is_slack_voice_clip": False})
case("_slack_file_kind", {"f": {"id": "F5"}, "mimetype": "application/pdf"})
case("_slack_file_kind", {"f": {}, "mimetype": "text/plain"})
case("_slack_file_kind", {"f": {}, "mimetype": "image/jpeg"})
case("_slack_file_kind", {"f": {}, "mimetype": None}, expect_raise="AttributeError")
case("_slack_file_kind", {"f": {}, "mimetype": "video/mp4"},
     io={"is_slack_voice_clip": script(raise_spec={"type": "RuntimeError", "message": "clip fail"})},
     expect_raise="RuntimeError")

# ------------------------------------------------ _cron_continuable_surface
case("_cron_continuable_surface", state={"extra": {"cron_continuable_surface": "in_channel"}})
case("_cron_continuable_surface", state={"extra": {"cron_continuable_surface": " IN_CHANNEL "}})
case("_cron_continuable_surface", state={"extra": {"cron_continuable_surface": "thread"}})
case("_cron_continuable_surface", state={"extra": {"cron_continuable_surface": None}})
case("_cron_continuable_surface", state={"extra": {"cron_continuable_surface": 5}})
case("_cron_continuable_surface", state={"extra": {"cron_continuable_surface": True}})
case("_cron_continuable_surface", state={"extra": {}})
case("_cron_continuable_surface", state={"extra": {"cron_continuable_surface": "in_channel extra"}})

# ------------------------------------------------ _slack_allow_bots
case("_slack_allow_bots", state={"extra": {"allow_bots": "all"}},
     io={"_extra_or_secret": "all"})
case("_slack_allow_bots", state={"extra": {"allow_bots": "MENTIONS"}},
     io={"_extra_or_secret": "MENTIONS"})
case("_slack_allow_bots", state={"extra": {"allow_bots": "none"}},
     io={"_extra_or_secret": "none"})
case("_slack_allow_bots", state={"extra": {"allow_bots": "bogus"}},
     io={"_extra_or_secret": "bogus", "logger.warning": None})
case("_slack_allow_bots", state={"extra": {"allow_bots": 5}},
     io={"_extra_or_secret": 5, "logger.warning": None})
case("_slack_allow_bots", state={"extra": {"allow_bots": "x"}},
     io={"_extra_or_secret": script(raise_spec={"type": "RuntimeError", "message": "secret"})},
     expect_raise="RuntimeError")
case("_slack_allow_bots", state={"extra": {"allow_bots": "?"}},
     io={"_extra_or_secret": "?",
         "logger.warning": script(raise_spec={"type": "RuntimeError", "message": "log boom"})},
     expect_raise="RuntimeError")
case("_slack_allow_bots", state={"extra": {"allow_bots": " ALL "}},
     io={"_extra_or_secret": " ALL "})

# ------------------------------------------------ _reactions_enabled
case("_reactions_enabled", state={"extra": {"reactions": "true"}},
     io={"_extra_or_secret": "true"})
case("_reactions_enabled", state={"extra": {"reactions": "false"}},
     io={"_extra_or_secret": "false"})
case("_reactions_enabled", state={"extra": {"reactions": "0"}},
     io={"_extra_or_secret": "0"})
case("_reactions_enabled", state={"extra": {"reactions": "NO"}},
     io={"_extra_or_secret": "NO"})
case("_reactions_enabled", state={"extra": {"reactions": 5}},
     io={"_extra_or_secret": 5})
case("_reactions_enabled", state={"extra": {"reactions": None}},
     io={"_extra_or_secret": None})
case("_reactions_enabled", state={"extra": {"reactions": "x"}},
     io={"_extra_or_secret": script(raise_spec={"type": "RuntimeError", "message": "boom"})},
     expect_raise="RuntimeError")
case("_reactions_enabled", state={"extra": {"reactions": " FALSE "}},
     io={"_extra_or_secret": " FALSE "})

# ------------------------------------------------ _is_retryable_upload_error
case("_is_retryable_upload_error", {"exc": exc("RuntimeError", "x", response=obj(status_code=429))})
case("_is_retryable_upload_error", {"exc": exc("RuntimeError", "x", response=obj(status_code=503))})
case("_is_retryable_upload_error", {"exc": exc("RuntimeError", "x", response=obj(status_code=200))})
case("_is_retryable_upload_error", {"exc": exc("RuntimeError", "rate_limited")})
case("_is_retryable_upload_error", {"exc": exc("RuntimeError", "Connection Reset by peer")})
case("_is_retryable_upload_error", {"exc": exc("RuntimeError", "plain", exc_message_attr="SERVICE UNAVAILABLE")})
case("_is_retryable_upload_error", {"exc": exc("RuntimeError", "plain")},
     io={"_is_retryable_error": True})
case("_is_retryable_upload_error", {"exc": exc("RuntimeError", "plain")},
     io={"_is_retryable_error": False})
case("_is_retryable_upload_error", {"exc": exc("RuntimeError", "plain")},
     io={"_is_retryable_error": script(raise_spec={"type": "RuntimeError", "message": "classify"})},
     expect_raise="RuntimeError")
case("_is_retryable_upload_error", {"exc": exc("RuntimeError", "oops", response=None)},
     io={"_is_retryable_error": False})


# ======================== block-building / event-field-extraction (16) =====

# ------------------------------------------------ _button
case("_button", {"text": "Approve", "action_id": "a1", "value": "yes",
                 "style": "primary", "emoji": True})
case("_button", {"text": "Cancel", "action_id": "a2", "value": "no"})
case("_button", {"text": "", "action_id": "b", "value": ""})
case("_button", {"text": "X", "action_id": "c", "value": "v",
                 "style": "", "emoji": False})
case("_button", {"text": "Danger", "action_id": "d", "value": "1",
                 "style": "danger", "emoji": True})
case("_button", {"text": "Z", "action_id": "e", "value": "0"})

# ------------------------------------------------ _markdown_block_payload
case("_markdown_block_payload", {"content": "hello"})
case("_markdown_block_payload", {"content": "  hi  "})
case("_markdown_block_payload", {"content": "   "})
case("_markdown_block_payload", {"content": ""})
case("_markdown_block_payload", {"content": None})
case("_markdown_block_payload", {"content": "x" * 11500})
case("_markdown_block_payload", {"content": "x" * 11501})

# ------------------------------------------------ _feedback_block
case("_feedback_block", io={"t": ["Good", "Good access", "Bad", "Bad access"]})
case("_feedback_block", io={"t": "X"})
case("_feedback_block", io={"t": ["", "", "", ""]})
case("_feedback_block", io={"t": ["A" * 90, "B", "C", "D"]})
case("_feedback_block", io={"t": [script(raise_spec={"type": "RuntimeError", "message": "boom"})]},
     expect_raise="RuntimeError")
case("_feedback_block", io={"t": ["Good", "Good access",
                                 script(raise_spec={"type": "RuntimeError", "message": "3rd"})]},
     expect_raise="RuntimeError")
case("_feedback_block", io={"t": [script(raise_spec={"type": "SystemExit", "message": "bye"})]},
     expect_raise="SystemExit")
case("_feedback_block", io={}, expect_raise="RuntimeError")

# ------------------------------------------------ _append_feedback_block
case("_append_feedback_block", {"blocks": [{"type": "section"}]},
     state={"extra": {"feedback_buttons": True}},
     io={"t": ["Good", "Good access", "Bad", "Bad access"]})
case("_append_feedback_block", {"blocks": [{"type": "section"}]},
     state={"extra": {"feedback_buttons": False}})
case("_append_feedback_block", {"blocks": []},
     state={"extra": {"feedback_buttons": True}})
case("_append_feedback_block", {"blocks": None},
     state={"extra": {"feedback_buttons": True}})
case("_append_feedback_block", {"blocks": [{"type": "divider"}] * 51},
     state={"extra": {"feedback_buttons": True}})
case("_append_feedback_block", {"blocks": [{"type": "divider"}]},
     state={"extra": {"feedback_buttons": "yes"}},
     io={"t": "Z"})
case("_append_feedback_block", {"blocks": [{"type": "divider"}]},
     state={"extra": {"feedback_buttons": True}},
     io={"t": [script(raise_spec={"type": "RuntimeError", "message": "log"})]},
     expect_raise="RuntimeError")

# ------------------------------------------------ _build_identity_prompt
case("_build_identity_prompt", {"team_id": "T1"},
     state={"_team_bot_names": {"T1": "MyBot"}, "_bot_display_name": "Fallback"})
case("_build_identity_prompt", {"team_id": "T1"},
     state={"_team_bot_names": {}, "_bot_display_name": "Fallback"})
case("_build_identity_prompt", {"team_id": ""},
     state={"_team_bot_names": {"T1": "MyBot"}, "_bot_display_name": "Fallback"})
case("_build_identity_prompt", {"team_id": "T1"},
     state={"_team_bot_names": {}, "_bot_display_name": ""})
case("_build_identity_prompt", {"team_id": "  T1  "},
     state={"_team_bot_names": {"T1": "MyBot"}, "_bot_display_name": "Fallback"})
case("_build_identity_prompt", {"team_id": "T1"},
     state={"_team_bot_names": {"T1": "  Bot  "}, "_bot_display_name": ""})
case("_build_identity_prompt", {"team_id": "T1"})

# ------------------------------------------------ _event_team_id
case("_event_team_id", {"event": {"team": "T_EV"},
                        "body": {"authorizations": [{"team_id": "T_AUTH"}],
                                 "team_id": "T_ENV"}})
case("_event_team_id", {"event": {"team": "T_EV"},
                        "body": {"authorizations": [{"team_id": "T_AUTH"}],
                                 "team_id": "T_ENV"}},
     install_set_arg={"T_ENV": {}, "T_OTHER": {}})
case("_event_team_id", {"event": {"team": "T_EV"},
                        "body": {"authorizations": [{"team_id": "T_AUTH"}]}},
     install_set_arg={"T_SOLO": {}})
case("_event_team_id", {"event": {"team": "T_EV"}, "body": {"team_id": "T_ENV"}},
     install_set_arg={"T_A": {}, "T_B": {}})
case("_event_team_id", {"event": {"team": "T_EV"}, "body": {"team_id": "T_ENV"}},
     install_set_arg={})
case("_event_team_id", {"event": {}, "body": {"team_id": {"id": "T_DICT"}}})
case("_event_team_id", {"event": {"team": {"id": "T_INNER"}}, "body": {}})
case("_event_team_id", {"event": {}, "body": {}})
case("_event_team_id", {"event": {"team": "T_EV"}, "body": {}},
     install_set_arg=None)

# ------------------------------------------------ _context_channel_id
case("_context_channel_id", {"context": {"channel_id": "C1"}})
case("_context_channel_id", {"context": {"channel_id": 7}})
case("_context_channel_id", {"context": {"entities": [{"value": {"channel_id": "C2"}}]}})
case("_context_channel_id", {"context": {"entities": [{"type": "x_channel_id", "value": "C3"}]}})
case("_context_channel_id", {"context": {"entities": [{"type": "other", "value": "C4"},
                                                     {"value": {"channel_id": "C5"}}]}})
case("_context_channel_id", {"context": {}})
case("_context_channel_id", {"context": None})
case("_context_channel_id", {"context": {"entities": [3, {"value": {"channel_id": "C6"}}]}})

# ------------------------------------------------ _first_file_share
case("_first_file_share", {"file_obj": {"shares": {"public": {"C1": [{"ts": "1"}]}}},
                           "channel_id": "C1"})
case("_first_file_share", {"file_obj": {"shares": {"public": {"C2": [{"ts": "2"}]},
                                                   "private": {"C1": [{"ts": "3"}]}}},
                           "channel_id": "C1"})
case("_first_file_share", {"file_obj": {"shares": {"public": {"X": [{"ts": "9"}]}}},
                           "channel_id": "C1"})
case("_first_file_share", {"file_obj": {"shares": {}}, "channel_id": "C1"})
case("_first_file_share", {"file_obj": {"shares": {"public": {"C1": [5]}}},
                           "channel_id": "C1"})
case("_first_file_share", {"file_obj": {"shares": {"public": {"X": [{"ts": "8"}]}}}})
case("_first_file_share", {"file_obj": {"shares": {"public": 5, "private": {"Y": [{"ts": "7"}]}}},
                           "channel_id": "C1"})

# ------------------------------------------------ _is_foreign_author
case("_is_foreign_author", {"author_team": "T1", "resolved_team": "T2"})
case("_is_foreign_author", {"author_team": "T1", "resolved_team": "T2"},
     state={"_team_clients": {"T1": {}}})
case("_is_foreign_author", {"author_team": "T1", "resolved_team": "T1"},
     state={"_team_clients": {"T1": {}}})
case("_is_foreign_author", {"author_team": "", "resolved_team": "T2"})
case("_is_foreign_author", {"author_team": "T1", "resolved_team": ""})
case("_is_foreign_author", {"author_team": "T1", "resolved_team": "T2"},
     state={"_team_clients": {}})

# ------------------------------------------------ _agent_view_event_fields
case("_agent_view_event_fields", {"event": {"context": {"channel_id": "C1"}, "user": "U1"},
                                  "body": {"team_id": "T1"}})
case("_agent_view_event_fields", {"event": {"app_context": {"channel_id": "C2"}, "user_id": "U2"},
                                  "body": {"team_id": "T2"}})
case("_agent_view_event_fields", {"event": {"context": {"channel_id": "C3"}, "user": "U3",
                                            "team_id": "T3"}, "body": {}})
case("_agent_view_event_fields", {"event": {}, "body": {}})
case("_agent_view_event_fields", {"event": {"context": {"channel_id": "C4"}, "user": "U4"},
                                  "body": {"team_id": "TX"}},
     state={"_team_clients": {"T5": {}}})
case("_agent_view_event_fields", {"event": {"context": {"entities": [{"value": {"channel_id": "C6"}}]},
                                            "user": "U6"}, "body": {"team_id": "T6"}})

# ------------------------------------------------ _extract_assistant_thread_metadata
case("_extract_assistant_thread_metadata",
     {"event": {"assistant_thread": {"channel_id": "C1", "thread_ts": "111.1",
                                     "user_id": "U1", "team_id": "T1",
                                     "context": {"channel_id": "CX"}}},
      "body": {"team_id": "T1"}})
case("_extract_assistant_thread_metadata",
     {"event": {"assistant_thread": {}, "channel": "C2", "thread_ts": "222.2",
                "user": "U2", "context": {"channel_id": "CY"}},
      "body": {"team_id": "T2"}})
case("_extract_assistant_thread_metadata",
     {"event": {"assistant_thread": {}, "message_ts": "333.3", "user": "U3",
                "app_context": {"channel_id": "CZ"}}, "body": {}})
case("_extract_assistant_thread_metadata", {"event": {}, "body": {}})
case("_extract_assistant_thread_metadata",
     {"event": {"assistant_thread": {"channel_id": "C4", "team_id": "T4"}}, "body": {}},
     state={"_team_clients": {"T4": {}}})
case("_extract_assistant_thread_metadata",
     {"event": {"assistant_thread": {"context": {"channel_id": "C5"}, "user_id": "U5"}},
      "body": {"authorizations": [{"team_id": "T5"}]}})

# ------------------------------------------------ _event_declares_bot_sender
case("_event_declares_bot_sender", {"event": {"bot_id": "B1"}})
case("_event_declares_bot_sender", {"event": {"subtype": "bot_message"}})
case("_event_declares_bot_sender", {"event": {"user_profile": {"is_bot": True}}})
case("_event_declares_bot_sender", {"event": {"app_id": "A1", "user": "U1"}},
     io={"slack_api_human_users": [["U1"]]})
case("_event_declares_bot_sender", {"event": {"app_id": "A1", "user": "U2"}},
     io={"slack_api_human_users": [["U1"]]})
case("_event_declares_bot_sender", {"event": {"app_id": "A1", "user": "U1",
                                              "client_msg_id": "cm1"}})
case("_event_declares_bot_sender", {"event": {"text": "hi", "user": "U3"}})
case("_event_declares_bot_sender", {"event": {"app_id": "A1", "user": "U1"}},
     io={"slack_api_human_users": script(raise_spec={"type": "SystemExit", "message": "bye"})},
     expect_raise="SystemExit")
case("_event_declares_bot_sender", {"event": {"app_id": "A1", "user": "U1"}},
     io={}, expect_raise="RuntimeError")

# ------------------------------------------------ _resolve_thread_ts
case("_resolve_thread_ts", {"reply_to": "123.4", "metadata": {"thread_id": "T.1"}},
     state={"extra": {}})
case("_resolve_thread_ts", {"reply_to": "123.4", "metadata": {"thread_ts": "T.2"}},
     state={"extra": {}})
case("_resolve_thread_ts", {"reply_to": "123.4", "metadata": {}}, state={"extra": {}})
case("_resolve_thread_ts", {"reply_to": "123.4", "metadata": {"thread_id": "123.4"}},
     state={"extra": {"reply_in_thread": False}})
case("_resolve_thread_ts", {"reply_to": "123.4", "metadata": {"thread_id": "T.3"}},
     state={"extra": {"reply_in_thread": False}})
case("_resolve_thread_ts", {"reply_to": "123.4"},
     state={"extra": {"reply_in_thread": False}})
case("_resolve_thread_ts", {"reply_to": "123.4", "metadata": {"thread_id": "123.4"}},
     state={"extra": {"reply_in_thread": None}})

# ------------------------------------------------ _session_thread_ts
case("_session_thread_ts", {"event": {"thread_ts": "999.1"}, "ts": "999.1", "is_dm": True,
                            "assistant_meta": {}}, state={"extra": {}})
case("_session_thread_ts", {"event": {}, "ts": "999.2", "is_dm": True,
                            "assistant_meta": {"thread_ts": "888.2"}}, state={"extra": {}})
case("_session_thread_ts", {"event": {}, "ts": "999.3", "is_dm": True,
                            "assistant_meta": {}}, state={"extra": {}})
case("_session_thread_ts", {"event": {}, "ts": "999.4", "is_dm": True,
                            "assistant_meta": {}},
     state={"extra": {"dm_top_level_threads_as_sessions": False}})
case("_session_thread_ts", {"event": {"_hermes_no_thread_response": True, "thread_ts": "777.5"},
                            "ts": "777.5", "is_dm": False, "assistant_meta": {}},
     state={"extra": {}})
case("_session_thread_ts", {"event": {"thread_ts": "666.6"}, "ts": "555.5", "is_dm": False,
                            "assistant_meta": {}}, state={"extra": {}})
case("_session_thread_ts", {"event": {"thread_ts": "555.5"}, "ts": "555.5", "is_dm": False,
                            "assistant_meta": {}}, state={"extra": {}})
case("_session_thread_ts", {"event": {"thread_ts": "555.5"}, "ts": "555.5", "is_dm": False,
                            "assistant_meta": {}}, state={"extra": {"reply_in_thread": False}})

# ------------------------------------------------ _normalize_changed_message
case("_normalize_changed_message", {"event": {"ts": "outer.1", "event_ts": "evt.1",
                                              "channel": "C1", "team": "T1",
                                              "message": {"ts": "orig.1", "text": "edited",
                                                          "edited": {"ts": "e.1"}}}})
case("_normalize_changed_message", {"event": {"ts": "orig.2",
                                              "message": {"ts": "orig.2", "text": "x"}}},
     state={"_processed_message_ts": {"__set__": ["orig.2"]}})
case("_normalize_changed_message", {"event": {"message": "notadict"}})
case("_normalize_changed_message", {"event": {"ts": "outer.3", "event_ts": "evt.3",
                                              "message": {"ts": "orig.3", "text": "y"}}})
case("_normalize_changed_message", {"event": {"ts": "orig.4",
                                              "message": {"ts": "orig.4",
                                                          "edited": {"ts": "edit.4"}}}})
case("_normalize_changed_message", {"event": {"ts": "orig.5", "channel": "C5", "team_id": "T5",
                                              "message": {"ts": "orig.5", "channel": "", "team": ""}}})
case("_normalize_changed_message", {"event": {"message": {"text": "no ts"}}})

# ------------------------------------------------ _assistant_thread_title_enabled
case("_assistant_thread_title_enabled", state={"extra": {}})
case("_assistant_thread_title_enabled", state={"extra": {"assistant_thread_titles": True}})
case("_assistant_thread_title_enabled", state={"extra": {"assistant_thread_titles": False}})
case("_assistant_thread_title_enabled", state={"extra": {"assistant_thread_titles": "false"}})
case("_assistant_thread_title_enabled", state={"extra": {"assistant_thread_titles": "YES"}})
case("_assistant_thread_title_enabled", state={"extra": {"assistant_thread_titles": 0}})
case("_assistant_thread_title_enabled", state={"extra": {"assistant_thread_titles": None}})
case("_assistant_thread_title_enabled", state={"extra": {"assistant_thread_titles": "off"}})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")


if __name__ == "__main__":
    main()
