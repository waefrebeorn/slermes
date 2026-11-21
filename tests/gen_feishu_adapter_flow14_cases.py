#!/usr/bin/env python3
"""Generate tests/fixtures/feishu_adapter_flow14.jsonl — input cases for the
Feishu adapter remainders FLOW port (flow14).

Every case is hand-scripted: in.io maps a seam name to a result (plain JSON
value, an ordered LIST of results, or {"__script__":true,"raise":{type,message}})
that the C universal seam and the oracle's recording stubs replay identically.
Run `python tests/gen_feishu_adapter_flow14_cases.py` to regenerate.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "feishu_adapter_flow14.jsonl"
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
    return [ret(v) for v in vals]


def sr(ok=True, mid=None, err=None):
    return {"success": ok, "message_id": mid, "error": err}


# ── _run_blocking ──
RB = {"func_args": [1, 2]}
add("_run_blocking", RB, io={"get_running_loop": rt("L"), "get_sdk_executor": rt("EX"),
                             "run_in_executor": rt("OK")})
add("_run_blocking", {"func_args": []},
    io={"get_running_loop": rt("L"), "get_sdk_executor": rt("EX"), "run_in_executor": rt(None)})
add("_run_blocking", RB, io={"get_running_loop": rt("L"), "get_sdk_executor": rt("EX"),
                             "run_in_executor": [rais("ValueError", "bad")]})
add("_run_blocking", RB, io={"get_running_loop": rt("L"), "get_sdk_executor": rt("EX"),
                             "run_in_executor": [rais("SystemExit", "sx")]})
add("_run_blocking", RB, io={"get_running_loop": rt("L"),
                             "get_sdk_executor": [rais("SystemExit", "sx")]})
add("_run_blocking", RB, io={"get_running_loop": [rais("ValueError", "bad")]})
add("_run_blocking", RB, io={}, note="fail-closed")


# ── _run_official_feishu_ws_client ──
W = {"nonce": "N1", "reconnect_interval": 3, "ping_interval": None}
OKWS = {"new_event_loop": rt("L"), "set_event_loop": rt(None),
        "install_ws_isolation": rt(None), "apply_overrides": rt(None),
        "start": rt(None), "all_tasks": rt([]),
        "loop_stop": rt(None), "loop_close": rt(None)}
add("_run_official_feishu_ws_client", W, io=OKWS)
add("_run_official_feishu_ws_client", dict(W, ping_interval=20),
    io={"new_event_loop": rt("L"), "set_event_loop": rt(None),
        "install_ws_isolation": rt(None), "apply_overrides": rt(None),
        "start": rt(None), "all_tasks": rt(["T1", "T2"]),
        "task_done": rt(False, True), "task_cancel": rt(None),
        "gather": rt("G"), "run_until_complete": rt(None),
        "loop_stop": rt(None), "loop_close": rt(None)})
add("_run_official_feishu_ws_client", W,
    io=dict(OKWS, apply_overrides=[rais("ValueError", "bad")], logger_debug=rt(None)))
add("_run_official_feishu_ws_client", W, io=dict(OKWS, apply_overrides=[rais("SystemExit", "sx")]))
add("_run_official_feishu_ws_client", W, io=dict(OKWS, start=[rais("ValueError", "bad")]))
add("_run_official_feishu_ws_client", W, io=dict(OKWS, start=[rais("SystemExit", "sx")]))
add("_run_official_feishu_ws_client", W, io=dict(OKWS, loop_stop=[rais("SystemExit", "sx")]))
add("_run_official_feishu_ws_client", W, io={}, note="fail-closed")


# ── _send_exec_approval_prompt ──
EA = {"has_client": True, "chat_id": "C1", "text": "Approve **now**? DL", "session_key": "S1",
      "request_id": "R1", "metadata": {"x": 1}, "card_actions": {"once": "act_once", "deny": "act_deny"},
      "actions": [["Approve", "once", ""], ["Deny", "deny", "danger"]]}
EASR = sr(True, "m1")
add("_send_exec_approval_prompt", dict(EA, has_client=False), io={})
add("_send_exec_approval_prompt", EA,
    io={"next_approval": rt(7), "t": rt("Approval Header"),
        "card_button": rt({"tag": "button"}, {"tag": "button2"}),
        "card": rt({"card": 1}), "ea_deadline_line": rt("DL"),
        "send_interactive_card": rt(EASR)})
add("_send_exec_approval_prompt", EA,
    io={"next_approval": rt(8), "t": rt("H"),
        "card_button": [ret({"b": 1}), rais("ValueError", "bad")],
        "proxy_error": rt("PROXY"), "log_warning": rt(None)})
add("_send_exec_approval_prompt", EA,
    io={"next_approval": rt(80), "t": rt("H"),
        "card_button": rt({"b": 1}, {"b": 2}), "card": rt({"c": 1}),
        "ea_deadline_line": rt(""), "send_interactive_card": rt(sr(False, None, "send failed"))})
add("_send_exec_approval_prompt", EA,
    io={"next_approval": rt(9), "t": rt("H"), "card_button": [rais("ValueError", "bad")],
        "proxy_error": rt("PROXY"), "log_warning": rt(None)})
add("_send_exec_approval_prompt", EA, io={"next_approval": [rais("SystemExit", "sx")]})
add("_send_exec_approval_prompt", dict(EA, actions=[["Go", "unknown", ""]]),
    io={"next_approval": rt(1), "t": rt("H"), "proxy_error": rt("KEYERR"), "log_warning": rt(None)})
add("_send_exec_approval_prompt", EA, io={}, note="fail-closed")


# ── _send_interactive_card ──
CARD_ARGS = {"chat_id": "C1", "card": {"a": 1}, "metadata": {"m": 2},
             "failure_message": "card failed", "state_id": 11, "session_key": "S1",
             "extra_state": {"body": "B", "request_id": "R"}}
add("_send_interactive_card", CARD_ARGS,
    io={"json_dumps": rt('{"a":1}'), "feishu_send_with_retry": rt({"code": 0}),
        "finalize_send_result": rt(sr(True, "mid1")), "state_set": rt(None)})
add("_send_interactive_card", dict(CARD_ARGS, extra_state=None),
    io={"json_dumps": rt("P"), "feishu_send_with_retry": rt({"code": 0}),
        "finalize_send_result": rt(sr(False, None, "no"))})
add("_send_interactive_card", CARD_ARGS,
    io={"json_dumps": rt("P"), "feishu_send_with_retry": [rais("ValueError", "bad")]})
add("_send_interactive_card", CARD_ARGS,
    io={"json_dumps": rt("P"), "feishu_send_with_retry": rt({"code": 0}),
        "finalize_send_result": [rais("SystemExit", "sx")]})
add("_send_interactive_card", CARD_ARGS,
    io={"json_dumps": rt("P"), "feishu_send_with_retry": rt({"code": 0}),
        "finalize_send_result": rt(sr(True, "m2")), "state_set": [rais("SystemExit", "sx")]})
add("_send_interactive_card", CARD_ARGS,
    io={"json_dumps": rt("P"), "feishu_send_with_retry": rt({"code": 0}),
        "finalize_send_result": rt(sr(True, None)), "state_set": rt(None)})
add("_send_interactive_card", CARD_ARGS, io={}, note="fail-closed")


# ── _send_raw_message ──
RAW = {"chat_id": "ou_123", "msg_type": "text", "payload": "hello", "reply_to": None, "metadata": None}
add("_send_raw_message", dict(RAW, metadata={"thread_id": "T1", "reply_to_message_id": "RM1"}),
    io={"uuid4": rt("U1"), "build_reply_message_body": rt("BODY"),
        "build_reply_message_request": rt("REQ"), "run_blocking": rt({"code": 0})})
add("_send_raw_message", dict(RAW, reply_to="DIRECT", metadata={"thread_id": "T1"}),
    io={"uuid4": rt("U2"), "build_reply_message_body": rt("BODY"),
        "build_reply_message_request": rt("REQ"), "run_blocking": rt({"code": 0})})
add("_send_raw_message", dict(RAW, metadata={"thread_id": "T2"}),
    io={"uuid4": rt("U3"), "build_create_message_body": rt("BODY"),
        "build_create_message_request": rt("REQ"), "run_blocking": rt({"code": 0})})
add("_send_raw_message", dict(RAW, chat_id="feishu_user_id:U9"),
    io={"uuid4": rt("U4"), "build_create_message_body": rt("BODY"),
        "build_create_message_request": rt("REQ"), "run_blocking": rt({"code": 0})})
add("_send_raw_message", dict(RAW, chat_id="oc_abc"),
    io={"uuid4": rt("U5"), "build_create_message_body": rt("BODY"),
        "build_create_message_request": rt("REQ"), "run_blocking": rt({"code": 0})})
add("_send_raw_message", RAW,
    io={"uuid4": rt("U6"), "build_create_message_body": rt("BODY"),
        "build_create_message_request": rt("REQ"), "run_blocking": [rais("SystemExit", "sx")]})
add("_send_raw_message", RAW, io={}, note="fail-closed")


# ── _send_uploaded_file_message ──
UF = {"has_client": True, "chat_id": "C1", "file_path": "/tmp/a.png", "file_name": None,
      "reply_to": None, "metadata": None, "caption": "cap", "outbound_message_type": "file"}
UPU = {"path_exists": rt(True), "path_basename": rt("a.png"),
       "resolve_outbound_file_routing": rt(["image", "image"]), "open": rt("FO"),
       "build_file_upload_body": rt("BODY"), "build_file_upload_request": rt("REQ"),
       "run_blocking": rt({"file_key": "KEY"}),
       "send_uploaded_key": rt(sr(True, "m1")), "finalize_send_result": rt(sr(True, "m2"))}
add("_send_uploaded_file_message", dict(UF, has_client=False), io={})
add("_send_uploaded_file_message", UF, io={"path_exists": rt(False)})
add("_send_uploaded_file_message", UF, io=dict(UPU, extract_response_field=rt("KEY")))
add("_send_uploaded_file_message", UF,
    io={"path_exists": rt(True), "path_basename": rt("a.png"),
        "resolve_outbound_file_routing": rt(["image", "image"]), "open": rt("FO"),
        "build_file_upload_body": rt("BODY"), "build_file_upload_request": rt("REQ"),
        "run_blocking": rt({"file_key": "KEY"}), "extract_response_field": rt(None),
        "response_error_result": rt(sr(False, None, "missing file_key"))})
UFOPUS = dict(UF, file_path="/tmp/a.opus", outbound_message_type="audio")
add("_send_uploaded_file_message", UFOPUS,
    io={"path_exists": rt(True), "path_basename": rt("a.opus"),
        "resolve_outbound_file_routing": rt(["opus", "audio"]),
        "get_audio_duration_ms": rt(1234), "open": rt("FO"),
        "build_file_upload_body": rt("BODY"), "build_file_upload_request": rt("REQ"),
        "run_blocking": rt({"file_key": "KEY"}), "extract_response_field": rt("KEY"),
        "send_uploaded_key": rt(sr(True, "m1")), "finalize_send_result": rt(sr(True, "m2"))})
# full audio retry: response code 99992402 in a thread
UFRETRY = dict(UF, caption=None, metadata={"thread_id": "T9", "reply_to_message_id": "MID1"},
               outbound_message_type="audio")
add("_send_uploaded_file_message", UFRETRY,
    io={"path_exists": rt(True), "path_basename": rt("a.opus"),
        "resolve_outbound_file_routing": rt(["opus", "audio"]),
        "get_audio_duration_ms": rt(10), "open": rt("FO"),
        "build_file_upload_body": rt("BODY"), "build_file_upload_request": rt("REQ"),
        "run_blocking": rt({"file_key": "KEY"}), "extract_response_field": rt("KEY"),
        "send_uploaded_key": rt({"code": 99992402}),
        "response_succeeded": rt(False, False), "json_dumps": rt('{"file_key":"KEY"}'),
        "log_info": rt(None), "feishu_send_with_retry": rt({"code": 0}, sr(True, "m3")),
        "log_warning": rt(None), "finalize_send_result": rt(sr(True, "m4"))})
add("_send_uploaded_file_message", UF,
    io=dict(UPU, extract_response_field=rt("KEY"), run_blocking=[rais("SystemExit", "sx")]))
add("_send_uploaded_file_message", UF,
    io=dict(UPU, extract_response_field=rt("KEY"),
            send_uploaded_key=[rais("ValueError", "bad")],
            proxy_error=rt("PROXY"), log_error=rt(None)))
add("_send_uploaded_file_message", UF, io={}, note="fail-closed")


# ── _send_uploaded_key ──
UKEY = {"chat_id": "C1", "reply_to": None, "metadata": None, "caption": "A caption",
        "key_msg_type": "image", "key_payload": {"file_key": "K"},
        "media_tag": {"tag": "media", "file_key": "K", "file_name": "a.png"}}
add("_send_uploaded_key", UKEY,
    io={"build_media_post_payload": rt({"post": 1}), "feishu_send_with_retry": rt({"code": 0})})
add("_send_uploaded_key", dict(UKEY, caption=None),
    io={"json_dumps": rt('{"file_key":"K"}'), "feishu_send_with_retry": rt({"code": 0})})
add("_send_uploaded_key", dict(UKEY, caption=""),
    io={"json_dumps": rt("P"), "feishu_send_with_retry": rt({"code": 0})})
add("_send_uploaded_key", UKEY,
    io={"build_media_post_payload": [rais("SystemExit", "sx")]})
add("_send_uploaded_key", UKEY,
    io={"build_media_post_payload": rt({"post": 1}),
        "feishu_send_with_retry": [rais("ValueError", "bad")]})
add("_send_uploaded_key", UKEY, io={}, note="fail-closed")


# ── _shutdown_sdk_executor ──
add("_shutdown_sdk_executor", {"has_lock": False, "has_executor": True}, io={})
add("_shutdown_sdk_executor", {"has_lock": True, "has_executor": True},
    io={"lock_enter": rt(None), "take_executor": rt("EX"), "lock_exit": rt(None),
        "executor_shutdown": rt(None)})
add("_shutdown_sdk_executor", {"has_lock": True, "has_executor": False},
    io={"lock_enter": rt(None), "take_executor": rt(None), "lock_exit": rt(None)})
add("_shutdown_sdk_executor", {"has_lock": True, "has_executor": True},
    io={"lock_enter": rt(None), "take_executor": rt("EX"), "lock_exit": rt(None),
        "executor_shutdown": [rais("TypeError", "no cancel_futures"), ret(None)]})
add("_shutdown_sdk_executor", {"has_lock": True, "has_executor": True},
    io={"lock_enter": rt(None), "take_executor": rt("EX"), "lock_exit": rt(None),
        "executor_shutdown": [rais("SystemExit", "sx")]})
add("_shutdown_sdk_executor", {"has_lock": True, "has_executor": True},
    io={"lock_enter": [rais("SystemExit", "sx")]})
add("_shutdown_sdk_executor", {"has_lock": True, "has_executor": True},
    io={"lock_enter": rt(None), "take_executor": [rais("SystemExit", "sx")],
        "lock_exit": rt(None)})
add("_shutdown_sdk_executor", {"has_lock": True, "has_executor": True},
    io={"lock_enter": rt(None), "take_executor": rt("EX"), "lock_exit": rt(None)},
    note="fail-closed")


# ── _standalone_send ──
SA = {"pconfig": {"domain": "feishu"}, "chat_id": "C1", "message": "hello",
      "thread_id": None, "media_files": None}
add("_standalone_send", dict(SA, message=""), io={"load_lark_oapi": rt(False),
    "send_error": rt({"success": False, "error": "deps"})})
add("_standalone_send", SA,
    io={"load_lark_oapi": rt(True), "build_lark_client": rt("CLIENT"),
        "send": rt(sr(True, "m1"))})
add("_standalone_send", dict(SA, thread_id="T1"),
    io={"load_lark_oapi": rt(True), "build_lark_client": rt("CLIENT"),
        "send": rt(sr(True, "m2"))})
add("_standalone_send", dict(SA, message="", media_files=[["/tmp/a.png", False]]),
    io={"load_lark_oapi": rt(True), "build_lark_client": rt("CLIENT"),
        "path_exists": rt(True), "media_send": rt(sr(True, "m3"))})
add("_standalone_send", dict(SA, message="", media_files=[["/tmp/a.png", False]]),
    io={"load_lark_oapi": rt(True), "build_lark_client": rt("CLIENT"),
        "path_exists": rt(False),
        "send_error": rt({"success": False, "error": "no media"})})
add("_standalone_send", dict(SA, message=""),
    io={"load_lark_oapi": rt(True), "build_lark_client": rt("CLIENT"),
        "send_error": rt({"success": False, "error": "nothing"})})
add("_standalone_send", SA,
    io={"load_lark_oapi": rt(True), "build_lark_client": rt("CLIENT"),
        "send": [rais("SystemExit", "sx")]})
add("_standalone_send", SA, io={}, note="fail-closed")


# ── _stop_webhook_server ──
add("_stop_webhook_server", {"has_runner": False}, io={})
add("_stop_webhook_server", {"has_runner": True},
    io={"webhook_cleanup": rt(None), "clear_webhook_state": rt(None)})
add("_stop_webhook_server", {"has_runner": True},
    io={"webhook_cleanup": [rais("ValueError", "bad")], "clear_webhook_state": rt(None)})
add("_stop_webhook_server", {"has_runner": True},
    io={"webhook_cleanup": [rais("SystemExit", "sx")], "clear_webhook_state": rt(None)})
add("_stop_webhook_server", {"has_runner": True},
    io={"webhook_cleanup": rt(None), "clear_webhook_state": [rais("SystemExit", "sx")]})
add("_stop_webhook_server", {"has_runner": True}, io={}, note="fail-closed")


# ── _submit_on_loop ──
SL = {"loop": "LOOP1", "coro": "CORO1"}
add("_submit_on_loop", SL, io={"safe_schedule_threadsafe": rt(None)})
add("_submit_on_loop", SL, io={"safe_schedule_threadsafe": rt("FUT"),
                                "add_done_callback": rt(None)})
add("_submit_on_loop", dict(SL, coro="CORO2"), io={"safe_schedule_threadsafe": rt("FUT2"),
                                                   "add_done_callback": rt(None)})
add("_submit_on_loop", SL, io={"safe_schedule_threadsafe": [rais("ValueError", "bad")]})
add("_submit_on_loop", SL, io={"safe_schedule_threadsafe": [rais("SystemExit", "sx")]})
add("_submit_on_loop", SL, io={}, note="fail-closed")


# ── _supervise_websocket_thread ──
SUP = {"backoff": 5.0, "ws_client_present": True}
add("_supervise_websocket_thread", dict(SUP, runnings=[False], futures=["F1"]), io={})
add("_supervise_websocket_thread", dict(SUP, runnings=[True], futures=[None]), io={})
add("_supervise_websocket_thread", dict(SUP, runnings=[True], futures=["F1"]),
    io={"shield": [rais("CancelledError", "cancelled")]})
add("_supervise_websocket_thread", dict(SUP, runnings=[True, True], futures=["F1"],
                                        ws_client_present=False),
    io={"shield": rt(None)})
add("_supervise_websocket_thread", dict(SUP, runnings=[True, True, False], futures=["F1"]),
    io={"shield": rt(None), "logger_error": rt(None),
        "write_runtime_status": rt(None), "sleep": rt(None)})
add("_supervise_websocket_thread", dict(SUP, runnings=[True, True, True, False], futures=["F1"]),
    io={"shield": rt(None), "logger_error": rt(None), "write_runtime_status": rt(None),
        "sleep": rt(None), "connect_websocket": rt(None)})
add("_supervise_websocket_thread", dict(SUP, runnings=[True, True, True, False], futures=["F1"]),
    io={"shield": rt(None), "logger_error": rt(None), "write_runtime_status": rt(None),
        "sleep": rt(None), "connect_websocket": [rais("ValueError", "boom")],
        "logger_warning": rt(None)})
add("_supervise_websocket_thread", dict(SUP, runnings=[True, True, False], futures=["F1"]),
    io={"shield": [rais("ValueError", "swallowed")], "logger_error": rt(None),
        "write_runtime_status": rt(None), "sleep": rt(None)})
add("_supervise_websocket_thread", dict(SUP, runnings=[True], futures=["F1"]),
    io={}, note="fail-closed")


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    from collections import Counter
    counts = Counter(c["func"] for c in CASES)
    print(f"wrote {len(CASES)} cases to {OUT}")
    for func in sorted(counts):
        flag = "" if counts[func] >= 6 else "  <-- UNDER 6"
        print(f"  {func}: {counts[func]}{flag}")
    assert all(v >= 6 for v in counts.values()), "every function needs >= 6 cases"


if __name__ == "__main__":
    main()
