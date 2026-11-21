#!/usr/bin/env python3
"""Generate tests/fixtures/feishu_adapter_flow12.jsonl — input cases for the
Feishu adapter remainders FLOW oracle.

Every case is hand-scripted: in.io maps a seam name to a result (plain JSON
value, an ordered LIST of results, or {"__script__":true,"raise":{type,message}})
that the C universal seam and the oracle's recording stubs replay identically.
Run `python tests/gen_feishu_adapter_flow12_cases.py` to regenerate.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "feishu_adapter_flow12.jsonl"
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


def partial(v):
    return {"__partial__": v}


# ── _poll_registration ──
CREDS = {"client_id": "cid", "client_secret": "sec",
         "user_info": {"tenant_brand": "lark", "open_id": "ou1"}}
add("_poll_registration", {"device_code": "dc", "interval": 2, "expire_in": 10, "domain": "feishu"},
    io={"monotonic": [ret(0), ret(0.1)],
        "accounts_base_url": [ret("https://accounts.feishu.cn")],
        "post_registration": [ret({"error": "access_denied"})]})
add("_poll_registration", {"device_code": "dc", "interval": 2, "expire_in": 10, "domain": "feishu"},
    io={"monotonic": [ret(0), ret(0.1)],
        "accounts_base_url": [ret("https://accounts.feishu.cn")],
        "post_registration": [ret(CREDS)]})
add("_poll_registration", {"device_code": "dc", "interval": 2, "expire_in": 10, "domain": "lark"},
    io={"monotonic": [ret(0), ret(0.1)],
        "accounts_base_url": [ret("https://accounts.larksuite.com")],
        "post_registration": [ret({"client_id": "a", "client_secret": "b", "user_info": {}})]})
add("_poll_registration", {"device_code": "dc", "interval": 2, "expire_in": 10, "domain": "feishu"},
    io={"monotonic": [ret(0), ret(0.1), ret(0.2), ret(0.3)],
        "accounts_base_url": [ret("B"), ret("B")],
        "post_registration": [ret({"error": "authorization_pending"}), ret(CREDS)],
        "sleep": [ret(None)]})
add("_poll_registration", {"device_code": "dc", "interval": 2, "expire_in": 10, "domain": "feishu"},
    io={"monotonic": [ret(0), ret(0.1), ret(20)],
        "accounts_base_url": [ret("B")],
        "post_registration": [rais("OSError", "net")],
        "sleep": [ret(None)]})
add("_poll_registration", {"device_code": "dc", "interval": 2, "expire_in": 10, "domain": "feishu"},
    io={"monotonic": [ret(0), ret(0.1)],
        "accounts_base_url": [ret("B")],
        "post_registration": [rais("SystemExit", "sx")]})
add("_poll_registration", {"device_code": "dc", "interval": 2, "expire_in": 10, "domain": "feishu"},
    io={"monotonic": [ret(0), ret(0.1)],
        "accounts_base_url": [rais("SystemExit", "sx")]})
add("_poll_registration", {"device_code": "dc", "interval": 2, "expire_in": 10, "domain": "feishu"},
    io={}, note="fail-closed")


# ── _pop_text_batch ──
add("_pop_text_batch", {"key": "k1"}, io={"pop_count": [ret(1)], "pop_batch": [ret({"msg": "A"})]})
add("_pop_text_batch", {"key": "k1"}, io={"pop_count": [ret(None)], "pop_batch": [ret(None)]})
add("_pop_text_batch", {"key": "k2"}, io={"pop_count": [ret(0)], "pop_batch": [ret("hello")]})
add("_pop_text_batch", {"key": "k1"}, io={"pop_count": [rais("ValueError", "bad")]})
add("_pop_text_batch", {"key": "k1"}, io={"pop_count": [ret(1)], "pop_batch": [rais("ValueError", "bad")]})
add("_pop_text_batch", {"key": "k1"}, io={"pop_count": [rais("SystemExit", "sx")]})
add("_pop_text_batch", {"key": "k1"}, io={}, note="fail-closed")


# ── _post_registration ──
add("_post_registration", {"base_url": "https://accounts.feishu.cn",
                           "body": {"action": "init"}}, io={"post_form": [ret({"code": 0})]})
add("_post_registration", {"base_url": "https://accounts.larksuite.com",
                           "body": {"action": "poll", "device_code": "dc", "tp": "ob_app"}},
    io={"post_form": [ret({"error": "authorization_pending"})]})
add("_post_registration", {"base_url": "B", "body": {"a": "1", "b": "2"}},
    io={"post_form": [ret({})]})
add("_post_registration", {"base_url": "B", "body": {"action": "init"}},
    io={"post_form": [rais("ValueError", "bad")]})
add("_post_registration", {"base_url": "B", "body": {"action": "init"}},
    io={"post_form": [rais("SystemExit", "sx")]})
add("_post_registration", {"base_url": "B", "body": {"action": "init"}},
    io={}, note="fail-closed")


# ── _prepare_client ──
PREP = {"sdk_domain": [ret("feishu")], "build_lark_client": [ret("CLIENT")],
        "build_event_handler": [ret("HANDLER")]}
add("_prepare_client", {"domain_name": "feishu"}, io=PREP)
add("_prepare_client", {"domain_name": "lark"},
    io={"sdk_domain": [ret("lark")], "build_lark_client": [ret("C")],
        "build_event_handler": [ret(None)]})
add("_prepare_client", {"domain_name": "feishu"}, io=dict(PREP, sdk_domain=[rais("ValueError", "bad")]))
add("_prepare_client", {"domain_name": "feishu"}, io=dict(PREP, build_lark_client=[rais("ValueError", "bad")]))
add("_prepare_client", {"domain_name": "feishu"}, io=dict(PREP, build_event_handler=[rais("ValueError", "bad")]))
add("_prepare_client", {"domain_name": "feishu"}, io=dict(PREP, build_event_handler=[rais("SystemExit", "sx")]))
add("_prepare_client", {"domain_name": "feishu"}, io={}, note="fail-closed")


# ── _probe_bot_http ──
BASE = "https://open.feishu.cn"
TOK = {"tenant_access_token": "***"}
add("_probe_bot_http", {"app_id": "app", "app_secret": "sec", "domain": "feishu"},
    io={"onboard_open_base_url": [ret(BASE)], "probe_get_json": [ret({"code": 999})]})
add("_probe_bot_http", {"app_id": "app", "app_secret": "sec", "domain": "feishu"},
    io={"onboard_open_base_url": [ret(BASE)],
        "probe_get_json": [ret(TOK), ret({"bot": {"open_id": "ou"}})],
        "parse_bot_response": [ret({"bot_name": "Bot", "bot_open_id": "ou"})]})
add("_probe_bot_http", {"app_id": "app", "app_secret": "sec", "domain": "lark"},
    io={"onboard_open_base_url": [ret("https://open.larksuite.com")],
        "probe_get_json": [ret(TOK), ret({})],
        "parse_bot_response": [ret(None)]})
add("_probe_bot_http", {"app_id": "app", "app_secret": "sec", "domain": "feishu"},
    io={"onboard_open_base_url": [ret(BASE)], "probe_get_json": [rais("OSError", "net")]})
add("_probe_bot_http", {"app_id": "app", "app_secret": "sec", "domain": "feishu"},
    io={"onboard_open_base_url": [ret(BASE)],
        "probe_get_json": [ret(TOK), rais("KeyError", "k")]})
add("_probe_bot_http", {"app_id": "app", "app_secret": "sec", "domain": "feishu"},
    io={"onboard_open_base_url": [ret(BASE)],
        "probe_get_json": [ret(TOK), ret({})],
        "parse_bot_response": [rais("SystemExit", "sx")]})
add("_probe_bot_http", {"app_id": "app", "app_secret": "sec", "domain": "feishu"},
    io={"onboard_open_base_url": [rais("SystemExit", "sx")]})
add("_probe_bot_http", {"app_id": "app", "app_secret": "sec", "domain": "feishu"},
    io={}, note="fail-closed")


# ── _process_inbound_message ──
def pim_io(**over):
    d = {
        "extract_content": [ret([" hi ", "text", [], [], False, []])],
        "strip_edge_mentions": [ret("hi")],
        "build_mention_hint": [ret("")],
        "fetch_message_text": [ret("parent text")],
        "get_chat_info": [ret({"name": "Chat"})],
        "resolve_sender_profile": [ret({"user_id": "u", "user_name": "U", "user_id_alt": "ua"})],
        "resolve_source_chat_type": [ret("group")],
        "build_source": [ret("SRC")],
        "resolve_channel_prompt": [ret("")],
        "make_message_event": [ret("EVT")],
        "dispatch_inbound": [ret(None)],
    }
    d.update(over)
    return d


PIM_ARGS = {"data": {"raw": 1}, "message": {"chat_id": "c1", "parent_id": "m0", "thread_id": "t1"},
            "sender_id": {"open_id": "ou1"}, "chat_type": "group", "message_id": "m1", "is_bot": False}
add("_process_inbound_message", PIM_ARGS, io=pim_io())
add("_process_inbound_message", PIM_ARGS, io=pim_io(
    extract_content=[ret([" @Bot ", "text", [], [], False, ["ou_bot"]])],
    strip_edge_mentions=[ret("")]))
add("_process_inbound_message", PIM_ARGS, io=pim_io(
    extract_content=[ret(["/cmd", "text", [], [], False, []])],
    strip_edge_mentions=[ret("/cmd")]))
add("_process_inbound_message", PIM_ARGS, io=pim_io(
    extract_content=[ret(["", "text", ["http://img"], ["image/png"], False, []])],
    build_mention_hint=[ret("[Hint]")]))
add("_process_inbound_message", PIM_ARGS, io=pim_io(get_chat_info=[ret({})]))
add("_process_inbound_message", dict(PIM_ARGS, is_bot=True), io=pim_io())
add("_process_inbound_message", PIM_ARGS, io=pim_io(extract_content=[rais("ValueError", "bad")]))
add("_process_inbound_message", PIM_ARGS, io=pim_io(
    get_chat_info=[rais("SystemExit", "sx")]))
add("_process_inbound_message", PIM_ARGS, io=pim_io(
    dispatch_inbound=[rais("SystemExit", "sx")]))
add("_process_inbound_message", PIM_ARGS, io={}, note="fail-closed")


# ── _qr_register_inner ──
BEGIN = {"qr_url": "https://qr", "device_code": "dc", "interval": 2, "expire_in": 60}
QRIO = {
    "init_registration": [ret(None)],
    "begin_registration": [ret(BEGIN)],
    "render_qr": [ret(True)],
    "poll_registration": [ret({"app_id": "a", "app_secret": "b", "domain": "feishu"})],
    "probe_bot": [ret({"bot_name": "Bot", "bot_open_id": "ou"})],
}
add("_qr_register_inner", {"initial_domain": "feishu", "timeout_seconds": 300}, io=QRIO)
add("_qr_register_inner", {"initial_domain": "lark", "timeout_seconds": 300}, io=dict(
    QRIO, poll_registration=[ret({"app_id": "a", "app_secret": "b", "domain": "lark"})],
    probe_bot=[ret(None)]))
add("_qr_register_inner", {"initial_domain": "feishu", "timeout_seconds": 30}, io=dict(
    QRIO, render_qr=[ret(False)], install_hint=[ret("hermes setup")]))
add("_qr_register_inner", {"initial_domain": "feishu", "timeout_seconds": 30}, io=dict(
    QRIO, poll_registration=[ret(None)]))
add("_qr_register_inner", {"initial_domain": "feishu", "timeout_seconds": 300}, io=dict(
    QRIO, init_registration=[rais("ValueError", "bad")]))
add("_qr_register_inner", {"initial_domain": "feishu", "timeout_seconds": 300}, io=dict(
    QRIO, begin_registration=[rais("SystemExit", "sx")]))
add("_qr_register_inner", {"initial_domain": "feishu", "timeout_seconds": 300}, io=dict(
    QRIO, probe_bot=[rais("SystemExit", "sx")]))
add("_qr_register_inner", {"initial_domain": "feishu", "timeout_seconds": 300}, io={}, note="fail-closed")


# ── _reaction_call ──
RX = {"build_request": [ret("REQ")], "run_blocking": [ret({"data": "DATA"})],
      "response_succeeded": [ret(True)]}
add("_reaction_call", {"verb": "add", "message_id": "m1", "ident": "👍", "api": "API"}, io=RX)
add("_reaction_call", {"verb": "add", "message_id": "m1", "ident": "x", "api": "API"},
    io=dict(RX, run_blocking=[ret({})]))
add("_reaction_call", {"verb": "remove", "message_id": "m1", "ident": "x", "api": "API"},
    io=dict(RX, response_succeeded=[ret(False)]))
add("_reaction_call", {"verb": "add", "message_id": "m1", "ident": "x", "api": "API"},
    io=dict(RX, build_request=[rais("ValueError", "bad")]))
add("_reaction_call", {"verb": "add", "message_id": "m1", "ident": "x", "api": "API"},
    io=dict(RX, run_blocking=[rais("ValueError", "bad")]))
add("_reaction_call", {"verb": "add", "message_id": "m1", "ident": "x", "api": "API"},
    io=dict(RX, response_succeeded=[rais("SystemExit", "sx")]))
add("_reaction_call", {"verb": "add", "message_id": "m1", "ident": "x", "api": "API"},
    io={}, note="fail-closed")


# ── _read_binary_response ──
add("_read_binary_response", {"response": {}}, io={})
add("_read_binary_response", {"response": {"file": {"has_getvalue": True}}},
    io={"getvalue": [ret("bytes-v")]})
add("_read_binary_response", {"response": {"file": {"has_getvalue": False}}},
    io={"read": [ret("stream-v")]})
add("_read_binary_response", {"response": {"file": {"has_getvalue": True}}},
    io={"getvalue": [ret("")]})
add("_read_binary_response", {"response": {"file": {"has_getvalue": True}}},
    io={"getvalue": [rais("ValueError", "bad")]})
add("_read_binary_response", {"response": {"file": {"has_getvalue": False}}},
    io={"read": [rais("SystemExit", "sx")]})
add("_read_binary_response", {"response": {"file": {"has_getvalue": True}}},
    io={}, note="fail-closed")


# ── _read_limited_feishu_webhook_body ──
add("_read_limited_feishu_webhook_body", {"max_bytes": 10}, io={"readexactly": [ret("hello")]})
add("_read_limited_feishu_webhook_body", {"max_bytes": 10}, io={"readexactly": [ret("")]})
add("_read_limited_feishu_webhook_body", {"max_bytes": 3}, io={"readexactly": [partial("abc")]})
add("_read_limited_feishu_webhook_body", {"max_bytes": 2}, io={"readexactly": [partial("abcd")]})
add("_read_limited_feishu_webhook_body", {"max_bytes": 2}, io={"readexactly": [ret("abcd")]})
add("_read_limited_feishu_webhook_body", {"max_bytes": 5}, io={"readexactly": [rais("ValueError", "bad")]})
add("_read_limited_feishu_webhook_body", {"max_bytes": 5}, io={"readexactly": [rais("SystemExit", "sx")]})
add("_read_limited_feishu_webhook_body", {"max_bytes": 5}, io={}, note="fail-closed")


# ── _should_batch_media_event ──
add("_should_batch_media_event", {"event": {"media_urls": ["u"], "message_type": "photo"}}, io={})
add("_should_batch_media_event", {"event": {"media_urls": ["u"], "message_type": "video"}}, io={})
add("_should_batch_media_event", {"event": {"media_urls": ["u"], "message_type": "document"}}, io={})
add("_should_batch_media_event", {"event": {"media_urls": ["u"], "message_type": "audio"}}, io={})
add("_should_batch_media_event", {"event": {"media_urls": [], "message_type": "photo"}}, io={})
add("_should_batch_media_event", {"event": {"media_urls": ["u"], "message_type": "text"}}, io={})
add("_should_batch_media_event", {"event": {"message_type": "photo"}}, io={})


# ── _resolve_media_message_type ──
for mt in ["image/png", "IMAGE/JPEG", "audio/opus", "video/mp4", "application/pdf", ""]:
    add("_resolve_media_message_type", {"media_type": mt, "default": "document"}, io={})
add("_resolve_media_message_type", {"media_type": None, "default": "photo"}, io={})
add("_resolve_media_message_type", {"media_type": "application/zip", "default": "photo"}, io={})


# ── _queue_empty ──
add("_queue_empty", {"events": []}, io={})
add("_queue_empty", {"events": ["a"]}, io={})
add("_queue_empty", {"events": ["a", "b"]}, io={})
add("_queue_empty", {}, io={})
add("_queue_empty", {"events": None}, io={})
add("_queue_empty", {"events": [1, 2, 3]}, io={})


# ── _render_qr ──
add("_render_qr", {"available": False, "url": "https://qr"}, io={})
add("_render_qr", {"available": True, "url": "https://qr"},
    io={"qr_construct": [ret(None)], "qr_add_data": [ret(None)],
        "qr_make": [ret(None)], "qr_print_ascii": [ret(None)]})
add("_render_qr", {"available": True, "url": "https://l"},
    io={"qr_construct": [ret(None)], "qr_add_data": [rais("ValueError", "bad")]})
add("_render_qr", {"available": True, "url": "https://l"},
    io={"qr_construct": [ret(None)], "qr_add_data": [ret(None)],
        "qr_make": [rais("ValueError", "bad")]})
add("_render_qr", {"available": True, "url": "https://l"},
    io={"qr_construct": [ret(None)], "qr_add_data": [ret(None)],
        "qr_make": [ret(None)], "qr_print_ascii": [rais("SystemExit", "sx")]})
add("_render_qr", {"available": True, "url": "https://l"}, io={}, note="fail-closed")


OUT.parent.mkdir(parents=True, exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for case in CASES:
        f.write(json.dumps(case, ensure_ascii=False) + "\n")
print(f"wrote {len(CASES)} cases to {OUT}")
