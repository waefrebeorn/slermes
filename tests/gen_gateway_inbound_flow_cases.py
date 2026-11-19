#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_inbound_flow.jsonl — the scripted-seam
cases for the gateway_inbound_flow oracle. Each case carries the function
name, its args, the initial self/world state, and the io scripts (seam-name
-> value | per-call list | {"__script__": ret/raise/live}). The io sequences
were derived by walking the live decision chains in call order (short-circuits
and suppress blocks included); the oracle cross-checks every io key against
the C port's seam registry.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "gateway_inbound_flow.jsonl")

CASES = []


def add(func, args, state=None, io=None):
    CASES.append({"func": func, "in": {"args": args, "state": state or {}, "io": io or {}}})


def auth_state(**over):
    base = {
        "profile_id": "p1", "instance_id": "i1", "epoch": 7,
        "retiring": False, "runner": {"_draining": False},
        "sessions": {}, "adopted": {}, "delivered_stops": {}, "pending_stops": {},
        "pending_steers": {}, "executing": {}, "rotated_owners": {},
        "config": None, "session_states": {}, "_startup_restore_in_progress": False,
        "executing_admission": None,
    }
    base.update(over)
    return base


def live(route="k1", platform="local", user_id="u1", chat_id="c1", settling=None):
    return {"route": route,
            "source": {"platform": platform, "user_id": user_id, "chat_id": chat_id},
            "settling": settling or {}}


def row(aid="a1", seq=1, status="queued", outcome=None, owner_epoch=7, generation=None,
        target="s1", request_id="r1", principal_id="messaging:x", payload=None,
        intent="queue", claimed=False, transcript_mark=0, queued_owner_epoch=None):
    d = {"seq": seq, "admission_id": aid, "request_id": request_id,
         "principal_id": principal_id, "target_session_id": target,
         "payload": payload if payload is not None else {"text": "hi"},
         "intent": intent, "status": status, "outcome": outcome,
         "owner_epoch": owner_epoch, "generation": generation}
    if claimed:
        d["transcript_mark"] = transcript_mark
        d["queued_owner_epoch"] = queued_owner_epoch
    return d


def srow(chat_id="c1", revision=4, generation=2):
    return {"chat_id": chat_id, "runtime_revision": revision, "runtime_generation": generation}


def actor(subject="u1", profile_id="p1", caps=("session:read",), transport_id="t1"):
    return {"subject": subject, "profile_id": profile_id,
            "capabilities": sorted(caps), "transport_id": transport_id}


def ref(sid="s1", profile_id="p1"):
    return {"profile_id": profile_id, "session_id": sid}


def raise_spec(kind, message="", **extra):
    return {"__script__": True, "raise": dict({"type": kind, "message": message}, **extra)}


def refusal(reason, field=None, detail=None):
    return raise_spec("AuthorityRefusal", reason=reason, field=field, detail=detail)


def script(ret=None, live_spec=None):
    d = {"__script__": True, "ret": ret}
    if live_spec is not None:
        d["live"] = live_spec
    return d


# ══════════════ admission_row ══════════════
add("admission_row", {"record": None})
add("admission_row", {"record": row()})
add("admission_row", {"record": row(claimed=True, transcript_mark=99, queued_owner_epoch=3)})
add("admission_row", {"record": row(status="terminal", outcome="cancelled", generation=5)})
add("admission_row", {"record": row(payload={"text": "x", "native_text_v1": {"event": {"text": "y"}}})})
add("admission_row", {"record": row(owner_epoch=0)})

# ══════════════ _require_admission_open ══════════════
add("_require_admission_open", {"queue_only": False}, auth_state())
add("_require_admission_open", {"queue_only": False}, auth_state(retiring=True))
add("_require_admission_open", {"queue_only": False},
    auth_state(runner={"_draining": True}))
add("_require_admission_open", {"queue_only": True},
    auth_state(runner={"_draining": True}), {"restart_wait_open": True})
add("_require_admission_open", {"queue_only": True},
    auth_state(runner={"_draining": True}), {"restart_wait_open": False})
add("_require_admission_open", {"queue_only": True},
    auth_state(retiring=True, runner={"_draining": True}), {"restart_wait_open": True})

# ══════════════ gate_refusal ══════════════
add("gate_refusal", {"check": False, "queue_only": False}, auth_state())
add("gate_refusal", {"check": True, "queue_only": False}, auth_state(), {"check": None})
add("gate_refusal", {"check": True, "queue_only": False}, auth_state(retiring=True),
    {"check": None})
add("gate_refusal", {"check": True, "queue_only": True},
    auth_state(runner={"_draining": True}), {"restart_wait_open": True, "check": None})
add("gate_refusal", {"check": True, "queue_only": False}, auth_state(),
    {"check": refusal("stale_generation", field="route",
                      detail="a /stop or /new of this chat came after this message")})
add("gate_refusal", {"check": True, "queue_only": False}, auth_state(),
    {"check": raise_spec("ValueError", "boom")})

# ══════════════ logical_owner ══════════════
add("logical_owner", {"session_id": ""}, auth_state())
add("logical_owner", {"session_id": None}, auth_state())
add("logical_owner", {"session_id": "s2"}, auth_state(), {"lineage.lineage": []})
add("logical_owner", {"session_id": "s2"}, auth_state(), {"lineage.lineage": ["root", "s2"]})
add("logical_owner", {"session_id": "s2"}, auth_state(), {"lineage.lineage": ["only"]})
add("logical_owner", {"session_id": "s2"}, auth_state(),
    {"lineage.lineage": raise_spec("OSError", "disk", errno=5)})

# ══════════════ physical_target ══════════════
add("physical_target", {"ref": ref("s2")}, auth_state(), {"lineage.tip": "tip1"})
add("physical_target", {"ref": ref("s2")}, auth_state(), {"lineage.tip": None})
add("physical_target", {"ref": ref("s2")}, auth_state(), {"lineage.tip": ""})
add("physical_target", {"ref": ref("")}, auth_state(), {"lineage.tip": None})
add("physical_target", {"ref": ref("s2")}, auth_state(),
    {"lineage.tip": raise_spec("RuntimeError", "gone")})

# ══════════════ _current_owner ══════════════
add("_current_owner", {"session_id": "s1"}, auth_state())
add("_current_owner", {"session_id": "s1"}, auth_state(rotated_owners={"s1": "k1"}),
    {"runner.session_store.lookup_by_session_key": None})
add("_current_owner", {"session_id": "s1"},
    auth_state(rotated_owners={"s1": "k1"}, sessions={"root": live()}),
    {"runner.session_store.lookup_by_session_key": {"session_id": "s9"},
     "lineage.lineage": ["root"]})
add("_current_owner", {"session_id": "s1"},
    auth_state(rotated_owners={"s1": "k1"}, sessions={"s1": live()}),
    {"runner.session_store.lookup_by_session_key": {"session_id": "s9"},
     "lineage.lineage": ["root"]})
add("_current_owner", {"session_id": "s1"},
    auth_state(rotated_owners={"s1": "k1"}, sessions={"s9": live()}),
    {"runner.session_store.lookup_by_session_key": {"session_id": "s9"},
     "lineage.lineage": []})
add("_current_owner", {"session_id": "s1"},
    auth_state(rotated_owners={"s1": "k1"}, sessions={"s1": live()}),
    {"runner.session_store.lookup_by_session_key": {"session_id": ""}})

# ══════════════ _receipt ══════════════
add("_receipt", {"row": row()}, auth_state())
add("_receipt", {"row": row(outcome="steering")}, auth_state())
add("_receipt", {"row": row(owner_epoch=0)}, auth_state())
add("_receipt", {"row": row(owner_epoch=None)}, auth_state())
add("_receipt", {"row": row(status="terminal", outcome="cancelled", generation=5)}, auth_state())
add("_receipt", {"row": row(seq=12, generation=None, target="s9")}, auth_state())

# ══════════════ _handle ══════════════
add("_handle", {"ref": ref()}, auth_state(), {"store.sessions.get": None})
add("_handle", {"ref": ref()}, auth_state(sessions={"s1": live()}),
    {"store.sessions.get": srow(), "store.admissions.list": []})
add("_handle", {"ref": ref()}, auth_state(sessions={"s1": live()}),
    {"store.sessions.get": srow(), "store.admissions.list": [row(status="started", generation=2)]})
add("_handle", {"ref": ref()}, auth_state(sessions={"s1": live()}),
    {"store.sessions.get": srow(),
     "store.admissions.list": [row(status="started", generation=2), row(aid="a2", seq=2, status="unknown")]})
add("_handle", {"ref": ref()},
    auth_state(sessions={"s1": live(settling={"a1": row(status="started", generation=2)})}),
    {"store.sessions.get": srow(),
     "store.admissions.list": [row(), row(aid="a2", seq=2)]})
add("_handle", {"ref": ref()}, auth_state(),
    {"store.sessions.get": srow(),
     "store.admissions.list": [row(aid="a2", seq=2), row(aid="a1", seq=1)]})

# ══════════════ authorize ══════════════
A_LOCAL = auth_state(sessions={"s1": live()})
add("authorize", {"actor": actor(profile_id="p2"), "ref": ref(), "capability": "session:read"},
    A_LOCAL, {"require_serving": None})
add("authorize", {"actor": actor(), "ref": ref(profile_id="p2"), "capability": "session:read"},
    A_LOCAL, {"require_serving": None})
add("authorize", {"actor": actor(), "ref": ref(), "capability": "session:control"},
    A_LOCAL, {"require_serving": None})
add("authorize", {"actor": actor(), "ref": ref(), "capability": "session:read"},
    A_LOCAL, {"require_serving": None, "is_history_mount": False, "same_owner": True,
              "touch_restored": None})
add("authorize", {"actor": actor(subject="u2"), "ref": ref(), "capability": "session:read"},
    A_LOCAL, {"require_serving": None, "is_history_mount": False, "same_owner": False})
add("authorize", {"actor": actor(subject="u2", caps=("session:read", "session:operator")),
                  "ref": ref(), "capability": "session:read"},
    A_LOCAL, {"require_serving": None, "is_history_mount": False, "same_owner": False,
              "touch_restored": None})
add("authorize", {"actor": actor(), "ref": ref(), "capability": "session:read"},
    auth_state(), {"require_serving": None, "store.sessions.get": None})
add("authorize", {"actor": actor(), "ref": ref(), "capability": "session:read"},
    auth_state(),
    {"require_serving": None, "store.sessions.get": srow(chat_id="local-x"),
     "store.bindings.binding_marks": {"policy": True, "legacy": False},
     "restore_local_session": script(None, live(route="k9", platform="telegram", user_id="u9", chat_id="c9")),
     "is_history_mount": False, "touch_restored": None})
add("authorize", {"actor": actor(), "ref": ref(), "capability": "session:read"},
    auth_state(),
    {"require_serving": None, "store.sessions.get": srow(chat_id="c1"),
     "store.bindings.binding_marks": {"policy": False, "legacy": False},
     "restore_routed_session": script(None),
     "mount_history": script(True, live(route="k9", platform="telegram", user_id="u9", chat_id="c9")),
     "is_history_mount": False, "touch_restored": None})
add("authorize", {"actor": actor(), "ref": ref(), "capability": "session:read"},
    auth_state(),
    {"require_serving": None, "store.sessions.get": srow(chat_id="c1"),
     "store.bindings.binding_marks": {"policy": False, "legacy": False},
     "restore_routed_session": script(None),
     "mount_history": script(False),
     "restore_api_session": script(None, live(route="k9", platform="api_server", user_id="u9", chat_id="c9")),
     "is_history_mount": False, "touch_restored": None})
add("authorize", {"actor": actor(), "ref": ref(), "capability": "session:read"},
    A_LOCAL,
    {"require_serving": None, "is_history_mount": True,
     "require_history_access": refusal("permission_denied", field="history", detail="read only")})
add("authorize", {"actor": actor(), "ref": ref(), "capability": "session:read"},
    auth_state(sessions={"s1": live(platform="telegram")}),
    {"require_serving": None, "is_history_mount": False, "touch_restored": None})
add("authorize", {"actor": actor(), "ref": ref(), "capability": "session:read"},
    auth_state(),
    {"require_serving": refusal("runtime_draining")})
add("authorize", {"actor": actor(), "ref": ref(), "capability": "session:read"},
    auth_state(),
    {"require_serving": None, "store.sessions.get": srow(chat_id=None),
     "store.bindings.binding_marks": {"policy": False, "legacy": True},
     "restore_local_session": script(None, live(route="k9", platform="telegram", user_id="u9", chat_id="c9")),
     "is_history_mount": False, "touch_restored": None})

# ══════════════ adopt_agent ══════════════
AG = {"id": "a1", "has_clear": True}
add("adopt_agent", {"session_id": "s1", "generation": 5, "agent": AG}, auth_state())
add("adopt_agent", {"session_id": "s1", "generation": 5, "agent": AG},
    auth_state(sessions={"s1": live()}, delivered_stops={"s1": 5}))
add("adopt_agent", {"session_id": "s1", "generation": 5, "agent": AG},
    auth_state(sessions={"s1": live()}, delivered_stops={"s1": 4}),
    {"agent.clear_interrupt": None})
add("adopt_agent", {"session_id": "s1", "generation": 5, "agent": {"id": "a1", "has_clear": False}},
    auth_state(sessions={"s1": live()}, delivered_stops={"s1": 4}))
add("adopt_agent", {"session_id": "s1", "generation": 5, "agent": AG},
    auth_state(sessions={"s1": live()}, pending_stops={"s1": 5}),
    {"request_hard_interrupt": None})
add("adopt_agent", {"session_id": "s1", "generation": 5, "agent": AG},
    auth_state(sessions={"s1": live()}, pending_stops={"s1": 4}))
add("adopt_agent", {"session_id": "s1", "generation": 5, "agent": AG},
    auth_state(sessions={"s1": live()}, pending_steers={"s1": [5, ["t1", "t2"]]}),
    {"agent.steer": None})
add("adopt_agent", {"session_id": "s1", "generation": 5, "agent": AG},
    auth_state(sessions={"s1": live()}, pending_steers={"s1": [4, ["t1"]]}))
add("adopt_agent", {"session_id": "s1", "generation": 5, "agent": AG},
    auth_state(sessions={"s1": live()}))
add("adopt_agent", {"session_id": "s1", "generation": 5, "agent": None},
    auth_state(sessions={"s1": live()}, pending_steers={"s1": [5, ["t1"]]}))

# ══════════════ publish_execution ══════════════
PE_ARGS = {"session_id": "s1", "generation": 5, "event_type": "message.delta",
           "payload": {"text": "x"}}
add("publish_execution", PE_ARGS, auth_state())
add("publish_execution", PE_ARGS, auth_state(sessions={"s1": live()}, executing={"s1": 4}))
add("publish_execution", PE_ARGS, auth_state(sessions={"s1": live()}))
add("publish_execution", PE_ARGS, auth_state(sessions={"s1": live()}, executing={"s1": 5}),
    {"event_stream.note_progress": None, "event_stream.publish": True, "publish_api_event": False})
add("publish_execution", PE_ARGS, auth_state(sessions={"s1": live()}, executing={"s1": 5}),
    {"event_stream.note_progress": None, "event_stream.publish": False, "publish_api_event": True})
add("publish_execution", PE_ARGS, auth_state(sessions={"s1": live()}, executing={"s1": 5}),
    {"event_stream.note_progress": None, "event_stream.publish": False, "publish_api_event": False})
add("publish_execution", PE_ARGS, auth_state(sessions={"s1": live()}, executing={"s1": 5}),
    {"event_stream.note_progress": None, "event_stream.publish": None, "publish_api_event": None})
# ══════════════ run_inbound helpers ══════════════
def ri_state(**over):
    base = {"config": None, "session_states": {}, "_startup_restore_in_progress": False,
            "executing_admission": None}
    base.update(over)
    return base


def src(platform="telegram", user_id="u1", chat_id="c1", chat_type="dm", is_bot=False,
        profile=None, rejected=False):
    return {"platform": platform, "user_id": user_id, "chat_id": chat_id,
            "chat_type": chat_type, "is_bot": is_bot, "profile": profile,
            "profile_route_rejected": rejected}


def ev(source=None, text="hello", **flags):
    d = {"source": source if source is not None else src(), "text": text}
    d.update(flags)
    return d


def kstate(pending=False):
    return {"k1": {"persistent": {"update_prompt_pending": pending}}}


NOTE = ("[Triggering message id: `{mid}` \u2014 use as `message_id` for reply/react/pin "
        "via the discord tools.]")

# ══════════════ discord_triggering_note ══════════════
add("discord_triggering_note", {"message_id": 123})
add("discord_triggering_note", {"message_id": "m1"})
add("discord_triggering_note", {"message_id": ""})
add("discord_triggering_note", {"message_id": "m\u00e9ssage-\U0001f389"})
add("discord_triggering_note", {"message_id": 42})

# ══════════════ strip_discord_triggering_note ══════════════
PFX = NOTE.format(mid="m1") + "\n\n"
add("strip_discord_triggering_note", {"event": {"message_id": "m1"}, "message_text": PFX + "body"})
add("strip_discord_triggering_note", {"event": {"message_id": "m1"}, "message_text": "plain body"})
add("strip_discord_triggering_note", {"event": {"message_id": None}, "message_text": PFX + "body"})
add("strip_discord_triggering_note", {"event": {"message_id": 0}, "message_text": PFX + "body"})
add("strip_discord_triggering_note", {"event": {"message_id": "m1"}, "message_text": {"a": 1}})
add("strip_discord_triggering_note", {"event": {"message_id": "m1"}, "message_text": PFX})
add("strip_discord_triggering_note", {"event": {"message_id": "m1"},
                                      "message_text": NOTE.format(mid="m1") + "body"})

# ══════════════ _hm_estop_turn_allowed ══════════════
EA_ARGS = {"event": ev(), "source": src()}
add("_hm_estop_turn_allowed", EA_ARGS, ri_state(),
    {"event.get_command": "stop", "resolve_command": {"name": "stop"}})
add("_hm_estop_turn_allowed", EA_ARGS, ri_state(),
    {"event.get_command": "bogus", "resolve_command": None,
     "self._session_key_for_source": "k1", "self._is_session_running": True})
add("_hm_estop_turn_allowed", EA_ARGS, ri_state(session_states=kstate(True)),
    {"event.get_command": None, "self._session_key_for_source": "k1",
     "self._hm_update_reply_refused": False})
add("_hm_estop_turn_allowed", EA_ARGS, ri_state(session_states=kstate(True)),
    {"event.get_command": None, "self._session_key_for_source": "k1",
     "self._hm_update_reply_refused": True, "self._is_session_running": False,
     "slash_confirm.get_pending": {"confirm_id": "c1"}})
add("_hm_estop_turn_allowed", EA_ARGS, ri_state(),
    {"event.get_command": None, "self._session_key_for_source": "k1",
     "self._is_session_running": False, "slash_confirm.get_pending": None,
     "approval.has_blocking_approval": True})
add("_hm_estop_turn_allowed", EA_ARGS, ri_state(),
    {"event.get_command": None, "self._session_key_for_source": "k1",
     "self._is_session_running": False, "slash_confirm.get_pending": None,
     "approval.has_blocking_approval": False})
add("_hm_estop_turn_allowed", EA_ARGS, ri_state(),
    {"event.get_command": raise_spec("RuntimeError", "no command"),
     "self._session_key_for_source": "k1", "self._is_session_running": True})
add("_hm_estop_turn_allowed", EA_ARGS, ri_state(),
    {"event.get_command": None,
     "self._session_key_for_source": raise_spec("RuntimeError", "no key")})

# ══════════════ _hm_estop_gate ══════════════
EG_ARGS = {"event": ev(), "source": src(), "is_internal": False}
add("_hm_estop_gate", dict(EG_ARGS, is_internal=True), ri_state())
add("_hm_estop_gate", EG_ARGS, ri_state(),
    {"agent.estop.paused_reply": raise_spec("ImportError", "no module")})
add("_hm_estop_gate", EG_ARGS, ri_state(), {"agent.estop.paused_reply": None})
add("_hm_estop_gate", EG_ARGS, ri_state(),
    {"agent.estop.paused_reply": "Paused.", "event.get_command": "stop",
     "resolve_command": {"name": "stop"}})
add("_hm_estop_gate", dict(EG_ARGS, event=ev(_heartbeat_session_id="h1")), ri_state(),
    {"agent.estop.paused_reply": "Paused.", "event.get_command": None,
     "self._session_key_for_source": "k1", "self._is_session_running": False,
     "slash_confirm.get_pending": None, "approval.has_blocking_approval": False})
add("_hm_estop_gate", EG_ARGS, ri_state(),
    {"agent.estop.paused_reply": "Paused.", "event.get_command": None,
     "self._session_key_for_source": "k1", "self._is_session_running": False,
     "slash_confirm.get_pending": None, "approval.has_blocking_approval": False})
add("_hm_estop_gate", EG_ARGS, ri_state(),
    {"agent.estop.paused_reply": raise_spec("ValueError", "bad")})

# ══════════════ _hm_update_prompt_reply ══════════════
UP = {"event": ev(), "key": "k1"}
add("_hm_update_prompt_reply", UP, ri_state())
add("_hm_update_prompt_reply", UP, ri_state(session_states=kstate(False)))
add("_hm_update_prompt_reply", UP, ri_state(session_states=kstate(True)),
    {"self._hm_update_reply_refused": True})
add("_hm_update_prompt_reply", UP, ri_state(session_states=kstate(True)),
    {"self._hm_update_reply_refused": False, "event.get_command": "approve",
     "self._hm_write_update_response": None, "t": "Sent: y"})
add("_hm_update_prompt_reply", UP, ri_state(session_states=kstate(True)),
    {"self._hm_update_reply_refused": False, "event.get_command": "no",
     "self._hm_write_update_response": None, "t": "Sent: n"})
add("_hm_update_prompt_reply", dict(UP, event=ev(text="  yes please  ")),
    ri_state(session_states=kstate(True)),
    {"self._hm_update_reply_refused": False, "event.get_command": None,
     "self._hm_write_update_response": None, "t": "Sent: yes please"})
add("_hm_update_prompt_reply", dict(UP, event=ev(text="abcdefghijklmnopqrstuvwxy")),
    ri_state(session_states=kstate(True)),
    {"self._hm_update_reply_refused": False, "event.get_command": None,
     "self._hm_write_update_response": None, "t": "Sent."})
add("_hm_update_prompt_reply", UP, ri_state(session_states=kstate(True)),
    {"self._hm_update_reply_refused": False, "event.get_command": "yes",
     "self._hm_write_update_response": "disk full", "t": "Failed: disk full"})
add("_hm_update_prompt_reply", UP, ri_state(session_states=kstate(True)),
    {"self._hm_update_reply_refused": False, "event.get_command": "new",
     "resolve_command": {"name": "new"}, "self._hm_write_update_response": None})
add("_hm_update_prompt_reply", UP, ri_state(session_states=kstate(True)),
    {"self._hm_update_reply_refused": False, "event.get_command": "bogus",
     "resolve_command": None})
add("_hm_update_prompt_reply",
    dict(UP, event=ev(text="\u03b1\u03b2\u03b3\u03b4\u03b5\u03b6\u03b7\u03b8\u03b9\u03ba\u03bb\u03bc\u03bd\u03be\u03bf\u03c0\u03c1\u03c3\u03c4\u03c5\u03c6\u03c7\u03c8\u03c9\u03b1\u03b2\u03b3")),
    ri_state(session_states=kstate(True)),
    {"self._hm_update_reply_refused": False, "event.get_command": None,
     "self._hm_write_update_response": None, "t": "Sent."})
add("_hm_update_prompt_reply", UP, ri_state(session_states=kstate(True)),
    {"self._hm_update_reply_refused": False, "event.get_command": "new",
     "resolve_command": {"name": "new"}, "self._hm_write_update_response": "err"})

# ══════════════ _hm_slash_confirm_reply ══════════════
SC = {"event": ev(), "key": "k1"}
WORDS = [["once", "y"], ["always", "remember"], ["cancel", "no"]]
add("_hm_slash_confirm_reply", SC, ri_state(), {"slash_confirm.get_pending": None})
add("_hm_slash_confirm_reply", SC, ri_state(), {"slash_confirm.get_pending": {}})
add("_hm_slash_confirm_reply", SC, ri_state(),
    {"slash_confirm.get_pending": {"confirm_id": "c1"}, "approval.has_blocking_approval": True})
add("_hm_slash_confirm_reply", SC, ri_state(),
    {"slash_confirm.get_pending": {"confirm_id": "c1"}, "approval.has_blocking_approval": False,
     "event.get_command": "approve", "self._refused_slash_confirm": None,
     "slash_confirm.resolve": "Approved."})
add("_hm_slash_confirm_reply", dict(SC, event=ev(text=" !Always ")), ri_state(),
    {"slash_confirm.get_pending": {"confirm_id": "c1"}, "approval.has_blocking_approval": False,
     "event.get_command": None, "approval_input_words": WORDS,
     "self._refused_slash_confirm": None, "slash_confirm.resolve": None})
add("_hm_slash_confirm_reply", SC, ri_state(),
    {"slash_confirm.get_pending": {"confirm_id": "c1"}, "approval.has_blocking_approval": False,
     "event.get_command": "approve", "self._refused_slash_confirm": "Not yours."})
add("_hm_slash_confirm_reply", dict(SC, event=ev(text="hello")), ri_state(),
    {"slash_confirm.get_pending": {"confirm_id": "c1"}, "approval.has_blocking_approval": False,
     "event.get_command": "bogus", "approval_input_words": WORDS,
     "slash_confirm.clear_if_stale": None})
add("_hm_slash_confirm_reply", SC, ri_state(),
    {"slash_confirm.get_pending": {"confirm_id": "c1"},
     "approval.has_blocking_approval": raise_spec("RuntimeError", "boom"),
     "event.get_command": "approve", "self._refused_slash_confirm": None,
     "slash_confirm.resolve": "ok"})
add("_hm_slash_confirm_reply", dict(SC, event=ev(text="always")), ri_state(),
    {"slash_confirm.get_pending": {"confirm_id": "c1"}, "approval.has_blocking_approval": False,
     "event.get_command": None, "approval_input_words": [["always"], ["always"], []],
     "self._refused_slash_confirm": None, "slash_confirm.resolve": "ok"})

# ══════════════ _hm_busy_slash_or_photo ══════════════
BS = {"event": ev(), "source": src(), "key": "k1"}
add("_hm_busy_slash_or_photo", BS, ri_state(),
    {"event.get_command": "status", "resolve_command": {"name": "status"},
     "self._handle_status_command": "status reply"})
add("_hm_busy_slash_or_photo", BS, ri_state(),
    {"event.get_command": "context", "resolve_command": {"name": "context"},
     "self._handle_context_command": "context reply"})
add("_hm_busy_slash_or_photo", BS, ri_state(),
    {"event.get_command": "model", "resolve_command": {"name": "model"},
     "self._check_slash_access": "No access."})
add("_hm_busy_slash_or_photo", BS, ri_state(),
    {"event.get_command": "stop", "resolve_command": {"name": "stop"},
     "self._check_slash_access": None, "self._hm_drop_held_input": None,
     "self._hm_stop_owner_turn": None, "self._dispatch_busy_slash_command": "stopped"})
add("_hm_busy_slash_or_photo", BS, ri_state(),
    {"event.get_command": "new", "resolve_command": {"name": "new"},
     "self._check_slash_access": None, "self._hm_drop_held_input": None,
     "self._hm_stop_owner_turn": None, "self._dispatch_busy_slash_command": "reset"})
add("_hm_busy_slash_or_photo", BS, ri_state(),
    {"event.get_command": "foo", "resolve_command": {"name": "foo"},
     "self._check_slash_access": None, "self._dispatch_busy_slash_command": "ok"})
add("_hm_busy_slash_or_photo", dict(BS, event=ev(message_type="photo")), ri_state(),
    {"event.get_command": None, "self._hm_merge_pending_for_source": None})
add("_hm_busy_slash_or_photo", dict(BS, event=ev(message_type="text")), ri_state(),
    {"event.get_command": None})
add("_hm_busy_slash_or_photo", BS, ri_state(),
    {"event.get_command": "model", "resolve_command": {"name": "model"},
     "self._check_slash_access": ""})

# ══════════════ _hm_admit_event ══════════════
def admit_case(event, state=None, io=None):
    add("_hm_admit_event", {"event": event}, state or ri_state(), io)


admit_case(ev(internal=True), io={"reset_session_vars": None})
admit_case(ev(source=src(rejected=True)),
           io={"reset_session_vars": None, "self._intake_adapter_for": "ad1",
               "drop_rejected_route": None})
admit_case(ev(source=src(rejected=True)),
           io={"reset_session_vars": None,
               "self._intake_adapter_for": raise_spec("RuntimeError", "no adapter"),
               "drop_rejected_route": None})
admit_case(ev(source=src(platform="slack")),
           io={"reset_session_vars": None, "self._intake_adapter_for": "ad1",
               "_is_slack_ignored_channel": True})
SLACK_EV = ev(source=src(platform="slack"), _bot_loop_admitted=True)
admit_case(SLACK_EV,
           io={"reset_session_vars": None, "self._intake_adapter_for": "ad1",
               "_is_slack_ignored_channel": False, "self._scale_to_zero_note_real_inbound": None,
               "self._hm_pre_gateway_dispatch_hook": SLACK_EV,
               "self._is_user_authorized_for_source": True})
admit_case(ev(), ri_state(_startup_restore_in_progress=True),
           {"reset_session_vars": None, "self._queue_startup_restore_event": None})
admit_case(ev(_bot_loop_admitted=True),
           ri_state(_startup_restore_in_progress=True, executing_admission={"admission_id": "a1"}),
           {"reset_session_vars": None, "self._scale_to_zero_note_real_inbound": None,
            "self._hm_pre_gateway_dispatch_hook": ev(_bot_loop_admitted=True),
            "self._is_user_authorized_for_source": True})
admit_case(ev(_fifo_only=True), io={"reset_session_vars": None})
admit_case(ev(), io={"reset_session_vars": None, "self._scale_to_zero_note_real_inbound": None,
                     "self._hm_pre_gateway_dispatch_hook": None})
NEW_EV = ev(source=src(user_id="u2"), text="rewritten", _bot_loop_admitted=True)
admit_case(ev(), io={"reset_session_vars": None, "self._scale_to_zero_note_real_inbound": None,
                     "self._hm_pre_gateway_dispatch_hook": NEW_EV,
                     "self._is_user_authorized_for_source": True})
admit_case(ev(source=src(user_id=None)),
           io={"reset_session_vars": None, "self._scale_to_zero_note_real_inbound": None,
               "self._hm_pre_gateway_dispatch_hook": ev(source=src(user_id=None)),
               "self._is_user_authorized_for_source": False})
admit_case(ev(), io={"reset_session_vars": None, "self._scale_to_zero_note_real_inbound": None,
                     "self._hm_pre_gateway_dispatch_hook": ev(),
                     "self._is_user_authorized_for_source": False,
                     "self._get_unauthorized_dm_behavior": "pair",
                     "self._hm_offer_pairing_code": None})
admit_case(ev(), io={"reset_session_vars": None, "self._scale_to_zero_note_real_inbound": None,
                     "self._hm_pre_gateway_dispatch_hook": ev(),
                     "self._is_user_authorized_for_source": False,
                     "self._get_unauthorized_dm_behavior": "decline",
                     "self._hm_send_unauthorized_decline": None})
admit_case(ev(), io={"reset_session_vars": None, "self._scale_to_zero_note_real_inbound": None,
                     "self._hm_pre_gateway_dispatch_hook": ev(),
                     "self._is_user_authorized_for_source": False,
                     "self._get_unauthorized_dm_behavior": "ignore",
                     "self._hm_report_ignored_dm": None})
admit_case(ev(source=src(chat_type="group")),
           io={"reset_session_vars": None, "self._scale_to_zero_note_real_inbound": None,
               "self._hm_pre_gateway_dispatch_hook": ev(source=src(chat_type="group")),
               "self._is_user_authorized_for_source": False})
admit_case(ev(), io={"reset_session_vars": None, "self._scale_to_zero_note_real_inbound": None,
                     "self._hm_pre_gateway_dispatch_hook": ev(),
                     "self._is_user_authorized_for_source": True,
                     "self._admit_bot_message_for_source": False})
admit_case(ev(), io={"reset_session_vars": None, "self._scale_to_zero_note_real_inbound": None,
                     "self._hm_pre_gateway_dispatch_hook": ev(),
                     "self._is_user_authorized_for_source": True,
                     "self._admit_bot_message_for_source": True,
                     "self._admit_bot_chain": True})
admit_case(ev(_bot_loop_admitted=True), ri_state(config={"multiplex_profiles": True}),
           {"reset_session_vars": None, "self._canonicalize": None,
            "self._scale_to_zero_note_real_inbound": None,
            "self._hm_pre_gateway_dispatch_hook": ev(_bot_loop_admitted=True),
            "self._is_user_authorized_for_source": True})

# ══════════════ _hm_send_unauthorized_decline ══════════════
D_ARGS = {"source": src()}
add("_hm_send_unauthorized_decline", D_ARGS, ri_state(), {"self._pairing_store_for": None})
add("_hm_send_unauthorized_decline", D_ARGS, ri_state(),
    {"self._pairing_store_for": "ps1", "pairing_store.has_recent_decline": True})
add("_hm_send_unauthorized_decline", D_ARGS,
    ri_state(config={"unauthorized_dm_decline_message": "Custom bye."}),
    {"self._pairing_store_for": "ps1", "pairing_store.has_recent_decline": False,
     "pairing_store.record_decline": None, "self._delivery_adapter_for": "ad1",
     "adapter.send": None})
add("_hm_send_unauthorized_decline", D_ARGS, ri_state(),
    {"self._pairing_store_for": "ps1", "pairing_store.has_recent_decline": False,
     "pairing_store.record_decline": None, "self._delivery_adapter_for": "ad1",
     "adapter.send": None})
add("_hm_send_unauthorized_decline", D_ARGS, ri_state(),
    {"self._pairing_store_for": "ps1", "pairing_store.has_recent_decline": False,
     "pairing_store.record_decline": raise_spec("OSError", "locked", errno=110)})
add("_hm_send_unauthorized_decline", D_ARGS, ri_state(),
    {"self._pairing_store_for": "ps1", "pairing_store.has_recent_decline": False,
     "pairing_store.record_decline": raise_spec("TimeoutError", "timed out")})
add("_hm_send_unauthorized_decline", D_ARGS, ri_state(),
    {"self._pairing_store_for": "ps1", "pairing_store.has_recent_decline": False,
     "pairing_store.record_decline": raise_spec("ValueError", "bad")})
add("_hm_send_unauthorized_decline", D_ARGS, ri_state(),
    {"self._pairing_store_for": "ps1", "pairing_store.has_recent_decline": False,
     "pairing_store.record_decline": None, "self._delivery_adapter_for": None})
add("_hm_send_unauthorized_decline", D_ARGS, ri_state(),
    {"self._pairing_store_for": "ps1", "pairing_store.has_recent_decline": False,
     "pairing_store.record_decline": None, "self._delivery_adapter_for": "ad1",
     "adapter.send": raise_spec("RuntimeError", "net")})
add("_hm_send_unauthorized_decline", {"source": src(platform=None)}, ri_state(),
    {"self._pairing_store_for": "ps1", "pairing_store.has_recent_decline": False,
     "pairing_store.record_decline": None, "self._delivery_adapter_for": "ad1",
     "adapter.send": None})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False, separators=(",", ":")) + "\n")
    counts = {}
    for case in CASES:
        counts[case["func"]] = counts.get(case["func"], 0) + 1
    print(f"wrote {OUT}: {len(CASES)} cases, {len(counts)} funcs")
    for fn, n in sorted(counts.items()):
        flag = "" if n >= 5 else "  (<5!)"
        print(f"  {fn}: {n}{flag}")


if __name__ == "__main__":
    main()
