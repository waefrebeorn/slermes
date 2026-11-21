#!/usr/bin/env python3
"""Deterministic case generator for the Telegram adapter FLOW part 16 oracle.

Writes tests/fixtures/telegram_adapter_flow16.jsonl. Each case is
{"func", "in": {"args", "state"?, "io"}}. Re-running reproduces byte-identical
fixtures. Every ported function gets >= 6 cases; every seam name appears in at
least one case's io (asserted at the bottom).
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "telegram_adapter_flow16.jsonl"

# EXACTLY the C ta16_call seam names (set-equality enforced in the oracle).
SEAM_NAMES = [
    "accept_update", "admission_get", "answer_callback", "append_transcript", "auth_env_configured",
    "auth_fn_call", "background_add", "build_client", "build_message_event", "callback_auth_call",
    "coerce_allow_set", "config_exists", "current_bot_username", "edit_html_quiet", "edit_message_text",
    "ensure_future", "entity_sources", "entity_span", "entity_type", "env_allowlist_decision",
    "event_session_key", "extract_mentions", "fail_update_preparation", "format_message", "get_hermes_home",
    "get_language", "get_or_create_session", "handler_call", "has_hook", "html_escape",
    "is_sender_authorized", "legacy_runner_auth_fn", "logger_debug", "logger_info", "logger_log",
    "logger_warning", "merge_batched_event", "normalize_platform_event", "observe_attributed_text",
    "observe_shared_source", "pass_unauthorized_dm", "picker_nav_row", "read_config", "redact_error",
    "refresh_gates", "resolve_routing", "rich_content_ok", "rows_of_two", "sender_suffix", "sha256_hex",
    "source_for_platform_event_auth", "source_from_message", "task_cancel", "task_done",
    "task_done_callback", "time_time", "toast", "translate", "write_config",
]
SEAM_NAMES = list(dict.fromkeys(SEAM_NAMES))

CASES = []
ALL_IO = set()
ALL_FUNCS = set()


def sc(ret=None, raise_type=None, raise_msg=""):
    d = {"__script__": True}
    if raise_type is not None:
        d["raise"] = {"type": raise_type, "message": raise_msg}
    else:
        d["ret"] = ret
    return d


def mk(func, args=None, state=None, io=None, label=""):
    full = {s: sc(None) for s in SEAM_NAMES}
    if io:
        full.update(io)
    c = {"func": func, "in": {"args": args or {}, "state": state or {}, "io": full}}
    if label:
        c["_label"] = label
    ALL_IO.update(full.keys())
    ALL_FUNCS.add(func)
    CASES.append(c)


def src(uid="u1", ct="private", cid="c1", tid=None, uname="u", bot=False):
    return {"user_id": uid, "chat_type": ct, "chat_id": cid,
            "thread_id": tid, "user_name": uname, "is_bot": bot}


# ── _is_user_authorized_from_message ────────────────────────────────────────
# 1: no user identity -> True, only the two entry seams fire.
mk("_is_user_authorized_from_message", {"message": {"text": "m"}}, {},
   {"source_from_message": sc(src(uid=None))})
# 2: allow_from grant (DM).
mk("_is_user_authorized_from_message", {"message": {"text": "m"}},
   {"config_extra": {"allow_from": "u1,u2"}},
   {"source_from_message": sc(src()), "coerce_allow_set": sc(["u1", "u2"])})
# 3: group allow_from grant.
mk("_is_user_authorized_from_message", {"message": {"text": "m"}},
   {"config_extra": {"group_allow_from": "*"}},
   {"source_from_message": sc(src(ct="group")), "coerce_allow_set": sc(["*"])})
# 4: callable callback override -> authorized True.
mk("_is_user_authorized_from_message", {"message": {"text": "m"}},
   {"_is_callback_user_authorized": True, "config_extra": {"allow_from": "x"}},
   {"source_from_message": sc(src()), "coerce_allow_set": sc(["x"]), "callback_auth_call": sc(True)})
# 5: callback override raises (suppressed), then runner callback grants.
mk("_is_user_authorized_from_message", {"message": {"text": "m"}},
   {"_is_callback_user_authorized": True, "_authorization_check": True,
    "config_extra": {"allow_from": "x"}},
   {"source_from_message": sc(src()), "coerce_allow_set": sc(["x"]),
    "callback_auth_call": sc(raise_type="RuntimeError", raise_msg="boom"),
    "is_sender_authorized": sc(True)})
# 6: legacy auth_fn grants (no registered callback; env configured).
mk("_is_user_authorized_from_message", {"message": {"text": "m"}},
   {"config_extra": {"allow_from": "x"}},
   {"source_from_message": sc(src()), "coerce_allow_set": sc(["x"]),
    "legacy_runner_auth_fn": sc("fn"), "auth_env_configured": sc(True),
    "auth_fn_call": sc(True)})
# 7: sender-auth grants via registered callback.
mk("_is_user_authorized_from_message", {"message": {"text": "m"}},
   {"_authorization_check": True, "config_extra": {"allow_from": "x"}},
   {"source_from_message": sc(src()), "coerce_allow_set": sc(["x"]),
    "is_sender_authorized": sc(True)})
# 8: auth_fn raises -> logger.debug fallback -> env allowlist grants.
mk("_is_user_authorized_from_message", {"message": {"text": "m"}},
   {"config_extra": {"allow_from": "x"}},
   {"source_from_message": sc(src()), "coerce_allow_set": sc(["x"]),
    "legacy_runner_auth_fn": sc("fn"), "auth_env_configured": sc(True),
    "auth_fn_call": sc(raise_type="ValueError", raise_msg="bad"),
    "env_allowlist_decision": sc(True)})
# 9: no allowlist, no env -> unknown DMs pass (True) before runner chain.
mk("_is_user_authorized_from_message", {"message": {"text": "m"}}, {},
   {"source_from_message": sc(src()), "legacy_runner_auth_fn": sc("fn")})
# 10: denied, forwarded for DM pairing.
mk("_is_user_authorized_from_message", {"message": {"text": "m"}},
   {"config_extra": {"allow_from": "x"}},
   {"source_from_message": sc(src()), "coerce_allow_set": sc(["x"]),
    "env_allowlist_decision": sc(False), "pass_unauthorized_dm": sc(True)})

# ── _legacy_runner_auth_fn ──────────────────────────────────────────────────
# Pure attribute navigation. state "_runner_auth" = present callable marker.
mk("_legacy_runner_auth_fn", {}, {"_runner_auth": "fn"})
mk("_legacy_runner_auth_fn", {}, {"_runner_auth": None})
mk("_legacy_runner_auth_fn", {}, {})
mk("_legacy_runner_auth_fn", {"probe": 1}, {"_runner_auth": "other"})
mk("_legacy_runner_auth_fn", {}, {"_runner_auth": "fn", "extra": 1})
mk("_legacy_runner_auth_fn", {}, {"_runner_auth": 0})

# ── _log_blocked_user ───────────────────────────────────────────────────────
for lvl, what, uid, cid in [(30, "unauthorized user", 7, 99),
                            (40, "banned user", None, 99),
                            (30, "x", 1, None),
                            (20, "guest", "u", "c"),
                            (10, "", 0, 0),
                            (30, "spam", -1, -2)]:
    mk("_log_blocked_user", {"level": lvl, "what": what,
                             "from_user": {"id": uid}, "chat": {"id": cid}}, {})

# ── _menu_fingerprint ───────────────────────────────────────────────────────
for lang, menu, digest in [
        ("en", [["a", "b"]], "d1"),
        ("de", [], "d2"),
        ("en", [["x", "y"], ["z", "w"]], "d3"),
        ("fr", [["only"]], "d4"),
        ("", [["a"]], "d5"),
        ("en", [["1", "2"], []], "d6")]:
    mk("_menu_fingerprint", {"menu_commands": menu}, {},
       {"get_language": sc(lang), "sha256_hex": sc(digest)})

# ── _merge_into_pending ─────────────────────────────────────────────────────
for key, lane in [("k1", "photo"), ("k2", "text"), ("k3", "album"),
                  ("k4", ""), ("k5", "batch"), ("k6", None)]:
    mk("_merge_into_pending", {"pending": {"p": 1}, "key": key, "event": {"text": "e"}},
       {"_merge_caption": True},
       {"resolve_routing": sc(None), "merge_batched_event": sc(lane)})
mk("_merge_into_pending", {"pending": {}, "key": "kx", "event": {"text": "e"}},
   {"_merge_caption": False},
   {"resolve_routing": sc(raise_type="RuntimeError", raise_msg="rr"),
    "merge_batched_event": sc("lane")})

# ── _message_mentions_bot ───────────────────────────────────────────────────
def ent(t, offset=0, length=5, uid=None):
    d = {"type": t, "offset": offset, "length": length}
    if uid is not None:
        d["user"] = {"id": uid}
    return d

mk("_message_mentions_bot", {"message": {"text": "hi"}}, {}, {})  # no bot
mk("_message_mentions_bot", {"message": {"text": "hi"}}, {"_bot": {"id": 9}},
   {"current_bot_username": sc(None), "entity_sources": sc([])})
mk("_message_mentions_bot", {"message": {"text": "hi @hermes"}},
   {"_bot": {"id": 9}},
   {"current_bot_username": sc("hermes"),
    "entity_sources": sc([["hi @hermes", [ent("mention")]]]),
    "entity_type": sc("mention"), "entity_span": sc("@HERMES")})
mk("_message_mentions_bot", {"message": {"text": "/new@hermes"}},
   {"_bot": {"id": 9}},
   {"current_bot_username": sc("hermes"),
    "entity_sources": sc([["/new@hermes", [ent("bot_command")]]]),
    "entity_type": sc("bot_command"), "entity_span": sc("/new@Hermes")})
mk("_message_mentions_bot", {"message": {"text": "hey"}},
   {"_bot": {"id": 9}},
   {"current_bot_username": sc("hermes"),
    "entity_sources": sc([["hey", [ent("text_mention", uid=9)]]]),
    "entity_type": sc("text_mention")})
mk("_message_mentions_bot", {"message": {"text": "hey"}},
   {"_bot": {"id": 9}},
   {"current_bot_username": sc("hermes"), "entity_sources": sc([]),
    "extract_mentions": sc(["hermes"])})
mk("_message_mentions_bot", {"message": {"text": "hey"}},
   {"_bot": {"id": 9}},
   {"current_bot_username": sc("hermes"), "entity_sources": sc([]),
    "extract_mentions": sc(["other"])})

# ── _notify_clarify_expired ─────────────────────────────────────────────────
Q = {"message": {"text": "old"}, "id": 5}
mk("_notify_clarify_expired", {"query": Q, "user_display": "Ann"}, {},
   {"toast": sc("TOAST"), "answer_callback": sc(None),
    "html_escape": sc("old"), "translate": sc("BODY"),
    "edit_html_quiet": sc(None)})
mk("_notify_clarify_expired", {"query": Q, "user_display": "Ann"}, {},
   {"toast": sc("T"), "answer_callback": sc(raise_type="RuntimeError", raise_msg="ack"),
    "html_escape": sc("&amp;"), "translate": sc("B"), "edit_html_quiet": sc(None)})
mk("_notify_clarify_expired", {"query": {"message": {"text": None}, "id": 6}},
   {}, {"toast": sc("T"), "answer_callback": sc(None),
        "html_escape": sc(""), "translate": sc("B"), "edit_html_quiet": sc(None)})
mk("_notify_clarify_expired", {"query": Q, "user_display": "X"}, {},
   {"toast": sc("T"), "answer_callback": sc(None),
    "html_escape": sc("a"), "translate": sc("b"),
    "edit_html_quiet": sc(raise_type="OSError", raise_msg="edit")})
mk("_notify_clarify_expired", {"query": Q, "user_display": "Y"}, {},
   {"toast": sc("T"), "answer_callback": sc(None),
    "html_escape": sc("<x>"), "translate": sc("Z"), "edit_html_quiet": sc(None)})
mk("_notify_clarify_expired", {"query": Q, "user_display": "Z"}, {},
   {"toast": sc(raise_type="ValueError", raise_msg="t"), "answer_callback": sc(None),
    "html_escape": sc("s"), "translate": sc("y"), "edit_html_quiet": sc(None)})
# BaseException passthrough: SystemExit is NOT swallowed by suppress(Exception).
mk("_notify_clarify_expired", {"query": Q, "user_display": "SE"}, {},
   {"toast": sc(raise_type="SystemExit", raise_msg="bye"), "answer_callback": sc(None),
    "html_escape": sc("s"), "translate": sc("y"), "edit_html_quiet": sc(None)})

# ── _observe_unmentioned_group_message ──────────────────────────────────────
mk("_observe_unmentioned_group_message",
   {"message": {"chat": {"id": 7}}, "msg_type": "TEXT", "update_id": 11},
   {})  # no session store -> early return
mk("_observe_unmentioned_group_message",
   {"message": {"chat": {"id": 7}}, "msg_type": "TEXT", "update_id": 11},
   {"_session_store": True, "name": "tg"},
   {"build_message_event": sc({"source": {"user_id": "u1"}, "message_id": 42}),
    "observe_shared_source": sc({"user_id": "u1"}),
    "get_or_create_session": sc({"session_id": "s1"}),
    "observe_attributed_text": sc("observed text"), "time_time": sc(100.5),
    "accept_update": sc(None), "append_transcript": sc(None), "logger_info": sc(None)})
mk("_observe_unmentioned_group_message",
   {"message": {"chat": {"id": 7}}, "msg_type": "TEXT", "update_id": 11},
   {"_session_store": True, "name": "tg"},
   {"build_message_event": sc({"source": {"user_id": None}, "message_id": 0}),
    "observe_shared_source": sc({"user_id": None}),
    "get_or_create_session": sc({"session_id": "s2"}),
    "observe_attributed_text": sc("t"), "time_time": sc(1.0),
    "accept_update": sc(None), "append_transcript": sc(None), "logger_info": sc(None)})
mk("_observe_unmentioned_group_message",
   {"message": {"chat": {"id": 7}}, "msg_type": "TEXT", "update_id": 11,
    "event": {"source": {"user_id": "u2"}, "message_id": 9}},
   {"_session_store": True},
   {"observe_shared_source": sc({"user_id": "u2"}),
    "get_or_create_session": sc({"session_id": "s3"}),
    "observe_attributed_text": sc("e"), "time_time": sc(2.0),
    "accept_update": sc(None), "append_transcript": sc(None), "logger_info": sc(None)})
mk("_observe_unmentioned_group_message",
   {"message": {"chat": {"id": 7}}, "msg_type": "TEXT", "update_id": 11},
   {"_session_store": True, "name": "tg"},
   {"build_message_event": sc({"source": {"user_id": "u3"}, "message_id": 1}),
    "observe_shared_source": sc({"user_id": "u3"}),
    "get_or_create_session": sc(raise_type="RuntimeError", raise_msg="store"),
    "fail_update_preparation": sc(None), "logger_warning": sc(None)})
mk("_observe_unmentioned_group_message",
   {"message": {"chat": {"id": 7}}, "msg_type": "TEXT", "update_id": 11},
   {"_session_store": True, "name": "tg"},
   {"build_message_event": sc(None), "fail_update_preparation": sc(None),
    "logger_warning": sc(None)})

# ── _on_platform_update ──────────────────────────────────────────────────────
mk("_on_platform_update", {"update": {"id": 1}, "context": {}}, {})  # no handler
mk("_on_platform_update", {"update": {"id": 1}, "context": {}},
   {"_platform_event_handler": True, "_update_admission": None},
   {"has_hook": sc(False)})
mk("_on_platform_update", {"update": {"id": 2}, "context": {}},
   {"_platform_event_handler": True},
   {"has_hook": sc(True), "normalize_platform_event": sc(None)})
mk("_on_platform_update", {"update": {"id": 3}, "context": {}},
   {"_platform_event_handler": True},
   {"has_hook": sc(True), "normalize_platform_event": sc(raise_type="ValueError", raise_msg="n"),
    "fail_update_preparation": sc(None), "logger_debug": sc(None)})
mk("_on_platform_update", {"update": {"id": 4}, "context": {}},
   {"_platform_event_handler": True},
   {"has_hook": sc(True), "normalize_platform_event": sc({"kind": "e"}),
    "source_for_platform_event_auth": sc({"user_id": "u"}), "accept_update": sc(None),
    "handler_call": sc(None)})
mk("_on_platform_update", {"update": {"id": 5}, "context": {}},
   {"_platform_event_handler": True, "_update_admission": True,
    "_updates_dispatched_total": 4},
   {"admission_get": sc(None), "has_hook": sc(True),
    "normalize_platform_event": sc({"kind": "e"}),
    "source_for_platform_event_auth": sc({"user_id": "u"}), "accept_update": sc(None),
    "handler_call": sc(raise_type="RuntimeError", raise_msg="h"),
    "fail_update_preparation": sc(None), "logger_debug": sc(None)})

# ── _orphan_and_rebuild_polling_client ──────────────────────────────────────
REQ = {"_client": {"is_closed": False}, "_build_client": True}
mk("_orphan_and_rebuild_polling_client", {"polling_req": REQ}, {})  # closed? -> not here
mk("_orphan_and_rebuild_polling_client",
   {"polling_req": {"_client": None, "_build_client": True}}, {})
mk("_orphan_and_rebuild_polling_client",
   {"polling_req": {"_client": {"is_closed": True}, "_build_client": True}}, {})
mk("_orphan_and_rebuild_polling_client",
   {"polling_req": {"_client": {"is_closed": False}, "_build_client": None}}, {})
mk("_orphan_and_rebuild_polling_client", {"polling_req": REQ}, {},
   {"build_client": sc({"is_closed": False}), "ensure_future": sc("task"),
    "background_add": sc(None), "task_done_callback": sc(None)})
mk("_orphan_and_rebuild_polling_client", {"polling_req": REQ}, {},
   {"build_client": sc(raise_type="RuntimeError", raise_msg="bc"), "logger_debug": sc(None)})

# ── _paged_keyboard ─────────────────────────────────────────────────────────
for btns, pm, cols in [
        (["a", "b", "c"], {"total_pages": 1, "page": 0, "page_info": "p1"}, 2),
        (["a", "b", "c"], {"total_pages": 3, "page": 0, "page_info": "p0"}, 2),
        (["a", "b", "c"], {"total_pages": 3, "page": 1, "page_info": "p1"}, 1),
        ([], {"total_pages": 0, "page": 0, "page_info": "z"}, 2),
        (["x"], {"total_pages": 2, "page": 1, "page_info": "q"}, 3),
        (["a", "b"], {"total_pages": 5, "page": 2, "page_info": "r"}, 2)]:
    mk("_paged_keyboard",
       {"buttons": btns, "page_meta": pm, "nav_prefix": "np", "tail_row": ["T"], "columns": cols}, {},
       {"rows_of_two": sc([btns[:2], btns[2:]]), "picker_nav_row": sc(["NAV"])})

# ── _persist_dm_topic_thread_id ─────────────────────────────────────────────
CFG_NEW = {"platforms": {"telegram": {"extra": {}}}}
CFG_MATCH = {"platforms": {"telegram": {"extra": {"dm_topics": [
    {"chat_id": 1, "topics": [{"name": "t", "thread_id": 0}]}]}}}}
CFG_MATCH2 = {"platforms": {"telegram": {"extra": {"dm_topics": [
    {"chat_id": 1, "topics": [{"name": "t", "thread_id": 5}]}]}}}}
mk("_persist_dm_topic_thread_id", {"chat_id": 1, "topic_name": "t", "thread_id": 9},
   {}, {"get_hermes_home": sc("/home"), "config_exists": sc(False), "logger_warning": sc(None)})
mk("_persist_dm_topic_thread_id", {"chat_id": 1, "topic_name": "t", "thread_id": 9},
   {}, {"get_hermes_home": sc("/home"), "config_exists": sc(True),
        "read_config": sc({"platforms": {}}), "write_config": sc(None), "logger_info": sc(None)})
mk("_persist_dm_topic_thread_id", {"chat_id": 1, "topic_name": "t", "thread_id": 9},
   {}, {"get_hermes_home": sc("/home"), "config_exists": sc(True),
        "read_config": sc(CFG_MATCH), "write_config": sc(None), "logger_info": sc(None)})
mk("_persist_dm_topic_thread_id", {"chat_id": 1, "topic_name": "t", "thread_id": 9,
                                   "replace_existing": True}, {},
   {"get_hermes_home": sc("/home"), "config_exists": sc(True),
    "read_config": sc(CFG_MATCH2), "write_config": sc(None), "logger_info": sc(None)})
mk("_persist_dm_topic_thread_id", {"chat_id": 1, "topic_name": "t", "thread_id": 5},
   {}, {"get_hermes_home": sc("/home"), "config_exists": sc(True),
        "read_config": sc(CFG_MATCH2), "logger_info": sc(None)})
mk("_persist_dm_topic_thread_id", {"chat_id": 1, "topic_name": "t", "thread_id": 9},
   {}, {"get_hermes_home": sc("/home"), "config_exists": sc(True),
        "read_config": sc(raise_type="OSError", raise_msg="disk"), "logger_warning": sc(None)})

# ── _photo_batch_key ────────────────────────────────────────────────────────
for sk, suf, mgid in [("s1", "", "ag1"), ("s2", ":h", "ag2"), ("s3", ":h", None),
                      ("s4", "", "ag4"), ("", ":z", "ag5"), ("s6", ":q", 0)]:
    mk("_photo_batch_key", {"event": {"e": 1}, "msg": {"media_group_id": mgid}}, {},
       {"event_session_key": sc(sk), "sender_suffix": sc(suf)})

# ── _picker_edit ────────────────────────────────────────────────────────────
for txt, toast_v, raise_t in [("Hi", None, None), ("*Hi*", "T", None),
                              ("A", None, "TelegramError"), ("B", "T", "TelegramError"),
                              ("C", None, "ValueError"), ("D", "T2", None)]:
    io = {"answer_callback": sc(None), "format_message": sc("fmt:" + txt),
          "edit_message_text": sc(None)}
    if raise_t:
        io["edit_message_text"] = sc(raise_type=raise_t, raise_msg="boom")
        io["redact_error"] = sc("red")
        io["logger_warning"] = sc(None)
    mk("_picker_edit", {"query": {"id": 7}, "text_md": txt, "keyboard": ["KB"], "toast": toast_v}, {}, io)

# ── EXTRA 1: _should_attempt_rich_draft ─────────────────────────────────────
for en, dr, sd, dd, ok in [(True, True, False, False, True),
                           (False, True, False, False, True),
                           (True, False, False, False, True),
                           (True, True, True, False, True),
                           (True, True, False, True, True),
                           (True, True, False, False, False)]:
    mk("_should_attempt_rich_draft", {"content": "c"},
       {"_rich_messages_enabled": en, "_rich_drafts_enabled": dr,
        "_rich_send_disabled": sd, "_rich_draft_disabled": dd},
       {"rich_content_ok": sc(ok)})

# ── EXTRA 2: _restart_task_attr ─────────────────────────────────────────────
mk("_restart_task_attr", {"attr": "_t", "coro": "C"}, {}, {"ensure_future": sc("nt")})
mk("_restart_task_attr", {"attr": "_t", "coro": "C"},
   {"_t": {"done": False}}, {"task_done": sc(False), "task_cancel": sc(None), "ensure_future": sc("nt")})
mk("_restart_task_attr", {"attr": "_t", "coro": "C"},
   {"_t": {"done": True}}, {"task_done": sc(True), "ensure_future": sc("nt")})
mk("_restart_task_attr", {"attr": "_t", "coro": "C"},
   {"_t": None}, {"ensure_future": sc("nt")})
mk("_restart_task_attr", {"attr": "_other", "coro": "C"}, {},
   {"ensure_future": sc(raise_type="RuntimeError", raise_msg="ef")})
mk("_restart_task_attr", {"attr": "_t", "coro": "C"},
   {"_t": {"done": False}}, {"task_done": sc(False), "task_cancel": sc("x"), "ensure_future": sc("nt2")})


assert ALL_IO == set(SEAM_NAMES), (sorted(set(SEAM_NAMES) - ALL_IO), sorted(ALL_IO - set(SEAM_NAMES)))
assert ALL_FUNCS == {
    "_is_user_authorized_from_message", "_legacy_runner_auth_fn", "_log_blocked_user",
    "_menu_fingerprint", "_merge_into_pending", "_message_mentions_bot",
    "_notify_clarify_expired", "_observe_unmentioned_group_message", "_on_platform_update",
    "_orphan_and_rebuild_polling_client", "_paged_keyboard", "_persist_dm_topic_thread_id",
    "_photo_batch_key", "_picker_edit", "_should_attempt_rich_draft", "_restart_task_attr",
}
assert len(CASES) >= 16 * 6

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        c = dict(c)
        c.pop("_label", None)
        f.write(json.dumps(c, ensure_ascii=False) + "\n")
print("wrote", len(CASES), "cases to", OUT)
