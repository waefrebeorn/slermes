#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_agent_flow.jsonl — {func,in} cases for
t_port_gateway_agent_flow.c / oracle_gateway_agent_flow.py. >=4 cases per port.
List-returning seams are wrapped ([[...]]) so the scripted RESULT is the list;
per-call sequences are plain arrays of results. Inputs are consumed identically
by the C harness and the oracle (which drives LIVE quarry code)."""
import json
from pathlib import Path

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def st(**kw):
    base = {"session_id": "s1", "connection_epoch": 1, "state": {"info": {}}}
    base.update(kw)
    return base


# ── effort_word ──
add("effort_word", config={"enabled": False})
add("effort_word", config={"effort": "high"})
add("effort_word", config={"effort": ""})
add("effort_word", config={"enabled": True, "effort": "low"})
add("effort_word", config=None)
add("effort_word", config={"effort": 5})

# ── route_off_session ──
add("route_off_session", st=st(state={"info": {"model": "sess"}}), model="pick", provider="", alias="", seams={"runs_route": False})
add("route_off_session", st=st(state={"info": {"model": "sess"}}), model="pick", provider="prov", alias="", seams={"runs_route": False})
add("route_off_session", st=st(state={"info": {"model": "sess"}}), model="", provider="", alias="", seams={"runs_route": False})
add("route_off_session", st=st(state={"info": {}}), model="pick", provider="", alias="", seams={"runs_route": False})
add("route_off_session", st=st(state={"info": {"model": "sess"}}), model="pick", provider="", alias="", seams={"runs_route": True})
add("route_off_session", st=st(state={"info": {"model": "sess"}}), model="pick", provider="", alias="al", seams={"runs_route": True})

# ── set_reasoning ──
add("set_reasoning", st=st(), config={"effort": "high"}, explicit=True,
    seams={"call": [{"revision": 3, "execution_generation": 7}, None], "uuid4_hex": "abc"})
add("set_reasoning", st=st(state={"info": {"reasoning_effort": "high"}}), config={"effort": "high"}, explicit=False, seams={})
add("set_reasoning", st=st(), config=None, explicit=True, seams={})
add("set_reasoning", st=st(), config={"effort": "high"}, explicit=False,
    seams={"call": {"__raise__": "GatewayClientError", "msg": "boom"}})
add("set_reasoning", st=st(), config={"effort": "high"}, explicit=True,
    seams={"call": [{"execution_generation": 7}, None], "uuid4_hex": "abc"})
add("set_reasoning", st=st(), config={"enabled": False}, explicit=True,
    seams={"call": [{"revision": 1, "execution_generation": 2}, None], "uuid4_hex": "n"})

# ── sync_show_reasoning ──
add("sync_show_reasoning", st=st(connection_epoch=2, _reasoning_callback={}, _show_reasoning_sent=None), seams={"call": None})
add("sync_show_reasoning", st=st(connection_epoch=2, _reasoning_callback=None, _show_reasoning_sent=None), seams={"call": None})
add("sync_show_reasoning", st=st(connection_epoch=2, _reasoning_callback={}, _show_reasoning_sent=["s1", True, 2]), seams={"call": None})
add("sync_show_reasoning", st=st(connection_epoch=2, _reasoning_callback={}, _show_reasoning_sent=["s1", True, 1]), seams={"call": None})
add("sync_show_reasoning", st=st(connection_epoch=2, _reasoning_callback={}, _show_reasoning_sent=None),
    seams={"call": {"__raise__": "GatewayClientError", "msg": "x"}})

# ── service_tier ──
add("service_tier", st=st(state={}), tier="fast", seams={"call": None})
add("service_tier", st=st(state={"service_tier_sent": "fast"}), tier="fast", seams={"call": None})
add("service_tier", st=st(state={}), tier=None, seams={"call": None})
add("service_tier", st=st(state={"service_tier_sent": "fast"}), tier=None, seams={"call": None})
add("service_tier", st=st(state={}), tier="fast", seams={"call": {"__raise__": "GatewayClientError", "msg": "x"}})

# ── correct ──
add("correct", st=st(_generation=5, suppress_status_output=False), verb="steer", text="hi", seams={"call": {"status": "queued"}})
add("correct", st=st(_generation=5, suppress_status_output=False), verb="redirect", text="hi", seams={"call": {"status": "redirected"}})
add("correct", st=st(_generation=5, suppress_status_output=False), verb="steer", text="hi", seams={"call": {"status": "other"}})
add("correct", st=st(_generation=None), verb="steer", text="x", seams={})
add("correct", st=st(_generation=5, suppress_status_output=False), verb="steer", text="stop",
    seams={"call": {"voice_stopped": True}, "t": "STOPPED"})
add("correct", st=st(_generation=5), verb="steer", text="x", seams={"call": {"__raise__": "GatewayClientError", "msg": "e"}})

# ── take_turn_route ──
add("take_turn_route", st=st(_turn_route={"model": "m", "provider": "p"}, _turn_reasoning="high"))
add("take_turn_route", st=st(_turn_route={"model": "m"}, _turn_reasoning=None))
add("take_turn_route", st=st(_turn_route=None, _turn_reasoning=None))
add("take_turn_route", st=st(_turn_route={}, _turn_reasoning="high"))

# ── give_back_turn_route ──
add("give_back_turn_route", st=st(_taken_turn_route=[{"model": "m"}, "high"], _turn_route=None))
add("give_back_turn_route", st=st(_taken_turn_route=[{"model": "m"}, "high"], _turn_route={"model": "x"}))
add("give_back_turn_route", st=st(_taken_turn_route=None, _turn_route=None))
add("give_back_turn_route", st=st(_taken_turn_route=[{"model": "m"}, None], _turn_route=None))

# ── prepare_submit ──
add("prepare_submit", st=st(_reasoning_callback=None, _show_reasoning_sent=None,
    state={"gaps": 1, "attached_gaps": 0, "pending": [{"status": "unknown"}]}),
    turn_session_id="s1", seams={"reattach": {}, "start": None, "call": None, "pending_lines": [["line1"]]})
add("prepare_submit", st=st(_reasoning_callback=None, _show_reasoning_sent=["s1", False, 1],
    state={"gaps": 0, "attached_gaps": 0, "pending": []}), turn_session_id="s1", seams={"start": None})
add("prepare_submit", st=st(_reasoning_callback=None, _show_reasoning_sent=None,
    state={"gaps": 2, "attached_gaps": 0, "pending": [{"status": "queued"}]}),
    turn_session_id="s1", seams={"reattach": {}, "start": None, "call": None})
add("prepare_submit", st=st(_reasoning_callback={}, _show_reasoning_sent=None,
    state={"gaps": 0, "attached_gaps": 0, "pending": []}), turn_session_id="s1", seams={"start": None, "call": None})

# ── submit_params ──
add("submit_params", st=st(_moa_preset=None), user_message="hello", persist_user_message=None, turn_author=None, seams={"uuid4_hex": "sub1"})
add("submit_params", st=st(_moa_preset=None), user_message="api", persist_user_message="stored", turn_author=None, seams={"uuid4_hex": "sub2"})
add("submit_params", st=st(state={"info": {}}, _turn_route=None, _moa_preset=None),
    user_message=[{"type": "text", "text": "hi\n[Image attached at: x]"}, {"type": "image_url", "image_url": {"url": "data:image/png;base64,AAA"}}],
    persist_user_message=["ignored"], turn_author="bob",
    seams={"call": {"path": "/staged/a.png", "mime": "image/png"}, "uuid4_hex": "sub3"})
add("submit_params", st=st(_voice_turn_pending=True, _moa_preset=None), user_message="x", persist_user_message=None, turn_author="bob", seams={"uuid4_hex": "sub4"})
add("submit_params", st=st(state={"info": {"provider": "openai", "model": "gpt"}}, _moa_preset="moa-model"),
    user_message="x", persist_user_message=None, turn_author=None, seams={"uuid4_hex": "sub5"})
add("submit_params", st=st(_turn_route={"model": "once"}, _turn_reasoning="high", _moa_preset=None),
    user_message="x", persist_user_message=None, turn_author=None, seams={"uuid4_hex": "sub6"})
add("submit_params", st=st(clarify_callback={}, _finite=True, _pending_cli_user_message={"display_kind": "card"}, _moa_preset=None),
    user_message="x", persist_user_message=None, turn_author=None, seams={"uuid4_hex": "sub7"})

# ── clarify_reply ──
add("clarify_reply", callback_present=True, params={"questions": [{"qid": "q1", "question": "Q?"}]},
    seams={"clarify_callback": {"answers": {"q1": "yes"}, "outcome": "submitted"}})
add("clarify_reply", callback_present=False, params={"questions": [{"qid": "q1"}]}, seams={})
add("clarify_reply", callback_present=True, params={"questions": []}, seams={})
add("clarify_reply", callback_present=True, params={"questions": [{"qid": "q1"}]}, seams={"clarify_callback": "not-a-dict"})
add("clarify_reply", callback_present=True, params={"questions": [{"qid": "q1"}]}, seams={"clarify_callback": {"answers": {"q1": 5}}})
add("clarify_reply", callback_present=True, params={"questions": [{"qid": "q1"}]},
    seams={"clarify_callback": {"answers": {"q1": "a"}, "outcome": "bogus"}})

out = Path(__file__).resolve().parent / "fixtures" / "gateway_agent_flow.jsonl"
out.parent.mkdir(parents=True, exist_ok=True)
with open(out, "w", encoding="utf-8", newline="\n") as fh:
    for c in CASES:
        fh.write(json.dumps(c, ensure_ascii=False) + "\n")
print("wrote %d cases -> %s" % (len(CASES), out))