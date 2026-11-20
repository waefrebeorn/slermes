#!/usr/bin/env python3
"""Generate tests/fixtures/feishu_adapter_flow2.jsonl — input cases for the
Feishu adapter inbound-event + card-action FLOW remainder oracle.

For each derived case the io scripts are captured from the live reference run
with depth-filtered DELEGATING recorders: a seamed sibling called at depth 0
records {name, args, ret} and delegates to the real function (ported siblings)
or a deterministic stub (seamed-only helpers / foreign imports / logger /
time.time); nested seamed calls delegate silently. The recorded per-name result
queues are exactly what the C universal seam and the oracle's scripted stubs
replay. List-valued results are wrapped {"__script__": true, "ret": [...]}
(queue semantics — a bare array would be read as a SEQUENCE).

A derive-vs-drive self-consistency check runs every derived case through BOTH
the delegating live run and the oracle's scripted drive() and asserts identical
ret / trace / state. Hand-scripted branch-coverage and BaseException-propagation
cases are added on top.
"""
import copy
import json
import sys
import types
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import oracle_feishu_adapter_flow2 as o  # noqa: E402

OUT = Path(__file__).resolve().parent / "fixtures" / "feishu_adapter_flow2.jsonl"
CASES = []
CHECK = []

# Deterministic stubs for seamed-only helpers / foreign imports / clock. These
# drive the happy path; branch cases hand-script io overrides below.
REAL_STUBS = {
    "_submit_on_loop": lambda loop, coro: True,
    "_get_cached_sender_name": lambda sender_id: None,
    "_is_sender_authorized": lambda user_id, chat_type=None, chat_id=None, **k: True,
    "_handle_reaction_event": lambda event_type, data: "coro:reaction",
    "_handle_card_action_event": lambda data: "coro:card_action",
    "_resolve_approval": lambda **k: "coro:approval",
    "_resolve_update_prompt": lambda *a, **k: "coro:update_prompt",
    "_build_resolved_approval_card": lambda **k: {"tag": "resolved_approval"},
    "_build_resolved_update_prompt_card": lambda **k: {"tag": "resolved_update"},
    "_handle_clarify_card_action": lambda **k: {"tag": "clarify"},
    "_handle_choice_picker_card_action": lambda **k: {"tag": "picker"},
    "approval_refusal": lambda *a, **k: None,
    "refusal_text": lambda requester: "refused:" + str(requester),
    "update_prompt_refusal": lambda *a, **k: None,
    "time_time": lambda: 1000.0,
}


def wrap(r):
    return {"__script__": True, "ret": r} if isinstance(r, list) else r


def make_deleg(nm, slf, events, depth):
    if nm in o.REAL_DEFS:
        real = o.REAL_DEFS[nm] if nm in o.STATIC else types.MethodType(o.REAL_DEFS[nm], slf)
    elif nm in o.LOGGER_SEAMS:
        real = lambda *a, **k: None
    else:
        real = REAL_STUBS[nm]
    argfn = o.ARGMAP[nm]

    def f(*a, **k):
        ap = o.canon(argfn(*a, **k))
        d = depth[0]
        depth[0] += 1
        try:
            result = real(*a, **k)
        finally:
            depth[0] -= 1
        if d == 0:
            events.append({"name": nm, "args": ap, "ret": o.canon(result)})
        return result
    return f


def derive_io(func, args):
    events = []
    depth = [0]
    for nm in o.REAL_DEFS:
        o.NS[nm] = o.REAL_DEFS[nm]
    for nm in o.FOREIGN_SEAMS:
        o.NS[nm] = make_deleg(nm, None, events, depth)
    o.NS["time"] = types.SimpleNamespace(time=make_deleg("time_time", None, events, depth))
    o.NS["logger"] = types.SimpleNamespace(
        debug=make_deleg("logger_debug", None, events, depth),
        info=make_deleg("logger_info", None, events, depth),
        warning=make_deleg("logger_warning", None, events, depth))
    o.NS["P2CardActionTriggerResponse"] = lambda: types.SimpleNamespace(card=None)
    o.NS["CallBackCard"] = lambda: types.SimpleNamespace(type=None, data=None)
    o.NS["CallBackToast"] = lambda: types.SimpleNamespace(type=None, content=None)
    rec = lambda nm, slf: make_deleg(nm, slf, events, depth)
    state_out = {}
    ret = o.live_call(func, copy.deepcopy(args), state_out, None, rec=rec)
    io = {}
    for ev in events:
        io.setdefault(ev["name"], []).append(wrap(ev["ret"]))
    return io, o.canon(ret), events, o.canon(state_out)


def add(func, args, io=None):
    args = copy.deepcopy(args)
    if io is None:
        io, dret, dtrace, dstate = derive_io(func, args)
        got = o.drive(func, {"args": copy.deepcopy(args), "io": io})
        want = {"ret": dret, "trace": dtrace, "state": dstate}
        if got != want:
            CHECK.append((func, args, want, got))
    CASES.append({"func": func, "in": {"args": args, "io": io}})


def rais(kind, msg):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


EV = {"operator": {"open_id": "ou_1", "user_id": "u_1", "union_id": "on_1"},
      "context": {"open_chat_id": "oc_1"}}
LOOP = {"is_closed": False}


def selfwith(**kw):
    base = {"_loop": LOOP, "_chat_info_cache": {}, "_card_action_tokens": {},
            "_approval_state": {}, "_update_prompt_state": {},
            "_admins": [], "_allowed_group_users": [], "config": {"extra": {}}}
    base.update(kw)
    return base


# ── format_message ──
for c in ["", "hi", "  hi  ", "a\nb", "  \t\n ", "plain text", "x\ry"]:
    add("format_message", {"content": c})

# ── _on_message_read_event ──
add("_on_message_read_event", {"data": {"event": {"message": {"message_id": "m1"}}}})
add("_on_message_read_event", {"data": {"event": {"message": {}}}})
add("_on_message_read_event", {"data": {"event": {"message": {"message_id": None}}}})
add("_on_message_read_event", {"data": {"event": {}}})
add("_on_message_read_event", {"data": {}})
add("_on_message_read_event", {"data": None})

# ── _on_p2p_chat_entered / _on_message_recalled (data ignored) ──
for d in [{}, {"event": {}}, None, {"x": 1}, {"event": {"message_id": "m"}}]:
    add("_on_p2p_chat_entered", {"data": d})
    add("_on_message_recalled", {"data": d})

# ── _on_bot_membership_change ──
add("_on_bot_membership_change", {"self": selfwith(_chat_info_cache={"oc_1": {"a": 1}, "oc_2": {}}),
                                  "data": {"event": {"chat_id": "oc_1"}}, "verb": "added to"})
add("_on_bot_membership_change", {"self": selfwith(_chat_info_cache={"oc_1": {"a": 1}}),
                                  "data": {"event": {"chat_id": "oc_x"}}, "verb": "removed from"})
add("_on_bot_membership_change", {"self": selfwith(), "data": {"event": {}}, "verb": "added to"})
add("_on_bot_membership_change", {"self": selfwith(), "data": {"event": {"chat_id": None}}, "verb": "added to"})
add("_on_bot_membership_change", {"self": selfwith(), "data": None, "verb": "removed from"})
add("_on_bot_membership_change", {"self": selfwith(_chat_info_cache={"": {}}),
                                  "data": {"event": {"chat_id": ""}}, "verb": "added to"})

# ── wrappers ──
for d in [{"event": {"chat_id": "oc_1"}}, {"event": {}}, None, {"event": {"chat_id": None}}, {}]:
    add("_on_bot_added_to_chat", {"self": selfwith(_chat_info_cache={"oc_1": {}}), "data": d})
    add("_on_bot_removed_from_chat", {"self": selfwith(_chat_info_cache={"oc_1": {}}), "data": d})

# ── _loop_accepts_callbacks ──
for lp in [None, {"is_closed": False}, {"is_closed": True}, {},
           {"is_closed": False, "running": True}, {"is_closed": True, "running": False}]:
    add("_loop_accepts_callbacks", {"loop": lp})

# ── _is_interactive_operator_authorized ──
add("_is_interactive_operator_authorized", {"self": selfwith(), "open_id": "  ", "user_id": "", "union_id": "", "chat_id": ""})
add("_is_interactive_operator_authorized", {"self": selfwith(_admins=["ou_1"]), "open_id": "ou_1"})
add("_is_interactive_operator_authorized", {"self": selfwith(_admins=["ou_x"]), "open_id": "ou_1"})
add("_is_interactive_operator_authorized", {"self": selfwith(_admins=["*"]), "open_id": "ou_any"})
add("_is_interactive_operator_authorized", {"self": selfwith(_allowed_group_users=["u_1"]), "open_id": "ou_1", "user_id": "u_1"})
add("_is_interactive_operator_authorized", {"self": selfwith(_admins=["on_1"]), "open_id": "ou_1", "union_id": "on_1"})
add("_is_interactive_operator_authorized", {"self": selfwith(), "open_id": "ou_1", "user_id": "u_1", "chat_id": "oc_9"})
add("_is_interactive_operator_authorized", {"self": selfwith(), "open_id": "ou_1", "user_id": "", "chat_id": ""},
    io={"_is_sender_authorized": [None], "logger_debug": [None]})
add("_is_interactive_operator_authorized", {"self": selfwith(), "open_id": "ou_1", "user_id": "u_1"},
    io={"_is_sender_authorized": [False]})
add("_is_interactive_operator_authorized", {"self": selfwith(_admins=["ou_1"]), "open_id": " ou_1 "})

# ── _card_response ──
add("_card_response", {"sdk": {"response": True, "card": True}})
add("_card_response", {"sdk": {"response": True, "card": True}, "card_data": {"x": 1}})
add("_card_response", {"sdk": {"response": False}})
add("_card_response", {"sdk": {"response": True, "card": False}, "card_data": {"x": 1}})
add("_card_response", {"sdk": {"response": True, "card": True}, "card_data": None})
add("_card_response", {"sdk": {"response": True, "card": True}, "card_data": {"a": [1, 2]}})

# ── _card_toast (hand-scripted _card_response) ──
add("_card_toast", {"sdk": {"toast": True}, "text": "nope"},
    io={"_card_response": [{"card": None}]})
add("_card_toast", {"sdk": {"toast": False}, "text": "nope"},
    io={"_card_response": [{"card": None}]})
add("_card_toast", {"sdk": {"toast": True}, "text": ""},
    io={"_card_response": [{"card": {"type": "raw", "data": {"k": 1}}}]})
add("_card_toast", {"sdk": {"toast": True}, "text": "only Al"},
    io={"_card_response": [None]})
add("_card_toast", {"sdk": {"toast": True}, "text": "x"},
    io={"_card_response": [rais("SystemExit", "boom")]})

# ── _validate_card_action ──
add("_validate_card_action", {"self": selfwith(_admins=["ou_1"]), "event": EV, "state": {"chat_id": "oc_1"}, "label": "approval", "ident": "5"})
add("_validate_card_action", {"self": selfwith(_admins=["ou_1"]), "event": EV, "state": {"chat_id": "oc_2"}, "label": "approval", "ident": "5"})
add("_validate_card_action", {"self": selfwith(_admins=["ou_x"]), "event": EV, "state": {"chat_id": "oc_1"}, "label": "approval", "ident": "5"})
add("_validate_card_action", {"self": selfwith(_admins=["ou_1"]), "event": {"operator": {}, "context": {}}, "state": {}, "label": "update prompt", "ident": 7})
add("_validate_card_action", {"self": selfwith(_admins=["ou_1"]), "event": EV, "state": {"chat_id": "oc_1"}, "label": "approval", "ident": "5"},
    io={"_is_interactive_operator_authorized": [True], "_get_cached_sender_name": ["Al"]})
add("_validate_card_action", {"self": selfwith(), "event": EV, "state": {"chat_id": "oc_1"}, "label": "approval", "ident": "5"},
    io={"_is_interactive_operator_authorized": [False], "logger_warning": [None]})
add("_validate_card_action", {"self": selfwith(_admins=["ou_1"]), "event": EV, "state": {"chat_id": "oc_1"}, "label": "approval", "ident": "5"},
    io={"_is_interactive_operator_authorized": [rais("KeyboardInterrupt", "int")]})

# ── _handle_approval_card_action ──
AP_SELF = selfwith(_approval_state={"5": {"session_key": "s", "chat_id": "oc_1", "request_id": "r1", "body": "do it"}},
                   _admins=["ou_1"], config={"extra": {"k": "v"}})
AP_ARGS = {"self": AP_SELF, "event": EV, "action_value": {"approval_id": "5", "hermes_action": "approve_once"}, "loop": LOOP}
add("_handle_approval_card_action", AP_ARGS)
for ha, ch in [("approve_session", "session"), ("approve_always", "always"), ("deny", "deny"), ("unknown", "deny")]:
    a = copy.deepcopy(AP_ARGS); a["action_value"]["hermes_action"] = ha
    add("_handle_approval_card_action", a)
add("_handle_approval_card_action", {"self": selfwith(), "event": EV, "action_value": {}, "loop": LOOP})
add("_handle_approval_card_action", {"self": selfwith(), "event": EV, "action_value": {"approval_id": "9"}, "loop": LOOP})
add("_handle_approval_card_action", AP_ARGS,
    io={"_validate_card_action": [wrap(["ou_1", "oc_1", "Al", "u_1", "on_1"])],
        "approval_refusal": ["Bob"], "refusal_text": ["only Bob"],
        "_card_toast": [{"card": None, "toast": {"type": "warning", "content": "only Bob"}}]})
add("_handle_approval_card_action", AP_ARGS,
    io={"_validate_card_action": [wrap(["ou_1", "oc_1", "Al", "u_1", "on_1"])],
        "approval_refusal": [None], "_resolve_approval": ["coro:approval"], "_submit_on_loop": [False],
        "_card_response": [{"card": None}]})
add("_handle_approval_card_action", AP_ARGS,
    io={"_validate_card_action": [None], "_card_response": [{"card": None}]})
add("_handle_approval_card_action", AP_ARGS,
    io={"_validate_card_action": [wrap(["ou_1", "oc_1", "Al", "u_1", "on_1"])],
        "approval_refusal": [None], "_resolve_approval": [rais("SystemExit", "exit")]})

# ── _handle_update_prompt_card_action ──
UP_SELF = selfwith(_update_prompt_state={"7": {"session_key": "s", "chat_id": "oc_1"}}, _admins=["ou_1"])
UP_ARGS = {"self": UP_SELF, "event": EV, "action_value": {"update_prompt_id": "7", "hermes_update_prompt_action": "y"}, "loop": LOOP}
add("_handle_update_prompt_card_action", UP_ARGS)
a = copy.deepcopy(UP_ARGS); a["action_value"]["hermes_update_prompt_action"] = " N "
add("_handle_update_prompt_card_action", a)
add("_handle_update_prompt_card_action", {"self": selfwith(), "event": EV, "action_value": {}, "loop": LOOP})
add("_handle_update_prompt_card_action", {"self": selfwith(), "event": EV, "action_value": {"update_prompt_id": "9"}, "loop": LOOP})
a = copy.deepcopy(UP_ARGS); a["action_value"]["hermes_update_prompt_action"] = "maybe"
add("_handle_update_prompt_card_action", a)
add("_handle_update_prompt_card_action", UP_ARGS,
    io={"_validate_card_action": [wrap(["ou_1", "oc_1", "Al", "u_1", "on_1"])],
        "update_prompt_refusal": ["Bob"], "refusal_text": ["only Bob"],
        "_card_toast": [{"card": None, "toast": {"type": "warning", "content": "only Bob"}}]})
add("_handle_update_prompt_card_action", UP_ARGS,
    io={"_validate_card_action": [wrap(["ou_1", "oc_1", "Al", "u_1", "on_1"])],
        "update_prompt_refusal": [None], "_resolve_update_prompt": ["coro:up"], "_submit_on_loop": [False],
        "_card_response": [{"card": None}]})
add("_handle_update_prompt_card_action", UP_ARGS,
    io={"_validate_card_action": [None], "_card_response": [{"card": None}]})

# ── _pop_validated_prompt_state ──
POP = {"states": {"5": {"session_key": "s", "chat_id": "oc_1"}}, "ident": "5", "label": "Approval",
       "open_id": "ou_1", "chat_id": "oc_1", "unauthorized_fmt": "[Feishu] Unauthorized approval click by %s for approval %s",
       "operator_repr": "ou_1", "user_id": "", "union_id": ""}
add("_pop_validated_prompt_state", dict(POP, self=selfwith(_admins=["ou_1"])))
add("_pop_validated_prompt_state", dict(POP, states={}, self=selfwith()))
add("_pop_validated_prompt_state", dict(POP, self=selfwith(_admins=["ou_x"])))
add("_pop_validated_prompt_state", dict(POP, chat_id="oc_2", self=selfwith(_admins=["ou_1"])))
add("_pop_validated_prompt_state", dict(POP, states={"5": {"session_key": "s"}}, chat_id="", self=selfwith(_admins=["ou_1"])))
add("_pop_validated_prompt_state", dict(POP, self=selfwith()),
    io={"_is_interactive_operator_authorized": [False], "logger_warning": [None]})
add("_pop_validated_prompt_state", dict(POP, self=selfwith(_admins=["ou_1"])),
    io={"_is_interactive_operator_authorized": [rais("GeneratorExit", "gen")]})

# ── _is_card_action_duplicate ──
add("_is_card_action_duplicate", {"self": selfwith(_card_action_tokens={}), "token": "t1"})
add("_is_card_action_duplicate", {"self": selfwith(_card_action_tokens={"t1": 999.0}), "token": "t1"})
add("_is_card_action_duplicate", {"self": selfwith(_card_action_tokens={"old": 50.0, "t1": 999.0}), "token": "t2"})
add("_is_card_action_duplicate", {"self": selfwith(_card_action_tokens={"a": 100.0, "b": 200.0}), "token": "a"})
add("_is_card_action_duplicate", {"self": selfwith(_card_action_tokens={"t1": 1000.0}), "token": "t1"})
add("_is_card_action_duplicate", {"self": selfwith(_card_action_tokens={"old": 99.0}), "token": "new"},
    io={"time_time": [2000]})
add("_is_card_action_duplicate", {"self": selfwith(), "token": "t"},
    io={"time_time": [rais("SystemExit", "clock")]})

# ── _on_reaction_event ──
RE_DATA = {"event": {"message_id": "m1", "operator_type": "user", "reaction_type": {"emoji_type": "THUMBSUP"}}}
add("_on_reaction_event", {"self": selfwith(), "event_type": "im.message.reaction.created_v1", "data": RE_DATA})
add("_on_reaction_event", {"self": selfwith(), "event_type": "im.message.reaction.deleted_v1", "data": RE_DATA})
add("_on_reaction_event", {"self": selfwith(), "event_type": "im.message.reaction.created_v1",
                           "data": {"event": {"message_id": "m1", "operator_type": "bot"}}})
add("_on_reaction_event", {"self": selfwith(), "event_type": "im.message.reaction.created_v1",
                           "data": {"event": {"message_id": "m1", "operator_type": "app"}}})
add("_on_reaction_event", {"self": selfwith(), "event_type": "im.message.reaction.created_v1",
                           "data": {"event": {"message_id": "", "operator_type": "user"}}})
add("_on_reaction_event", {"self": selfwith(_loop={"is_closed": True}), "event_type": "x.created", "data": RE_DATA},
    io={"logger_debug": [None], "_loop_accepts_callbacks": [False]})
add("_on_reaction_event", {"self": selfwith(), "event_type": "x.created", "data": RE_DATA},
    io={"logger_debug": [None], "_loop_accepts_callbacks": [True], "_handle_reaction_event": [rais("SystemExit", "r")]})

# ── _on_card_action_trigger ──
def ct(value):
    return {"self": selfwith(), "data": {"event": {"action": {"value": value}}}}
add("_on_card_action_trigger", ct({"hermes_action": "approve_once", "approval_id": "5"}))
add("_on_card_action_trigger", ct({"hermes_update_prompt_action": "y", "update_prompt_id": "7"}))
add("_on_card_action_trigger", ct({"hermes_clarify_action": "x"}))
add("_on_card_action_trigger", ct({"hermes_choice_picker_action": "x"}))
add("_on_card_action_trigger", ct({}))
add("_on_card_action_trigger", ct("xyz"))
add("_on_card_action_trigger", {"self": selfwith(_loop={"is_closed": True}), "data": {}})
add("_on_card_action_trigger", ct({"hermes_action": "approve_once"}),
    io={"_loop_accepts_callbacks": [True], "_handle_approval_card_action": [rais("SystemExit", "route")]})
add("_on_card_action_trigger", ct({}),
    io={"_loop_accepts_callbacks": [True], "_handle_card_action_event": ["coro:card"],
        "_submit_on_loop": [True], "_card_response": [{"card": None}]})


def main():
    if CHECK:
        print(f"GENERATOR SELF-CONSISTENCY FAILURES: {len(CHECK)}")
        for func, args, want, got in CHECK[:8]:
            print(f"  {func} args={json.dumps(args, ensure_ascii=False)[:200]}")
            print(f"    want={json.dumps(want, ensure_ascii=False)[:300]}")
            print(f"    got ={json.dumps(got, ensure_ascii=False)[:300]}")
        sys.exit(1)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"WROTE {OUT}: {len(CASES)} cases, {len(counts)} funcs")
    low = [f for f, n in counts.items() if n < 5]
    print("funcs with <5 cases:", low)
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    if low or len(counts) != len(o.TARGETS):
        sys.exit(1)


if __name__ == "__main__":
    main()