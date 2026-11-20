#!/usr/bin/env python3
"""Generate tests/fixtures/slack_adapter_flow6.jsonl for the Slack
plugin-adapter FLOW port, wave 6 (src/gateway/platforms/slack_adapter_flow6.c).

Deterministic corpus: >=6 cases per ported function (18 funcs), covering pure
logic, chained seams with ORDERED traces, state echoes (closure parts/extras),
fail-closed missing scripts and BaseException passthrough. Case shape:
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
OUT = os.path.join(HERE, "fixtures", "slack_adapter_flow6.jsonl")
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


def obj(**attrs):
    return {"__obj__": attrs}


# ------------------------------- _retry_after_from_exc
case("_retry_after_from_exc", {"e": obj(response=obj(headers={"Retry-After": "120"}))},
     io={"parse_retry_after_seconds": 120})
case("_retry_after_from_exc", {"e": obj(response=obj(headers={"Retry-After": "Wed, 21 Oct 2015 07:28:00 GMT"}))},
     io={"parse_retry_after_seconds": "Wed, 21 Oct 2015 07:28:00 GMT"})
case("_retry_after_from_exc", {"e": obj(response=obj(headers={}))},
     io={"parse_retry_after_seconds": None})
case("_retry_after_from_exc", {"e": obj()}, io={"parse_retry_after_seconds": 5})
case("_retry_after_from_exc", {"e": None}, io={"parse_retry_after_seconds": 0})
case("_retry_after_from_exc", {"e": obj(response=obj(headers={"x": "y"}))},
     io={"parse_retry_after_seconds": rais("ValueError", "bad retry-after")},
     expect_raise="ValueError")
case("_retry_after_from_exc", {"e": obj(response=obj(headers={}))}, io={},
     expect_raise="RuntimeError")

# ------------------------------- _standalone_post_text
for i, (cid, txt, tid, resp) in enumerate([
    ("C1", "hello", None, {"ok": True, "ts": "1"}),
    ("C2", "world", "111.2", {"ok": True}),
    ("D3", "dm", None, {"ok": False}),
]):
    case("_standalone_post_text",
         {"chat_id": cid, "text": txt, "unfurl_kwargs": {"unfurl_links": False}, "thread_id": tid},
         io={"standalone_post_kwargs": {"channel": cid, "text": txt},
             "chat_postMessage": resp,
             "slack_response_payload": {"success": bool(resp.get("ok"))}})
case("_standalone_post_text", {"chat_id": "C1", "text": "x", "unfurl_kwargs": {}, "thread_id": None},
     io={"standalone_post_kwargs": rais("RuntimeError", "kwargs boom")}, expect_raise="RuntimeError")
case("_standalone_post_text", {"chat_id": "C1", "text": "x", "unfurl_kwargs": {}, "thread_id": None},
     io={"standalone_post_kwargs": {"channel": "C1"},
         "chat_postMessage": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_standalone_post_text", {"chat_id": "C1", "text": "x", "unfurl_kwargs": {}, "thread_id": None},
     io={"standalone_post_kwargs": {"channel": "C1"}, "chat_postMessage": {"ok": True}},
     expect_raise="RuntimeError")  # slack_response_payload missing -> fail-closed

# ------------------------------- slack_deps_present
for i in range(6):
    case("slack_deps_present", {}, state={"extra": {"probe": i}})

# ------------------------------- _stream_key
case("_stream_key", {"chat_id": "C1", "metadata": {"thread_id": "T9"}},
     io={"metadata_team_id": "T9", "resolve_thread_ts": "111.2",
         "workspace_thread_key": script(["T9", "C1", "111.2"])})
case("_stream_key", {"chat_id": "C2", "metadata": None},
     io={"metadata_team_id": None, "scope_id_for_chat": "T1",
         "resolve_thread_ts": None, "workspace_thread_key": ["T1", "C2", ""]})
case("_stream_key", {"chat_id": "D3", "metadata": {}},
     io={"metadata_team_id": "", "scope_id_for_chat": None,
         "resolve_thread_ts": 0, "workspace_thread_key": ["", "D3", ""]})
case("_stream_key", {"chat_id": "C4", "metadata": {"a": 1}},
     io={"metadata_team_id": "T2", "resolve_thread_ts": [123],
         "workspace_thread_key": {"k": 1}})
case("_stream_key", {"chat_id": "C5", "metadata": {"a": 2}},
     io={"metadata_team_id": rais("SystemExit", "base passes"),
         "scope_id_for_chat": "T3"}, expect_raise="SystemExit")
case("_stream_key", {"chat_id": "C6", "metadata": {"a": 3}},
     io={"metadata_team_id": None, "scope_id_for_chat": "T4"},
     expect_raise="RuntimeError")  # resolve_thread_ts missing
case("_stream_key", {"chat_id": "C7", "metadata": {"a": 4}},
     io={"metadata_team_id": "T5", "resolve_thread_ts": "9",
         "workspace_thread_key": rais("ValueError", "bad key")}, expect_raise="ValueError")

# ------------------------------- _evict_oldest_by_ts
case("_evict_oldest_by_ts", {"entries": [1, 2, 3, 4], "cap": 2},
     io={"discard_oldest_by_thread_ts": None})
case("_evict_oldest_by_ts", {"entries": [1, 2, 3], "cap": 3},
     io={"discard_oldest_by_thread_ts": None})
case("_evict_oldest_by_ts", {"entries": ["a", "b", "c", "d", "e"], "cap": 3},
     io={"discard_oldest_by_thread_ts": "dropped"})
case("_evict_oldest_by_ts", {"entries": [], "cap": 0},
     io={"discard_oldest_by_thread_ts": None})
case("_evict_oldest_by_ts", {"entries": {"a": 1, "b": 2, "c": 3}, "cap": 1},
     io={"discard_oldest_by_thread_ts": None})
case("_evict_oldest_by_ts", {"entries": [1, 2, 3, 4], "cap": 2},
     io={"discard_oldest_by_thread_ts": rais("KeyboardInterrupt", "base")},
     expect_raise="KeyboardInterrupt")
case("_evict_oldest_by_ts", {"entries": [1, 2, 3, 4], "cap": 2}, io={},
     expect_raise="RuntimeError")  # fail-closed

# ------------------------------- _trim_bot_message_timestamps
case("_trim_bot_message_timestamps", {}, state={"_bot_message_ts": [1, 2, 3, 4], "_BOT_TS_MAX": 2},
     io={"evict_oldest_by_ts": None})
case("_trim_bot_message_timestamps", {}, state={"_bot_message_ts": [1, 2], "_BOT_TS_MAX": 4},
     io={"evict_oldest_by_ts": None})
case("_trim_bot_message_timestamps", {}, state={"_bot_message_ts": [], "_BOT_TS_MAX": 0},
     io={"evict_oldest_by_ts": None})
case("_trim_bot_message_timestamps", {}, state={"_bot_message_ts": list(range(7)), "_BOT_TS_MAX": 3},
     io={"evict_oldest_by_ts": "ok"})
case("_trim_bot_message_timestamps", {}, state={"_bot_message_ts": [1, 2, 3], "_BOT_TS_MAX": 1},
     io={"evict_oldest_by_ts": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_trim_bot_message_timestamps", {}, state={"_bot_message_ts": [1, 2, 3], "_BOT_TS_MAX": 1},
     io={}, expect_raise="RuntimeError")

# ------------------------------- _trim_mentioned_threads
case("_trim_mentioned_threads", {}, state={"_mentioned_threads": [1, 2, 3, 4], "_MENTIONED_THREADS_MAX": 2},
     io={"discard_oldest_by_thread_ts": None})
case("_trim_mentioned_threads", {}, state={"_mentioned_threads": [1, 2], "_MENTIONED_THREADS_MAX": 4},
     io={"discard_oldest_by_thread_ts": None})
case("_trim_mentioned_threads", {}, state={"_mentioned_threads": [], "_MENTIONED_THREADS_MAX": 0},
     io={"discard_oldest_by_thread_ts": None})
case("_trim_mentioned_threads", {}, state={"_mentioned_threads": list(range(9)), "_MENTIONED_THREADS_MAX": 4},
     io={"discard_oldest_by_thread_ts": "ok"})
case("_trim_mentioned_threads", {}, state={"_mentioned_threads": [1, 2, 3], "_MENTIONED_THREADS_MAX": 1},
     io={"discard_oldest_by_thread_ts": rais("GeneratorExit", "base")}, expect_raise="GeneratorExit")
case("_trim_mentioned_threads", {}, state={"_mentioned_threads": [1, 2, 3], "_MENTIONED_THREADS_MAX": 1},
     io={}, expect_raise="RuntimeError")

# ------------------------------- _EA_HEADER
for i, txt in enumerate(["Approval required", ":warning: already", "", "A: B", "x" * 12, "None"]):
    case("_EA_HEADER", {}, io={"t": txt})
case("_EA_HEADER", {}, io={"t": rais("RuntimeError", "translate fail")}, expect_raise="RuntimeError")

# ------------------------------- _EA_SMART_DENY_LINE
for i, txt in enumerate(["Denied: by other", "no colon here", "a:b:c", "", "label: ", "OnlyLabel"]):
    case("_EA_SMART_DENY_LINE", {}, io={"t": txt})
case("_EA_SMART_DENY_LINE", {}, io={"t": rais("ValueError", "bad")}, expect_raise="ValueError")

# ------------------------------- _slash_channel_gated
case("_slash_channel_gated", {"channel_id": "C1"},
     io={"is_ignored_channel": True, "slack_allowed_channels": None})
case("_slash_channel_gated", {"channel_id": "C1"},
     io={"is_ignored_channel": False, "slack_allowed_channels": script(["C2", "C3"])})
case("_slash_channel_gated", {"channel_id": "C1"},
     io={"is_ignored_channel": False, "slack_allowed_channels": script(["C1", "C2"])})
case("_slash_channel_gated", {"channel_id": "D9"},
     io={"is_ignored_channel": False, "slack_allowed_channels": ["C1"]})
case("_slash_channel_gated", {"channel_id": "C1"},
     io={"is_ignored_channel": False, "slack_allowed_channels": script([])})
case("_slash_channel_gated", {"channel_id": "C1"},
     io={"is_ignored_channel": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_slash_channel_gated", {"channel_id": "C1"}, io={"is_ignored_channel": False},
     expect_raise="RuntimeError")  # slack_allowed_channels missing

# ------------------------------- _is_slack_cdn_url
def cdn(host, scheme="https"):
    return obj(hostname=host, scheme=scheme)

case("_is_slack_cdn_url", {"url": "https://files.slack.com/a"},
     io={"urlparse": cdn("files.slack.com")})
case("_is_slack_cdn_url", {"url": "https://slack.com/"},
     io={"urlparse": cdn("SLACK.COM.")})
case("_is_slack_cdn_url", {"url": "https://evil.example/"},
     io={"urlparse": cdn("evil.example")})
case("_is_slack_cdn_url", {"url": "http://files.slack.com/"},
     io={"urlparse": cdn("files.slack.com", "http")})
case("_is_slack_cdn_url", {"url": "not a url"},
     io={"urlparse": rais("ValueError", "malformed")})
case("_is_slack_cdn_url", {"url": "https://x/"},
     io={"urlparse": cdn(None)})
case("_is_slack_cdn_url", {"url": "https://x/"},
     io={"urlparse": rais("KeyboardInterrupt", "base")}, expect_raise="KeyboardInterrupt")

# ------------------------------- _format_chunks
case("_format_chunks", {"content": "hello"},
     io={"format_message": "*hello*", "truncate_message": script(["*hello*"])})
case("_format_chunks", {"content": "long"},
     io={"format_message": "*long*", "truncate_message": script(["*lo", "ng*"])})
case("_format_chunks", {"content": "empty"},
     io={"format_message": "*empty*", "truncate_message": script([])})
case("_format_chunks", {"content": "x"},
     io={"format_message": rais("ValueError", "fmt")}, expect_raise="ValueError")
case("_format_chunks", {"content": "y"},
     io={"format_message": "y", "truncate_message": rais("SystemExit", "base")},
     expect_raise="SystemExit")
case("_format_chunks", {"content": "z"}, io={"format_message": "z"},
     expect_raise="RuntimeError")

# ------------------------------- _team_in_install_set
case("_team_in_install_set", {"team_id": "T1"}, state={"_team_clients": {"T1": obj(token="x")}})
case("_team_in_install_set", {"team_id": "T2"}, state={"_team_clients": {"T1": obj(token="x")}})
case("_team_in_install_set", {"team_id": ""}, state={"_team_clients": {"T1": obj(token="x")}})
case("_team_in_install_set", {"team_id": None}, state={"_team_clients": {}})
case("_team_in_install_set", {"team_id": "T3"}, state={"_team_clients": {}})
case("_team_in_install_set", {"team_id": "T4"}, state={})

# ------------------------------- _build_thread_session_key
for i, (store, src, key) in enumerate([
    (True, "source-A", "session-A"), (True, "source-B", "session-B"),
    (True, 123, 456), (True, None, None),
]):
    case("_build_thread_session_key",
         {"channel_id": "C1", "thread_ts": "111.2", "user_id": "U1", "team_id": "T1", "chat_type": "group"},
         state={"_session_store": store},
         io={"thread_session_source": src, "source_session_key": key})
case("_build_thread_session_key",
     {"channel_id": "C1", "thread_ts": "111.2", "user_id": "U1"}, state={"_session_store": False})
case("_build_thread_session_key",
     {"channel_id": "C1", "thread_ts": "111.2", "user_id": "U1"}, state={"_session_store": True},
     io={"thread_session_source": rais("RuntimeError", "bad source")})
case("_build_thread_session_key",
     {"channel_id": "C1", "thread_ts": "111.2", "user_id": "U1"}, state={"_session_store": True},
     io={"thread_session_source": "s", "source_session_key": rais("SystemExit", "base")},
     expect_raise="SystemExit")

# ------------------------------- _warn_if_inchannel_without_flat_reply
case("_warn_if_inchannel_without_flat_reply", {"team_name": "Acme"},
     state={"extra": {"reply_in_thread": True}},
     io={"cron_continuable_surface": "in_channel", "logger.warning": None})
case("_warn_if_inchannel_without_flat_reply", {"team_name": "Beta"},
     state={"extra": {"reply_in_thread": False}},
     io={"cron_continuable_surface": "in_channel", "logger.warning": None})
case("_warn_if_inchannel_without_flat_reply", {"team_name": "Gamma"},
     io={"cron_continuable_surface": "threaded", "logger.warning": None})
case("_warn_if_inchannel_without_flat_reply", {"team_name": "Delta"},
     state={"extra": {}},
     io={"cron_continuable_surface": "in_channel", "logger.warning": None})
case("_warn_if_inchannel_without_flat_reply", {"team_name": "Eps"},
     io={"cron_continuable_surface": rais("ValueError", "surface")})
case("_warn_if_inchannel_without_flat_reply", {"team_name": "Zeta"},
     state={"extra": {"reply_in_thread": True}},
     io={"cron_continuable_surface": "in_channel", "logger.warning": rais("SystemExit", "base")},
     expect_raise="SystemExit")
case("_warn_if_inchannel_without_flat_reply", {"team_name": "Eta"},
     io={})

# ------------------------------- _esc
for i, txt in enumerate(["plain", "a&b", "<tag>", "a&b<c>d", "x>y&z<w", ""]):
    case("_esc", {"text": txt})

# ------------------------------- _unseen
case("_unseen", {"piece": "ping", "base": "hello world"}, state={"extras": ["alpha", "beta"]})
case("_unseen", {"piece": "ping", "base": "ping here"}, state={"extras": ["alpha"]})
case("_unseen", {"piece": "x", "base": "abc"}, state={"extras": ["axb", "yyy"]})
case("_unseen", {"piece": "x", "base": "abc"}, state={"extras": []})
case("_unseen", {"piece": "key", "base": "value"}, state={})
case("_unseen", {"piece": "ab", "base": "xxabxx", "": ""}, state={"extras": ["zz"]})

# ------------------------------- _append_line
case("_append_line", {"text": "hello"}, state={"parts": []})
case("_append_line", {"text": "  spaced  "}, state={"parts": []})
case("_append_line", {"text": "quoted", "quote_depth": 2}, state={"parts": ["first"]})
case("_append_line", {"text": "bulleted", "bullet": "- "}, state={"parts": []})
case("_append_line", {"text": "both", "quote_depth": 1, "bullet": "* "}, state={"parts": []})
case("_append_line", {"text": "   "}, state={"parts": []})
case("_append_line", {"text": "", "quote_depth": 3}, state={"parts": []})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for rec in CASES:
            f.write(json.dumps(rec, ensure_ascii=False) + "\n")
    # quick local coverage report
    from collections import Counter
    c = Counter(r["func"] for r in CASES)
    keys = set()
    for r in CASES:
        keys |= set((r["in"]).get("io") or {})
    print(f"wrote {len(OUT)} bytes, {len(CASES)} cases, {len(c)} funcs")
    print("min per func:", min(c.values()), "seam io keys:", len(keys))
    for f in sorted(c):
        print(f"  {f}: {c[f]}")


if __name__ == "__main__":
    main()
