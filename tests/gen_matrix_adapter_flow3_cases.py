#!/usr/bin/env python3
"""Generate tests/fixtures/matrix_adapter_flow3.jsonl -- input cases for the
Matrix-adapter sync/event + reaction flow oracle (the oracle computes want from
LIVE adapter.py at gate time; this script only emits {func, in} inputs).

Deterministic handcrafted branch-coverage cases (>=6 per function). Each flow
carries normal, branch, scripted-Exception-swallow, BaseException-passthrough
and fail-closed (fc=True) variants. Domains: prompt/registry entries are
{"__attr__": {...}}; `on_selected` is a string marker; choices/offered/
bot_reaction_events are plain dicts; expires_at is an int or null; registries
and config live in args.self (they are NOT echoed); state carries only
_warned_encrypted_drop_rooms / _pending_reactions.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "matrix_adapter_flow3.jsonl"
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


def ad(attrs):
    return {"__attr__": attrs}


def rs(kind, msg=""):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def sc(v):
    return {"__script__": True, "ret": v}


def prompt(chat="!r:s", resolved=False, expires=None, requester=None,
           on_selected="sel", session="sk", request="req", offered=None,
           choices=None, bot=None):
    return ad({
        "chat_id": chat, "resolved": resolved, "expires_at": expires,
        "requester_user_id": requester, "on_selected": on_selected,
        "session_key": session, "request_id": request,
        "offered": offered if offered is not None else {},
        "choices": choices if choices is not None else {"yes": "yes", "no": "no"},
        "bot_reaction_events": bot if bot is not None else {},
    })


def reg(key, reacts_to, p):
    return {key: {reacts_to: p}}


def event(sender="@u:s", event_id="$e", room_id="!r:s", content=None,
          message_id="m1", chat_id="!r:s"):
    return {
        "sender": sender, "event_id": event_id, "room_id": room_id,
        "content": content if content is not None else {"m.relates_to": {
            "event_id": "$target", "key": "yes"}},
        "message_id": message_id,
        "source": {"chat_id": chat_id},
    }


APPR = "_approval_prompts_by_event"
MODP = "_model_picker_prompts_by_event"
CHOP = "_choice_picker_prompts_by_event"
APPS = "_approval_prompt_by_session"

# ── _warn_encrypted_drops (sync, zero seams) ──
def enc_room(*encrypted):
    return {"timeline": {"events": [{"type": t} for t in encrypted]}}

WSELF = {"_e2ee_mode": "off"}
add("_warn_encrypted_drops", {"rooms_join": {}, "client": ad({"crypto": {"device": 1}}),
                              "self": WSELF}, state={"_warned_encrypted_drop_rooms": []})
add("_warn_encrypted_drops", {"rooms_join": {}, "client": ad({"crypto": None}),
                              "self": WSELF}, state={"_warned_encrypted_drop_rooms": []})
add("_warn_encrypted_drops", {"rooms_join": {"!a:s": enc_room("m.room.encrypted")},
                              "client": ad({"crypto": None}), "self": WSELF},
    state={"_warned_encrypted_drop_rooms": []})
add("_warn_encrypted_drops", {"rooms_join": {"!a:s": enc_room("m.room.encrypted")},
                              "client": ad({"crypto": None}), "self": {"_e2ee_mode": "optional"}},
    state={"_warned_encrypted_drop_rooms": []})
add("_warn_encrypted_drops", {"rooms_join": {"!a:s": enc_room("m.room.encrypted")},
                              "client": ad({"crypto": None}), "self": WSELF},
    state={"_warned_encrypted_drop_rooms": ["!a:s"]})
add("_warn_encrypted_drops", {"rooms_join": {"!a:s": enc_room("m.room.message")},
                              "client": ad({"crypto": None}), "self": WSELF},
    state={"_warned_encrypted_drop_rooms": []})
add("_warn_encrypted_drops", {"rooms_join": {"!a:s": {}}, "client": ad({"crypto": None}),
                              "self": WSELF}, state={"_warned_encrypted_drop_rooms": []})
add("_warn_encrypted_drops", {"rooms_join": {"!a:s": enc_room("m.room.encrypted"),
                                              "!b:s": enc_room("m.room.encrypted")},
                              "client": ad({"crypto": None}), "self": WSELF},
    state={"_warned_encrypted_drop_rooms": ["!z:s"]})

# ── _is_allowed_matrix_room_event (async) ──
add("_is_allowed_matrix_room_event", {"room_id": "!r:s", "self": {"_allowed_room_ids": None}})
add("_is_allowed_matrix_room_event", {"room_id": "!r:s", "self": {"_allowed_room_ids": []}})
add("_is_allowed_matrix_room_event", {"room_id": "!r:s", "self": {"_allowed_room_ids": ["!r:s"]}})
add("_is_allowed_matrix_room_event", {"room_id": "!r:s", "self": {"_allowed_room_ids": ["!x:s"]}},
    {"self._is_dm_room": True})
add("_is_allowed_matrix_room_event", {"room_id": "!r:s", "self": {"_allowed_room_ids": ["!x:s"]}},
    {"self._is_dm_room": False})
add("_is_allowed_matrix_room_event", {"room_id": "!r:s", "self": {"_allowed_room_ids": ["!x:s"]}},
    {"self._is_dm_room": rs("RuntimeError", "boom")})
add("_is_allowed_matrix_room_event", {"room_id": "!r:s", "self": {"_allowed_room_ids": ["!x:s"]}},
    {"self._is_dm_room": rs("SystemExit", "bye")})
add("_is_allowed_matrix_room_event", {"room_id": "!r:s", "self": {"_allowed_room_ids": ["!x:s"]}}, fc=True)

# ── _redact_reaction (async) ──
for v in [True, False, None, "ok", 0]:
    add("_redact_reaction", {"room_id": "!r:s", "reaction_event_id": "$rr", "reason": "why"},
        {"self.redact_message": v})
add("_redact_reaction", {"room_id": "!r:s", "reaction_event_id": "$rr"},
    {"self.redact_message": rs("RuntimeError", "nope")})
add("_redact_reaction", {"room_id": "!r:s", "reaction_event_id": "$rr", "reason": "x"},
    {"self.redact_message": rs("SystemExit", "bye")})
add("_redact_reaction", {"room_id": "!r:s", "reaction_event_id": "$rr"}, fc=True)

# ── on_processing_start (async) ──
add("on_processing_start", {"event": event(), "self": {"_reactions_enabled": False}})
add("on_processing_start", {"event": event(message_id=""), "self": {"_reactions_enabled": True}})
add("on_processing_start", {"event": event(chat_id=""), "self": {"_reactions_enabled": True}})
add("on_processing_start", {"event": event(), "self": {"_reactions_enabled": True}},
    {"self._send_reaction": "$eyes"}, state={"_pending_reactions": {}})
add("on_processing_start", {"event": event(), "self": {"_reactions_enabled": True}},
    {"self._send_reaction": None}, state={"_pending_reactions": {}})
add("on_processing_start", {"event": event(), "self": {"_reactions_enabled": True}},
    {"self._send_reaction": rs("RuntimeError", "send")}, state={"_pending_reactions": {}})
add("on_processing_start", {"event": event(), "self": {"_reactions_enabled": True}},
    {"self._send_reaction": rs("SystemExit", "bye")}, state={"_pending_reactions": {}})
add("on_processing_start", {"event": event(), "self": {"_reactions_enabled": True}},
    state={"_pending_reactions": {}}, fc=True)

# ── on_processing_complete (async) ──
PC = {"self": {"_reactions_enabled": True}}
add("on_processing_complete", {"event": event(), "outcome": "success",
                               "self": {"_reactions_enabled": False}})
add("on_processing_complete", {"event": event(), "outcome": "cancelled", "self": PC["self"]})
add("on_processing_complete", {"event": event(), "outcome": "success", "self": PC["self"]},
    {"self._send_reaction": "$tick"}, state={"_pending_reactions": {}})
add("on_processing_complete", {"event": event(), "outcome": "failed", "self": PC["self"]},
    {"self._schedule_reaction_redaction": None, "self._send_reaction": "$tick"},
    state={"_pending_reactions": {"('!r:s', 'm1')": "$eyes"}})
add("on_processing_complete", {"event": event(), "outcome": "success", "self": PC["self"]},
    {"self._schedule_reaction_redaction": None, "self._send_reaction": "$tick"},
    state={"_pending_reactions": {"('!r:s', 'm1')": "$eyes"}})
add("on_processing_complete", {"event": event(), "outcome": "success", "self": PC["self"]},
    {"self._schedule_reaction_redaction": rs("RuntimeError", "sched"),
     "self._send_reaction": "$tick"},
    state={"_pending_reactions": {"('!r:s', 'm1')": "$eyes"}})
add("on_processing_complete", {"event": event(), "outcome": "failed", "self": PC["self"]},
    {"self._send_reaction": rs("SystemExit", "bye")}, state={"_pending_reactions": {}})
add("on_processing_complete", {"event": event(), "outcome": "success", "self": PC["self"]},
    state={"_pending_reactions": {}}, fc=True)

# ── _on_reaction (async) ──
add("_on_reaction", {"event": event()}, {"self._is_self_sender": True})
add("_on_reaction", {"event": event()},
    {"self._is_self_sender": False, "self._is_duplicate_event": True})
add("_on_reaction", {"event": event(content={})},
    {"self._is_self_sender": False, "self._is_duplicate_event": False})
add("_on_reaction", {"event": event()},
    {"self._is_self_sender": False, "self._is_duplicate_event": False,
     "hash_for_log": "h", "self._handle_approval_reaction": True})
add("_on_reaction", {"event": event()},
    {"self._is_self_sender": False, "self._is_duplicate_event": False,
     "hash_for_log": "h", "self._handle_approval_reaction": False,
     "self._handle_model_picker_reaction": True})
add("_on_reaction", {"event": event()},
    {"self._is_self_sender": False, "self._is_duplicate_event": False,
     "hash_for_log": "h", "self._handle_approval_reaction": False,
     "self._handle_model_picker_reaction": False,
     "self._handle_choice_picker_reaction": True})
add("_on_reaction", {"event": event()},
    {"self._is_self_sender": False, "self._is_duplicate_event": False,
     "hash_for_log": "h", "self._handle_approval_reaction": rs("RuntimeError", "h")})
add("_on_reaction", {"event": event()}, {"self._is_self_sender": rs("SystemExit", "bye"),
                                          "hash_for_log": "h"})
add("_on_reaction", {"event": event()}, fc=True)

# ── _claim_reaction_prompt (async) ──
CLAIM = {"room_id": "!r:s", "reacts_to": "$target", "key": "yes", "sender": "@u:s",
         "label": "approval", "invalid_text": "bad"}
add("_claim_reaction_prompt", dict(CLAIM, self=reg(APPR, "$target", prompt())),
    {"self._matrix_prompt_expired": False, "self._validate_matrix_prompt_reactor": True})
add("_claim_reaction_prompt", dict(CLAIM, self=reg(APPR, "$target", prompt(resolved=True))), {})
add("_claim_reaction_prompt", dict(CLAIM, room_id="!other:s", self=reg(APPR, "$target", prompt())))
add("_claim_reaction_prompt", dict(CLAIM, self=reg(APPR, "$target", prompt(expires=10))),
    {"self._matrix_prompt_expired": True, "on_expired": None})
add("_claim_reaction_prompt", dict(CLAIM, self=reg(APPR, "$target", prompt())),
    {"self._matrix_prompt_expired": False, "self._validate_matrix_prompt_reactor": False})
add("_claim_reaction_prompt", dict(CLAIM, self=reg(APPR, "$target", prompt())),
    {"self._matrix_prompt_expired": False, "self._validate_matrix_prompt_reactor": True})
add("_claim_reaction_prompt", dict(CLAIM, key="nope", self=reg(APPR, "$target", prompt())),
    {"self._matrix_prompt_expired": False, "self._validate_matrix_prompt_reactor": True,
     "self._send_invalid_reaction_feedback": None})
add("_claim_reaction_prompt", dict(CLAIM, self=reg(APPR, "$target", prompt())),
    {"self._matrix_prompt_expired": rs("SystemExit", "bye")})
add("_claim_reaction_prompt", dict(CLAIM, self=reg(APPR, "$target", prompt())),
    {"self._matrix_prompt_expired": True, "on_expired": rs("RuntimeError", "exp")})
add("_claim_reaction_prompt", dict(CLAIM, self=reg(APPR, "$target", prompt())), fc=True)

# ── _handle_approval_reaction (async) ──
HA = {"room_id": "!r:s", "reacts_to": "$target", "key": "yes", "sender": "@u:s",
      "self": dict(reg(APPR, "$target", prompt()),
                   **{APPS: {"sk": prompt()},
                      "_approval_reaction_map": {"yes": "yes", "no": "no"},
                      "config": ad({"extra": {"k": 1}})}),
      "io": {"t": "invalid"}}
CLAIM_F = {"__script__": True, "ret": [False, None, None]}
CLAIM_T = {"__script__": True, "ret": [True, prompt(), None]}
CLAIM_CH = {"__script__": True, "ret": [True, prompt(), "yes"]}

add("_handle_approval_reaction", {k: v for k, v in HA.items() if k != "io"},
    {"t": "invalid", "self._claim_reaction_prompt": CLAIM_F})
add("_handle_approval_reaction", {k: v for k, v in HA.items() if k != "io"},
    {"t": "invalid", "self._claim_reaction_prompt": CLAIM_T})
add("_handle_approval_reaction", {k: v for k, v in HA.items() if k != "io"},
    {"t": "invalid", "self._claim_reaction_prompt": {
        "__script__": True, "ret": [True, prompt(offered={"no": 1}), "yes"]},
     "self._send_invalid_reaction_feedback": None})
add("_handle_approval_reaction", {k: v for k, v in HA.items() if k != "io"},
    {"t": "invalid", "self._claim_reaction_prompt": CLAIM_CH,
     "approval_refusal": "@owner:s", "refusal_text": "not you",
     "self._send_invalid_reaction_feedback": None})
add("_handle_approval_reaction", {k: v for k, v in HA.items() if k != "io"},
    {"t": "invalid", "self._claim_reaction_prompt": CLAIM_CH,
     "approval_refusal": None, "resolve_gateway_approval_card": 0,
     "self._expire_matrix_approval_prompt": None})
add("_handle_approval_reaction", {k: v for k, v in HA.items() if k != "io"},
    {"t": "invalid", "self._claim_reaction_prompt": CLAIM_CH,
     "approval_refusal": None, "resolve_gateway_approval_card": 2,
     "self._redact_bot_approval_reactions": None})
add("_handle_approval_reaction", {k: v for k, v in HA.items() if k != "io"},
    {"t": "invalid", "self._claim_reaction_prompt": CLAIM_CH,
     "approval_refusal": None, "resolve_gateway_approval_card": rs("RuntimeError", "resolve")})
add("_handle_approval_reaction", {k: v for k, v in HA.items() if k != "io"},
    {"t": "invalid", "self._claim_reaction_prompt": CLAIM_CH,
     "approval_refusal": None, "resolve_gateway_approval_card": rs("SystemExit", "bye")})
add("_handle_approval_reaction", {k: v for k, v in HA.items() if k != "io"},
    {"t": "invalid", "self._claim_reaction_prompt": CLAIM_CH}, fc=True)

# ── _handle_model_picker_reaction / _handle_choice_picker_reaction (async) ──
for fn, key, registry_key in [
    ("_handle_model_picker_reaction", MODP, MODP),
    ("_handle_choice_picker_reaction", CHOP, CHOP),
]:
    base = {"room_id": "!r:s", "reacts_to": "$target", "key": "yes", "sender": "@u:s",
            "self": reg(key, "$target", prompt())}
    add(fn, base, {"t": "invalid", "self._handle_picker_reaction": True})
    add(fn, base, {"t": "invalid", "self._handle_picker_reaction": False})
    add(fn, base, {"t": rs("SystemExit", "bye")})
    add(fn, base, {"t": "invalid", "self._handle_picker_reaction": rs("RuntimeError", "pick")})
    add(fn, base, {"t": "invalid", "self._handle_picker_reaction": rs("KeyboardInterrupt", "stop")})
    add(fn, base, {"t": "invalid"}, fc=True)

# ── _handle_picker_reaction (async) ──
HP = {"room_id": "!r:s", "reacts_to": "$target", "key": "yes", "sender": "@u:s",
      "label": "model picker", "invalid_text": "bad", "verbs": ["switch model", "verb.key"],
      "redact_bot_reactions": False,
      "self": reg(MODP, "$target", prompt(on_selected="sel"))}
add("_handle_picker_reaction", HP,
    {"self._claim_reaction_prompt": {"__script__": True, "ret": [False, None, None]}})
add("_handle_picker_reaction", HP,
    {"self._claim_reaction_prompt": {"__script__": True, "ret": [True, prompt(), None]}})
add("_handle_picker_reaction", HP,
    {"self._claim_reaction_prompt": {"__script__": True, "ret": [True, prompt(), "modelA"]},
     "prompt.on_selected": "confirmation", "self.send": None})
add("_handle_picker_reaction", dict(HP, redact_bot_reactions=True),
    {"self._claim_reaction_prompt": {"__script__": True, "ret": [True, prompt(), ["a", "b"]]},
     "prompt.on_selected": "done", "self._redact_bot_model_picker_reactions": None,
     "self.send": None})
add("_handle_picker_reaction", HP,
    {"self._claim_reaction_prompt": {"__script__": True, "ret": [True, prompt(), "m"]},
     "prompt.on_selected": rs("RuntimeError", "sel fail"), "t": "failed",
     "self.send": None})
add("_handle_picker_reaction", HP,
    {"self._claim_reaction_prompt": {"__script__": True, "ret": [True, prompt(), "m"]},
     "prompt.on_selected": rs("SystemExit", "bye")})
add("_handle_picker_reaction", HP,
    {"self._claim_reaction_prompt": {"__script__": True, "ret": [True, prompt(), "m"]},
     "prompt.on_selected": "ok", "t": rs("SystemExit", "bye")})
add("_handle_picker_reaction", HP, fc=True)

# ── _validate_matrix_prompt_reactor (async) ──
VAL = {"room_id": "!r:s", "target_event_id": "$target", "sender": "@u:s",
       "prompt_label": "approval"}
add("_validate_matrix_prompt_reactor", dict(VAL, prompt=prompt()),
    {"self._is_authorized_user": False, "t": "unauth",
     "self._send_invalid_reaction_feedback": None})
add("_validate_matrix_prompt_reactor", dict(VAL, prompt=prompt(requester="@other:s")),
    {"self._is_authorized_user": True, "t": "not requester",
     "self._send_invalid_reaction_feedback": None})
add("_validate_matrix_prompt_reactor", dict(VAL, prompt=prompt(requester="@u:s")),
    {"self._is_authorized_user": True, "picker_refusal": None})
add("_validate_matrix_prompt_reactor", dict(VAL, prompt=prompt(requester="@u:s")),
    {"self._is_authorized_user": True, "picker_refusal": "@owner:s",
     "refusal_text": "opened by other", "self._send_invalid_reaction_feedback": None})
add("_validate_matrix_prompt_reactor",
    dict(VAL, prompt=prompt(), self={"_approval_require_sender": False}),
    {"self._is_authorized_user": True, "picker_refusal": None})
add("_validate_matrix_prompt_reactor", dict(VAL, prompt=prompt()),
    {"self._is_authorized_user": rs("SystemExit", "bye")})
add("_validate_matrix_prompt_reactor", dict(VAL, prompt=prompt()), fc=True)

# ── _send_invalid_reaction_feedback (async) ──
FB = {"room_id": "!r:s", "target_event_id": "$target", "text": "invalid"}
for v in [None, True, "sent"]:
    add("_send_invalid_reaction_feedback", FB, {"self.send": v})
add("_send_invalid_reaction_feedback", FB, {"self.send": rs("RuntimeError", "boom")})
add("_send_invalid_reaction_feedback", FB, {"self.send": rs("SystemExit", "bye")})
add("_send_invalid_reaction_feedback",
    dict(FB, text=""), {"self.send": None})
add("_send_invalid_reaction_feedback", FB, fc=True)

# ── _expire_matrix_approval_prompt / _expire_matrix_model_picker_prompt (async) ──
for fn, registry_key, redact_key, text_key in [
    ("_expire_matrix_approval_prompt", APPR, "self._redact_bot_approval_reactions",
     "platform.matrix.approval.expired"),
    ("_expire_matrix_model_picker_prompt", MODP, "self._redact_bot_model_picker_reactions",
     "platform.shared.model_picker_expired"),
]:
    base = {"room_id": "!r:s", "target_event_id": "$target",
            "prompt": prompt(), "self": dict(reg(registry_key, "$target", prompt()),
                                             **{APPS: {"sk": prompt()}})}
    add(fn, base, {"t": "expired", redact_key: None,
                   "self._send_invalid_reaction_feedback": None})
    add(fn, base, {"t": "expired", redact_key: rs("RuntimeError", "red"),
                   "self._send_invalid_reaction_feedback": None})
    add(fn, base, {"t": "expired", redact_key: None,
                   "self._send_invalid_reaction_feedback": rs("SystemExit", "bye")})
    add(fn, base, {redact_key: None, "t": rs("KeyboardInterrupt", "stop")})
    add(fn, base, {"t": "expired", redact_key: None}, fc=True)
    add(fn, dict(base, prompt=prompt(session="s2", request="r2")),
        {"t": "expired", redact_key: None, "self._send_invalid_reaction_feedback": None})

# ── _redact_bot_approval_reactions (async) ──
add("_redact_bot_approval_reactions",
    {"room_id": "!r:s", "prompt": prompt(bot={})})
add("_redact_bot_approval_reactions",
    {"room_id": "!r:s", "prompt": prompt(bot={"\U0001f44d": "$e1"})},
    {"self._schedule_reaction_redaction": None})
add("_redact_bot_approval_reactions",
    {"room_id": "!r:s", "prompt": prompt(bot={"a": "$e1", "b": "$e2"})},
    {"self._schedule_reaction_redaction": None})
add("_redact_bot_approval_reactions",
    {"room_id": "!r:s", "prompt": prompt(bot={"a": "$e1"})},
    {"self._schedule_reaction_redaction": rs("RuntimeError", "sched")})
add("_redact_bot_approval_reactions",
    {"room_id": "!r:s", "prompt": prompt(bot={"a": "$e1"})},
    {"self._schedule_reaction_redaction": rs("SystemExit", "bye")})
add("_redact_bot_approval_reactions",
    {"room_id": "!r:s", "prompt": prompt(bot={"a": "$e1"})}, fc=True)

# ── _redact_bot_model_picker_reactions (async; per-item Exception swallow) ──
add("_redact_bot_model_picker_reactions",
    {"room_id": "!r:s", "prompt": prompt(bot={})})
add("_redact_bot_model_picker_reactions",
    {"room_id": "!r:s", "prompt": prompt(bot={"a": "$e1"})},
    {"self.redact_message": True})
add("_redact_bot_model_picker_reactions",
    {"room_id": "!r:s", "prompt": prompt(bot={"a": "$e1", "b": "$e2"})},
    {"self.redact_message": [rs("RuntimeError", "red"), True]})
add("_redact_bot_model_picker_reactions",
    {"room_id": "!r:s", "prompt": prompt(bot={"a": "$e1", "b": "$e2"})},
    {"self.redact_message": True})
add("_redact_bot_model_picker_reactions",
    {"room_id": "!r:s", "prompt": prompt(bot={"a": "$e1"})},
    {"self.redact_message": rs("SystemExit", "bye")})
add("_redact_bot_model_picker_reactions",
    {"room_id": "!r:s", "prompt": prompt(bot={"a": "$e1"})}, fc=True)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")


if __name__ == "__main__":
    main()
