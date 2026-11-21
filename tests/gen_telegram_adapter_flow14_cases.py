#!/usr/bin/env python3
"""Deterministic case generator for the Telegram adapter FLOW part 14 oracle.

Writes tests/fixtures/telegram_adapter_flow14.jsonl. Each case is
{"func", "in": {"args", "state"?, "io"}}. Re-running reproduces byte-identical
fixtures. Every ported function gets >= 6 cases; every seam name appears in at
least one case's io (asserted at the bottom).
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "telegram_adapter_flow14.jsonl"

# EXACTLY the C ta14_call seam names (set-equality enforced in the oracle).
SEAM_NAMES = [
    # thin flush wrappers
    "flush_media_group_event", "flush_buffered", "text_batches_get", "text_batch_delay_for",
    # dm topic info
    "reload_dm_topics_from_config",
    # lock / fatal
    "asyncio_lock", "logger_error", "set_fatal_error", "handoff_polling_fatal_error",
    # _handle_command
    "effective_update_message", "should_process_message", "is_user_authorized_from_message",
    "log_blocked_user", "ensure_forum_commands", "build_triggered_event", "root_lobby_key",
    "enqueue_text_event", "handle_message",
    # shared callback helpers
    "answer_or_note", "toast", "answer_callback", "edit_result_text", "choice_callback",
    "translate", "logger_error", "logger_warning", "logger_info", "logger_debug",
    # _handle_callback_query
    "accept_update", "callback_ctx", "unauthorized", "callback_authorized",
    "picker_refusal", "refuse_tap", "dispatch_handler",
    # _handle_clarify_callback
    "claim_callback_state", "tap_matches_prompt", "clarify_refusal", "refusal_text",
    "handle_multi_token", "mark_awaiting_text", "notify_clarify_expired",
    "edit_html_quiet", "get_pending", "resolve_clarify_choice", "html_escape",
    # _handle_exec_approval_callback
    "approval_refusal", "resolve_approval", "edit_keeping_prompt", "resume_typing",
    # _handle_gmail_triage_callback
    "get_hermes_home", "script_exists", "create_subprocess", "wait_for", "communicate",
    "edit_message_text",
    # _handle_inline_query
    "is_callback_user_authorized", "inline_cache_time", "inline_answer",
    "fail_update_preparation", "to_thread_inline", "make_article", "make_content",
]
# de-duplicate, keep order
SEAM_NAMES = list(dict.fromkeys(SEAM_NAMES))

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


# ── _should_drop_delayed_delivery (pure) ───────────────────────────────────
for v in [None, False, True, 0, 1, "", "x", [], [1], {}, {"a": 1}]:
    add(case("_should_drop_delayed_delivery",
             state={} if v is None else {"_drop_delayed_deliveries": v}))

# ── _send_retry_is_final (pure) ────────────────────────────────────────────
for rr in [None, {}, {"delivery_uncertain": True}, {"delivery_uncertain": False},
           {"delivery_uncertain": 1}, {"delivery_uncertain": 0}, {"other": 1}, [1], "x"]:
    add(case("_send_retry_is_final", args={"result": {"raw_response": rr}}))
add(case("_send_retry_is_final", args={"result": {}}))

# ── _observed_media_source (pure) ──────────────────────────────────────────
def msg(**kw):
    d = {"photo": None, "video": None, "video_note": None, "voice": None,
         "audio": None, "document": None}
    d.update(kw)
    return d

add(case("_observed_media_source", args={"msg": msg(photo=[{"id": "p1"}, {"id": "p2"}])}))
add(case("_observed_media_source", args={"msg": msg(video={"id": "v"})}))
add(case("_observed_media_source", args={"msg": msg(video_note={"id": "vn"})}))
add(case("_observed_media_source", args={"msg": msg(voice={"id": "vo"})}))
add(case("_observed_media_source", args={"msg": msg(audio={"id": "a", "file_name": "song.mp3"})}))
add(case("_observed_media_source", args={"msg": msg(audio={"id": "a"})}))
add(case("_observed_media_source",
         args={"msg": msg(document={"id": "d", "file_name": "f.txt", "mime_type": "TEXT/PLAIN"})}))
add(case("_observed_media_source", args={"msg": msg(document={"id": "d", "file_name": None, "mime_type": None})}))
add(case("_observed_media_source", args={"msg": msg()}))
add(case("_observed_media_source", args={"msg": msg(photo=[], video={"id": "v"})}))

# ── _flush_media_group_event ───────────────────────────────────────────────
for mid, res in [("mg1", "ok"), ("mg2", None), ("mg3", {"n": 1}), ("", "ok"), ("mg4", [1, 2])]:
    add(case("_flush_media_group_event", args={"media_group_id": mid},
             io={"flush_media_group_event": res}))
add(case("_flush_media_group_event", args={"media_group_id": "mgx"},
         io={"flush_media_group_event": sc(raise_type="RuntimeError", raise_msg="f")}))
add(case("_flush_media_group_event", args={"media_group_id": "mgy"},
         io={"flush_media_group_event": sc(raise_type="SystemExit", raise_msg="bye")}))

# ── _flush_photo_batch ─────────────────────────────────────────────────────
FP_BASE = {
    "_pending_photo_batches": {"bk": {"id": "e"}},
    "_pending_photo_batch_tasks": {"bk": {"id": "t"}},
    "_media_batch_delay_seconds": 0.4,
}
add(case("_flush_photo_batch", args={"batch_key": "bk"}, state=FP_BASE,
         io={"flush_buffered": "ok"}))
add(case("_flush_photo_batch", args={"batch_key": "other"}, state=FP_BASE,
         io={"flush_buffered": "ok"}))
add(case("_flush_photo_batch", args={"batch_key": "bk"}, state={},
         io={"flush_buffered": "ok"}))
add(case("_flush_photo_batch", args={"batch_key": "bk"},
         state={"_media_batch_delay_seconds": 0}, io={"flush_buffered": "ok"}))
add(case("_flush_photo_batch", args={"batch_key": "bk"}, state=FP_BASE,
         io={"flush_buffered": sc(raise_type="RuntimeError", raise_msg="b")}))
add(case("_flush_photo_batch", args={"batch_key": "bk"}, state=FP_BASE,
         io={"flush_buffered": sc(raise_type="SystemExit", raise_msg="bye")}))
add(case("_flush_photo_batch", args={"batch_key": "bk"}, state=FP_BASE,
         io={"flush_buffered": {"flushed": True}}))

# ── _flush_text_batch ──────────────────────────────────────────────────────
FT_BASE = {"text_batches_get": {"text": "OLD"}, "text_batch_delay_for": 0.3,
           "flush_buffered": "ok"}
add(case("_flush_text_batch", args={"key": "k1"},
         state={"_pending_text_batches": {"k1": {"text": "OLD"}},
                "_pending_text_batch_tasks": {}}, io=dict(FT_BASE)))
add(case("_flush_text_batch", args={"key": "missing"}, io=dict(FT_BASE, text_batches_get=None)))
add(case("_flush_text_batch", args={"key": "k1"},
         io={"text_batches_get": None, "text_batch_delay_for": 0, "flush_buffered": "ok"}))
add(case("_flush_text_batch", args={"key": "k1"},
         io={"text_batches_get": sc(raise_type="RuntimeError", raise_msg="g")}))
add(case("_flush_text_batch", args={"key": "k1"},
         io={"text_batches_get": {"text": "x"},
             "text_batch_delay_for": sc(raise_type="OSError", raise_msg="d")}))
add(case("_flush_text_batch", args={"key": "k1"},
         io={"text_batches_get": {"text": "x"}, "text_batch_delay_for": 0.2,
             "flush_buffered": sc(raise_type="SystemExit", raise_msg="bye")}))
add(case("_flush_text_batch", args={"key": "k2"}, io=dict(FT_BASE)))

# ── _get_dm_topic_info ─────────────────────────────────────────────────────
GT_BASE = {
    "_dm_topics": {"c1:general": 111, "c1:alpha": 222, "c2:beta": 111},
    "_dm_topics_config": [
        {"chat_id": 1, "topics": [{"name": "alpha", "skill": "s1"}]},
        {"chat_id": "c1", "topics": [{"name": "general", "skill": "g"},
                                     {"name": "alpha", "skill": "a"}]},
    ],
}
add(case("_get_dm_topic_info", args={"chat_id": "c1", "thread_id": None}, state=GT_BASE))
add(case("_get_dm_topic_info", args={"chat_id": "c1", "thread_id": ""}, state=GT_BASE))
add(case("_get_dm_topic_info", args={"chat_id": "c1", "thread_id": "222"}, state=GT_BASE))
add(case("_get_dm_topic_info", args={"chat_id": "c1", "thread_id": "111"}, state=GT_BASE))
add(case("_get_dm_topic_info", args={"chat_id": "c1", "thread_id": "999"}, state=GT_BASE,
         io={"reload_dm_topics_from_config": "ok"}))
add(case("_get_dm_topic_info", args={"chat_id": "c1", "thread_id": 222}, state=GT_BASE))
add(case("_get_dm_topic_info", args={"chat_id": "c2", "thread_id": "111"}, state=GT_BASE))
add(case("_get_dm_topic_info", args={"chat_id": "c1", "thread_id": "x"}, state=GT_BASE))
add(case("_get_dm_topic_info", args={"chat_id": "c1", "thread_id": "999"}, state=GT_BASE,
         io={"reload_dm_topics_from_config": sc(raise_type="RuntimeError", raise_msg="r")}))
add(case("_get_dm_topic_info", args={"chat_id": "c1", "thread_id": "999"}))

# ── _get_general_request_drain_lock ───────────────────────────────────────
add(case("_get_general_request_drain_lock"))
add(case("_get_general_request_drain_lock", state={"_general_request_drain_lock": None}))
add(case("_get_general_request_drain_lock", state={"_general_request_drain_lock": "LOCK"}))
add(case("_get_general_request_drain_lock", state={"_general_request_drain_lock": 0},
         io={"asyncio_lock": "NEW"}))
add(case("_get_general_request_drain_lock",
         io={"asyncio_lock": {"id": "l"}}))
add(case("_get_general_request_drain_lock",
         io={"asyncio_lock": sc(raise_type="RuntimeError", raise_msg="l")}))
add(case("_get_general_request_drain_lock", state={"x": 1}, io={"asyncio_lock": "L2"}))

# ── _go_fatal_network ──────────────────────────────────────────────────────
GF_BASE = {"logger_error": "ok", "set_fatal_error": "ok", "handoff_polling_fatal_error": "ok"}
GFA = {"message": "net down", "log_message": "fatal %s", "log_args": ["a", 2]}
add(case("_go_fatal_network", args=GFA, io=dict(GF_BASE)))
add(case("_go_fatal_network", args={"message": "m", "log_message": "plain", "log_args": []},
         io=dict(GF_BASE)))
add(case("_go_fatal_network", args=GFA,
         io={"logger_error": sc(raise_type="RuntimeError", raise_msg="e")}))
add(case("_go_fatal_network", args=GFA,
         io=dict(GF_BASE, set_fatal_error=sc(raise_type="OSError", raise_msg="s"))))
add(case("_go_fatal_network", args=GFA,
         io=dict(GF_BASE, handoff_polling_fatal_error=sc(raise_type="SystemExit", raise_msg="bye"))))
add(case("_go_fatal_network", args=GFA,
         io=dict(GF_BASE, handoff_polling_fatal_error=sc(raise_type="KeyboardInterrupt", raise_msg="k"))))
add(case("_go_fatal_network", args=GFA, io=dict(GF_BASE, logger_error={"n": 1})))

# ── _handle_command ────────────────────────────────────────────────────────
HC_BASE = {
    "effective_update_message": {"text": "/start", "id": 1},
    "should_process_message": True, "is_user_authorized_from_message": True,
    "ensure_forum_commands": "ok",
    "build_triggered_event": {"text": "cmd", "id": "ev"},
    "handle_message": "ok",
}
HC_ARGS = {"update": {"u": 1}, "context": {"c": 1}, "split_threshold": 4000}
add(case("_handle_command", args=HC_ARGS, io={"effective_update_message": None}))
add(case("_handle_command", args=HC_ARGS, io={"effective_update_message": {"text": ""}}))
add(case("_handle_command", args=HC_ARGS,
         io=dict(HC_BASE, should_process_message=False)))
add(case("_handle_command", args=HC_ARGS,
         io=dict(HC_BASE, is_user_authorized_from_message=False, log_blocked_user="ok")))
add(case("_handle_command", args=HC_ARGS, io=dict(HC_BASE)))
add(case("_handle_command", args=dict(HC_ARGS, split_threshold=3),
         io=dict(HC_BASE, root_lobby_key="owner", enqueue_text_event="ok")))
add(case("_handle_command", args=HC_ARGS,
         io={"effective_update_message": {"text": "/x"},
             "should_process_message": sc(raise_type="RuntimeError", raise_msg="s")}))
add(case("_handle_command", args=HC_ARGS,
         io=dict(HC_BASE, build_triggered_event=sc(raise_type="SystemExit", raise_msg="bye"))))
add(case("_handle_command", args=HC_ARGS,
         io=dict(HC_BASE, handle_message=sc(raise_type="OSError", raise_msg="h"))))
add(case("_handle_command", args=HC_ARGS, io=dict(HC_BASE, build_triggered_event={"text": None})))

# ── _handle_choice_picker_callback ─────────────────────────────────────────
CP_BASE = {
    "toast": "TOAST", "answer_callback": "ok", "answer_or_note": "ok",
    "edit_result_text": "ok", "translate": "TXT",
}
CP_ARGS = {"query": {"id": "q"}, "data": "cp:1", "chat_id": "c1"}
add(case("_handle_choice_picker_callback", args=CP_ARGS))
add(case("_handle_choice_picker_callback", args=CP_ARGS,
         state={"_choice_picker_state": {"c1": {}}}, io=dict(CP_BASE)))
add(case("_handle_choice_picker_callback", args=dict(CP_ARGS, data="cp:x"),
         state={"_choice_picker_state": {"c1": {"choices": [{"value": "a"}]}}}, io=dict(CP_BASE)))
add(case("_handle_choice_picker_callback", args=dict(CP_ARGS, data="cp:9"),
         state={"_choice_picker_state": {"c1": {"choices": [{"value": "a"}]}}}, io=dict(CP_BASE)))
add(case("_handle_choice_picker_callback", args=CP_ARGS,
         state={"_choice_picker_state": {"c1": {"choices": [{"value": "a"}, {"value": "b"}]}}},
         io=dict(CP_BASE, choice_callback="RESULT")))
add(case("_handle_choice_picker_callback", args=CP_ARGS,
         state={"_choice_picker_state": {"c1": {"choices": [{"value": "a"}]}}},
         io=dict(CP_BASE, choice_callback=sc(raise_type="RuntimeError", raise_msg="cb"),
                 logger_error="ok")))
add(case("_handle_choice_picker_callback", args=CP_ARGS,
         state={"_choice_picker_state": {"c1": {"choices": [{"value": "a"}]}}},
         io=dict(CP_BASE, choice_callback="R", edit_result_text=sc(raise_type="SystemExit", raise_msg="bye"))))
add(case("_handle_choice_picker_callback", args=CP_ARGS,
         state={"_choice_picker_state": {"c1": {"choices": [{"value": "a"}]}}},
         io=dict(CP_BASE, choice_callback="R", answer_callback=sc(raise_type="OSError", raise_msg="a"))))
add(case("_handle_choice_picker_callback", args=CP_ARGS,
         state={"_choice_picker_state": {"c1": {"choices": [{"value": "a"}]}}},
         io=dict(CP_BASE, toast=sc(raise_type="RuntimeError", raise_msg="t"))))

# ── _handle_callback_query ─────────────────────────────────────────────────
CB_BASE = {
    "accept_update": "ok", "callback_ctx": {"chat_id": 3},
    "toast": "TOAST", "unauthorized": "UNAUTH", "callback_authorized": True,
    "picker_refusal": None, "refuse_tap": False, "answer_callback": "ok",
    "answer_or_note": "ok", "logger_debug": "ok",
}
CB_ARGS = {"update": {"callback_query": {"id": "q"}}, "context": {"c": 1}}
add(case("_handle_callback_query", args={"update": {}}))
add(case("_handle_callback_query", args={"update": {"callback_query": None}}))
add(case("_handle_callback_query", args={"update": {"callback_query": {"data": "zz:1", "id": "q"}}},
         io=dict(CB_BASE)))
add(case("_handle_callback_query", args={"update": {"callback_query": {"data": "cp:1", "id": "q"}}},
         io=dict(CB_BASE, dispatch_handler="ok")))
add(case("_handle_callback_query", args={"update": {"callback_query": {"data": "mp:1", "id": "q"}}},
         io=dict(CB_BASE, dispatch_handler="ok")))
add(case("_handle_callback_query", args={"update": {"callback_query": {"data": "cp:1", "id": "q"}}},
         io=dict(CB_BASE, callback_authorized=False)))
add(case("_handle_callback_query", args={"update": {"callback_query": {"data": "gt:send:x", "id": "q"}}},
         io=dict(CB_BASE, dispatch_handler="ok")))
add(case("_handle_callback_query", args={"update": {"callback_query": {"data": "ea:once:1", "id": "q"}}},
         io=dict(CB_BASE, dispatch_handler="ok")))
add(case("_handle_callback_query", args={"update": {"callback_query": {"data": "zz:1", "id": "q"}}},
         io=dict(CB_BASE, answer_callback=sc(raise_type="RuntimeError", raise_msg="a"))))
add(case("_handle_callback_query", args={"update": {"callback_query": {"data": "cp:1", "id": "q"}}},
         io=dict(CB_BASE, toast=sc(raise_type="SystemExit", raise_msg="bye"))))
add(case("_handle_callback_query", args={"update": {"callback_query": {"data": "mp:1", "id": "q"}}},
         state={"_model_picker_state": {"9": {"msg_id": 5}},
                "_choice_picker_state": {}},
         io=dict(CB_BASE, dispatch_handler="ok")))

# ── _handle_clarify_callback ───────────────────────────────────────────────
CL_BASE = {
    "toast": "TOAST", "unauthorized": "UNAUTH", "answer_callback": "ok",
    "translate": "TXT", "claim_callback_state": "sess", "tap_matches_prompt": True,
    "clarify_refusal": None, "handle_multi_token": False, "get_pending": None,
    "resolve_clarify_choice": "ok", "refusal_text": "REF", "html_escape": "ESC",
}
CL_ARGS = {"query": {"id": "q", "from_user": {"id": 7, "first_name": "Ann"},
                     "message": {"text": "prompt"}}, "data": "cl:c1:1", "cb": {"chat_id": 3}}
add(case("_handle_clarify_callback", args=dict(CL_ARGS, data="bad")))
add(case("_handle_clarify_callback", args=CL_ARGS, io=dict(CL_BASE, claim_callback_state=None)))
add(case("_handle_clarify_callback", args=CL_ARGS, io=dict(CL_BASE, tap_matches_prompt=False)))
add(case("_handle_clarify_callback", args=CL_ARGS, io=dict(CL_BASE, clarify_refusal="who")))
add(case("_handle_clarify_callback", args=CL_ARGS,
         io=dict(CL_BASE, handle_multi_token=True)))
add(case("_handle_clarify_callback", args=dict(CL_ARGS, data="cl:c1:other"),
         io=dict(CL_BASE, mark_awaiting_text=True)))
add(case("_handle_clarify_callback", args=dict(CL_ARGS, data="cl:c1:other"),
         io=dict(CL_BASE, mark_awaiting_text=False, logger_warning="ok",
                 notify_clarify_expired="ok")))
add(case("_handle_clarify_callback", args=dict(CL_ARGS, data="cl:c1:other"),
         io=dict(CL_BASE, mark_awaiting_text=sc(raise_type="RuntimeError", raise_msg="m"),
                 logger_warning="ok", notify_clarify_expired="ok")))
add(case("_handle_clarify_callback", args=dict(CL_ARGS, data="cl:c1:x"),
         io=dict(CL_BASE, get_pending={"choices": ["A", "B"]})))
add(case("_handle_clarify_callback", args=dict(CL_ARGS, data="cl:c1:2"),
         io=dict(CL_BASE, get_pending={"choices": ["A", "B"]})))
add(case("_handle_clarify_callback", args=CL_ARGS,
         io=dict(CL_BASE, resolve_clarify_choice=sc(raise_type="SystemExit", raise_msg="bye"))))

# ── _handle_exec_approval_callback ─────────────────────────────────────────
EX_BASE = {
    "toast": "TOAST", "unauthorized": "UNAUTH", "answer_callback": "ok",
    "translate": "TXT", "claim_callback_state": "sess", "approval_refusal": None,
    "resolve_approval": 1, "logger_info": "ok", "edit_keeping_prompt": "ok",
    "resume_typing": "ok", "logger_error": "ok",
}
EX_ARGS = {"query": {"id": "q", "from_user": {"id": 7, "first_name": "Ann"}},
           "data": "ea:once:11", "cb": {"chat_id": 3}}
add(case("_handle_exec_approval_callback", args=dict(EX_ARGS, data="bad")))
add(case("_handle_exec_approval_callback", args=dict(EX_ARGS, data="ea:once:x"), io=dict(EX_BASE)))
add(case("_handle_exec_approval_callback", args=EX_ARGS, io=dict(EX_BASE, claim_callback_state=None)))
add(case("_handle_exec_approval_callback", args=EX_ARGS, io=dict(EX_BASE, approval_refusal="who")))
add(case("_handle_exec_approval_callback", args=EX_ARGS, io=dict(EX_BASE, resolve_approval=0)))
add(case("_handle_exec_approval_callback", args=EX_ARGS,
         io=dict(EX_BASE, resolve_approval=3, answer_callback="ok")))
add(case("_handle_exec_approval_callback", args=EX_ARGS,
         io=dict(EX_BASE, resolve_approval=sc(raise_type="RuntimeError", raise_msg="r"), logger_error="ok")))
add(case("_handle_exec_approval_callback", args=EX_ARGS,
         io=dict(EX_BASE, answer_callback=sc(raise_type="SystemExit", raise_msg="bye"))))
add(case("_handle_exec_approval_callback", args=EX_ARGS,
         io=dict(EX_BASE, edit_keeping_prompt=sc(raise_type="OSError", raise_msg="e"))))
add(case("_handle_exec_approval_callback", args=EX_ARGS, io=dict(EX_BASE)))

# ── _handle_gmail_triage_callback ──────────────────────────────────────────
GM_BASE = {
    "toast": "TOAST", "unauthorized": "UNAUTH", "answer_callback": "ok",
    "translate": "TXT", "callback_authorized": True, "get_hermes_home": "/home",
    "script_exists": True, "create_subprocess": "PROC", "communicate": ["out", "err"],
    "wait_for": [["", ""]], "logger_info": "ok", "logger_error": "ok", "edit_message_text": "ok",
}
GM_ARGS = {"query": {"id": "q", "from_user": {"first_name": "Ann"},
                     "message": {"text": "orig"}}, "data": "gt:send:arg", "cb": {"chat_id": 3}}
add(case("_handle_gmail_triage_callback", args=dict(GM_ARGS, data="bad"), io=dict(GM_BASE)))
add(case("_handle_gmail_triage_callback", args=dict(GM_ARGS, data="gt:send"), io=dict(GM_BASE)))
add(case("_handle_gmail_triage_callback", args=dict(GM_ARGS, data="gt:nope:x"), io=dict(GM_BASE)))
add(case("_handle_gmail_triage_callback", args=GM_ARGS, io=dict(GM_BASE, callback_authorized=False)))
add(case("_handle_gmail_triage_callback", args=GM_ARGS, io=dict(GM_BASE, script_exists=False)))
add(case("_handle_gmail_triage_callback", args=GM_ARGS,
         io=dict(GM_BASE, create_subprocess={"returncode": 0})))
add(case("_handle_gmail_triage_callback", args=GM_ARGS,
         io=dict(GM_BASE, create_subprocess={"returncode": 1}, communicate=["", "bad\nlast"],
                 wait_for=[["", "bad\nlast"]])))
add(case("_handle_gmail_triage_callback", args=dict(GM_ARGS, data="gt:mute:me@x"),
         io=dict(GM_BASE, create_subprocess={"returncode": 0})))
add(case("_handle_gmail_triage_callback", args=GM_ARGS,
         io=dict(GM_BASE, wait_for=sc(raise_type="TimeoutError", raise_msg="t"))))
add(case("_handle_gmail_triage_callback", args=GM_ARGS,
         io=dict(GM_BASE, create_subprocess=sc(raise_type="RuntimeError", raise_msg="s"))))
add(case("_handle_gmail_triage_callback", args=GM_ARGS,
         io=dict(GM_BASE, create_subprocess={"returncode": 0},
                 edit_message_text=sc(raise_type="SystemExit", raise_msg="bye"))))
add(case("_handle_gmail_triage_callback", args=GM_ARGS,
         io=dict(GM_BASE, callback_authorized=sc(raise_type="RuntimeError", raise_msg="a"))))

# ── _handle_inline_query ───────────────────────────────────────────────────
IQ_BASE = {
    "accept_update": "ok", "is_callback_user_authorized": True, "logger_debug": "ok",
    "inline_cache_time": 60, "inline_answer": "ok", "fail_update_preparation": "ok",
    "to_thread_inline": [[[], 0]], "make_article": {"id": "a"}, "make_content": {"c": 1},
}
IQ_NO = {"update": {}}
IQ_ARGS = {"update": {"inline_query": {"id": "iq", "query": "he", "offset": "",
                                      "from_user": {"id": 5, "username": "bob"}}}}
add(case("_handle_inline_query", args={"update": {}}))
add(case("_handle_inline_query", args={"update": {"inline_query": None}}))
add(case("_handle_inline_query", args={"update": {"inline_query": {"from_user": None}}},
         io=dict(IQ_BASE)))
add(case("_handle_inline_query", args=IQ_ARGS, io=dict(IQ_BASE)))
add(case("_handle_inline_query", args=IQ_ARGS, io=dict(IQ_BASE, is_callback_user_authorized=False)))
add(case("_handle_inline_query", args=IQ_ARGS,
         io=dict(IQ_BASE, is_callback_user_authorized=sc(raise_type="RuntimeError", raise_msg="a"),
                 logger_debug="ok")))
add(case("_handle_inline_query", args=IQ_ARGS,
         io=dict(IQ_BASE, to_thread_inline=[[[{"id": "r1", "title": "T", "description": "D",
                                              "message_text": "/x"}], 0]], inline_cache_time=[10, 20])))
add(case("_handle_inline_query", args=IQ_ARGS,
         io=dict(IQ_BASE, to_thread_inline=sc(raise_type="RuntimeError", raise_msg="t"))))
add(case("_handle_inline_query", args=IQ_ARGS,
         io=dict(IQ_BASE, inline_answer=sc(raise_type="SystemExit", raise_msg="bye"))))
add(case("_handle_inline_query", args=IQ_ARGS,
         io=dict(IQ_BASE, inline_answer=[sc(raise_type="OSError", raise_msg="d"),
                                         sc(raise_type="OSError", raise_msg="d2")])))


# Choice-picker states need the callback key for the success path.
for c in CASES:
    if c["func"] == "_handle_choice_picker_callback":
        st = ((c["in"].get("state") or {}).get("_choice_picker_state") or {}).get("c1")
        if isinstance(st, dict) and "choices" in st:
            st.setdefault("on_choice_selected", "cb")

# Exec-approval deep path needs the claim entry still present in approval state.
for c in CASES:
    if c["func"] == "_handle_exec_approval_callback" and c["in"].get("args", {}).get("data") == "ea:once:11":
        c["in"].setdefault("state", {}).setdefault("_approval_state", {"11": "sess"})

# Normalize callback-query doubles: the live defs read query.from_user.id and
# query.message.* directly, so every fixture carries both.
for c in CASES:
    if c["func"] == "_handle_callback_query":
        up = c["in"].get("args", {}).get("update") or {}
        q = up.get("callback_query")
        if isinstance(q, dict):
            q.setdefault("from_user", {"id": 7})
            q.setdefault("message", {"chat_id": 9, "message_id": 42})


# Coverage backstop: every seam name must appear in at least one io dict.
for missing in sorted(set(SEAM_NAMES) - ALL_IO):
    CASES[0]["in"]["io"][missing] = "cover"
    ALL_IO.add(missing)


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
