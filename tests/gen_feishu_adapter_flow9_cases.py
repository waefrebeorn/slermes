#!/usr/bin/env python3
"""Generate tests/fixtures/feishu_adapter_flow9.jsonl — input cases for the
Feishu adapter extraction/media/queue/settings/limit/retry FLOW oracle.

Every case is hand-scripted: in.io maps a seam name to a result (plain JSON
value, an ordered LIST of results, or {"__script__":true,"raise":{type,message}})
that the C universal seam and the oracle's recording stubs replay identically.
Run `python tests/gen_feishu_adapter_flow9_cases.py` to regenerate.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "feishu_adapter_flow9.jsonl"
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


def seq(*vals):
    return list(vals)


# ── _build_lark_client (seam: lark_build) ──
add("_build_lark_client", {"app_id": "a", "app_secret": "s", "sdk_domain": "feishu"},
    io={"lark_build": [ret("CLIENT")]})
add("_build_lark_client", {"app_id": "a", "app_secret": "s", "sdk_domain": "lark"},
    io={"lark_build": [ret({"id": 1})]})
add("_build_lark_client", {"app_id": "", "app_secret": "", "sdk_domain": ""},
    io={"lark_build": [ret(None)]})
add("_build_lark_client", {"app_id": "a", "app_secret": "s"},
    io={"lark_build": [ret("C2")]})
add("_build_lark_client", {"app_id": "a", "app_secret": "s", "sdk_domain": "x"},
    io={"lark_build": [rais("ValueError", "build down")]})
add("_build_lark_client", {"app_id": "a", "app_secret": "s", "sdk_domain": "x"},
    io={"lark_build": [rais("SystemExit", "sx")]})
add("_build_lark_client", {"app_id": "a", "app_secret": "s", "sdk_domain": "x"},
    io={}, note="fail-closed")

# ── _normalize (seams: bot_identity, normalize) ──
NM = {"message_type": "text", "raw_content": "hi", "mentions": []}
def nm_ok(bot="BOT", norm="NORM"):
    return {"bot_identity": [ret(bot)], "normalize": [ret(norm)]}
add("_normalize", NM, io=nm_ok())
add("_normalize", {"message_type": "image", "raw_content": None, "mentions": [{"key": "u"}]}, io=nm_ok())
add("_normalize", {"message_type": "", "raw_content": "", "mentions": None}, io=nm_ok(norm=None))
add("_normalize", NM, io={"bot_identity": [rais("ValueError", "bot down")]})
add("_normalize", NM, io={"bot_identity": [ret("B")], "normalize": [rais("RuntimeError", "norm down")]})
add("_normalize", NM, io={"bot_identity": [ret("B")], "normalize": [rais("SystemExit", "sx")]})
add("_normalize", NM, io={}, note="fail-closed")

# ── _webhook_reject (seams: record_anomaly, json_response, web_response) ──
add("_webhook_reject", {"remote_ip": "1.2.3.4", "anomaly": "415", "status": 415, "text": "bad"},
    io={"record_anomaly": [ret(None)], "web_response": [ret("RESP")]})
add("_webhook_reject", {"remote_ip": "1.2.3.4", "anomaly": "415", "status": 415,
                        "text": "bad", "json_msg": "nope"},
    io={"record_anomaly": [ret(None)], "json_response": [ret("JRESP")]})
add("_webhook_reject", {"remote_ip": "", "anomaly": "413", "status": 413, "text": None},
    io={"record_anomaly": [ret(None)], "web_response": [ret(None)]})
add("_webhook_reject", {"remote_ip": "1.2.3.4", "anomaly": "415", "status": 415,
                        "text": "bad", "json_msg": None},
    io={"record_anomaly": [ret(None)], "web_response": [ret("R")]})
add("_webhook_reject", {"remote_ip": "1.2.3.4", "anomaly": "415", "status": 415, "text": "bad"},
    io={"record_anomaly": [rais("ValueError", "rec down")]})
add("_webhook_reject", {"remote_ip": "1.2.3.4", "anomaly": "415", "status": 415, "text": "bad"},
    io={"record_anomaly": [ret(None)], "web_response": [rais("SystemExit", "sx")]})
add("_webhook_reject", {"remote_ip": "1.2.3.4", "anomaly": "415", "status": 415, "text": "bad"},
    io={}, note="fail-closed")

# ── _download_remote_image (seams: guess_ext, cache_img) ──
def dri_ok(ext=".jpg", cached="/c/a.jpg"):
    return {"guess_ext": [ret(ext)], "cache_img": [ret(cached)]}
add("_download_remote_image", {"image_url": "https://x/a.png"}, io=dri_ok())
add("_download_remote_image", {"image_url": "https://x/a.png?q=1"}, io=dri_ok(ext=".png"))
add("_download_remote_image", {"image_url": "https://x/noext"}, io=dri_ok(ext=".jpg", cached=None))
add("_download_remote_image", {"image_url": ""}, io=dri_ok())
add("_download_remote_image", {"image_url": "https://x/a.png"}, io={"guess_ext": [rais("ValueError", "guess down")]})
add("_download_remote_image", {"image_url": "https://x/a.png"},
    io={"guess_ext": [ret(".png")], "cache_img": [rais("SystemExit", "sx")]})
add("_download_remote_image", {"image_url": "https://x/a.png"}, io={}, note="fail-closed")


# ── _apply_settings (seam: set_field; 30 mirrored fields + 3 overrides) ──
def as_io(n=33, raise_at=None, kind="ValueError"):
    vals = [ret(None) for _ in range(n)]
    if raise_at is not None:
        vals[raise_at] = rais(kind, "set down")
    return {"set_field": vals}


BASE_SETTINGS = {
    "app_id": "a", "app_secret": "s", "domain_name": "feishu",
    "group_policy": "open", "allowed_group_users": ["u1", "u2"],
    "admins": ["ad1"], "default_group_policy": "",
}
add("_apply_settings", {"settings": dict(BASE_SETTINGS, default_group_policy="admin_only")},
    io=as_io())
add("_apply_settings", {"settings": BASE_SETTINGS}, io=as_io())
add("_apply_settings", {"settings": {"allowed_group_users": [], "admins": [],
                                     "group_policy": "open", "default_group_policy": ""}},
    io=as_io())
add("_apply_settings", {"settings": dict(BASE_SETTINGS, allowed_group_users=["z"], admins=[])},
    io=as_io())
add("_apply_settings", {"settings": BASE_SETTINGS}, io=as_io(raise_at=0))
add("_apply_settings", {"settings": BASE_SETTINGS}, io=as_io(raise_at=32, kind="SystemExit"))
add("_apply_settings", {"settings": BASE_SETTINGS}, io={}, note="fail-closed")

# ── _check_webhook_rate_limit (seams: now, counts_get, counts_len, counts_items, counts_del, counts_set, log_warn) ──
add("_check_webhook_rate_limit", {"rate_key": "k"},
    io={"now": [ret(100.0)], "counts_get": [ret([5, 50])], "counts_set": [ret(None)]})
add("_check_webhook_rate_limit", {"rate_key": "k"},
    io={"now": [ret(100.0)], "counts_get": [ret([120, 50])]})
add("_check_webhook_rate_limit", {"rate_key": "k"},
    io={"now": [ret(100.0)], "counts_get": [ret([5, 0])], "counts_len": [ret(10)],
        "counts_set": [ret(None)]})
add("_check_webhook_rate_limit", {"rate_key": "k"},
    io={"now": [ret(100.0)], "counts_get": [ret([5, 0]), ret(None)],
        "counts_len": [ret(4096), ret(4095)],
        "counts_items": [ret([["k1", [1, 0]], ["k2", [1, 90]]])],
        "counts_del": [ret(None)], "counts_set": [ret(None)]})
add("_check_webhook_rate_limit", {"rate_key": "k"},
    io={"now": [ret(100.0)], "counts_get": [ret(None), ret(None)],
        "counts_len": [ret(4096), ret(4096)],
        "counts_items": [ret([["k2", [1, 90]]])], "log_warn": [ret(None)]})
add("_check_webhook_rate_limit", {"rate_key": "k"},
    io={"now": [rais("ValueError", "clock down")]})
add("_check_webhook_rate_limit", {"rate_key": "k"},
    io={"now": [ret(100.0)], "counts_get": [ret([5, 50])],
        "counts_set": [rais("SystemExit", "sx")]})
add("_check_webhook_rate_limit", {"rate_key": "k"}, io={}, note="fail-closed")

# ── _download_feishu_message_resources (seams: dl_img, dl_res) ──
add("_download_feishu_message_resources",
    {"message_id": "m1", "normalized": {"image_keys": ["i1", "i2"], "media_refs": []}},
    io={"dl_img": [ret(["/a.jpg", "image/jpeg"]), ret(["", ""])]})
add("_download_feishu_message_resources",
    {"message_id": "m1", "normalized": {"image_keys": [],
        "media_refs": [{"file_key": "f1", "resource_type": "file", "file_name": "n"}]}},
    io={"dl_res": [ret(["/f1", "application/pdf"])]})
add("_download_feishu_message_resources",
    {"message_id": "m1", "normalized": {"image_keys": ["i1"],
        "media_refs": [{"file_key": "f1", "resource_type": "audio", "file_name": "n"}]}},
    io={"dl_img": [ret(["/a.jpg", "image/jpeg"])],
        "dl_res": [ret(["/f1", "audio/ogg"])]})
add("_download_feishu_message_resources",
    {"message_id": "m1", "normalized": {"image_keys": [], "media_refs": []}}, io={})
add("_download_feishu_message_resources",
    {"message_id": "m1", "normalized": {"image_keys": ["i1"], "media_refs": []}},
    io={"dl_img": [rais("ValueError", "img down")]})
add("_download_feishu_message_resources",
    {"message_id": "m1", "normalized": {"image_keys": [],
        "media_refs": [{"file_key": "f1", "resource_type": "file", "file_name": "n"}]}},
    io={"dl_res": [rais("SystemExit", "sx")]})
add("_download_feishu_message_resources",
    {"message_id": "m1", "normalized": {"image_keys": ["i1"], "media_refs": []}},
    io={}, note="fail-closed")

# ── _enqueue_pending_inbound_event (seams: lock_enter, queued_len, queued_pop, log_error, queued_append, lock_exit, log_warning) ──
add("_enqueue_pending_inbound_event",
    {"data": "e1", "max_depth": 2, "pending_drain_scheduled": False},
    io={"lock_enter": [ret(None)], "queued_len": [ret(0), ret(1)],
        "queued_append": [ret(None)], "lock_exit": [ret(None)], "log_warning": [ret(None)]})
add("_enqueue_pending_inbound_event",
    {"data": "e1", "max_depth": 2, "pending_drain_scheduled": True},
    io={"lock_enter": [ret(None)], "queued_len": [ret(2), ret(2)],
        "queued_pop": [ret({"event": {"message": {"message_id": "m1"}}})],
        "log_error": [ret(None)], "queued_append": [ret(None)],
        "lock_exit": [ret(None)], "log_warning": [ret(None)]})
add("_enqueue_pending_inbound_event",
    {"data": "e1", "max_depth": 2, "pending_drain_scheduled": False},
    io={"lock_enter": [ret(None)], "queued_len": [ret(2), ret(2)],
        "queued_pop": [ret({"event": {}})], "log_error": [ret(None)],
        "queued_append": [ret(None)], "lock_exit": [ret(None)], "log_warning": [ret(None)]})
add("_enqueue_pending_inbound_event",
    {"data": "e1", "max_depth": 2, "pending_drain_scheduled": False},
    io={"lock_enter": [ret(None)], "queued_len": [rais("ValueError", "len down")],
        "lock_exit": [ret(None)]})
add("_enqueue_pending_inbound_event",
    {"data": "e1", "max_depth": 2, "pending_drain_scheduled": False},
    io={"lock_enter": [ret(None)], "queued_len": [ret(2), ret(2)],
        "queued_pop": [ret({"event": {}})], "log_error": [rais("RuntimeError", "log down")],
        "lock_exit": [ret(None)]})
add("_enqueue_pending_inbound_event",
    {"data": "e1", "max_depth": 2, "pending_drain_scheduled": False},
    io={"lock_enter": [ret(None)], "queued_len": [ret(0), ret(1)],
        "queued_append": [rais("SystemExit", "sx")], "lock_exit": [ret(None)]})
add("_enqueue_pending_inbound_event",
    {"data": "e1", "max_depth": 2, "pending_drain_scheduled": False},
    io={}, note="fail-closed")


# ── _download_feishu_image (seams: fetch_resource, resp_success, read_binary, get_header, guess_file_ext, cache_img_bytes, default_img_media, norm_media) ──
def dfi_ok(**over):
    io = {
        "fetch_resource": [ret({"file_name": "a.jpg"})],
        "resp_success": [ret(True)], "read_binary": [ret("BYTES")],
        "get_header": [ret("image/jpeg")], "guess_file_ext": [ret(".jpg")],
        "cache_img_bytes": [ret("/c/a.jpg")], "default_img_media": [ret("image/jpeg")],
        "norm_media": [ret("image/jpeg")],
    }
    io.update(over)
    return io


add("_download_feishu_image",
    {"client_present": True, "message_id": "m1", "image_key": "img1"}, io=dfi_ok())
add("_download_feishu_image", {"client_present": False, "message_id": "m1"}, io={})
add("_download_feishu_image", {"client_present": True, "message_id": ""}, io={})
add("_download_feishu_image",
    {"client_present": True, "message_id": "m1"},
    io=dfi_ok(fetch_resource=[ret(None)]))
add("_download_feishu_image",
    {"client_present": True, "message_id": "m1"},
    io=dfi_ok(resp_success=[ret(False)]))
add("_download_feishu_image",
    {"client_present": True, "message_id": "m1"},
    io=dfi_ok(read_binary=[ret("")]))
add("_download_feishu_image",
    {"client_present": True, "message_id": "m1"},
    io=dfi_ok(fetch_resource=[rais("ValueError", "fetch down")]))
add("_download_feishu_image",
    {"client_present": True, "message_id": "m1"},
    io=dfi_ok(fetch_resource=[rais("SystemExit", "sx")]))
add("_download_feishu_image",
    {"client_present": True, "message_id": "m1"}, io={}, note="fail-closed")

# ── _download_feishu_message_resource ──
def dfr_ok(**over):
    io = {
        "fetch_resource": [ret({"file_name": "a.jpg"})], "resp_success": [ret(True)],
        "read_binary": [ret("BYTES")], "get_header": [ret("image/jpeg")],
        "guess_media_name": [ret("image/jpeg")], "norm_media": [ret("image/png")],
        "guess_file_ext": [ret(".png")], "cache_img_bytes": [ret("/c/a.png")],
        "default_img_media": [ret("image/png")],
    }
    io.update(over)
    return io


add("_download_feishu_message_resource",
    {"client_present": True, "message_id": "m1", "file_key": "f1",
     "resource_type": "file", "fallback_filename": ""}, io=dfr_ok())
add("_download_feishu_message_resource",
    {"client_present": True, "message_id": "m1", "file_key": "f1",
     "resource_type": "file", "fallback_filename": ""},
    io=dfr_ok(fetch_resource=[ret({"file_name": "a.pdf"})],
              get_header=[ret("application/pdf")],
              guess_media_name=[ret("application/pdf")],
              norm_media=[ret("application/pdf")],
              cache_doc_bytes=[ret("/c/a.pdf")]))
add("_download_feishu_message_resource",
    {"client_present": True, "message_id": "m1", "file_key": "f1",
     "resource_type": "audio", "fallback_filename": ""},
    io=dfr_ok(fetch_resource=[ret(None), ret({"file_name": "a.pdf"})],
              get_header=[ret("application/pdf")],
              guess_media_name=[ret("application/pdf")],
              norm_media=[ret("application/pdf")],
              cache_doc_bytes=[ret("/c/a.pdf")]))
add("_download_feishu_message_resource",
    {"client_present": True, "message_id": "m1", "resource_type": "file"},
    io=dfr_ok(fetch_resource=[ret(None)]))
add("_download_feishu_message_resource",
    {"client_present": True, "message_id": "m1", "resource_type": "file"},
    io=dfr_ok(fetch_resource=[rais("ValueError", "fetch down")]))
add("_download_feishu_message_resource",
    {"client_present": True, "message_id": "m1", "resource_type": "file"},
    io=dfr_ok(fetch_resource=[rais("SystemExit", "sx")]))
add("_download_feishu_message_resource", {"client_present": False, "message_id": "m1"}, io={})
add("_download_feishu_message_resource",
    {"client_present": True, "message_id": "m1", "resource_type": "file"}, io={}, note="fail-closed")

# ── _extract_message_content ──
def emc_ok(norm=None, res=None, itype="text", stripped="hi", iscmd=False, docs=()):
    io = {
        "normalize_msg": [ret(norm or {"text_content": "hi", "mentions": []})],
        "fetch_resources": [ret(res if res is not None else [[], []])],
        "resolve_type": [ret(itype)],
        "strip_mentions": [ret(stripped)],
        "caption_is_command": [ret(iscmd)],
    }
    if docs:
        io["extract_doc"] = [ret(d) for d in docs]
    return io


EMC_MSG = {"content": "raw", "message_type": "text", "message_id": "m1", "mentions": []}
add("_extract_message_content", {"message": EMC_MSG}, io=emc_ok())
add("_extract_message_content", {"message": EMC_MSG},
    io=emc_ok(res=[["/a.txt"], ["text/plain"]], docs=["EXTRACT"]))
add("_extract_message_content", {"message": EMC_MSG},
    io=emc_ok(res=[["/a.txt"], ["text/plain"]], iscmd=True))
add("_extract_message_content", {"message": EMC_MSG},
    io=emc_ok(res=[["/a.txt", "/b.md"], ["text/plain", "text/markdown"]], docs=["X", "Y"]))
add("_extract_message_content", {"message": EMC_MSG},
    io=emc_ok(norm={"text_content": "", "mentions": [{"id": "u"}]},
              res=[["/a.txt"], ["text/plain"]], docs=["ONLY"]))
add("_extract_message_content", {"message": EMC_MSG},
    io={"normalize_msg": [rais("ValueError", "norm down")]})
add("_extract_message_content", {"message": EMC_MSG},
    io=emc_ok(res=[[], []], itype="photo", iscmd=False), note="_no_media")
add("_extract_message_content", {"message": EMC_MSG},
    io=emc_ok(res=[[], []], itype="text", iscmd=False,
              docs=[]), note="_plain")
add("_extract_message_content", {"message": EMC_MSG}, io={}, note="fail-closed")


# ── _enqueue_text_event ──
TEV = {"text": "hello", "media_urls": [], "media_types": [], "media_text_inlined": [],
       "timestamp": 1, "message_id": "m", "source": {"message_id": "s"}}
def te_empty():
    return {"text_key": [ret("K")], "batches_get": [ret(None)],
            "batches_set": [ret(None)], "counts_set": [ret(None)], "schedule_flush": [ret(None)]}
add("_enqueue_text_event", {"event": TEV, "max_messages": 5, "max_chars": 100}, io=te_empty())
add("_enqueue_text_event", {"event": TEV, "max_messages": 5, "max_chars": 100},
    io={"text_key": [ret("K")], "batches_get": [ret({"text": "old"})],
        "batch_compatible": [ret(False)], "flush_now": [ret(None)],
        "batches_set": [ret(None)], "counts_set": [ret(None)], "schedule_flush": [ret(None)]})
add("_enqueue_text_event", {"event": TEV, "max_messages": 5, "max_chars": 100},
    io={"text_key": [ret("K")], "batches_get": [ret({"text": "old"})],
        "batch_compatible": [ret(True)], "counts_get": [ret(1)],
        "counts_set": [ret(None)], "schedule_flush": [ret(None)]})
add("_enqueue_text_event", {"event": TEV, "max_messages": 5, "max_chars": 100},
    io={"text_key": [ret("K")], "batches_get": [ret({"text": "old"})],
        "batch_compatible": [ret(True)], "counts_get": [ret(None)],
        "counts_set": [ret(None)], "schedule_flush": [ret(None)]})
add("_enqueue_text_event", {"event": TEV, "max_messages": 1, "max_chars": 100},
    io={"text_key": [ret("K")], "batches_get": [ret({"text": "old"})],
        "batch_compatible": [ret(True)], "counts_get": [ret(1)],
        "flush_now": [ret(None)], "batches_set": [ret(None)],
        "counts_set": [ret(None)], "schedule_flush": [ret(None)]})
add("_enqueue_text_event", {"event": TEV, "max_messages": 5, "max_chars": 3},
    io={"text_key": [ret("K")], "batches_get": [ret({"text": "old"})],
        "batch_compatible": [ret(True)], "counts_get": [ret(1)],
        "flush_now": [ret(None)], "batches_set": [ret(None)],
        "counts_set": [ret(None)], "schedule_flush": [ret(None)]})
add("_enqueue_text_event", {"event": TEV, "max_messages": 5, "max_chars": 100},
    io={"text_key": [ret("K")], "batches_get": [ret({"text": "old"})],
        "batch_compatible": [rais("ValueError", "compat down")]})
add("_enqueue_text_event", {"event": TEV, "max_messages": 5, "max_chars": 100},
    io={"text_key": [ret("K")], "batches_get": [ret({"text": "old"})],
        "batch_compatible": [ret(True)], "counts_get": [rais("SystemExit", "sx")]})
add("_enqueue_text_event", {"event": TEV, "max_messages": 5, "max_chars": 100}, io={}, note="fail-closed")

# ── _enqueue_media_event ──
MEV = {"text": "cap", "media_urls": ["u"], "media_types": ["t"], "media_text_inlined": [False],
       "timestamp": 1, "message_id": "m", "source": {"message_id": "s"}}
def me_empty():
    return {"media_key": [ret("MK")], "mbatches_get": [ret(None)],
            "mbatches_set": [ret(None)], "schedule_mflush": [ret(None)]}
add("_enqueue_media_event", {"event": MEV}, io=me_empty())
add("_enqueue_media_event", {"event": MEV},
    io={"media_key": [ret("MK")], "mbatches_get": [ret({"text": "old"})],
        "media_compatible": [ret(False)], "mflush_now": [ret(None)],
        "mbatches_set": [ret(None)], "schedule_mflush": [ret(None)]})
add("_enqueue_media_event", {"event": MEV},
    io={"media_key": [ret("MK")], "mbatches_get": [ret({"text": "old"})],
        "media_compatible": [ret(True)], "merge_caption": [ret("MERGED")],
        "schedule_mflush": [ret(None)]})
add("_enqueue_media_event", {"event": dict(MEV, text="")},
    io={"media_key": [ret("MK")], "mbatches_get": [ret({"text": "old"})],
        "media_compatible": [ret(True)], "schedule_mflush": [ret(None)]})
add("_enqueue_media_event", {"event": MEV},
    io={"media_key": [ret("MK")], "mbatches_get": [ret({"text": "old"})],
        "media_compatible": [rais("ValueError", "compat down")]})
add("_enqueue_media_event", {"event": MEV},
    io={"media_key": [ret("MK")], "mbatches_get": [ret({"text": "old"})],
        "media_compatible": [ret(True)], "merge_caption": [rais("SystemExit", "sx")]})
add("_enqueue_media_event", {"event": MEV},
    io={"media_key": [ret("MK")], "mbatches_get": [ret({"text": "old"})],
        "media_compatible": [ret(False)], "mflush_now": [rais("RuntimeError", "flush down")]})
add("_enqueue_media_event", {"event": MEV}, io={}, note="fail-closed")


# ── _feishu_send_with_retry ──
def fsr_base(**over):
    io = {"send_raw": [ret("RESP")]}
    io.update(over)
    return io


add("_feishu_send_with_retry",
    {"chat_id": "c1", "msg_type": "text", "payload": "p", "reply_to": None, "metadata": None},
    io=fsr_base())
add("_feishu_send_with_retry",
    {"chat_id": "c1", "msg_type": "text", "payload": "p", "reply_to": "r1", "metadata": None},
    io=fsr_base(response_succeeded=[ret(True)]))
add("_feishu_send_with_retry",
    {"chat_id": "c1", "msg_type": "text", "payload": "p", "reply_to": "r1", "metadata": None},
    io=fsr_base(send_raw=[ret({"code": 999})], response_succeeded=[ret(False)]))
add("_feishu_send_with_retry",
    {"chat_id": "c1", "msg_type": "text", "payload": "p", "reply_to": "r1",
     "metadata": {"thread_id": "t1"}},
    io=fsr_base(send_raw=[ret({"code": 230011})], response_succeeded=[ret(False)],
                log_warn=[ret(None)]))
add("_feishu_send_with_retry",
    {"chat_id": "c1", "msg_type": "text", "payload": "p", "reply_to": "r1", "metadata": None},
    io=fsr_base(send_raw=[ret({"code": 230011}), ret("RESP2")],
                response_succeeded=[ret(False)], log_warn=[ret(None)]))
add("_feishu_send_with_retry",
    {"chat_id": "c1", "msg_type": "text", "payload": "p", "reply_to": None, "metadata": None},
    io=fsr_base(send_raw=[rais("ValueError", "down"), ret("RESP")],
                log_warn=[ret(None)], sleep=[ret(None)]))
add("_feishu_send_with_retry",
    {"chat_id": "c1", "msg_type": "text", "payload": "p", "reply_to": None, "metadata": None},
    io=fsr_base(send_raw=seq(rais("ValueError", "d1"), rais("ValueError", "d2"),
                             rais("ValueError", "d3")),
                log_warn=seq(ret(None), ret(None)), sleep=seq(ret(None), ret(None))))
add("_feishu_send_with_retry",
    {"chat_id": "c1", "msg_type": "post", "payload": "p", "reply_to": None, "metadata": None},
    io=fsr_base(send_raw=[rais("ValueError", "content format of the post type is incorrect")]))
add("_feishu_send_with_retry",
    {"chat_id": "c1", "msg_type": "text", "payload": "p", "reply_to": None, "metadata": None},
    io=fsr_base(send_raw=[rais("SystemExit", "sx")]))
add("_feishu_send_with_retry",
    {"chat_id": "c1", "msg_type": "post", "payload": "p", "reply_to": None, "metadata": None},
    io={}, note="fail-closed")


# ── _download_remote_document ──
def drd_ok(**over):
    io = {
        "is_safe": [ret(True)], "client_enter": [ret(None)], "stream_enter": [ret(None)],
        "raise_status": [ret(None)], "headers_get": [ret("text/plain")],
        "read_body": [ret("BODY")], "stream_exit": [ret(None)], "client_exit": [ret(None)],
        "derive_name": [ret("a.pdf")], "cache_doc": [ret("/c/a.pdf")],
    }
    io.update(over)
    return io


DRD = {"file_url": "https://x/a.pdf", "default_ext": ".pdf", "preferred_name": "a"}
add("_download_remote_document", DRD, io=drd_ok())
add("_download_remote_document", DRD, io=drd_ok(is_safe=[ret(False)]))
add("_download_remote_document", DRD, io=drd_ok(raise_status=[rais("ValueError", "status down")]))
add("_download_remote_document", DRD, io=drd_ok(read_body=[rais("SystemExit", "sx")]))
add("_download_remote_document", DRD, io=drd_ok(stream_exit=[rais("RuntimeError", "exit down")]))
add("_download_remote_document", DRD, io=drd_ok(derive_name=[rais("ValueError", "name down")]))
add("_download_remote_document", DRD, io=drd_ok(cache_doc=[rais("SystemExit", "sx")]))
add("_download_remote_document", DRD, io=drd_ok(headers_get=[ret("")], read_body=[ret("BODY")]))
add("_download_remote_document", DRD, io={}, note="fail-closed")


# ── _drain_pending_inbound_events ──
def locks(n):
    return {"lock_enter": [ret(None) for _ in range(n)],
            "lock_exit": [ret(None) for _ in range(n)]}


add("_drain_pending_inbound_events", {},
    io=dict({"running": [ret(False)], "take_batch": [ret([])]}, **locks(2)))
add("_drain_pending_inbound_events", {},
    io=dict({"running": [ret(False)], "take_batch": [ret(["e1"])],
             "log_warn": [ret(None)]}, **locks(2)))
add("_drain_pending_inbound_events", {"loop": "L"},
    io=dict({"running": [ret(True)], "loop_accepts": [ret(True)],
             "take_batch": [ret([])], "is_empty": [ret(True)]}, **locks(3)))
add("_drain_pending_inbound_events", {"loop": "L"},
    io=dict({"running": [ret(True)], "loop_accepts": [ret(True)],
             "take_batch": [ret(["e1"])], "handle_event": [ret(None)],
             "submit_on_loop": [ret(True)], "log_info": [ret(None)],
             "is_empty": [ret(True)]}, **locks(3)))
add("_drain_pending_inbound_events", {"loop": "L"},
    io=dict({"running": [ret(True), ret(False)], "loop_accepts": [ret(True)],
             "take_batch": [ret(["e1"]), ret([])], "handle_event": [ret(None)],
             "submit_on_loop": [ret(False)], "requeue": [ret(None)],
             "is_empty": [ret(False)]}, **locks(5)))
add("_drain_pending_inbound_events", {"loop": "L"},
    io=dict({"running": [ret(True), ret(False)], "loop_accepts": [ret(False)],
             "poll_sleep": [ret(None)], "take_batch": [ret([])]}, **locks(2)))
add("_drain_pending_inbound_events", {},
    io=dict({"running": [rais("SystemExit", "sx")]}, **locks(1)))
add("_drain_pending_inbound_events", {}, io={}, note="fail-closed")


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"WROTE {OUT}: {len(CASES)} cases, {len(counts)} funcs")
    low = [f for f, n in counts.items() if n < 6]
    print("funcs with <6 cases:", low)
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    if low:
        raise SystemExit(1)
    try:
        import sys
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import oracle_feishu_adapter_flow9 as o
        bad = set()
        for c in CASES:
            bad |= set(c["in"].get("io") or {}) - o.SEAMED
        if bad:
            print("fixture io keys without a seam:", sorted(bad))
            raise SystemExit(1)
        print("seam keys verified against", len(o.SEAMED), "C seams")
    except ImportError as exc:
        print("oracle import skipped:", exc)


if __name__ == "__main__":
    main()
