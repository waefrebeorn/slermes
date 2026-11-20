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


def case(func, args=None, io=None, state=None, expect_raise=None):
    inp = {"args": args or {}}
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
