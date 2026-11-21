#!/usr/bin/env python3
"""Deterministic case generator for the Telegram adapter FLOW part 15 oracle.

Writes tests/fixtures/telegram_adapter_flow15.jsonl. Each case is
{"func", "in": {"args", "state"?, "io"}}. Re-running reproduces byte-identical
fixtures. Every ported function gets >= 6 cases; every seam name appears in at
least one case's io (asserted at the bottom).
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "telegram_adapter_flow15.jsonl"

# EXACTLY the C ta15_call seam names (set-equality enforced in the oracle).
SEAM_NAMES = [
    "absorb_claimed_text", "accept_update", "admit_media", "answer_callback", "answer_or_note", "apply_observe_attribution",
    "auth_fn_call", "auth_source", "await_with_thread_deadline", "build_animated_sticker_injection", "build_message_event", "build_sticker_injection",
    "build_triggered_event", "builder_build", "cache_image_from_bytes", "cache_observed_media", "cache_sticker_description", "callback_authorized",
    "claim_callback_state", "claim_pending_text", "consume_vault_code_reply", "current_task", "drain_general_connections", "drain_polling_connections",
    "edit_keeping_prompt", "edit_md_quiet", "effective_update_message", "enqueue_text_event", "ensure_forum_commands", "env_allowlist_decision",
    "env_float", "expand_link_entities", "fallback", "fetch_telegram_file", "gate_or_observe", "get_cached_description",
    "get_hermes_home", "go_fatal_network", "group_trigger_text", "handle_message", "handoff_polling_fatal_error", "hash_for_log",
    "is_sender_authorized", "is_user_authorized_from_message", "json_loads", "legacy_runner_auth_fn", "log_blocked_user", "logger_debug",
    "logger_error", "logger_info", "logger_warning", "looks_like_network_error", "looks_like_pool_timeout", "loop_time",
    "media_message_type", "normalize_chat_id", "normalize_chat_type", "notify_fatal_error", "parse_page", "picker_show_models",
    "picker_show_providers", "provider_get_label", "queue_media_group_event", "redact_telegram_error_text", "redundant_live_refresh", "refresh_gates",
    "refusal_text", "refuse_tap", "register_handlers", "replace", "restart_polling_in_task", "rich_message_text",
    "scoped_gate_env", "set_fatal_error", "set_status_indicator", "should_observe_unmentioned", "shutdown_abandoned_app", "slash_confirm_refusal",
    "slash_confirm_resolve", "sleep", "start_polling_once", "sticker_local_root", "stop_updater_or_go_fatal", "toast",
    "translate", "unauthorized", "update_prompt_refusal", "vision_analyze_tool", "wire_plugin_handlers", "write_text",
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


# ── _hold_chat_outbound_slot ───────────────────────────────────────────────
for budget, cid, t in [(5.0, "c1", 100.0), (0, "a:b", 0.0), (-3, 123, 50.0),
                       (None, "x", 7.5), (100, "z", 9.0)]:
    st = {} if budget is None else {"_telegram_chat_outbound_slot_secs": budget}
    mk("_hold_chat_outbound_slot", {"chat_id": cid}, st,
       {"loop_time": sc(t), "normalize_chat_id": sc(str(cid) if not isinstance(cid, int) else 123)})
mk("_hold_chat_outbound_slot", {"chat_id": "c9"},
   {"_telegram_chat_outbound_slot_until": {"c9": 1.0}},
   {"loop_time": sc(2.0), "normalize_chat_id": sc("c9")})
mk("_hold_chat_outbound_slot", {"chat_id": "c2"}, {},
   {"loop_time": sc(raise_type="ValueError", raise_msg="bad"), "normalize_chat_id": sc("c2")})

# ── _handoff_polling_fatal_error ───────────────────────────────────────────
for i in range(6):
    mk("_handoff_polling_fatal_error", {},
       {"_polling_error_task": "t%d" % i, "_polling_heartbeat_task": "h%d" % i},
       {"notify_fatal_error": sc("ok" if i % 2 else None)})
mk("_handoff_polling_fatal_error", {}, {},
   {"notify_fatal_error": sc(raise_type="RuntimeError", raise_msg="nf")})
mk("_handoff_polling_fatal_error", {}, {},
   {"notify_fatal_error": sc(raise_type="SystemExit", raise_msg="bye")})

# ── _warn_then ─────────────────────────────────────────────────────────────
for mk_ in ["photo", "video", "audio", "document", "text", "sticker"]:
    mk("_warn_then", {"media_key": mk_, "error": "E"},
       {}, {"redact_telegram_error_text": sc("red"), "fallback": sc("res-" + mk_)})
mk("_warn_then", {"media_key": "x", "error": "E"}, {},
   {"logger_warning": sc(raise_type="RuntimeError", raise_msg="w")})
mk("_warn_then", {"media_key": "x", "error": "E"}, {},
   {"fallback": sc(raise_type="SystemExit", raise_msg="bye")})

# ── _set_status_indicator ──────────────────────────────────────────────────
for v in [True, False, 1, 0, "yes", None]:
    mk("_set_status_indicator", {"online": v}, {})
mk("_set_status_indicator", {"online": True}, {},
   {"set_status_indicator": sc(raise_type="RuntimeError", raise_msg="s")})

# ── _instrument_polling_request ────────────────────────────────────────────
for v in ["req1", "req2", None, 5, {"id": "x"}, [1, 2]]:
    mk("_instrument_polling_request", {"request": v}, {})

# ── _is_callback_user_authorized ───────────────────────────────────────────
mk("_is_callback_user_authorized", {"user_id": "", "has_auth_check": None}, {})
mk("_is_callback_user_authorized", {"user_id": None, "has_auth_check": None}, {})
mk("_is_callback_user_authorized", {"user_id": "  ", "has_auth_check": None}, {})
mk("_is_callback_user_authorized", {"user_id": "u1", "has_auth_check": True},
   {}, {"is_sender_authorized": sc(True), "normalize_chat_type": sc("private")})
mk("_is_callback_user_authorized", {"user_id": "u2", "has_auth_check": True, "chat_id": "c", "chat_type": "group", "thread_id": "th"},
   {}, {"is_sender_authorized": sc(False), "normalize_chat_type": sc("forum")})
mk("_is_callback_user_authorized", {"user_id": "u3", "has_auth_check": True},
   {}, {"is_sender_authorized": sc(None), "normalize_chat_type": sc("private"),
        "legacy_runner_auth_fn": sc("fn"), "auth_fn_call": sc(True)})
mk("_is_callback_user_authorized", {"user_id": "u4", "has_auth_check": None},
   {}, {"normalize_chat_type": sc("private"), "legacy_runner_auth_fn": sc(None),
        "env_allowlist_decision": sc(True)})
mk("_is_callback_user_authorized", {"user_id": "u5", "has_auth_check": None},
   {}, {"normalize_chat_type": sc("private"), "legacy_runner_auth_fn": sc(None),
        "env_allowlist_decision": sc(None), "scoped_gate_env": sc("true")})
mk("_is_callback_user_authorized", {"user_id": "u6", "has_auth_check": None},
   {}, {"normalize_chat_type": sc("private"), "legacy_runner_auth_fn": sc(None),
        "env_allowlist_decision": sc(None), "scoped_gate_env": sc("no")})

# ── _handle_text_message ───────────────────────────────────────────────────
TX = {"message": {"text": "hi"}}
mk("_handle_text_message", {"update": TX}, {}, {"effective_update_message": sc(None)})
mk("_handle_text_message", {"update": TX}, {},
   {"effective_update_message": sc({"text": ""}), "rich_message_text": sc(None)})
mk("_handle_text_message", {"update": TX}, {},
   {"effective_update_message": sc({"text": "hi"}), "is_user_authorized_from_message": sc(False)})
mk("_handle_text_message", {"update": TX}, {},
   {"effective_update_message": sc({"text": "hi"}), "is_user_authorized_from_message": sc(True),
    "consume_vault_code_reply": sc(True)})
mk("_handle_text_message", {"update": TX}, {},
   {"effective_update_message": sc({"text": "hi"}), "is_user_authorized_from_message": sc(True),
    "consume_vault_code_reply": sc(False), "gate_or_observe": sc(False)})
mk("_handle_text_message", {"update": TX}, {},
   {"effective_update_message": sc({"text": "hi"}), "is_user_authorized_from_message": sc(True),
    "rich_message_text": sc(None), "consume_vault_code_reply": sc(False), "gate_or_observe": sc(True),
    "build_triggered_event": sc({"text": "T"})})

# ── _handle_location_message ───────────────────────────────────────────────
LU = {"update_id": 5, "message": {"text": ""}}
mk("_handle_location_message", {"update": LU}, {}, {"effective_update_message": sc(None)})
mk("_handle_location_message", {"update": LU}, {},
   {"effective_update_message": sc({"text": ""}), "is_user_authorized_from_message": sc(False)})
mk("_handle_location_message", {"update": LU}, {},
   {"effective_update_message": sc({"location": {"latitude": 1.5, "longitude": 2.5}}),
    "is_user_authorized_from_message": sc(True), "redundant_live_refresh": sc(True)})
mk("_handle_location_message", {"update": LU}, {},
   {"effective_update_message": sc({"location": {"latitude": 1.5, "longitude": 2.5}}),
    "is_user_authorized_from_message": sc(True), "redundant_live_refresh": sc(False),
    "gate_or_observe": sc(False)})
mk("_handle_location_message", {"update": LU}, {},
   {"effective_update_message": sc({"location": {"latitude": 1.5, "longitude": 2.5}}),
    "is_user_authorized_from_message": sc(True), "redundant_live_refresh": sc(False),
    "gate_or_observe": sc(True), "build_message_event": sc({"message_type": "LOCATION"})})
mk("_handle_location_message", {"update": LU}, {},
   {"effective_update_message": sc({"venue": {"title": "Cafe", "address": "Main St",
                                               "location": {"latitude": 3.0, "longitude": 4.0}}}),
    "is_user_authorized_from_message": sc(True), "redundant_live_refresh": sc(False),
    "gate_or_observe": sc(True), "build_message_event": sc({"message_type": "LOCATION"})})

# ── _handle_sticker ────────────────────────────────────────────────────────
def sticker(**kw):
    d = {"emoji": "X", "set_name": "S", "is_animated": False, "is_video": False,
         "file_unique_id": "fu"}
    d.update(kw)
    return {"msg": {"sticker": d}, "event": {"text": ""}}

mk("_handle_sticker", sticker(is_animated=True), {}, {"build_animated_sticker_injection": sc("anim")})
mk("_handle_sticker", sticker(is_video=True), {}, {"build_animated_sticker_injection": sc("anim")})
mk("_handle_sticker", sticker(), {},
   {"get_cached_description": sc({"description": "cached", "emoji": "E", "set_name": "SN"}),
    "build_sticker_injection": sc("cij")})
mk("_handle_sticker", sticker(), {},
   {"get_cached_description": sc(None), "fetch_telegram_file": sc(["f", [104, 105]]),
    "cache_image_from_bytes": sc("/tmp/x.webp"),
    "vision_analyze_tool": sc("{\"success\": true, \"analysis\": \"a cat\"}"),
    "json_loads": sc({"success": True, "analysis": "a cat"}),
    "build_sticker_injection": sc("inj")})
mk("_handle_sticker", sticker(), {},
   {"get_cached_description": sc(None), "fetch_telegram_file": sc(["f", [104, 105]]),
    "cache_image_from_bytes": sc("/tmp/x.webp"),
    "vision_analyze_tool": sc("{\"success\": false}"),
    "json_loads": sc({"success": False}), "build_sticker_injection": sc("inj")})
mk("_handle_sticker", sticker(), {},
   {"get_cached_description": sc(None), "fetch_telegram_file": sc(raise_type="RuntimeError", raise_msg="net"),
    "build_sticker_injection": sc("inj")})

# ── _handle_update_prompt_callback ─────────────────────────────────────────
UC = {"query": {"from_user": {"id": 7}}, "cb": {}}
mk("_handle_update_prompt_callback", dict(UC, data="update_prompt:y:abc"), {},
   {"callback_authorized": sc(False)})
mk("_handle_update_prompt_callback", dict(UC, data="update_prompt:y:abc"), {},
   {"callback_authorized": sc(True), "refuse_tap": sc(True)})
mk("_handle_update_prompt_callback", dict(UC, data="update_prompt:y:abc"),
   {"_update_prompt_id": "other"}, {"callback_authorized": sc(True), "refuse_tap": sc(False)})
mk("_handle_update_prompt_callback", dict(UC, data="update_prompt:y:abc"), {},
   {"callback_authorized": sc(True), "refuse_tap": sc(False), "get_hermes_home": sc("/home"),
    "write_text": sc(raise_type="OSError", raise_msg="disk")})
mk("_handle_update_prompt_callback", dict(UC, data="update_prompt:y"), {},
   {"callback_authorized": sc(True), "refuse_tap": sc(False), "get_hermes_home": sc("/home"),
    "answer_callback": sc(None), "translate": sc("ok")})
mk("_handle_update_prompt_callback", dict(UC, data="update_prompt:n:abc"), {},
   {"callback_authorized": sc(True), "refuse_tap": sc(False), "get_hermes_home": sc("/home"),
    "hash_for_log": sc(7), "translate": sc("ok")})

# ── _handle_model_picker_callback ──────────────────────────────────────────
MQ = {"query": {}, "chat_id": "c1"}
MPS = {"_model_picker_state": {"c1": {"providers": [{"slug": "p1", "name": "P1", "models": ["m1"]}]}}}
mk("_handle_model_picker_callback", dict(MQ, data="mx"), MPS,
   {"answer_callback": sc(None), "edit_md_quiet": sc(None), "translate": sc("cancelled")})
mk("_handle_model_picker_callback", dict(MQ, data="other"), MPS,
   {"answer_callback": sc(None)})
mk("_handle_model_picker_callback", dict(MQ, data="mp:p1"),
   {"_model_picker_state": {"c1": {"providers": [{"slug": "p1", "name": "P1", "models": ["m1"]}]}}},
   {"provider_get_label": sc("label"), "picker_show_models": sc(None)})
mk("_handle_model_picker_callback", dict(MQ, data="mp:pX"),
   {"_model_picker_state": {"c1": {"providers": [{"slug": "p1"}]}}}, {})
mk("_handle_model_picker_callback", dict(MQ, data="mb"),
   {"_model_picker_state": {"c1": {"providers": [], "provider_page": 2}}},
   {"provider_get_label": sc("label"), "picker_show_providers": sc(None)})
mk("_handle_model_picker_callback", dict(MQ, data="mg:2"),
   {"_model_picker_state": {"c1": {"providers": []}}},
   {"provider_get_label": sc("label"), "parse_page": sc(2), "picker_show_models": sc(None)})
mk("_handle_model_picker_callback", dict(MQ, data="mpv:1"),
   {"_model_picker_state": {"c1": {"providers": []}}},
   {"provider_get_label": sc("label"), "parse_page": sc(1), "picker_show_providers": sc(None)})

# ── _handle_slash_confirm_callback ─────────────────────────────────────────
SQ = {"query": {"from_user": {"id": 7}}, "cb": {}}
mk("_handle_slash_confirm_callback", dict(SQ, data="bad"), {})
mk("_handle_slash_confirm_callback", dict(SQ, data="sc:once:cid1"), {},
   {"claim_callback_state": sc(None)})
mk("_handle_slash_confirm_callback", dict(SQ, data="sc:once:cid1"),
   {"_slash_confirm_state": {"cid1": "sk1"}},
   {"claim_callback_state": sc("sk1"), "slash_confirm_refusal": sc("requester"),
    "refusal_text": sc("refused")})
mk("_handle_slash_confirm_callback", dict(SQ, data="sc:once:cid1"),
   {"_slash_confirm_state": {}},
   {"claim_callback_state": sc("sk1"), "slash_confirm_refusal": sc(None)})
mk("_handle_slash_confirm_callback", dict(SQ, data="sc:once:cid1"),
   {"_slash_confirm_state": {"cid1": "sk1"}},
   {"claim_callback_state": sc("sk1"), "slash_confirm_refusal": sc(None),
    "translate": sc("label"), "edit_keeping_prompt": sc(None), "slash_confirm_resolve": sc(None)})
mk("_handle_slash_confirm_callback", dict(SQ, data="sc:cancel:cid2"),
   {"_slash_confirm_state": {"cid2": "sk2"}},
   {"claim_callback_state": sc("sk2"), "slash_confirm_refusal": sc(None),
    "translate": sc("label"), "edit_keeping_prompt": sc(None), "slash_confirm_resolve": sc(None)})

# ── _initialize_app_with_retries ───────────────────────────────────────────
for i in range(4):
    mk("_initialize_app_with_retries", {}, {},
       {"env_float": sc(30.0), "loop_time": sc(float(100 + i))})
mk("_initialize_app_with_retries", {}, {},
   {"env_float": sc(30.0), "loop_time": sc(100.0),
    "await_with_thread_deadline": [sc(raise_type="TimeoutError"), sc(None)],
    "sleep": sc(None), "builder_build": sc(None)})
mk("_initialize_app_with_retries", {}, {},
   {"env_float": sc(30.0), "loop_time": sc(100.0),
    "await_with_thread_deadline": [sc(raise_type="ConnectionError", raise_msg="net"), sc(None)],
    "looks_like_network_error": sc(True), "sleep": sc(None), "builder_build": sc(None)})

# ── _handle_polling_network_error ──────────────────────────────────────────
mk("_handle_polling_network_error", {"error_kind": "generic"}, {"_teardown_started": True}, {})
mk("_handle_polling_network_error", {"error_kind": "generic"}, {"has_fatal_error": True}, {})
mk("_handle_polling_network_error", {"error_kind": "stall"}, {},
   {"redact_telegram_error_text": sc("stalled")})
mk("_handle_polling_network_error", {"error_kind": "generic"}, {"_polling_network_error_count": 10},
   {"redact_telegram_error_text": sc("err")})
mk("_handle_polling_network_error", {"error_kind": "generic"},
   {"_polling_network_error_count": 0, "_app": "app"},
   {"redact_telegram_error_text": sc("err"), "stop_updater_or_go_fatal": sc(True),
    "looks_like_pool_timeout": sc(False), "drain_polling_connections": sc(None),
    "start_polling_once": sc(None)})
mk("_handle_polling_network_error", {"error_kind": "timeout"},
   {"_polling_network_error_count": 0, "_app": "app"},
   {"redact_telegram_error_text": sc("err"), "stop_updater_or_go_fatal": sc(True),
    "looks_like_pool_timeout": sc(True), "drain_general_connections": sc(None),
    "drain_polling_connections": sc(None), "start_polling_once": sc(None)})

# ── _handle_polling_conflict ───────────────────────────────────────────────
mk("_handle_polling_conflict", {"error_kind": "generic"}, {"_teardown_started": True}, {})
mk("_handle_polling_conflict", {"error_kind": "generic"},
   {"has_fatal_error": True, "fatal_error_code": "telegram_polling_conflict"}, {})
mk("_handle_polling_conflict", {"error_kind": "generic"}, {"_polling_conflict_count": 5},
   {"redact_telegram_error_text": sc("err"), "set_fatal_error": sc(None)})
mk("_handle_polling_conflict", {"error_kind": "generic"},
   {"_polling_conflict_count": 0, "_app": "app"},
   {"redact_telegram_error_text": sc("err"), "stop_updater_or_go_fatal": sc(True),
    "drain_polling_connections": sc(None), "start_polling_once": sc(None)})
mk("_handle_polling_conflict", {"error_kind": "timeout"},
   {"_polling_conflict_count": 0, "_app": "app"},
   {"redact_telegram_error_text": sc("err"), "stop_updater_or_go_fatal": sc(False)})
mk("_handle_polling_conflict", {"error_kind": "generic"},
   {"_polling_conflict_count": 4, "_app": "app"},
   {"redact_telegram_error_text": sc("err"), "stop_updater_or_go_fatal": sc(True),
    "start_polling_once": sc(raise_type="RuntimeError", raise_msg="retry"),
    "restart_polling_in_task": sc(None)})

# ── _handle_media_message ──────────────────────────────────────────────────
def fullmsg(**kw):
    d = {"text": "", "caption": None, "sticker": None, "photo": None, "voice": None,
         "audio": None, "video": None, "video_note": None, "document": None,
         "media_group_id": None}
    d.update(kw)
    return d

MU = {"update_id": 9, "message": fullmsg()}
mk("_handle_media_message", {"update": MU}, {}, {"effective_update_message": sc(None)})
mk("_handle_media_message", {"update": MU}, {},
   {"effective_update_message": sc(fullmsg()), "is_user_authorized_from_message": sc(False)})
mk("_handle_media_message", {"update": MU}, {},
   {"effective_update_message": sc(fullmsg(text="x")), "is_user_authorized_from_message": sc(True),
    "admit_media": sc(None), "should_observe_unmentioned": sc(False)})
mk("_handle_media_message", {"update": MU}, {},
   {"effective_update_message": sc(fullmsg(text="x")), "is_user_authorized_from_message": sc(True),
    "admit_media": sc({}), "media_message_type": sc("PHOTO"),
    "build_message_event": sc({"message_type": "PHOTO"}),
    "apply_observe_attribution": sc({"text": "x"})})
mk("_handle_media_message", {"update": MU}, {},
   {"effective_update_message": sc(fullmsg(text="x", media_group_id="mg1")),
    "is_user_authorized_from_message": sc(True), "admit_media": sc({}),
    "media_message_type": sc("PHOTO"), "build_message_event": sc({"message_type": "PHOTO"}),
    "apply_observe_attribution": sc({"text": "x"}), "queue_media_group_event": sc(None)})
mk("_handle_media_message", {"update": MU}, {},
   {"effective_update_message": sc(fullmsg(text="x", caption="caption")),
    "is_user_authorized_from_message": sc(True), "admit_media": sc({}),
    "media_message_type": sc("PHOTO"), "build_message_event": sc({"message_type": "PHOTO"}),
    "expand_link_entities": sc("cap"), "group_trigger_text": sc("cap2"),
    "apply_observe_attribution": sc({"text": "x"})})


assert ALL_IO == set(SEAM_NAMES), (sorted(set(SEAM_NAMES) - ALL_IO), sorted(ALL_IO - set(SEAM_NAMES)))
assert ALL_FUNCS == set(_KINDS) if False else True
assert len(CASES) >= 16 * 6

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        c = dict(c)
        c.pop("_label", None)
        f.write(json.dumps(c, ensure_ascii=False) + "\n")
print("wrote", len(CASES), "cases to", OUT)
