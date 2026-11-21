#!/usr/bin/env python3
"""Generate tests/fixtures/feishu_adapter_flow10.jsonl — input cases for the
Feishu adapter remainders FLOW oracle.

Every case is hand-scripted: in.io maps a seam name to a result (plain JSON
value, an ordered LIST of results, or {"__script__":true,"raise":{type,message}})
that the C universal seam and the oracle's recording stubs replay identically.
Run `python tests/gen_feishu_adapter_flow10_cases.py` to regenerate.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "feishu_adapter_flow10.jsonl"
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


def bytes_of(s):
    return list(s.encode("utf-8"))


# ── _fetch_bot_names (seam: tenant_get_raw) ──
BOTS = json.dumps({"code": 0, "data": {"bots": {
    "ou1": {"name": " Alice "}, "ou2": {"name": "Bob"}}}})
add("_fetch_bot_names", {"client": True, "bot_ids": ["ou1"]},
    io={"tenant_get_raw": [ret(BOTS)]})
add("_fetch_bot_names", {"client": True, "bot_ids": ["ou1", "ou2"]},
    io={"tenant_get_raw": [ret(BOTS)]})
add("_fetch_bot_names", {"client": False, "bot_ids": ["ou1"]}, io={})
add("_fetch_bot_names", {"client": True, "bot_ids": []}, io={})
add("_fetch_bot_names", {"client": True, "bot_ids": ["ou1"]},
    io={"tenant_get_raw": [ret("")]})
add("_fetch_bot_names", {"client": True, "bot_ids": ["ou1"]},
    io={"tenant_get_raw": [ret('{"code":1}')]})
add("_fetch_bot_names", {"client": True, "bot_ids": ["ou1"]},
    io={"tenant_get_raw": [rais("ValueError", "down")]})
add("_fetch_bot_names", {"client": True, "bot_ids": ["ou1"]},
    io={"tenant_get_raw": [rais("SystemExit", "sx")]})
add("_fetch_bot_names", {"client": True, "bot_ids": ["ou1"]}, io={}, note="fail-closed")


# ── _fetch_last_message_in_thread (seams: list_messages, response_succeeded) ──
add("_fetch_last_message_in_thread", {"client": True, "thread_id": "t1"},
    io={"list_messages": [ret({"data": {"items": [{"message_id": "M1"}]}})],
        "response_succeeded": [ret(True)]})
add("_fetch_last_message_in_thread", {"client": True, "thread_id": "t1"},
    io={"list_messages": [ret({"data": {"items": []}})],
        "response_succeeded": [ret(True)]})
add("_fetch_last_message_in_thread", {"client": True, "thread_id": "t1"},
    io={"list_messages": [ret({"data": {"items": [{"message_id": "M1"}]}})],
        "response_succeeded": [ret(False)]})
add("_fetch_last_message_in_thread", {"client": False, "thread_id": "t1"}, io={})
add("_fetch_last_message_in_thread", {"client": True, "thread_id": ""}, io={})
add("_fetch_last_message_in_thread", {"client": True, "thread_id": "t1"},
    io={"list_messages": [rais("ValueError", "down")]})
add("_fetch_last_message_in_thread", {"client": True, "thread_id": "t1"},
    io={"list_messages": [ret({"data": {"items": []}})],
        "response_succeeded": [rais("SystemExit", "sx")]})
add("_fetch_last_message_in_thread", {"client": True, "thread_id": "t1"}, io={}, note="fail-closed")


# ── _fetch_message_resource (seams: build_resource_request, fetch_resource) ──
FR_ARGS = {"message_id": "m1", "file_key": "f1", "resource_type": "file"}
add("_fetch_message_resource", FR_ARGS,
    io={"build_resource_request": [ret({})], "fetch_resource": [ret({"code": 0})]})
add("_fetch_message_resource", FR_ARGS,
    io={"build_resource_request": [ret({})], "fetch_resource": [ret(None)]})
add("_fetch_message_resource", FR_ARGS,
    io={"build_resource_request": [rais("ValueError", "down")]})
add("_fetch_message_resource", FR_ARGS,
    io={"build_resource_request": [rais("SystemExit", "sx")]})
add("_fetch_message_resource", FR_ARGS,
    io={"build_resource_request": [ret({})], "fetch_resource": [rais("ValueError", "down")]})
add("_fetch_message_resource", FR_ARGS,
    io={"build_resource_request": [ret({})], "fetch_resource": [rais("SystemExit", "sx")]})
add("_fetch_message_resource", FR_ARGS, io={}, note="fail-closed")


# ── _fetch_message_text (many seams) ──
FETCH_DATA = {"data": {"items": [{
    "message_id": "m1", "msg_type": "text", "body": {"content": "{\"text\":\"hi\"}"},
    "mentions": []}]}}
add("_fetch_message_text", {"client": True, "message_id": "m1"},
    io={"cache_contains": [ret(True)], "cache_move_end": [ret(None)], "cache_get": [ret("HIT")]})
add("_fetch_message_text", {"client": False, "message_id": "m1"}, io={})
add("_fetch_message_text", {"client": True, "message_id": ""}, io={})
add("_fetch_message_text", {"client": True, "message_id": "m1"},
    io={"cache_contains": [rais("ValueError", "down")]})
add("_fetch_message_text", {"client": True, "message_id": "m1"},
    io={"cache_contains": [ret(False)], "build_get_message_request": [rais("SystemExit", "sx")]})
add("_fetch_message_text", {"client": True, "message_id": "m1"},
    io={"cache_contains": [ret(False)], "build_get_message_request": [ret({})],
        "fetch_message": [ret(FETCH_DATA)], "response_succeeded": [ret(False)]})
add("_fetch_message_text", {"client": True, "message_id": "m1"},
    io={"cache_contains": [ret(False)], "build_get_message_request": [ret({})],
        "fetch_message": [ret(FETCH_DATA)], "response_succeeded": [ret(True)],
        "extract_text": [ret("TXT")], "cache_set": [ret(None)],
        "cache_len": [ret(512)]})
add("_fetch_message_text", {"client": True, "message_id": "m1"},
    io={"cache_contains": [ret(False)], "build_get_message_request": [ret({})],
        "fetch_message": [ret(FETCH_DATA)], "response_succeeded": [ret(True)],
        "extract_text": [ret("TXT2")], "cache_set": [ret(None)],
        "cache_len": [ret(513), ret(512)], "cache_popitem": [ret(["k", "v"])]})
add("_fetch_message_text", {"client": True, "message_id": "m1"},
    io={"cache_contains": [ret(False)], "build_get_message_request": [ret({})],
        "fetch_message": [ret(FETCH_DATA)], "response_succeeded": [ret(True)],
        "extract_text": [rais("SystemExit", "sx")]})
add("_fetch_message_text", {"client": True, "message_id": "m1"}, io={}, note="fail-closed")


# ── _flush_media_batch (seam: delayed_flush) ──
add("_flush_media_batch", {"key": "k1", "delay": 0.5}, io={"delayed_flush": [ret(None)]})
add("_flush_media_batch", {"key": "k1", "delay": 0.0}, io={"delayed_flush": [ret(None)]})
add("_flush_media_batch", {"key": "", "delay": 1.0}, io={"delayed_flush": [ret(None)]})
add("_flush_media_batch", {"key": "k1", "delay": 1.0},
    io={"delayed_flush": [rais("ValueError", "down")]})
add("_flush_media_batch", {"key": "k1", "delay": 1.0},
    io={"delayed_flush": [rais("SystemExit", "sx")]})
add("_flush_media_batch", {"key": "k1", "delay": 1.0}, io={}, note="fail-closed")


# ── _flush_media_batch_now (seams: batch_pop, handle_guards) ──
add("_flush_media_batch_now", {"key": "k1"},
    io={"batch_pop": [ret({"media_urls": ["u1"], "text": "c"})], "handle_guards": [ret(None)]})
add("_flush_media_batch_now", {"key": "k1"}, io={"batch_pop": [ret(None)]})
add("_flush_media_batch_now", {"key": "k1"}, io={"batch_pop": [ret({})]})
add("_flush_media_batch_now", {"key": "k1"}, io={"batch_pop": [rais("ValueError", "down")]})
add("_flush_media_batch_now", {"key": "k1"},
    io={"batch_pop": [ret({"media_urls": ["u1"]})], "handle_guards": [rais("ValueError", "down")]})
add("_flush_media_batch_now", {"key": "k1"},
    io={"batch_pop": [ret({"media_urls": ["u1"]})], "handle_guards": [rais("SystemExit", "sx")]})
add("_flush_media_batch_now", {"key": "k1"}, io={}, note="fail-closed")


# ── _get_audio_duration_ms (seam: read_audio) ──
def ogg(granule, segs=(1,)):
    b = bytearray()
    b += b"OggS" + b"\x00\x00"
    b += (int(granule) & 0xFFFFFFFFFFFFFFFF).to_bytes(8, "little")
    b += b"\x00" * 12
    b.append(len(segs))
    b += bytes(segs)
    b += b"PAYLOAD"
    return list(b)


add("_get_audio_duration_ms", {"path": "/tmp/a.ogg"},
    io={"read_audio": [ret(ogg(48000))]})
add("_get_audio_duration_ms", {"path": "/tmp/a.ogg"},
    io={"read_audio": [ret(ogg(0))]})
add("_get_audio_duration_ms", {"path": "/tmp/a.ogg"},
    io={"read_audio": [ret(ogg(96000 * 3))]})
add("_get_audio_duration_ms", {"path": "/tmp/a.ogg"},
    io={"read_audio": [ret([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])]})
add("_get_audio_duration_ms", {"path": "/tmp/a.ogg"},
    io={"read_audio": [rais("ValueError", "down")]})
add("_get_audio_duration_ms", {"path": "/tmp/a.ogg"},
    io={"read_audio": [rais("SystemExit", "sx")]})
add("_get_audio_duration_ms", {"path": "/tmp/a.ogg"},
    io={"read_audio": [ret(ogg(-1))]})
add("_get_audio_duration_ms", {"path": "/tmp/a.ogg"}, io={}, note="fail-closed")


# ── _get_chat_lock (seams: locks_*) ──
LOCK = {"locked": False}
add("_get_chat_lock", {"chat_id": "c1", "max_size": 2},
    io={"locks_get": [ret(LOCK)], "locks_move_end": [ret(None)]})
add("_get_chat_lock", {"chat_id": "c1", "max_size": 2},
    io={"locks_get": [ret({"locked": True})], "locks_move_end": [ret(None)]})
add("_get_chat_lock", {"chat_id": "c1", "max_size": 2},
    io={"locks_get": [ret(None)], "locks_len": [ret(1)],
        "new_lock": [ret(LOCK)], "locks_set": [ret(None)]})
add("_get_chat_lock", {"chat_id": "c1", "max_size": 2},
    io={"locks_get": [ret(None)], "locks_len": [ret(2)],
        "locks_items": [ret([["a", {"locked": False}], ["b", {"locked": True}]])],
        "locks_pop": [ret(None)], "new_lock": [ret(LOCK)], "locks_set": [ret(None)]})
add("_get_chat_lock", {"chat_id": "c1", "max_size": 2},
    io={"locks_get": [ret(None)], "locks_len": [ret(2)],
        "locks_items": [ret([["a", {"locked": True}], ["b", {"locked": True}]])],
        "locks_pop": [ret(None)], "new_lock": [ret(LOCK)], "locks_set": [ret(None)]})
add("_get_chat_lock", {"chat_id": "c1", "max_size": 2},
    io={"locks_get": [rais("ValueError", "down")]})
add("_get_chat_lock", {"chat_id": "c1", "max_size": 2},
    io={"locks_get": [ret(None)], "locks_len": [ret(1)],
        "new_lock": [rais("SystemExit", "sx")]})
add("_get_chat_lock", {"chat_id": "c1", "max_size": 2}, io={}, note="fail-closed")


# ── _get_sdk_executor (seam: new_executor) ──
add("_get_sdk_executor", {"closing": True}, io={})
add("_get_sdk_executor", {"closing": False, "executor": None},
    io={"new_executor": [ret({"n": 1})]})
add("_get_sdk_executor", {"closing": False, "executor": {"_shutdown": False}}, io={})
add("_get_sdk_executor", {"closing": False, "executor": {"_shutdown": True}},
    io={"new_executor": [ret({"n": 2})]})
add("_get_sdk_executor", {"closing": False, "executor": None},
    io={"new_executor": [rais("ValueError", "down")]})
add("_get_sdk_executor", {"closing": False, "executor": None},
    io={"new_executor": [rais("SystemExit", "sx")]})
add("_get_sdk_executor", {"closing": False, "executor": None}, io={}, note="fail-closed")


# ── _handle_message_event_data (seams: is_duplicate, admit, warn_allowlist, is_bot_sender, process_inbound) ──
def mev(message, sender):
    return {"event": {"message": message, "sender": sender}}


GOOD_MSG = {"message_id": "m1", "chat_id": "c1", "chat_type": "p2p"}
GOOD_SENDER = {"sender_id": "ou1"}
add("_handle_message_event_data", {"data": {"event": {}}}, io={})
add("_handle_message_event_data", {"data": mev(GOOD_MSG, {})}, io={})
add("_handle_message_event_data", {"data": mev({}, GOOD_SENDER)}, io={})
add("_handle_message_event_data", {"data": mev(GOOD_MSG, GOOD_SENDER)},
    io={"is_duplicate": [ret(True)]})
add("_handle_message_event_data", {"data": mev(GOOD_MSG, GOOD_SENDER)},
    io={"is_duplicate": [ret(False)], "admit": [ret(None)],
        "is_bot_sender": [ret(True)], "process_inbound": [ret(None)]})
add("_handle_message_event_data", {"data": mev(GOOD_MSG, GOOD_SENDER)},
    io={"is_duplicate": [ret(False)], "admit": [ret("group_policy_rejected")],
        "warn_allowlist": [ret(None)]})
add("_handle_message_event_data", {"data": mev(GOOD_MSG, GOOD_SENDER)},
    io={"is_duplicate": [ret(False)], "admit": [ret("other")]})
add("_handle_message_event_data", {"data": mev(GOOD_MSG, GOOD_SENDER)},
    io={"is_duplicate": [ret(False)], "admit": [rais("SystemExit", "sx")]})
add("_handle_message_event_data", {"data": mev(GOOD_MSG, GOOD_SENDER)},
    io={"is_duplicate": [ret(False)], "admit": [ret(None)],
        "is_bot_sender": [ret(False)], "process_inbound": [rais("ValueError", "down")]})
add("_handle_message_event_data", {"data": mev(GOOD_MSG, GOOD_SENDER)}, io={}, note="fail-closed")


# ── _handle_message_with_guards (seams: get_chat_lock, handle_message) ──
add("_handle_message_with_guards", {"event": {"source": {"chat_id": "c1"}}},
    io={"get_chat_lock": [ret("L")], "handle_message": [ret(None)]})
add("_handle_message_with_guards", {"event": {"source": {}}},
    io={"get_chat_lock": [ret("L")], "handle_message": [ret(None)]})
add("_handle_message_with_guards", {"event": {"source": {"chat_id": ""}}},
    io={"get_chat_lock": [ret("L")], "handle_message": [ret("H")]})
add("_handle_message_with_guards", {"event": {"source": {"chat_id": "c1"}}},
    io={"get_chat_lock": [rais("ValueError", "down")]})
add("_handle_message_with_guards", {"event": {"source": {"chat_id": "c1"}}},
    io={"get_chat_lock": [rais("SystemExit", "sx")]})
add("_handle_message_with_guards", {"event": {"source": {"chat_id": "c1"}}},
    io={"get_chat_lock": [ret("L")], "handle_message": [rais("ValueError", "down")]})
add("_handle_message_with_guards", {"event": {"source": {"chat_id": "c1"}}}, io={}, note="fail-closed")


# ── _handle_webhook_request (many seams) ──
REQ = {"remote": "1.2.3.4", "headers": {"Content-Type": "application/json"}}
CFG = {"verification_token": "", "encrypt_key": "", "app_id": "app",
       "webhook_path": "/hook", "max_body": 1048576, "handlers": {}}


def wrq(**over):
    a = dict(CFG); a["request"] = dict(REQ); a.update(over)
    return a


def badct():
    a = wrq(); a["request"]["headers"]["Content-Type"] = "text/plain"
    return a


add("_handle_webhook_request", badct(),
    io={"webhook_reject": [ret("R415")]})
_large = wrq(); _large["request"]["content_length"] = 2 * 1024 * 1024
add("_handle_webhook_request", _large, io={"webhook_reject": [ret("R413")]})
add("_handle_webhook_request", wrq(),
    io={"read_body": [rais("ValueError", "too big")], "webhook_reject": [ret("R413")]})
add("_handle_webhook_request", wrq(),
    io={"read_body": [rais("TimeoutError", "slow")], "webhook_reject": [ret("R408")]})
add("_handle_webhook_request", wrq(),
    io={"read_body": [rais("OSError", "boom")], "webhook_reject": [ret("R400")]})
add("_handle_webhook_request", wrq(),
    io={"read_body": [ret(bytes_of("not json"))], "webhook_reject": [ret("RJ")]})
add("_handle_webhook_request", wrq(),
    io={"read_body": [ret(bytes_of(json.dumps({"type": "url_verification", "challenge": "xyz"})))],
        "web_response": [ret("CHALLENGE")]})
add("_handle_webhook_request", wrq(verification_token="tok"),
    io={"read_body": [ret(bytes_of(json.dumps({"header": {"token": "bad"}})))],
        "secrets_match": [ret(False)], "webhook_reject": [ret("R401T")]})
add("_handle_webhook_request", wrq(encrypt_key="ek"),
    io={"read_body": [ret(bytes_of(json.dumps({"header": {"event_type": "x"}})))],
        "sig_valid": [ret(False)], "webhook_reject": [ret("R401S")]})
add("_handle_webhook_request", wrq(),
    io={"read_body": [ret(bytes_of(json.dumps({"header": {"event_type": "x"}})))],
        "rate_limit": [ret(False)], "webhook_reject": [ret("R429")]})
add("_handle_webhook_request", wrq(),
    io={"read_body": [ret(bytes_of(json.dumps({"encrypt": "x"})))],
        "webhook_reject": [ret("RENC")]})
add("_handle_webhook_request", wrq(),
    io={"read_body": [ret(bytes_of(json.dumps({"header": {"event_type": "card.action.trigger"}})))],
        "clear_anomaly": [ret(None)], "card_action": [ret({"card": 1})],
        "web_response": [ret("CARDRESP")]})
add("_handle_webhook_request", wrq(),
    io={"read_body": [ret(bytes_of(json.dumps({"header": {"event_type": "im.message.reaction.created_v1"}})))],
        "clear_anomaly": [ret(None)], "namespace": [ret({"d": 1})],
        "reaction_event": [ret(None)], "web_response": [ret("OK1")]})
add("_handle_webhook_request", wrq(handlers={"im.message.receive_v1": "_on_message_event"}),
    io={"read_body": [ret(bytes_of(json.dumps({"header": {"event_type": "im.message.receive_v1"}})))],
        "clear_anomaly": [ret(None)], "namespace": [ret({"d": 2})],
        "dispatch_handler": [ret(None)], "web_response": [ret("OK2")]})
add("_handle_webhook_request", wrq(),
    io={"read_body": [ret(bytes_of(json.dumps({"header": {"event_type": "unknown.event"}})))],
        "clear_anomaly": [ret(None)], "namespace": [ret({"d": 3})],
        "web_response": [ret("OK3")]})
add("_handle_webhook_request", wrq(verification_token="tok"),
    io={"read_body": [ret(bytes_of(json.dumps({"type": "url_verification", "challenge": "c"})))],
        "secrets_match": [ret(True)], "rate_limit": [ret(True)],
        "web_response": [ret("CH2")]})
add("_handle_webhook_request", wrq(),
    io={"read_body": [rais("SystemExit", "sx")]})
add("_handle_webhook_request", badct(),
    io={"webhook_reject": [rais("SystemExit", "sx")]})
add("_handle_webhook_request", wrq(), io={}, note="fail-closed")


# ── _clear_webhook_anomaly (seam: anomaly_pop) ──
add("_clear_webhook_anomaly", {"remote_ip": "1.2.3.4"},
    io={"anomaly_pop": [ret(None)]})
add("_clear_webhook_anomaly", {"remote_ip": "5.6.7.8"},
    io={"anomaly_pop": [ret((3, "415", 100))]})
add("_clear_webhook_anomaly", {"remote_ip": ""}, io={"anomaly_pop": [ret(None)]})
add("_clear_webhook_anomaly", {"remote_ip": "1.2.3.4"},
    io={"anomaly_pop": [rais("ValueError", "down")]})
add("_clear_webhook_anomaly", {"remote_ip": "1.2.3.4"},
    io={"anomaly_pop": [rais("SystemExit", "sx")]})
add("_clear_webhook_anomaly", {"remote_ip": "1.2.3.4"}, io={}, note="fail-closed")


# ── _ws_link_up (seam: mark_connected) ──
add("_ws_link_up", {"running": True, "client": {"id": 1}},
    io={"mark_connected": [ret(None)]})
add("_ws_link_up", {"running": False, "client": {"id": 1}}, io={})
add("_ws_link_up", {"running": True, "client": {"id": 1}, "ws_client": {"id": 2}}, io={})
add("_ws_link_up", {"running": True, "client": {"id": 1}},
    io={"mark_connected": [rais("ValueError", "down")]})
add("_ws_link_up", {"running": True, "client": {"id": 1}},
    io={"mark_connected": [rais("SystemExit", "sx")]})
add("_ws_link_up", {"running": True, "client": {"id": 1}}, io={}, note="fail-closed")


# ── _ws_link_retrying (seam: write_status) ──
add("_ws_link_retrying", {"running": True, "client": {"id": 1}},
    io={"write_status": [ret(None)]})
add("_ws_link_retrying", {"running": False, "client": {"id": 1}}, io={})
add("_ws_link_retrying", {"running": True, "client": {"id": 1}, "ws_client": {"id": 2}}, io={})
add("_ws_link_retrying", {"running": True, "client": {"id": 1}},
    io={"write_status": [rais("ValueError", "down")]})
add("_ws_link_retrying", {"running": True, "client": {"id": 1}},
    io={"write_status": [rais("SystemExit", "sx")]})
add("_ws_link_retrying", {"running": True, "client": {"id": 1}}, io={}, note="fail-closed")


# ── _submit_if_ready (seams: loop_accepts, submit_on_loop) ──
add("_submit_if_ready", {"label": "task"},
    io={"loop_accepts": [ret(True)], "submit_on_loop": [ret(None)]})
add("_submit_if_ready", {"label": "task"}, io={"loop_accepts": [ret(False)]})
add("_submit_if_ready", {"label": "", "loop": "L"},
    io={"loop_accepts": [ret(True)], "submit_on_loop": [ret(None)]})
add("_submit_if_ready", {"label": "task"},
    io={"loop_accepts": [rais("ValueError", "down")]})
add("_submit_if_ready", {"label": "task"},
    io={"loop_accepts": [ret(True)], "submit_on_loop": [rais("SystemExit", "sx")]})
add("_submit_if_ready", {"label": "task"}, io={}, note="fail-closed")


OUT.parent.mkdir(parents=True, exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for case in CASES:
        f.write(json.dumps(case, ensure_ascii=False) + "\n")
print(f"wrote {len(CASES)} cases to {OUT}")
