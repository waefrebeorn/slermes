#!/usr/bin/env python3
"""Generate tests/fixtures/matrix_adapter_flow2.jsonl — input cases for the
Matrix-adapter FLOW remainder oracle (the oracle computes want from LIVE
adapter.py at gate time; this script only emits {func, in} inputs).

Deterministic handcrafted branch-coverage cases (>=5 per function). Each flow
carries normal-path, branch, scripted-Exception-swallow, BaseException-
passthrough and fail-closed (fc=True) variants. Domains: client/state_store
getters return {null, non-empty record}; attribute carriers wrap {"__attr__":
{...}}; method presence is a key holding True; set-valued state sorts on both
sides; no integral floats in str()-computed positions.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "matrix_adapter_flow2.jsonl"
CASES = []


def add(func, args, io=None, state=None, fc=False):
    inp = {"args": args}
    if io:
        inp["io"] = io
    if state is not None:
        inp["state"] = state
    case = {"func": func, "in": inp}
    if fc:
        case["fc"] = True
    CASES.append(case)


def fl(v):
    return {"__float__": True, "v": v}


def rs(kind, msg=""):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def sc(v):
    return {"__script__": True, "ret": v}


def ad(attrs):
    return {"__attr__": attrs}


def cli(**kw):
    return dict(kw)


# ── _extra_truthy ──
for cfg in [True, False, "true", "1", "yes", "TRUE", "false", "0", "no", "", "bogus", 5, None]:
    add("_extra_truthy", {"extra": {}, "key": "k", "env_name": "E", "default": ""},
        {"extra_or_secret": cfg})
add("_extra_truthy", {"extra": {}, "key": "k", "env_name": "E", "default": ""}, fc=True)
add("_extra_truthy", {"extra": {}, "key": "k", "env_name": "E", "default": ""},
    {"extra_or_secret": rs("SystemExit", "bye")})

# ── _parse_require_mention ──
for cfg in [True, False, "false", "0", "off", "no", "", "bogus", None, "true"]:
    add("_parse_require_mention", {"extra": {}}, {"extra_or_secret": cfg})
add("_parse_require_mention", {"extra": {}}, fc=True)
add("_parse_require_mention", {"extra": {}}, {"extra_or_secret": rs("KeyboardInterrupt", "stop")})

# ── _parse_thread_require_mention ──
for cfg in [False, True, "false", "off", "", "yes", None]:
    add("_parse_thread_require_mention", {"extra": {}}, {"extra_or_secret": cfg})
add("_parse_thread_require_mention", {"extra": {}}, fc=True)
add("_parse_thread_require_mention", {"extra": {}}, {"extra_or_secret": rs("SystemExit", "bye")})

# ── _reset_clock_skew_detector ──
for st in [{}, {"_late_grace_drops": 5, "_late_grace_skew": 100, "_clock_skew_warned": True},
           {"other": 1}, {"_clock_skew_warned": True},
           {"_late_grace_drops": 2, "_late_grace_skew": 50.5, "_clock_skew_warned": False, "extra": "x"}]:
    add("_reset_clock_skew_detector", {}, state=st)

# ── _note_late_grace_drop ──
def note_state(drops, skew, warned):
    return {"_late_grace_drops": drops, "_late_grace_skew": skew, "_clock_skew_warned": warned}


add("_note_late_grace_drop", {"event_ts": 50, "self": {"_startup_ts": 100}},
    {"time.time": 200}, state=note_state(2, 100, True))  # warned -> no seam
add("_note_late_grace_drop", {"event_ts": 50, "self": {"_startup_ts": 100}},
    {"time.time": 120}, state=note_state(0, 0, False))  # diff 20 <= 30
add("_note_late_grace_drop", {"event_ts": 900, "self": {"_startup_ts": 1000}},
    {"time.time": 2000}, state=note_state(0, 0, False))  # first drop skew 100
add("_note_late_grace_drop", {"event_ts": 900, "self": {"_startup_ts": 1000}},
    {"time.time": 2000}, state=note_state(1, 100, False))  # matching -> 2
add("_note_late_grace_drop", {"event_ts": 900, "self": {"_startup_ts": 1000}},
    {"time.time": 2000}, state=note_state(2, 100, False))  # -> 3 warned
add("_note_late_grace_drop", {"event_ts": 800, "self": {"_startup_ts": 1000}},
    {"time.time": 2000}, state=note_state(1, 100, False))  # skew 200 differs -> reset
add("_note_late_grace_drop", {"event_ts": 1995, "self": {"_startup_ts": 1000}},
    {"time.time": 2000}, state=note_state(0, 0, False))  # skew 5 not > 5
add("_note_late_grace_drop", {"event_ts": 0, "self": {"_startup_ts": 100000}},
    {"time.time": 200000}, state=note_state(0, 0, False))  # skew 100000 >= 86400
add("_note_late_grace_drop", {"event_ts": 900, "self": {"_startup_ts": 1000}},
    {"time.time": None}, state=note_state(0, 0, False))  # None - int TypeError
add("_note_late_grace_drop", {"event_ts": None, "self": {"_startup_ts": 1000}},
    {"time.time": 2000}, state=note_state(0, 0, False))  # int - None TypeError
add("_note_late_grace_drop", {"event_ts": 900, "self": {"_startup_ts": 1000}},
    {"time.time": rs("SystemExit", "bye")}, state=note_state(0, 0, False))
add("_note_late_grace_drop", {"event_ts": 900, "self": {"_startup_ts": 1000}},
    state=note_state(0, 0, False), fc=True)  # no time.time script

# ── _is_authorized_user ──
add("_is_authorized_user", {"user_id": "@a:s", "room_id": "!r:s", "self": {"_authorization_check": True}},
    {"self._is_sender_authorized": True})
add("_is_authorized_user", {"user_id": "@a:s", "self": {"_authorization_check": True}},
    {"self._is_sender_authorized": False})
add("_is_authorized_user", {"user_id": "@a:s", "self": {"_authorization_check": True}},
    {"self._is_sender_authorized": 1})  # `is True` identity -> False
add("_is_authorized_user", {"user_id": "@a:s", "self": {"_authorization_check": None}},
    {"get_scoped_secret": "yes"})
add("_is_authorized_user", {"user_id": "@a:s", "self": {"_allowed_user_ids": ["@a:s"]}},
    {"get_scoped_secret": "false"})
add("_is_authorized_user", {"user_id": "@b:s", "self": {"_allowed_user_ids": ["@a:s"]}},
    {"get_scoped_secret": "false"})
add("_is_authorized_user", {"user_id": "@x:s", "self": {"_allowed_user_ids": []}},
    {"get_scoped_secret": ""})
add("_is_authorized_user", {"user_id": "@a:s", "self": {}}, {"get_scoped_secret": None})
add("_is_authorized_user", {"user_id": "@a:s", "self": {}}, {"get_scoped_secret": rs("SystemExit", "bye")})
add("_is_authorized_user", {"user_id": "@a:s", "self": {"_authorization_check": True}},
    {"self._is_sender_authorized": rs("SystemExit", "bye")})
add("_is_authorized_user", {"user_id": "@a:s", "self": {}}, fc=True)

# ── _on_invite ──
def ev(room="!r:s", sender="@a:s", content=None):
    e = {"room_id": room, "sender": sender}
    if content is not None:
        e["content"] = content
    return e


add("_on_invite", {"event": ev(content={"is_direct": True})},
    {"self._is_authorized_user": True, "self._schedule_invite_join": None})
add("_on_invite", {"event": ev(content={"is_direct": True})},
    {"self._is_authorized_user": False})
add("_on_invite", {"event": ev(content={"is_direct": False})},
    {"self._is_authorized_user": True, "self._schedule_invite_join": None})
add("_on_invite", {"event": ev(sender="", content={"is_direct": True})},
    {"self._is_authorized_user": True, "self._schedule_invite_join": None})
add("_on_invite", {"event": ev()},
    {"self._is_authorized_user": True, "self._schedule_invite_join": None})
add("_on_invite", {"event": ev(content={"is_direct": True})},
    {"self._is_authorized_user": rs("SystemExit", "bye")})
add("_on_invite", {"event": ev(content={"is_direct": True})},
    {"self._is_authorized_user": True, "self._schedule_invite_join": rs("SystemExit", "bye")})
add("_on_invite", {"event": ev(content={"is_direct": True})}, fc=True)

# ── _schedule_pending_invite_joins ──
add("_schedule_pending_invite_joins", {"sync_data": None}, state={"_joined_rooms": []})
add("_schedule_pending_invite_joins", {"sync_data": {"rooms": {"invite": {}}}}, state={"_joined_rooms": []})
add("_schedule_pending_invite_joins", {"sync_data": {"rooms": {"invite": "x"}}}, state={"_joined_rooms": []})
add("_schedule_pending_invite_joins", {"sync_data": {"rooms": None}}, state={"_joined_rooms": []})
add("_schedule_pending_invite_joins", {"sync_data": {"rooms": {"invite": {"!r1": {}}}}},
    state={"_joined_rooms": ["!r1"]})
add("_schedule_pending_invite_joins", {"sync_data": {"rooms": {"invite": {"!r1": {}}}}},
    {"self._extract_invite_dm_signal": [[True, "@a:s"]], "self._is_authorized_user": True,
     "self._schedule_invite_join": None}, state={"_joined_rooms": []})
add("_schedule_pending_invite_joins", {"sync_data": {"rooms": {"invite": {"!r1": {}}}}},
    {"self._extract_invite_dm_signal": [[False, "@a:s"]], "self._is_authorized_user": False},
    state={"_joined_rooms": []})
add("_schedule_pending_invite_joins", {"sync_data": {"rooms": {"invite": {"!r1": {}, "!r2": {}}}}},
    {"self._extract_invite_dm_signal": [[True, "@b:s"]], "self._is_authorized_user": True,
     "self._schedule_invite_join": None}, state={"_joined_rooms": ["!r1"]})
add("_schedule_pending_invite_joins", {"sync_data": {"rooms": {"invite": {"!r1": {}}}}},
    {"self._extract_invite_dm_signal": rs("SystemExit", "bye")}, state={"_joined_rooms": []})
add("_schedule_pending_invite_joins", {"sync_data": {"rooms": {"invite": {"!r1": {}}}}},
    state={"_joined_rooms": []}, fc=True)

# ── _extract_invite_dm_signal ──
def member_ev(state_key="@bot:s", membership="invite", is_direct=True, sender="@a:s", typ="m.room.member"):
    e = {"type": typ, "state_key": state_key, "content": {"membership": membership, "is_direct": is_direct}}
    if sender is not None:
        e["sender"] = sender
    return e


def room(events):
    return {"invite_state": {"events": events}}


add("_extract_invite_dm_signal", {"invited_room": room([member_ev()]), "self": {"_user_id": "@bot:s"}})
add("_extract_invite_dm_signal", {"invited_room": room([member_ev(is_direct=False)]), "self": {"_user_id": "@bot:s"}})
add("_extract_invite_dm_signal", {"invited_room": room([member_ev(sender=None)]), "self": {"_user_id": "@bot:s"}})
add("_extract_invite_dm_signal", {"invited_room": room([member_ev()]), "self": {"_user_id": ""}})
add("_extract_invite_dm_signal", {"invited_room": "x", "self": {"_user_id": "@bot:s"}})
add("_extract_invite_dm_signal", {"invited_room": {"invite_state": "x"}, "self": {"_user_id": "@bot:s"}})
add("_extract_invite_dm_signal", {"invited_room": {"invite_state": {"events": "x"}}, "self": {"_user_id": "@bot:s"}})
add("_extract_invite_dm_signal", {"invited_room": room([member_ev(membership="join")]), "self": {"_user_id": "@bot:s"}})
add("_extract_invite_dm_signal", {"invited_room": room([member_ev(state_key="@other:s")]), "self": {"_user_id": "@bot:s"}})
add("_extract_invite_dm_signal", {"invited_room": room([{"type": "m.room.member", "state_key": "@bot:s", "content": "x"}]), "self": {"_user_id": "@bot:s"}})
add("_extract_invite_dm_signal", {"invited_room": room([]), "self": {"_user_id": "@bot:s"}})

# ── _join_room_by_id ──
add("_join_room_by_id", {"room_id": "", "self": {"_client": cli(join_room=True)}}, state={"_joined_rooms": []})
add("_join_room_by_id", {"room_id": "!r", "self": {"_client": cli(join_room=True)}}, state={"_joined_rooms": ["!r"]})
add("_join_room_by_id", {"room_id": "!r", "self": {"_client": None}}, state={"_joined_rooms": []})
add("_join_room_by_id", {"room_id": "!r", "self": {"_client": cli(join_room=True)}},
    {"client.join_room": None, "self._invalidate_room_identities": None, "self._refresh_dm_cache": None},
    state={"_joined_rooms": []})
add("_join_room_by_id", {"room_id": "!r", "self": {"_client": cli(join_room=True, leave_room=True)}},
    {"client.join_room": rs("RuntimeError", "No servers in room"), "client.leave_room": None},
    state={"_joined_rooms": []})
add("_join_room_by_id", {"room_id": "!r", "self": {"_client": cli(join_room=True, leave_room=True)}},
    {"client.join_room": rs("RuntimeError", "room not found"), "client.leave_room": None},
    state={"_joined_rooms": []})
add("_join_room_by_id", {"room_id": "!r", "self": {"_client": cli(join_room=True, leave_room=True)}},
    {"client.join_room": rs("RuntimeError", "transient")}, state={"_joined_rooms": []})
add("_join_room_by_id", {"room_id": "!r", "self": {"_client": cli(join_room=True)}},
    {"client.join_room": rs("SystemExit", "bye")}, state={"_joined_rooms": []})
add("_join_room_by_id", {"room_id": "!r", "self": {"_client": cli(join_room=True)}},
    {"client.join_room": None, "self._invalidate_room_identities": rs("SystemExit", "bye")},
    state={"_joined_rooms": []})
add("_join_room_by_id", {"room_id": "!r", "self": {"_client": cli(join_room=True)}},
    {"client.join_room": None, "self._invalidate_room_identities": None,
     "self._refresh_dm_cache": rs("RuntimeError", "x")}, state={"_joined_rooms": []})
add("_join_room_by_id", {"room_id": "!r", "self": {"_client": cli(join_room=True, leave_room=True)}},
    {"client.join_room": rs("RuntimeError", "no servers"), "client.leave_room": rs("SystemExit", "bye")},
    state={"_joined_rooms": []})
add("_join_room_by_id", {"room_id": "!r", "self": {"_client": cli(join_room=True)}},
    state={"_joined_rooms": []}, fc=True)

# ── _fetch_m_direct ──
add("_fetch_m_direct", {"self": {"_client": None}})
add("_fetch_m_direct", {"self": {"_client": cli(get_account_data=True)}},
    {"client.get_account_data": rs("RuntimeError", "x")})
add("_fetch_m_direct", {"self": {"_client": cli(get_account_data=True)}},
    {"client.get_account_data": rs("SystemExit", "bye")})
add("_fetch_m_direct", {"require_dict": False, "self": {"_client": cli(get_account_data=True)}},
    {"client.get_account_data": ad({"content": {"@a": ["!r1"]}})})
add("_fetch_m_direct", {"require_dict": True, "self": {"_client": cli(get_account_data=True)}},
    {"client.get_account_data": ad({"content": {"@a": ["!r1"]}})})
add("_fetch_m_direct", {"require_dict": True, "self": {"_client": cli(get_account_data=True)}},
    {"client.get_account_data": ad({"content": ["!r1"]})})
add("_fetch_m_direct", {"require_dict": False, "self": {"_client": cli(get_account_data=True)}},
    {"client.get_account_data": ad({"content": ["!r1"]})})
add("_fetch_m_direct", {"self": {"_client": cli(get_account_data=True)}},
    {"client.get_account_data": ad({})})
add("_fetch_m_direct", {"self": {"_client": cli(get_account_data=True)}},
    {"client.get_account_data": {"@a": ["!r1"]}})
add("_fetch_m_direct", {"self": {"_client": cli(get_account_data=True)}},
    {"client.get_account_data": None})
add("_fetch_m_direct", {"self": {"_client": cli(get_account_data=True)}},
    {"client.get_account_data": "x"})
add("_fetch_m_direct", {"self": {"_client": cli(get_account_data=True)}}, fc=True)

# ── _refresh_dm_cache ──
add("_refresh_dm_cache", {"self": {"_client": None}}, state={"_joined_rooms": []})
add("_refresh_dm_cache", {"self": {"_client": cli(mxid="@bot:s")}},
    {"self._fetch_m_direct": None}, state={"_joined_rooms": []})
add("_refresh_dm_cache", {"self": {"_client": cli(mxid="@bot:s")}},
    {"self._fetch_m_direct": {"@a": ["!r1", "!r2"], "@b": ["!r3"]},
     "self._invalidate_room_identities": None}, state={"_joined_rooms": ["!r1", "!r3", "!r9"]})
add("_refresh_dm_cache", {"self": {"_client": cli(mxid="@bot:s")}},
    {"self._fetch_m_direct": {}, "self._invalidate_room_identities": None},
    state={"_joined_rooms": ["!r1"]})
add("_refresh_dm_cache", {"self": {"_client": cli(mxid="@bot:s")}},
    {"self._fetch_m_direct": sc(["x"])}, state={"_joined_rooms": []})
add("_refresh_dm_cache", {"self": {"_client": cli(mxid="@bot:s")}},
    {"self._fetch_m_direct": rs("SystemExit", "bye")}, state={"_joined_rooms": []})
add("_refresh_dm_cache", {"self": {"_client": cli(mxid="@bot:s")}},
    {"self._fetch_m_direct": {"@a": ["!r1"]}, "self._invalidate_room_identities": rs("SystemExit", "bye")},
    state={"_joined_rooms": ["!r1"]})
add("_refresh_dm_cache", {"self": {"_client": cli(mxid="@bot:s")}},
    state={"_joined_rooms": []}, fc=True)

# ── _record_dm_room ──
add("_record_dm_room", {"room_id": "!r", "inviter": "@a", "self": {"_client": None}}, state={"_dm_rooms": {}})
add("_record_dm_room", {"room_id": "!r", "inviter": "@a", "self": {"_client": cli(set_account_data=True)}},
    {"self._fetch_m_direct": None, "client.set_account_data": None, "self._invalidate_room_identities": None},
    state={"_dm_rooms": {}})
add("_record_dm_room", {"room_id": "!r", "inviter": "@a", "self": {"_client": cli(set_account_data=True)}},
    {"self._fetch_m_direct": {"@a": ["!r"]}, "self._invalidate_room_identities": None},
    state={"_dm_rooms": {}})
add("_record_dm_room", {"room_id": "!r", "inviter": "@a", "self": {"_client": cli(set_account_data=True)}},
    {"self._fetch_m_direct": {"@a": ["!other"]}, "client.set_account_data": None,
     "self._invalidate_room_identities": None}, state={"_dm_rooms": {}})
add("_record_dm_room", {"room_id": "!r", "inviter": "@a", "self": {"_client": cli(set_account_data=True)}},
    {"self._fetch_m_direct": {"@a": "notlist"}, "client.set_account_data": None,
     "self._invalidate_room_identities": None}, state={"_dm_rooms": {}})
add("_record_dm_room", {"room_id": "!r", "inviter": "@a", "self": {"_client": cli(set_account_data=True)}},
    {"self._fetch_m_direct": None, "client.set_account_data": rs("RuntimeError", "x"),
     "self._invalidate_room_identities": None}, state={"_dm_rooms": {}})
add("_record_dm_room", {"room_id": "!r", "inviter": "@a", "self": {"_client": cli(set_account_data=True)}},
    {"self._fetch_m_direct": None, "client.set_account_data": rs("SystemExit", "bye")},
    state={"_dm_rooms": {}})
add("_record_dm_room", {"room_id": "!r", "inviter": "@a", "self": {"_client": cli(set_account_data=True)}},
    {"self._fetch_m_direct": rs("SystemExit", "bye")}, state={"_dm_rooms": {}})
add("_record_dm_room", {"room_id": "!r", "inviter": "@a", "self": {"_client": cli(set_account_data=True)}},
    state={"_dm_rooms": {}}, fc=True)

# ── _is_dm_room ──
add("_is_dm_room", {"room_id": "!r"}, {"self._resolve_room_identity": ad({"chat_type": "dm"})})
add("_is_dm_room", {"room_id": "!r"}, {"self._resolve_room_identity": ad({"chat_type": "group"})})
add("_is_dm_room", {"room_id": "!r"}, {"self._resolve_room_identity": None})
add("_is_dm_room", {"room_id": "!r"}, {"self._resolve_room_identity": {"chat_type": "dm"}})
add("_is_dm_room", {"room_id": "!r"}, {"self._resolve_room_identity": rs("SystemExit", "bye")})
add("_is_dm_room", {"room_id": "!r"}, fc=True)

# ── _send_reaction ──
add("_send_reaction", {"room_id": "!r", "event_id": "$e", "emoji": "eyes", "self": {"_client": None}})
add("_send_reaction", {"room_id": "!r", "event_id": "$e", "emoji": "eyes", "self": {"_client": cli(send_message_event=True)}},
    {"client.send_message_event": "$react"})
add("_send_reaction", {"room_id": "!r", "event_id": "$e", "emoji": "eyes", "self": {"_client": cli(send_message_event=True)}},
    {"client.send_message_event": 123})
add("_send_reaction", {"room_id": "!r", "event_id": "$e", "emoji": "eyes", "self": {"_client": cli(send_message_event=True)}},
    {"client.send_message_event": None})
add("_send_reaction", {"room_id": "!r", "event_id": "$e", "emoji": "eyes", "self": {"_client": cli(send_message_event=True)}},
    {"client.send_message_event": rs("RuntimeError", "x")})
add("_send_reaction", {"room_id": "!r", "event_id": "$e", "emoji": "eyes", "self": {"_client": cli(send_message_event=True)}},
    {"client.send_message_event": rs("SystemExit", "bye")})
add("_send_reaction", {"room_id": "!r", "event_id": "$e", "emoji": "eyes", "self": {"_client": cli(send_message_event=True)}}, fc=True)

# ── send_read_receipt ──
add("send_read_receipt", {"room_id": "!r", "event_id": "$e", "self": {"_client": None}})
add("send_read_receipt", {"room_id": "!r", "event_id": "$e", "self": {"_client": cli(set_fully_read_marker=True)}},
    {"client.set_fully_read_marker": None})
add("send_read_receipt", {"room_id": "!r", "event_id": "$e", "self": {"_client": cli(send_receipt=True)}},
    {"client.send_receipt": None})
add("send_read_receipt", {"room_id": "!r", "event_id": "$e", "self": {"_client": cli(set_read_markers=True)}},
    {"client.set_read_markers": None})
add("send_read_receipt", {"room_id": "!r", "event_id": "$e", "self": {"_client": cli(mxid="@bot:s")}})
add("send_read_receipt", {"room_id": "!r", "event_id": "$e", "self": {"_client": cli(send_receipt=True)}},
    {"client.send_receipt": rs("RuntimeError", "x")})
add("send_read_receipt", {"room_id": "!r", "event_id": "$e", "self": {"_client": cli(send_receipt=True)}},
    {"client.send_receipt": rs("SystemExit", "bye")})
add("send_read_receipt", {"room_id": "!r", "event_id": "$e", "self": {"_client": cli(send_receipt=True)}}, fc=True)

# ── _client_op ──
add("_client_op", {"ok_msg": ["ok"], "err_msg": "err", "level": "warning", "self": {"_client": None}})
add("_client_op", {"ok_msg": ["ok"], "err_msg": "err", "self": {"_client": cli(mxid="@bot:s")}},
    {"coro_factory": None})
add("_client_op", {"ok_msg": ["ok"], "err_msg": "err", "self": {"_client": cli(mxid="@bot:s")}},
    {"coro_factory": rs("RuntimeError", "x")})
add("_client_op", {"ok_msg": ["ok"], "err_msg": "err", "self": {"_client": cli(mxid="@bot:s")}},
    {"coro_factory": rs("SystemExit", "bye")})
add("_client_op", {"ok_msg": ["ok"], "err_msg": "err", "self": {"_client": cli(mxid="@bot:s")}},
    {"coro_factory": rs("CancelledError", "")})
add("_client_op", {"ok_msg": ["ok"], "err_msg": "err", "self": {"_client": cli(mxid="@bot:s")}}, fc=True)

# ── redact_message ──
add("redact_message", {"room_id": "!r", "event_id": "$e", "reason": "x", "self": {"_client": None}})
add("redact_message", {"room_id": "!r", "event_id": "$e", "reason": "x", "self": {"_client": cli(redact=True)}},
    {"client.redact": None})
add("redact_message", {"room_id": "!r", "event_id": "$e", "reason": "", "self": {"_client": cli(redact=True)}},
    {"client.redact": None})
add("redact_message", {"room_id": "!r", "event_id": "$e", "reason": "x", "self": {"_client": cli(redact=True)}},
    {"client.redact": rs("RuntimeError", "x")})
add("redact_message", {"room_id": "!r", "event_id": "$e", "reason": "x", "self": {"_client": cli(redact=True)}},
    {"client.redact": rs("SystemExit", "bye")})
add("redact_message", {"room_id": "!r", "event_id": "$e", "reason": "x", "self": {"_client": cli(redact=True)}}, fc=True)

# ── invite_user ──
add("invite_user", {"room_id": "!r", "user_id": "@a:s", "self": {"_client": None}})
add("invite_user", {"room_id": "!r", "user_id": "@a:s", "self": {"_client": cli(invite_user=True)}},
    {"client.invite_user": None})
add("invite_user", {"room_id": "!r", "user_id": "@a:s", "self": {"_client": cli(invite_user=True)}},
    {"client.invite_user": rs("RuntimeError", "x")})
add("invite_user", {"room_id": "!r", "user_id": "@a:s", "self": {"_client": cli(invite_user=True)}},
    {"client.invite_user": rs("SystemExit", "bye")})
add("invite_user", {"room_id": "!r", "user_id": "@a:s", "self": {"_client": cli(invite_user=True)}},
    {"client.invite_user": rs("CancelledError", "")})
add("invite_user", {"room_id": "!r", "user_id": "@a:s", "self": {"_client": cli(invite_user=True)}}, fc=True)

# ── set_presence ──
add("set_presence", {"state": "online", "status_msg": "", "self": {"_client": None}})
add("set_presence", {"state": "online", "status_msg": "", "self": {"_client": cli(set_presence=True)}},
    {"client.set_presence": None})
add("set_presence", {"state": "offline", "status_msg": "busy", "self": {"_client": cli(set_presence=True)}},
    {"client.set_presence": None})
add("set_presence", {"state": "unavailable", "status_msg": "", "self": {"_client": cli(set_presence=True)}},
    {"client.set_presence": None})
add("set_presence", {"state": "bogus", "status_msg": "", "self": {"_client": cli(set_presence=True)}})
add("set_presence", {"state": 5, "status_msg": "", "self": {"_client": cli(set_presence=True)}})
add("set_presence", {"state": ["x"], "status_msg": "", "self": {"_client": cli(set_presence=True)}})
add("set_presence", {"state": "online", "status_msg": "", "self": {"_client": cli(set_presence=True)}},
    {"client.set_presence": rs("RuntimeError", "x")})
add("set_presence", {"state": "online", "status_msg": "", "self": {"_client": cli(set_presence=True)}},
    {"client.set_presence": rs("SystemExit", "bye")})
add("set_presence", {"state": "online", "status_msg": "", "self": {"_client": cli(set_presence=True)}}, fc=True)

# ── _get_room_member_count ──
add("_get_room_member_count", {"room_id": "!r", "self": {"_client": None}})
add("_get_room_member_count", {"room_id": "!r", "self": {"_client": cli(state_store={"get_members": True})}},
    {"state_store.get_members": ["a", "b"]})
add("_get_room_member_count", {"room_id": "!r", "self": {"_client": cli(state_store={"get_members": True}, joined_members=True)}},
    {"state_store.get_members": None, "client.joined_members": ad({"members": ["a", "b", "c"]})})
add("_get_room_member_count", {"room_id": "!r", "self": {"_client": cli(state_store={"get_members": True}, joined_members=True)}},
    {"state_store.get_members": rs("RuntimeError", "x"), "client.joined_members": ad({"members": ["a"]})})
add("_get_room_member_count", {"room_id": "!r", "self": {"_client": cli(state_store={"get_members": True}, joined_members=True)}},
    {"state_store.get_members": 5, "client.joined_members": ad({"members": ["a", "b"]})})
add("_get_room_member_count", {"room_id": "!r", "self": {"_client": cli(joined_members=True)}},
    {"client.joined_members": ad({"members": ["a", "b", "c"]})})
add("_get_room_member_count", {"room_id": "!r", "self": {"_client": cli(joined_members=True)}},
    {"client.joined_members": None})
add("_get_room_member_count", {"room_id": "!r", "self": {"_client": cli(joined_members=True)}},
    {"client.joined_members": ad({})})
add("_get_room_member_count", {"room_id": "!r", "self": {"_client": cli(mxid="@bot:s")}})
add("_get_room_member_count", {"room_id": "!r", "self": {"_client": cli(state_store={"get_members": True})}},
    {"state_store.get_members": rs("SystemExit", "bye")})
add("_get_room_member_count", {"room_id": "!r", "self": {"_client": cli(joined_members=True)}},
    {"client.joined_members": rs("SystemExit", "bye")})
add("_get_room_member_count", {"room_id": "!r", "self": {"_client": cli(state_store={"get_members": True})}}, fc=True)

# ── _get_room_state_value ──
add("_get_room_state_value", {"room_id": "!r", "event_type": "m.room.name", "key": "name", "self": {"_client": None}})
add("_get_room_state_value", {"room_id": "!r", "event_type": "m.room.name", "key": "name", "self": {"_client": cli(mxid="@bot:s")}})
add("_get_room_state_value", {"room_id": "!r", "event_type": "m.room.name", "key": "name", "self": {"_client": cli(get_state_event=True)}},
    {"client.get_state_event": rs("RuntimeError", "x")})
add("_get_room_state_value", {"room_id": "!r", "event_type": "m.room.name", "key": "name", "self": {"_client": cli(get_state_event=True)}},
    {"client.get_state_event": rs("SystemExit", "bye")})
add("_get_room_state_value", {"room_id": "!r", "event_type": "m.room.name", "key": "name", "self": {"_client": cli(get_state_event=True)}},
    {"client.get_state_event": {"name": "Room"}, "self._state_event_value": "Room"})
add("_get_room_state_value", {"room_id": "!r", "event_type": "m.room.name", "key": "name", "self": {"_client": cli(get_state_event=True)}},
    {"client.get_state_event": {"name": "Room"}, "self._state_event_value": "  "})
add("_get_room_state_value", {"room_id": "!r", "event_type": "m.room.name", "key": "name", "self": {"_client": cli(get_state_event=True)}},
    {"client.get_state_event": {"name": "Room"}, "self._state_event_value": None})
add("_get_room_state_value", {"room_id": "!r", "event_type": "m.room.name", "key": "name", "self": {"_client": cli(get_state_event=True)}},
    {"client.get_state_event": {"name": "Room"}, "self._state_event_value": 5})
add("_get_room_state_value", {"room_id": "!r", "event_type": "m.room.name", "key": "name", "self": {"_client": cli(get_state_event=True)}},
    {"client.get_state_event": {"name": "Room"}, "self._state_event_value": rs("SystemExit", "bye")})
add("_get_room_state_value", {"room_id": "!r", "event_type": "m.room.name", "key": "name", "self": {"_client": cli(get_state_event=True)}}, fc=True)

# ── _get_display_name ──
add("_get_display_name", {"room_id": "!r", "user_id": "@bob:s", "self": {"_client": cli(state_store={"get_member": True})}},
    {"state_store.get_member": ad({"displayname": "Alice"})})
add("_get_display_name", {"room_id": "!r", "user_id": "@bob:s", "self": {"_client": cli(state_store={"get_member": True})}},
    {"state_store.get_member": ad({"displayname": ""})})
add("_get_display_name", {"room_id": "!r", "user_id": "@bob:s", "self": {"_client": cli(state_store={"get_member": True})}},
    {"state_store.get_member": None})
add("_get_display_name", {"room_id": "!r", "user_id": "@bob:s", "self": {"_client": cli(state_store={"get_member": True})}},
    {"state_store.get_member": rs("RuntimeError", "x")})
add("_get_display_name", {"room_id": "!r", "user_id": "@bob:s", "self": {"_client": cli(mxid="@bot:s")}})
add("_get_display_name", {"room_id": "!r", "user_id": "bob", "self": {"_client": None}})
add("_get_display_name", {"room_id": "!r", "user_id": "@bob", "self": {"_client": None}})
add("_get_display_name", {"room_id": "!r", "user_id": "@bob:s", "self": {"_client": cli(state_store={"get_member": True})}},
    {"state_store.get_member": rs("SystemExit", "bye")})
add("_get_display_name", {"room_id": "!r", "user_id": "@bob:s", "self": {"_client": None}})
add("_get_display_name", {"room_id": "!r", "user_id": "@bob:s", "self": {"_client": cli(state_store={"get_member": True})}}, fc=True)

# ── _set_typing ──
add("_set_typing", {"chat_id": "!r", "timeout": 5000, "self": {"_client": None}})
add("_set_typing", {"chat_id": "!r", "timeout": 5000, "self": {"_client": cli(set_typing=True)}},
    {"client.set_typing": None})
add("_set_typing", {"chat_id": "!r", "timeout": 5000, "self": {"_client": cli(set_typing=True)}},
    {"client.set_typing": rs("RuntimeError", "x")})
add("_set_typing", {"chat_id": "!r", "timeout": 5000, "self": {"_client": cli(set_typing=True)}},
    {"client.set_typing": rs("SystemExit", "bye")})
add("_set_typing", {"chat_id": "!r", "timeout": 5000, "self": {"_client": cli(set_typing=True)}}, fc=True)

# ── send_typing ──
add("send_typing", {"chat_id": "!r", "self": {"_client": None}})
add("send_typing", {"chat_id": "!r", "self": {"_client": cli(set_typing=True)}}, {"client.set_typing": None})
add("send_typing", {"chat_id": "!r", "self": {"_client": cli(set_typing=True)}},
    {"client.set_typing": rs("RuntimeError", "x")})
add("send_typing", {"chat_id": "!r", "self": {"_client": cli(set_typing=True)}},
    {"client.set_typing": rs("SystemExit", "bye")})
add("send_typing", {"chat_id": "!r", "self": {"_client": cli(set_typing=True)}}, fc=True)

# ── stop_typing ──
add("stop_typing", {"chat_id": "!r", "self": {"_client": None}})
add("stop_typing", {"chat_id": "!r", "self": {"_client": cli(set_typing=True)}}, {"client.set_typing": None})
add("stop_typing", {"chat_id": "!r", "self": {"_client": cli(set_typing=True)}},
    {"client.set_typing": rs("RuntimeError", "x")})
add("stop_typing", {"chat_id": "!r", "self": {"_client": cli(set_typing=True)}},
    {"client.set_typing": rs("CancelledError", "")})
add("stop_typing", {"chat_id": "!r", "self": {"_client": cli(set_typing=True)}}, fc=True)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {OUT} : {len(CASES)} cases, {len(counts)} funcs")
    low = [f for f, n in counts.items() if n < 5]
    if low:
        print("WARNING funcs with <5 cases:", low)
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")


if __name__ == "__main__":
    main()