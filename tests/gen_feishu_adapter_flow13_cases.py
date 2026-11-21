#!/usr/bin/env python3
"""Generate tests/fixtures/feishu_adapter_flow13.jsonl — input cases for the
Feishu adapter remainders FLOW port (flow13).

Every case is hand-scripted: in.io maps a seam name to a result (plain JSON
value, an ordered LIST of results, or {"__script__":true,"raise":{type,message}})
that the C universal seam and the oracle's recording stubs replay identically.
Run `python tests/gen_feishu_adapter_flow13_cases.py` to regenerate.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "feishu_adapter_flow13.jsonl"
CASES = []


def add(func, args, io=None, note=None):
    case = {"func": func, "in": {"args": args, "io": dict(io or {})}}
    if note:
        case["_note"] = note
    CASES.append(case)


def rais(kind, msg):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def ret(v):
    return {"__script__": True, "ret": v}


def rt(*vals):
    """ordered list of ret() scripts"""
    return [ret(v) for v in vals]


# ── _record_webhook_anomaly (args: remote_ip, status, prior) ──
A = {"remote_ip": "1.2.3.4", "status": "400"}
add("_record_webhook_anomaly", dict(A, prior=[0, "", 100]),
    io={"time_time": rt(100), "anomaly_set": rt(None)})
add("_record_webhook_anomaly", dict(A, prior=[24, "401", 100]),
    io={"time_time": rt(200), "anomaly_set": rt(None)})
add("_record_webhook_anomaly", {"remote_ip": "5.6.7.8", "status": "500", "prior": None},
    io={"time_time": rt(50), "anomaly_set": rt(None)})
add("_record_webhook_anomaly", {"remote_ip": "9.9.9.9", "status": "403", "prior": [30, "400", 100]},
    io={"time_time": rt(30000), "anomaly_set": rt(None)})
add("_record_webhook_anomaly", dict(A, prior=[0, "", 100]),
    io={"time_time": [rais("SystemExit", "sx")]})
add("_record_webhook_anomaly", dict(A, prior=[0, "", 100]),
    io={"time_time": rt(1), "anomaly_set": [rais("ValueError", "bad")]})
add("_record_webhook_anomaly", dict(A, prior=[0, "", 100]), io={}, note="fail-closed")


# ── _release_app_lock (args: app_lock_identity) ──
SCOPE = "feishu-app-id"
add("_release_app_lock", {"app_lock_identity": "ID"},
    io={"release_scoped_lock": rt(None), "clear_app_lock_identity": rt(None)})
add("_release_app_lock", {"app_lock_identity": ""}, io={})
add("_release_app_lock", {"app_lock_identity": "ID"},
    io={"release_scoped_lock": [rais("ValueError", "bad")], "clear_app_lock_identity": rt(None)})
add("_release_app_lock", {"app_lock_identity": "ID"},
    io={"release_scoped_lock": [rais("SystemExit", "sx")], "clear_app_lock_identity": rt(None)})
add("_release_app_lock", {"app_lock_identity": "ID"},
    io={"release_scoped_lock": rt(None), "clear_app_lock_identity": [rais("SystemExit", "sx")]})
add("_release_app_lock", {"app_lock_identity": "ID"},
    io={"release_scoped_lock": [rais("ValueError", "bad")],
        "clear_app_lock_identity": [rais("ValueError", "bad2")]})
add("_release_app_lock", {"app_lock_identity": "ID"}, io={}, note="fail-closed")


# ── _reschedule_batch_task (args: key, has_prior) ──
K = {"key": "chat1"}
add("_reschedule_batch_task", dict(K, has_prior=True),
    io={"task_done": rt(True), "flush_fn": rt(None), "create_task": rt("TASK")})
add("_reschedule_batch_task", dict(K, has_prior=True),
    io={"task_done": rt(False), "task_cancel": rt(None), "flush_fn": rt(None), "create_task": rt("TASK")})
add("_reschedule_batch_task", dict(K, has_prior=False),
    io={"flush_fn": rt(None), "create_task": rt("TASK")})
add("_reschedule_batch_task", dict(K, has_prior=True),
    io={"task_done": [rais("SystemExit", "sx")]})
add("_reschedule_batch_task", dict(K, has_prior=True),
    io={"task_done": rt(True), "flush_fn": [rais("SystemExit", "sx")]})
add("_reschedule_batch_task", {"key": "chat2", "has_prior": False},
    io={"flush_fn": rt(None), "create_task": [rais("ValueError", "bad")]})
add("_reschedule_batch_task", dict(K, has_prior=True), io={}, note="fail-closed")


# ── _reset_batch_buffers (no args) ──
OK3 = {"clear_text_batches": rt(None), "clear_text_counts": rt(None), "clear_media_batches": rt(None)}
add("_reset_batch_buffers", {}, io=OK3)
add("_reset_batch_buffers", {},
    io={"clear_text_batches": rt(None), "clear_text_counts": [rais("ValueError", "bad")]})
add("_reset_batch_buffers", {},
    io={"clear_text_batches": rt(None), "clear_text_counts": rt(None),
        "clear_media_batches": [rais("SystemExit", "sx")]})
add("_reset_batch_buffers", {}, io={"clear_text_batches": [rais("SystemExit", "sx")]})
add("_reset_batch_buffers", {},
    io={"clear_text_batches": rt(None), "clear_text_counts": [rais("SystemExit", "sx")]})
add("_reset_batch_buffers", {}, io={}, note="fail-closed")


# ── _resolve_approval ──
POP = "pop_validated_prompt_state"
STATE = {"session_key": "S1", "request_id": "R1", "chat_id": "C1"}
BASE = {"choice": "approve", "open_id": "ou1", "chat_id": "C1", "user_id": "u1",
        "union_id": "un1", "ident": "ap1", "extra": {"k": "v"}}
add("_resolve_approval", dict(BASE), io={POP: rt(None)})
add("_resolve_approval", dict(BASE, state=STATE),
    io={POP: rt(STATE), "resolve_gateway_approval_card": rt(1)})
add("_resolve_approval", dict(BASE, state=STATE),
    io={POP: rt(STATE), "resolve_gateway_approval_card": rt(0),
        "t": rt("expired notice"), "send": rt(None)})
add("_resolve_approval", dict(BASE, choice="deny", state=STATE),
    io={POP: rt(STATE), "resolve_gateway_approval_card": rt(0)})
add("_resolve_approval", dict(BASE, chat_id="", state={"session_key": "S1", "request_id": None, "chat_id": ""}),
    io={POP: rt({"session_key": "S1", "request_id": None, "chat_id": ""}),
        "resolve_gateway_approval_card": rt(0)})
add("_resolve_approval", dict(BASE, state=STATE),
    io={POP: rt(STATE), "resolve_gateway_approval_card": [rais("ValueError", "bad")]})
add("_resolve_approval", dict(BASE, state=STATE),
    io={POP: rt(STATE), "resolve_gateway_approval_card": [rais("SystemExit", "sx")]})
add("_resolve_approval", dict(BASE, state=STATE),
    io={POP: rt(STATE), "resolve_gateway_approval_card": rt(0),
        "t": rt("msg"), "send": [rais("ValueError", "bad")]})
add("_resolve_approval", dict(BASE), io={}, note="fail-closed")


# ── _resolve_channel_prompt ──
add("_resolve_channel_prompt", {"chat_id": "c1", "parent_id": None, "extra": {}},
    io={"resolve_channel_prompt": rt("PROMPT")})
add("_resolve_channel_prompt", {"chat_id": "c2", "parent_id": "p1", "extra": {"x": "y"}},
    io={"resolve_channel_prompt": rt("PROMPT2")})
add("_resolve_channel_prompt", {"chat_id": "c3", "parent_id": None, "extra": {}},
    io={"resolve_channel_prompt": rt(None)})
add("_resolve_channel_prompt", {"chat_id": "c1", "parent_id": None, "extra": {}},
    io={"resolve_channel_prompt": [rais("ValueError", "bad")]})
add("_resolve_channel_prompt", {"chat_id": "c1", "parent_id": None, "extra": {}},
    io={"resolve_channel_prompt": [rais("SystemExit", "sx")]})
add("_resolve_channel_prompt", {"chat_id": "c1", "parent_id": None, "extra": {}},
    io={}, note="fail-closed")


# ── _resolve_normalized_message_type ──
add("_resolve_normalized_message_type", {"preferred": "audio", "media_types": []}, io={})
add("_resolve_normalized_message_type", {"preferred": "photo", "media_types": ["image/png"]},
    io={"resolve_media_message_type": rt("photo")})
add("_resolve_normalized_message_type", {"preferred": "document", "media_types": []},
    io={"resolve_media_message_type": rt("document")})
add("_resolve_normalized_message_type", {"preferred": "text", "media_types": ["x"]}, io={})
add("_resolve_normalized_message_type", {"preferred": "photo", "media_types": ["image/png"]},
    io={"resolve_media_message_type": [rais("SystemExit", "sx")]})
add("_resolve_normalized_message_type", {"preferred": "photo", "media_types": ["image/png"]},
    io={}, note="fail-closed")


# ── _resolve_sender_name_from_api ──
# seams: time_time, get_cached_sender_name, fetch_bot_names, sender_cache_set,
#        sender_cache_get, build_get_user_request, run_blocking
add("_resolve_sender_name_from_api", {"sender_id": "ou_1", "is_bot": False, "has_client": False}, io={})
add("_resolve_sender_name_from_api", {"sender_id": "  ou_1  ", "is_bot": False, "has_client": True, "cached_name": "Cached"},
    io={"time_time": rt(100), "get_cached_sender_name": rt("Cached")})
add("_resolve_sender_name_from_api", {"sender_id": "ou_1", "is_bot": True, "has_client": True, "cached_name": None},
    io={"time_time": rt(100), "get_cached_sender_name": rt(None),
        "fetch_bot_names": rt({"ou_1": "Bot"}),
        "sender_cache_set": rt(None), "sender_cache_get": rt(["Bot", 700])})
add("_resolve_sender_name_from_api", {"sender_id": "ou_1", "is_bot": True, "has_client": True, "cached_name": None},
    io={"time_time": rt(100), "get_cached_sender_name": rt(None), "fetch_bot_names": rt(None)})
add("_resolve_sender_name_from_api",
    {"sender_id": "ou_1", "is_bot": False, "has_client": True, "cached_name": None,
     "response": {"success": True, "user": {"name": "  Alice  "}}},
    io={"time_time": rt(100), "get_cached_sender_name": rt(None),
        "build_get_user_request": rt("REQ"), "run_blocking": rt({"success": True, "user": {"name": "  Alice  "}}),
        "sender_cache_set": rt(None)})
add("_resolve_sender_name_from_api",
    {"sender_id": "on_1", "is_bot": False, "has_client": True, "cached_name": None,
     "response": {"success": False, "user": {}}},
    io={"time_time": rt(100), "get_cached_sender_name": rt(None),
        "build_get_user_request": rt("REQ"), "run_blocking": rt({"success": False, "user": {}})})
add("_resolve_sender_name_from_api", {"sender_id": "ou_1", "is_bot": False, "has_client": True, "cached_name": None},
    io={"time_time": rt(100), "get_cached_sender_name": rt(None),
        "build_get_user_request": rt("REQ"), "run_blocking": [rais("SystemExit", "sx")]})
add("_resolve_sender_name_from_api", {"sender_id": "ou_1", "is_bot": False, "has_client": True, "cached_name": None},
    io={"time_time": rt(100), "get_cached_sender_name": rt(None),
        "build_get_user_request": rt("REQ"), "run_blocking": [rais("ValueError", "bad")]})
add("_resolve_sender_name_from_api", {"sender_id": "ou_1", "is_bot": False, "has_client": True, "cached_name": None},
    io={}, note="fail-closed")


# ── _resolve_sender_profile ──
add("_resolve_sender_profile",
    {"sender_id": {"open_id": "O", "user_id": "U", "union_id": "X"}, "is_bot": False},
    io={"resolve_sender_name_from_api": rt("Name")})
add("_resolve_sender_profile",
    {"sender_id": {"open_id": "O", "user_id": None, "union_id": None}, "is_bot": True},
    io={"resolve_sender_name_from_api": rt("BotName")})
add("_resolve_sender_profile",
    {"sender_id": {"open_id": None, "user_id": None, "union_id": "X"}, "is_bot": False},
    io={"resolve_sender_name_from_api": rt(None)})
add("_resolve_sender_profile",
    {"sender_id": {"open_id": "O", "user_id": "U", "union_id": "X"}, "is_bot": False},
    io={"resolve_sender_name_from_api": [rais("SystemExit", "sx")]})
add("_resolve_sender_profile",
    {"sender_id": {"open_id": None, "user_id": "U2", "union_id": "X2"}, "is_bot": False},
    io={"resolve_sender_name_from_api": rt("Named")})
add("_resolve_sender_profile",
    {"sender_id": {"open_id": "O", "user_id": "U", "union_id": "X"}, "is_bot": False},
    io={}, note="fail-closed")


# ── _resolve_update_prompt ──
UP = {"ident": "p1", "answer": "A", "open_id": "ou1", "chat_id": "C1", "user_id": "u1", "union_id": "un1"}
USTATE = {"session_key": "S2"}
add("_resolve_update_prompt", dict(UP), io={POP: rt(None)})
add("_resolve_update_prompt", dict(UP),
    io={POP: rt(USTATE), "write_update_prompt_response": rt(None)})
add("_resolve_update_prompt", dict(UP, answer="B"),
    io={POP: rt(USTATE), "write_update_prompt_response": [rais("ValueError", "bad")]})
add("_resolve_update_prompt", dict(UP),
    io={POP: rt(USTATE), "write_update_prompt_response": [rais("SystemExit", "sx")]})
add("_resolve_update_prompt", dict(UP), io={POP: [rais("ValueError", "bad")]})
add("_resolve_update_prompt", dict(UP), io={}, note="fail-closed")


# ── _schedule_text_batch_flush ──
add("_schedule_text_batch_flush", {"key": "t1"},
    io={"reschedule_batch_task": rt(None)})
add("_schedule_text_batch_flush", {"key": "t2"},
    io={"reschedule_batch_task": [rais("ValueError", "bad")]})
add("_schedule_text_batch_flush", {"key": "t3"},
    io={"reschedule_batch_task": [rais("SystemExit", "sx")]})
add("_schedule_text_batch_flush", {"key": "t4"}, io={"reschedule_batch_task": rt(None)})
add("_schedule_text_batch_flush", {"key": "t5"}, io={"reschedule_batch_task": rt(None)})
add("_schedule_text_batch_flush", {"key": "t6"}, io={}, note="fail-closed")


# ── _schedule_media_batch_flush ──
add("_schedule_media_batch_flush", {"key": "m1"}, io={"reschedule_batch_task": rt(None)})
add("_schedule_media_batch_flush", {"key": "m2"},
    io={"reschedule_batch_task": [rais("ValueError", "bad")]})
add("_schedule_media_batch_flush", {"key": "m3"},
    io={"reschedule_batch_task": [rais("SystemExit", "sx")]})
add("_schedule_media_batch_flush", {"key": "m4"}, io={"reschedule_batch_task": rt(None)})
add("_schedule_media_batch_flush", {"key": "m5"}, io={"reschedule_batch_task": rt(None)})
add("_schedule_media_batch_flush", {"key": "m6"}, io={}, note="fail-closed")


# ── _EA_REASON_LABEL ──
add("_EA_REASON_LABEL", {}, io={"t": rt("Reason")})
add("_EA_REASON_LABEL", {}, io={"t": rt("")})
add("_EA_REASON_LABEL", {}, io={"t": rt("A:B")})
add("_EA_REASON_LABEL", {}, io={"t": [rais("ValueError", "bad")]})
add("_EA_REASON_LABEL", {}, io={"t": [rais("SystemExit", "sx")]})
add("_EA_REASON_LABEL", {}, io={}, note="fail-closed")


# ── _EA_SMART_DENY_LINE ──
add("_EA_SMART_DENY_LINE", {}, io={"t": rt("Label:rest")})
add("_EA_SMART_DENY_LINE", {}, io={"t": rt("Nosep")})
add("_EA_SMART_DENY_LINE", {}, io={"t": rt("a:b:c")})
add("_EA_SMART_DENY_LINE", {}, io={"t": [rais("ValueError", "bad")]})
add("_EA_SMART_DENY_LINE", {}, io={"t": [rais("SystemExit", "sx")]})
add("_EA_SMART_DENY_LINE", {}, io={}, note="fail-closed")


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    # per-function count summary (assert >= 6 each)
    from collections import Counter
    counts = Counter(c["func"] for c in CASES)
    print(f"wrote {len(CASES)} cases to {OUT}")
    for func in sorted(counts):
        flag = "" if counts[func] >= 6 else "  <-- UNDER 6"
        print(f"  {func}: {counts[func]}{flag}")
    assert all(v >= 6 for v in counts.values()), "every function needs >= 6 cases"


if __name__ == "__main__":
    main()
