#!/usr/bin/env python3
"""Generate tests/fixtures/slack_adapter_flow3.jsonl for the Slack
plugin-adapter FLOW port, wave 3 (src/gateway/platforms/slack_adapter_flow3.c).

Deterministic corpus: >=5 cases per ported function (13 UNCLAIMED funcs),
covering pure-leaf edges, seamed decision chains (scripted results + ordered
traces), state echoes, fail-closed missing scripts and BaseException
passthrough through except-Exception sites. Case shape:
{"func", "in": {"args","io"?,"state"}, "expect_raise"?}.

Representation markers (shared with the C port + oracle):
- {"__obj__": {attrs}} -- attribute carrier (SimpleNamespace double)
- {"__set__": [members]} -- set carrier (C keeps members sorted)
- {"__exc__": {type,message,...attrs}} -- exception instance double; an attr
  shaped {"__callable__": name} becomes a recorder firing that seam with
  {"code": <first positional arg>} (response.get is the only such call).
- io values: a plain JSON value (every call), a LIST consumed in call order,
  or {"__script__":true,"ret":v} / {"__script__":true,"raise":{...}}.

Domain guards (ASCII scope, shared with the port header):
- str()-computed positions carry str/int/bool/None only (no containers, no
  non-integral floats). time.monotonic carries NON-INTEGRAL floats so int()
  truncation is unambiguous. Exception instances use non-KeyError kinds so
  str(exc) == message. _strip_stream_cursor glyphs are the four class consts.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "slack_adapter_flow3.jsonl")
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


CALLABLE_GET = {"__obj__": {"get": {"__callable__": "response.get"}}}
NO_GET = {"__obj__": {}}

# ---------------------------------------------------------------- _mrkdwn_insensitive
for text in [
    "plain text", "*bold* _em_ ~del~",
    "a  b\t\tc\n\nd", "***mix*** ~~x~~ __y__",
    "", "   ", "  *  leading  ",
]:
    case("_mrkdwn_insensitive", {"text": text})

# ---------------------------------------------------------------- _workspace_thread_key
case("_workspace_thread_key", {"team_id": "T1", "channel_id": "C1", "thread_ts": "123.4"})
case("_workspace_thread_key", {"team_id": None, "channel_id": "C2", "thread_ts": "555.1"})
case("_workspace_thread_key", {"team_id": "T3", "channel_id": "", "thread_ts": "1.0"})
case("_workspace_thread_key", {"team_id": "T4", "channel_id": "C4", "thread_ts": 0})
case("_workspace_thread_key", {"team_id": 7, "channel_id": 8, "thread_ts": 9})
case("_workspace_thread_key", {"team_id": True, "channel_id": False, "thread_ts": "9.9"})
case("_workspace_thread_key", {"team_id": "T7", "channel_id": "C7", "thread_ts": None})

# ---------------------------------------------------------------- _agent_view_context_key
case("_agent_view_context_key", {"team_id": "T1", "user_id": "U1"})
case("_agent_view_context_key", {"team_id": "", "user_id": "U2"})
case("_agent_view_context_key", {"team_id": "T3", "user_id": None})
case("_agent_view_context_key", {"team_id": 4, "user_id": 5})
case("_agent_view_context_key", {"team_id": True, "user_id": False})
case("_agent_view_context_key", {"team_id": "T6", "user_id": 0})
case("_agent_view_context_key", {"team_id": None, "user_id": "U7"})

# ---------------------------------------------------------------- _thread_cache_key
case("_thread_cache_key", {"channel_id": "C1", "thread_ts": "111.1", "team_id": "T1"})
case("_thread_cache_key", {"channel_id": "C2", "thread_ts": "222.2", "team_id": ""})
case("_thread_cache_key", {"channel_id": 3, "thread_ts": 4, "team_id": 5})
case("_thread_cache_key", {"channel_id": True, "thread_ts": False, "team_id": None})
case("_thread_cache_key", {"channel_id": "", "thread_ts": "5.5", "team_id": "T5"})
case("_thread_cache_key", {"channel_id": "C6", "thread_ts": 0, "team_id": "T6"})

# ---------------------------------------------------------------- _thread_root_message
case("_thread_root_message", {"messages": [{"ts": "1.0", "text": "a"},
                                           {"ts": "2.0", "text": "b"}], "thread_ts": "2.0"})
case("_thread_root_message", {"messages": [{"ts": "1.0"}, {"ts": "2.0"}], "thread_ts": "9.9"})
case("_thread_root_message", {"messages": [], "thread_ts": "1.0"})
case("_thread_root_message", {"messages": [{"text": "no-ts"}], "thread_ts": ""})
case("_thread_root_message", {"messages": [1, {"ts": "1.0"}], "thread_ts": "1.0"},
     expect_raise="AttributeError")
case("_thread_root_message", {"messages": [{"ts": 1.0}, {"ts": "1.0"}], "thread_ts": 1.0})
case("_thread_root_message", {"messages": [{"ts": "1.0"}], "thread_ts": None})

# ---------------------------------------------------------------- _default_status_text
case("_default_status_text", {"started": None},
     io={"time.monotonic": 100.5, "t": "thinking"})
case("_default_status_text", {},
     io={"time.monotonic": 10.25, "t": "thinking"})
case("_default_status_text", {"started": 70.2},
     io={"time.monotonic": 100.7, "t": "still"})
case("_default_status_text", {"started": 10.0},
     io={"time.monotonic": 125.9, "t": "still"})
case("_default_status_text", {"started": 0.0},
     io={"time.monotonic": 60.3, "t": "still"})
case("_default_status_text", {"started": 100.9},
     io={"time.monotonic": 100.1, "t": "thinking"})
case("_default_status_text", {"started": None},
     io={"time.monotonic": 100.5, "t": script(raise_spec={"type": "RuntimeError", "message": "boom"})},
     expect_raise="RuntimeError")
case("_default_status_text", {"started": None},
     io={"time.monotonic": 100.5, "t": script(raise_spec={"type": "SystemExit", "message": "bye"})},
     expect_raise="SystemExit")

# ---------------------------------------------------------------- _is_block_payload_rejection
case("_is_block_payload_rejection", {"exc": exc("SlackApiError", "boom", response=CALLABLE_GET)},
     io={"response.get": "invalid_blocks"})
case("_is_block_payload_rejection", {"exc": exc("SlackApiError", "boom", response=CALLABLE_GET)},
     io={"response.get": "other_error"})
case("_is_block_payload_rejection", {"exc": exc("SlackApiError", "boom", response=CALLABLE_GET)},
     io={"response.get": "msg_too_long"})
case("_is_block_payload_rejection", {"exc": exc("SlackApiError", "boom", response=CALLABLE_GET)},
     io={"response.get": script(raise_spec={"type": "RuntimeError", "message": "x"})})
case("_is_block_payload_rejection", {"exc": exc("SlackApiError", "boom", response=CALLABLE_GET)},
     io={"response.get": script(raise_spec={"type": "SystemExit", "message": "x"})},
     expect_raise="SystemExit")
case("_is_block_payload_rejection", {"exc": exc("SlackApiError", "has too_many_blocks inside", response=NO_GET)})
case("_is_block_payload_rejection", {"exc": exc("RuntimeError", "invalid_blocks at server")})
case("_is_block_payload_rejection", {"exc": exc("RuntimeError", "plain failure")})

# ---------------------------------------------------------------- _strip_stream_cursor
case("_strip_stream_cursor", {"text": "hello\u2589"})
case("_strip_stream_cursor", {"text": "hello\u258d"})
case("_strip_stream_cursor", {"text": "hello\u258c"})
case("_strip_stream_cursor", {"text": "hello\u2026"})
case("_strip_stream_cursor", {"text": "plain text"})
case("_strip_stream_cursor", {"text": "hello\u258c  "})
case("_strip_stream_cursor", {"text": "\u2589"})
case("_strip_stream_cursor", {"text": ""})

# ---------------------------------------------------------------- _extra_flag
case("_extra_flag", {"key": "a"}, state={"extra": {"a": "true"}})
case("_extra_flag", {"key": "a"}, state={"extra": {"a": "YES"}})
case("_extra_flag", {"key": "a"}, state={"extra": {"a": "on"}})
case("_extra_flag", {"key": "a"}, state={"extra": {"a": "false"}})
case("_extra_flag", {"key": "a"}, state={"extra": {"a": 5}})
case("_extra_flag", {"key": "a"}, state={"extra": {"a": True}})
case("_extra_flag", {"key": "a"}, state={"extra": {"a": None}, "default": True})
case("_extra_flag", {"key": "a", "default": True}, state={"extra": {}})
case("_extra_flag", {"key": "a", "default": False}, state={"extra": {"a": None}})

# ---------------------------------------------------------------- _slack_require_mention
case("_slack_require_mention", {}, state={"extra": {"require_mention": "false"}},
     io={"_get_scoped_secret": "true"})
case("_slack_require_mention", {}, state={"extra": {"require_mention": "0"}},
     io={"_get_scoped_secret": "true"})
case("_slack_require_mention", {}, state={"extra": {"require_mention": "NO"}},
     io={"_get_scoped_secret": "true"})
case("_slack_require_mention", {}, state={"extra": {"require_mention": "off"}},
     io={"_get_scoped_secret": "true"})
case("_slack_require_mention", {}, state={"extra": {"require_mention": "true"}},
     io={"_get_scoped_secret": "false"})
case("_slack_require_mention", {}, state={"extra": {"require_mention": False}},
     io={"_get_scoped_secret": "true"})
case("_slack_require_mention", {}, state={"extra": {"require_mention": None}},
     io={"_get_scoped_secret": "false"})
case("_slack_require_mention", {}, state={"extra": {"require_mention": " FALSE "}},
     io={"_get_scoped_secret": "true"})

# ---------------------------------------------------------------- _extra_or_env_flag
case("_extra_or_env_flag", {"key": "k", "env_var": "SLACK_K"},
     io={"_extra_or_secret": "true"}, state={"extra": {"k": "true"}})
case("_extra_or_env_flag", {"key": "k", "env_var": "SLACK_K"},
     io={"_extra_or_secret": " yes "}, state={"extra": {"k": "yes"}})
case("_extra_or_env_flag", {"key": "k", "env_var": "SLACK_K", "strip": True},
     io={"_extra_or_secret": " yes "}, state={"extra": {"k": "yes"}})
case("_extra_or_env_flag", {"key": "k", "env_var": "SLACK_K"},
     io={"_extra_or_secret": "off"}, state={"extra": {"k": "off"}})
case("_extra_or_env_flag", {"key": "k", "env_var": "SLACK_K"},
     io={"_extra_or_secret": False}, state={"extra": {"k": False}})
case("_extra_or_env_flag", {"key": "k", "env_var": "SLACK_K"},
     io={"_extra_or_secret": True}, state={"extra": {"k": True}})
case("_extra_or_env_flag", {"key": "k", "env_var": "SLACK_K"},
     io={"_extra_or_secret": 5}, state={"extra": {"k": 5}})
case("_extra_or_env_flag", {"key": "k", "env_var": "SLACK_K"},
     io={"_extra_or_secret": None}, state={"extra": {"k": None}})

# ---------------------------------------------------------------- _extra_or_env_channel_set
case("_extra_or_env_channel_set", {"key": "k", "env_var": "SLACK_K"},
     io={"_extra_or_secret": ["C1", " C2 ", "", "C3"]}, state={"extra": {"k": ["C1"]}})
case("_extra_or_env_channel_set", {"key": "k", "env_var": "SLACK_K"},
     io={"_extra_or_secret": "C1, C2,,C3"}, state={"extra": {"k": "x"}})
case("_extra_or_env_channel_set", {"key": "k", "env_var": "SLACK_K", "coerce_scalar": True},
     io={"_extra_or_secret": 123}, state={"extra": {"k": 123}})
case("_extra_or_env_channel_set", {"key": "k", "env_var": "SLACK_K"},
     io={"_extra_or_secret": None}, state={"extra": {"k": None}})
case("_extra_or_env_channel_set", {"key": "k", "env_var": "SLACK_K"},
     io={"_extra_or_secret": ""}, state={"extra": {"k": ""}})
case("_extra_or_env_channel_set", {"key": "k", "env_var": "SLACK_K"},
     io={"_extra_or_secret": [5, "C2"]}, state={"extra": {"k": [5]}})
case("_extra_or_env_channel_set", {"key": "k", "env_var": "SLACK_K", "coerce_scalar": True},
     io={"_extra_or_secret": None}, state={"extra": {"k": None}})
case("_extra_or_env_channel_set", {"key": "k", "env_var": "SLACK_K"},
     io={"_extra_or_secret": "  ,  ,"}, state={"extra": {"k": "  ,  ,"}})


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
