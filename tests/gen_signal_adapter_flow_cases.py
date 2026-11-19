#!/usr/bin/env python3
"""Generate tests/fixtures/signal_adapter_flow.jsonl for the Signal adapter FLOW
port. Deterministic; >=5 cases per ported function. Honours the corpus
restrictions documented in src/gateway/platforms/signal_adapter_flow.c (ASCII
scope, seamed json.loads / Path / unquote ASCII-%XX domain, integer timestamp
keys, scripted time.monotonic sequences, SignalRateLimitError retry_after).

Run: python tests/gen_signal_adapter_flow_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "signal_adapter_flow.jsonl")

ACCT = "+15551234567"
UUID = "u-15551234567"
CASES = []


def case(func, args, state=None, io=None):
    inp = {"args": args}
    if state is not None:
        inp["state"] = state
    if io is not None:
        inp["io"] = io
    CASES.append({"func": func, "in": inp})


def raise_spec(kind, msg="", **extra):
    r = {"type": kind, "message": msg}
    r.update(extra)
    return {"__script__": True, "raise": r}


def sc(ret):
    return {"__script__": True, "ret": ret}


def send_state(**over):
    base = {
        "account": ACCT,
        "_sent_message_timestamps": {},
        "_max_sent_message_timestamps": 500,
        "_recent_sent_timestamps": {},
        "_recent_sent_ttl_seconds": 300.0,
        "_max_recent_timestamps": 512,
    }
    base.update(over)
    return base


def mention_state(**over):
    base = {"_account_normalized": ACCT, "require_mention": False,
            "_recipient_uuid_by_number": {}}
    base.update(over)
    return base


# ── _ext_to_mime ──
for ext in [".jpg", ".png", ".MP3", " .wav ", ".xyz", None, "", ".m4a", ".pdf", ".webp"]:
    case("_ext_to_mime", {"ext": ext})

# ── _extract_quote_author ──
case("_extract_quote_author", {"quote_data": {"author": "+15550001111"}})
case("_extract_quote_author", {"quote_data": {"authorNumber": "+15550002222"}})
case("_extract_quote_author", {"quote_data": {"authorUuid": UUID}})
case("_extract_quote_author", {"quote_data": {"author": "", "authorNumber": "+15550003333"}})
case("_extract_quote_author", {"quote_data": {}})
case("_extract_quote_author", {"quote_data": "not-a-dict"})
case("_extract_quote_author", {"quote_data": None})
case("_extract_quote_author", {"quote_data": {"authorServiceId": "svc-1"}})
case("_extract_quote_author", {"quote_data": {"author": 0, "authorUuid": UUID}})
case("_extract_quote_author", {"quote_data": {"authorAci": "aci-1", "author": "+15550004444"}})

# ── _validate_send_result ──
case("_validate_send_result", {"result": {"results": [{"type": "SUCCESS"}]}}, state={})
case("_validate_send_result", {"result": {"results": [{"type": "NETWORK_FAILURE"}]}}, state={})
case("_validate_send_result", {"result": {"results": [{"success": False, "failure": "bad"}]}}, state={})
case("_validate_send_result", {"result": {"results": [{"success": False}]}}, state={})
case("_validate_send_result", {"result": {"results": [{"type": "SUCCESS"}, {"type": "FAILURE"}]}}, state={})
case("_validate_send_result", {"result": {"results": [5, {"type": "SUCCESS"}]}}, state={})
case("_validate_send_result", {"result": {}}, state={})
case("_validate_send_result", {"result": None}, state={})
case("_validate_send_result", {"result": {"results": []}}, state={})
case("_validate_send_result", {"result": {"results": [{"success": True}]}}, state={})

# ── _consume_sent_timestamp ──
case("_consume_sent_timestamp", {"ts": 123}, state={"_recent_sent_timestamps": {"123": 1.0}})
case("_consume_sent_timestamp", {"ts": 999}, state={"_recent_sent_timestamps": {"123": 1.0}})
case("_consume_sent_timestamp", {"ts": 0}, state={"_recent_sent_timestamps": {"123": 1.0}})
case("_consume_sent_timestamp", {"ts": 123}, state={"_recent_sent_timestamps": {}})
case("_consume_sent_timestamp", {"ts": None}, state={"_recent_sent_timestamps": {"123": 1.0}})
case("_consume_sent_timestamp", {"ts": 123}, state={})
case("_consume_sent_timestamp", {"ts": 123}, state={"_recent_sent_timestamps": {"123": 1.0, "456": 2.0}})

# ── _remember_sent_message_timestamp ──
case("_remember_sent_message_timestamp", {"timestamp": 100},
     state={"_sent_message_timestamps": {}, "_max_sent_message_timestamps": 500})
case("_remember_sent_message_timestamp", {"timestamp": None},
     state={"_sent_message_timestamps": {"9": None}, "_max_sent_message_timestamps": 500})
case("_remember_sent_message_timestamp", {"timestamp": 100},
     state={"_sent_message_timestamps": {"100": None, "50": None}, "_max_sent_message_timestamps": 500})
case("_remember_sent_message_timestamp", {"timestamp": "200"},
     state={"_sent_message_timestamps": {}, "_max_sent_message_timestamps": 500})
case("_remember_sent_message_timestamp", {"timestamp": 3},
     state={"_sent_message_timestamps": {"1": None, "2": None}, "_max_sent_message_timestamps": 2})
case("_remember_sent_message_timestamp", {"timestamp": 0},
     state={"_sent_message_timestamps": {}, "_max_sent_message_timestamps": 500})
case("_remember_sent_message_timestamp", {"timestamp": 7}, state={"_max_sent_message_timestamps": 500})

# ── _track_sent_timestamp ──
case("_track_sent_timestamp", {"rpc_result": {"timestamp": 100}}, state=send_state(),
     io={"time.monotonic": 50})
case("_track_sent_timestamp", {"rpc_result": {}}, state=send_state(), io={"time.monotonic": 50})
case("_track_sent_timestamp", {"rpc_result": {"timestamp": 0}}, state=send_state(), io={"time.monotonic": 50})
case("_track_sent_timestamp", {"rpc_result": None}, state=send_state(), io={"time.monotonic": 50})
case("_track_sent_timestamp", {"rpc_result": {"timestamp": 100}},
     state=send_state(_recent_sent_timestamps={"1": 10.0}), io={"time.monotonic": 500})
case("_track_sent_timestamp", {"rpc_result": {"timestamp": 100}},
     state=send_state(_recent_sent_timestamps={"1": 490.0, "2": 495.0}, _max_recent_timestamps=2),
     io={"time.monotonic": 500})
case("_track_sent_timestamp", {"rpc_result": {"timestamp": 100}}, state={"account": ACCT},
     io={"time.monotonic": 50})

# ── _close_client ──
case("_close_client", {}, state={"client": {"connected": True}}, io={"client.aclose": None})
case("_close_client", {}, state={"client": None}, io={"client.aclose": None})
case("_close_client", {}, state={}, io={"client.aclose": None})
case("_close_client", {}, state={"client": {"connected": True}},
     io={"client.aclose": raise_spec("RuntimeError", "close failed")})
case("_close_client", {}, state={"client": {"connected": True}},
     io={"client.aclose": raise_spec("SystemExit", "bye")})
case("_close_client", {}, state={"client": {"connected": True}},
     io={"client.aclose": raise_spec("KeyboardInterrupt", "interrupt")})

# ── _handle_sse_line ──
case("_handle_sse_line", {"line": ":keepalive"}, state={"_last_sse_activity": 0}, io={"time.time": 100})
case("_handle_sse_line", {"line": 'data: {"a":1}'}, state={"_last_sse_activity": 0},
     io={"time.time": 100, "json.loads": {"a": 1}, "_handle_envelope": None})
case("_handle_sse_line", {"line": "data:"}, state={"_last_sse_activity": 0}, io={"time.time": 100})
case("_handle_sse_line", {"line": "data:   "}, state={"_last_sse_activity": 0}, io={"time.time": 100})
case("_handle_sse_line", {"line": "other"}, state={"_last_sse_activity": 0}, io={"time.time": 100})
case("_handle_sse_line", {"line": "data: bad"}, state={"_last_sse_activity": 0},
     io={"time.time": 100, "json.loads": raise_spec("JSONDecodeError", "Expecting value"), "logger.debug": None})
case("_handle_sse_line", {"line": "data: x"}, state={"_last_sse_activity": 0},
     io={"time.time": 100, "json.loads": {"a": 1}, "_handle_envelope": raise_spec("RuntimeError", "boom"),
         "logger.exception": None})
case("_handle_sse_line", {"line": "data: x"}, state={"_last_sse_activity": 0},
     io={"time.time": 100, "json.loads": {"a": 1}, "_handle_envelope": raise_spec("SystemExit", "bye")})
case("_handle_sse_line", {"line": ":keepalive"}, state={"_last_sse_activity": 0}, io={})

# ── _unwrap_sync_message ──
case("_unwrap_sync_message", {"envelope_data": {"syncMessage": {"sentMessage": {"destinationNumber": ACCT, "timestamp": 9}}}},
     state={"_account_normalized": ACCT, "_recent_sent_timestamps": {"9": 1.0}})
case("_unwrap_sync_message", {"envelope_data": {"syncMessage": {"sentMessage": {"destinationNumber": ACCT, "timestamp": 9}}}},
     state={"_account_normalized": ACCT, "_recent_sent_timestamps": {}})
case("_unwrap_sync_message", {"envelope_data": {"syncMessage": {"sentMessage": {"destinationNumber": "+1999", "timestamp": 9}}}},
     state={"_account_normalized": ACCT, "_recent_sent_timestamps": {}})
case("_unwrap_sync_message", {"envelope_data": {"syncMessage": {"sentMessage": {"destinationNumber": "+1999", "groupInfo": {"groupId": "g1"}, "timestamp": 9}}}},
     state={"_account_normalized": ACCT, "_recent_sent_timestamps": {}})
case("_unwrap_sync_message", {"envelope_data": {"dataMessage": {"message": "hi"}}},
     state={"_account_normalized": ACCT, "_recent_sent_timestamps": {}})
case("_unwrap_sync_message", {"envelope_data": {"syncMessage": "not-dict"}},
     state={"_account_normalized": ACCT, "_recent_sent_timestamps": {}})
case("_unwrap_sync_message", {"envelope_data": {"syncMessage": {"sentMessage": {"destination": ACCT, "timestamp": 9}}}},
     state={"_account_normalized": ACCT, "_recent_sent_timestamps": {}})
case("_unwrap_sync_message", {"envelope_data": {"syncMessage": {"sentMessage": {}}}},
     state={"_account_normalized": ACCT, "_recent_sent_timestamps": {}})

# ── _apply_group_mention_rules ──
case("_apply_group_mention_rules", {"text": "hello", "data_message": {}}, state=mention_state())
case("_apply_group_mention_rules", {"text": "@%s hi" % ACCT, "data_message": {}},
     state=mention_state(require_mention=True))
case("_apply_group_mention_rules", {"text": "hi", "data_message": {"mentions": []}},
     state=mention_state(require_mention=True), io={"logger.debug": None})
case("_apply_group_mention_rules", {"text": "hi", "data_message": {"mentions": [{"number": ACCT}]}},
     state=mention_state(require_mention=True))
case("_apply_group_mention_rules", {"text": "@%s hi" % ACCT, "data_message": {}},
     state=mention_state(require_mention=True, _recipient_uuid_by_number={ACCT: UUID}))
case("_apply_group_mention_rules", {"text": None, "data_message": {}}, state=mention_state())
case("_apply_group_mention_rules", {"text": "@%s  hi" % ACCT, "data_message": {}},
     state=mention_state(require_mention=True))
case("_apply_group_mention_rules", {"text": "hi", "data_message": {"mentions": [{"uuid": UUID}]}},
     state=mention_state(require_mention=True, _recipient_uuid_by_number={ACCT: UUID}))
case("_apply_group_mention_rules", {"text": "hi", "data_message": {}},
     state=mention_state(require_mention=True, _account_normalized=""), io={"logger.debug": None})

# ── _collect_attachments ──
case("_collect_attachments", {"attachments_data": [{"id": "a1", "size": 10}]}, state={},
     io={"_fetch_attachment": sc(["/cache/a1.png", ".png"])})
case("_collect_attachments", {"attachments_data": [{"id": "a1", "contentType": "image/custom"}]}, state={},
     io={"_fetch_attachment": sc(["/cache/a1.png", ".png"])})
case("_collect_attachments", {"attachments_data": [{"id": "a1", "size": 200000000}]}, state={},
     io={"logger.warning": None})
case("_collect_attachments", {"attachments_data": [{}]}, state={}, io={})
case("_collect_attachments", {"attachments_data": [{"id": "a1"}]}, state={},
     io={"_fetch_attachment": raise_spec("RuntimeError", "fetch failed"), "logger.exception": None})
case("_collect_attachments", {"attachments_data": [{"id": "a1"}]}, state={},
     io={"_fetch_attachment": sc([None, ".png"])})
case("_collect_attachments", {"attachments_data": [{"id": "a1"}, {"id": "a2"}]}, state={},
     io={"_fetch_attachment": [["/cache/a1.mp3", ".mp3"], ["/cache/a2.pdf", ".pdf"]]})
case("_collect_attachments", {"attachments_data": [{"id": "a1"}]}, state={},
     io={"_fetch_attachment": raise_spec("SystemExit", "bye")})
case("_collect_attachments", {"attachments_data": [{"id": "a1"}]}, state={}, io={})

# ── _group_allowed ──
case("_group_allowed", {"group_id": "g1"}, state={"group_allow_from": ["*"]}, io={"logger.debug": None})
case("_group_allowed", {"group_id": "g1"}, state={"group_allow_from": ["g1"]}, io={"logger.debug": None})
case("_group_allowed", {"group_id": "g1"}, state={"group_allow_from": ["g2"]}, io={"logger.debug": None})
case("_group_allowed", {"group_id": "g1"}, state={"group_allow_from": []}, io={"logger.debug": None})
case("_group_allowed", {"group_id": "g2"}, state={"group_allow_from": ["g1", "g2"]}, io={"logger.debug": None})
case("_group_allowed", {"group_id": ""}, state={"group_allow_from": ["g1"]}, io={"logger.debug": None})
case("_group_allowed", {"group_id": "g1"}, state={}, io={"logger.debug": None})
case("_group_allowed", {"group_id": "g1"}, state={"group_allow_from": ["g2"]}, io={})

# ── _with_target ──
case("_with_target", {"params": {}, "chat_id": "group:g1"}, state={}, io={})
case("_with_target", {"params": {}, "chat_id": ACCT, "resolve": True}, state={},
     io={"_resolve_recipient": UUID})
case("_with_target", {"params": {}, "chat_id": ACCT, "resolve": False}, state={}, io={})
case("_with_target", {"params": {}, "chat_id": ACCT, "resolve": True}, state={},
     io={"_resolve_recipient": raise_spec("RuntimeError", "resolve failed")})
case("_with_target", {"params": {"account": ACCT}, "chat_id": "group:g1"}, state={}, io={})
case("_with_target", {"params": {}, "chat_id": ACCT, "resolve": True}, state={},
     io={"_resolve_recipient": ACCT})
case("_with_target", {"params": {}, "chat_id": ACCT}, state={}, io={})

# ── _rpc_send ──
case("_rpc_send", {"params": {"account": ACCT}, "fail_error": "RPC send failed"}, state=send_state(),
     io={"_rpc": {"timestamp": 7}, "time.monotonic": 50})
case("_rpc_send", {"params": {"account": ACCT}, "fail_error": "RPC send failed"}, state=send_state(),
     io={"_rpc": None})
case("_rpc_send", {"params": {"account": ACCT}, "fail_error": "RPC send failed"}, state=send_state(),
     io={"_rpc": {"results": [{"type": "FAILURE"}]}})
case("_rpc_send", {"params": {"account": ACCT}, "fail_error": "RPC send failed"}, state=send_state(),
     io={"_rpc": raise_spec("RuntimeError", "conn lost")})
case("_rpc_send", {"params": {"account": ACCT}, "fail_error": "RPC send failed"}, state=send_state(),
     io={"_rpc": raise_spec("SystemExit", "bye")})
case("_rpc_send", {"params": {"account": ACCT}, "fail_error": "RPC send failed"}, state=send_state(),
     io={"_rpc": {"results": [{"success": True}]}, "time.monotonic": 50})
case("_rpc_send", {"params": {"account": ACCT}, "fail_error": "RPC send failed"}, state=send_state(), io={})

# ── _resolve_image_path ──
case("_resolve_image_path", {"image_url": "file:///cache/a.png"}, state={},
     io={"Path.exists": True, "Path.stat": {"st_size": 100}})
case("_resolve_image_path", {"image_url": "file:///cache/a.png"}, state={},
     io={"Path.exists": False})
case("_resolve_image_path", {"image_url": "http://x/a.png"}, state={},
     io={"cache_image_from_url": "/cache/a.png", "Path.exists": True, "Path.stat": {"st_size": 100}})
case("_resolve_image_path", {"image_url": "http://x/a.png"}, state={},
     io={"cache_image_from_url": raise_spec("RuntimeError", "net down")})
case("_resolve_image_path", {"image_url": "http://x/a.png"}, state={},
     io={"cache_image_from_url": raise_spec("SystemExit", "bye")})
case("_resolve_image_path", {"image_url": "file:///cache/a.png"}, state={},
     io={"Path.exists": True, "Path.stat": {"st_size": 200000000}})
case("_resolve_image_path", {"image_url": "http://x/a.png"}, state={},
     io={"cache_image_from_url": None})
case("_resolve_image_path", {"image_url": "file:///cache/my%20file.png"}, state={},
     io={"Path.exists": True, "Path.stat": {"st_size": 5}})
case("_resolve_image_path", {"image_url": "http://x/a.png"}, state={}, io={})

# ── _send_attachment_batch ──
case("_send_attachment_batch", {"scheduler": {"m": 1}, "params": {"account": ACCT}, "n": 1, "label": "1/1"},
     state=send_state(),
     io={"_signal_send_timeout": 60, "scheduler.acquire": None, "time.monotonic": [10, 12, 50],
         "_rpc": {"timestamp": 5, "results": [{"type": "SUCCESS"}]},
         "scheduler.report_rpc_duration": None, "logger.info": None})
case("_send_attachment_batch", {"scheduler": {"m": 1}, "params": {"account": ACCT}, "n": 1, "label": "1/1"},
     state=send_state(),
     io={"_signal_send_timeout": 60, "scheduler.acquire": None, "time.monotonic": [10, 20],
         "_rpc": [raise_spec("SignalRateLimitError", "rate limited", retry_after=4),
                  raise_spec("SignalRateLimitError", "rate limited", retry_after=4)],
         "scheduler.feedback": None, "logger.warning": None, "logger.error": None})
case("_send_attachment_batch", {"scheduler": {"m": 1}, "params": {"account": ACCT}, "n": 1, "label": "1/1"},
     state=send_state(),
     io={"_signal_send_timeout": 60, "scheduler.acquire": None, "time.monotonic": [10, 20, 22, 50],
         "_rpc": [raise_spec("SignalRateLimitError", "rate limited", retry_after=4),
                  {"timestamp": 5, "results": [{"type": "SUCCESS"}]}],
         "scheduler.feedback": None, "scheduler.report_rpc_duration": None,
         "logger.warning": None, "logger.info": None})
case("_send_attachment_batch", {"scheduler": {"m": 1}, "params": {"account": ACCT}, "n": 1, "label": "1/1"},
     state=send_state(),
     io={"_signal_send_timeout": 60, "scheduler.acquire": None, "time.monotonic": [10, 12, 20, 22, 50],
         "_rpc": [{"results": [{"type": "FAILURE"}]}, {"timestamp": 5, "results": [{"type": "SUCCESS"}]}],
         "asyncio.sleep": None, "scheduler.report_rpc_duration": None,
         "logger.error": None, "logger.info": None})
case("_send_attachment_batch", {"scheduler": {"m": 1}, "params": {"account": ACCT}, "n": 1, "label": "1/1"},
     state=send_state(),
     io={"_signal_send_timeout": 60, "scheduler.acquire": None, "time.monotonic": [10, 12, 20, 22],
         "_rpc": [{"results": [{"type": "FAILURE"}]}, {"results": [{"type": "FAILURE"}]}],
         "asyncio.sleep": None, "logger.error": None, "logger.info": None})
case("_send_attachment_batch", {"scheduler": {"m": 1}, "params": {"account": ACCT}, "n": 1, "label": "1/1"},
     state=send_state(),
     io={"_signal_send_timeout": 60, "scheduler.acquire": None, "time.monotonic": [10],
         "_rpc": raise_spec("RuntimeError", "conn lost")})
case("_send_attachment_batch", {"scheduler": None, "params": {"account": ACCT}, "n": 1, "label": "1/1"},
     state=send_state(), io={"_signal_send_timeout": 60})
case("_send_attachment_batch", {"scheduler": {"m": 1}, "params": {"account": ACCT}, "n": 1, "label": "1/1"},
     state=send_state(), io={})

# ── _send_file ──
case("_send_file", {"chat_id": ACCT, "file_path": "/f.png", "caption": "hi", "fail_error": "RPC send with attachment failed"},
     state=send_state(), io={"_resolve_recipient": UUID, "_rpc": {"timestamp": 5}, "time.monotonic": 50})
case("_send_file", {"chat_id": "group:g1", "file_path": "/f.png", "caption": None, "fail_error": "RPC send with attachment failed"},
     state=send_state(), io={"_rpc": {"timestamp": 5}, "time.monotonic": 50})
case("_send_file", {"chat_id": ACCT, "file_path": "/f.png", "caption": "hi", "fail_error": "RPC send with attachment failed"},
     state=send_state(), io={"_resolve_recipient": UUID, "_rpc": None})
case("_send_file", {"chat_id": ACCT, "file_path": "/f.png", "caption": "hi", "fail_error": "RPC send with attachment failed"},
     state=send_state(), io={"_resolve_recipient": UUID, "_rpc": {"results": [{"type": "FAILURE"}]}})
case("_send_file", {"chat_id": ACCT, "file_path": "/f.png", "caption": None, "fail_error": "RPC send with attachment failed"},
     state=send_state(), io={"_resolve_recipient": UUID, "_rpc": {"timestamp": 5}, "time.monotonic": 50})
case("_send_file", {"chat_id": ACCT, "file_path": "/f.png", "caption": "hi", "fail_error": "RPC send with attachment failed"},
     state=send_state(), io={"_resolve_recipient": raise_spec("RuntimeError", "resolve failed")})

# ── _send_reaction_rpc ──
case("_send_reaction_rpc", {"chat_id": "group:g1", "params": {"account": ACCT, "emoji": "x", "targetAuthor": "+1", "targetTimestamp": 5}},
     state={}, io={"_rpc": {}})
case("_send_reaction_rpc", {"chat_id": ACCT, "params": {"account": ACCT, "emoji": "x", "targetAuthor": "+1", "targetTimestamp": 5}},
     state={}, io={"_rpc": {}})
case("_send_reaction_rpc", {"chat_id": ACCT, "params": {"account": ACCT, "emoji": "", "remove": True}},
     state={}, io={"_rpc": None})
case("_send_reaction_rpc", {"chat_id": ACCT, "params": {"account": ACCT}},
     state={}, io={"_rpc": raise_spec("RuntimeError", "conn lost")})
case("_send_reaction_rpc", {"chat_id": "group:g1", "params": {"account": ACCT}},
     state={}, io={"_rpc": {"results": []}})
case("_send_reaction_rpc", {"chat_id": ACCT, "params": {"account": ACCT}}, state={}, io={})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    low = [k for k, v in counts.items() if v < 5]
    print(f"funcs: {len(counts)}; under-5: {low}")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")


if __name__ == "__main__":
    main()