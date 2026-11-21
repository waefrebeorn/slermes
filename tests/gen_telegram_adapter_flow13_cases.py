#!/usr/bin/env python3
"""Deterministic case generator for the Telegram adapter FLOW part 13 oracle.

Writes tests/fixtures/telegram_adapter_flow13.jsonl. Each case is
{"func", "in": {"args", "state"?, "io"}}. Re-running reproduces byte-identical
fixtures. Every ported function gets >= 6 cases; every seam name appears in at
least one case's io (asserted at the bottom).
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "telegram_adapter_flow13.jsonl"

SEAM_NAMES = [
    "req_shutdown", "bounded_request_step", "orphan_and_rebuild_polling_client",
    "req_initialize", "logger_debug",
    "admission_get", "claim_set_failed",
    "normalize_telegram_chat_id", "link_preview_kwargs", "edit_message_text",
    "await_with_thread_deadline",
    "edit_text", "redact", "logger_warning",
    "edit_overflow_split",
    "format_message", "query_edit_message_text",
    "current_bot_username", "foreign_command_target", "leading_bot_handles",
    "foreign_handle_fullmatch", "extract_bot_mention_usernames",
    "schedule_bot_identity_recheck", "chat_id_str", "logger_info",
    "forum_lock_aenter", "menu_max_commands", "to_thread", "bot_commands",
    "set_my_commands", "forum_lock_aexit",
    "note_arrival", "should_drop_delayed_delivery", "hold_inbound_event",
    "text_batch_key", "merged_typed_text", "super_enqueue_text_event",
    "record_typed_text", "accept_update",
    "merge_into_pending", "task_done", "task_cancel", "flush_photo_batch",
    "create_task",
    "current_task", "sleep", "asyncio_wait", "log_fn", "handle_message",
    "edit_timestamp", "sha256_hexdigest",
    "super_text_batch_key", "shared_sender_suffix",
    "super_mark_connected", "schedule_held_inbound_redispatch",
    "super_mark_disconnected",
]

CASES = []
ALL_IO = set()
ALL_FUNCS = set()


def case(func, args=None, state=None, io=None):
    c = {"func": func, "in": {"args": args or {}}}
    if state is not None:
        c["in"]["state"] = state
    c["in"]["io"] = io if io is not None else {}
    return c


def sc(ret=None, raise_type=None, raise_msg=""):
    d = {"__script__": True}
    if raise_type is not None:
        d["raise"] = {"type": raise_type, "message": raise_msg}
    else:
        d["ret"] = ret
    return d


def add(c):
    ALL_IO.update((c["in"].get("io") or {}).keys())
    ALL_FUNCS.add(c["func"])
    CASES.append(c)


# ── _drain_polling_connections ──────────────────────────────────────────────
OK_DRAIN = {
    "req_shutdown": "SH", "req_initialize": "IN",
    "bounded_request_step": [True, True], "logger_debug": "ok",
}
add(case("_drain_polling_connections", args={"app_kind": "none"}))
add(case("_drain_polling_connections", args={"app_kind": "no_bot"}))
add(case("_drain_polling_connections", args={"app_kind": "ok", "req_kind": "missing"}))
add(case("_drain_polling_connections", args={"app_kind": "ok", "req": {"id": 1}}, io=dict(OK_DRAIN)))
add(case("_drain_polling_connections", args={"app_kind": "ok", "req": {"id": 2}}, io={
    "req_shutdown": "SH", "bounded_request_step": [False, True], "req_initialize": "IN",
    "orphan_and_rebuild_polling_client": "ok", "logger_debug": "ok"}))
add(case("_drain_polling_connections", args={"app_kind": "ok", "req": {"id": 3}}, io={
    "req_shutdown": "SH", "bounded_request_step": [True, False], "req_initialize": "IN",
    "orphan_and_rebuild_polling_client": "ok"}))
add(case("_drain_polling_connections", args={"app_kind": "ok", "req": {"id": 4}}, io={
    "req_shutdown": sc(raise_type="RuntimeError", raise_msg="sh")}))
add(case("_drain_polling_connections", args={"app_kind": "ok", "req": {"id": 5}}, io={
    "req_shutdown": "SH", "bounded_request_step": sc(raise_type="OSError", raise_msg="b")}))
add(case("_drain_polling_connections", args={"app_kind": "ok", "req": {"id": 6}}, io={
    "req_shutdown": "SH", "bounded_request_step": [True, True], "req_initialize": sc(raise_type="SystemExit", raise_msg="bye")}))
add(case("_drain_polling_connections", args={"app_kind": "ok", "req": {"id": 7}}, io={
    "req_shutdown": "SH", "bounded_request_step": [False, False], "req_initialize": "IN",
    "orphan_and_rebuild_polling_client": ["o1", "o2"]}))

# ── _fail_update_preparation ───────────────────────────────────────────────
add(case("_fail_update_preparation"))
add(case("_fail_update_preparation", state={"_update_admission": None}))
add(case("_fail_update_preparation", state={"_update_admission": {}},
         io={"admission_get": None}))
for claim in [{"id": "c1"}, {"id": "c2", "n": 3}, "claim-str", [1]]:
    add(case("_fail_update_preparation", state={"_update_admission": {}},
             io={"admission_get": claim, "claim_set_failed": "ok"}))
add(case("_fail_update_preparation", state={"_update_admission": {}},
         io={"admission_get": sc(raise_type="RuntimeError", raise_msg="g")}))
add(case("_fail_update_preparation", state={"_update_admission": {}},
         io={"admission_get": {"id": "x"}, "claim_set_failed": sc(raise_type="OSError", raise_msg="s")}))
add(case("_fail_update_preparation", state={"_update_admission": {}},
         io={"admission_get": [None, {"id": "y"}], "claim_set_failed": "ok"}))

# ── _edit_text ──────────────────────────────────────────────────────────────
BASE_EDIT = {
    "normalize_telegram_chat_id": "norm-chat", "link_preview_kwargs": {},
    "edit_message_text": "AWAIT", "await_with_thread_deadline": "ok",
}
for pm in [None, "MD", "", "HTML"]:
    io = dict(BASE_EDIT)
    add(case("_edit_text", args={"chat_id": "raw", "message_id": 100,
                                 "text": "hello", "parse_mode": pm}, io=io))
add(case("_edit_text", args={"chat_id": "raw", "message_id": 101, "text": "t"}, io={
    "normalize_telegram_chat_id": "norm-chat",
    "link_preview_kwargs": {"disable_link_previews": True},
    "edit_message_text": "AWAIT", "await_with_thread_deadline": "ok"}))
add(case("_edit_text", args={"chat_id": "c", "message_id": 102, "text": "t"}, io={
    "normalize_telegram_chat_id": sc(raise_type="ValueError", raise_msg="norm")}))
add(case("_edit_text", args={"chat_id": "c", "message_id": 103, "text": "t"}, io={
    "normalize_telegram_chat_id": "n",
    "link_preview_kwargs": sc(raise_type="RuntimeError", raise_msg="lpk")}))
add(case("_edit_text", args={"chat_id": "c", "message_id": 104, "text": "t"}, io={
    "normalize_telegram_chat_id": "n", "link_preview_kwargs": {},
    "edit_message_text": sc(raise_type="OSError", raise_msg="emt")}))
add(case("_edit_text", args={"chat_id": "c", "message_id": 105, "text": "t"}, io={
    "normalize_telegram_chat_id": "n", "link_preview_kwargs": {},
    "edit_message_text": "A", "await_with_thread_deadline": sc(raise_type="SystemExit", raise_msg="bye")}))

# ── _edit_markdown_or_plain ─────────────────────────────────────────────────
EM_ARGS = {"chat_id": "c", "message_id": 1, "formatted": "FMT", "plain": "PLAIN",
           "warn_fmt": "WARN %s"}
add(case("_edit_markdown_or_plain", args=EM_ARGS,
         io={"edit_text": "ok"}))
for kind, msg in [("RuntimeError", "Message_not_modified"), ("ValueError", "NOT MODIFIED")]:
    add(case("_edit_markdown_or_plain", args=EM_ARGS,
             io={"edit_text": sc(raise_type=kind, raise_msg=msg)}))
add(case("_edit_markdown_or_plain", args=EM_ARGS,
         io={"edit_text": sc(raise_type="RuntimeError", raise_msg="retry after 5")}))
add(case("_edit_markdown_or_plain", args=EM_ARGS,
         io={"edit_text": [sc(raise_type="RuntimeError", raise_msg="bad fmt"), "ok"],
             "redact": "RED", "logger_warning": "ok"}))
add(case("_edit_markdown_or_plain", args=EM_ARGS,
         io={"edit_text": [sc(raise_type="RuntimeError", raise_msg="bad"), sc(raise_type="SystemExit", raise_msg="bye")],
             "redact": "RED", "logger_warning": "ok"}))
add(case("_edit_markdown_or_plain", args=EM_ARGS,
         io={"edit_text": sc(raise_type="SystemExit", raise_msg="bye")}))
add(case("_edit_markdown_or_plain", args=EM_ARGS,
         io={"edit_text": sc(raise_type="RuntimeError", raise_msg="boom"),
             "redact": sc(raise_type="OSError", raise_msg="r")}))
add(case("_edit_markdown_or_plain", args=EM_ARGS,
         io={"edit_text": sc(raise_type="RuntimeError", raise_msg="boom"),
             "redact": "RED", "logger_warning": sc(raise_type="KeyboardInterrupt", raise_msg="k")}))

# ── _edit_overflow_split ────────────────────────────────────────────────────
for i, (res, fin, meta) in enumerate([
    ({"msg": 1}, False, None), (None, True, {"a": 1}), ([1, 2], True, None),
    ("str", False, {"x": "y"}), (True, True, {}), (5, False, None),
]):
    io = {"edit_overflow_split": (res if isinstance(res, dict) and "__script__" in res else sc(ret=res))}
    add(case("_edit_overflow_split", args={"chat_id": "c", "message_id": 2,
                                           "content": "body", "finalize": fin,
                                           "metadata": meta}, io=io))
add(case("_edit_overflow_split", args={"chat_id": "c", "message_id": 2, "content": "b", "finalize": True},
         io={"edit_overflow_split": sc(raise_type="RuntimeError", raise_msg="split")}))
add(case("_edit_overflow_split", args={"chat_id": "c", "message_id": 2, "content": "b", "finalize": True},
         io={"edit_overflow_split": sc(raise_type="SystemExit", raise_msg="bye")}))

# ── _edit_result_text ───────────────────────────────────────────────────────
ERT = {"query": {"id": "q"}, "result_text": "RESULT"}
add(case("_edit_result_text", args=ERT,
         io={"format_message": "FMT", "query_edit_message_text": "ok"}))
add(case("_edit_result_text", args=ERT,
         io={"format_message": "FMT",
             "query_edit_message_text": [sc(raise_type="RuntimeError", raise_msg="bad"), "ok"]}))
add(case("_edit_result_text", args=ERT,
         io={"format_message": sc(raise_type="ValueError", raise_msg="fmt"),
             "query_edit_message_text": "ok"}))
add(case("_edit_result_text", args=ERT,
         io={"format_message": sc(raise_type="SystemExit", raise_msg="bye")}))
add(case("_edit_result_text", args=ERT,
         io={"format_message": "FMT",
             "query_edit_message_text": sc(raise_type="SystemExit", raise_msg="bye")}))
add(case("_edit_result_text", args=ERT,
         io={"format_message": "FMT",
             "query_edit_message_text": [sc(raise_type="OSError", raise_msg="x"), sc(raise_type="TypeError", raise_msg="y")]}))
add(case("_edit_result_text", args={"query": {"id": "q2"}, "result_text": "R2"},
         io={"format_message": "F2", "query_edit_message_text": "ok"}))

# ── _edit_md_quiet ──────────────────────────────────────────────────────────
EMQ = {"query": {"id": "q"}, "text_md": "MD"}
add(case("_edit_md_quiet", args=EMQ,
         io={"format_message": "FMT", "query_edit_message_text": "ok"}))
add(case("_edit_md_quiet", args=EMQ,
         io={"format_message": sc(raise_type="ValueError", raise_msg="f")}))
add(case("_edit_md_quiet", args=EMQ,
         io={"format_message": "FMT", "query_edit_message_text": sc(raise_type="OSError", raise_msg="e")}))
add(case("_edit_md_quiet", args=EMQ,
         io={"format_message": sc(raise_type="SystemExit", raise_msg="bye")}))
add(case("_edit_md_quiet", args=EMQ,
         io={"format_message": "FMT", "query_edit_message_text": sc(raise_type="KeyboardInterrupt", raise_msg="k")}))
add(case("_edit_md_quiet", args={"query": {"id": "q2"}, "text_md": "M2"},
         io={"format_message": "F2", "query_edit_message_text": "ok"}))

# ── _explicit_bot_mentions_exclude_self ────────────────────────────────────
MSG = {"message_id": 42, "text": "hello", "chat": {"id": 9}}
EB_FULL = {
    "current_bot_username": "me", "foreign_command_target": None,
    "leading_bot_handles": sc(ret=["other"]), "foreign_handle_fullmatch": True,
    "extract_bot_mention_usernames": sc(ret=[]), "schedule_bot_identity_recheck": "ok",
    "chat_id_str": "chat9", "logger_info": "ok",
}
add(case("_explicit_bot_mentions_exclude_self", args={"bot_present": False, "message": MSG}))
add(case("_explicit_bot_mentions_exclude_self", args={"message": MSG},
         io={"current_bot_username": None}))
add(case("_explicit_bot_mentions_exclude_self", args={"message": MSG},
         io={"current_bot_username": "me", "foreign_command_target": "other",
             "schedule_bot_identity_recheck": "ok", "chat_id_str": "chat9", "logger_info": "ok"}))
add(case("_explicit_bot_mentions_exclude_self", args={"message": MSG}, io=dict(EB_FULL)))
add(case("_explicit_bot_mentions_exclude_self", args={"message": MSG},
         io={"current_bot_username": "me", "foreign_command_target": None,
             "leading_bot_handles": sc(ret=["me", "other"]), "foreign_handle_fullmatch": True,
             "extract_bot_mention_usernames": sc(ret=["me"])}))
add(case("_explicit_bot_mentions_exclude_self", args={"message": MSG},
         io={"current_bot_username": "me", "foreign_command_target": None,
             "leading_bot_handles": sc(ret=[])}))
add(case("_explicit_bot_mentions_exclude_self", args={"message": MSG},
         io={"current_bot_username": sc(raise_type="RuntimeError", raise_msg="u")}))
add(case("_explicit_bot_mentions_exclude_self", args={"message": MSG},
         io={"current_bot_username": "me", "foreign_command_target": sc(raise_type="OSError", raise_msg="f")}))
add(case("_explicit_bot_mentions_exclude_self", args={"message": MSG},
         io={"current_bot_username": "me", "foreign_command_target": None,
             "leading_bot_handles": sc(ret=["other"]), "foreign_handle_fullmatch": sc(raise_type="ValueError", raise_msg="h")}))
add(case("_explicit_bot_mentions_exclude_self", args={"message": MSG, "log_skip": False},
         io=dict(EB_FULL, logger_info="unused")))

# ── _ensure_forum_commands ──────────────────────────────────────────────────
NOFORUM = {"chat": {"id": 1, "is_forum": False}}
FORUM = {"chat": {"id": 5, "is_forum": True}}
EF_OK = {
    "menu_max_commands": 10, "to_thread": sc(ret=[["c1", "c2"], None]),
    "bot_commands": sc(ret=["cmd1", "cmd2"]), "set_my_commands": "ok",
    "logger_info": "ok",
}
add(case("_ensure_forum_commands", args={"message": {}}))
add(case("_ensure_forum_commands", args={"message": NOFORUM}))
add(case("_ensure_forum_commands", args={"message": FORUM}, state={"_forum_command_registered": [1, 5, 9]}))
add(case("_ensure_forum_commands", args={"message": FORUM}, state={"_forum_command_registered": [9, 1]}, io=dict(EF_OK)))
add(case("_ensure_forum_commands", args={"message": FORUM}, io=dict(EF_OK)))
add(case("_ensure_forum_commands", args={"message": FORUM},
         io={"menu_max_commands": sc(raise_type="RuntimeError", raise_msg="m"),
             "redact": "RED", "logger_warning": "ok"}))
add(case("_ensure_forum_commands", args={"message": FORUM},
         io={"menu_max_commands": 10,
             "to_thread": sc(raise_type="SystemExit", raise_msg="bye")}))
add(case("_ensure_forum_commands", args={"message": FORUM},
         io={"menu_max_commands": 10, "to_thread": sc(ret=[["m"], None]),
             "bot_commands": sc(raise_type="OSError", raise_msg="b"),
             "redact": "RED", "logger_warning": "ok"}))
add(case("_ensure_forum_commands", args={"message": FORUM},
         io={"menu_max_commands": 10, "to_thread": sc(ret=[["m"], None]),
             "bot_commands": sc(ret=["c"]), "set_my_commands": sc(raise_type="RuntimeError", raise_msg="s"),
             "redact": "RED", "logger_warning": "ok"}))
add(case("_ensure_forum_commands", args={"message": FORUM},
         io={"menu_max_commands": sc(raise_type="RuntimeError", raise_msg="m"),
             "redact": sc(raise_type="KeyboardInterrupt", raise_msg="r")}))

# ── _enqueue_text_event ─────────────────────────────────────────────────────
EV = {"id": "e1", "text": "T"}
ET_BASE = {
    "note_arrival": "ok", "should_drop_delayed_delivery": False,
    "text_batch_key": "k1", "merged_typed_text": None,
    "super_enqueue_text_event": "ok", "accept_update": "ok",
}
add(case("_enqueue_text_event", args={"event": EV},
         io={"note_arrival": "ok", "should_drop_delayed_delivery": True,
             "hold_inbound_event": "ok"}))
add(case("_enqueue_text_event", args={"event": EV}, io=dict(ET_BASE)))
add(case("_enqueue_text_event", args={"event": EV},
         state={"_pending_text_batches": {"k1": {"text": "OLD"}}},
         io=dict(ET_BASE, merged_typed_text="NEW", record_typed_text="ok")))
add(case("_enqueue_text_event", args={"event": EV},
         state={"_pending_text_batches": {"k1": {"text": "OLD"}}},
         io=dict(ET_BASE, merged_typed_text="OLD")))
add(case("_enqueue_text_event", args={"event": EV},
         io={"note_arrival": sc(raise_type="RuntimeError", raise_msg="n")}))
add(case("_enqueue_text_event", args={"event": EV},
         io={"note_arrival": "ok", "should_drop_delayed_delivery": sc(raise_type="OSError", raise_msg="s")}))
add(case("_enqueue_text_event", args={"event": EV},
         io={"note_arrival": "ok", "should_drop_delayed_delivery": False,
             "text_batch_key": sc(raise_type="ValueError", raise_msg="k")}))
add(case("_enqueue_text_event", args={"event": EV},
         io={"note_arrival": "ok", "should_drop_delayed_delivery": False, "text_batch_key": "k1",
             "merged_typed_text": sc(raise_type="RuntimeError", raise_msg="m")}))
add(case("_enqueue_text_event", args={"event": EV},
         io={"note_arrival": "ok", "should_drop_delayed_delivery": False, "text_batch_key": "k1",
             "merged_typed_text": None, "super_enqueue_text_event": sc(raise_type="SystemExit", raise_msg="bye")}))
add(case("_enqueue_text_event", args={"event": EV},
         io=dict(ET_BASE, accept_update=sc(raise_type="OSError", raise_msg="a"))))

# ── _flush_buffered ─────────────────────────────────────────────────────────
FB_BASE = {
    "current_task": 1, "sleep": "ok", "should_drop_delayed_delivery": False,
    "handle_message": "ok", "asyncio_wait": "ok",
}
FB_PENDING = {"k1": {"id": "ev"}}
add(case("_flush_buffered", args={"pending": dict(FB_PENDING), "tasks": {}, "key": "k1",
                                  "delay": 0.5, "where": "txt", "log_fn": False}, io=dict(FB_BASE)))
add(case("_flush_buffered", args={"pending": dict(FB_PENDING), "tasks": {}, "key": "k1",
                                  "delay": 0.5, "where": "txt", "log_fn": True},
         io=dict(FB_BASE, log_fn="ok")))
add(case("_flush_buffered", args={"pending": dict(FB_PENDING), "tasks": {}, "key": "k1",
                                  "delay": 0.5, "where": "txt", "log_fn": False},
         io=dict(FB_BASE, should_drop_delayed_delivery=True, hold_inbound_event="ok")))
add(case("_flush_buffered", args={"pending": dict(FB_PENDING), "tasks": {"k1": 2}, "key": "k1",
                                  "delay": 0.5, "where": "txt"}, io=dict(FB_BASE)))
add(case("_flush_buffered", args={"pending": dict(FB_PENDING), "tasks": {"k1": 1}, "key": "k1",
                                  "delay": 0.5, "where": "txt"}, io=dict(FB_BASE)))
add(case("_flush_buffered", args={"pending": dict(FB_PENDING), "tasks": {}, "key": "k1",
                                  "delay": 0.5, "where": "txt"},
         state={"_committed_flush_tasks": {"txt|k1": 2}},
         io=dict(FB_BASE, task_done=False)))
add(case("_flush_buffered", args={"pending": dict(FB_PENDING), "tasks": {}, "key": "k1",
                                  "delay": 0.5, "where": "txt"},
         state={"_committed_flush_tasks": {"txt|k1": 2}},
         io=dict(FB_BASE, task_done=True)))
add(case("_flush_buffered", args={"pending": dict(FB_PENDING), "tasks": {}, "key": "k1",
                                  "delay": 0.5, "where": "txt"},
         io=dict(FB_BASE, handle_message=sc(raise_type="CancelledError", raise_msg="c"),
                 hold_inbound_event="ok")))
add(case("_flush_buffered", args={"pending": dict(FB_PENDING), "tasks": {}, "key": "k1",
                                  "delay": 0.5, "where": "txt"},
         io=dict(FB_BASE, handle_message=sc(raise_type="RuntimeError", raise_msg="boom"))))
add(case("_flush_buffered", args={"pending": dict(FB_PENDING), "tasks": {}, "key": "k1",
                                  "delay": 0.5, "where": "txt"},
         io={"current_task": 1, "sleep": sc(raise_type="CancelledError", raise_msg="c")}))

# ── _enqueue_photo_event ────────────────────────────────────────────────────
PE_BASE = {
    "note_arrival": "ok", "should_drop_delayed_delivery": False,
    "merge_into_pending": "bk1", "accept_update": "ok",
    "flush_photo_batch": "CORO", "create_task": {"id": "task"},
}
add(case("_enqueue_photo_event", args={"batch_key": "b", "event": EV},
         io={"note_arrival": "ok", "should_drop_delayed_delivery": True,
             "hold_inbound_event": "ok"}))
add(case("_enqueue_photo_event", args={"batch_key": "b", "event": EV}, io=dict(PE_BASE)))
add(case("_enqueue_photo_event", args={"batch_key": "b", "event": EV},
         state={"_pending_photo_batch_tasks": {"bk1": {"id": 9}}},
         io=dict(PE_BASE, task_done=True)))
add(case("_enqueue_photo_event", args={"batch_key": "b", "event": EV},
         state={"_pending_photo_batch_tasks": {"bk1": {"id": 9}}},
         io=dict(PE_BASE, task_done=False, task_cancel="ok")))
add(case("_enqueue_photo_event", args={"batch_key": "b", "event": EV},
         io={"note_arrival": "ok", "should_drop_delayed_delivery": False,
             "merge_into_pending": sc(raise_type="RuntimeError", raise_msg="m")}))
add(case("_enqueue_photo_event", args={"batch_key": "b", "event": EV},
         io=dict(PE_BASE, accept_update=sc(raise_type="OSError", raise_msg="a"))))
add(case("_enqueue_photo_event", args={"batch_key": "b", "event": EV},
         io=dict(PE_BASE, flush_photo_batch=sc(raise_type="ValueError", raise_msg="f"))))
add(case("_enqueue_photo_event", args={"batch_key": "b", "event": EV},
         io=dict(PE_BASE, create_task=sc(raise_type="SystemExit", raise_msg="bye"))))
add(case("_enqueue_photo_event", args={"batch_key": "b", "event": EV},
         state={"_pending_photo_batch_tasks": {"bk1": {"id": 9}}},
         io=dict(PE_BASE, task_done=sc(raise_type="OSError", raise_msg="d"))))

# ── _edit_revision ──────────────────────────────────────────────────────────
TS = {"__ts__": 1700000000}
add(case("_edit_revision", args={"text": "x"}))
add(case("_edit_revision", args={"edit_date": 12345, "text": "x"}))
add(case("_edit_revision", args={"edit_date": TS, "update_id": 7, "text": "x"}))
add(case("_edit_revision", args={"edit_date": TS, "update_id": 0, "text": "x"}))
add(case("_edit_revision", args={"edit_date": TS, "text": "hello", "caption": "cap"},
         io={"edit_timestamp": 1700000000, "sha256_hexdigest": "abcdef0123456789REST"}))
add(case("_edit_revision", args={"edit_date": TS, "caption": "cap-only"},
         io={"edit_timestamp": 1700000001, "sha256_hexdigest": "0123456789abcdefZZ"}))
add(case("_edit_revision", args={"edit_date": TS, "text": "hello"},
         io={"edit_timestamp": sc(raise_type="OSError", raise_msg="t")}))
add(case("_edit_revision", args={"edit_date": TS, "text": "hello"},
         io={"edit_timestamp": 1700000000, "sha256_hexdigest": sc(raise_type="RuntimeError", raise_msg="h")}))

# ── _text_batch_key ─────────────────────────────────────────────────────────
for s1, s2 in [("c1", "|s1"), ("c2", ""), ("", "|s"), ("ch:9", "|sender"),
               ("x", "|z"), ("a", ""), ("", "")]:
    add(case("_text_batch_key", args={"event": EV},
             io={"super_text_batch_key": s1, "shared_sender_suffix": s2}))
add(case("_text_batch_key", args={"event": EV},
         io={"super_text_batch_key": sc(raise_type="RuntimeError", raise_msg="s")}))
add(case("_text_batch_key", args={"event": EV},
         io={"super_text_batch_key": "a", "shared_sender_suffix": sc(raise_type="OSError", raise_msg="x")}))

# ── _mark_connected ─────────────────────────────────────────────────────────
for drop in [False, True, None]:
    add(case("_mark_connected", state={"_drop_delayed_deliveries": drop},
             io={"super_mark_connected": "ok", "schedule_held_inbound_redispatch": "ok"}))
add(case("_mark_connected",
         io={"super_mark_connected": sc(raise_type="RuntimeError", raise_msg="s")}))
add(case("_mark_connected",
         io={"super_mark_connected": "ok", "schedule_held_inbound_redispatch": sc(raise_type="OSError", raise_msg="d")}))
add(case("_mark_connected", io={"super_mark_connected": "ok", "schedule_held_inbound_redispatch": "ok"}))
add(case("_mark_connected", state={"x": 1},
         io={"super_mark_connected": "ok", "schedule_held_inbound_redispatch": "ok"}))

# ── _mark_disconnected ──────────────────────────────────────────────────────
for drop in [False, True, None]:
    add(case("_mark_disconnected", state={"_drop_delayed_deliveries": drop},
             io={"super_mark_disconnected": "ok"}))
add(case("_mark_disconnected",
         io={"super_mark_disconnected": sc(raise_type="SystemExit", raise_msg="bye")}))
add(case("_mark_disconnected", io={"super_mark_disconnected": "ok"}))
add(case("_mark_disconnected", state={"x": 2}, io={"super_mark_disconnected": "ok"}))
add(case("_mark_disconnected", io={"super_mark_disconnected": "bad"}))
add(case("_mark_disconnected", io={"super_mark_disconnected": [1, 2]}))


# The async context-manager seams must be scripted on every forum case.
for c in CASES:
    if c["func"] == "_ensure_forum_commands":
        io = c["in"].setdefault("io", {})
        io.setdefault("forum_lock_aenter", "ok")
        io.setdefault("forum_lock_aexit", "ok")
        ALL_IO.update(("forum_lock_aenter", "forum_lock_aexit"))


# ── assertions ──
counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
assert all(v >= 6 for v in counts.values()), {k: v for k, v in counts.items() if v < 6}
assert ALL_IO == set(SEAM_NAMES), ("seam coverage drift", set(SEAM_NAMES) - ALL_IO, ALL_IO - set(SEAM_NAMES))

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")
print(f"wrote {len(CASES)} cases to {OUT} ({len(ALL_FUNCS)} funcs, {len(ALL_IO)} seams)")
for f in sorted(counts):
    print(f"  {f}: {counts[f]}")
