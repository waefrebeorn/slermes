#!/usr/bin/env python3
"""Generate tests/fixtures/slack_adapter_flow9.jsonl for the Slack
plugin-adapter FLOW port, wave 9 (src/gateway/platforms/slack_adapter_flow9.c).

Deterministic corpus: >=6 cases per ported function (16 funcs), covering pure
logic, chained seams with ORDERED traces, fail-closed missing scripts and
BaseException passthrough. Case shape:
{"func", "in": {"args","io"?,"state"}, "expect_raise"?}.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "slack_adapter_flow9.jsonl")
CASES = []


def case(func, args=None, io=None, state=None, expect_raise=None):
    inp = {"args": dict(args or {}), "state": dict(state) if state is not None else {}}
    if io is not None:
        inp["io"] = dict(io)
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


def slist(x):
    """Wrap a LIST data value so the seam backend does not consume it as a
    per-call result sequence."""
    return script(ret=x)


def rais(kind, message, **extra):
    spec = {"type": kind, "message": message}
    spec.update(extra)
    return script(raise_spec=spec)


EMPTY = slist([])  # shared list-valued seam result

# ------------------------------- _date
case("_date", {"group1": "a^b^THIRD^rest", "group2": ""})
case("_date", {"group1": "a^b", "group2": "fallback"})
case("_date", {"group1": "a^b^c", "group2": ""})
case("_date", {"group1": "", "group2": "F"})
case("_date", {"group1": "a^b^c^d", "group2": None})
case("_date", {"group1": "^b^", "group2": ""})

# ------------------------------- _runner_auth_fn
case("_runner_auth_fn", {"self": {"_message_handler": {"__self__": {"_is_user_authorized": "FN"}}}})
case("_runner_auth_fn", {"self": {"_message_handler": {"__self__": {}}}})
case("_runner_auth_fn", {"self": {"_message_handler": None}})
case("_runner_auth_fn", {"self": {}})
case("_runner_auth_fn", {})
case("_runner_auth_fn", {"self": {"_message_handler": {"__self__": {"_is_user_authorized": None}}}})

# ------------------------------- _drop_bot_sender
case("_drop_bot_sender", {"event": {"user": "U1"}, "bot_user_id": "B"},
     io={"event_declares_bot_sender": True, "slack_allow_bots": "none"})
case("_drop_bot_sender", {"event": {}, "bot_user_id": "B"},
     io={"event_declares_bot_sender": False})
case("_drop_bot_sender", {"event": {"user": "U1", "channel": "C"}, "bot_user_id": "B"},
     io={"event_declares_bot_sender": False, "resolve_user_is_bot": True,
         "slack_allow_bots": "all"})
case("_drop_bot_sender", {"event": {"user": "U1", "channel": "C"}, "bot_user_id": "B"},
     io={"event_declares_bot_sender": False, "resolve_user_is_bot": True,
         "slack_allow_bots": "mentions", "mention_detection_text": "hello",
         "logger.debug": None})
case("_drop_bot_sender", {"event": {"user": "B", "client_msg_id": "x"}, "bot_user_id": "B"},
     io={"event_declares_bot_sender": True, "slack_allow_bots": "mentions",
         "mention_detection_text": "<@B> hi"})
case("_drop_bot_sender", {"event": {"user": "U1", "client_msg_id": "x"}, "bot_user_id": "B"},
     io={"event_declares_bot_sender": True, "slack_allow_bots": "mentions",
         "mention_detection_text": "<@B> hi"})
case("_drop_bot_sender", {"event": {"user": "B"}, "bot_user_id": "B"},
     io={"event_declares_bot_sender": True, "slack_allow_bots": "all"})
case("_drop_bot_sender", {"event": {"user": "U1"}, "bot_user_id": "B"},
     io={"event_declares_bot_sender": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _early_reject_unauthorized
case("_early_reject_unauthorized", {"user_id": "U", "channel_id": "C", "authorization_check": "chk"},
     io={"is_sender_authorized": False, "runner_auth_fn": None, "logger.warning": None})
case("_early_reject_unauthorized", {"user_id": "U", "channel_id": "C", "authorization_check": "chk"},
     io={"is_sender_authorized": True, "runner_auth_fn": None})
case("_early_reject_unauthorized", {"user_id": "U", "channel_id": "C", "is_dm": True,
     "authorization_check": None, "thread_id": "T"},
     io={"runner_auth_fn": {"__callable__": True}, "build_source": {"src": 1},
         "auth_fn_call": False, "logger.warning": None})
case("_early_reject_unauthorized", {"user_id": "U", "channel_id": "C"},
     io={"runner_auth_fn": None})
case("_early_reject_unauthorized", {"user_id": "", "channel_id": "C"},
     io={"runner_auth_fn": None})
case("_early_reject_unauthorized", {"user_id": "U", "channel_id": "C", "authorization_check": "chk"},
     io={"is_sender_authorized": None, "runner_auth_fn": {"__callable__": True},
         "build_source": "S", "auth_fn_call": True})
case("_early_reject_unauthorized", {"user_id": "U", "channel_id": "C"},
     io={"runner_auth_fn": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_early_reject_unauthorized", {"user_id": "U", "channel_id": "C", "authorization_check": "chk"},
     io={"is_sender_authorized": rais("ValueError", "x")}, expect_raise="ValueError")

# ------------------------------- _ensure_dm_conversation
case("_ensure_dm_conversation", {"chat_id": "C1"}, io={})
case("_ensure_dm_conversation", {"chat_id": "U1", "team_id": "T"},
     state={"_dm_conversation_cache": {"T:U1": "D1"}})
case("_ensure_dm_conversation", {"chat_id": "U2", "team_id": "T"},
     io={"get_client": None, "conversations_open": {"channel": {"id": "D2"}},
         "trim_oldest_dict_entries": None, "remember_channel_team": None})
case("_ensure_dm_conversation", {"chat_id": "W3", "team_id": ""},
     io={"get_client": None, "conversations_open": {"channel": {"id": "D3"}},
         "trim_oldest_dict_entries": None})
case("_ensure_dm_conversation", {"chat_id": "U4"},
     io={"get_client": None, "conversations_open": {}})
case("_ensure_dm_conversation", {"chat_id": "U5"},
     io={"get_client": None, "conversations_open": rais("ValueError", "open"),
         "logger.warning": None})
case("_ensure_dm_conversation", {"chat_id": "U6"},
     io={"get_client": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_ensure_dm_conversation", {"chat_id": "U7", "team_id": "T"},
     io={"get_client": None, "conversations_open": {"channel": {"id": "D7"}},
         "trim_oldest_dict_entries": rais("ValueError", "trim"), "logger.warning": None})

# ------------------------------- _ensure_socket_watchdog
case("_ensure_socket_watchdog", {}, io={"create_task": {"id": "t"}, "add_done_callback": None})
case("_ensure_socket_watchdog", {"task": {"id": "t1"}},
     io={"task.done": True, "create_task": {"id": "t2"}, "add_done_callback": None})
case("_ensure_socket_watchdog", {"task": {"id": "t1"}}, io={"task.done": False})
case("_ensure_socket_watchdog", {"task": {"id": "t1"}},
     io={"task.done": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_ensure_socket_watchdog", {}, io={"create_task": rais("ValueError", "c")},
     expect_raise="ValueError")
case("_ensure_socket_watchdog", {"task": None},
     io={"create_task": "T", "add_done_callback": rais("KeyboardInterrupt", "k")},
     expect_raise="KeyboardInterrupt")

# ------------------------------- _extract_additional_text_from_slack_blocks
case("_extract_additional_text_from_slack_blocks", {"blocks": [], "primary_text": "p", "bot_uid": "B"},
     io={"normalize_text": "P", "fenced_matches": EMPTY})
case("_extract_additional_text_from_slack_blocks", {"blocks": [{"type": "table"}], "primary_text": "p"},
     io={"normalize_text": "P", "fenced_matches": EMPTY, "render_table_block": "TBL"})
case("_extract_additional_text_from_slack_blocks",
     {"blocks": [{"type": "rich_text", "elements": [{"type": "rich_text_section"}]}],
      "primary_text": "prim"},
     io={"normalize_text": ["P", "N"], "fenced_matches": EMPTY, "extract_text_blocks": "  new text  "})
case("_extract_additional_text_from_slack_blocks",
     {"blocks": [{"type": "rich_text", "elements": [{"type": "rich_text_section"}]}],
      "primary_text": "prim"},
     io={"normalize_text": ["P", "P"], "fenced_matches": EMPTY, "extract_text_blocks": "same"})
case("_extract_additional_text_from_slack_blocks",
     {"blocks": [{"type": "rich_text", "elements": [{"type": "rich_text_preformatted"}]}],
      "primary_text": "prim"},
     io={"normalize_text": ["P", "C", "C"], "fenced_matches": ["raw"],
         "extract_text_blocks": "code"})
case("_extract_additional_text_from_slack_blocks",
     {"blocks": [{"type": "rich_text", "elements": [{"type": "rich_text_preformatted"}]}],
      "primary_text": "prim"},
     io={"normalize_text": ["P", "C", "X"], "fenced_matches": ["raw"],
         "extract_text_blocks": "code"})
case("_extract_additional_text_from_slack_blocks",
     {"blocks": [{"type": "rich_text", "elements": [{"type": "rich_text_section"}]}],
      "primary_text": "prim"},
     io={"normalize_text": "P", "fenced_matches": EMPTY,
         "extract_text_blocks": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_extract_additional_text_from_slack_blocks", {"blocks": [{"type": "table"}], "primary_text": "p"},
     io={"normalize_text": "P", "fenced_matches": EMPTY,
         "render_table_block": rais("ValueError", "t")}, expect_raise="ValueError")

# ------------------------------- _extract_text_from_slack_attachments
case("_extract_text_from_slack_attachments", {"attachments": []}, io={})
case("_extract_text_from_slack_attachments",
     {"attachments": [{"pretext": "P", "title": "T", "text": "X"}]},
     io={"shared_message_sections": slist(["FWD"]), "is_forward": False})
case("_extract_text_from_slack_attachments",
     {"attachments": [{"fields": [{"title": "A", "value": "B"}]}]},
     io={"shared_message_sections": EMPTY, "is_forward": False})
case("_extract_text_from_slack_attachments", {"attachments": [{"fallback": "FB"}]},
     io={"shared_message_sections": EMPTY, "is_forward": False})
case("_extract_text_from_slack_attachments",
     {"attachments": [{"is_msg_unfurl": True, "text": "T"}]},
     io={"shared_message_sections": EMPTY})
case("_extract_text_from_slack_attachments", {"attachments": [{"text": "T"}]},
     io={"shared_message_sections": EMPTY, "is_forward": True})
case("_extract_text_from_slack_attachments", {"attachments": [{"blocks": [{"type": "x"}]}]},
     io={"shared_message_sections": EMPTY, "is_forward": False,
         "extract_text_blocks": "BT"})
case("_extract_text_from_slack_attachments", {"attachments": [{"text": "T"}]},
     io={"shared_message_sections": EMPTY, "is_forward": rais("SystemExit", "base")},
     expect_raise="SystemExit")

# ------------------------------- _fatal_missing_env
for _i, _n in enumerate(["SLACK_BOT_TOKEN", "SLACK_APP_TOKEN", "X",
                         "SLACK_SIGNING_SECRET", "A_B", "Z"]):
    case("_fatal_missing_env", {"env_name": _n},
         io={"logger.error": None, "set_fatal_error": None})
case("_fatal_missing_env", {"env_name": "X"},
     io={"logger.error": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _fetch_thread_context
FC = {"channel_id": "C", "thread_ts": "100", "current_ts": "101", "team_id": "T",
      "ttl": 60, "cache_max": 10}
case("_fetch_thread_context", dict(FC),
     io={"thread_cache_key": "K", "monotonic": 100,
         "thread_cache_get": {"content": "CACHED", "fetched_at": 95, "messages": []}})
case("_fetch_thread_context", dict(FC, after_ts="103"),
     io={"thread_cache_key": "K", "monotonic": 100,
         "thread_cache_get": {"content": "CACHED", "fetched_at": 95, "messages": [{"ts": "105"}]},
         "format_thread_context": slist(["DELTA", "P"])})
case("_fetch_thread_context", dict(FC),
     io={"thread_cache_key": "K", "monotonic": 100, "thread_cache_get": None,
         "conversations_replies_with_backoff": {"messages": []}})
case("_fetch_thread_context", dict(FC),
     io={"thread_cache_key": "K", "monotonic": 100, "thread_cache_get": None,
         "conversations_replies_with_backoff": None})
case("_fetch_thread_context", dict(FC),
     io={"thread_cache_key": "K", "monotonic": 100, "thread_cache_get": None,
         "conversations_replies_with_backoff": {"messages": [{"ts": "100"}, {"ts": "102"}]},
         "format_thread_context": slist(["CONTENT", "PARENT"]),
         "thread_root_message": {"user": "U"}, "thread_cache_set": None,
         "thread_cache_len": 2})
case("_fetch_thread_context", dict(FC, after_ts="103"),
     io={"thread_cache_key": "K", "monotonic": 100, "thread_cache_get": None,
         "conversations_replies_with_backoff": {"messages": [{"ts": "100"}, {"ts": "102"}]},
         "format_thread_context": [slist(["CONTENT", "PARENT"]), slist(["DELTA", "PARENT"])],
         "thread_root_message": {"user": "U"}, "thread_cache_set": None,
         "thread_cache_len": 2})
case("_fetch_thread_context", dict(FC),
     io={"thread_cache_key": "K", "monotonic": 100, "thread_cache_get": None,
         "conversations_replies_with_backoff": {"messages": [{"ts": "100"}]},
         "format_thread_context": slist(["C", "P"]), "thread_root_message": {},
         "thread_cache_set": None, "thread_cache_len": 11,
         "thread_cache_del_stale": None})
case("_fetch_thread_context", dict(FC, force_refresh=True, chat_type="dm"),
     io={"thread_cache_key": "K", "monotonic": 100,
         "conversations_replies_with_backoff": {"messages": []}})
case("_fetch_thread_context", dict(FC),
     io={"thread_cache_key": "K", "monotonic": 100, "thread_cache_get": None,
         "conversations_replies_with_backoff": rais("ValueError", "net"),
         "logger.warning": None})
case("_fetch_thread_context", dict(FC),
     io={"thread_cache_key": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _fetch_thread_parent_text
FP = {"channel_id": "C", "thread_ts": "100", "team_id": "T", "ttl": 60, "bot_uid": "B"}
case("_fetch_thread_parent_text", dict(FP),
     io={"thread_cache_key": "K", "monotonic": 100,
         "thread_cache_get": {"parent_text": "PT", "fetched_at": 95, "messages": []}})
case("_fetch_thread_parent_text", dict(FP, strip_bot_mention=False),
     io={"thread_cache_key": "K", "monotonic": 100,
         "thread_cache_get": {"parent_text": "PT", "fetched_at": 95, "messages": []},
         "thread_root_message": {"text": "  ROOT  "}})
case("_fetch_thread_parent_text", dict(FP, strip_bot_mention=False),
     io={"thread_cache_key": "K", "monotonic": 100,
         "thread_cache_get": {"parent_text": "PT", "fetched_at": 95, "messages": []},
         "thread_root_message": None})
case("_fetch_thread_parent_text", dict(FP),
     io={"thread_cache_key": "K", "monotonic": 100, "thread_cache_get": None,
         "get_client": None, "conversations_replies": {"messages": []}})
case("_fetch_thread_parent_text", dict(FP),
     io={"thread_cache_key": "K", "monotonic": 100, "thread_cache_get": None,
         "get_client": None, "conversations_replies": {"messages": [{"ts": "999"}]}})
case("_fetch_thread_parent_text", dict(FP),
     io={"thread_cache_key": "K", "monotonic": 100, "thread_cache_get": None,
         "get_client": None, "conversations_replies": {"messages": [{"ts": "100"}]},
         "render_message_text": " <@B> hello "})
case("_fetch_thread_parent_text", dict(FP, strip_bot_mention=False),
     io={"thread_cache_key": "K", "monotonic": 100, "thread_cache_get": None,
         "get_client": None, "conversations_replies": {"messages": [{"ts": "100"}]},
         "render_message_text": " <@B> hello "})
case("_fetch_thread_parent_text", dict(FP),
     io={"thread_cache_key": "K", "monotonic": 100, "thread_cache_get": None,
         "get_client": None,
         "conversations_replies": rais("ValueError", "r"), "logger.debug": None})
case("_fetch_thread_parent_text", dict(FP),
     io={"thread_cache_key": "K", "monotonic": 100, "thread_cache_get": None,
         "get_client": None, "conversations_replies": {"messages": [{"ts": "100"}]},
         "render_message_text": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _finalize_interactive_message
FI = {"channel_id": "C", "msg_ts": "100", "decision_text": "D", "label": "approval"}
case("_finalize_interactive_message", dict(FI),
     io={"get_client": None, "sanitize_blocks": slist([{"sanitized": True}]), "chat_update": None})
case("_finalize_interactive_message", dict(FI, sanitize=False),
     io={"get_client": None, "chat_update": None})
case("_finalize_interactive_message", dict(FI, original_text="", placeholder="PH"),
     io={"get_client": None, "sanitize_blocks": EMPTY, "chat_update": None})
case("_finalize_interactive_message", dict(FI),
     io={"get_client": None, "sanitize_blocks": EMPTY,
         "chat_update": rais("ValueError", "u"), "logger.warning": None})
case("_finalize_interactive_message", dict(FI),
     io={"get_client": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_finalize_interactive_message", dict(FI),
     io={"get_client": None, "sanitize_blocks": rais("ValueError", "s"),
         "logger.warning": None})
case("_finalize_interactive_message", dict(FI),
     io={"get_client": None, "sanitize_blocks": EMPTY,
         "chat_update": rais("KeyboardInterrupt", "k")}, expect_raise="KeyboardInterrupt")

# ------------------------------- _format_thread_context
FT = {"thread_ts": "100", "current_ts": "101", "team_id": "T", "channel_id": "C",
      "chat_type": "group"}
M3 = [{"ts": "100"}, {"ts": "101"}, {"ts": "102"}]
case("_format_thread_context", dict(FT, messages=M3),
     io={"render_message_text": ["PARENT", "CHILD"], "reactions_marker": [None, None],
         "thread_context_line": ["LINE_P", "LINE_C"]})
case("_format_thread_context", dict(FT, messages=M3),
     io={"render_message_text": ["PARENT", "CHILD"], "reactions_marker": [None, None],
         "thread_context_line": ["[unverified] L", "L2"]})
case("_format_thread_context", dict(FT, messages=M3, after_ts="101"),
     io={"render_message_text": ["P", "C"], "reactions_marker": [None, None],
         "thread_context_line": ["LINE_C"]})
case("_format_thread_context", dict(FT, messages=[{"ts": "100"}], bot_uid="B"),
     io={"render_message_text": " <@B> hi ", "reactions_marker": None,
         "thread_context_line": "LINE"})
case("_format_thread_context", dict(FT, messages=[{"ts": "100"}]),
     io={"render_message_text": "HI", "reactions_marker": ":eyes:",
         "thread_context_line": "LINE"})
case("_format_thread_context", dict(FT, messages=[{"ts": "101"}]), io={})
case("_format_thread_context", dict(FT, messages=[{"ts": "100"}]),
     io={"render_message_text": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _get_thread_watermark
GW = {"channel_id": "C", "thread_ts": "100", "user_id": "U", "team_id": "T"}
for _v in ["123", "", None, "abc"]:
    case("_get_thread_watermark", dict(GW), io={"thread_watermark_io": _v})
case("_get_thread_watermark", dict(GW),
     io={"thread_watermark_io": rais("ValueError", "w")})
case("_get_thread_watermark", dict(GW),
     io={"thread_watermark_io": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _handle_app_context_changed
for _i in range(5):
    case("_handle_app_context_changed", {"event": {"channel": "C%d" % _i}, "body": {}},
         io={"agent_view_event_fields": {"team_id": "T", "user_id": "U", "context_channel_id": "X"},
             "cache_agent_view_context": None})
case("_handle_app_context_changed", {"event": {"channel": "C"}, "body": {}},
     io={"agent_view_event_fields": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _handle_app_home_opened
FH = {"team_id": "T", "user_id": "U", "context_channel_id": "X"}
case("_handle_app_home_opened", {"event": {"tab": "home"}})
case("_handle_app_home_opened", {"event": {"tab": "messages", "channel": "C1"}, "body": {}},
     io={"agent_view_event_fields": FH, "remember_channel_team": None,
         "cache_agent_view_context": None, "seed_dm_session": None,
         "set_assistant_suggested_prompts": None})
case("_handle_app_home_opened", {"event": {"tab": "messages", "channel": "C2"}, "body": {}},
     io={"agent_view_event_fields": {"team_id": "", "user_id": "U", "context_channel_id": ""},
         "cache_agent_view_context": None, "seed_dm_session": None,
         "set_assistant_suggested_prompts": None})
case("_handle_app_home_opened", {"event": {"tab": "messages", "channel_id": "C3"}, "body": {}},
     io={"agent_view_event_fields": FH, "remember_channel_team": None,
         "cache_agent_view_context": None, "seed_dm_session": None,
         "set_assistant_suggested_prompts": None})
case("_handle_app_home_opened", {"event": {"tab": "messages", "channel": "C4"}, "body": {}},
     io={"agent_view_event_fields": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_handle_app_home_opened", {"event": {"tab": "messages", "channel": "C5"}, "body": {}},
     io={"agent_view_event_fields": FH, "remember_channel_team": None,
         "cache_agent_view_context": None, "seed_dm_session": rais("ValueError", "s")},
     expect_raise="ValueError")
case("_handle_app_home_opened", {"event": {"tab": "messages", "channel": "C6"}, "body": {}},
     io={"agent_view_event_fields": FH, "remember_channel_team": None,
         "cache_agent_view_context": None, "seed_dm_session": None,
         "set_assistant_suggested_prompts": rais("KeyboardInterrupt", "k")},
     expect_raise="KeyboardInterrupt")


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for rec in CASES:
            f.write(json.dumps(rec, ensure_ascii=False) + "\n")
    counts = {}
    for r in CASES:
        counts[r["func"]] = counts.get(r["func"], 0) + 1
    print("wrote", OUT, "cases", len(CASES), "funcs", len(counts))
    for f in sorted(counts):
        print("  " + f + ": " + str(counts[f]))


if __name__ == "__main__":
    main()
