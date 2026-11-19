#!/usr/bin/env python3
"""Generate tests/fixtures/relay_adapter_flow2.jsonl — differential cases for
the gateway/relay/adapter.py UNCLAIMED-remainder FLOW port (relay_adapter_flow2).

Each case is {"func": <live_name>, "in": {...}} where in.io carries the
universal-seam scripts and in.state the mutable carrier. >=5 cases per func
exercise the happy path, branch edges, fail-closed (absent script), scripted
raises, and BaseException passthrough through `except Exception` guards.

Domain (ASCII scope): str()/strip() operands ASCII; metadata/fields/state maps
dict-or-falsy; supported_ops list-or-falsy; SendResult doubles tagged
{"__send_result__":true,...}; _MediaNotDelivered {"__not_delivered__":true,reason};
callable doubles {"__callable__":seam}. Integral-FLOAT str() positions excluded.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "relay_adapter_flow2.jsonl"
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def sr(**k):
    return {"__send_result__": True, **k}


def nd(reason):
    return {"__not_delivered__": True, "reason": reason}


def rz(kind, msg):
    return {"__raise__": kind, "message": msg}


T = {"__transport__": True}          # present transport marker
DESC = {"platform": "telegram"}      # primary descriptor double


# ── _chat_platform ──
case("_chat_platform", chat_id="c1", state={"platform_by_chat": {"c1": "slack"}, "descriptor": DESC})
case("_chat_platform", chat_id="zz", state={"platform_by_chat": {}, "descriptor": DESC})
case("_chat_platform", chat_id="c1", state={"platform_by_chat": {"c1": ""}, "descriptor": DESC})
case("_chat_platform", chat_id=42, state={"platform_by_chat": {"42": "discord"}, "descriptor": DESC})
case("_chat_platform", chat_id="c1", state={"platform_by_chat": {}, "descriptor": {"platform": None}})
case("_chat_platform", chat_id="c1", state={"platform_by_chat": {"c1": None}, "descriptor": DESC})

# ── warning_notifications_enabled ──
case("warning_notifications_enabled", logical_platform="slack",
     io={"super.warning_notifications_enabled": True}, state={"descriptor": DESC})
case("warning_notifications_enabled", metadata={"_relay_logical_platform": "discord"},
     io={"super.warning_notifications_enabled": False}, state={"descriptor": DESC})
case("warning_notifications_enabled", chat_id="c1",
     io={"super.warning_notifications_enabled": True},
     state={"platform_by_chat": {"c1": "slack"}, "descriptor": DESC})
case("warning_notifications_enabled", chat_id="zz",
     io={"super.warning_notifications_enabled": False}, state={"descriptor": DESC})
case("warning_notifications_enabled", logical_platform="slack",
     io={"super.warning_notifications_enabled": rz("RuntimeError", "boom")}, state={"descriptor": DESC})
case("warning_notifications_enabled", logical_platform="slack", io={}, state={"descriptor": DESC})
case("warning_notifications_enabled", logical_platform="slack",
     io={"super.warning_notifications_enabled": rz("KeyboardInterrupt", "ki")}, state={"descriptor": DESC})
case("warning_notifications_enabled", chat_id=None, metadata={},
     io={"super.warning_notifications_enabled": True}, state={"descriptor": DESC})

# ── _text_metadata ──
case("_text_metadata", chat_id="c1", metadata={"k": "v"},
     io={"_with_format_hints_for_chat": {"k": "v", "format_hints": {"rich_blocks": True}},
         "_with_scope": {"k": "v", "format_hints": {"rich_blocks": True}, "scope_id": "s1"}})
case("_text_metadata", chat_id="c1", metadata={"k": "v"}, io={"_with_scope": {}})
case("_text_metadata", chat_id="c1", metadata={},
     io={"_with_format_hints_for_chat": rz("ValueError", "bad"), "_with_scope": {}})
case("_text_metadata", chat_id="c1", metadata={},
     io={"_with_format_hints_for_chat": {}, "_with_scope": rz("RuntimeError", "scope")})
case("_text_metadata", chat_id="c1", metadata=None,
     io={"_with_format_hints_for_chat": {}, "_with_scope": {"scope_id": "s"}})
case("_text_metadata", chat_id="c1", metadata={},
     io={"_with_format_hints_for_chat": rz("GeneratorExit", "ge"), "_with_scope": {}})

# ── _draft_frame ──
case("_draft_frame", chat_id="c1", draft_id=7, content="hi", final=False, metadata={"message_id": "m1"},
     io={"_with_format_hints_for_chat": {"message_id": "m1"}, "_with_scope": {"message_id": "m1", "scope_id": "s"}})
case("_draft_frame", chat_id="c1", draft_id=1, content="x", final=False, metadata=None,
     io={"_with_format_hints_for_chat": {}, "_with_scope": {}})
case("_draft_frame", chat_id="c1", draft_id=2, content="x", final=True, metadata={},
     io={"_with_format_hints_for_chat": {}, "_with_scope": {}})
case("_draft_frame", chat_id="c1", draft_id=3, content="x", final=False, metadata={},
     io={"_with_scope": {}})
case("_draft_frame", chat_id="c1", draft_id=4, content="x", final=False, metadata={"a": 1, "b": 2},
     io={"_with_format_hints_for_chat": {"a": 1, "b": 2}, "_with_scope": {"a": 1, "b": 2, "user_id": "u"}})

# ── send_draft ──
_SD_IO = {"supports_draft_streaming": True, "_draft_key": "c1:turn:m1",
          "stream_is_message_for_chat": True, "_evict_oldest": None,
          "_with_format_hints_for_chat": {"message_id": "m1"},
          "_with_scope": {"message_id": "m1", "scope_id": "s"},
          "_outbound": {"success": True, "message_id": "99"}}
case("send_draft", chat_id="c1", draft_id=7, content="hi", metadata={"message_id": "m1"},
     state={"transport": T}, io=dict(_SD_IO))
case("send_draft", chat_id="c1", draft_id=7, content="hi",
     io={"supports_draft_streaming": False})
case("send_draft", chat_id="c1", draft_id=7, content="hi",
     io={"supports_draft_streaming": True}, state={"transport": None})
case("send_draft", chat_id="c1", draft_id=7, content="hi", metadata={},
     state={"transport": T, "sealed_draft_by_chat": {"c1:": 7}},
     io={"supports_draft_streaming": True, "_draft_key": "c1:", "_evict_oldest": None, "logger.warning": None})
case("send_draft", chat_id="c1", draft_id=7, content="hi", metadata={},
     state={"transport": T, "sealed_draft_by_chat": {"c1:": 7}, "tombstone_swallow_logged": {"c1:": 7}},
     io={"supports_draft_streaming": True, "_draft_key": "c1:"})
case("send_draft", chat_id="c1", draft_id=7, content="hi", metadata={},
     state={"transport": T},
     io={"supports_draft_streaming": True, "_draft_key": "c1:", "stream_is_message_for_chat": False,
         "_with_format_hints_for_chat": {}, "_with_scope": {},
         "_outbound": rz("RuntimeError", "boom")})
case("send_draft", chat_id="c1", draft_id=7, content="hi", metadata={},
     state={"transport": T},
     io={"supports_draft_streaming": True, "_draft_key": "c1:", "stream_is_message_for_chat": False,
         "_with_format_hints_for_chat": {}, "_with_scope": {},
         "_outbound": {"success": False, "ambiguous": True, "error": "ack lost"}})
case("send_draft", chat_id="c1", draft_id=7, content="hi", metadata={},
     state={"transport": T, "open_draft_by_chat": {"c1:": 7}},
     io={"supports_draft_streaming": True, "_draft_key": "c1:", "stream_is_message_for_chat": False,
         "_with_format_hints_for_chat": {}, "_with_scope": {},
         "_outbound": {"success": False, "code": "egress_declined", "error": "egress declined: nope"},
         "logger.warning": None})
case("send_draft", chat_id="c1", draft_id=7, content="hi", metadata={},
     state={"transport": T},
     io={"supports_draft_streaming": True, "_draft_key": "c1:", "stream_is_message_for_chat": False,
         "_with_format_hints_for_chat": {}, "_with_scope": {},
         "_outbound": {"success": False, "error": "plain failure"}})
case("send_draft", chat_id="c1", draft_id=7, content="hi", io={})
case("send_draft", chat_id="c1", draft_id=7, content="hi", metadata={},
     state={"transport": T},
     io={"supports_draft_streaming": True, "_draft_key": "c1:", "stream_is_message_for_chat": False,
         "_with_format_hints_for_chat": {}, "_with_scope": {},
         "_outbound": rz("KeyboardInterrupt", "ki")})
case("send_draft", chat_id="c1", draft_id=7, content="hi", metadata={},
     state={"transport": T},
     io={"supports_draft_streaming": True, "_draft_key": "c1:", "stream_is_message_for_chat": False,
         "_with_format_hints_for_chat": {}, "_with_scope": {}, "_outbound": None})
case("send_draft", chat_id="c1", draft_id=7, content="hi", metadata={},
     state={"transport": T},
     io={"supports_draft_streaming": True, "_draft_key": "c1:", "stream_is_message_for_chat": False,
         "_with_format_hints_for_chat": {}, "_with_scope": {},
         "_outbound": {"success": False, "ambiguous": True}})

# ── _absorb_into_open_draft ──
case("_absorb_into_open_draft", chat_id="c1", content="f", metadata={}, interim=True, io={})
case("_absorb_into_open_draft", chat_id="c1", content="f", metadata={}, interim=False,
     io={"_match_open_draft": None})
case("_absorb_into_open_draft", chat_id="c1", content="f", metadata={}, interim=False,
     io={"_match_open_draft": "c1:", "_seal_open_draft": sr(success=True, message_id="9")})
case("_absorb_into_open_draft", chat_id="c1", content="f", metadata={}, interim=False,
     io={"_match_open_draft": "c1:",
         "_seal_open_draft": sr(success=False, error="x", raw_response={"success": False, "code": "egress_declined"}),
         "logger.warning": None})
case("_absorb_into_open_draft", chat_id="c1", content="f", metadata={}, interim=False,
     io={"_match_open_draft": "c1:", "_seal_open_draft": sr(success=False, error="lane down"),
         "logger.warning": None})
case("_absorb_into_open_draft", chat_id="c1", content="f", metadata={}, interim=False, io={})
case("_absorb_into_open_draft", chat_id="c1", content="f", metadata={}, interim=False,
     io={"_match_open_draft": "c1:", "_seal_open_draft": rz("CancelledError", "cancel")})
case("_absorb_into_open_draft", chat_id="c1", content="f", metadata={}, interim=False,
     io={"_match_open_draft": "c1:", "_seal_open_draft": sr(success=False, error=None),
         "logger.warning": None})

# ── _card_frame ──
case("_card_frame", chat_id="c1", op="task_card", reply_to="r1", metadata={"a": 1}, fields={"progress": 50},
     state={"transport": T},
     io={"supports_native_task_cards": True, "_card_key": "turn:r1", "_with_scope": {"a": 1, "scope_id": "s"},
         "_outbound": {"success": True}})
case("_card_frame", chat_id="c1", op="task_card", reply_to=None, metadata={},
     io={"supports_native_task_cards": False})
case("_card_frame", chat_id="c1", op="task_card", reply_to=None, metadata={},
     io={"supports_native_task_cards": True}, state={"transport": None})
case("_card_frame", chat_id="c1", op="task_card_stop", reply_to="r1", metadata={},
     state={"transport": T},
     io={"supports_native_task_cards": True, "_card_key": "turn:r1", "_with_scope": {},
         "_outbound": rz("RuntimeError", "down")})
case("_card_frame", chat_id="c1", op="task_card", reply_to="r1", metadata={}, fields={"x": 1, "y": 2},
     state={"transport": T},
     io={"supports_native_task_cards": True, "_card_key": "turn:r1", "_with_scope": {},
         "_outbound": {"success": True}})
case("_card_frame", chat_id="c1", op="task_card", reply_to="r1", metadata={},
     state={"transport": T}, io={"supports_native_task_cards": True, "_with_scope": {}})
case("_card_frame", chat_id="c1", op="task_card", reply_to="r1", metadata={},
     state={"transport": T},
     io={"supports_native_task_cards": True, "_card_key": "turn:r1", "_with_scope": {},
         "_outbound": rz("SystemExit", "exit")})
case("_card_frame", chat_id="c1", op="task_card", reply_to="r1", metadata={"m": 1},
     fields={"card_id": "ck", "x": 1},
     state={"transport": T},
     io={"supports_native_task_cards": True, "_card_key": "turn:r1", "_with_scope": {"m": 1, "scope_id": "s"},
         "_outbound": {"success": True}})

# ── _task_card_metadata ──
case("_task_card_metadata", reply_to="123", metadata={"a": 1})
case("_task_card_metadata", reply_to="123", metadata={"thread_ts": "999"})
case("_task_card_metadata", reply_to=None, metadata={"a": 1})
case("_task_card_metadata", reply_to="123", metadata=None)
case("_task_card_metadata", reply_to="", metadata={})
case("_task_card_metadata", reply_to=55, metadata={})

# ── native_task_card_destination_supported ──
_SL = {"platform_by_chat": {"c1": "slack"}, "descriptor": {"platform": "slack"}}
case("native_task_card_destination_supported", chat_id="c1", reply_to="r1", metadata={},
     state={"platform_by_chat": {"c1": "discord"}, "descriptor": DESC})
case("native_task_card_destination_supported", chat_id="c1", reply_to="r1", metadata={}, state=_SL)
case("native_task_card_destination_supported", chat_id="c1", reply_to=None, metadata={}, state=_SL)
case("native_task_card_destination_supported", chat_id="c1", reply_to=None, metadata={"thread_id": "t1"}, state=_SL)
case("native_task_card_destination_supported", chat_id="c1", reply_to=None,
     metadata={"thread_id": None, "thread_ts": "ts"}, state=_SL)
case("native_task_card_destination_supported", chat_id="c1", reply_to=None, metadata={"thread_id": 5}, state=_SL)

# ── _relay_platform_extra ──
case("_relay_platform_extra", platform="slack", state={"config": {"extra": {"slack": {"x": 1}, "y": 2}}})
case("_relay_platform_extra", platform="slack", state={"config": {"extra": {"slack": "notdict", "y": 2}}})
case("_relay_platform_extra", platform="slack", state={"config": {"extra": {}}})
case("_relay_platform_extra", platform="slack", state={"config": {"extra": None}})
case("_relay_platform_extra", platform="slack", state={"config": None})
case("_relay_platform_extra", platform="discord", state={"config": {"extra": {"slack": {"x": 1}}}})

# ── _slack_flag ──
case("_slack_flag", knob="reply_in_thread", default=True,
     io={"_relay_slack_extra": {"reply_in_thread": "false"}, "_coerce_flag": False})
case("_slack_flag", knob="x", default=True, io={"_relay_slack_extra": None})
case("_slack_flag", knob="x", default=False, io={"_relay_slack_extra": [1, 2]})
case("_slack_flag", knob="x", default=True,
     io={"_relay_slack_extra": {"x": "yes"}, "_coerce_flag": rz("ValueError", "bad")})
case("_slack_flag", knob="x", default=True,
     io={"_relay_slack_extra": {"x": "yes"}, "_coerce_flag": rz("KeyboardInterrupt", "ki")})
case("_slack_flag", knob="x", default=True, io={})
case("_slack_flag", knob="missing", default=False,
     io={"_relay_slack_extra": {"other": 1}, "_coerce_flag": False})

# ── _stamp_slack_unfurl ──
case("_stamp_slack_unfurl", platform="slack", metadata={"a": 1},
     io={"_slack_unfurl_hints": {"unfurl_links": True}})
case("_stamp_slack_unfurl", platform="slack", metadata={"a": 1}, io={"_slack_unfurl_hints": None})
case("_stamp_slack_unfurl", platform="slack", metadata={"a": 1}, io={"_slack_unfurl_hints": {}})
case("_stamp_slack_unfurl", platform="slack", metadata=None,
     io={"_slack_unfurl_hints": {"unfurl_links": True}})
case("_stamp_slack_unfurl", platform="slack", metadata={"a": 1}, io={})
case("_stamp_slack_unfurl", platform="slack", metadata={"a": 1},
     io={"_slack_unfurl_hints": rz("GeneratorExit", "ge")})
case("_stamp_slack_unfurl", platform="slack", metadata={"unfurl_links": False, "b": 2},
     io={"_slack_unfurl_hints": {"unfurl_links": True}})

# ── hold_redial / release_redial ──
for fn in ("hold_redial", "release_redial"):
    case(fn, io={"_toggle_transport_redial": True})
    case(fn, io={"_toggle_transport_redial": False})
    case(fn, io={"_toggle_transport_redial": rz("RuntimeError", "boom")})
    case(fn, io={})
    case(fn, io={"_toggle_transport_redial": rz("SystemExit", "exit")})

# ── _prepare_slack_egress ──
case("_prepare_slack_egress", chat_id="c1", reply_to="r1", metadata={"a": 1}, state=_SL,
     io={"_apply_slack_thread_anchor": "r1", "_slack_unfurl_hints": {"unfurl_media": False}})
case("_prepare_slack_egress", chat_id="c1", reply_to="r1", metadata={}, state=_SL,
     io={"_apply_slack_thread_anchor": None, "_slack_unfurl_hints": None})
case("_prepare_slack_egress", chat_id="c1", reply_to="r1", metadata={}, state=_SL,
     io={"_apply_slack_thread_anchor": rz("RuntimeError", "anchor"), "_slack_unfurl_hints": {}})
case("_prepare_slack_egress", chat_id="c1", reply_to="r1", metadata={}, state=_SL,
     io={"_apply_slack_thread_anchor": "r1"})
case("_prepare_slack_egress", chat_id="c1", reply_to=None, metadata={"a": 1},
     state={"platform_by_chat": {"c1": "discord"}, "descriptor": DESC},
     io={"_apply_slack_thread_anchor": None, "_slack_unfurl_hints": None})
case("_prepare_slack_egress", chat_id="c1", reply_to="r1", metadata=None, state=_SL,
     io={"_apply_slack_thread_anchor": "r1", "_slack_unfurl_hints": {"unfurl_links": True}})

# ── status_metadata_for_turn ──
case("status_metadata_for_turn", chat_id="c1", anchor="a1", metadata={"k": "v"},
     io={"_with_status_thread_anchor": {"k": "v", "thread_id": "a1"}})
case("status_metadata_for_turn", chat_id="c1", anchor=None, metadata={},
     io={"_with_status_thread_anchor": {}})
case("status_metadata_for_turn", chat_id="c1", anchor="a1", metadata=None,
     io={"_with_status_thread_anchor": {"thread_id": "a1"}})
case("status_metadata_for_turn", chat_id="c1", anchor="a1", metadata={},
     io={"_with_status_thread_anchor": rz("RuntimeError", "boom")})
case("status_metadata_for_turn", chat_id="c1", anchor="a1", metadata={}, io={})
case("status_metadata_for_turn", chat_id="c1", anchor="a1", metadata={},
     io={"_with_status_thread_anchor": rz("KeyboardInterrupt", "ki")})

# ── delete_message ──
case("delete_message", chat_id="c1", message_id="m1", state={"transport": T},
     io={"_descriptor_for_chat": {"supported_ops": ["send", "delete"]},
         "_with_scope": {"scope_id": "s"}, "_outbound": {"success": True}})
case("delete_message", chat_id="c1", message_id="m1", state={"transport": None}, io={})
case("delete_message", chat_id="c1", message_id="m1", state={"transport": T},
     io={"_descriptor_for_chat": {"supported_ops": ["send"]}})
case("delete_message", chat_id="c1", message_id="m1", state={"transport": T},
     io={"_descriptor_for_chat": {"supported_ops": ["delete"]}, "_with_scope": {},
         "_outbound": rz("RuntimeError", "down"), "logger.debug": None})
case("delete_message", chat_id="c1", message_id="m1", state={"transport": T},
     io={"_descriptor_for_chat": {"supported_ops": ["delete"]}, "logger.debug": None})
case("delete_message", chat_id="c1", message_id="m1", state={"transport": T},
     io={"_descriptor_for_chat": {"supported_ops": ["delete"]}, "_with_scope": {}, "_outbound": None})
case("delete_message", chat_id="c1", message_id="m1", state={"transport": T},
     io={"_descriptor_for_chat": {}})
case("delete_message", chat_id="c1", message_id="m1", state={"transport": T},
     io={"_descriptor_for_chat": {"supported_ops": 5}})
case("delete_message", chat_id="c1", message_id="m1", state={"transport": T},
     io={"_descriptor_for_chat": {"supported_ops": ["delete"]}, "_with_scope": {},
         "_outbound": rz("GeneratorExit", "ge")})
case("delete_message", chat_id="c1", message_id="m1", state={"transport": T},
     io={"_descriptor_for_chat": {"supported_ops": ["delete"]}, "_with_scope": {},
         "_outbound": {"success": False}})

# ── _send_local_media ──
_LM = dict(chat_id="c1", media_kind="image", path="/p.png", reply_to=None, metadata={},
           fallback={"__callable__": "fallback"})
case("_send_local_media", caption="cap", filename="f.png",
     io={"_send_media": sr(success=True, message_id="9")}, **_LM)
case("_send_local_media", caption=None,
     io={"_send_media": None, "fallback": sr(success=True, message_id="7")}, **_LM)
case("_send_local_media", caption=None,
     io={"_send_media": nd("upload failed"), "fallback": sr(success=True, message_id="7", raw_response={"x": 1})}, **_LM)
case("_send_local_media", caption=None,
     io={"_send_media": rz("RuntimeError", "media")}, **_LM)
case("_send_local_media", caption=None,
     io={"_send_media": None, "fallback": rz("RuntimeError", "fb")}, **_LM)
case("_send_local_media", caption=None, fallback=None, io={"_send_media": None},
     **{k: v for k, v in _LM.items() if k != "fallback"})
case("_send_local_media", caption=None, io={}, **_LM)
case("_send_local_media", caption=None,
     io={"_send_media": rz("KeyboardInterrupt", "ki")}, **_LM)
case("_send_local_media", caption=None,
     io={"_send_media": nd("connector did not deliver the attachment"),
         "fallback": sr(success=False, message_id=None, raw_response=None)}, **_LM)

# ── _mint_and_send_prompt ──
_MP = dict(kind="exec_approval", prompt_state={"session_key": "sk"}, chat_id="c1",
           prompt_kind="approval", text="t", options=[], metadata={})
case("_mint_and_send_prompt", state={"pending_prompts": {"pid1": {"x": 1}}},
     io={"_mint_prompt": "pid1", "_send_prompt": sr(success=True, message_id="9")}, **_MP)
case("_mint_and_send_prompt", state={"pending_prompts": {"pid1": {"x": 1}}},
     io={"_mint_prompt": "pid1", "_send_prompt": None}, **_MP)
case("_mint_and_send_prompt", state={"pending_prompts": {"pid1": {"x": 1}}},
     io={"_mint_prompt": "pid1", "_send_prompt": sr(success=False, error="declined")}, **_MP)
case("_mint_and_send_prompt", io={"_send_prompt": sr(success=True)}, **_MP)
case("_mint_and_send_prompt",
     io={"_mint_prompt": "pid1", "_send_prompt": rz("RuntimeError", "send")}, **_MP)
case("_mint_and_send_prompt",
     io={"_mint_prompt": "pid1", "_send_prompt": rz("CancelledError", "cancel")}, **_MP)
case("_mint_and_send_prompt", state={"pending_prompts": {}},
     io={"_mint_prompt": "pidX", "_send_prompt": None}, **_MP)

# ── _send_exec_approval_prompt ──
def prompt(actions, **kw):
    base = dict(chat_id="c1", session_key="sk", text="approve?", command="ls",
                description="d", smart_denied=False, metadata={}, request_id="r1")
    base["actions"] = actions
    base.update(kw)
    return base


case("_send_exec_approval_prompt",
     prompt=prompt([["Approve", "once", "primary"], ["Deny", "deny", ""]]),
     state={"pending_prompts": {}}, io={"_mint_prompt": "pid1", "_send_prompt": sr(success=True, message_id="9")})
case("_send_exec_approval_prompt", prompt=prompt([["Approve", "once", ""]]),
     io={"_mint_prompt": "pid1", "_send_prompt": None})
case("_send_exec_approval_prompt", prompt=prompt([["Approve", "once"]]), io={})
case("_send_exec_approval_prompt", prompt=prompt([["Approve", "once", "primary", "extra"]]), io={})
case("_send_exec_approval_prompt", prompt=prompt([5]), io={})
case("_send_exec_approval_prompt", prompt=prompt(None), io={})
case("_send_exec_approval_prompt", prompt=prompt([["Approve", "once", ""]]),
     io={"_send_prompt": sr(success=True)})
case("_send_exec_approval_prompt",
     prompt=prompt([["Approve", "once", "primary"]]),
     state={"pending_prompts": {"pid1": {"x": 1}}},
     io={"_mint_prompt": "pid1", "_send_prompt": sr(success=False, error="declined")})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-func count check (>=5)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print(f"wrote {len(CASES)} cases across {len(counts)} funcs to {OUT}")
    if short:
        print("WARNING funcs with <5 cases:", short)
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")


if __name__ == "__main__":
    main()